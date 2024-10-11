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
    snd_pcm_hw_params_t *params;

    void Init() {
        int rc;
        
        // Open PCM device
        rc = snd_pcm_open(&capture_handle, "default", SND_PCM_STREAM_CAPTURE, 0);
        if (rc < 0) {
            throw std::runtime_error("Cannot open audio device: " + std::string(snd_strerror(rc)));
        }

        // Init parameters
        snd_pcm_hw_params_alloca(&params);
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

        PrintCurrentParameters();
        snd_pcm_prepare(capture_handle);
    }

    void PrintCurrentParameters() {
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

    std::vector<int16_t> CaptureAudio(unsigned int duration_ms) {
        snd_pcm_uframes_t frames_to_read = (sample_rate * duration_ms) / 1000;
        std::vector<int16_t> buffer(frames_to_read * channels);

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