# Compiler and flags
CXX = g++
CXXFLAGS = -Wall -Wextra -pedantic -std=c++17

# The name of the output binary
TARGET = bluetooth_light_control

# The source file
SRC = src/main.cpp

# SimpleBLE library flags
SIMPLEBLE_INCLUDE = -I/usr/local/include
SIMPLEBLE_LIB = -lsimpleble

# D-Bus library flags
DBUS_LIB = -ldbus-1

# Flags for SimpleBLE dependencies
CXXFLAGS += $(SIMPLEBLE_INCLUDE)
LDFLAGS = -L/usr/local/lib $(SIMPLEBLE_LIB) $(DBUS_LIB) -lpthread


# The default rule
all: $(TARGET)

# Rule to build the target binary
$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Clean up build artifacts
clean:
	rm -f $(TARGET)

.PHONY: all clean