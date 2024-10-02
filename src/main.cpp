#include <iostream>
#include <vector>
#include <simpleble/SimpleBLE.h>
#include <thread>
#include <chrono>

std::string device_address = "BE:67:00:AC:C8:82";
SimpleBLE::BluetoothUUID service_uuid =
      SimpleBLE::BluetoothUUID("0000fff0-0000-1000-8000-00805f9b34fb");
SimpleBLE::BluetoothUUID device_uuid =
      SimpleBLE::BluetoothUUID("0000fff3-0000-1000-8000-00805f9b34fb");
std::string ON_MESSAGE = "7e0704ff00010201ef";
std::string OFF_MESSAGE = "7e07050300000010ef"; // RGB 0's


#define INFO_LOG(msg) std::cout << "Info: " << msg << std::endl;
#define ERROR_LOG(msg) std::cout << "Error: " << msg << std::endl;


std::optional<SimpleBLE::Adapter> InitAdapter() {
    if (!SimpleBLE::Adapter::bluetooth_enabled()) {
        ERROR_LOG("Bluetooth is not enabled");
        return std::nullopt;
    }

    auto adapters = SimpleBLE::Adapter::get_adapters();
    if (adapters.empty()) {
        ERROR_LOG("No Bluetooth adapters found");
        return std::nullopt;
    }

    return adapters[0];
}

std::optional<SimpleBLE::Peripheral> FindDevice(SimpleBLE::Adapter &adapter) {
   adapter.scan_for(5000);
   for (auto& peripheral : adapter.scan_get_results()) {
      if (peripheral.address().find(device_address) != std::string::npos) {
         return peripheral;
      }
   }
   return std::nullopt;
}

bool ConnectToDevice(SimpleBLE::Peripheral &device) {
   const int max_retries = 5;
   const int retry_delay_ms = 1000;

   for (int attempt = 0; attempt < max_retries; ++attempt) {
      try {
         device.connect();
         if (device.is_connected()) {
            INFO_LOG("Connected to Device");
            return true;
         }
      } catch (const SimpleBLE::Exception::OperationFailed& e) {
         ERROR_LOG("Connection failed: " + std::string(e.what()));
      } catch (const std::exception& e) {
         ERROR_LOG("Unexpected error during connection: " + std::string(e.what()));
      }

      if (attempt < max_retries - 1) {
         INFO_LOG("Retrying in " + std::to_string(retry_delay_ms) + " ms...");
         std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay_ms));
      }
   }
   ERROR_LOG("Failed to connect after " + std::to_string(max_retries) + " attempts");
   return false;
}

void PrintServiceInfo(SimpleBLE::Peripheral &device) {
   for (auto &service : device.services()) {
      INFO_LOG("Service UUID: " + service.uuid());
      for (auto &c : service.characteristics()) {
         INFO_LOG("\tCharacteristic UUID: " + std::string(c.uuid()));
      }
   }
}

void SendHexMessage(SimpleBLE::Peripheral &device, const std::string &hexMessage) {
    try {
        SimpleBLE::ByteArray bytes = SimpleBLE::ByteArray::fromHex(hexMessage);
        device.write_command(service_uuid, device_uuid, bytes);
        INFO_LOG("Message sent successfully: " + hexMessage);
    } catch (const SimpleBLE::Exception::OperationFailed& e) {
        ERROR_LOG("Failed to send message: " + std::string(e.what()));
    } catch (const std::exception& e) {
        ERROR_LOG("Unexpected error while sending message: " + std::string(e.what()));
    }
}

int main(int /* argc */, char** /* argv */) {
   auto adapter = InitAdapter();
   if (!adapter.has_value()) return 1;

   auto device = FindDevice(adapter.value());
   if (!device.has_value()) return 1;
   std::this_thread::sleep_for(std::chrono::seconds(1));

   if(!ConnectToDevice(device.value())) return 1;

   SendHexMessage(*device, OFF_MESSAGE);
   std::this_thread::sleep_for(std::chrono::seconds(5));
   SendHexMessage(*device, ON_MESSAGE);
   
   device->disconnect();
   return 0;
}