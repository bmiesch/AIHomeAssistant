#include "led_manager.h"
#include "log.h"
#include <atomic>
#include <csignal>
#include <iostream>
#include <thread>
#include <chrono>

std::atomic<bool> should_run(true);

void signalHandler(int signum) {
    INFO_LOG("Interrupt signal (" + std::to_string(signum) + ") received.");
    should_run = false;
}

int main() {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::string broker_address = std::getenv("MQTT_BROKER");
    if (broker_address.empty()) {
        ERROR_LOG("MQTT_BROKER environment variable not set.");
        return 1;
    }

    // LED configurations
    std::vector<BLEDeviceConfig> device_configs = {
        BLEDeviceConfig{
            "BE:67:00:AC:C8:82",
            SimpleBLE::BluetoothUUID("0000fff0-0000-1000-8000-00805f9b34fb"),
            SimpleBLE::BluetoothUUID("0000fff3-0000-1000-8000-00805f9b34fb")
      },
      BLEDeviceConfig{
            "BE:67:00:6A:B5:A6",
            SimpleBLE::BluetoothUUID("0000fff0-0000-1000-8000-00805f9b34fb"),
            SimpleBLE::BluetoothUUID("0000fff3-0000-1000-8000-00805f9b34fb")
      }
    };

    auto getEnvVar = [](const char* name) -> std::string {
        const char* value = std::getenv(name);
        if (!value) {
            throw std::runtime_error(std::string("Environment variable not set: ") + name);
        }
        return std::string(value);
    };
    const auto username = getEnvVar("MQTT_USERNAME");
    const auto password = getEnvVar("MQTT_PASSWORD");
    const auto ca_path = getEnvVar("MQTT_CA_DIR") + "/ca.crt";

    try {
        LEDManager led_manager(device_configs, broker_address, "led_manager_client",
            ca_path, username, password);
        led_manager.Initialize();

        while(should_run) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        INFO_LOG("Initiating shutdown sequence...");
        led_manager.Stop();
        INFO_LOG("Shutdown complete.");
    } catch (const std::exception& e) {
        ERROR_LOG("Error: " + std::string(e.what()));
        return 1;
    }

    return 0;
}