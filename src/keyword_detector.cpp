#include "keyword_detector.h"
#include "log.h"
#include <stdexcept>
#include <algorithm>

void KeywordDetector::InitConfig(const std::string& hmm_path, const std::string& dict_path, const std::string& kws_path) {
    DEBUG_LOG("Initializing PocketSphinx configuration");
    if (ps_config_set_str(config.get(), "hmm", hmm_path.c_str()) == nullptr ||
        ps_config_set_str(config.get(), "dict", dict_path.c_str()) == nullptr ||
        ps_config_set_str(config.get(), "kws", kws_path.c_str()) == nullptr ||
        ps_config_set_float(config.get(), "kws_threshold", 1e-20) == nullptr ||
        ps_config_set_bool(config.get(), "verbose", true) == nullptr) {
        ERROR_LOG("Failed to set PocketSphinx configuration");
        throw std::runtime_error("Failed to set configuration");
    }
    INFO_LOG("PocketSphinx configuration initialized successfully");
}

KeywordDetector::KeywordDetector(const std::string& hmm_path, const std::string& dict_path, const std::string& kws_path)
    : config(ps_config_init(nullptr), ps_config_free),
      ps(nullptr, ps_free) {
    InitConfig(hmm_path, dict_path, kws_path);
    ps.reset(ps_init(config.get()));
    if (!ps) {
        ERROR_LOG("Failed to initialize PocketSphinx");
        throw std::runtime_error("Failed to initialize PocketSphinx");
    }
    INFO_LOG("KeywordDetector initialized successfully");
}

bool KeywordDetector::DetectKeyword(const std::vector<int16_t>& buffer, bool verbose) const {
    if (ps_start_utt(ps.get()) < 0) {
        ERROR_LOG("Failed to start utterance in DetectKeyword");
        throw std::runtime_error("Failed to start utterance");
    }
    
    if (ps_process_raw(ps.get(), buffer.data(), buffer.size(), false, false) < 0) {
        ERROR_LOG("Failed to process audio in DetectKeyword");
        throw std::runtime_error("Failed to process audio");
    }
    
    if (ps_end_utt(ps.get()) < 0) {
        ERROR_LOG("Failed to end utterance in DetectKeyword");
        throw std::runtime_error("Failed to end utterance");
    }

    const char* hyp = ps_get_hyp(ps.get(), nullptr);
    if (hyp != nullptr) {
        std::string hypothesis(hyp);
        std::transform(hypothesis.begin(), hypothesis.end(), hypothesis.begin(), ::tolower);
        DEBUG_LOG("Detected hypothesis: " + hypothesis);

        if (hypothesis.find("hello") != std::string::npos) {
            if (verbose) INFO_LOG("Activation word 'hello' detected");
            return true;
        }
    } else {
        DEBUG_LOG("No hypothesis detected");
    }
    return false;
}

Command KeywordDetector::DetectCommand(const std::vector<int16_t>& buffer, bool verbose) {
    if (ps_start_utt(ps.get()) < 0) {
        ERROR_LOG("Failed to start utterance in DetectCommand");
        throw std::runtime_error("Failed to start utterance");
    }
    
    if (ps_process_raw(ps.get(), buffer.data(), buffer.size(), false, false) < 0) {
        ERROR_LOG("Failed to process audio in DetectCommand");
        throw std::runtime_error("Failed to process audio");
    }
    
    if (ps_end_utt(ps.get()) < 0) {
        ERROR_LOG("Failed to end utterance in DetectCommand");
        throw std::runtime_error("Failed to end utterance");
    }

    const char* hyp = ps_get_hyp(ps.get(), nullptr);
    if (hyp != nullptr) {
        std::string hypothesis(hyp);
        std::transform(hypothesis.begin(), hypothesis.end(), hypothesis.begin(), ::tolower);
        DEBUG_LOG("Detected command hypothesis: " + hypothesis);

        // Check for commands
        if (hypothesis.find("activate") != std::string::npos &&
            hypothesis.find("light") != std::string::npos) {
            if (verbose) INFO_LOG("Command detected: TURN_ON");
            return Command::TURN_ON;
        } else if (hypothesis.find("turn") != std::string::npos && 
                   hypothesis.find("off") != std::string::npos &&
                   hypothesis.find("light") != std::string::npos) {
            if (verbose) INFO_LOG("Command detected: TURN_OFF");
            return Command::TURN_OFF;
        }
    } else {
        DEBUG_LOG("No command hypothesis detected");
    }
    return Command::NO_COMMAND;
}