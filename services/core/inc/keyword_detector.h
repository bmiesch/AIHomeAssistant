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

    static constexpr const char* KEYWORD_LIST = 
        "hello /0.5/\n"
        "activate /0.5/\n"
        "light /0.5/\n"
        "off /0.7/\n"
        "activate light /0.7/\n"
        "turn off light /0.7/\n";

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
    explicit KeywordDetector(const std::string& hmm_path = "/usr/local/share/pocketsphinx/model/en-us/en-us");

    // Delete copy constructor and assignment operator
    KeywordDetector(const KeywordDetector&) = delete;
    KeywordDetector& operator=(const KeywordDetector&) = delete;

    bool DetectKeyword(const std::vector<int16_t>& buffer, bool verbose = false) const;
    Command DetectCommand(const std::vector<int16_t>& buffer, bool verbose = false);
};