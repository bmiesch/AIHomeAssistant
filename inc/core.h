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
    std::unique_ptr<AudioCapture> audio_capture;
    std::unique_ptr<KeywordDetector> keyword_detector;
    
    std::atomic<bool> running{true};
    std::thread audio_thread;
    std::thread audio_processing_thread;

    std::queue<std::vector<int16_t>> audio_queue;
    std::mutex audio_queue_mutex;
    std::condition_variable audio_queue_cv;

    mqtt::async_client mqtt_client;
    mqtt::connect_options mqtt_conn_opts;

    void AudioCaptureLoop();
    void AudioProcessingLoop();

    // MQTT callback overrides
    void connected(const std::string& cause) override;
    void connection_lost(const std::string& cause) override;
    void message_arrived(mqtt::const_message_ptr msg) override;
    void delivery_complete(mqtt::delivery_token_ptr token) override;

    void PublishLEDManagerCommand(const std::string& command, const json& params);
    void HandleServiceStatus(const std::string& topic, const std::string& payload);

    std::thread worker_thread;
    void Run();

public:
    Core(const std::string& broker_address, const std::string& client_id);
    ~Core();

    void Initialize();
    void Stop();
};