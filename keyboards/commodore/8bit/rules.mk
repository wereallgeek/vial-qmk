# MCU name
MCU = RP2040
BOOTLOADER = rp2040

# Build Options
#   Do not enable features in both rules.mk and info.json
#   Features are already defined in info.json
#   Only define build-specific settings here

# Custom source files for PS/2 device implementation
SRC += ps2_keyboard.c \
       kb.c

# Compiler optimization
OPT_DEFS += -O2

# If you need to override any info.json features, do it here
# But it's better to keep everything in info.json for consistency
EXTRAINCDIRS += lib/chibios/os/oslib/include

# Latching Caps Lock switch: build with  CAPS_LATCHING=yes
#   qmk compile -kb commodore/8bit -km c16-vial -e CAPS_LATCHING=yes
#   (or: make commodore/8bit:c16-vial CAPS_LATCHING=yes)
# Output file gets a "_latching" suffix, e.g. commodore_8bit_c16-vial_latching.uf2
# NOTE: run "qmk clean" when switching between normal and latching builds, the
# compiler flags are not tracked and old object files would be reused.
ifeq ($(strip $(CAPS_LATCHING)),yes)
    OPT_DEFS += -DCAPS_LATCHING
    # 'override' so this also works when TARGET was passed on make's command line
    override TARGET := $(TARGET)_latching
endif
