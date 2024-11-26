#pragma once

#include <alsa/asoundlib.h>
#include <vector>

enum class AudioMode {
    STEREO,
    MONO_MIX,
    MONO_LEFT,
    MONO_RIGHT
};

class AudioCapture {
private:
    snd_pcm_t *audio_capture_device_;
    unsigned int sample_rate_;
    unsigned int channels_;
    snd_pcm_format_t format_;
    snd_pcm_hw_params_t *params_;

    void InitParams();
    void PrintCurrentParameters();
    void ResetCaptureDevice();

public:
    AudioCapture(unsigned int rate = 16000, unsigned int chans = 1);
    ~AudioCapture();

    std::vector<int16_t> CaptureAudio(unsigned int duration_ms);

    AudioCapture(const AudioCapture&) = delete;
    AudioCapture& operator=(const AudioCapture&) = delete;
};