#pragma once

#include <alsa/asoundlib.h>
#include <vector>
#include <memory>

enum class AudioMode {
    STEREO,
    MONO_MIX,
    MONO_LEFT,
    MONO_RIGHT
};

class AudioCapture {
private:
    std::unique_ptr<snd_pcm_t, decltype(&snd_pcm_close)> audio_capture_device_;
    snd_pcm_hw_params_t *params_;
    unsigned int sample_rate_;
    unsigned int channels_;
    snd_pcm_format_t format_;

    void InitParams();
    void PrintCurrentParameters();
    void ResetCaptureDevice();

public:
    AudioCapture(unsigned int rate = 16000, unsigned int chans = 1);
    ~AudioCapture() = default;

    std::vector<int16_t> CaptureAudio(unsigned int duration_ms);
    std::vector<int16_t> CapturePorcupineFrame();

    AudioCapture(const AudioCapture&) = delete;
    AudioCapture& operator=(const AudioCapture&) = delete;
};