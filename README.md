# AI Home Assistant

A distributed IoT system implementing a locally-processed voice assistant using MQTT for service communication and Picovoice models for voice processing.

## Overview
This project demonstrates a microservices architecture for home automation, featuring local voice processing and distributed service management.

## Architecture Components

### Voice Processing Pipeline
- Wake word detection using Picovoice
- Audio processing and intent classification
- Service routing based on intent classification

### Service Manager
Distributed service orchestrator implementing:
- Service lifecycle management and deployment via systemd
- Health monitoring and status reporting
- Configuration management and distribution
- Remote management interface via MQTT

### Core Services
- **Core Service**: Handles audio pipeline, wake word detection, and intent classification
- **LED Manager**: BLE-based LED control service
- more to come...

## Technical Stack
- Rust and Docker for Service Manager
- C++ for services
- MQTT for service communication
- SimpleBLE for device interactions
- Picovoice models for voice processing