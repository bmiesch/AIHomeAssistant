# Use the latest Debian arm64 image as base
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
WORKDIR /
RUN git clone https://github.com/cmusphinx/pocketsphinx.git
WORKDIR /pocketsphinx
RUN cmake -S . -B build
RUN cmake --build build
RUN cmake --build build --target install

# Build SimpleBLE from source
WORKDIR /
RUN git clone https://github.com/OpenBluetoothToolbox/SimpleBLE.git
WORKDIR /SimpleBLE/simpleble
RUN cmake -Bbuild -H. -DCMAKE_BUILD_TYPE=Release
RUN cmake --build build --config Release --target all -v
RUN cmake --install build --config Release

# Build paho-mqtt-cpp from source
WORKDIR /
RUN git clone https://github.com/eclipse/paho.mqtt.cpp.git
WORKDIR /paho.mqtt.cpp
RUN git checkout v1.4.0
RUN git submodule init
RUN git submodule update
RUN cmake -Bbuild -H. -DPAHO_WITH_MQTT_C=ON -DPAHO_BUILD_EXAMPLES=ON -DPAHO_WITH_SSL=ON
RUN cmake --build build/ --target install
RUN ldconfig

# Set up working directory
WORKDIR /app

# Copy your source files
COPY src/ /app/src/
COPY inc/ /app/inc/
COPY Makefile /app/

# Set up ccache
ENV CCACHE_DIR=/ccache
RUN mkdir -p /ccache

# Set compiler to use ccache
ENV CROSS_COMPILE aarch64-linux-gnu-
ENV CXX="ccache g++"

# Compile the project
RUN make CC=${CROSS_COMPILE}gcc CXX=${CROSS_COMPILE}g++

# # Set the default command to run your application
# CMD ["./start"]