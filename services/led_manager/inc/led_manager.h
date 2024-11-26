#pragma once

#include <vector>
#include <memory>
#include <string>
#include <mutex>
#include <queue>
#include <atomic>
#include <simpleble/SimpleBLE.h>
#include <nlohmann/json.hpp>
#include <mqtt/async_client.h>
#include "ble_device.h"

using json = nlohmann::json;


class LEDManager : public virtual mqtt::callback {
private:
    std::atomic<bool> running_{true};
    std::vector<BLEDeviceConfig> device_configs_;
    std::unique_ptr<SimpleBLE::Adapter> adapter_;
    
    std::mutex devices_mutex_;
    std::vector<std::unique_ptr<BLEDevice>> devices_;

    std::mutex cmd_queue_mutex_;
    std::condition_variable cmd_queue_cv_;
    std::queue<json> cmd_queue_;

    using CommandHandler = std::function<void(const json&)>;
    std::unordered_map<std::string, CommandHandler> command_handlers_ = {
        {"turn_on",  [this](const json&) { TurnOnAll(); }},
        {"turn_off", [this](const json&) { TurnOffAll(); }},
        {"set_color", [this](const json& payload) { 
            int r = payload["params"]["r"];
            int g = payload["params"]["g"];
            int b = payload["params"]["b"];
            SetColor(r, g, b);
        }}
    };

    void InitializeMqttConnection();
    void InitAdapter();
    void FindAndInitDevices(std::vector<BLEDeviceConfig>& dc);
    void FindAndInitDevice(BLEDeviceConfig& config);
    void PublishStatus();
    void HandleCommand(const json& command);

    mqtt::async_client mqtt_client_;
    mqtt::ssl_options mqtt_ssl_opts_;
    mqtt::connect_options mqtt_conn_opts_;
    
    // MQTT topics
    const std::string COMMAND_TOPIC = "home/services/led_manager/command";
    const std::string STATUS_TOPIC = "home/services/led_manager/status";
    const std::string LED_STATE_TOPIC_PREFIX = "home/devices/leds/";

    // Command handlers
    void ReconnectDevices();
    void ReinitDevices();
    void TurnOnAll();
    void TurnOffAll();
    void SetColor(int r, int g, int b);

    // MQTT callback overrides
    void connected(const std::string& cause) override;
    void connection_lost(const std::string& cause) override;
    void message_arrived(mqtt::const_message_ptr msg) override;
    void delivery_complete(mqtt::delivery_token_ptr token) override;

    std::thread worker_thread_;
    void Run();

public:
    LEDManager(const std::vector<BLEDeviceConfig>& configs, const std::string& broker_address,
               const std::string& client_id);
    ~LEDManager();

    void Initialize();
    void Stop();

    LEDManager(const LEDManager&) = delete;
    LEDManager& operator=(const LEDManager&) = delete;
};