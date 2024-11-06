  
# Thread Affinity
Integrate MQTT threads with the main thread (mainly for led_manager has it has thread affinity to it's devices)

This is called "thread affinity" or "thread confinement" - when a hardware resource or device can only be accessed from a specific thread. This is common in hardware programming where the device driver or hardware interface must be accessed from the same thread that initialized it to maintain consistency and avoid race conditions.

