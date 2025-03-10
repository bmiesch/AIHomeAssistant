#include "security_camera.h"
#include "log.h"
#include <chrono>
#include <vector>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <random>
#include <algorithm>

SecurityCamera::SecurityCamera(const std::string& broker_address, const std::string& client_id, 
                             const std::string& ca_path, const std::string& username, 
                             const std::string& password)
    : PahoMqttClient(broker_address, client_id, ca_path, username, password) {
    
    // Get environment variables
    int camera_id = 0;
    int width = 640;
    int height = 480;
    int fps = 15;
    
    GetEnvVar("CAMERA_ID", camera_id);
    GetEnvVar("FRAME_WIDTH", width);
    GetEnvVar("FRAME_HEIGHT", height);
    GetEnvVar("FPS_TARGET", fps);
    
    // Get SSL certificate and key paths
    GetEnvVar("HTTPS_CERT_PATH", cert_file_);
    GetEnvVar("HTTPS_KEY_PATH", key_file_);
    GetEnvVar("HTTPS_ENABLED", use_https_);
    
    // Initialize camera with settings
    camera_capture_ = std::make_unique<CameraCapture>(camera_id, width, height, fps);
    frame_processor_ = std::make_unique<FrameProcessor>();

    // Set up MQTT message callback
    SetMessageCallback([this](mqtt::const_message_ptr msg) {
        this->IncomingMessage(msg->get_topic(), msg->to_string());
    });

    // Subscribe to command topic
    Subscribe(COMMAND_TOPIC);
    
    // Initialize OpenSSL if HTTPS is enabled
    if (use_https_) {
        if (!InitializeSSL()) {
            ERROR_LOG("Failed to initialize SSL, falling back to HTTP");
            use_https_ = false;
        }
    }
}

SecurityCamera::~SecurityCamera() {
    DEBUG_LOG("SecurityCamera destructor called");
    Stop();
    
    // Cleanup SSL
    if (ssl_ctx_) {
        CleanupSSL();
    }
}

void SecurityCamera::Initialize() {
    INFO_LOG("Initializing Security Camera Service");
    
    try {
        // Initialize camera first
        if (!camera_capture_->Initialize()) {
            ERROR_LOG("Failed to initialize camera");
            throw std::runtime_error("Failed to initialize camera");
        }
        INFO_LOG("Camera initialized successfully");
        
        // Initialize frame processor
        if (!frame_processor_->Initialize()) {
            ERROR_LOG("Failed to initialize frame processor");
            throw std::runtime_error("Failed to initialize frame processor");
        }
        INFO_LOG("Frame processor initialized successfully");
        
        // Get stream port from environment if available
        int port = stream_port_.load();
        if (GetEnvVar("STREAM_PORT", port)) {
            stream_port_.store(port);
        }
        
        // Only start threads after successful initialization
        capture_thread_ = std::thread(&SecurityCamera::CaptureLoop, this);
        processing_thread_ = std::thread(&SecurityCamera::ProcessingLoop, this);
        worker_thread_ = std::thread(&SecurityCamera::Run, this);
        
        INFO_LOG("Security Camera Service initialized successfully");
    } catch (const std::exception& e) {
        ERROR_LOG("Initialization failed: " + std::string(e.what()));
        Stop();  // Clean up any partially initialized resources
        throw;  // Re-throw the exception to notify the caller
    }
}

void SecurityCamera::Stop() {
    if (!running_) return;
    
    INFO_LOG("Stopping Security Camera Service");
    
    // Stop streaming if active
    if (streaming_) {
        StopStreaming();
    }
    
    // Set running flag to false to stop threads
    running_ = false;
    
    // Notify all waiting threads
    frame_queue_cv_.notify_all();
    command_queue_cv_.notify_all();
    
    // Wait for threads to finish
    if (capture_thread_.joinable()) {
        capture_thread_.join();
        DEBUG_LOG("Capture thread joined");
    }
    if (processing_thread_.joinable()) {
        processing_thread_.join();
        DEBUG_LOG("Processing thread joined");
    }
    if (worker_thread_.joinable()) {
        worker_thread_.join();
        DEBUG_LOG("Worker thread joined");
    }
    if (stream_server_thread_.joinable()) {
        stream_server_thread_.join();
        DEBUG_LOG("Stream server thread joined");
    }
    
    try {
        // Publish offline status
        PublishStatus("offline");
        
        // Disconnect from MQTT broker
        Disconnect();
    } catch (const std::exception& e) {
        ERROR_LOG("Error during shutdown: " + std::string(e.what()));
    }
    
    INFO_LOG("Security Camera Service stopped");
}

void SecurityCamera::Run() {
    INFO_LOG("Worker thread started");
    running_ = true;

    auto last_status_time = std::chrono::steady_clock::now();
    const auto status_interval = std::chrono::seconds(5);
    
    while (running_) {
        
        auto now = std::chrono::steady_clock::now();
        if (now - last_status_time >= status_interval) {
            try {
                PublishStatus("online");
            } catch (const std::exception& e) {
                ERROR_LOG("Exception in status update: " + std::string(e.what()));
            }
            last_status_time = now;
        }

        // Process any pending commands
        json command;
        bool has_command = false;
        
        {
            std::unique_lock<std::mutex> lock(command_queue_mutex_);
            if (!command_queue_.empty()) {
                command = command_queue_.front();
                command_queue_.pop();
                has_command = true;
            }
        }
        
        if (has_command) {
            ProcessCommand(command);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    INFO_LOG("Worker thread stopped");
}

void SecurityCamera::ProcessCommand(const json& command) {
    try {
        if (!command.contains("action")) {
            ERROR_LOG("Missing 'action' field in command");
            return;
        }

        std::string action = command["action"];
        DEBUG_LOG("Processing action: " + action);
        
        if (action == "snapshot") {
            cv::Mat frame;
            {
                std::lock_guard<std::mutex> lock(frame_queue_mutex_);
                if (!frame_queue_.empty()) {
                    frame = frame_queue_.front();
                }
            }
            
            if (!frame.empty()) {
                PublishSnapshot(frame);
            }
        }
        else if (action == "start_stream") {
            if (!streaming_) {
                if (StartStreaming()) {
                    INFO_LOG("Streaming started on port " + std::to_string(stream_port_.load()));
                } else {
                    ERROR_LOG("Failed to start streaming");
                }
            } else {
                WARN_LOG("Streaming already active");
                // Republish stream info
                PublishStreamInfo(true, stream_url_);
            }
        }
        else if (action == "stop_stream") {
            if (streaming_) {
                StopStreaming();
                INFO_LOG("Streaming stopped");
            } else {
                WARN_LOG("Streaming not active");
            }
        }
        else if (action == "get_stream_status") {
            PublishStreamInfo(streaming_, stream_url_);
        }
        else if (action == "request_token") {
            std::string token = GenerateToken();
            PublishToken(token);
            INFO_LOG("New stream token generated");
        }
        else if (action == "night_mode_on") {
            camera_capture_->SetNightMode(true);
            INFO_LOG("Night mode enabled");
        }
        else if (action == "night_mode_off") {
            camera_capture_->SetNightMode(false);
            INFO_LOG("Night mode disabled");
        }
        else if (action == "set_night_mode_threshold") {
            if (command.contains("threshold") && command["threshold"].is_number()) {
                int threshold = command["threshold"];
                camera_capture_->SetNightModeThreshold(threshold);
                INFO_LOG("Night mode threshold set to " + std::to_string(threshold));
            } else {
                ERROR_LOG("Missing or invalid 'threshold' field for set_night_mode_threshold action");
            }
        }
        else {
            ERROR_LOG("Unknown action: " + action);
        }
        
    } catch (const std::exception& e) {
        ERROR_LOG("Error processing command: " + std::string(e.what()));
    }
}

void SecurityCamera::CaptureLoop() {
    INFO_LOG("Capture thread started");
    
    while (running_) {
        try {
            // Capture frame
            cv::Mat frame = camera_capture_->CaptureFrame();
            
            if (frame.empty()) {
                WARN_LOG("Empty frame captured");
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            
            // Store latest frame for streaming
            if (streaming_) {
                std::lock_guard<std::mutex> lock(latest_frame_mutex_);
                frame.copyTo(latest_frame_);
            }
            
            // Add frame to queue for processing
            {
                std::lock_guard<std::mutex> lock(frame_queue_mutex_);
                
                // If queue is getting too large, remove oldest frames
                while (frame_queue_.size() > 5) {
                    frame_queue_.pop();
                }
                
                frame_queue_.push(frame);
            }
            
            // Notify processing thread
            frame_queue_cv_.notify_one();
            
            // Sleep to maintain desired frame rate
            std::this_thread::sleep_for(std::chrono::milliseconds(33)); // ~30 FPS
        } catch (const std::exception& e) {
            ERROR_LOG("Error in capture loop: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
    INFO_LOG("Capture thread stopped");
}

void SecurityCamera::ProcessingLoop() {
    INFO_LOG("Processing thread started");
    
    while (running_) {
        cv::Mat frame;
        {
            std::unique_lock<std::mutex> lock(frame_queue_mutex_);
            frame_queue_cv_.wait(lock, [this] { 
                return !frame_queue_.empty() || !running_; 
            });
            
            if (!running_) break;
            
            frame = frame_queue_.front();
            frame_queue_.pop();
        }
        
        if (!frame.empty()) {
            // Process frame and get detections
            auto result = frame_processor_->ProcessFrame(frame);
            
            // Publish detections
            json details;
            details["fps"] = result.fps;
            details["latency_ms"] = result.latency_ms;
            
            // Group detections by type
            int person_count = 0;
            int vehicle_count = 0;
            int animal_count = 0;
            
            for (const auto& det : result.detections) {
                if (det.class_name == "person") {
                    person_count++;
                } else if (det.class_name == "car" || det.class_name == "truck" || 
                         det.class_name == "bus" || det.class_name == "motorcycle") {
                    vehicle_count++;
                } else if (det.class_name == "dog" || det.class_name == "cat" || det.class_name == "bird") {
                    animal_count++;
                }
            }
            
            // Publish detection results
            if (!result.detections.empty()) {
                json detection_details = result.ToJson();
                detection_details["person_count"] = person_count;
                detection_details["vehicle_count"] = vehicle_count;
                detection_details["animal_count"] = animal_count;
                
                Publish(DETECTIONS_TOPIC, detection_details);
                
                // Also publish snapshot if something was detected
                PublishSnapshot(frame);
            }
        }
    }
    
    INFO_LOG("Processing thread stopped");
}

void SecurityCamera::IncomingMessage(const std::string& topic, const std::string& payload) {
    try {
        json command = json::parse(payload);
        
        if (topic == COMMAND_TOPIC) {
            std::lock_guard<std::mutex> lock(command_queue_mutex_);
            command_queue_.push(command);
            command_queue_cv_.notify_one();
        }
    } catch (const std::exception& e) {
        ERROR_LOG("Error processing command: " + std::string(e.what()));
    }
}

void SecurityCamera::PublishStatus(const std::string& status) {
    json payload;
    payload["status"] = status;
    payload["timestamp"] = std::time(nullptr);
    
    // Add night mode information
    if (camera_capture_) {
        payload["night_mode"] = camera_capture_->IsNightMode();
        payload["night_mode_threshold"] = camera_capture_->GetNightModeThreshold();
    }
    
    Publish(STATUS_TOPIC, payload);
}

void SecurityCamera::PublishSnapshot(const cv::Mat& frame) {
    // Convert frame to base64
    std::string base64_image = MatToBase64(frame);
    
    json payload;
    payload["image"] = base64_image;
    payload["timestamp"] = std::time(nullptr);

    payload["width"] = frame.cols;
    payload["height"] = frame.rows;
    
    Publish(SNAPSHOT_TOPIC, payload);
}

std::string SecurityCamera::MatToBase64(const cv::Mat& image) {
    // Compress the image with lower quality for faster processing
    std::vector<int> compression_params;
    compression_params.push_back(cv::IMWRITE_JPEG_QUALITY);
    compression_params.push_back(80); // Lower quality for faster processing
    
    std::vector<uchar> buffer;
    cv::imencode(".jpg", image, buffer, compression_params);
    
    std::string base64_image = "data:image/jpeg;base64,";
    
    // Use a more efficient base64 encoding approach
    static const char base64_chars[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    // Pre-calculate the output size to avoid reallocations
    size_t output_size = 4 * ((buffer.size() + 2) / 3) + base64_image.size();
    base64_image.reserve(output_size);
    
    size_t i = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    
    for (const uchar& byte : buffer) {
        char_array_3[i++] = byte;
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;
            
            for (i = 0; i < 4; i++) {
                base64_image += base64_chars[char_array_4[i]];
            }
            i = 0;
        }
    }
    
    if (i) {
        for (size_t j = i; j < 3; j++) {
            char_array_3[j] = '\0';
        }
        
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        
        for (size_t j = 0; j < i + 1; j++) {
            base64_image += base64_chars[char_array_4[j]];
        }
        
        while (i++ < 3) {
            base64_image += '=';
        }
    }
    
    return base64_image;
}

bool SecurityCamera::GetEnvVar(const std::string& name, std::string& value) {
    const char* env_value = std::getenv(name.c_str());
    if (env_value) {
        value = std::string(env_value);
        return true;
    }
    return false;
}

bool SecurityCamera::GetEnvVar(const std::string& name, int& value) {
    std::string str_value;
    if (GetEnvVar(name, str_value)) {
        try {
            value = std::stoi(str_value);
            return true;
        } catch (const std::exception& e) {
            ERROR_LOG("Error converting environment variable " + name + " to int: " + e.what());
        }
    }
    return false;
}

void SecurityCamera::PublishStreamInfo(bool streaming, const std::string& url) {
    json payload;
    payload["streaming"] = streaming;
    if (streaming && !url.empty()) {
        payload["url"] = url;
        payload["requires_token"] = true;
    }
    payload["timestamp"] = std::time(nullptr);
    
    Publish(STREAM_TOPIC, payload);
}

void SecurityCamera::PublishToken(const std::string& token) {
    json payload;
    payload["token"] = token;
    payload["expires"] = std::time(nullptr) + TOKEN_EXPIRATION_TIME;
    
    Publish(TOKEN_TOPIC, payload);
}

bool SecurityCamera::StartStreaming() {
    if (streaming_) {
        return true; // Already streaming
    }
    
    try {
        // Set flag before starting thread
        streaming_ = true;
        
        // Create stream server thread
        stream_server_thread_ = std::thread(&SecurityCamera::StreamServerLoop, this);
        
        // Wait a bit for the server to start
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Get local IP address
        std::string host_ip = "localhost"; // Default fallback
        
        // Try to get the actual IP from environment
        std::string env_ip;
        if (GetEnvVar("HOST_IP", env_ip) && !env_ip.empty()) {
            host_ip = env_ip;
        }
        
        // Construct stream URL
        int port = stream_port_.load();
        std::string protocol = use_https_ ? "https" : "http";
        stream_url_ = protocol + "://" + host_ip + ":" + std::to_string(port) + "/stream?token=TOKEN";
        
        // Publish stream info
        PublishStreamInfo(true, stream_url_);
        
        return true;
    } catch (const std::exception& e) {
        ERROR_LOG("Error starting stream: " + std::string(e.what()));
        streaming_ = false;
        return false;
    }
}

void SecurityCamera::StopStreaming() {
    if (!streaming_) {
        return; // Not streaming
    }
    
    // Set streaming flag to false first to signal all threads to stop
    streaming_ = false;
    
    // Give client threads a moment to notice the streaming flag change
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Close all client connections
    {
        std::lock_guard<std::mutex> lock(stream_clients_mutex_);
        for (const auto& client : stream_clients_) {
            try {
                if (use_https_ && client.ssl) {
                    SSL_shutdown(client.ssl);
                    SSL_free(client.ssl);
                }
                close(client.socket);
            } catch (const std::exception& e) {
                ERROR_LOG("Error closing client connection: " + std::string(e.what()));
            }
        }
        stream_clients_.clear();
    }
    
    // Wait for stream server thread to finish
    if (stream_server_thread_.joinable()) {
        try {
            stream_server_thread_.join();
        } catch (const std::exception& e) {
            ERROR_LOG("Error joining stream server thread: " + std::string(e.what()));
        }
    }
    
    // Publish stream info
    PublishStreamInfo(false);
}

void SecurityCamera::StreamServerLoop() {
    int port = stream_port_.load();
    INFO_LOG("Starting " + std::string(use_https_ ? "HTTPS" : "HTTP") + " stream server on port " + std::to_string(port));
    
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        ERROR_LOG("Failed to create socket");
        streaming_ = false;
        return;
    }
    
    // Set socket options to allow reuse of address
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        ERROR_LOG("Failed to set socket options");
        close(server_socket);
        streaming_ = false;
        return;
    }
    
    // Set up server address
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    // Bind socket
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        ERROR_LOG("Failed to bind socket");
        close(server_socket);
        streaming_ = false;
        return;
    }
    
    // Listen for connections
    if (listen(server_socket, 5) < 0) {
        ERROR_LOG("Failed to listen on socket");
        close(server_socket);
        streaming_ = false;
        return;
    }
    
    // Set socket to non-blocking mode
    int flags = fcntl(server_socket, F_GETFL, 0);
    fcntl(server_socket, F_SETFL, flags | O_NONBLOCK);
    
    INFO_LOG("MJPEG stream server started");
    
    // Accept connections and handle clients
    while (streaming_ && running_) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        
        if (client_socket >= 0) {
            // Got a new client
            INFO_LOG("New streaming client connected: " + 
                     std::string(inet_ntoa(client_addr.sin_addr)) + ":" + 
                     std::to_string(ntohs(client_addr.sin_port)));
            
            // Handle client in a separate thread
            std::thread client_thread(&SecurityCamera::HandleStreamClient, this, client_socket);
            client_thread.detach(); // Let it run independently
        }
        
        // Clean up expired tokens periodically
        CleanupExpiredTokens();
        
        // Sleep to avoid busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Close server socket
    close(server_socket);
    
    INFO_LOG("MJPEG stream server stopped");
}

void SecurityCamera::HandleStreamClient(int client_socket) {
    SSL* ssl = nullptr;
    bool client_added_to_list = false;
    
    try {
        // Set up SSL if HTTPS is enabled
        if (use_https_ && ssl_ctx_) {
            ssl = SSL_new(ssl_ctx_);
            if (!ssl) {
                throw std::runtime_error("Failed to create SSL structure");
            }
            
            SSL_set_fd(ssl, client_socket);
            
            if (SSL_accept(ssl) <= 0) {
                throw std::runtime_error("SSL handshake failed");
            }
            
            INFO_LOG("SSL connection established");
        }
        
        // Read HTTP request
        char buffer[4096] = {0};
        int bytes_read;
        
        if (use_https_ && ssl) {
            bytes_read = SSL_read(ssl, buffer, sizeof(buffer) - 1);
        } else {
            bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        }
        
        if (bytes_read <= 0) {
            throw std::runtime_error("Failed to read HTTP request");
        }
        
        buffer[bytes_read] = '\0';
        std::string request(buffer);
        
        // Parse HTTP request
        std::map<std::string, std::string> headers;
        std::string path;
        if (!ParseHttpRequest(request, headers, path)) {
            throw std::runtime_error("Invalid HTTP request");
        }
        
        // Extract token from query string
        std::string token;
        size_t token_pos = path.find("token=");
        if (token_pos != std::string::npos) {
            token = path.substr(token_pos + 6);
            size_t end_pos = token.find('&');
            if (end_pos != std::string::npos) {
                token = token.substr(0, end_pos);
            }
        }
        
        // Validate token
        if (!ValidateToken(token)) {
            // Send 401 Unauthorized
            std::string response = "HTTP/1.1 401 Unauthorized\r\n"
                                  "Content-Type: text/plain\r\n"
                                  "Connection: close\r\n\r\n"
                                  "Invalid or expired token";
            
            if (use_https_ && ssl) {
                SSL_write(ssl, response.c_str(), response.size());
            } else {
                send(client_socket, response.c_str(), response.size(), 0);
            }
            
            throw std::runtime_error("Invalid token");
        }
        
        // Add client to list
        {
            std::lock_guard<std::mutex> lock(stream_clients_mutex_);
            stream_clients_.push_back({client_socket, ssl});
            client_added_to_list = true;
        }
        
        // Send HTTP response header
        std::string header = "HTTP/1.1 200 OK\r\n"
                            "Connection: close\r\n"
                            "Cache-Control: no-cache\r\n"
                            "Pragma: no-cache\r\n"
                            "Content-Type: multipart/x-mixed-replace; boundary=mjpegstream\r\n\r\n";
        
        if (use_https_ && ssl) {
            if (SSL_write(ssl, header.c_str(), header.size()) < 0) {
                throw std::runtime_error("Failed to send HTTP header");
            }
        } else {
            if (send(client_socket, header.c_str(), header.size(), 0) < 0) {
                throw std::runtime_error("Failed to send HTTP header");
            }
        }
        
        // Stream frames until client disconnects or streaming stops
        while (streaming_ && running_) {
            // Get latest frame
            cv::Mat frame;
            {
                std::lock_guard<std::mutex> lock(latest_frame_mutex_);
                if (!latest_frame_.empty()) {
                    frame = latest_frame_.clone();
                }
            }
            
            if (!frame.empty()) {
                try {
                    // Send frame to client
                    SendMJPEGFrame(ssl, client_socket, frame);
                } catch (const std::exception& e) {
                    throw std::runtime_error("Error sending MJPEG frame: " + std::string(e.what()));
                }
            }
            
            // Sleep to maintain desired frame rate
            std::this_thread::sleep_for(std::chrono::milliseconds(33)); // ~30 FPS
        }
    } catch (const std::exception& e) {
        DEBUG_LOG("Client disconnected: " + std::string(e.what()));
    }
    
    // Remove client from list if it was added
    if (client_added_to_list) {
        std::lock_guard<std::mutex> lock(stream_clients_mutex_);
        auto it = std::find_if(stream_clients_.begin(), stream_clients_.end(),
                              [client_socket](const ClientInfo& info) { return info.socket == client_socket; });
        if (it != stream_clients_.end()) {
            stream_clients_.erase(it);
        }
    }
    
    // Clean up SSL
    if (use_https_ && ssl) {
        try {
            SSL_shutdown(ssl);
            SSL_free(ssl);
        } catch (...) {
            // Ignore errors during cleanup
        }
    }
    
    // Close client socket
    try {
        close(client_socket);
    } catch (...) {
        // Ignore errors during cleanup
    }
}

void SecurityCamera::SendMJPEGFrame(SSL* ssl, int client_socket, const cv::Mat& frame) {
    try {
        // Compress frame to JPEG
        std::vector<uchar> buffer;
        std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, 80}; // 80% quality
        cv::imencode(".jpg", frame, buffer, params);
        
        // Create MJPEG frame header
        std::stringstream header;
        header << "--mjpegstream\r\n"
               << "Content-Type: image/jpeg\r\n"
               << "Content-Length: " << buffer.size() << "\r\n\r\n";
        
        std::string header_str = header.str();
        
        // Send header
        int result = 0;
        if (use_https_ && ssl) {
            result = SSL_write(ssl, header_str.c_str(), header_str.size());
            if (result < 0) {
                throw std::runtime_error("Failed to send frame header");
            }
            
            // Send image data
            result = SSL_write(ssl, buffer.data(), buffer.size());
            if (result < 0) {
                throw std::runtime_error("Failed to send frame data");
            }
            
            // Send boundary
            std::string boundary = "\r\n";
            result = SSL_write(ssl, boundary.c_str(), boundary.size());
            if (result < 0) {
                throw std::runtime_error("Failed to send frame boundary");
            }
        } else {
            result = send(client_socket, header_str.c_str(), header_str.size(), 0);
            if (result < 0) {
                throw std::runtime_error("Failed to send frame header");
            }
            
            // Send image data
            result = send(client_socket, buffer.data(), buffer.size(), 0);
            if (result < 0) {
                throw std::runtime_error("Failed to send frame data");
            }
            
            // Send boundary
            std::string boundary = "\r\n";
            result = send(client_socket, boundary.c_str(), boundary.size(), 0);
            if (result < 0) {
                throw std::runtime_error("Failed to send frame boundary");
            }
        }
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Error sending MJPEG frame: ") + e.what());
    }
}

std::string SecurityCamera::GenerateToken() {
    // Generate a random token
    const std::string chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<> distribution(0, chars.size() - 1);
    
    std::string token;
    token.reserve(32);
    for (int i = 0; i < 32; ++i) {
        token += chars[distribution(generator)];
    }
    
    // Store token with expiration time
    {
        std::lock_guard<std::mutex> lock(tokens_mutex_);
        valid_tokens_[token] = std::time(nullptr) + TOKEN_EXPIRATION_TIME;
    }
    
    return token;
}

bool SecurityCamera::ValidateToken(const std::string& token) {
    if (token.empty()) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(tokens_mutex_);
    auto it = valid_tokens_.find(token);
    if (it == valid_tokens_.end()) {
        return false;
    }
    
    // Check if token has expired
    time_t now = std::time(nullptr);
    if (it->second < now) {
        valid_tokens_.erase(it);
        return false;
    }
    
    return true;
}

void SecurityCamera::CleanupExpiredTokens() {
    std::lock_guard<std::mutex> lock(tokens_mutex_);
    time_t now = std::time(nullptr);
    
    for (auto it = valid_tokens_.begin(); it != valid_tokens_.end();) {
        if (it->second < now) {
            it = valid_tokens_.erase(it);
        } else {
            ++it;
        }
    }
}

bool SecurityCamera::ParseHttpRequest(const std::string& request, std::map<std::string, std::string>& headers, std::string& path) {
    std::istringstream stream(request);
    std::string line;
    
    // Parse request line
    if (!std::getline(stream, line)) {
        return false;
    }
    
    std::istringstream request_line(line);
    std::string method, http_version;
    
    if (!(request_line >> method >> path >> http_version)) {
        return false;
    }
    
    // Parse headers
    while (std::getline(stream, line) && line != "\r") {
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string name = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 1);
            
            // Trim whitespace
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of("\r\n") + 1);
            
            headers[name] = value;
        }
    }
    
    return true;
}

bool SecurityCamera::InitializeSSL() {
    // Initialize OpenSSL
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
    
    // Create SSL context
    ssl_ctx_ = SSL_CTX_new(TLS_server_method());
    if (!ssl_ctx_) {
        ERROR_LOG("Failed to create SSL context");
        return false;
    }
    
    // Set up certificate and private key
    if (cert_file_.empty() || key_file_.empty()) {
        ERROR_LOG("SSL certificate or key file path not specified");
        SSL_CTX_free(ssl_ctx_);
        ssl_ctx_ = nullptr;
        return false;
    }
    
    if (SSL_CTX_use_certificate_file(ssl_ctx_, cert_file_.c_str(), SSL_FILETYPE_PEM) <= 0) {
        ERROR_LOG("Failed to load SSL certificate");
        SSL_CTX_free(ssl_ctx_);
        ssl_ctx_ = nullptr;
        return false;
    }
    
    if (SSL_CTX_use_PrivateKey_file(ssl_ctx_, key_file_.c_str(), SSL_FILETYPE_PEM) <= 0) {
        ERROR_LOG("Failed to load SSL private key");
        SSL_CTX_free(ssl_ctx_);
        ssl_ctx_ = nullptr;
        return false;
    }
    
    // Verify private key
    if (!SSL_CTX_check_private_key(ssl_ctx_)) {
        ERROR_LOG("SSL private key does not match the certificate");
        SSL_CTX_free(ssl_ctx_);
        ssl_ctx_ = nullptr;
        return false;
    }
    
    INFO_LOG("SSL initialized successfully");
    return true;
}

void SecurityCamera::CleanupSSL() {
    if (ssl_ctx_) {
        SSL_CTX_free(ssl_ctx_);
        ssl_ctx_ = nullptr;
    }
    
    EVP_cleanup();
    ERR_free_strings();
}

bool SecurityCamera::GetEnvVar(const std::string& name, bool& value) {
    std::string str_value;
    if (GetEnvVar(name, str_value)) {
        std::transform(str_value.begin(), str_value.end(), str_value.begin(), ::tolower);
        if (str_value == "true" || str_value == "1" || str_value == "yes") {
            value = true;
            return true;
        } else if (str_value == "false" || str_value == "0" || str_value == "no") {
            value = false;
            return true;
        }
    }
    return false;
} 