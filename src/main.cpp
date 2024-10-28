#include "core.h"
#include "led_manager.h"
#include <atomic>
#include <csignal>
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>


std::atomic<bool> should_run(true);
Core* g_core = nullptr;
LEDManager* g_led_manager = nullptr;

void signalHandler(int signum) {
    std::cout << "Interrupt signal (" << signum << ") received." << std::endl;
    should_run = false;
    if (g_core) {
        g_core->Stop();
    }
    if (g_led_manager) {
        g_led_manager->Stop();
    }
}

int main() {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

   // Default address for Mosquitto MQTT Broker
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
      Core core(broker_address, "core_client");
      LEDManager led_manager(device_configs, broker_address, "led_manager_client");

      g_core = &core;
      g_led_manager = &led_manager;

      core.Initialize();
      led_manager.Initialize();

      std::thread core_thread([&core]() {
         core.Run();
      });

      std::thread led_manager_thread([&led_manager]() {
         led_manager.Run();
      });

      // Wait for shutdown signal
      while(should_run) {
         std::this_thread::sleep_for(std::chrono::seconds(1));
      }

      std::cout << "Initiating shutdown sequence...\n";
      
      // Stop the components first
      core.Stop();
      led_manager.Stop();

      if (core_thread.joinable()) core_thread.join();
      if (led_manager_thread.joinable()) led_manager_thread.join();

      g_core = nullptr;
      g_led_manager = nullptr;

      std::cout << "Core and LED manager stopped. Exiting.\n";
   } catch (const std::exception& e) {
      std::cerr << "Error: " << e.what() << std::endl;
      return 1;
   }

   return 0;
}