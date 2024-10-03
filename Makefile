# Compiler and flags
CXX = g++
CXXFLAGS = -Wall -Wextra -pedantic -std=c++17

# The name of the output binary
TARGET = bluetooth_light_control

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

# Flags for SimpleBLE dependencies
CXXFLAGS += $(SIMPLEBLE_INCLUDE) $(INCLUDES)
LDFLAGS = -L/usr/local/lib $(SIMPLEBLE_LIB) $(DBUS_LIB) -lpthread

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