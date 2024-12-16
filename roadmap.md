
# TODO
- [ ] Intent Classification Model (Rhino)
- [ ] Experiment with TFLite
- [ ] Setup communication with remote LLM (Agentic)
- [ ] Setup Grafana for monitoring & logging
- [ ] Control LEDs from web interface
- [X] Maybe new build system?
- [X] Remove multiple SSH connections in service.rs
- [X] Solve LED re-connection issues
- [ ] Add ability to copy files to remote device (service generic)


# MQTT Design
Services - home/services/{service_name}
  * Status - home/services/{service_name}/status
  * Commands - home/services/{service_name}/command

Devices - home/devices/{device_name} (no devices currently)
  * Status - home/devices/{device_name}/status
  * Commands - home/devices/{device_name}/command


# Ideas
Enhanced Voice Processing & Natural Language Understanding
  Replace the simple keyword detection with a more sophisticated wake word system (like Picovoice or a custom PyTorch model)
  Add full Natural Language Understanding to process complex commands
  Implement voice activity detection (VAD) to better segment commands
  Train a custom wake word model for a unique activation phrase

Computer Vision Integration
  Use cases:
  Presence detection for automated lighting
  Gesture control for lights
  Object detection for home inventory
  Face recognition for personalized automations
  Activity recognition for smart automation triggers

Intelligent Automation & Learning
  Implement a reinforcement learning agent that learns user preferences over time
  Create predictive models for lighting patterns based on time, presence, and activities
  Use anomaly detection for unusual patterns (security)
  Implement transfer learning to adapt pre-trained models to your specific use case

Multi-Modal AI System
 Combine audio and visual inputs for better context understanding
 Use sensor fusion for more accurate presence detection
 Create a more natural and context-aware interaction system

Edge AI Implementation
 Deploy TensorFlow Lite or ONNX Runtime models on the Raspberry Pi
 Implement model quantization for better performance
 Create a pipeline for regular model updates
 Balance between edge and cloud processing

Advanced Project Ideas
 Emotion-Aware Lighting: Use audio tone and facial expression analysis to adjust lighting
 Activity-Based Automation: Learn and predict daily routines
 Smart Energy Management: Use ML to optimize power consumption
 Contextual Scene Understanding: Combine multiple inputs to understand room context