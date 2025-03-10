# AI Home Assistant

A distributed MQTT-based IoT system controlled via a locally-processed voice assistant that uses wake words and intent classification. A Service Manager is used to cross-compile, deploy, and manage services on remote devices.

## Architecture Components

### Voice Processing Pipeline
- ALSA audio capture
- Wake word detection with Picovoice Porcupine ("Jarvis") to trigger intent classification
- Intent classification with Picovoice Rhino -> Command
- Commands routed to appropriate services

### Service Manager / Orchestrator
- Cross-compile services for different architectures with Docker
- Deploy services to remote devices and manage them with systemd
- Health monitoring and status reporting
- MQTT client for direct service control

### Core Services
- **Core Service**: Handles audio pipeline, wake word detection, and intent classification
- **LED Manager**: BLE-based LED control service
- **Security Camera**: ML-based security camera with vehicle, person, and animal detection - Streams video to web UI
- more to come...

## Technical Stack
- Rust and Docker for Service Manager
- C++ for services
- MQTT for service communication
- SimpleBLE for device interactions
- Picovoice models for voice processing
- OpenCV and YOLO for image processing