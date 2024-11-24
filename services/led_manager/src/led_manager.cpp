#include "led_manager.h"
#include "log.h"
#include <fstream>

using json = nlohmann::json;

LEDManager::LEDManager(const std::vector<BLEDeviceConfig>& configs, const std::string& broker_address, const std::string& client_id)
    : device_configs(configs),
      mqtt_client(broker_address, client_id) {

    try {
        InitializeMqttConnection();
        mqtt_client.set_callback(*this);
        INFO_LOG("LEDManager initialized with broker: " + broker_address + ", client_id: " + client_id);
    }
    catch (const std::exception& e) {
        ERROR_LOG("Failed to initialize LEDManager: " + std::string(e.what()));
        throw;
    }
}

LEDManager::~LEDManager() {
    DEBUG_LOG("LEDManager destructor called");
    Stop();
}

void LEDManager::Initialize() {
    try {
        INFO_LOG("Connecting to MQTT broker...");
        mqtt::token_ptr conntok = mqtt_client.connect(mqtt_conn_opts);
        conntok->wait();
        mqtt_client.subscribe(COMMAND_TOPIC, 1);
        INFO_LOG("Successfully connected to MQTT broker");
    } catch (const mqtt::exception& e) {
        ERROR_LOG("Error connecting to MQTT broker: " + std::string(e.what()));
        throw;
    }

    INFO_LOG("Starting main worker thread");
    worker_thread = std::thread(&LEDManager::Run, this);
}

void LEDManager::Run() {
    InitAdapter();
    FindAndInitDevices(device_configs);
    INFO_LOG("LEDManager running...");

    auto last_status_time = std::chrono::steady_clock::now();
    auto last_reconnect_time = std::chrono::steady_clock::now();
    auto last_reinit_time = std::chrono::steady_clock::now();
    const auto status_interval = std::chrono::seconds(5);
    const auto reconnect_interval = std::chrono::seconds(10);
    const auto reinit_interval = std::chrono::seconds(60);

    while (running) {
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
            std::unique_lock<std::mutex> lock(cmd_queue_mutex);
            cmd_queue_cv.wait_for(lock, std::chrono::seconds(1),
                [this] {return !cmd_queue.empty() || !running; });
            
            if (!running) break;
            if (!cmd_queue.empty()) {
                command = std::move(cmd_queue.front());
                cmd_queue.pop();
            }
        }
        if (!command.is_null()) {
            HandleCommand(command);
        }

        // Publish heartbeat status
        now = std::chrono::steady_clock::now();
        if (now - last_status_time >= status_interval) {
            PublishStatus();
            last_status_time = now;
        }
    }
    INFO_LOG("LEDManager stopped");
}

void LEDManager::Stop() {
    INFO_LOG("Stopping LEDManager");
    running = false;
    cmd_queue_cv.notify_one();

    // Disconnect all devices
    for (auto& device : devices) {
        device->Disconnect();
    }
    devices.clear();
    
    if (worker_thread.joinable()) {
        worker_thread.join();
    }
    
    try {
        mqtt_client.publish(STATUS_TOPIC, "{\"status\": \"offline\"}", 1, false);
        mqtt_client.disconnect()->wait();
        DEBUG_LOG("MQTT client disconnected");
    } catch (const mqtt::exception& e) {
        ERROR_LOG("Error disconnecting from MQTT broker: " + std::string(e.what()));
    }
}

void LEDManager::InitializeMqttConnection() {
    auto getEnvVar = [](const char* name) -> std::string {
        const char* value = std::getenv(name);
        if (!value) {
            throw std::runtime_error(std::string("Environment variable not set: ") + name);
        }
        return std::string(value);
    };

    const auto username = getEnvVar("MQTT_USERNAME");
    const auto password = getEnvVar("MQTT_PASSWORD");
    const auto ca_path = getEnvVar("MQTT_CA_DIR") + "/ca.crt";

    // Test CA certificate file access
    {
        std::ifstream cert_file(ca_path);
        if (!cert_file.good()) {
            ERROR_LOG("Cannot read CA certificate at: " + ca_path);
            throw std::runtime_error("CA certificate not readable");
        }
        INFO_LOG("Successfully opened CA certificate");
    }

    mqtt::will_options will_opts(STATUS_TOPIC, mqtt::binary_ref("offline"), 1, false);

    try {
        mqtt_ssl_opts = mqtt::ssl_options_builder()
            .trust_store(ca_path)
            .enable_server_cert_auth(true)
            .finalize();

        mqtt_conn_opts = mqtt::connect_options_builder()
            .keep_alive_interval(std::chrono::seconds(20))
            .clean_session(true)
            .automatic_reconnect(true)
            .user_name(username)
            .password(password)
            .will(will_opts)
            .ssl(mqtt_ssl_opts)
            .finalize();
    }
    catch (const mqtt::exception& e) {
        throw std::runtime_error("MQTT configuration failed: " + std::string(e.what()));
    }
}

void LEDManager::InitAdapter() {
    DEBUG_LOG("Initializing Bluetooth adapter");
    auto adapters = SimpleBLE::Adapter::get_adapters();
    if (adapters.empty()) {
        ERROR_LOG("No Bluetooth adapters found");
        throw std::runtime_error("No Bluetooth adapters found");
    }
    adapter = std::make_unique<SimpleBLE::Adapter>(adapters[0]);
    INFO_LOG("Bluetooth adapter initialized successfully");
}

void LEDManager::FindAndInitDevices(std::vector<BLEDeviceConfig>& dc) {
    adapter->scan_for(5000);
    auto peripherals = adapter->scan_get_results();
    DEBUG_LOG("Found " + std::to_string(peripherals.size()) + " BLE devices");

    for(const auto& config : dc) {
        INFO_LOG("Scanning for device: " + config.address);
        bool found = false;

        for (auto& peripheral : peripherals) {
            if (peripheral.address() == config.address) {
                try {
                    auto device = std::make_unique<BLEDevice>(
                        std::make_unique<SimpleBLE::Peripheral>(std::move(peripheral)),
                        config.address,
                        config.serv_uuid,
                        config.char_uuid);
                    std::lock_guard<std::mutex> lock(devices_mutex);
                    devices.push_back(std::move(device));
                    INFO_LOG("Successfully initialized device: " + config.address);
                    found = true;
                    break;
                } catch (const std::exception& e) {
                    ERROR_LOG("Failed to initialize device " + config.address + ": " + e.what());
                }
            }
        }
        if(!found) WARN_LOG("Device not found: " + config.address);
    }
}

void LEDManager::FindAndInitDevice(BLEDeviceConfig& config) {
    adapter->scan_for(5000);
    auto peripherals = adapter->scan_get_results();
    DEBUG_LOG("Found " + std::to_string(peripherals.size()) + " BLE devices");

    for (auto& peripheral : peripherals) {
        if (peripheral.address() == config.address) {
            try {
                auto device = std::make_unique<BLEDevice>(
                    std::make_unique<SimpleBLE::Peripheral>(std::move(peripheral)),
                    config.address,
                    config.serv_uuid,
                    config.char_uuid);
                std::lock_guard<std::mutex> lock(devices_mutex);
                devices.push_back(std::move(device));
                INFO_LOG("Successfully initialized device: " + config.address);
                return;
            } catch (const std::exception& e) {
                ERROR_LOG("Failed to initialize device " + config.address + ": " + e.what());
            }
        }
    }
    WARN_LOG("Device not found: " + config.address);
}

void LEDManager::PublishStatus() {
    json status;
    status["status"] = "online";
    status["device_count"] = devices.size();
    
    try {
        DEBUG_LOG("Publishing status update");
        mqtt_client.publish(STATUS_TOPIC, status.dump(), 1, false);
    } catch (const mqtt::exception& e) {
        ERROR_LOG("Error publishing status: " + std::string(e.what()));
    }
}

/*
 * HandleCommand and Command Handlers 
 */
void LEDManager::HandleCommand(const json& payload) {
    try {
        std::string action = payload["command"];
        DEBUG_LOG("Handling command: " + action);
        
        auto handler = command_handlers.find(action);
        if (handler != command_handlers.end()) {
            handler->second(payload);
        } else {
            WARN_LOG("Unknown command received: " + action);
        }
    } catch (const std::exception& e) {
        ERROR_LOG("Error handling command: " + std::string(e.what()));
    }
}

void LEDManager::ReconnectDevices() {
    for (auto& device : devices) {
        device->Connect();
    }
}

void LEDManager::ReinitDevices() {
    for (auto& config : device_configs) {
        if (std::find_if(devices.begin(), devices.end(), 
                         [config](const std::unique_ptr<BLEDevice>& d) {
                             return d->GetAddress() == config.address;
                         }) == devices.end()) {
            FindAndInitDevice(config);
        }
    }
}

void LEDManager::TurnOnAll() {
    INFO_LOG("Turning on all devices");
    std::lock_guard<std::mutex> lock(devices_mutex);
    for (auto& device : devices) {
        device->TurnOn();
    }
}

void LEDManager::TurnOffAll() {
    INFO_LOG("Turning off all devices");
    std::lock_guard<std::mutex> lock(devices_mutex);
    for (auto& device : devices) {
        device->TurnOff();
    }
}

void LEDManager::SetColor(int r, int g, int b) {
    INFO_LOG("Setting color for all devices (R:" + std::to_string(r) + 
             ", G:" + std::to_string(g) + 
             ", B:" + std::to_string(b) + ")");
    std::lock_guard<std::mutex> lock(devices_mutex);
    for (auto& device : devices) {
        device->SetColor(r, g, b);
    }
}

/*
 * MQTT Callback Functions
 * These functions override the virtual callbacks from mqtt::callback
 * - connected: Called when connection to broker is established
 * - connection_lost: Called when connection to broker is lost
 * - message_arrived: Called when a message is received on a subscribed topic
 * - delivery_complete: Called when a message publish is completed
 */
void LEDManager::connected(const std::string& cause) {
    INFO_LOG("Connected to MQTT broker: " + cause);
    mqtt_client.subscribe(COMMAND_TOPIC, 1);
}

void LEDManager::connection_lost(const std::string& cause) {
    WARN_LOG("MQTT connection lost: " + cause);
}

void LEDManager::message_arrived(mqtt::const_message_ptr msg) {
    if (msg->get_topic() == COMMAND_TOPIC) {
        try {
            DEBUG_LOG("Received message on command topic");
            json payload = json::parse(msg->get_payload());
            {
                std::lock_guard<std::mutex> lock(cmd_queue_mutex);
                cmd_queue.push(payload);
            }
            cmd_queue_cv.notify_one();

        } catch (const json::parse_error& e) {
            ERROR_LOG("Error parsing command: " + std::string(e.what()));
        }
    }
}

void LEDManager::delivery_complete(mqtt::delivery_token_ptr token) {
    (void)token;
    DEBUG_LOG("MQTT message delivered");
}