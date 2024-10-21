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


/*
 * Provides global access to a single DeviceManager instance using the Singleton pattern
 */
class DeviceManagerSingleton {
public:
    static DeviceManager* getInstance() {
        static std::once_flag flag;
        std::call_once(flag, &DeviceManagerSingleton::initInstance);
        return instance.get();
    }

private:
    static void initInstance() {
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
        instance = std::make_unique<DeviceManager>(device_configs);
    }

    static std::unique_ptr<DeviceManager> instance;
};