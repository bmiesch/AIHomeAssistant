#include <iostream>
#include <vector>
#include <simpleble/SimpleBLE.h>
#include <thread>
#include <chrono>


class Device {
private:
    SimpleBLE::Peripheral peripheral;
    std::string address;
    SimpleBLE::BluetoothUUID serv_uuid;
    SimpleBLE::BluetoothUUID char_uuid;

    void Connect() {
        try {
            device.connect();
            if (device.is_connected()) {
            INFO_LOG("Connected to Device: " + address);
            }
        } catch (const SimpleBLE::Exception::OperationFailed& e) {
            ERROR_LOG("Connection failed: " + std::string(e.what()));
        } catch (const std::exception& e) {
            ERROR_LOG("Unexpected error during connection: " + std::string(e.what()));
        }
    }

public:
    Device(std::string addr, SimpleBLE::BluetoothUUID serv_uuid,
           SimpleBLE::BluetoothUUID char_uuid)
        : address(addr), serv_uuid(serv_uuid), char_uuid(char_uuid) {
                Connect();
    }

    void TurnOn() {
        try {
            peripheral.write_command(serv_uuid, char_uuid, SimpleBLE::ByteArray::fromHex("7e0704ff00010201ef"));
            SetColor(0, 255, 255);
            INFO_LOG("Turned on " + address);
        } catch (const SimpleBLE::Exception::OperationFailed& e) {
            ERROR_LOG("Failed to send message: " + std::string(e.what()));
        } catch (const std::exception& e) {
            ERROR_LOG("Unexpected error while sending message: " + std::string(e.what()));
        }
    }

    void TurnOff() {
        SetColor(0, 0, 0);
    }

    void SetColor(int8_t r, int8_t g, int8_t b) {
        std::string hex = "7e070503" + 
            SimpleBLE::ByteArray({r, g, b}).toHexString(false) + "10ef";
        try {
            peripheral.write_command(serv_uuid, char_uuid, SimpleBLE::ByteArray::fromHex(hex));
            INFO_LOG("Color set for device " + address);
        } catch (const std::exception& e) {
            ERROR_LOG("Failed to set color for " + address + ": " + e.what());
        }
    }
}