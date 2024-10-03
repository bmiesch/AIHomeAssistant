#include "device.h"
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>

// Assuming you have these logging macros defined somewhere
#define INFO_LOG(x) std::cout << "INFO: " << x << std::endl
#define WARN_LOG(x) std::cout << "WARN: " << x << std::endl
#define ERROR_LOG(x) std::cerr << "ERROR: " << x << std::endl

void Device::Connect() {
    const int MAX_ATTEMPTS = 3;
    const int RETRY_DELAY_MS = 1000;

    INFO_LOG("Entering Connect function for Device: " + address);

    for (int i = 1; i <= MAX_ATTEMPTS; i++) {
        try {
            peripheral->connect();
            if (peripheral->is_connected()) {
                INFO_LOG("Connected to Device: " + address);
                return;
            }
        } catch (const std::exception& e) {
            if (i < MAX_ATTEMPTS) {
                WARN_LOG("Attempt " + std::to_string(i) + " failed to connect to " 
                            + address + ": " + e.what() + ". Retrying...");
                std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
            } else {
                ERROR_LOG("All attempts failed to connect to " + address 
                            + ". Last error: " + e.what());
            }
        }
    }
}

Device::Device(std::unique_ptr<SimpleBLE::Peripheral> p, std::string addr,
               SimpleBLE::BluetoothUUID serv_uuid, SimpleBLE::BluetoothUUID char_uuid)
    : peripheral(std::move(p)), address(std::move(addr)), serv_uuid(std::move(serv_uuid)), char_uuid(std::move(char_uuid)) {
    if(!peripheral) throw std::runtime_error("Null peripheral passed to Device constructor");
    Connect();
}

Device::~Device() {
    if (peripheral && peripheral->is_connected()) {
        peripheral->disconnect();
    }
}

bool Device::IsConnected() {
    return peripheral->is_connected();
}

void Device::TurnOn() {
    try {
        peripheral->write_command(serv_uuid, char_uuid, SimpleBLE::ByteArray::fromHex("7e0704ff00010201ef"));
        SetColor(static_cast<uint8_t>(0), static_cast<uint8_t>(255), static_cast<uint8_t>(255));
        INFO_LOG("Turned on " + address);
    } catch (const SimpleBLE::Exception::OperationFailed& e) {
        ERROR_LOG("Failed to send message: " + std::string(e.what()));
    } catch (const std::exception& e) {
        ERROR_LOG("Unexpected error while sending message: " + std::string(e.what()));
    }
}

void Device::TurnOff() {
    SetColor(static_cast<uint8_t>(0), static_cast<uint8_t>(0), static_cast<uint8_t>(0));
}

void Device::SetColor(uint8_t r, uint8_t g, uint8_t b) {
    char hex[21];
    snprintf(hex, sizeof(hex), "7e070503%02x%02x%02x10ef", r, g, b);
    try {
        peripheral->write_command(serv_uuid, char_uuid, SimpleBLE::ByteArray::fromHex(hex));
        INFO_LOG("Color set for device " + address);
    } catch (const std::exception& e) {
        ERROR_LOG("Failed to set color for " + address + ": " + e.what());
    }
}