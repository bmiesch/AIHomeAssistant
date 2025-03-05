#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <atomic>

class CameraCapture {
public:
    CameraCapture(int camera_id = 0, int width = 640, int height = 480, int fps = 15);
    ~CameraCapture();

    bool Initialize();
    cv::Mat CaptureFrame();
    bool IsOpened() const;
    
    // Night mode settings
    void SetNightMode(bool enabled);
    bool IsNightMode() const;
    void SetNightModeThreshold(int threshold);
    int GetNightModeThreshold() const;
    bool DetectNightMode(const cv::Mat& frame) const;

    // Camera settings
    void SetResolution(int width, int height);
    void SetFPS(int fps);
    
private:
    cv::VideoCapture cap_;
    int camera_id_;
    int width_;
    int height_;
    int fps_;
    std::atomic<bool> night_mode_{false};
    int night_mode_threshold_{50};

    // Night vision enhancement
    cv::Mat EnhanceNightVision(const cv::Mat& frame) const;
    cv::Mat AdjustBrightnessContrast(const cv::Mat& image, int brightness = 0, int contrast = 0) const;
}; 