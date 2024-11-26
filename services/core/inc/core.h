#pragma once

#include "audio_capture.h"
#include "keyword_detector.h"
#include <thread>
#include <atomic>
#include <queue>
#include <condition_variable>
#include <memory>
#include <mqtt/async_client.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;


class Core : public virtual mqtt::callback {
private:
    std::unique_ptr<AudioCapture> audio_capture_;
    std::unique_ptr<KeywordDetector> keyword_detector_;
    
    std::atomic<bool> running_{true};
    std::thread audio_thread_;
    std::thread audio_processing_thread_;

    std::queue<std::vector<int16_t>> audio_queue_;
    std::mutex audio_queue_mutex_;
    std::condition_variable audio_queue_cv_;

    mqtt::async_client mqtt_client_;
    mqtt::ssl_options mqtt_ssl_opts_;
    mqtt::connect_options mqtt_conn_opts_;

    void AudioCaptureLoop();
    void AudioProcessingLoop();

    // Topics
    const std::string STATUS_TOPIC = "home/services/core/status";

    // MQTT callback overrides
    void connected(const std::string& cause) override;
    void connection_lost(const std::string& cause) override;
    void message_arrived(mqtt::const_message_ptr msg) override;
    void delivery_complete(mqtt::delivery_token_ptr token) override;

    void InitializeMqttConnection();
    void PublishLEDManagerCommand(const std::string& command, const json& params);
    void HandleServiceStatus(const std::string& topic, const std::string& payload);
    void PublishStatus();

    std::thread worker_thread_;
    void Run();

public:
    Core(const std::string& broker_address, const std::string& client_id);
    ~Core();

    void Initialize();
    void Stop();

    Core(const Core&) = delete;
    Core& operator=(const Core&) = delete;
};