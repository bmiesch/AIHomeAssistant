#include "paho_mqtt_client.h"
#include "log.h"
#include <fstream>


PahoMqttClient::PahoMqttClient(const std::string& broker_address, const std::string& client_id,
    const std::string& ca_path, const std::string& username, const std::string& password)
    : mqtt_client_(broker_address, client_id) {

    mqtt_client_.set_callback(*this);

    {
        std::ifstream cert_file(ca_path);
        if (!cert_file.good()) {
            ERROR_LOG("Cannot read CA certificate at: " + ca_path);
            throw std::runtime_error("CA certificate not readable");
        }
        INFO_LOG("Successfully opened CA certificate");
    }

    mqtt_ssl_opts_ = mqtt::ssl_options_builder()
        .trust_store(ca_path)
        .enable_server_cert_auth(true)
        .finalize();

    mqtt::will_options will_opts("home/services/" + client_id + "/status", mqtt::binary_ref("offline"), 1, false);

    mqtt_conn_opts_ = mqtt::connect_options_builder()
        .keep_alive_interval(std::chrono::seconds(20))
        .clean_session(true)
        .automatic_reconnect(true)
        .user_name(username)
        .password(password)
        .will(will_opts)
        .ssl(mqtt_ssl_opts_)
        .finalize();
    
    Connect();
}

void PahoMqttClient::Connect() {
    try {
        auto connToken = mqtt_client_.connect(mqtt_conn_opts_);
        if (!connToken->wait_for(std::chrono::seconds(5))) {
            throw std::runtime_error("Failed to connect to MQTT broker");
        }
    } catch (const mqtt::exception& exc) {
        std::string error_msg = "Failed to connect to MQTT broker: " + std::string(exc.what());
        ERROR_LOG(error_msg.c_str());
    }
}

void PahoMqttClient::Disconnect() {
    mqtt_client_.disconnect();
}

void PahoMqttClient::Publish(const std::string& topic, const nlohmann::json& payload) {
    try {
        std::string message = payload.dump();
        mqtt::message_ptr pubmsg = mqtt::message::create(topic, message);
        pubmsg->set_qos(1);
        mqtt_client_.publish(pubmsg)->wait();
    } catch (const mqtt::exception& exc) {
        std::string error_msg = "Failed to publish message: " + std::string(exc.what());
        ERROR_LOG(error_msg.c_str());
    }
}

void PahoMqttClient::Subscribe(const std::string& topic) {
    mqtt_client_.subscribe(topic, 1);
    INFO_LOG("Subscribed to topic: " + topic);
}

void PahoMqttClient::SetMessageCallback(mqtt::async_client::message_handler callback) {
    message_callback_ = callback;
    INFO_LOG("MQTT message callback set");
}

void PahoMqttClient::connected(const std::string& cause) {
    INFO_LOG("MQTT connected: " + cause);
}

void PahoMqttClient::connection_lost(const std::string& cause) {
    WARN_LOG("MQTT connection lost: " + cause);
}

void PahoMqttClient::message_arrived(mqtt::const_message_ptr msg) {
    if (message_callback_) {
        message_callback_(msg);
    }
    else {
        ERROR_LOG("No message callback set");
    }
}

void PahoMqttClient::delivery_complete(mqtt::delivery_token_ptr token) {
    (void)token;
    // DEBUG_LOG("MQTT message delivered");
}