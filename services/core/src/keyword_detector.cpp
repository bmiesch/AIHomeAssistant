#include "keyword_detector.h"
#include "log.h"
#include <stdexcept>
#include <algorithm>
#include <fstream>
#include <filesystem> 

KeywordDetector::KeywordDetector(const std::string& hmm_path)
    : config_(ps_config_init(nullptr), ps_config_free),
      kws_config_(nullptr, ps_config_free),
      jsgf_config_(nullptr, ps_config_free),
      ps_(nullptr, ps_free) {
    
    CreateConfigWithFiles();
    InitConfig(hmm_path);
    ps_.reset(ps_init(config_.get()));
    if (!ps_) {
        ERROR_LOG("Failed to initialize PocketSphinx");
        throw std::runtime_error("Failed to initialize PocketSphinx");
    }
    INFO_LOG("KeywordDetector initialized successfully");
}

void KeywordDetector::InitConfig(const std::string& hmm_path) {
    DEBUG_LOG("Initializing PocketSphinx configuration");
    if (ps_config_set_str(config_.get(), "hmm", hmm_path.c_str()) == nullptr ||
        ps_config_set_bool(config_.get(), "verbose", true) == nullptr) {
        ERROR_LOG("Failed to set PocketSphinx configuration");
        throw std::runtime_error("Failed to set configuration");
    }

    std::string dict_path = GetTempPath("keyword.dict");
    std::string kws_path = GetTempPath("keyword.list");
    std::string gram_path = GetTempPath("commands.gram");

    // Keyword Configuration
    kws_config_.reset(ps_config_init(nullptr));
    std::string kws_json = R"({
        "hmm": ")" + hmm_path + R"(",
        "dict": ")" + dict_path + R"(",
        "kws": ")" + kws_path + R"(",
        "kws_threshold": 1e-20
    })";
    if (ps_config_parse_json(kws_config_.get(), kws_json.c_str()) == nullptr) {
        ERROR_LOG("Failed to parse KWS configuration");
        throw std::runtime_error("Failed to parse KWS configuration");
    }

    // JSGF Grammar Configuration
    jsgf_config_.reset(ps_config_init(nullptr));
    std::string jsgf_json = R"({
        "hmm": ")" + hmm_path + R"(",
        "dict": ")" + dict_path + R"(",
        "jsgf": ")" + gram_path + R"("
    })";
    if (ps_config_parse_json(jsgf_config_.get(), jsgf_json.c_str()) == nullptr) {
        ERROR_LOG("Failed to parse JSGF configuration");
        throw std::runtime_error("Failed to parse JSGF configuration");
    }

    if (!kws_config_ || !jsgf_config_) {
        ERROR_LOG("Failed to create mode-specific configurations");
        throw std::runtime_error("Failed to create configurations");
    }

    INFO_LOG("PocketSphinx configuration initialized successfully");
}

bool KeywordDetector::DetectKeyword(const std::vector<int16_t>& buffer, bool verbose) const {
    if (ps_reinit(ps_.get(), kws_config_.get()) < 0) {
        ERROR_LOG("Failed to switch to keyword spotting mode");
        throw std::runtime_error("Failed to switch configuration");
    }

    if (ps_start_utt(ps_.get()) < 0) {
        ERROR_LOG("Failed to start utterance in DetectKeyword");
        throw std::runtime_error("Failed to start utterance");
    }
    
    if (ps_process_raw(ps_.get(), buffer.data(), buffer.size(), false, false) < 0) {
        ERROR_LOG("Failed to process audio in DetectKeyword");
        throw std::runtime_error("Failed to process audio");
    }
    
    if (ps_end_utt(ps_.get()) < 0) {
        ERROR_LOG("Failed to end utterance in DetectKeyword");
        throw std::runtime_error("Failed to end utterance");
    }

    const char* hyp = ps_get_hyp(ps_.get(), nullptr);
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
    if (ps_reinit(ps_.get(), jsgf_config_.get()) < 0) {
        ERROR_LOG("Failed to switch to JSGF grammar mode");
        throw std::runtime_error("Failed to switch configuration");
    }

    if (ps_start_utt(ps_.get()) < 0) {
        ERROR_LOG("Failed to start utterance in DetectCommand");
        throw std::runtime_error("Failed to start utterance");
    }
    
    if (ps_process_raw(ps_.get(), buffer.data(), buffer.size(), false, false) < 0) {
        ERROR_LOG("Failed to process audio in DetectCommand");
        throw std::runtime_error("Failed to process audio");
    }
    
    if (ps_end_utt(ps_.get()) < 0) {
        ERROR_LOG("Failed to end utterance in DetectCommand");
        throw std::runtime_error("Failed to end utterance");
    }

    const char* hyp = ps_get_hyp(ps_.get(), nullptr);
    if (hyp != nullptr) {
        std::string hypothesis(hyp);
        DEBUG_LOG("Detected command hypothesis: " + hypothesis);

        if (hypothesis == "turn light on") {
            if (verbose) INFO_LOG("Command detected: TURN_ON");
            return Command::TURN_ON;
        } else if (hypothesis == "turn light off") {
            if (verbose) INFO_LOG("Command detected: TURN_OFF");
            return Command::TURN_OFF;
        }
    } else {
        DEBUG_LOG("No command hypothesis detected");
    }
    return Command::NO_COMMAND;
}

void KeywordDetector::CreateConfigWithFiles() {
    WriteStringToFile(GetTempPath("keyword.dict"), KEYWORD_DICT);
    WriteStringToFile(GetTempPath("keyword.list"), KEYWORD_LIST);
    WriteStringToFile(GetTempPath("commands.gram"), COMMANDS_GRAM);
}

void KeywordDetector::WriteStringToFile(const std::string& path, const char* content) {
    std::ofstream file(path);
    file << content;
    file.close();
}
