#include "keyword_detector.h"
#include <iostream>
#include <stdexcept>
#include <algorithm>

void KeywordDetector::InitConfig(const std::string& hmm_path, const std::string& dict_path, const std::string& kws_path) {
    if (ps_config_set_str(config.get(), "hmm", hmm_path.c_str()) == nullptr ||
        ps_config_set_str(config.get(), "dict", dict_path.c_str()) == nullptr ||
        ps_config_set_str(config.get(), "kws", kws_path.c_str()) == nullptr ||
        ps_config_set_float(config.get(), "kws_threshold", 1e-20) == nullptr ||
        ps_config_set_bool(config.get(), "verbose", true) == nullptr) {
        throw std::runtime_error("Failed to set configuration");
    }
}

KeywordDetector::KeywordDetector(const std::string& hmm_path, const std::string& dict_path, const std::string& kws_path)
    : config(ps_config_init(nullptr), ps_config_free),
      ps(nullptr, ps_free) {
    InitConfig(hmm_path, dict_path, kws_path);
    ps.reset(ps_init(config.get()));
    if (!ps) {
        throw std::runtime_error("Failed to initialize PocketSphinx");
    }
}

bool KeywordDetector::DetectKeyword(const std::vector<int16_t>& buffer, bool verbose) const {
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

        if (hypothesis.find("hello") != std::string::npos) {
            if (verbose) std::cout << "Activate words detected" << std::endl;
            return true;
        }
    } else {
        std::cout << "Hypothesis not detected" << std::endl;
    }
    return false;
}

Command KeywordDetector::DetectCommand(const std::vector<int16_t>& buffer, bool verbose) {
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

        // Check for other keywords
        if (hypothesis.find("activate") != std::string::npos &&
            hypothesis.find("light") != std::string::npos) {
            if (verbose) std::cout << "Turning on light" << std::endl;
            return Command::TURN_ON;
        } else if (hypothesis.find("turn") != std::string::npos && 
                   hypothesis.find("off") != std::string::npos &&
                   hypothesis.find("light") != std::string::npos) {
            if (verbose) std::cout << "Turning off light" << std::endl;
            return Command::TURN_OFF;
        }
    } else {
        std::cout << "Hypothesis not detected" << std::endl;
    }
    return Command::NO_COMMAND;
}