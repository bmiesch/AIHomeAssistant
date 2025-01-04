#pragma once

#include <mqtt/async_client.h>
#include <mqtt/callback.h>
#include <mqtt/types.h>
#include <nlohmann/json.hpp>
#include "mqtt_interface.h"

class PahoMqttClient : public IMqttClient, public mqtt::callback {
private:
    mqtt::async_client mqtt_client_;
    mqtt::ssl_options mqtt_ssl_opts_;
    mqtt::connect_options mqtt_conn_opts_;
    mqtt::async_client::message_handler message_callback_;  

    // mqtt::callback implementation
    void connected(const std::string& cause) override;
    void connection_lost(const std::string& cause) override;
    void message_arrived(mqtt::const_message_ptr msg) override;
    void delivery_complete(mqtt::delivery_token_ptr token) override;

public:
    explicit PahoMqttClient(const std::string& broker_address, const std::string& client_id,
        const std::string& ca_path, const std::string& username, const std::string& password);

    // IMqttClient implementation
    void Connect() override;
    void Disconnect() override;
    void Publish(const std::string& topic, const nlohmann::json& payload) override;
    void Subscribe(const std::string& topic) override;
    void SetMessageCallback(mqtt::async_client::message_handler callback) override;
};