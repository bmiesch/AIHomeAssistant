
# TODO
- [ ] Setup communication with remote LLM
- [ ] Design a common interface for all devices/services
- [ ] What can I use the LLM for?
- [ ] Add MQTT client to control LEDs from web interface
- [X] Maybe new build system?
- [ ] Stream logs from remote devices to web interface
- [X] Remove multiple SSH connections in service.rs
- [X] Solve LED re-connection issues

# Command Parser and Executor (PocketSphinx)
* Simple ON/OFF and color change commands working via bluetooth
* How do I run this process constantly w/ low latency? 
  * Design a runtime and scheduling strategy


# Random
MQTT
docker compose for deployment
GO - service manager
rust - web app?
redis


# Next
Service Manager - Rust - controlled by the web interface
  * Deploy the services
    * Install dependencies on remote device
    * Cross-compile from mono repo, copy to remote server with rsync, create systemd service
    * Redeploy is as easy as doing the above steps again
  * Start Services - systemd
  * Stop Services - systemd
  * Monitor Services - MQTT Service

Web Interface - JavaScript/React with API - Rust/WASM Full Stack
  * Monitoring and Logging
  * Control Services via MQTT


# MQTT Routing
Services - home/services/{service_name}
  * Status - home/services/{service_name}/status
  * Commands - home/services/{service_name}/command

Devices - home/devices/{device_name} (no devices currently)
  * Status - home/devices/{device_name}/status
  * Commands - home/devices/{device_name}/command