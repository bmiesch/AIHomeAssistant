find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK "${CCACHE_PROGRAM}")
    message(STATUS "Using ccache: ${CCACHE_PROGRAM}")
endif()

cmake_minimum_required(VERSION 3.13)
project(security_camera_service)

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
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build" FORCE)
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")

# Modify compiler flags to be configuration-specific
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")

# Find required packages
find_package(PkgConfig REQUIRED)
find_package(OpenSSL REQUIRED)

# Set up external dependencies installation prefix
set(EXTERNAL_INSTALL_LOCATION ${CMAKE_BINARY_DIR}/external)

# OpenCV
ExternalProject_Add(opencv_build
    GIT_REPOSITORY https://github.com/opencv/opencv.git
    GIT_TAG 4.1.1
    GIT_SHALLOW ON
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${EXTERNAL_INSTALL_LOCATION}
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_CXX_STANDARD=17
        -DCMAKE_SYSTEM_NAME=Linux
        -DCMAKE_SYSTEM_PROCESSOR=aarch64
        -DCPU_BASELINE=NEON
        -DENABLE_NEON=ON
        -DENABLE_VFPV3=OFF
        -DBUILD_TESTS=OFF
        -DBUILD_PERF_TESTS=OFF
        -DBUILD_EXAMPLES=OFF
        -DBUILD_JAVA=OFF
        -DBUILD_opencv_python2=OFF
        -DBUILD_opencv_python3=OFF
        -DWITH_CUDA=OFF
        -DWITH_OPENCL=OFF
        -DWITH_EIGEN=ON
        -DWITH_OPENMP=ON
        -DWITH_IPP=OFF
        -DWITH_TBB=OFF
        -DWITH_FFMPEG=OFF
        -DBUILD_LIST=core,imgproc,imgcodecs,videoio,highgui,video,features2d,dnn
    BUILD_COMMAND
        ${CMAKE_COMMAND} --build <BINARY_DIR>
    INSTALL_COMMAND
        ${CMAKE_COMMAND} --build <BINARY_DIR> --target install
)

# nlohmann-json
ExternalProject_Add(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW ON
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${EXTERNAL_INSTALL_LOCATION}
        -DJSON_BuildTests=OFF
        -DJSON_MultipleHeaders=OFF
        -DCMAKE_CXX_STANDARD=17
        -DCMAKE_CXX_STANDARD_REQUIRED=ON
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

# Collect source files
file(GLOB SOURCES
    src/*.cpp
    ${COMMON_DIR}/src/*.cpp
    ${INTERFACES_DIR}/mqtt_interface/*.cpp
    ${INTERFACES_DIR}/service_interface/*.cpp
)

# Create executable with explicit sources
add_executable(${PROJECT_NAME} ${SOURCES})

# Add dependencies on external projects
add_dependencies(${PROJECT_NAME}
    opencv_build
    paho-mqtt-cpp
    json
)

# Add include directories
target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/inc
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${COMMON_DIR}/inc
        ${INTERFACES_DIR}/service_interface
        ${INTERFACES_DIR}/mqtt_interface
        ${EXTERNAL_INSTALL_LOCATION}/include
        ${EXTERNAL_INSTALL_LOCATION}/include/opencv4
)

# Link libraries
target_link_directories(${PROJECT_NAME}
    PRIVATE
        ${EXTERNAL_INSTALL_LOCATION}/lib
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        opencv_core
        opencv_imgproc
        opencv_imgcodecs
        opencv_videoio
        opencv_highgui
        opencv_video
        opencv_features2d
        opencv_dnn
        paho-mqtt3as
        paho-mqttpp3
        OpenSSL::SSL
        OpenSSL::Crypto
        pthread
)

# Installation rules
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

# Install OpenCV libraries
install(DIRECTORY ${EXTERNAL_INSTALL_LOCATION}/lib/
    DESTINATION lib/${PROJECT_NAME}
    FILES_MATCHING PATTERN "libopencv_*"
)

# Install Paho libraries
install(DIRECTORY ${EXTERNAL_INSTALL_LOCATION}/lib/
    DESTINATION lib/${PROJECT_NAME}
    FILES_MATCHING PATTERN "libpaho*"
)
