#include "ble_device.h"
#include "log.h"
#include <thread>
#include <chrono>


BLEDevice::BLEDevice(std::unique_ptr<SimpleBLE::Peripheral> p, std::string addr,
               SimpleBLE::BluetoothUUID serv_uuid, SimpleBLE::BluetoothUUID char_uuid)
    : peripheral(std::move(p)), address(std::move(addr)), 
      serv_uuid(std::move(serv_uuid)), char_uuid(std::move(char_uuid)) {
        
    if(!peripheral) {
        throw std::runtime_error("Null peripheral passed to Device constructor");
    }
    Connect();
}

BLEDevice::~BLEDevice() {
    if (peripheral && peripheral->is_connected()) {
        try {
            peripheral->disconnect();
            DEBUG_LOG("Disconnected from device: " + address);
        } catch (const std::exception& e) {
            ERROR_LOG("Error disconnecting from device " + address + ": " + e.what());
        }
    }
}

void BLEDevice::Connect() {
    const int MAX_ATTEMPTS = 3;
    const int RETRY_DELAY_MS = 1000;

    for (int i = 1; i <= MAX_ATTEMPTS; i++) {
        try {
            peripheral->connect();
            if (peripheral->is_connected()) {
                INFO_LOG("Connected to Device: " + address);
                return;
            }
            WARN_LOG("Connection attempt " + std::to_string(i) + " succeeded but device reports as disconnected");
        } catch (const std::exception& e) {
            if (i < MAX_ATTEMPTS) {
                WARN_LOG("Attempt " + std::to_string(i) + " failed to connect to " 
                        + address + ": " + e.what() + ". Retrying...");
                std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
            } else {
                ERROR_LOG("All attempts failed to connect to " + address 
                         + ". Last error: " + e.what());
                throw std::runtime_error("Failed to connect to device after " + 
                                       std::to_string(MAX_ATTEMPTS) + " attempts");
            }
        }
    }
}

bool BLEDevice::IsConnected() {
    bool connected = peripheral->is_connected();
    DEBUG_LOG("Device " + address + " connection status: " + (connected ? "connected" : "disconnected"));
    return connected;
}

void BLEDevice::TurnOn() {
    try {
        peripheral->write_command(serv_uuid, char_uuid, SimpleBLE::ByteArray::fromHex("7e0704ff00010201ef"));
        SetColor(static_cast<uint8_t>(0), static_cast<uint8_t>(255), static_cast<uint8_t>(255));
        INFO_LOG("Turned on device: " + address);
    } catch (const SimpleBLE::Exception::OperationFailed& e) {
        ERROR_LOG("Failed to turn on device " + address + ": " + e.what());
        throw;
    } catch (const std::exception& e) {
        ERROR_LOG("Unexpected error while turning on device " + address + ": " + e.what());
        throw;
    }
}

void BLEDevice::TurnOff() {
    DEBUG_LOG("Turning off device: " + address);
    SetColor(static_cast<uint8_t>(0), static_cast<uint8_t>(0), static_cast<uint8_t>(0));
}

void BLEDevice::SetColor(uint8_t r, uint8_t g, uint8_t b) {
    char hex[21];
    snprintf(hex, sizeof(hex), "7e070503%02x%02x%02x10ef", r, g, b);
    try {
        peripheral->write_command(serv_uuid, char_uuid, SimpleBLE::ByteArray::fromHex(hex));
        INFO_LOG("Set color (R:" + std::to_string(r) + 
                 ", G:" + std::to_string(g) + 
                 ", B:" + std::to_string(b) + 
                 ") for device: " + address);
    } catch (const std::exception& e) {
        ERROR_LOG("Failed to set color for " + address + ": " + e.what());
        throw;
    }
}