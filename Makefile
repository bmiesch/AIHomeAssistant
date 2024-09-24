# Compiler and flags
CXX = g++
CXXFLAGS = -Wall -std=c++11

# The name of the output binary
TARGET = bluetooth_light_control

# The source file
SRC = src/main2.cpp

# For macOS, avoid linking to BlueZ since it's Linux-specific
# If you want to build and test on macOS (without actual Bluetooth functionality)
# Comment this line out if you're compiling on Raspberry Pi or Linux
MACOS := $(shell uname)

ifeq ($(MACOS), Darwin)
    LDFLAGS =  # On macOS, no Bluetooth library is needed for now
    CXXFLAGS += -D__APPLE__
else
    # For Raspberry Pi or Linux, link against the BlueZ Bluetooth library
    LDFLAGS = -lbluetooth
    CXXFLAGS += -I/usr/include/bluetooth
endif

# The default rule
all: $(TARGET)

# Rule to build the target binary
$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

# Clean up build artifacts
clean:
	rm -f $(TARGET)