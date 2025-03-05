#include "security_camera.h"
#include "log.h"
#include <chrono>
#include <vector>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

SecurityCamera::SecurityCamera(const std::string& broker_address, const std::string& client_id, 
                             const std::string& ca_path, const std::string& username, 
                             const std::string& password)
    : PahoMqttClient(broker_address, client_id, ca_path, username, password),
      camera_capture_(std::make_unique<CameraCapture>()),
      frame_processor_(std::make_unique<FrameProcessor>()) {
    
    // Set up MQTT message callback
    SetMessageCallback([this](mqtt::const_message_ptr msg) {
        this->IncomingMessage(msg->get_topic(), msg->to_string());
    });

    // Subscribe to command topic
    Subscribe(COMMAND_TOPIC);
    
    // Get environment variables
    int camera_id = 0;
    int width = 640;
    int height = 480;
    int fps = 15;
    
    GetEnvVar("CAMERA_ID", camera_id);
    GetEnvVar("FRAME_WIDTH", width);
    GetEnvVar("FRAME_HEIGHT", height);
    GetEnvVar("FPS_TARGET", fps);
    GetEnvVar("NIGHT_MODE_THRESHOLD", night_mode_threshold_);
    
    // Initialize camera with settings
    camera_capture_ = std::make_unique<CameraCapture>(camera_id, width, height, fps);
    camera_capture_->SetNightModeThreshold(night_mode_threshold_);
    
    // Set up callbacks
    frame_processor_->SetMotionCallback([this](bool motion_detected, const json& details) {
        this->PublishMotionDetection(motion_detected, details);
    });
    
    frame_processor_->SetFaceDetectionCallback([this](int face_count, const json& details) {
        this->PublishFaceDetection(face_count, details);
    });
}

SecurityCamera::~SecurityCamera() {
    DEBUG_LOG("SecurityCamera destructor called");
    Stop();
}

void SecurityCamera::Initialize() {
    INFO_LOG("Initializing Security Camera Service");
    
    // Initialize camera
    if (!camera_capture_->Initialize()) {
        throw std::runtime_error("Failed to initialize camera");
    }
    
    // Initialize frame processor
    if (!frame_processor_->Initialize()) {
        throw std::runtime_error("Failed to initialize frame processor");
    }
    
    // Connect to MQTT broker
    Connect();
    
    // Start threads
    running_ = true;
    capture_thread_ = std::thread(&SecurityCamera::CaptureLoop, this);
    processing_thread_ = std::thread(&SecurityCamera::ProcessingLoop, this);
    worker_thread_ = std::thread(&SecurityCamera::Run, this);
    
    // Publish status
    PublishStatus("online");
    
    INFO_LOG("Security Camera Service initialized");
}

void SecurityCamera::Stop() {
    if (!running_) return;
    
    INFO_LOG("Stopping Security Camera Service");
    
    // Set running flag to false to stop threads
    running_ = false;
    
    // Notify processing thread to wake up
    frame_queue_cv_.notify_all();
    
    // Wait for threads to finish
    if (capture_thread_.joinable()) capture_thread_.join();
    if (processing_thread_.joinable()) processing_thread_.join();
    if (worker_thread_.joinable()) worker_thread_.join();
    
    // Publish offline status
    PublishStatus("offline");
    
    // Disconnect from MQTT broker
    Disconnect();
    
    INFO_LOG("Security Camera Service stopped");
}

void SecurityCamera::Run() {
    INFO_LOG("Worker thread started");
    
    while (running_) {
        // This thread can be used for periodic tasks like publishing status updates
        std::this_thread::sleep_for(std::chrono::seconds(60));
        
        if (running_) {
            PublishStatus("online");
        }
    }
    
    INFO_LOG("Worker thread stopped");
}

void SecurityCamera::CaptureLoop() {
    INFO_LOG("Capture thread started");
    
    while (running_) {
        try {
            // Capture frame
            cv::Mat frame = camera_capture_->CaptureFrame();
            
            if (frame.empty()) {
                WARN_LOG("Empty frame captured");
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            
            // Check for night mode
            bool is_night = camera_capture_->DetectNightMode(frame);
            if (is_night != night_mode_) {
                night_mode_ = is_night;
                camera_capture_->SetNightMode(night_mode_);
                INFO_LOG(std::string("Night mode ") + (night_mode_ ? "enabled" : "disabled"));
            }
            
            // Add frame to queue
            {
                std::lock_guard<std::mutex> lock(frame_queue_mutex_);
                frame_queue_.push(frame);
                
                // Limit queue size
                if (frame_queue_.size() > 10) {
                    frame_queue_.pop();
                    WARN_LOG("Frame queue overflow, dropping oldest frame");
                }
            }
            
            // Notify processing thread
            frame_queue_cv_.notify_one();
            
        } catch (const std::exception& e) {
            ERROR_LOG("Error in capture loop: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
    INFO_LOG("Capture thread stopped");
}

void SecurityCamera::ProcessingLoop() {
    INFO_LOG("Processing thread started");
    
    while (running_) {
        cv::Mat frame;
        
        // Get frame from queue
        {
            std::unique_lock<std::mutex> lock(frame_queue_mutex_);
            frame_queue_cv_.wait(lock, [this] { 
                return !frame_queue_.empty() || !running_; 
            });
            
            if (!running_) break;
            
            frame = frame_queue_.front();
            frame_queue_.pop();
        }
        
        try {
            // Process frame
            auto result = frame_processor_->ProcessFrame(frame, night_mode_);
            
            // Periodically publish a snapshot (every 100 frames with motion or faces)
            static int frame_count = 0;
            frame_count++;
            
            if ((result.motion_detected || result.face_count > 0) && frame_count >= 100) {
                PublishSnapshot(frame);
                frame_count = 0;
            }
            
        } catch (const std::exception& e) {
            ERROR_LOG("Error in processing loop: " + std::string(e.what()));
        }
    }
    
    INFO_LOG("Processing thread stopped");
}

void SecurityCamera::IncomingMessage(const std::string& topic, const std::string& payload) {
    DEBUG_LOG("Received message on topic: " + topic + ", payload: " + payload);
    
    if (topic == COMMAND_TOPIC) {
        try {
            json command = json::parse(payload);
            
            if (command.contains("action")) {
                std::string action = command["action"];
                
                if (action == "snapshot") {
                    // Capture and publish a snapshot
                    cv::Mat frame = camera_capture_->CaptureFrame();
                    if (!frame.empty()) {
                        PublishSnapshot(frame);
                    }
                } else if (action == "enable_face_detection" && command.contains("enabled")) {
                    bool enabled = command["enabled"];
                    frame_processor_->EnableFaceDetection(enabled);
                    INFO_LOG(std::string("Face detection ") + (enabled ? "enabled" : "disabled"));
                } else if (action == "enable_motion_detection" && command.contains("enabled")) {
                    bool enabled = command["enabled"];
                    frame_processor_->EnableMotionDetection(enabled);
                    INFO_LOG(std::string("Motion detection ") + (enabled ? "enabled" : "disabled"));
                } else if (action == "set_motion_sensitivity" && command.contains("sensitivity")) {
                    double sensitivity = command["sensitivity"];
                    frame_processor_->SetMotionSensitivity(sensitivity);
                    INFO_LOG("Motion sensitivity set to " + std::to_string(sensitivity));
                } else if (action == "set_night_mode_threshold" && command.contains("threshold")) {
                    int threshold = command["threshold"];
                    night_mode_threshold_ = threshold;
                    camera_capture_->SetNightModeThreshold(threshold);
                    INFO_LOG("Night mode threshold set to " + std::to_string(threshold));
                }
            }
        } catch (const std::exception& e) {
            ERROR_LOG("Error processing command: " + std::string(e.what()));
        }
    }
}

void SecurityCamera::PublishStatus(const std::string& status) {
    json payload;
    payload["status"] = status;
    payload["timestamp"] = std::time(nullptr);
    payload["night_mode"] = static_cast<bool>(night_mode_);
    payload["night_mode_threshold"] = night_mode_threshold_;
    
    Publish(STATUS_TOPIC, payload);
}

void SecurityCamera::PublishMotionDetection(bool motion_detected, const json& details) {
    json payload;
    payload["motion_detected"] = motion_detected;
    payload["timestamp"] = std::time(nullptr);
    payload["night_mode"] = static_cast<bool>(night_mode_);
    
    // Add any additional details
    for (auto& [key, value] : details.items()) {
        payload[key] = value;
    }
    
    Publish(MOTION_TOPIC, payload);
}

void SecurityCamera::PublishFaceDetection(int face_count, const json& details) {
    json payload;
    payload["face_count"] = face_count;
    payload["timestamp"] = std::time(nullptr);
    payload["night_mode"] = static_cast<bool>(night_mode_);
    
    // Add any additional details
    for (auto& [key, value] : details.items()) {
        payload[key] = value;
    }
    
    Publish(FACE_DETECTION_TOPIC, payload);
}

void SecurityCamera::PublishSnapshot(const cv::Mat& frame) {
    // Convert frame to base64
    std::string base64_image = MatToBase64(frame);
    
    json payload;
    payload["image"] = base64_image;
    payload["timestamp"] = std::time(nullptr);
    payload["night_mode"] = static_cast<bool>(night_mode_);
    payload["width"] = frame.cols;
    payload["height"] = frame.rows;
    
    Publish(SNAPSHOT_TOPIC, payload);
}

std::string SecurityCamera::MatToBase64(const cv::Mat& image) {
    std::vector<uchar> buffer;
    cv::imencode(".jpg", image, buffer);
    
    std::string base64_image = "data:image/jpeg;base64,";
    
    // Convert to base64
    static const char base64_chars[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    
    for (const uchar& byte : buffer) {
        char_array_3[i++] = byte;
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;
            
            for (i = 0; i < 4; i++) {
                base64_image += base64_chars[char_array_4[i]];
            }
            i = 0;
        }
    }
    
    if (i) {
        for (j = i; j < 3; j++) {
            char_array_3[j] = '\0';
        }
        
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        
        for (j = 0; j < i + 1; j++) {
            base64_image += base64_chars[char_array_4[j]];
        }
        
        while (i++ < 3) {
            base64_image += '=';
        }
    }
    
    return base64_image;
}

bool SecurityCamera::GetEnvVar(const std::string& name, std::string& value) {
    const char* env_value = std::getenv(name.c_str());
    if (env_value) {
        value = std::string(env_value);
        return true;
    }
    return false;
}

bool SecurityCamera::GetEnvVar(const std::string& name, int& value) {
    std::string str_value;
    if (GetEnvVar(name, str_value)) {
        try {
            value = std::stoi(str_value);
            return true;
        } catch (const std::exception& e) {
            ERROR_LOG("Error converting environment variable " + name + " to int: " + e.what());
        }
    }
    return false;
} 