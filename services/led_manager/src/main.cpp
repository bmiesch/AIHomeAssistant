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

    // Default address for MQTT Broker
    std::string broker_address = "tcp://localhost:1883";

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

    try {
        LEDManager led_manager(device_configs, broker_address, "led_manager_client");
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