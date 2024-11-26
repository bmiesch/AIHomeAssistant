#include "audio_capture.h"
#include <stdexcept>
#include "log.h"


AudioCapture::AudioCapture(unsigned int rate, unsigned int chans)
    : audio_capture_device_(nullptr), sample_rate_(rate), channels_(chans), format_(SND_PCM_FORMAT_S16_LE) {
    INFO_LOG("Initializing audio capture with rate: " + std::to_string(rate) + " Hz, channels: " + std::to_string(chans));
    InitParams();
}

AudioCapture::~AudioCapture() {
    if (audio_capture_device_) {
        snd_pcm_close(audio_capture_device_);
        DEBUG_LOG("Audio capture device closed");
    }
}

void AudioCapture::InitParams() {
    int rc = snd_pcm_open(&audio_capture_device_, "default", SND_PCM_STREAM_CAPTURE, 0);
    if (rc < 0) {
        ERROR_LOG("Cannot open audio device: " + std::string(snd_strerror(rc)));
        throw std::runtime_error("Cannot open audio device: " + std::string(snd_strerror(rc)));
    }

    // Init parameters
    snd_pcm_hw_params_alloca(&params_);
    snd_pcm_hw_params_any(audio_capture_device_, params_);

    // Set parameters
    rc = snd_pcm_hw_params_set_access(audio_capture_device_, params_, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (rc < 0) ERROR_LOG("Cannot set access type: " + std::string(snd_strerror(rc)));
    
    rc = snd_pcm_hw_params_set_format(audio_capture_device_, params_, format_);
    if (rc < 0) ERROR_LOG("Cannot set sample format: " + std::string(snd_strerror(rc)));
    
    rc = snd_pcm_hw_params_set_channels(audio_capture_device_, params_, channels_);
    if (rc < 0) ERROR_LOG("Cannot set channel count: " + std::string(snd_strerror(rc)));
    
    rc = snd_pcm_hw_params_set_rate_near(audio_capture_device_, params_, &sample_rate_, 0);
    if (rc < 0) ERROR_LOG("Cannot set sample rate: " + std::string(snd_strerror(rc)));

    // Apply parameters
    rc = snd_pcm_hw_params(audio_capture_device_, params_);
    if (rc < 0) {
        ERROR_LOG("Cannot set hardware parameters: " + std::string(snd_strerror(rc)));
        throw std::runtime_error("Cannot set hardware parameters: " + std::string(snd_strerror(rc)));
    }

    PrintCurrentParameters();
    snd_pcm_prepare(audio_capture_device_);
}

void AudioCapture::PrintCurrentParameters() {
    snd_pcm_uframes_t buffer_size;
    unsigned int rate;
    snd_pcm_format_t format;
    unsigned int channels;

    snd_pcm_hw_params_get_buffer_size(params_, &buffer_size);
    snd_pcm_hw_params_get_rate(params_, &rate, nullptr);
    snd_pcm_hw_params_get_format(params_, &format);
    snd_pcm_hw_params_get_channels(params_, &channels);

    INFO_LOG("Buffer size: " + std::to_string(buffer_size) + " frames");
    INFO_LOG("Sample rate: " + std::to_string(rate) + " Hz");
    INFO_LOG("Format: " + std::string(snd_pcm_format_name(format)));
    INFO_LOG("Channels: " + std::to_string(channels));
}

void AudioCapture::ResetCaptureDevice() {
    snd_pcm_drop(audio_capture_device_);
    snd_pcm_prepare(audio_capture_device_);
    snd_pcm_reset(audio_capture_device_);
}

std::vector<int16_t> AudioCapture::CaptureAudio(unsigned int duration_ms) {
    ResetCaptureDevice();

    snd_pcm_uframes_t frames_to_read = (sample_rate_ * duration_ms) / 1000;
    std::vector<int16_t> buffer(frames_to_read * channels_);

    snd_pcm_sframes_t frames_read = 0;
    while (frames_read < snd_pcm_sframes_t(frames_to_read)) {
        snd_pcm_sframes_t rc = snd_pcm_readi(audio_capture_device_, 
                                            buffer.data() + frames_read * channels_, 
                                            frames_to_read - frames_read);
        if (rc == -EPIPE) {
            WARN_LOG("Overrun occurred");
            snd_pcm_prepare(audio_capture_device_);
        } else if (rc < 0) {
            ERROR_LOG("Error code: " + std::to_string(rc));
            ERROR_LOG("Error description: " + std::string(snd_strerror(rc)));
            throw std::runtime_error("Error from read: " + std::string(snd_strerror(rc)));
        } else {
            frames_read += rc;
        }
    }
    return buffer;
}