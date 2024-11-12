#pragma once

#include <string>
#include <memory>
#include <simpleble/SimpleBLE.h>


struct BLEDeviceConfig {
    std::string address;
    SimpleBLE::BluetoothUUID serv_uuid;
    SimpleBLE::BluetoothUUID char_uuid;
};

class BLEDevice {
private:
    std::unique_ptr<SimpleBLE::Peripheral> peripheral;
    std::string address;
    SimpleBLE::BluetoothUUID serv_uuid;
    SimpleBLE::BluetoothUUID char_uuid;

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
};