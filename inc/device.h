#ifndef DEVICE_H
#define DEVICE_H

#include <string>
#include <memory>
#include <simpleble/SimpleBLE.h>

class Device {
private:
    std::unique_ptr<SimpleBLE::Peripheral> peripheral;
    std::string address;
    SimpleBLE::BluetoothUUID serv_uuid;
    SimpleBLE::BluetoothUUID char_uuid;

    void Connect();

public:
    Device(std::unique_ptr<SimpleBLE::Peripheral> p, std::string addr,
           SimpleBLE::BluetoothUUID serv_uuid, SimpleBLE::BluetoothUUID char_uuid);
    ~Device();

    bool IsConnected();
    void TurnOn();
    void TurnOff();
    void SetColor(uint8_t r, uint8_t g, uint8_t b);
};

#endif // DEVICE_H