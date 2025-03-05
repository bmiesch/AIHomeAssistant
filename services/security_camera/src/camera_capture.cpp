#include "camera_capture.h"
#include "log.h"
#include <opencv2/imgproc.hpp>

CameraCapture::CameraCapture(int camera_id, int width, int height, int fps)
    : camera_id_(camera_id), width_(width), height_(height), fps_(fps) {
}

CameraCapture::~CameraCapture() {
    if (cap_.isOpened()) {
        cap_.release();
    }
}

bool CameraCapture::Initialize() {
    INFO_LOG("Initializing camera with ID: " + std::to_string(camera_id_));
    
    // Open camera
    cap_.open(camera_id_);
    if (!cap_.isOpened()) {
        ERROR_LOG("Failed to open camera with ID: " + std::to_string(camera_id_));
        return false;
    }
    
    // Set camera properties
    SetResolution(width_, height_);
    SetFPS(fps_);
    
    INFO_LOG("Camera initialized successfully");
    return true;
}

cv::Mat CameraCapture::CaptureFrame() {
    cv::Mat frame;
    
    if (!cap_.isOpened()) {
        ERROR_LOG("Camera is not opened");
        return frame;
    }
    
    // Capture frame
    cap_ >> frame;
    
    if (frame.empty()) {
        WARN_LOG("Empty frame captured");
        return frame;
    }
    
    // Apply night vision enhancement if in night mode
    if (night_mode_) {
        frame = EnhanceNightVision(frame);
    }
    
    return frame;
}

bool CameraCapture::IsOpened() const {
    return cap_.isOpened();
}

void CameraCapture::SetNightMode(bool enabled) {
    night_mode_ = enabled;
}

bool CameraCapture::IsNightMode() const {
    return night_mode_;
}

void CameraCapture::SetNightModeThreshold(int threshold) {
    night_mode_threshold_ = threshold;
}

int CameraCapture::GetNightModeThreshold() const {
    return night_mode_threshold_;
}

bool CameraCapture::DetectNightMode(const cv::Mat& frame) const {
    if (frame.empty()) {
        return false;
    }
    
    // Convert to HSV and calculate average brightness
    cv::Mat hsv;
    cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);
    
    // Calculate average brightness (V channel)
    cv::Scalar mean = cv::mean(hsv);
    double avg_brightness = mean[2]; // V channel
    
    // Detect night mode based on threshold
    return avg_brightness < night_mode_threshold_;
}

void CameraCapture::SetResolution(int width, int height) {
    width_ = width;
    height_ = height;
    
    cap_.set(cv::CAP_PROP_FRAME_WIDTH, width_);
    cap_.set(cv::CAP_PROP_FRAME_HEIGHT, height_);
    
    // Verify that the resolution was set correctly
    double actual_width = cap_.get(cv::CAP_PROP_FRAME_WIDTH);
    double actual_height = cap_.get(cv::CAP_PROP_FRAME_HEIGHT);
    
    if (actual_width != width_ || actual_height != height_) {
        WARN_LOG("Requested resolution (" + std::to_string(width_) + "x" + std::to_string(height_) + 
                ") not supported. Using " + std::to_string(static_cast<int>(actual_width)) + "x" + 
                std::to_string(static_cast<int>(actual_height)) + " instead.");
        
        width_ = static_cast<int>(actual_width);
        height_ = static_cast<int>(actual_height);
    }
}

void CameraCapture::SetFPS(int fps) {
    fps_ = fps;
    cap_.set(cv::CAP_PROP_FPS, fps_);
    
    // Verify that the FPS was set correctly
    double actual_fps = cap_.get(cv::CAP_PROP_FPS);
    
    if (actual_fps != fps_) {
        WARN_LOG("Requested FPS (" + std::to_string(fps_) + ") not supported. Using " + 
                std::to_string(static_cast<int>(actual_fps)) + " instead.");
        
        fps_ = static_cast<int>(actual_fps);
    }
}

cv::Mat CameraCapture::EnhanceNightVision(const cv::Mat& frame) const {
    if (frame.empty()) {
        return frame;
    }
    
    // Convert to YUV color space
    cv::Mat yuv;
    cv::cvtColor(frame, yuv, cv::COLOR_BGR2YUV);
    
    // Split channels
    std::vector<cv::Mat> yuv_channels(3);
    cv::split(yuv, yuv_channels);
    
    // Apply histogram equalization to Y channel
    cv::equalizeHist(yuv_channels[0], yuv_channels[0]);
    
    // Merge channels back
    cv::merge(yuv_channels, yuv);
    
    // Convert back to BGR
    cv::Mat enhanced;
    cv::cvtColor(yuv, enhanced, cv::COLOR_YUV2BGR);
    
    // Apply brightness and contrast adjustment
    enhanced = AdjustBrightnessContrast(enhanced, 10, 20);
    
    return enhanced;
}

cv::Mat CameraCapture::AdjustBrightnessContrast(const cv::Mat& image, int brightness, int contrast) const {
    cv::Mat adjusted = image.clone();
    
    // Apply brightness and contrast adjustment
    double alpha = 1.0 + contrast / 100.0;
    int beta = brightness;
    
    adjusted.convertTo(adjusted, -1, alpha, beta);
    
    return adjusted;
} 