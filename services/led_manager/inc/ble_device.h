#pragma once

#include <string>
#include <memory>
#include <simpleble/SimpleBLE.h>
#include <atomic>


struct BLEDeviceConfig {
    std::string address_;
    SimpleBLE::BluetoothUUID serv_uuid_;
    SimpleBLE::BluetoothUUID char_uuid_;
};

class BLEDevice {
private:
    std::unique_ptr<SimpleBLE::Peripheral> peripheral_;
    std::string address_;
    SimpleBLE::BluetoothUUID serv_uuid_;
    SimpleBLE::BluetoothUUID char_uuid_;

public:
    BLEDevice(std::unique_ptr<SimpleBLE::Peripheral> p, std::string addr,
           SimpleBLE::BluetoothUUID serv_uuid, SimpleBLE::BluetoothUUID char_uuid);
    ~BLEDevice();

    void Connect();
    bool IsConnected();
    void Disconnect();
    
    void TurnOn();
    void TurnOff();
    void SetColor(uint8_t r, uint8_t g, uint8_t b);
    std::string GetAddress();

    BLEDevice(const BLEDevice&) = delete;
    BLEDevice& operator=(const BLEDevice&) = delete;
};