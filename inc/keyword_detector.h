#pragma once

#include <pocketsphinx.h>
#include <vector>
#include <memory>
#include <string>


enum class Command {
    TURN_OFF,
    TURN_ON,
    NO_COMMAND
};

class KeywordDetector {
private:
    std::unique_ptr<ps_config_t, decltype(&ps_config_free)> config;
    std::unique_ptr<ps_config_t, decltype(&ps_config_free)> kws_config;
    std::unique_ptr<ps_config_t, decltype(&ps_config_free)> jsgf_config;
    std::unique_ptr<ps_decoder_t, decltype(&ps_free)> ps;

    void InitConfig(const std::string& hmm_path, const std::string& dict_path, const std::string& kws_path);

public:
    KeywordDetector(const std::string& hmm_path = "/usr/local/share/pocketsphinx/model/en-us/en-us",
                    const std::string& dict_path = "keyword.dict",
                    const std::string& kws_path = "keyword.list");

    // Delete copy constructor and assignment operator
    KeywordDetector(const KeywordDetector&) = delete;
    KeywordDetector& operator=(const KeywordDetector&) = delete;

    bool DetectKeyword(const std::vector<int16_t>& buffer, bool verbose = false) const;
    Command DetectCommand(const std::vector<int16_t>& buffer, bool verbose = false);
};