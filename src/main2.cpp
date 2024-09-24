#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <unistd.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>
#include <uuid/uuid.h>

// Function to connect to a Bluetooth device using UUID
int connectToDevice(const std::string& address, const std::string& uuid) {
    struct sockaddr_rc addr = { 0 };
    int s, status;
    uuid_t service_uuid;
    int channel = -1;

    // Convert string UUID to uuid_t
    if (uuid_parse(uuid.c_str(), service_uuid) < 0) {
        std::cerr << "Invalid UUID" << std::endl;
        return -1;
    }

    // Perform SDP search
    bdaddr_t target;
    str2ba(address.c_str(), &target);
    
    sdp_session_t* session = sdp_connect(&target, BDADDR_LOCAL, SDP_RETRY_IF_BUSY);
    if (!session) {
        std::cerr << "Failed to connect to SDP server" << std::endl;
        return -1;
    }

    sdp_list_t* search_list = sdp_list_append(NULL, &service_uuid);
    sdp_list_t* attrid_list = sdp_list_append(NULL, sdp_uint16_create(SDP_ATTR_PROTO_DESC_LIST));

    sdp_list_t* response_list = NULL;
    if (sdp_service_search_attr_req(session, search_list, SDP_ATTR_REQ_RANGE, attrid_list, &response_list) < 0) {
        std::cerr << "Service search failed" << std::endl;
        sdp_close(session);
        return -1;
    }

    sdp_list_t* proto_list = NULL;
    if (sdp_get_proto_desc(response_list, RFCOMM_UUID, &proto_list) == 0) {
        channel = sdp_get_proto_port(proto_list, RFCOMM_UUID);
    }

    sdp_list_free(response_list, NULL, NULL);
    sdp_list_free(search_list, NULL, NULL);
    sdp_list_free(attrid_list, NULL, NULL);
    sdp_close(session);

    if (channel < 0) {
        std::cerr << "Service not found" << std::endl;
        return -1;
    }

    // Create socket and connect
    s = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
    if (s == -1) {
        std::cerr << "Error creating socket" << std::endl;
        return -1;
    }

    addr.rc_family = AF_BLUETOOTH;
    addr.rc_channel = (uint8_t) channel;
    str2ba(address.c_str(), &addr.rc_bdaddr);

    status = connect(s, (struct sockaddr *)&addr, sizeof(addr));
    if (status == -1) {
        std::cerr << "Error connecting to device" << std::endl;
        close(s);
        return -1;
    }

    return s;
}

std::vector<uint8_t> hexStringToBytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    for (unsigned int i = 0; i < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        uint8_t byte = (uint8_t) strtol(byteString.c_str(), NULL, 16);
        bytes.push_back(byte);
    }
    return bytes;
}

bool sendHexData(int socket, const std::string& hexData) {
    std::vector<uint8_t> data = hexStringToBytes(hexData);
    int bytes_sent = send(socket, data.data(), data.size(), 0);
    if (bytes_sent == -1) {
        std::cerr << "Error sending data" << std::endl;
        return false;
    }
    return true;
}

// Function to receive data from the Bluetooth device
std::string receiveData(int socket) {
    char buffer[1024] = { 0 };
    int bytes_received = recv(socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received == -1) {
        std::cerr << "Error receiving data" << std::endl;
        return "";
    }
    return std::string(buffer, bytes_received);
}

int main() {
    // Known device address and UUID
    std::string deviceAddress = "2DC0DCE8-324D-3A88-1DED-D455BFF46107";
    std::string serviceUUID = "0000fff3-0000-1000-8000-00805f9b34fb";

    // Specific message to send
    std::string hexMessageToSend = "48656C6C6F20426C7565746F6F746821";

    // Connect to the device
    int socket = connectToDevice(deviceAddress, serviceUUID);
    if (socket == -1) {
        std::cerr << "Failed to connect. Exiting." << std::endl;
        return 1;
    }

    std::cout << "Connected to device: " << deviceAddress << std::endl;

    // Send the specific message
    if (sendHexData(socket, hexMessageToSend)) {
        std::cout << "Sent hex: " << hexMessageToSend << std::endl;
    }

    // Close the connection
    close(socket);
    std::cout << "Connection closed." << std::endl;

    return 0;
}