#pragma once

#include <vector>
#include <memory>
#include <string>
#include <pv_porcupine.h>
#include <pv_rhino.h>

enum class Command {
    PROCESSING,
    TURN_OFF,
    TURN_ON,
    NO_COMMAND
};

class KeywordDetector {
private:
    std::unique_ptr<pv_porcupine_t, decltype(&pv_porcupine_delete)> porcupine_;
    std::unique_ptr<pv_rhino_t, decltype(&pv_rhino_delete)> rhino_;

public:
    explicit KeywordDetector(const std::string& porcupine_model_path = "/usr/local/lib/core/porcupine_params.pv",
                             const std::string& porcupine_keyword_path = "/usr/local/lib/core/jarvis_raspberry-pi.ppn",
                             const std::string& rhino_model_path = "/usr/local/lib/core/rhino_params.pv",
                             const std::string& rhino_context_path = "/usr/local/lib/core/Smart-Home_en_raspberry-pi_v3_0_0.rhn");

    bool DetectKeyword(const std::vector<int16_t>& buffer, bool verbose = false) const;
    Command DetectCommand(const std::vector<int16_t>& buffer, bool verbose = false);

    KeywordDetector(const KeywordDetector&) = delete;
    KeywordDetector& operator=(const KeywordDetector&) = delete;
};