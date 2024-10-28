#include "kernel.h"
#include "led_manager.h"
#include <atomic>
#include <csignal>
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>


std::atomic<bool> should_run(true);
Kernel* g_kernel = nullptr;
LEDManager* g_led_manager = nullptr;

void signalHandler(int signum) {
    std::cout << "Interrupt signal (" << signum << ") received." << std::endl;
    should_run = false;
    if (g_kernel) {
        g_kernel->Stop();
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
   std::vector<DeviceConfig> device_configs = {
      DeviceConfig{
            "BE:67:00:AC:C8:82",
            SimpleBLE::BluetoothUUID("0000fff0-0000-1000-8000-00805f9b34fb"),
            SimpleBLE::BluetoothUUID("0000fff3-0000-1000-8000-00805f9b34fb")
      },
      DeviceConfig{
            "BE:67:00:6A:B5:A6",
            SimpleBLE::BluetoothUUID("0000fff0-0000-1000-8000-00805f9b34fb"),
            SimpleBLE::BluetoothUUID("0000fff3-0000-1000-8000-00805f9b34fb")
      }
   };

   try {
      Kernel kernel(broker_address, "kernel_client");
      LEDManager led_manager(device_configs, broker_address, "led_manager_client");

      g_kernel = &kernel;
      g_led_manager = &led_manager;

      kernel.Initialize();
      led_manager.Initialize();

      std::thread kernel_thread([&kernel]() {
         while (should_run) {
            kernel.Run();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
         }
      });

      std::thread led_manager_thread([&led_manager]() {
         while (should_run) {
            led_manager.Run();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
         }
      });

      // Wait for a signal to stop
      while(should_run) {
         std::this_thread::sleep_for(std::chrono::seconds(1));
      }

      std::cout << "Stopping kernel and LED manager...\n";

      kernel_thread.join();
      led_manager_thread.join();

      g_kernel = nullptr;
      g_led_manager = nullptr;

      std::cout << "Kernel and LED manager stopped. Exiting.\n";
   } catch (const std::exception& e) {
      std::cerr << "Error: " << e.what() << std::endl;
      return 1;
   }

   return 0;
}