#pragma once

#include <vector>
#include <memory>
#include <string>
#include <simpleble/SimpleBLE.h>
#include "device.h"

struct DeviceConfig {
    std::string address;
    SimpleBLE::BluetoothUUID serv_uuid;
    SimpleBLE::BluetoothUUID char_uuid;
};

class DeviceManager {
private:
    std::vector<DeviceConfig> device_configs;
    std::vector<std::unique_ptr<Device>> devices;
    std::unique_ptr<SimpleBLE::Adapter> adapter;

    void InitAdapter();
    void FindAndInitDevice(DeviceConfig& dc);

public:
    DeviceManager(const std::vector<DeviceConfig>& configs);
    ~DeviceManager();

    void TurnOnDevices();
    void TurnOffDevices();
    void SetDevicesColor(int8_t r, int8_t g, int8_t b);
};
