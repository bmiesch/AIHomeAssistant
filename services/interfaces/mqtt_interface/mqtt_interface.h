#pragma once

#include <string>
#include <functional>
#include <mqtt/async_client.h>
#include <nlohmann/json.hpp>

class IMqttClient {
public:
    virtual ~IMqttClient() = default;

    // Essential connection methods
    virtual void Connect() = 0;
    virtual void Disconnect() = 0;

    // Core messaging methods
    virtual void Publish(const std::string& topic, const nlohmann::json& payload) = 0;
    virtual void Subscribe(const std::string& topic) = 0;

    // Callback methods
    virtual void SetMessageCallback(mqtt::async_client::message_handler callback) = 0;
};