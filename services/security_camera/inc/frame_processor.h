#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <atomic>
#include <functional>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct DetectionResult {
    bool motion_detected{false};
    int face_count{0};
    std::vector<cv::Rect> face_locations;
    double fps{0.0};
    double latency_ms{0.0};
    bool night_mode{false};
    
    json ToJson() const;
};

using MotionCallback = std::function<void(bool, const json&)>;
using FaceDetectionCallback = std::function<void(int, const json&)>;

class FrameProcessor {
public:
    FrameProcessor();
    ~FrameProcessor();

    bool Initialize();
    
    // Process a frame and return detection results
    DetectionResult ProcessFrame(cv::Mat& frame, bool night_mode);
    
    // Callback registration
    void SetMotionCallback(MotionCallback callback);
    void SetFaceDetectionCallback(FaceDetectionCallback callback);
    
    // Settings
    void EnableFaceDetection(bool enable);
    void EnableMotionDetection(bool enable);
    void SetMotionSensitivity(double sensitivity);
    
private:
    // Face detection
    cv::CascadeClassifier face_cascade_;
    std::atomic<bool> face_detection_enabled_{true};
    
    // Motion detection
    std::atomic<bool> motion_detection_enabled_{true};
    double motion_sensitivity_{25.0};
    cv::Mat prev_frame_;
    std::deque<cv::Mat> frame_buffer_;
    std::mutex buffer_mutex_;
    
    // Performance tracking
    std::chrono::time_point<std::chrono::steady_clock> last_fps_time_;
    int frame_count_{0};
    double fps_{0.0};
    
    // Callbacks
    MotionCallback motion_callback_;
    FaceDetectionCallback face_detection_callback_;
    
    // Detection methods
    std::vector<cv::Rect> DetectFaces(const cv::Mat& frame);
    bool DetectMotion(const cv::Mat& current_frame);
    
    // Helper methods
    void UpdateFPS();
    void DrawInfo(cv::Mat& frame, const DetectionResult& result);
}; 