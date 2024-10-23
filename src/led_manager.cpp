#include "led_manager.h"

using json = nlohmann::json;

LEDManager::LEDManager(const std::vector<DeviceConfig>& configs, const std::string& broker_address, const std::string& client_id)
    : device_configs(configs),
      mqtt_client(broker_address, client_id),
      mqtt_conn_opts(mqtt::connect_options_builder()
        .keep_alive_interval(std::chrono::seconds(20))
        .clean_session(true)
        .automatic_reconnect(true)
        .finalize()) {
    
    mqtt_client.set_callback(*this);
}

LEDManager::~LEDManager() {
    Stop();
}

void LEDManager::Initialize() {
    InitAdapter();
    for (auto& config : device_configs) {
        FindAndInitDevice(config);
    }

    try {
        mqtt::token_ptr conntok = mqtt_client.connect(mqtt_conn_opts);
        conntok->wait();
        mqtt_client.subscribe(COMMAND_TOPIC, 1);
    } catch (const mqtt::exception& e) {
        std::cerr << "Error connecting to MQTT broker: " << e.what() << std::endl;
    }
}

void LEDManager::Run() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        PublishStatus();
    }
}

void LEDManager::Stop() {
    try {
        mqtt_client.disconnect()->wait();
    } catch (const mqtt::exception& e) {
        std::cerr << "Error disconnecting from MQTT broker: " << e.what() << std::endl;
    }
}

void LEDManager::InitAdapter() {
    auto adapters = SimpleBLE::Adapter::get_adapters();
    if (adapters.empty()) {
        throw std::runtime_error("No Bluetooth adapters found");
    }
    adapter = std::make_unique<SimpleBLE::Adapter>(adapters[0]);
    adapter->set_callback_on_scan_start([]() { std::cout << "Scan started." << std::endl; });
    adapter->set_callback_on_scan_stop([]() { std::cout << "Scan stopped." << std::endl; });
    adapter->set_callback_on_scan_found([this](SimpleBLE::Peripheral peripheral) {
        std::cout << "Found device: " << peripheral.address() << std::endl;
    });
}

void LEDManager::FindAndInitDevice(const DeviceConfig& dc) {
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
            return;
        }
    }
    std::cerr << "Device not found: " << dc.address << std::endl;
}

void LEDManager::HandleCommand(const json& command) {
    std::string action = command["action"];
    if (action == "turn_on") {
        TurnOnAll();
    } else if (action == "turn_off") {
        TurnOffAll();
    } else if (action == "set_color") {
        int r = command["params"]["r"];
        int g = command["params"]["g"];
        int b = command["params"]["b"];
        SetColor(r, g, b);
    }
    // else if (action == "set_individual") {
    //     std::string address = command["params"]["address"];
    //     std::string led_action = command["params"]["action"];
    //     json led_params = command["params"]["params"];
    //     SetIndividualLED(address, led_action, led_params);
    // }
}

void LEDManager::TurnOnAll() {
    std::lock_guard<std::mutex> lock(devices_mutex);
    for (auto& device : devices) {
        device->TurnOn();
    }
}

void LEDManager::TurnOffAll() {
    std::lock_guard<std::mutex> lock(devices_mutex);
    for (auto& device : devices) {
        device->TurnOff();
    }
}

void LEDManager::SetColor(int r, int g, int b) {
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
        mqtt_client.publish(STATUS_TOPIC, status.dump(), 1, false);
    } catch (const mqtt::exception& e) {
        std::cerr << "Error publishing status: " << e.what() << std::endl;
    }
}



void LEDManager::connected(const std::string& cause) {
    std::cout << "Connected to MQTT broker: " << cause << std::endl;
    mqtt_client.subscribe(COMMAND_TOPIC, 1);
}

void LEDManager::connection_lost(const std::string& cause) {
    std::cout << "Connection lost: " << cause << std::endl;
}

void LEDManager::message_arrived(mqtt::const_message_ptr msg) {
    if (msg->get_topic() == COMMAND_TOPIC) {
        try {
            json command = json::parse(msg->get_payload());
            HandleCommand(command);
        } catch (const json::parse_error& e) {
            std::cerr << "Error parsing command: " << e.what() << std::endl;
        }
    }
}

void LEDManager::delivery_complete(mqtt::delivery_token_ptr token) {
    (void)token;
    std::cout << "Message delivered" << std::endl;
    // This method is called when a message is successfully delivered to the broker
}
