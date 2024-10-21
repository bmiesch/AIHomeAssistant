#pragma once

#include "audio_capture.h"
#include "device_manager.h"
#include "keyword_detector.h"
#include <thread>
#include <atomic>
#include <queue>
#include <condition_variable>
#include <memory>

class Kernel {
private:
    std::unique_ptr<AudioCapture> audio_capture;
    std::unique_ptr<DeviceManager> device_manager;
    std::unique_ptr<KeywordDetector> keyword_detector;
    
    std::atomic<bool> running{true};
    std::thread audio_thread;
    std::thread audio_processing_thread;

    std::queue<std::vector<int16_t>> audio_queue;
    std::mutex audio_queue_mutex;
    std::condition_variable audio_queue_cv;

    std::atomic<Command> command;

    void AudioCaptureLoop();
    void AudioProcessingLoop();

public:
    Kernel();
    ~Kernel();

    void Run();
    void Stop();
};