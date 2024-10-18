#include "audio_capture.h"
#include <iostream>
#include <stdexcept>

void AudioCapture::InitParams() {
    int rc;
    
    // Open PCM device
    rc = snd_pcm_open(&audio_capture_device, "default", SND_PCM_STREAM_CAPTURE, 0);
    if (rc < 0) {
        throw std::runtime_error("Cannot open audio device: " + std::string(snd_strerror(rc)));
    }

    // Init parameters
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(audio_capture_device, params);

    // Set parameters
    rc = snd_pcm_hw_params_set_access(audio_capture_device, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (rc < 0) std::cerr << "Cannot set access type: " << snd_strerror(rc) << std::endl;
    rc = snd_pcm_hw_params_set_format(audio_capture_device, params, format);
    if (rc < 0) std::cerr << "Cannot set sample format: " << snd_strerror(rc) << std::endl;
    rc = snd_pcm_hw_params_set_channels(audio_capture_device, params, channels);
    if (rc < 0) std::cerr << "Cannot set channel count: " << snd_strerror(rc) << std::endl;
    rc = snd_pcm_hw_params_set_rate_near(audio_capture_device, params, &sample_rate, 0);
    if (rc < 0) std::cerr << "Cannot set sample rate: " << snd_strerror(rc) << std::endl;

    // Apply parameters
    rc = snd_pcm_hw_params(audio_capture_device, params);
    if (rc < 0) {
        throw std::runtime_error("Cannot set hardware parameters: " + std::string(snd_strerror(rc)));
    }

    PrintCurrentParameters();
    snd_pcm_prepare(audio_capture_device);
}

void AudioCapture::PrintCurrentParameters() {
    snd_pcm_uframes_t buffer_size;
    unsigned int rate;
    snd_pcm_format_t format;
    unsigned int channels;

    snd_pcm_hw_params_get_buffer_size(params, &buffer_size);
    snd_pcm_hw_params_get_rate(params, &rate, nullptr);
    snd_pcm_hw_params_get_format(params, &format);
    snd_pcm_hw_params_get_channels(params, &channels);

    std::cout << "Buffer size: " << buffer_size << " frames\n"
              << "Sample rate: " << rate << " Hz\n"
              << "Format: " << snd_pcm_format_name(format) << "\n"
              << "Channels: " << channels << "\n";
}

void AudioCapture::ResetCaptureDevice() {
    snd_pcm_drop(audio_capture_device);
    snd_pcm_prepare(audio_capture_device);
    snd_pcm_reset(audio_capture_device);
}

AudioCapture::AudioCapture(unsigned int rate, unsigned int chans)
    : audio_capture_device(nullptr), sample_rate(rate), channels(chans), format(SND_PCM_FORMAT_S16_LE) {
    InitParams();
}

AudioCapture::~AudioCapture() {
    if (audio_capture_device) {
        snd_pcm_close(audio_capture_device);
    }
}

std::vector<int16_t> AudioCapture::CaptureAudio(unsigned int duration_ms) {
    ResetCaptureDevice();

    snd_pcm_uframes_t frames_to_read = (sample_rate * duration_ms) / 1000;
    std::vector<int16_t> buffer(frames_to_read * channels);

    snd_pcm_sframes_t frames_read = 0;
    while (frames_read < snd_pcm_sframes_t(frames_to_read)) {
        snd_pcm_sframes_t rc = snd_pcm_readi(audio_capture_device, 
                                            buffer.data() + frames_read * channels, 
                                            frames_to_read - frames_read);
        if (rc == -EPIPE) {
            std::cerr << "Overrun occurred" << std::endl;
            snd_pcm_prepare(audio_capture_device);
        } else if (rc < 0) {
            std::cerr << "Error code: " << rc << std::endl;
            std::cerr << "Error description: " << snd_strerror(rc) << std::endl;
            throw std::runtime_error("Error from read: " + std::string(snd_strerror(rc)));
        } else {
            frames_read += rc;
        }
    }
    return buffer;
}