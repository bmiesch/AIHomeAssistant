#pragma once

#include <alsa/asoundlib.h>
#include <vector>

class AudioCapture {
private:
    snd_pcm_t *audio_capture_device;
    unsigned int sample_rate;
    unsigned int channels;
    snd_pcm_format_t format;
    snd_pcm_hw_params_t *params;

    void InitParams();
    void PrintCurrentParameters();
    void ResetCaptureDevice();

public:
    AudioCapture(unsigned int rate = 16000, unsigned int chans = 1);
    ~AudioCapture();

    std::vector<int16_t> CaptureAudio(unsigned int duration_ms);
};