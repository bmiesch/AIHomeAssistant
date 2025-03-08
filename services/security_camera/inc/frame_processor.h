#pragma once

#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct Detection {
    std::string class_name;
    float confidence;
    cv::Rect box;
};

struct DetectionResult {
    std::vector<Detection> detections;
    double fps{0.0};
    double latency_ms{0.0};
    
    json ToJson() const;
};

class FrameProcessor {
public:
    FrameProcessor();
    ~FrameProcessor();

    bool Initialize();
    DetectionResult ProcessFrame(cv::Mat& frame);
    
private:
    cv::dnn::Net net_;
    std::vector<std::string> class_names_;
    float conf_threshold_{0.5};
    
    // Helper methods
    std::vector<Detection> Detect(const cv::Mat& frame);
    void DrawDetections(cv::Mat& frame, const std::vector<Detection>& detections);
};