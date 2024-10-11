#include <alsa/asoundlib.h>
#include <vector>
#include <iostream>
#include <stdexcept>

class AudioCapture {
private:
    snd_pcm_t *capture_handle;
    unsigned int sample_rate;
    unsigned int channels;
    snd_pcm_format_t format;

    void Init() {
        int rc;
        
        // Open PCM device
        rc = snd_pcm_open(&capture_handle, "default", SND_PCM_STREAM_CAPTURE, 0);
        if (rc < 0) {
            throw std::runtime_error("Cannot open audio device: " + std::string(snd_strerror(rc)));
        }

        snd_pcm_hw_params_t *params;
        snd_pcm_hw_params_alloca(&params);
        
        // Fill params with default values
        snd_pcm_hw_params_any(capture_handle, params);

        // Set parameters
        rc = snd_pcm_hw_params_set_access(capture_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
        if (rc < 0) std::cerr << "Cannot set access type: " << snd_strerror(rc) << std::endl;

        rc = snd_pcm_hw_params_set_format(capture_handle, params, format);
        if (rc < 0) std::cerr << "Cannot set sample format: " << snd_strerror(rc) << std::endl;

        rc = snd_pcm_hw_params_set_channels(capture_handle, params, channels);
        if (rc < 0) std::cerr << "Cannot set channel count: " << snd_strerror(rc) << std::endl;

        rc = snd_pcm_hw_params_set_rate_near(capture_handle, params, &sample_rate, 0);
        if (rc < 0) std::cerr << "Cannot set sample rate: " << snd_strerror(rc) << std::endl;

        // Apply parameters
        rc = snd_pcm_hw_params(capture_handle, params);
        if (rc < 0) {
            throw std::runtime_error("Cannot set hardware parameters: " + std::string(snd_strerror(rc)));
        }

        // Print current parameters
        snd_pcm_uframes_t buffer_size;
        snd_pcm_hw_params_get_buffer_size(params, &buffer_size);
        std::cout << "Buffer size: " << buffer_size << " frames" << std::endl;

        unsigned int actual_rate;
        snd_pcm_hw_params_get_rate(params, &actual_rate, 0);
        std::cout << "Actual sample rate: " << actual_rate << " Hz" << std::endl;

        snd_pcm_format_t actual_format;
        snd_pcm_hw_params_get_format(params, &actual_format);
        std::cout << "Actual format: " << snd_pcm_format_name(actual_format) << std::endl;

        unsigned int actual_channels;
        snd_pcm_hw_params_get_channels(params, &actual_channels);
        std::cout << "Actual channels: " << actual_channels << std::endl;

        snd_pcm_prepare(capture_handle);
    }

public:
    // Was: 48k, 1 channel, 32-bit
    AudioCapture(unsigned int rate = 16000, unsigned int chans = 1)
        : capture_handle(nullptr), sample_rate(rate), channels(chans), format(SND_PCM_FORMAT_S16_LE) {
        Init();
    }

    ~AudioCapture() {
        if (capture_handle) {
            snd_pcm_close(capture_handle);
        }
    }

    std::vector<int32_t> CaptureAudio(unsigned int duration_ms) {
        snd_pcm_uframes_t frames_to_read = (sample_rate * duration_ms) / 1000;
        std::vector<int32_t> buffer(frames_to_read * channels);

        std::cout << "Attempting to read " << frames_to_read << " frames" << std::endl;

        snd_pcm_sframes_t frames_read = 0;
        while (frames_read < snd_pcm_sframes_t(frames_to_read)) {
            snd_pcm_sframes_t rc = snd_pcm_readi(capture_handle, 
                                                buffer.data() + frames_read * channels, 
                                                frames_to_read - frames_read);
            if (rc == -EPIPE) {
                std::cerr << "Overrun occurred" << std::endl;
                snd_pcm_prepare(capture_handle);
            } else if (rc < 0) {
                std::cerr << "Error code: " << rc << std::endl;
                std::cerr << "Error description: " << snd_strerror(rc) << std::endl;
                throw std::runtime_error("Error from read: " + std::string(snd_strerror(rc)));
            } else {
                frames_read += rc;
                std::cout << "Read " << rc << " frames, total: " << frames_read << std::endl;
            }
        }

        std::cout << "Finished reading. Total frames: " << frames_read << std::endl;

        return buffer;
    }
};