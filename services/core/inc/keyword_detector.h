#pragma once

#include <pocketsphinx.h>
#include <vector>
#include <memory>
#include <string>
#include <pv_porcupine.h>

enum class Command {
    TURN_OFF,
    TURN_ON,
    NO_COMMAND
};

class KeywordDetector {
private:
    std::unique_ptr<ps_config_t, decltype(&ps_config_free)> config_;
    std::unique_ptr<ps_config_t, decltype(&ps_config_free)> jsgf_config_;
    std::unique_ptr<ps_decoder_t, decltype(&ps_free)> ps_;
    std::unique_ptr<pv_porcupine_t, decltype(&pv_porcupine_delete)> porcupine_;

    void InitConfig(const std::string& hmm_path);

    static constexpr const char* KEYWORD_DICT = 
        "hello HH AH L OW\n"
        "activate AE K T AH V EY T\n"
        "activate(2) AE K T IH V EY T\n"
        "light L AY T\n"
        "off AO F\n"
        "turn T ER N\n"
        "turn(2) T R N\n"
        "on AA N\n";

    static constexpr const char* COMMANDS_GRAM = 
        "#JSGF V1.0;\n"
        "grammar commands;\n"
        "public <command> = (turn light on | turn light off);\n";

    static void CreateConfigWithFiles();
    static void WriteStringToFile(const std::string& path, const char* content);
    static std::string GetTempPath(const std::string& filename) {
        return std::string("/tmp/sphinx_") + filename;
    }


public:
    explicit KeywordDetector(const std::string& hmm_path = "/usr/local/share/pocketsphinx/model/en-us/en-us",
                             const std::string& porcupine_model_path = "/opt/services/lib/porcupine_params.pv",
                             const std::string& porcupine_keyword_path = "/opt/services/lib/jarvis_raspberry-pi.ppn");

    bool DetectKeyword(const std::vector<int16_t>& buffer, bool verbose = false) const;
    Command DetectCommand(const std::vector<int16_t>& buffer, bool verbose = false);

    KeywordDetector(const KeywordDetector&) = delete;
    KeywordDetector& operator=(const KeywordDetector&) = delete;
};