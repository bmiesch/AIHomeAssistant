#include "ble_device.h"
#include "log.h"
#include <thread>
#include <chrono>
#include "led_manager.h"

BLEDevice::BLEDevice(std::unique_ptr<SimpleBLE::Peripheral> p, std::string addr,
               SimpleBLE::BluetoothUUID serv_uuid, SimpleBLE::BluetoothUUID char_uuid)
    : peripheral(std::move(p)), address(std::move(addr)), 
      serv_uuid(std::move(serv_uuid)), char_uuid(std::move(char_uuid)) {
        
    if(!peripheral) {
        throw std::runtime_error("Null peripheral passed to Device constructor");
    }

    peripheral->set_callback_on_connected([this]() {
        INFO_LOG("Connected to device: " + address);
    });

    peripheral->set_callback_on_disconnected([this]() {
        WARN_LOG("Auto disconnected from device: " + address);
    });
    Connect();
}

BLEDevice::~BLEDevice() {
    Disconnect();
}

const int MAX_ATTEMPTS = 3;
const int RETRY_DELAY_MS = 1000;
void BLEDevice::Connect() {
    try {
        if (!peripheral->is_connected()) {
            peripheral->connect();
        }
    } catch (const SimpleBLE::Exception::OperationFailed& e) {
        ERROR_LOG("Failed to connect to device " + address + ": " + e.what());
    } catch (const std::exception& e) {
        ERROR_LOG("Unexpected error while connecting to device " + address + ": " + e.what());
    }
}

void BLEDevice::Disconnect() {
    try {
        if (!peripheral->is_connected()) {
            DEBUG_LOG("Device " + address + " already disconnected");
            return;
        }
        peripheral->disconnect();
    } catch (const SimpleBLE::Exception::OperationFailed& e) {
        ERROR_LOG("Failed to disconnect from device " + address + ": " + e.what());
    } catch (const std::exception& e) {
        ERROR_LOG("Error disconnecting from device " + address + ": " + e.what());
    }
}

bool BLEDevice::IsConnected() {
    try {
        bool connected = peripheral->is_connected();
        DEBUG_LOG("Device " + address + " connection status: " + (connected ? "connected" : "disconnected"));
        return connected;
    } catch (const SimpleBLE::Exception::OperationFailed& e) {
        ERROR_LOG("Failed to check connection status for device " + address + ": " + e.what());
        return false;
    } catch (const std::exception& e) {
        ERROR_LOG("Error checking connection status for device " + address + ": " + e.what());
        return false;
    }
}

void BLEDevice::TurnOn() {
    try {
        Connect();
        peripheral->write_command(serv_uuid, char_uuid, SimpleBLE::ByteArray::fromHex("7e0704ff00010201ef"));
        SetColor(static_cast<uint8_t>(0), static_cast<uint8_t>(255), static_cast<uint8_t>(255));
        INFO_LOG("Turned on device: " + address);
    } catch (const SimpleBLE::Exception::OperationFailed& e) {
        ERROR_LOG("Failed to turn on device " + address + ": " + e.what());
    } catch (const std::exception& e) {
        ERROR_LOG("Unexpected error while turning on device " + address + ": " + e.what());
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
        Connect();
        peripheral->write_command(serv_uuid, char_uuid, SimpleBLE::ByteArray::fromHex(hex));
        INFO_LOG("Set color (R:" + std::to_string(r) + 
                 ", G:" + std::to_string(g) + 
                 ", B:" + std::to_string(b) + 
                 ") for device: " + address);
    } catch (const SimpleBLE::Exception::OperationFailed& e) {
        ERROR_LOG("Failed to set color for " + address + ": " + e.what());
    } catch (const std::exception& e) {
        ERROR_LOG("Failed to set color for " + address + ": " + e.what());
    }
}

std::string BLEDevice::GetAddress() {
    return address;
}