#include "core.h"
#include "log.h"
#include <chrono>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

using json = nlohmann::json;


Core::Core(const std::string& broker_address, const std::string& client_id) 
    : audio_capture_(std::make_unique<AudioCapture>()),
      keyword_detector_(std::make_unique<KeywordDetector>()),
      mqtt_client_(broker_address, client_id) {
    
    try {
        InitializeMqttConnection();
        mqtt_client_.set_callback(*this);
        INFO_LOG("Core initialized with broker: " + broker_address + ", client_id: " + client_id);
    }
    catch (const std::exception& e) {
        ERROR_LOG("Failed to initialize Core: " + std::string(e.what()));
        throw;
    }
}

Core::~Core() {
    DEBUG_LOG("Core destructor called");
    Stop();
}

void Core::AudioCaptureLoop() {
    while(running_) {
        try {
            auto buffer = audio_capture_->CaptureAudio(1000);
            if (!running_) break;
            
            if (buffer.empty()) {
                WARN_LOG("Received empty buffer from audio capture");
                continue;
            }
            
            {
                std::lock_guard<std::mutex> lock(audio_queue_mutex_);
                if (audio_queue_.size() > 5) {
                    WARN_LOG("Audio queue overflow: " + std::to_string(audio_queue_.size()) + " buffers");
                    while (audio_queue_.size() > 3) {
                        audio_queue_.pop();
                    }
                }
                audio_queue_.push(std::move(buffer));
            }
            audio_queue_cv_.notify_one();
        } catch (const std::exception& e) {
            ERROR_LOG("Exception in audio capture: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}


void Core::AudioProcessingLoop() {
    while(running_) {
        std::vector<int16_t> buffer;
        {
            std::unique_lock<std::mutex> lock(audio_queue_mutex_);
            audio_queue_cv_.wait(lock, [this] { return !audio_queue_.empty() || !running_; });
            if (!running_) break;
            buffer = std::move(audio_queue_.front());
            audio_queue_.pop();
        }

        if (running_ && keyword_detector_->DetectKeyword(buffer, true)) {
            if (!running_) break;
            
            // Clear any backlogged audio before starting command collection
            {
                std::lock_guard<std::mutex> lock(audio_queue_mutex_);
                std::queue<std::vector<int16_t>> empty;
                std::swap(audio_queue_, empty);
            }
            INFO_LOG("Keyword detected! Listening for command...");
            
            // Collect audio for 5 seconds (5 buffers of 1000ms each)
            std::vector<int16_t> command_buffer;
            for (int i = 0; i < 5; ++i) {
                std::unique_lock<std::mutex> lock(audio_queue_mutex_);
                // Wait for audio data to be available in the queue or for the running flag to be false
                audio_queue_cv_.wait(lock, [this] {return !audio_queue_.empty() || !running_; });
                if (!running_) break;
                if (audio_queue_.empty()) {
                    WARN_LOG("Missed audio buffer during command collection");
                    continue;
                }
                auto next_buffer = std::move(audio_queue_.front());
                audio_queue_.pop();
                command_buffer.insert(command_buffer.end(), next_buffer.begin(), next_buffer.end());
            }

            if (!running_) break;
            Command cmd = keyword_detector_->DetectCommand(command_buffer, true);
            if (!running_) break;

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

void Core::Initialize() {
    try {
        INFO_LOG("Connecting to MQTT broker...");
        mqtt::token_ptr conntok = mqtt_client_.connect(mqtt_conn_opts_);
        conntok->wait();
    } catch (const mqtt::exception& e) {
        ERROR_LOG("MQTT connection error: " + std::string(e.what()));
        throw;
    }

    INFO_LOG("Starting main worker thread");
    worker_thread_ = std::thread(&Core::Run, this);
}

void Core::Run() {
    running_ = true;
    INFO_LOG("Starting Core threads");

    INFO_LOG("Starting audio capture thread");
    audio_thread_ = std::thread(&Core::AudioCaptureLoop, this);

    INFO_LOG("Starting audio processing thread");
    audio_processing_thread_ = std::thread(&Core::AudioProcessingLoop, this);

    auto last_status_time = std::chrono::steady_clock::now();
    const auto status_interval = std::chrono::seconds(5);

    while (running_) {

        auto now = std::chrono::steady_clock::now();
        if (now - last_status_time >= status_interval) {
            PublishStatus();
            last_status_time = now;
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}   

void Core::Stop() {
    INFO_LOG("Stopping Core");
    running_ = false;

    // Clear any pending audio data
    {
        std::lock_guard<std::mutex> lock(audio_queue_mutex_);
        std::queue<std::vector<int16_t>> empty;
        std::swap(audio_queue_, empty);
    }
    audio_queue_cv_.notify_all();
    
    if (audio_processing_thread_.joinable()) audio_processing_thread_.join();
    if (audio_thread_.joinable()) audio_thread_.join();
    if (worker_thread_.joinable()) worker_thread_.join();

    try {
        mqtt_client_.publish(STATUS_TOPIC, "{\"status\": \"offline\"}", 1, false);
        mqtt_client_.disconnect()->wait();
        DEBUG_LOG("MQTT client disconnected");
    }
    catch (const mqtt::exception& e) {
        ERROR_LOG("MQTT disconnect error: " + std::string(e.what()));
    }
}

void Core::InitializeMqttConnection() {
    auto getEnvVar = [](const char* name) -> std::string {
        const char* value = std::getenv(name);
        if (!value) {
            throw std::runtime_error(std::string("Environment variable not set: ") + name);
        }
        return std::string(value);
    };

    const auto username = getEnvVar("MQTT_USERNAME");
    const auto password = getEnvVar("MQTT_PASSWORD");
    const auto ca_path = getEnvVar("MQTT_CA_DIR") + "/ca.crt";

    // Test CA certificate file access
    {
        std::ifstream cert_file(ca_path);
        if (!cert_file.good()) {
            ERROR_LOG("Cannot read CA certificate at: " + ca_path);
            throw std::runtime_error("CA certificate not readable");
        }
        INFO_LOG("Successfully opened CA certificate");
    }

    mqtt::will_options will_opts(STATUS_TOPIC, mqtt::binary_ref("offline"), 1, false);

    try {
        mqtt_ssl_opts_ = mqtt::ssl_options_builder()
            .trust_store(ca_path)
            .enable_server_cert_auth(true)
            .finalize();

        mqtt_conn_opts_ = mqtt::connect_options_builder()
            .keep_alive_interval(std::chrono::seconds(20))
            .clean_session(true)
            .automatic_reconnect(true)
            .user_name(username)
            .password(password)
            .will(will_opts)
            .ssl(mqtt_ssl_opts_)
            .finalize();
    }
    catch (const mqtt::exception& e) {
        throw std::runtime_error("MQTT configuration failed: " + std::string(e.what()));
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
        mqtt_client_.publish(topic, payload, 1, false)->wait_for(std::chrono::seconds(10));
    } catch (const mqtt::exception& e) {
        ERROR_LOG("Error publishing command: " + std::string(e.what()));
        throw;
    }
}

void Core::HandleServiceStatus(const std::string& topic, const std::string& payload) {
    DEBUG_LOG("Service status update - Topic: " + topic + ", Payload: " + payload);
    // React to service status changes if necessary
}

void Core::PublishStatus() {
    try {
        mqtt_client_.publish(STATUS_TOPIC, R"({"status":"online"})", 1, false);
    } catch (const mqtt::exception& e) {
        ERROR_LOG("Error publishing status: " + std::string(e.what()));
    }
}

/*
 * MQTT Callback Functions
 * These functions override the virtual callbacks from mqtt::callback
 * - connected: Called when connection to broker is established
 * - connection_lost: Called when connection to broker is lost
 * - message_arrived: Called when a message is received on a subscribed topic
 * - delivery_complete: Called when a message publish is completed
 */
void Core::connected(const std::string& cause) {
    INFO_LOG("Connected to MQTT broker: " + cause);
    mqtt_client_.subscribe("home/devices/#", 0);
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

void Core::delivery_complete(mqtt::delivery_token_ptr token) {
    (void)token;
    DEBUG_LOG("MQTT message delivered");
}