#include "kernel.h"
#include <iostream>
#include <chrono>
#include <nlohmann/json.hpp>

using json = nlohmann::json;


void Kernel::AudioCaptureLoop() {
    while(running) {
        auto buffer = audio_capture->CaptureAudio(1000);
        {
            std::lock_guard<std::mutex> lock(audio_queue_mutex);
            audio_queue.push(std::move(buffer));
        }
        audio_queue_cv.notify_one();
    }
}

void Kernel::AudioProcessingLoop() {
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
            std::cout << "Keyword detected! Listening for command..." << std::endl;
            
            // Collect audio for 5 seconds (5 buffers of 1000ms each)
            std::vector<int16_t> command_buffer;
            for (int i = 0; i < 5; ++i) {
                std::unique_lock<std::mutex> lock(audio_queue_mutex);
                audio_queue_cv.wait(lock, [this] { return !audio_queue.empty() || !running; });
                if (!running) break;
                auto next_buffer = std::move(audio_queue.front());
                audio_queue.pop();
                command_buffer.insert(command_buffer.end(), next_buffer.begin(), next_buffer.end());
            }

            Command cmd = keyword_detector->DetectCommand(command_buffer, true);

            switch(cmd) {
                case Command::TURN_ON:
                    PublishLEDManagerCommand("turn_on", json::object());
                    break;
                case Command::TURN_OFF:
                    PublishLEDManagerCommand("turn_off", json::object());
                    break;
                case Command::NO_COMMAND:
                default:
                    std::cout << "No command detected." << std::endl; 
                    break;
            }
        }
    }
}

Kernel::Kernel(const std::string& broker_address, const std::string& client_id) 
    : mqtt_client(broker_address, client_id),
      mqtt_conn_opts(mqtt::connect_options_builder()
        .keep_alive_interval(std::chrono::seconds(20))
        .clean_session(true)
        .automatic_reconnect(true)
        .finalize()) {

    // Initialize modules
    audio_capture = std::make_unique<AudioCapture>();
    keyword_detector = std::make_unique<KeywordDetector>();
    
    mqtt_client.set_callback(*this);
}

Kernel::~Kernel() {
    Stop();
}

void Kernel::Initialize() {
    try {
        mqtt::token_ptr conntok = mqtt_client.connect(mqtt_conn_opts);
        conntok->wait();
    } catch (const mqtt::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

void Kernel::Run() {
    running = true;

    // Initialize threads
    audio_thread = std::thread(&Kernel::AudioCaptureLoop, this);
    audio_processing_thread = std::thread(&Kernel::AudioProcessingLoop, this);

    // Wait for tasks
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    audio_thread.join();
    audio_processing_thread.join();
}   

void Kernel::Stop() {
    running = false;
    audio_queue_cv.notify_all();
    if (audio_thread.joinable()) audio_thread.join();
    if (audio_processing_thread.joinable()) audio_processing_thread.join();

    try {
        mqtt_client.disconnect()->wait();
    }
    catch (const mqtt::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

void Kernel::PublishLEDManagerCommand(const std::string& command, const json& params) {
    json message;
    message["command"] = command;
    message["params"] = params;
    std::string topic = "home/services/led_manager/command";
    std::string payload = message.dump();

    try {
        mqtt_client.publish(topic, payload, 1, false)->wait_for(std::chrono::seconds(10));
    } catch (const mqtt::exception& e) {
        std::cerr << "Error publishing command: " << e.what() << std::endl;
    }
}

void Kernel::connected(const std::string& cause) {
    std::cout << "Connected to MQTT broker: " << cause << std::endl;
    mqtt_client.subscribe("home/devices/#", 0);
}

void Kernel::connection_lost(const std::string& cause) {
    std::cout << "Connection lost: " << cause << std::endl;
}

void Kernel::message_arrived(mqtt::const_message_ptr msg) {
    std::string topic = msg->get_topic();
    std::string payload = msg->to_string();

    if (topic.find("home/services/") == 0) {
        HandleServiceStatus(topic, payload);
    }    
}

void Kernel::HandleServiceStatus(const std::string& topic, const std::string& payload) {
    std::cout << "Service status update: " << topic << " - " << payload << std::endl;
    // React to service status changes if necessary
}

void Kernel::delivery_complete(mqtt::delivery_token_ptr token) {
    (void)token;
    std::cout << "Message delivered" << std::endl;
}