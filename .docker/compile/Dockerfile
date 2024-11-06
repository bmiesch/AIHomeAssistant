# Stage 1: Build dependencies
FROM arm64v8/debian:bookworm

# Install dependencies
RUN apt-get update && apt-get install -y \
    crossbuild-essential-arm64 \
    ccache \
    g++ \
    make \
    libasound2-dev \
    libdbus-1-dev \
    git \
    cmake \
    libssl-dev \
    nlohmann-json3-dev \
    mosquitto \
    mosquitto-clients \
    && rm -rf /var/lib/apt/lists/*

# Build PocketSphinx from source
WORKDIR /deps/pocketsphinx
RUN git clone https://github.com/cmusphinx/pocketsphinx.git . && \
    cmake -S . -B build && \
    cmake --build build && \
    cmake --build build --target install

# Build SimpleBLE from source
WORKDIR /deps/SimpleBLE
RUN git clone https://github.com/OpenBluetoothToolbox/SimpleBLE.git . && \
    cd simpleble && \
    cmake -Bbuild -H. -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --config Release --target all -v && \
    cmake --install build --config Release

# Build paho-mqtt-cpp from source
WORKDIR /deps/paho.mqtt.cpp
RUN git clone https://github.com/eclipse/paho.mqtt.cpp.git . && \
    git checkout v1.4.0 && \
    git submodule init && \
    git submodule update && \
    cmake -Bbuild -H. -DPAHO_WITH_MQTT_C=ON -DPAHO_BUILD_EXAMPLES=ON -DPAHO_WITH_SSL=ON && \
    cmake --build build/ --target install && \
    ldconfig

# Set up working directory for the application
WORKDIR /app

# Copy Makefile to build directory
RUN mkdir -p /app/build
COPY Makefile /app/build/
