#include "core.h"
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

    // Default address for MQTT Broker
    std::string broker_address = "tcp://localhost:1883";

    try {
        Core core(broker_address, "core_client");
        core.Initialize();

        while(should_run) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        INFO_LOG("Initiating shutdown sequence...");
        core.Stop();
        INFO_LOG("Shutdown complete.");
    } catch (const std::exception& e) {
        ERROR_LOG("Error: " + std::string(e.what()));
        return 1;
    }

    return 0;
}