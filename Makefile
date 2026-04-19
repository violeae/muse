CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS ?=
LDFLAGS ?=

TARGET := bin/muse

PROJECT_SRCS := $(wildcard src/*.cpp)
MIDIFILE_SRCS := $(wildcard midifile/src/*.cpp)
RTMIDI_SRCS := RtMidi.cpp
SRCS := $(PROJECT_SRCS) $(MIDIFILE_SRCS) $(RTMIDI_SRCS)
OBJS := $(SRCS:.cpp=.o)
DEPS := $(OBJS:.o=.d)

INCLUDES := -I. -Iinclude -Imidifile/include
FLUID_CFLAGS := $(shell pkg-config --cflags fluidsynth)
FLUID_LIBS := $(shell pkg-config --libs fluidsynth)

CPPFLAGS += $(FLUID_CFLAGS)
ifeq ($(strip $(FLUID_LIBS)),)
LDLIBS ?= -lfluidsynth -lpthread
else
LDLIBS ?= $(FLUID_LIBS)
endif

.PHONY: all clean run

ifeq ($(OS),Windows_NT)
MKDIR_BIN = powershell -NoProfile -Command "New-Item -ItemType Directory -Force bin | Out-Null"
CPPFLAGS += -D__WINDOWS_MM__
LDLIBS += -lwinmm
else
MKDIR_BIN = mkdir -p bin
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	@$(MKDIR_BIN)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(DEPS) $(TARGET)

-include $(DEPS)
