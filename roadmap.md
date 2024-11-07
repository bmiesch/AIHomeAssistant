
# TODO
- [ ] Implement command parser and executor w/ PocketSphinx
- [ ] Setup communication with remote LLM
- [ ] Design a common interface for all devices/services
- [ ] What can I use the LLM for?
- [ ] Develop simple web app for control/admin
- [ ] Use systemd to run and sync the modules
- [ ] Maybe new build system?


# DONE
- [X] Get the bluetooth communication working
- [X] Get the audio capture working
- [X] Get the keyword detector working


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