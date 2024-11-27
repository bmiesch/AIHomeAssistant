cmake_minimum_required(VERSION 3.10)
project(led_manager_service)

include(ExternalProject)

set(SERVICES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(COMMON_DIR ${SERVICES_DIR}/common)
set(INTERFACES_DIR ${SERVICES_DIR}/interfaces)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable debug symbols and disable optimization (matching your Makefile)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -g -O0")

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(DBUS REQUIRED dbus-1)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json REQUIRED)

# Set up external dependencies installation prefix
set(EXTERNAL_INSTALL_LOCATION ${CMAKE_BINARY_DIR}/external)

# SimpleBLE
ExternalProject_Add(simpleble_external
    GIT_REPOSITORY https://github.com/OpenBluetoothToolbox/SimpleBLE.git
    GIT_TAG main
    SOURCE_SUBDIR simpleble
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${EXTERNAL_INSTALL_LOCATION}
        -DCMAKE_BUILD_TYPE=Release
    BUILD_COMMAND
        ${CMAKE_COMMAND} --build <BINARY_DIR> --config Release --target all
    INSTALL_COMMAND
        ${CMAKE_COMMAND} --install <BINARY_DIR> --config Release
)

# Paho MQTT C++
ExternalProject_Add(paho-mqtt-cpp
    GIT_REPOSITORY https://github.com/eclipse/paho.mqtt.cpp.git
    GIT_TAG v1.4.0
    UPDATE_COMMAND
        git submodule init && git submodule update
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${EXTERNAL_INSTALL_LOCATION}
        -DPAHO_WITH_MQTT_C=ON
        -DPAHO_BUILD_EXAMPLES=ON
        -DPAHO_WITH_SSL=ON
    BUILD_COMMAND
        ${CMAKE_COMMAND} --build <BINARY_DIR>
    INSTALL_COMMAND
        ${CMAKE_COMMAND} --build <BINARY_DIR> --target install
)

# Add the external install location to the CMAKE_PREFIX_PATH
list(APPEND CMAKE_PREFIX_PATH ${EXTERNAL_INSTALL_LOCATION})

# Collect all source files
file(GLOB_RECURSE SOURCES
    src/*.cpp
    ${COMMON_DIR}/src/*.cpp
    ${INTERFACES_DIR}/mqtt_interface/*.cpp
    ${INTERFACES_DIR}/service_interface/*.cpp
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Add dependencies on external projects
add_dependencies(${PROJECT_NAME}
    simpleble_external
    paho-mqtt-cpp
)

# Add include directories
target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/inc
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${COMMON_DIR}/inc
        ${INTERFACES_DIR}/mqtt_interface
        ${INTERFACES_DIR}/service_interface
        ${EXTERNAL_INSTALL_LOCATION}/include
        ${DBUS_INCLUDE_DIRS}
)

# Link libraries
target_link_directories(${PROJECT_NAME}
    PRIVATE
        ${EXTERNAL_INSTALL_LOCATION}/lib
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        simpleble
        paho-mqttpp3
        paho-mqtt3as
        OpenSSL::SSL
        OpenSSL::Crypto
        nlohmann_json::nlohmann_json
        pthread
        ${DBUS_LIBRARIES}
)

# Installation rules
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

# Optional: Enable ccache if available
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()