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

    // Subscribe to topics
    Subscribe(COMMAND_TOPIC);
}

Core::~Core() {
    DEBUG_LOG("Core destructor called");
    Stop();
}

void Core::AudioCaptureLoop() {
    // auto last_capture_time = std::chrono::steady_clock::now();
    // int frame_count = 0;
    
    while(running_) {
        try {
            auto frame = audio_capture_->CapturePorcupineFrame();
            if (!running_) break;
            
            // Log timing every 100 frames (roughly 3.2 seconds)
            // frame_count++;
            // if (frame_count % 100 == 0) {
            //     auto now = std::chrono::steady_clock::now();
            //     auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            //         now - last_capture_time).count();
            //     float fps = 100000.0f / duration;
            //     DEBUG_LOG("Audio capture rate: " + std::to_string(fps) + " fps, Queue size: " 
            //         + std::to_string(audio_queue_.size()));
            //     last_capture_time = now;
            // }
            
            {
                std::lock_guard<std::mutex> lock(audio_queue_mutex_);
                if (audio_queue_.size() > 125) {
                    ERROR_LOG("Audio queue overflow! Queue size: " + std::to_string(audio_queue_.size()) 
                        + " frames (" + std::to_string(audio_queue_.size() * 32) + "ms of audio)");
                }
                audio_queue_.push(std::move(frame));
            }
            audio_queue_cv_.notify_one();
        } catch (const std::exception& e) {
            ERROR_LOG("Exception in audio capture: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

void Core::AudioProcessingLoop() {
    static constexpr size_t FRAMES_FOR_COMMAND = 125;
    // auto last_process_time = std::chrono::steady_clock::now();
    // int frame_count = 0;

    while(running_) {
        std::vector<int16_t> frame;
        {
            std::unique_lock<std::mutex> lock(audio_queue_mutex_);
            audio_queue_cv_.wait(lock, [this] { return !audio_queue_.empty() || !running_; });
            if (!running_) break;
            frame = std::move(audio_queue_.front());
            audio_queue_.pop();
        }

        // frame_count++;
        // if (frame_count % 100 == 0) {
        //     auto now = std::chrono::steady_clock::now();
        //     auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        //         now - last_process_time).count();
        //     float fps = 100000.0f / duration;
        //     DEBUG_LOG("Audio processing rate: " + std::to_string(fps) + " fps");
        //     last_process_time = now;
        // }

        if (running_ && keyword_detector_->DetectKeyword(frame, true)) {
            INFO_LOG("Keyword detected! Listening for command...");
            
            // Clear any backlogged audio
            {
                std::lock_guard<std::mutex> lock(audio_queue_mutex_);
                std::queue<std::vector<int16_t>> empty;
                std::swap(audio_queue_, empty);
            }
            
            // Collect 4 seconds of audio for command detection
            std::vector<int16_t> command_buffer;
            command_buffer.reserve(FRAMES_FOR_COMMAND * 512);
            
            for (size_t i = 0; i < FRAMES_FOR_COMMAND; ++i) {
                std::unique_lock<std::mutex> lock(audio_queue_mutex_);
                if (!audio_queue_cv_.wait_for(lock, std::chrono::milliseconds(100),
                    [this] { return !audio_queue_.empty() || !running_; })) {
                    continue;
                }
                if (!running_) break;
                
                auto next_frame = std::move(audio_queue_.front());
                audio_queue_.pop();
                command_buffer.insert(command_buffer.end(), next_frame.begin(), next_frame.end());
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
                nlohmann::json status_msg = {{"status", "online"}};
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
    std::string topic = LED_MANAGER_COMMAND_TOPIC;
    std::string payload = message.dump();

    DEBUG_LOG("Publishing command: " + command + " to topic: " + topic);
    Publish(topic, payload);
}

void Core::HandleServiceStatus(const std::string& topic, const std::string& payload) {
    DEBUG_LOG("Service status update - Topic: " + topic + ", Payload: " + payload);
    // React to service status changes if necessary
}
