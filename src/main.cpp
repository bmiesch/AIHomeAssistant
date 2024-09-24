#include <iostream>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <unistd.h>
#include <sys/socket.h>
#include <cstring>

// Function to connect to the Bluetooth device using RFCOMM
int connectToDevice(const std::string& address) {
    struct sockaddr_rc addr = { 0 };
    int sock, status;

    // Allocate a socket for Bluetooth communication
    sock = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
    if (sock < 0) {
        std::cerr << "Failed to create socket!" << std::endl;
        return -1;
    }

    // Set the connection parameters (Bluetooth address and RFCOMM channel)
    addr.rc_family = AF_BLUETOOTH;
    addr.rc_channel = 1; // RFCOMM channel 1 is typical for devices
    str2ba(address.c_str(), &addr.rc_bdaddr); // Convert address

    // Attempt to connect to the Bluetooth device
    status = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    if (status < 0) {
        std::cerr << "Failed to connect to device: " << address << std::endl;
        close(sock);
        return -1;
    }

    std::cout << "Connected to " << address << std::endl;
    return sock; // Return the connected socket descriptor
}

// Function to send a command to the Bluetooth device
void sendCommand(int socket_fd, const std::string& command) {
    ssize_t bytes_written = write(socket_fd, command.c_str(), command.size());
    if (bytes_written < 0) {
        std::cerr << "Failed to send command: " << command << std::endl;
    } else {
        std::cout << "Command sent: " << command << std::endl;
    }
}

int main() {
    // Bluetooth address of your lights (replace this with your actual device's MAC address)
    std::string device_address = "XX:XX:XX:XX:XX:XX";  // Replace with your device's Bluetooth MAC address

    // Step 1: Connect to the Bluetooth device
    int socket_fd = connectToDevice(device_address);
    if (socket_fd < 0) {
        return 1;  // Exit if connection failed
    }

    // Step 2: Send commands (for example, to turn the lights on or off)
    // Command to turn lights on (you will need to determine the exact command from your reverse engineering)
    sendCommand(socket_fd, "ON");  // Example command; replace with the real command

    // Optionally, send other commands (for example, changing color)
    // sendCommand(socket_fd, "OFF"); // Example to turn lights off
    // sendCommand(socket_fd, "\xFF\x00\x00"); // Example RGB color change (red)

    // Step 3: Close the connection
    close(socket_fd);
    std::cout << "Connection closed." << std::endl;

    return 0;
}
