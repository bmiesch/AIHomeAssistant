#include <stdexcept>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <cmath>
#include <algorithm>
#include <limits>
#include "keyword_detector.h"
#include "log.h"

KeywordDetector::KeywordDetector(const std::string& hmm_path, const std::string& porcupine_model_path, const std::string& porcupine_keyword_path)
    : config_(ps_config_init(nullptr), ps_config_free),
      jsgf_config_(nullptr, ps_config_free),
      ps_(nullptr, ps_free),
      porcupine_(nullptr, pv_porcupine_delete) {

    // Porcupine
    const char* access_key = std::getenv("PICOVOICE_ACCESS_KEY");
    if (access_key == nullptr) {
        throw std::runtime_error("PICOVOICE_ACCESS_KEY is not set");
    }

    const char* model_path = porcupine_model_path.c_str();
    const char* keyword_path = porcupine_keyword_path.c_str();
    const float sensitivity = 0.7f;
    pv_porcupine_t* porcupine_raw = nullptr;
    pv_status_t status = pv_porcupine_init(
        access_key,
        model_path,
        1, // Number of keywords
        &keyword_path,
        &sensitivity, 
        &porcupine_raw
    );

    if (status != PV_STATUS_SUCCESS) {
        throw std::runtime_error("Failed to initialize Porcupine");
    }
    porcupine_.reset(porcupine_raw);

    // PocketSphinx
    CreateConfigWithFiles();
    InitConfig(hmm_path);
    ps_.reset(ps_init(config_.get()));
    if (!ps_) {
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
    std::string gram_path = GetTempPath("commands.gram");

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

    INFO_LOG("PocketSphinx configuration initialized successfully");
}

bool KeywordDetector::DetectKeyword(const std::vector<int16_t>& buffer, bool verbose) const {
    std::vector<int16_t> processed = buffer;
    (void)verbose;
    
    // Remove DC offset
    int32_t sum = 0;
    for (const auto& sample : buffer) {
        sum += sample;
    }
    int16_t offset = sum / buffer.size();
    
    // Apply DC offset removal and small gain
    const float gain = 1.5f;
    for (auto& sample : processed) {
        int32_t adjusted = (sample - offset) * gain;
        sample = std::max(std::min(adjusted, 32767), -32768);
    }

    int32_t keyword_index = -1;
    pv_porcupine_process(porcupine_.get(), processed.data(), &keyword_index);
    
    if (keyword_index >= 0) {
        INFO_LOG("Keyword detected!");
        return true;
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
    WriteStringToFile(GetTempPath("commands.gram"), COMMANDS_GRAM);
}

void KeywordDetector::WriteStringToFile(const std::string& path, const char* content) {
    std::ofstream file(path);
    file << content;
    file.close();
}
