#pragma once

#include <vector>
#include <memory>
#include <string>
#include <mutex>
#include <queue>
#include <atomic>
#include <unordered_map>
#include <functional>
#include <simpleble/SimpleBLE.h>
#include <nlohmann/json.hpp>

#include "ble_device.h"
#include "paho_mqtt_client.h"
#include "service_interface.h"

using json = nlohmann::json;

class LEDManager : public IService, public PahoMqttClient {
public:
    // Constructor & Destructor
    LEDManager(const std::vector<BLEDeviceConfig>& configs, 
               const std::string& broker_address, 
               const std::string& client_id,
               const std::string& ca_path, 
               const std::string& username, 
               const std::string& password);
    ~LEDManager() override;

    // Delete copy constructor and assignment operator
    LEDManager(const LEDManager&) = delete;
    LEDManager& operator=(const LEDManager&) = delete;

    // IService interface implementation
    void Initialize() override;
    void Stop() override;

private:
    // Constants
    static constexpr const char* COMMAND_TOPIC = "home/services/led_manager/command";
    static constexpr const char* STATUS_TOPIC = "home/services/led_manager/status";
    static constexpr const char* LED_STATE_TOPIC_PREFIX = "home/devices/leds/";

    // Types
    using CommandHandler = std::function<void(const json&)>;

    // State
    std::atomic<bool> running_{true};
    std::vector<BLEDeviceConfig> device_configs_;
    std::unique_ptr<SimpleBLE::Adapter> adapter_;
    
    // Thread management
    std::thread worker_thread_;
    void Run() override;

    // Device management
    std::mutex devices_mutex_;
    std::vector<std::unique_ptr<BLEDevice>> devices_;
    void InitAdapter();
    void FindAndInitDevices(std::vector<BLEDeviceConfig>& dc);
    void FindAndInitDevice(BLEDeviceConfig& config);
    void ReconnectDevices();
    void ReinitDevices();

    // Command handling
    std::mutex cmd_queue_mutex_;
    std::condition_variable cmd_queue_cv_;
    std::queue<nlohmann::json> cmd_queue_;
    void HandleCommand(const nlohmann::json& command);
    void IncomingMessage(const std::string& topic, const std::string& payload);

    // LED control operations
    void TurnOnAll();
    void TurnOffAll();
    void SetColor(int r, int g, int b);

    // Command handlers map
    const std::unordered_map<std::string, CommandHandler> command_handlers_ = {
        {"turn_on",   [this](const nlohmann::json&) { TurnOnAll(); }},
        {"turn_off",  [this](const nlohmann::json&) { TurnOffAll(); }},
        {"set_color", [this](const nlohmann::json& payload) { 
            int r = payload["params"]["r"];
            int g = payload["params"]["g"];
            int b = payload["params"]["b"];
            SetColor(r, g, b);
        }}
    };
};