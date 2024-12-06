#include "led_manager.h"
#include "log.h"
#include <fstream>

using json = nlohmann::json;

LEDManager::LEDManager(const std::vector<BLEDeviceConfig>& configs, const std::string& broker_address, const std::string& client_id,
    const std::string& ca_path, const std::string& username, const std::string& password)
    : PahoMqttClient(broker_address, client_id, ca_path, username, password),
      device_configs_(configs) {

    SetMessageCallback([this](mqtt::const_message_ptr msg) {
        this->IncomingMessage(msg->get_topic(), msg->to_string());
    });

    // Subscribe to topics
    Subscribe(COMMAND_TOPIC);
}

LEDManager::~LEDManager() {
    DEBUG_LOG("LEDManager destructor called");
    Stop();
}

void LEDManager::Initialize() {
    INFO_LOG("Starting main worker thread");
    worker_thread_ = std::thread(&LEDManager::Run, this);
}

void LEDManager::Run() {
    InitAdapter();
    FindAndInitDevices(device_configs_);
    INFO_LOG("LEDManager running...");

    auto last_status_time = std::chrono::steady_clock::now();
    auto last_reconnect_time = std::chrono::steady_clock::now();
    auto last_reinit_time = std::chrono::steady_clock::now();
    const auto status_interval = std::chrono::seconds(5);
    const auto reconnect_interval = std::chrono::seconds(10);
    const auto reinit_interval = std::chrono::seconds(60);

    while (running_) {
        // Reinitialize devices if they are not connected
        auto now = std::chrono::steady_clock::now();
        if (now - last_reinit_time >= reinit_interval) {
            ReinitDevices();
            last_reinit_time = now;
        }

        // Reconnect devices if they are disconnected
        now = std::chrono::steady_clock::now();
        if (now - last_reconnect_time >= reconnect_interval) {
            ReconnectDevices();
            last_reconnect_time = now;
        }

        // Handle commands
        json command;
        {
            std::unique_lock<std::mutex> lock(cmd_queue_mutex_);
            cmd_queue_cv_.wait_for(lock, std::chrono::seconds(1),
                [this] {return !cmd_queue_.empty() || !running_; });
            
            if (!running_) break;
            if (!cmd_queue_.empty()) {
                command = std::move(cmd_queue_.front());
                cmd_queue_.pop();
            }
        }
        if (!command.is_null()) {
            HandleCommand(command);
        }

        // Publish heartbeat status
        now = std::chrono::steady_clock::now();
        if (now - last_status_time >= status_interval) {
            try {
                nlohmann::json status_msg = {{"status", "online"}};
                Publish(STATUS_TOPIC, status_msg);
            } catch (const std::exception& e) {
                ERROR_LOG("Exception in status update: " + std::string(e.what()));
            }
            last_status_time = now;
        }
    }
    INFO_LOG("LEDManager stopped");
}

void LEDManager::Stop() {
    INFO_LOG("Stopping LEDManager");
    running_ = false;
    cmd_queue_cv_.notify_one();

    // Disconnect all devices
    for (auto& device : devices_) {
        device->Disconnect();
    }
    devices_.clear();
    
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    
    try {
        Publish(STATUS_TOPIC, "{\"status\": \"offline\"}");
        Disconnect();
        DEBUG_LOG("MQTT client disconnected");
    } catch (const mqtt::exception& e) {
        ERROR_LOG("Error disconnecting from MQTT broker: " + std::string(e.what()));
    }
}

void LEDManager::InitAdapter() {
    DEBUG_LOG("Initializing Bluetooth adapter");
    auto adapters = SimpleBLE::Adapter::get_adapters();
    if (adapters.empty()) {
        ERROR_LOG("No Bluetooth adapters found");
        throw std::runtime_error("No Bluetooth adapters found");
    }
    adapter_ = std::make_unique<SimpleBLE::Adapter>(adapters[0]);
    INFO_LOG("Bluetooth adapter initialized successfully");
}

void LEDManager::FindAndInitDevices(std::vector<BLEDeviceConfig>& dc) {
    adapter_->scan_for(5000);
    auto peripherals = adapter_->scan_get_results();
    DEBUG_LOG("Found " + std::to_string(peripherals.size()) + " BLE devices");

    for(const auto& config : dc) {
        INFO_LOG("Scanning for device: " + config.address_);
        bool found = false;

        for (auto& peripheral : peripherals) {
            if (peripheral.address() == config.address_) {
                try {
                    auto device = std::make_unique<BLEDevice>(
                        std::make_unique<SimpleBLE::Peripheral>(std::move(peripheral)),
                        config.address_,
                        config.serv_uuid_,
                        config.char_uuid_);
                    std::lock_guard<std::mutex> lock(devices_mutex_);
                    devices_.push_back(std::move(device));
                    INFO_LOG("Successfully initialized device: " + config.address_);
                    found = true;
                    break;
                } catch (const std::exception& e) {
                    ERROR_LOG("Failed to initialize device " + config.address_ + ": " + e.what());
                }
            }
        }
        if(!found) WARN_LOG("Device not found: " + config.address_);
    }
}

void LEDManager::FindAndInitDevice(BLEDeviceConfig& config) {
    adapter_->scan_for(5000);
    auto peripherals = adapter_->scan_get_results();
    DEBUG_LOG("Found " + std::to_string(peripherals.size()) + " BLE devices");

    for (auto& peripheral : peripherals) {
        if (peripheral.address() == config.address_) {
            try {
                auto device = std::make_unique<BLEDevice>(
                    std::make_unique<SimpleBLE::Peripheral>(std::move(peripheral)),
                    config.address_,
                    config.serv_uuid_,
                    config.char_uuid_);
                std::lock_guard<std::mutex> lock(devices_mutex_);
                devices_.push_back(std::move(device));
                INFO_LOG("Successfully initialized device: " + config.address_);
                return;
            } catch (const std::exception& e) {
                ERROR_LOG("Failed to initialize device " + config.address_ + ": " + e.what());
            }
        }
    }
    WARN_LOG("Device not found: " + config.address_);
}

void LEDManager::IncomingMessage(const std::string& topic, const std::string& payload) {
    INFO_LOG("Received message on topic: " + topic + ", payload: " + payload);
    if (topic.find("home/services/led_manager/command") == 0) {
        HandleCommand(json::parse(payload));
    }
}

/*
 * HandleCommand and Command Handlers 
 */
void LEDManager::HandleCommand(const json& payload) {
    try {
        std::string action = payload["command"];
        DEBUG_LOG("Handling command: " + action);
        
        auto handler = command_handlers_.find(action);
        if (handler != command_handlers_.end()) {
            handler->second(payload);
        } else {
            WARN_LOG("Unknown command received: " + action);
        }
    } catch (const std::exception& e) {
        ERROR_LOG("Error handling command: " + std::string(e.what()));
    }
}

void LEDManager::ReconnectDevices() {
    for (auto& device : devices_) {
        device->Connect();
    }
}

void LEDManager::ReinitDevices() {
    for (auto& config : device_configs_) {
        if (std::find_if(devices_.begin(), devices_.end(), 
                         [config](const std::unique_ptr<BLEDevice>& d) {
                             return d->GetAddress() == config.address_;
                         }) == devices_.end()) {
            FindAndInitDevice(config);
        }
    }
}

void LEDManager::TurnOnAll() {
    INFO_LOG("Turning on all devices");
    std::lock_guard<std::mutex> lock(devices_mutex_);
    for (auto& device : devices_) {
        device->TurnOn();
    }
}

void LEDManager::TurnOffAll() {
    INFO_LOG("Turning off all devices");
    std::lock_guard<std::mutex> lock(devices_mutex_);
    for (auto& device : devices_) {
        device->TurnOff();
    }
}

void LEDManager::SetColor(int r, int g, int b) {
    INFO_LOG("Setting color for all devices (R:" + std::to_string(r) + 
             ", G:" + std::to_string(g) + 
             ", B:" + std::to_string(b) + ")");
    std::lock_guard<std::mutex> lock(devices_mutex_);
    for (auto& device : devices_) {
        device->SetColor(r, g, b);
    }
}
