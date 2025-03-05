#include "frame_processor.h"
#include "log.h"
#include <chrono>
#include <opencv2/imgproc.hpp>

json DetectionResult::ToJson() const {
    json result;
    result["motion_detected"] = motion_detected;
    result["face_count"] = face_count;
    result["fps"] = fps;
    result["latency_ms"] = latency_ms;
    result["night_mode"] = night_mode;
    
    // Add face locations
    json face_locs = json::array();
    for (const auto& rect : face_locations) {
        json face_rect;
        face_rect["x"] = rect.x;
        face_rect["y"] = rect.y;
        face_rect["width"] = rect.width;
        face_rect["height"] = rect.height;
        face_locs.push_back(face_rect);
    }
    result["face_locations"] = face_locs;
    
    return result;
}

FrameProcessor::FrameProcessor()
    : last_fps_time_(std::chrono::steady_clock::now()) {
}

FrameProcessor::~FrameProcessor() {
}

bool FrameProcessor::Initialize() {
    INFO_LOG("Initializing frame processor");
    
    // Load face cascade classifier
    std::string cascade_path = cv::samples::findFile("haarcascades/haarcascade_frontalface_default.xml");
    if (!face_cascade_.load(cascade_path)) {
        ERROR_LOG("Error loading face cascade classifier");
        // Continue without face detection
        face_detection_enabled_ = false;
        return true; // Still return true as this is not critical
    }
    
    INFO_LOG("Frame processor initialized successfully");
    return true;
}

DetectionResult FrameProcessor::ProcessFrame(cv::Mat& frame, bool night_mode) {
    auto start_time = std::chrono::steady_clock::now();
    DetectionResult result;
    result.night_mode = night_mode;
    
    if (frame.empty()) {
        return result;
    }
    
    // Update FPS
    UpdateFPS();
    result.fps = fps_;
    
    // Detect faces if enabled
    if (face_detection_enabled_) {
        result.face_locations = DetectFaces(frame);
        result.face_count = result.face_locations.size();
        
        // Call face detection callback if faces detected and callback is set
        if (result.face_count > 0 && face_detection_callback_) {
            face_detection_callback_(result.face_count, result.ToJson());
        }
    }
    
    // Detect motion if enabled
    if (motion_detection_enabled_) {
        result.motion_detected = DetectMotion(frame);
        
        // Call motion callback if motion detected and callback is set
        if (result.motion_detected && motion_callback_) {
            motion_callback_(true, result.ToJson());
        }
    }
    
    // Calculate latency
    auto end_time = std::chrono::steady_clock::now();
    result.latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    
    // Draw information on frame
    DrawInfo(frame, result);
    
    return result;
}

void FrameProcessor::SetMotionCallback(MotionCallback callback) {
    motion_callback_ = callback;
}

void FrameProcessor::SetFaceDetectionCallback(FaceDetectionCallback callback) {
    face_detection_callback_ = callback;
}

void FrameProcessor::EnableFaceDetection(bool enable) {
    face_detection_enabled_ = enable;
}

void FrameProcessor::EnableMotionDetection(bool enable) {
    motion_detection_enabled_ = enable;
}

void FrameProcessor::SetMotionSensitivity(double sensitivity) {
    motion_sensitivity_ = sensitivity;
}

std::vector<cv::Rect> FrameProcessor::DetectFaces(const cv::Mat& frame) {
    std::vector<cv::Rect> faces;
    
    // Convert to grayscale for face detection
    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    
    // Detect faces
    face_cascade_.detectMultiScale(gray, faces, 1.1, 4, 0, cv::Size(30, 30));
    
    return faces;
}

bool FrameProcessor::DetectMotion(const cv::Mat& current_frame) {
    // If this is the first frame, initialize and return false
    if (prev_frame_.empty()) {
        cv::cvtColor(current_frame, prev_frame_, cv::COLOR_BGR2GRAY);
        return false;
    }
    
    // Convert current frame to grayscale
    cv::Mat gray;
    cv::cvtColor(current_frame, gray, cv::COLOR_BGR2GRAY);
    
    // Calculate absolute difference between current and previous frame
    cv::Mat diff;
    cv::absdiff(prev_frame_, gray, diff);
    
    // Apply threshold to difference
    cv::Mat thresh;
    cv::threshold(diff, thresh, 25, 255, cv::THRESH_BINARY);
    
    // Dilate to fill in holes
    cv::dilate(thresh, thresh, cv::Mat(), cv::Point(-1, -1), 2);
    
    // Find contours
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(thresh, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    
    // Calculate total area of motion
    double total_area = 0;
    for (const auto& contour : contours) {
        total_area += cv::contourArea(contour);
    }
    
    // Calculate percentage of frame with motion
    double frame_area = current_frame.cols * current_frame.rows;
    double motion_percentage = (total_area / frame_area) * 100.0;
    
    // Update previous frame
    gray.copyTo(prev_frame_);
    
    // Detect motion based on sensitivity
    return motion_percentage > motion_sensitivity_;
}

void FrameProcessor::UpdateFPS() {
    frame_count_++;
    
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_fps_time_).count();
    
    if (duration >= 1000) {
        fps_ = frame_count_ * 1000.0 / duration;
        frame_count_ = 0;
        last_fps_time_ = now;
    }
}

void FrameProcessor::DrawInfo(cv::Mat& frame, const DetectionResult& result) {
    // Draw rectangles around faces
    for (const auto& face : result.face_locations) {
        cv::rectangle(frame, face, cv::Scalar(0, 255, 0), 2);
    }
    
    // Draw FPS
    cv::putText(frame, "FPS: " + std::to_string(static_cast<int>(result.fps)), 
                cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
    
    // Draw latency
    cv::putText(frame, "Latency: " + std::to_string(static_cast<int>(result.latency_ms)) + " ms", 
                cv::Point(10, 70), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
    
    // Draw night mode indicator
    if (result.night_mode) {
        cv::putText(frame, "NIGHT MODE", cv::Point(10, 110), 
                    cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 165, 255), 2);
    }
    
    // Draw motion detection indicator
    if (result.motion_detected) {
        cv::putText(frame, "MOTION DETECTED", cv::Point(10, frame.rows - 10), 
                    cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 0, 255), 2);
    }
    
    // Draw face count
    if (result.face_count > 0) {
        cv::putText(frame, "Faces: " + std::to_string(result.face_count), 
                    cv::Point(frame.cols - 200, 30), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
    }
} 