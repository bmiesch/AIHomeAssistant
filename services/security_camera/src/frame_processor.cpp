#include "frame_processor.h"
#include "log.h"
#include <chrono>
#include <fstream>

json DetectionResult::ToJson() const {
    json result;
    json detections_array = json::array();
    
    for (const auto& det : detections) {
        json detection;
        detection["class"] = det.class_name;
        detection["confidence"] = det.confidence;
        detection["box"] = {
            {"x", det.box.x},
            {"y", det.box.y},
            {"width", det.box.width},
            {"height", det.box.height}
        };
        detections_array.push_back(detection);
    }
    
    result["detections"] = detections_array;
    result["fps"] = fps;
    result["latency_ms"] = latency_ms;
    
    return result;
}

FrameProcessor::FrameProcessor() {
    // Initialize class names we care about
    class_names_ = {"person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck", "boat",
                   "bird", "cat", "dog", "horse", "sheep", "cow", "elephant", "bear", "zebra", "giraffe"};
}

FrameProcessor::~FrameProcessor() = default;

bool FrameProcessor::Initialize() {
    try {
        // Load YOLO network
        net_ = cv::dnn::readNetFromDarknet(
            "/usr/local/lib/security_camera/yolov3.cfg",
            "/usr/local/lib/security_camera/yolov3.weights"
        );

        net_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        net_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        INFO_LOG("Using CPU backend for inference");

        INFO_LOG("Frame processor initialized successfully");
        return true;
    } catch (const cv::Exception& e) {
        ERROR_LOG("OpenCV error during initialization: " + std::string(e.what()));
        return false;
    } catch (const std::exception& e) {
        ERROR_LOG("Error during initialization: " + std::string(e.what()));
        return false;
    }
}

DetectionResult FrameProcessor::ProcessFrame(cv::Mat& frame) {
    auto start = std::chrono::steady_clock::now();
    
    DetectionResult result;
    result.detections = Detect(frame);
    DrawDetections(frame, result.detections);
    
    auto end = std::chrono::steady_clock::now();
    result.latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    // Calculate FPS
    static auto last_time = start;
    static int frame_count = 0;
    frame_count++;
    
    auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(end - last_time).count();
    if (time_diff >= 1000) {  // Update FPS every second
        result.fps = frame_count * 1000.0 / time_diff;
        frame_count = 0;
        last_time = end;
    }
    
    return result;
}

std::vector<Detection> FrameProcessor::Detect(const cv::Mat& frame) {
    std::vector<Detection> detections;
    
    // Create blob from image
    cv::Mat blob = cv::dnn::blobFromImage(frame, 1/255.0, cv::Size(416, 416), cv::Scalar(), true, false);
    net_.setInput(blob);
    
    // Get output layer names
    std::vector<cv::String> outLayerNames = net_.getUnconnectedOutLayersNames();
    std::vector<cv::Mat> outs;
    net_.forward(outs, outLayerNames);
    
    // Process detections
    for (const auto& out : outs) {
        for (int i = 0; i < out.rows; ++i) {
            cv::Mat scores = out.row(i).colRange(5, out.cols);
            cv::Point classIdPoint;
            double confidence;
            cv::minMaxLoc(scores, nullptr, &confidence, nullptr, &classIdPoint);
            
            if (confidence > conf_threshold_) {
                int class_id = classIdPoint.x;
                std::string class_name = class_names_[class_id];
                
                // Only keep people, vehicles, and animals
                if (class_name == "person" || 
                    class_name == "car" || class_name == "truck" || class_name == "bus" || class_name == "motorcycle" ||
                    class_name == "dog" || class_name == "cat" || class_name == "bird") {
                    
                    Detection det;
                    det.class_name = class_name;
                    det.confidence = static_cast<float>(confidence);
                    
                    // Get bounding box
                    int centerX = static_cast<int>(out.at<float>(i, 0) * frame.cols);
                    int centerY = static_cast<int>(out.at<float>(i, 1) * frame.rows);
                    int width = static_cast<int>(out.at<float>(i, 2) * frame.cols);
                    int height = static_cast<int>(out.at<float>(i, 3) * frame.rows);
                    det.box = cv::Rect(centerX - width/2, centerY - height/2, width, height);
                    
                    detections.push_back(det);
                }
            }
        }
    }
    
    return detections;
}

void FrameProcessor::DrawDetections(cv::Mat& frame, const std::vector<Detection>& detections) {
    for (const auto& det : detections) {
        cv::rectangle(frame, det.box, cv::Scalar(0, 255, 0), 2);
        std::string label = det.class_name + " " + std::to_string(static_cast<int>(det.confidence * 100)) + "%";
        cv::putText(frame, label, cv::Point(det.box.x, det.box.y - 5),
                   cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 2);
    }
} 