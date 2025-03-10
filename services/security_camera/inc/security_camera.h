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
#include <vector>
#include <map>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <ctime>

#include "camera_capture.h"
#include "frame_processor.h"
#include "paho_mqtt_client.h"
#include "service_interface.h"

using json = nlohmann::json;

// Token expiration time in seconds (1 hour)
constexpr int TOKEN_EXPIRATION_TIME = 3600;

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
    static constexpr const char* STREAM_TOPIC = "home/services/security_camera/stream";
    static constexpr const char* TOKEN_TOPIC = "home/services/security_camera/token";

    // State
    std::atomic<bool> running_{true};
    std::atomic<bool> streaming_{false};
    std::atomic<int> stream_port_{8080};
    std::string stream_url_;
    std::string cert_file_;
    std::string key_file_;
    bool use_https_{true};
    
    // SSL context
    SSL_CTX* ssl_ctx_{nullptr};

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
    
    // Latest frame for streaming
    cv::Mat latest_frame_;
    std::mutex latest_frame_mutex_;
    
    // Streaming clients
    struct ClientInfo {
        int socket;
        SSL* ssl;
    };
    std::vector<ClientInfo> stream_clients_;
    std::mutex stream_clients_mutex_;
    
    // Token authentication
    std::map<std::string, time_t> valid_tokens_;
    std::mutex tokens_mutex_;
    
    // Threads
    std::thread capture_thread_;
    std::thread processing_thread_;
    std::thread worker_thread_;
    std::thread stream_server_thread_;

    // Thread management and IService interface implementation
    void Run() override;

    // MQTT handling
    void IncomingMessage(const std::string& topic, const std::string& payload);
    void PublishStatus(const std::string& status);
    void PublishSnapshot(const cv::Mat& frame);
    void PublishStreamInfo(bool streaming, const std::string& url = "");
    void PublishToken(const std::string& token);

    // Processing loops
    void CaptureLoop();
    void ProcessingLoop();
    void ProcessCommand(const json& command);
    void StreamServerLoop();

    // Streaming methods
    bool StartStreaming();
    void StopStreaming();
    void HandleStreamClient(int client_socket);
    void SendMJPEGFrame(SSL* ssl, int client_socket, const cv::Mat& frame);
    
    // Token authentication
    std::string GenerateToken();
    bool ValidateToken(const std::string& token);
    void CleanupExpiredTokens();
    bool ParseHttpRequest(const std::string& request, std::map<std::string, std::string>& headers, std::string& path);

    // SSL/TLS methods
    bool InitializeSSL();
    void CleanupSSL();

    // Helper methods
    std::string MatToBase64(const cv::Mat& image);
    bool GetEnvVar(const std::string& name, std::string& value);
    bool GetEnvVar(const std::string& name, int& value);
    bool GetEnvVar(const std::string& name, bool& value);
}; 