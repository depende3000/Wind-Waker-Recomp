# Minimal libnx boot/storage/display probe. The product NRO comes later, after
# the Aurora/GX graphics path has been proven on physical Switch hardware.
.SUFFIXES:

ifeq ($(strip $(DEVKITPRO)),)
$(error DEVKITPRO is unset; use the devkitPro Switch development environment)
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITPRO)/libnx/switch_rules

TARGET := BlueWakeSwitchProbe
BUILD ?= $(abspath $(CURDIR)/../build/switch-probe)
SOURCES := source
DATA :=
INCLUDES :=

APP_TITLE := Wind Waker Recomp Switch Probe
APP_AUTHOR := Wind Waker Recomp contributors
APP_VERSION := 0.1.0
NO_ICON := 1

ARCH := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
CFLAGS := -g -Wall -Wextra -O2 -ffunction-sections -fdata-sections $(ARCH)
CFLAGS += -D__SWITCH__
CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions
ASFLAGS := -g $(ARCH)
LDFLAGS := -specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH)
LIBS := -lnx
LIBDIRS := $(PORTLIBS) $(LIBNX)

ifneq ($(abspath $(BUILD)),$(abspath $(CURDIR)))
export OUTPUT := $(abspath $(BUILD)/$(TARGET))
export TOPDIR := $(CURDIR)
export VPATH := $(CURDIR)/$(SOURCES)
export DEPSDIR := $(abspath $(BUILD))

CFILES := $(notdir $(wildcard $(SOURCES)/*.c))
CPPFILES := $(notdir $(wildcard $(SOURCES)/*.cpp))
SFILES := $(notdir $(wildcard $(SOURCES)/*.s))
BINFILES := $(notdir $(wildcard $(DATA)/*.*))

ifeq ($(strip $(CPPFILES)),)
export LD := $(CC)
else
export LD := $(CXX)
endif

export OFILES_BIN := $(addsuffix .o,$(BINFILES))
export OFILES_SRC := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES := $(OFILES_BIN) $(OFILES_SRC)
export HFILES_BIN := $(addsuffix .h,$(subst .,_,$(BINFILES)))
export CPPFLAGS := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
				   $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
				   -I$(abspath $(BUILD)) -D__SWITCH__
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)
export NROFLAGS := --nacp=$(OUTPUT).nacp

.PHONY: all clean
all: $(BUILD)

$(BUILD):
	@mkdir -p "$@" "$(dir $(OUTPUT))"
	@$(MAKE) --no-print-directory -C "$@" -f "$(CURDIR)/Makefile" BUILD="$(abspath $(BUILD))"

clean:
	@rm -rf "$(BUILD)" "$(OUTPUT).nro" "$(OUTPUT).nacp" "$(OUTPUT).elf"
else
.PHONY: all
DEPENDS := $(OFILES:.o=.d)

all: $(OUTPUT).nro

$(OUTPUT).nro: $(OUTPUT).elf $(OUTPUT).nacp
$(OUTPUT).elf: $(OFILES)
$(OFILES_SRC): $(HFILES_BIN)

-include $(DEPENDS)
endif