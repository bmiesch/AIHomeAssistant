#pragma once

#include <vector>
#include <memory>
#include <string>
#include <mutex>
#include <simpleble/SimpleBLE.h>
#include <nlohmann/json.hpp>
#include <mqtt/async_client.h>
#include "device.h"

using json = nlohmann::json;


struct DeviceConfig {
    std::string address;
    SimpleBLE::BluetoothUUID serv_uuid;
    SimpleBLE::BluetoothUUID char_uuid;
};

class LEDManager : public virtual mqtt::callback {
private:
    std::vector<DeviceConfig> device_configs;
    std::vector<std::unique_ptr<Device>> devices;
    std::unique_ptr<SimpleBLE::Adapter> adapter;
    std::mutex devices_mutex;

    void InitAdapter();
    void FindAndInitDevice(const DeviceConfig& dc);
    void HandleCommand(const json& command);
    void PublishStatus();

    mqtt::async_client mqtt_client;
    mqtt::connect_options mqtt_conn_opts;
    
    // MQTT topics
    const std::string COMMAND_TOPIC = "home/services/led_manager/command";
    const std::string STATUS_TOPIC = "home/services/led_manager/status";
    const std::string LED_STATE_TOPIC_PREFIX = "home/devices/leds/";

    // Command handlers
    void TurnOnAll();
    void TurnOffAll();
    void SetColor(int r, int g, int b);

    // MQTT callback overrides
    void connected(const std::string& cause) override;
    void connection_lost(const std::string& cause) override;
    void message_arrived(mqtt::const_message_ptr msg) override;
    void delivery_complete(mqtt::delivery_token_ptr token) override;

public:
    LEDManager(const std::vector<DeviceConfig>& configs, const std::string& broker_address,
               const std::string& client_id);
    ~LEDManager();

    void Initialize();
    void Run();
    void Stop();
};