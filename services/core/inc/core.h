#pragma once

#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <string>
#include <nlohmann/json.hpp>

#include "audio_capture.h"
#include "keyword_detector.h"
#include "paho_mqtt_client.h"
#include "service_interface.h"

using json = nlohmann::json;

class Core : public IService, public PahoMqttClient {
public:
    // Constructor & Destructor
    Core(const std::string& broker_address, 
         const std::string& client_id, 
         const std::string& ca_path, 
         const std::string& username, 
         const std::string& password);
    ~Core() override;

    // Delete copy constructor and assignment operator
    Core(const Core&) = delete;
    Core& operator=(const Core&) = delete;

    // IService interface implementation
    void Initialize() override;
    void Stop() override;

private:
    // Constants
    static constexpr const char* STATUS_TOPIC = "home/services/core/status";
    static constexpr const char* COMMAND_TOPIC = "home/services/core/command"; // TODO: Implement
    static constexpr const char* LED_MANAGER_COMMAND_TOPIC = "home/services/led_manager/command";

    // State
    std::atomic<bool> running_{true};

    // Audio processing
    std::unique_ptr<AudioCapture> audio_capture_;
    std::unique_ptr<KeywordDetector> keyword_detector_;
    std::queue<std::vector<int16_t>> audio_queue_;
    std::mutex audio_queue_mutex_;
    std::condition_variable audio_queue_cv_;
    std::thread audio_thread_;
    std::thread audio_processing_thread_;

    // Thread management
    std::thread worker_thread_;
    void Run() override;

    // MQTT handling
    void IncomingMessage(const std::string& topic, const std::string& payload);
    void PublishLEDManagerCommand(const std::string& command, const json& params);
    void HandleServiceStatus(const std::string& topic, const std::string& payload);

    // Audio processing loops
    void AudioCaptureLoop();
    void AudioProcessingLoop();
};