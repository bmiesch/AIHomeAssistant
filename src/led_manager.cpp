#include "led_manager.h"
#include "log.h"

using json = nlohmann::json;

LEDManager::LEDManager(const std::vector<DeviceConfig>& configs, const std::string& broker_address, const std::string& client_id)
    : device_configs(configs),
      mqtt_client(broker_address, client_id),
      mqtt_conn_opts(mqtt::connect_options_builder()
        .keep_alive_interval(std::chrono::seconds(20))
        .clean_session(true)
        .automatic_reconnect(true)
        .finalize()) {
    
    INFO_LOG("Initializing LEDManager with broker: " + broker_address + ", client_id: " + client_id);
    mqtt_client.set_callback(*this);
}

LEDManager::~LEDManager() {
    DEBUG_LOG("LEDManager destructor called");
    Stop();
}

void LEDManager::Initialize() {
    InitAdapter();
    for (auto& config : device_configs) {
        FindAndInitDevice(config);
    }

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
}

void LEDManager::Run() {
    INFO_LOG("LEDManager running...");
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        PublishStatus();
    }
}

void LEDManager::Stop() {
    try {
        INFO_LOG("Stopping LEDManager...");
        mqtt_client.disconnect()->wait();
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
    adapter = std::make_unique<SimpleBLE::Adapter>(adapters[0]);
    adapter->set_callback_on_scan_start([]() { DEBUG_LOG("Scan started"); });
    adapter->set_callback_on_scan_stop([]() { DEBUG_LOG("Scan stopped"); });
    adapter->set_callback_on_scan_found([this](SimpleBLE::Peripheral peripheral) {
        DEBUG_LOG("Found device: " + peripheral.address());
    });
    INFO_LOG("Bluetooth adapter initialized successfully");
}

void LEDManager::FindAndInitDevice(const DeviceConfig& dc) {
    INFO_LOG("Scanning for device: " + dc.address);
    adapter->scan_for(5000);
    auto peripherals = adapter->scan_get_results();
    for (auto& peripheral : peripherals) {
        if (peripheral.address() == dc.address) {
            auto peripheral_ptr = std::make_unique<SimpleBLE::Peripheral>(std::move(peripheral));
            auto device = std::make_unique<Device>(
                std::move(peripheral_ptr),
                dc.address,
                dc.serv_uuid,
                dc.char_uuid);
            std::lock_guard<std::mutex> lock(devices_mutex);
            devices.push_back(std::move(device));
            INFO_LOG("Successfully initialized device: " + dc.address);
            return;
        }
    }
    WARN_LOG("Device not found: " + dc.address);
}

void LEDManager::HandleCommand(const json& command) {
    std::string action = command["action"];
    DEBUG_LOG("Handling command: " + action);
    
    if (action == "turn_on") {
        TurnOnAll();
    } else if (action == "turn_off") {
        TurnOffAll();
    } else if (action == "set_color") {
        int r = command["params"]["r"];
        int g = command["params"]["g"];
        int b = command["params"]["b"];
        DEBUG_LOG("Setting color (R:" + std::to_string(r) + 
                 ", G:" + std::to_string(g) + 
                 ", B:" + std::to_string(b) + ")");
        SetColor(r, g, b);
    } else {
        WARN_LOG("Unknown command received: " + action);
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
            json command = json::parse(msg->get_payload());
            HandleCommand(command);
        } catch (const json::parse_error& e) {
            ERROR_LOG("Error parsing command: " + std::string(e.what()));
        }
    }
}

void LEDManager::delivery_complete(mqtt::delivery_token_ptr token) {
    (void)token;
    DEBUG_LOG("MQTT message delivered");
}