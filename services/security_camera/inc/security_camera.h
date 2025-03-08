#pragma once

#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <string>
#include <queue>
#include <opencv2/opencv.hpp>
#include <nlohmann/json.hpp>

#include "camera_capture.h"
#include "frame_processor.h"
#include "paho_mqtt_client.h"
#include "service_interface.h"

using json = nlohmann::json;

class SecurityCamera : public IService, public PahoMqttClient {
public:
    SecurityCamera(const std::string& broker_address, 
                  const std::string& client_id, 
                  const std::string& ca_path, 
                  const std::string& username, 
                  const std::string& password);
    ~SecurityCamera() override;

    SecurityCamera(const SecurityCamera&) = delete;
    SecurityCamera& operator=(const SecurityCamera&) = delete;

    // IService interface implementation
    void Initialize() override;
    void Stop() override;

private:
    // Constants
    static constexpr const char* STATUS_TOPIC = "home/services/security_camera/status";
    static constexpr const char* COMMAND_TOPIC = "home/services/security_camera/command";
    static constexpr const char* DETECTIONS_TOPIC = "home/services/security_camera/detections";
    static constexpr const char* SNAPSHOT_TOPIC = "home/services/security_camera/snapshot";

    // State
    std::atomic<bool> running_{true};

    // Command queue
    std::queue<json> command_queue_;
    std::mutex command_queue_mutex_;
    std::condition_variable command_queue_cv_;

    // Camera components
    std::unique_ptr<CameraCapture> camera_capture_;
    std::unique_ptr<FrameProcessor> frame_processor_;
    std::queue<cv::Mat> frame_queue_;
    std::mutex frame_queue_mutex_;
    std::condition_variable frame_queue_cv_;
    
    // Threads
    std::thread capture_thread_;
    std::thread processing_thread_;
    std::thread worker_thread_;

    // Thread management and IService interface implementation
    void Run() override;

    // MQTT handling
    void IncomingMessage(const std::string& topic, const std::string& payload);
    void PublishStatus(const std::string& status);
    void PublishSnapshot(const cv::Mat& frame);

    // Processing loops
    void CaptureLoop();
    void ProcessingLoop();
    void ProcessCommand(const json& command);

    // Helper methods
    std::string MatToBase64(const cv::Mat& image);
    bool GetEnvVar(const std::string& name, std::string& value);
    bool GetEnvVar(const std::string& name, int& value);
}; 