#include "kernel.h"
#include <iostream>
#include <chrono>

void Kernel::AudioCaptureLoop() {
    while(running) {
        auto buffer = audio_capture->CaptureAudio(1000);
        {
            std::lock_guard<std::mutex> lock(audio_queue_mutex);
            audio_queue.push(std::move(buffer));
        }
        audio_queue_cv.notify_one();
    }
}

void Kernel::AudioProcessingLoop() {
    while(running) {
        std::vector<int16_t> buffer;
        {
            std::unique_lock<std::mutex> lock(audio_queue_mutex);
            audio_queue_cv.wait(lock, [this] { return !audio_queue.empty() || !running; });
            if (!running) break;
            buffer = std::move(audio_queue.front());
            audio_queue.pop();
        }

        if (keyword_detector->DetectKeyword(buffer, true)) {
            std::cout << "Keyword detected! Listening for command..." << std::endl;
            
            // Collect audio for 5 seconds (5 buffers of 1000ms each)
            std::vector<int16_t> command_buffer;
            for (int i = 0; i < 5; ++i) {
                std::unique_lock<std::mutex> lock(audio_queue_mutex);
                audio_queue_cv.wait(lock, [this] { return !audio_queue.empty() || !running; });
                if (!running) break;
                auto next_buffer = std::move(audio_queue.front());
                audio_queue.pop();
                command_buffer.insert(command_buffer.end(), next_buffer.begin(), next_buffer.end());
            }

            Command cmd = keyword_detector->DetectCommand(command_buffer, true);
            DeviceManager* device_manager = DeviceManagerSingleton::getInstance();
            switch(cmd) {
                case Command::TURN_ON:
                    device_manager->TurnOnDevices();
                    break;
                case Command::TURN_OFF:
                    device_manager->TurnOffDevices();
                    break;
                case Command::NO_COMMAND:
                default:
                    std::cout << "No command detected." << std::endl; 
                    break;
            }
        }
    }
}

Kernel::Kernel() {
    // Initialize modules
    audio_capture = std::make_unique<AudioCapture>();
    keyword_detector = std::make_unique<KeywordDetector>();
}

Kernel::~Kernel() {
    Stop();
}

void Kernel::Run() {
    running = true;

    // Initialize threads
    audio_thread = std::thread(&Kernel::AudioCaptureLoop, this);
    audio_processing_thread = std::thread(&Kernel::AudioProcessingLoop, this);

    // Wait for tasks
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    audio_thread.join();
    audio_processing_thread.join();
}   

void Kernel::Stop() {
    running = false;
    audio_queue_cv.notify_all();
    if (audio_thread.joinable()) audio_thread.join();
    if (audio_processing_thread.joinable()) audio_processing_thread.join();
}