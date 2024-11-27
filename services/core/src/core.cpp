#include "core.h"
#include "log.h"
#include <chrono>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

using json = nlohmann::json;


Core::Core(const std::string& broker_address, const std::string& client_id, 
    const std::string& ca_path, const std::string& username, const std::string& password) 
    : PahoMqttClient(broker_address, client_id, ca_path, username, password),
      audio_capture_(std::make_unique<AudioCapture>()),
      keyword_detector_(std::make_unique<KeywordDetector>()) {

    SetMessageCallback([this](mqtt::const_message_ptr msg) {
        this->IncomingMessage(msg->get_topic(), msg->to_string());
    });
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
    Connect();

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
            try {
                nlohmann::json status_msg = {{"status", "offline"}};
                Publish(STATUS_TOPIC, status_msg);
            } catch (const std::exception& e) {
                ERROR_LOG("Exception in status update: " + std::string(e.what()));
            }
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
        nlohmann::json status_msg = {{"status", "offline"}};
        Publish(STATUS_TOPIC, status_msg);
        Disconnect();
        DEBUG_LOG("MQTT client disconnected");
    }
    catch (const mqtt::exception& e) {
        ERROR_LOG("MQTT disconnect error: " + std::string(e.what()));
    }
}

void Core::IncomingMessage(const std::string& topic, const std::string& payload) {
    DEBUG_LOG("Message received - Topic: " + topic + ", Payload: " + payload);
    if (topic.find("home/services/") == 0) {
        HandleServiceStatus(topic, payload);
    }
}

void Core::PublishLEDManagerCommand(const std::string& command, const json& params) {
    json message{{"command", command}, {"params", params}};
    std::string topic = "home/services/led_manager/command";
    std::string payload = message.dump();

    DEBUG_LOG("Publishing command: " + command + " to topic: " + topic);
    Publish(topic, payload);
}

void Core::HandleServiceStatus(const std::string& topic, const std::string& payload) {
    DEBUG_LOG("Service status update - Topic: " + topic + ", Payload: " + payload);
    // React to service status changes if necessary
}
