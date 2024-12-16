find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK "${CCACHE_PROGRAM}")
    message(STATUS "Using ccache: ${CCACHE_PROGRAM}")
endif()

cmake_minimum_required(VERSION 3.10)
project(core_service)

include(ExternalProject)

set(SERVICES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(COMMON_DIR ${SERVICES_DIR}/common)
set(INTERFACES_DIR ${SERVICES_DIR}/interfaces)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add build type configuration
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build" FORCE)
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")

# Modify compiler flags to be configuration-specific
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(ALSA REQUIRED alsa)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json REQUIRED)

# Set up external dependencies installation prefix
set(EXTERNAL_INSTALL_LOCATION ${CMAKE_BINARY_DIR}/external)

# PocketSphinx
ExternalProject_Add(pocketsphinx_external
    GIT_REPOSITORY https://github.com/cmusphinx/pocketsphinx.git
    GIT_TAG master
    GIT_SHALLOW ON
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${EXTERNAL_INSTALL_LOCATION}
    BUILD_COMMAND
        ${CMAKE_COMMAND} --build <BINARY_DIR>
    INSTALL_COMMAND
        ${CMAKE_COMMAND} --build <BINARY_DIR> --target install
)

# Paho MQTT C++
ExternalProject_Add(paho-mqtt-cpp
    GIT_REPOSITORY https://github.com/eclipse/paho.mqtt.cpp.git
    GIT_TAG v1.4.0
    GIT_SHALLOW ON
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

# Porcupine
ExternalProject_Add(porcupine
    GIT_REPOSITORY https://github.com/Picovoice/porcupine.git
    GIT_TAG v3.0
    GIT_SHALLOW ON
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND
        # Copy Headers
        ${CMAKE_COMMAND} -E make_directory ${EXTERNAL_INSTALL_LOCATION}/include
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            <SOURCE_DIR>/include/
            ${EXTERNAL_INSTALL_LOCATION}/include/porcupine
        # Copy Library
        COMMAND ${CMAKE_COMMAND} -E copy
            <SOURCE_DIR>/lib/raspberry-pi/cortex-a53-aarch64/libpv_porcupine.so
            ${EXTERNAL_INSTALL_LOCATION}/lib/
        # Copy Base Model
        COMMAND ${CMAKE_COMMAND} -E copy
            <SOURCE_DIR>/lib/common/porcupine_params.pv
            ${EXTERNAL_INSTALL_LOCATION}/lib/
        # Copy Keyword file
        COMMAND ${CMAKE_COMMAND} -E copy
            <SOURCE_DIR>/resources/keyword_files/raspberry-pi/jarvis_raspberry-pi.ppn
            ${EXTERNAL_INSTALL_LOCATION}/lib/
)

# Rhino
ExternalProject_Add(rhino
    GIT_REPOSITORY https://github.com/Picovoice/rhino.git
    GIT_TAG v3.0
    GIT_SHALLOW ON
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND
        # Copy Headers
        ${CMAKE_COMMAND} -E make_directory ${EXTERNAL_INSTALL_LOCATION}/include
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            <SOURCE_DIR>/include/
            ${EXTERNAL_INSTALL_LOCATION}/include/rhino
        # Copy Library
        COMMAND ${CMAKE_COMMAND} -E copy
            <SOURCE_DIR>/lib/raspberry-pi/cortex-a53-aarch64/libpv_rhino.so
            ${EXTERNAL_INSTALL_LOCATION}/lib/
        # Copy Base Model
        COMMAND ${CMAKE_COMMAND} -E copy
            <SOURCE_DIR>/lib/common/rhino_params.pv
            ${EXTERNAL_INSTALL_LOCATION}/lib/
        # Copy Context file from local repo
        COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/lib/Smart-Home_en_raspberry-pi_v3_0_0.rhn
            ${EXTERNAL_INSTALL_LOCATION}/lib/
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
    pocketsphinx_external
    paho-mqtt-cpp
    porcupine
    rhino
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
        ${EXTERNAL_INSTALL_LOCATION}/include/pocketsphinx
        ${ALSA_INCLUDE_DIRS}
        ${EXTERNAL_INSTALL_LOCATION}/include/porcupine
        ${EXTERNAL_INSTALL_LOCATION}/include/rhino
)

# Link libraries
target_link_directories(${PROJECT_NAME}
    PRIVATE
        ${EXTERNAL_INSTALL_LOCATION}/lib
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        pocketsphinx
        paho-mqttpp3
        paho-mqtt3as
        ${ALSA_LIBRARIES}
        OpenSSL::SSL
        OpenSSL::Crypto
        nlohmann_json::nlohmann_json
        pthread
        ${EXTERNAL_INSTALL_LOCATION}/lib/libpv_porcupine.so
        ${EXTERNAL_INSTALL_LOCATION}/lib/libpv_rhino.so
)

# Installation rules
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)