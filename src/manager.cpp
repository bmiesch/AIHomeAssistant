#include "manager.h"
#include <stdexcept>

void BLEManager::InitAdapter() {
    if (!SimpleBLE::Adapter::bluetooth_enabled()) {
        throw std::runtime_error("Bluetooth not enabled");
    }
    auto adapters = SimpleBLE::Adapter::get_adapters();
    if (adapters.empty()) {
        throw std::runtime_error("No bluetooth adapters found");
    }
    adapter = std::make_unique<SimpleBLE::Adapter>(std::move(adapters[0]));
}

void BLEManager::FindAndInitDevice(DeviceConfig& dc) {
    adapter->scan_for(5000);
    for (auto& peripheral : adapter->scan_get_results()) {
        if (peripheral.address().find(dc.address) != std::string::npos) {
            devices.push_back(std::make_unique<Device>(
                std::make_unique<SimpleBLE::Peripheral>(peripheral),
                dc.address,
                dc.serv_uuid,
                dc.char_uuid
            ));
            break;
        }
    }
}

BLEManager::BLEManager(const std::vector<DeviceConfig>& configs) : device_configs(configs) {
    InitAdapter();

    for (auto& d : device_configs) {
        FindAndInitDevice(d);
    }
}

BLEManager::~BLEManager() = default;

void BLEManager::TurnOnDevices() {
    for (const auto& d : devices) {
        if (d->IsConnected()) d->TurnOn();
    }
}

void BLEManager::TurnOffDevices() {
    for (const auto& d : devices) {
        if (d->IsConnected()) d->TurnOff();
    }
}

void BLEManager::SetDevicesColor(int8_t r, int8_t g, int8_t b) {
    for (const auto& d : devices) {
        if (d->IsConnected()) d->SetColor(r,g,b);
    }
}