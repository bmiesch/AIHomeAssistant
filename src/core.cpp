#include "core.h"
#include "log.h"
#include <chrono>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

void Core::AudioCaptureLoop() {
    while(running) {
        auto buffer = audio_capture->CaptureAudio(1000);
        {
            std::lock_guard<std::mutex> lock(audio_queue_mutex);
            if (audio_queue.size() > 5) {  // Keep 10 seconds of audio max
                WARN_LOG("Audio queue overflow: " + std::to_string(audio_queue.size()) + " buffers");
                while (audio_queue.size() > 3) {  // Keep last 5 seconds
                    audio_queue.pop();
                }
            }
            audio_queue.push(std::move(buffer));
        }
        audio_queue_cv.notify_one();
    }
}

void Core::AudioProcessingLoop() {
    while(running) {
        std::vector<int16_t> buffer;
        {
            std::unique_lock<std::mutex> lock(audio_queue_mutex);
            audio_queue_cv.wait(lock, [this] { return !audio_queue.empty() || !running; });
            if (!running) break;
            buffer = std::move(audio_queue.front());
            audio_queue.pop();
        }

        if (keyword_detector->DetectKeyword(buffer, true)) {
            // Clear any backlogged audio before starting command collection
            {
                std::lock_guard<std::mutex> lock(audio_queue_mutex);
                std::queue<std::vector<int16_t>> empty;
                std::swap(audio_queue, empty);
            }
            INFO_LOG("Keyword detected! Listening for command...");
            
            // Collect audio for 5 seconds (5 buffers of 1000ms each)
            std::vector<int16_t> command_buffer;
            for (int i = 0; i < 5; ++i) {
                std::unique_lock<std::mutex> lock(audio_queue_mutex);
                // Wait for audio data to be available in the queue or for the running flag to be false
                audio_queue_cv.wait(lock, [this] {return !audio_queue.empty() || !running; });
                if (!running) break;
                if (audio_queue.empty()) {
                    WARN_LOG("Missed audio buffer during command collection");
                    continue;
                }
                auto next_buffer = std::move(audio_queue.front());
                audio_queue.pop();
                command_buffer.insert(command_buffer.end(), next_buffer.begin(), next_buffer.end());
            }

            Command cmd = keyword_detector->DetectCommand(command_buffer, true);

            switch(cmd) {
                case Command::TURN_ON:
                    INFO_LOG("Command detected: TURN_ON");
                    PublishLEDManagerCommand("turn_on", json::object());
                    break;
                case Command::TURN_OFF:
                    INFO_LOG("Command detected: TURN_OFF");
                    PublishLEDManagerCommand("turn_off", json::object());
                    break;
                case Command::NO_COMMAND:
                default:
                    WARN_LOG("No command detected");
                    break;
            }
        }
    }
}

Core::Core(const std::string& broker_address, const std::string& client_id) 
    : mqtt_client(broker_address, client_id),
      mqtt_conn_opts(mqtt::connect_options_builder()
        .keep_alive_interval(std::chrono::seconds(20))
        .clean_session(true)
        .automatic_reconnect(true)
        .finalize()) {

    INFO_LOG("Initializing Core with broker: " + broker_address + ", client_id: " + client_id);
    audio_capture = std::make_unique<AudioCapture>();
    keyword_detector = std::make_unique<KeywordDetector>();
    
    mqtt_client.set_callback(*this);
}

Core::~Core() {
    DEBUG_LOG("Core destructor called");
    Stop();
}

void Core::Initialize() {
    try {
        INFO_LOG("Connecting to MQTT broker...");
        mqtt::token_ptr conntok = mqtt_client.connect(mqtt_conn_opts);
        conntok->wait();
    } catch (const mqtt::exception& e) {
        ERROR_LOG("MQTT connection error: " + std::string(e.what()));
        throw;
    }

    // Create and start the worker thread
    INFO_LOG("Starting main worker thread");
    worker_thread = std::thread(&Core::Run, this);
}

void Core::Run() {
    running = true;
    INFO_LOG("Starting Core threads");

    INFO_LOG("Starting audio capture thread");
    audio_thread = std::thread(&Core::AudioCaptureLoop, this);

    INFO_LOG("Starting audio processing thread");
    audio_processing_thread = std::thread(&Core::AudioProcessingLoop, this);

    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    INFO_LOG("Joining audio threads");
    audio_thread.join();
    audio_processing_thread.join();
}   

void Core::Stop() {
    INFO_LOG("Stopping Core");
    running = false;
    audio_queue_cv.notify_all();
    
    if (audio_thread.joinable()) audio_thread.join();
    if (audio_processing_thread.joinable()) audio_processing_thread.join();
    if (worker_thread.joinable()) worker_thread.join();

    try {
        mqtt_client.disconnect()->wait();
        DEBUG_LOG("MQTT client disconnected");
    }
    catch (const mqtt::exception& e) {
        ERROR_LOG("MQTT disconnect error: " + std::string(e.what()));
    }
}

void Core::PublishLEDManagerCommand(const std::string& command, const json& params) {
    json message;
    message["command"] = command;
    message["params"] = params;
    std::string topic = "home/services/led_manager/command";
    std::string payload = message.dump();

    DEBUG_LOG("Publishing command: " + command + " to topic: " + topic);
    try {
        mqtt_client.publish(topic, payload, 1, false)->wait_for(std::chrono::seconds(10));
    } catch (const mqtt::exception& e) {
        ERROR_LOG("Error publishing command: " + std::string(e.what()));
        throw;
    }
}

void Core::connected(const std::string& cause) {
    INFO_LOG("Connected to MQTT broker: " + cause);
    mqtt_client.subscribe("home/devices/#", 0);
}

void Core::connection_lost(const std::string& cause) {
    WARN_LOG("MQTT connection lost: " + cause);
}

void Core::message_arrived(mqtt::const_message_ptr msg) {
    std::string topic = msg->get_topic();
    std::string payload = msg->to_string();

    DEBUG_LOG("Message received - Topic: " + topic + ", Payload: " + payload);
    if (topic.find("home/services/") == 0) {
        HandleServiceStatus(topic, payload);
    }    
}

void Core::HandleServiceStatus(const std::string& topic, const std::string& payload) {
    DEBUG_LOG("Service status update - Topic: " + topic + ", Payload: " + payload);
    // React to service status changes if necessary
}

void Core::delivery_complete(mqtt::delivery_token_ptr token) {
    (void)token;
    DEBUG_LOG("MQTT message delivered");
}