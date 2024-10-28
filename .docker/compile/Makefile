# Compiler and flags
CXX = ccache g++
CROSS_CXX = ccache aarch64-linux-gnu-g++
CXXFLAGS = -Wall -Wextra -pedantic -std=c++17 -g -O0

# The name of the output binary
TARGET = start
CROSS_TARGET = start_arm64

# Source and build directories
SRCDIR = /app/source
SRCS := $(wildcard $(SRCDIR)/src/*.cpp)
OBJS = $(notdir $(SRCS:.cpp=.o))
CROSS_OBJS = $(notdir $(SRCS:.cpp=.cross.o))

# Header files directory
INCLUDES = -I$(SRCDIR)/inc -I$(SRCDIR)/src

# Library includes and flags (unchanged)
SIMPLEBLE_INCLUDE = -I/usr/local/include
SIMPLEBLE_LIB = -lsimpleble
DBUS_LIB = -ldbus-1
ASLA_INCLUDE = -I/usr/include
ASLA_LIB = -lasound
POCKETSPHINX_INCLUDE = -I/usr/local/include/pocketsphinx
POCKETSPHINX_LIB = -lpocketsphinx
PAHO_MQTT_INCLUDE = -I/usr/local/include
PAHO_MQTT_LIB = -lpaho-mqttpp3 -lpaho-mqtt3as

# Update CXXFLAGS and LDFLAGS
CXXFLAGS += $(SIMPLEBLE_INCLUDE) $(ASLA_INCLUDE) $(POCKETSPHINX_INCLUDE) $(PAHO_MQTT_INCLUDE) $(INCLUDES)
LDFLAGS = -L/usr/local/lib $(SIMPLEBLE_LIB) $(DBUS_LIB) $(ASLA_LIB) $(POCKETSPHINX_LIB) $(PAHO_MQTT_LIB) -lpthread

# The default rule
all: $(TARGET)

# Rule to build the target binary
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Rule to compile source files
%.o: $(SRCDIR)/src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Cross-compilation rules
cross: $(CROSS_TARGET)

$(CROSS_TARGET): $(CROSS_OBJS)
	$(CROSS_CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.cross.o: $(SRCDIR)/src/%.cpp
	$(CROSS_CXX) $(CXXFLAGS) -c $< -o $@

# Clean up build artifacts
clean:
	rm -f $(TARGET) $(CROSS_TARGET) $(OBJS) $(CROSS_OBJS)

.PHONY: all clean cross