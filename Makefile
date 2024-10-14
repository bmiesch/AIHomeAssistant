# Compiler and flags
CXX = g++
CXXFLAGS = -Wall -Wextra -pedantic -std=c++17 -g -O0

# The name of the output binary
TARGET = start

# Source files
SRCS = src/main.cpp src/manager.cpp src/device.cpp
OBJS = $(SRCS:.cpp=.o)

# Header files directory
INCLUDES = -Iinc -Isrc

# SimpleBLE library flags
SIMPLEBLE_INCLUDE = -I/usr/local/include
SIMPLEBLE_LIB = -lsimpleble

# D-Bus library flags
DBUS_LIB = -ldbus-1

# ASLA library flags
ASLA_INCLUDE = -I/usr/include
ASLA_LIB = -lasound

# PocketSphinx library flags
POCKETSPHINX_INCLUDE = -I/usr/local/include/pocketsphinx
POCKETSPHINX_LIB = -lpocketsphinx

# Flags for SimpleBLE dependencies
CXXFLAGS += $(SIMPLEBLE_INCLUDE) $(ASLA_INCLUDE) $(POCKETSPHINX_INCLUDE) $(INCLUDES)
LDFLAGS = -L/usr/local/lib $(SIMPLEBLE_LIB) $(DBUS_LIB) $(ASLA_LIB) $(POCKETSPHINX_LIB) -lpthread

# The default rule
all: $(TARGET)

# Rule to build the target binary
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Rule to compile source files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up build artifacts
clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all clean