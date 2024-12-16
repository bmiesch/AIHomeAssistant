#include <stdexcept>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <cmath>
#include <algorithm>
#include <limits>
#include "keyword_detector.h"
#include "log.h"

KeywordDetector::KeywordDetector(const std::string& porcupine_model_path,
                                 const std::string& porcupine_keyword_path,
                                 const std::string& rhino_model_path,
                                 const std::string& rhino_context_path)
    : porcupine_(nullptr, pv_porcupine_delete),
      rhino_(nullptr, pv_rhino_delete) {

    // Porcupine
    const char* access_key = std::getenv("PICOVOICE_ACCESS_KEY");
    if (access_key == nullptr) {
        throw std::runtime_error("PICOVOICE_ACCESS_KEY is not set");
    }

    const char* p_model_path = porcupine_model_path.c_str();
    const char* p_keyword_path = porcupine_keyword_path.c_str();
    const float p_sensitivity = 0.7f;
    pv_porcupine_t* porcupine_raw = nullptr;
    pv_status_t status = pv_porcupine_init(
        access_key,
        p_model_path,
        1, // Number of keywords
        &p_keyword_path,
        &p_sensitivity, 
        &porcupine_raw
    );

    if (status != PV_STATUS_SUCCESS) {
        throw std::runtime_error("Failed to initialize Porcupine");
    }
    porcupine_.reset(porcupine_raw);

    // Rhino
    const char* r_model_path = rhino_model_path.c_str();
    const char* r_context_path = rhino_context_path.c_str();
    const float r_sensitivity = 0.7f;
    const float r_endpoint_duration_sec = 0.5f;
    const bool r_require_endpoint = true;
    pv_rhino_t* rhino_raw = nullptr;
    pv_status_t rhino_status = pv_rhino_init(
        access_key,
        r_model_path,
        r_context_path,
        r_sensitivity,
        r_endpoint_duration_sec,
        r_require_endpoint,
        &rhino_raw
    );

    if (rhino_status != PV_STATUS_SUCCESS) {
        throw std::runtime_error("Failed to initialize Rhino");
    }
    rhino_.reset(rhino_raw);
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
    bool is_finalized = false;
    pv_status_t status = pv_rhino_process(rhino_.get(), buffer.data(), &is_finalized);
    
    if (status != PV_STATUS_SUCCESS) {
        ERROR_LOG("Failed to process audio in DetectCommand");
        throw std::runtime_error("Failed to process audio");
    }

    if (is_finalized) {
        bool is_understood = false;
        status = pv_rhino_is_understood(rhino_.get(), &is_understood);
        
        if (status != PV_STATUS_SUCCESS) {
            ERROR_LOG("Failed to check if command was understood");
            throw std::runtime_error("Failed to check command understanding");
        }

        if (is_understood) {
            const char* intent = nullptr;
            const char** slots = nullptr;
            const char** values = nullptr;
            int32_t num_slots = 0;
            
            status = pv_rhino_get_intent(rhino_.get(), &intent, &num_slots, 
                                       (const char***)&slots, 
                                       (const char***)&values);
            
            if (status != PV_STATUS_SUCCESS) {
                ERROR_LOG("Failed to get intent from Rhino");
                throw std::runtime_error("Failed to get intent");
            }

            std::string intent_str(intent);
            DEBUG_LOG("Detected intent: " + intent_str);

            Command result = Command::NO_COMMAND;
            if (intent_str == "changeState" && num_slots > 0 && std::string(values[0]) == "on") {
                if (verbose) INFO_LOG("Command detected: TURN_ON");
                result = Command::TURN_ON;
            } else if (intent_str == "changeState" && num_slots > 0 && std::string(values[0]) == "off") {
                if (verbose) INFO_LOG("Command detected: TURN_OFF");
                result = Command::TURN_OFF;
            }

            // Free the intent, slots, and values
            pv_rhino_free_slots_and_values(rhino_.get(), 
                                         const_cast<const char**>(slots), 
                                         const_cast<const char**>(values));

            pv_rhino_reset(rhino_.get());
            return result;
        } else {
            DEBUG_LOG("Command not understood");
            pv_rhino_reset(rhino_.get());
            return Command::NO_COMMAND;
        }
    }
    
    return Command::PROCESSING;
}