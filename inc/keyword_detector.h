#include <pocketsphinx.h>
#include <vector>
#include <iostream>
#include <memory> 
#include <stdexcept>


class KeywordDetector {
private:
    std::unique_ptr<ps_config_t, decltype(&ps_config_free)> config;
    std::unique_ptr<ps_decoder_t, decltype(&ps_free)> ps;

    void InitConfig(const std::string& hmm_path, const std::string& dict_path, const std::string& kws_path) {
        if (ps_config_set_str(config.get(), "hmm", hmm_path.c_str()) == nullptr ||
            ps_config_set_str(config.get(), "dict", dict_path.c_str()) == nullptr ||
            ps_config_set_str(config.get(), "kws", kws_path.c_str()) == nullptr ||
            ps_config_set_float(config.get(), "kws_threshold", 1e-20) == nullptr ||
            ps_config_set_bool(config.get(), "verbose", true) == nullptr) {
            throw std::runtime_error("Failed to set configuration");
        }
    }

public:
    KeywordDetector(const std::string& hmm_path = "/usr/local/share/pocketsphinx/model/en-us/en-us",
                    const std::string& dict_path = "keyword.dict",
                    const std::string& kws_path = "keyword.list")
        : config(ps_config_init(nullptr), ps_config_free),
          ps(nullptr, ps_free) {
        InitConfig(hmm_path, dict_path, kws_path);
        ps.reset(ps_init(config.get()));
        if (!ps) {
            throw std::runtime_error("Failed to initialize PocketSphinx");
        }
    }

    // Delete copy constructor and assignment operator
    KeywordDetector(const KeywordDetector&) = delete;
    KeywordDetector& operator=(const KeywordDetector&) = delete;

    bool DetectKeyword(const std::vector<int16_t>& buffer, bool verbose = false) const {
        if (ps_start_utt(ps.get()) < 0) {
            throw std::runtime_error("Failed to start utterance");
        }
        
        if (ps_process_raw(ps.get(), buffer.data(), buffer.size(), false, false) < 0) {
            throw std::runtime_error("Failed to process audio");
        }
        
        if (ps_end_utt(ps.get()) < 0) {
            throw std::runtime_error("Failed to end utterance");
        }

        const char* hyp = ps_get_hyp(ps.get(), nullptr);
        if (strcmp(hyp, "hello") && strcmp(hyp, "bam")) {
            if (verbose) std::cout << "Activate Words Detected" << std::endl;
            return true;
        }
        return false;
    }
};