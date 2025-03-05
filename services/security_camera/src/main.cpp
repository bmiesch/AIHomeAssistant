#include "security_camera.h"
#include "log.h"
#include <atomic>
#include <csignal>
#include <iostream>
#include <thread>
#include <chrono>

std::atomic<bool> should_run(true);

void signalHandler(int signum) {
    INFO_LOG("Interrupt signal (" + std::to_string(signum) + ") received.");
    should_run = false;
}

int main() {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::string broker_address = std::getenv("MQTT_BROKER");
    if (broker_address.empty()) {
        ERROR_LOG("MQTT_BROKER environment variable not set.");
        return 1;
    }

    auto getEnvVar = [](const char* name) -> std::string {
        const char* value = std::getenv(name);
        if (!value) {
            throw std::runtime_error(std::string("Environment variable not set: ") + name);
        }
        return std::string(value);
    };
    
    try {
        const auto username = getEnvVar("MQTT_USERNAME");
        const auto password = getEnvVar("MQTT_PASSWORD");
        const auto ca_path = getEnvVar("MQTT_CA_DIR") + "/ca.crt";

        INFO_LOG("Starting Security Camera Service...");
        SecurityCamera security_camera(broker_address, "security_camera", ca_path, username, password);
        security_camera.Initialize();

        INFO_LOG("Security Camera Service running. Press Ctrl+C to exit.");
        while(should_run) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        INFO_LOG("Initiating shutdown sequence...");
        security_camera.Stop();
        INFO_LOG("Security Camera Service stopped.");
    } catch (const std::exception& e) {
        ERROR_LOG("Error: " + std::string(e.what()));
        return 1;
    }

    return 0;
} 