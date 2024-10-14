#include <pocketsphinx.h>
#include <vector>
#include <iostream>
#include <memory> 
#include <stdexcept>
#include <algorithm>


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
        if (hyp != nullptr) {
            std::string hypothesis(hyp);
            std::transform(hypothesis.begin(), hypothesis.end(), hypothesis.begin(), ::tolower);
            std::cout << hypothesis << std::endl;

            // Check for individual words in the hypothesis
            if (hypothesis.find("hello") != std::string::npos) {
                if (verbose) std::cout << "Activate words detected" << std::endl;
                return true;
            }
        } else {
            std::cout << "Hypothesis not detected" << std::endl;
        }
        return false;
    }

    void DetectCommand(const std::vector<int16_t>& buffer, bool verbose = false) {
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
        if (hyp != nullptr) {
            std::string hypothesis(hyp);
            std::transform(hypothesis.begin(), hypothesis.end(), hypothesis.begin(), ::tolower);

            // Check for other keywords
            if (hypothesis.find("turn on") != std::string::npos &&
                hypothesis.find("light") != std::string::npos) {
                if (verbose) std::cout << "Turning on light" << std::endl;
            } else if (hypothesis.find("turn off") != std::string::npos && 
                       hypothesis.find("light") != std::string::npos) {
                if (verbose) std::cout << "Turning off light" << std::endl;
            }
        } else {
            std::cout << "Hypothesis not detected" << std::endl;
        }
    }

};