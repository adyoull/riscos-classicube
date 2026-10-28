# RISC OS (ARMv7, hard float) with GCCSDK's GCC 10 and the riscos-mesa devkit.
#   make riscos RELEASE=1 RISCOS_DEVKIT=<devkit dir>
# arm-riscos-gnueabihf-gcc must be on the PATH (or set CC).
SOURCE_DIRS := src
BUILD_DIR	:= build/riscos
TARGET 		:= ClassiCube
OEXT		:= ,e1f

RISCOS_DEVKIT ?= ../riscos-mesa-devkit
CC		:= arm-riscos-gnueabihf-gcc

# -fstack-clash-protection: needed by UnixLib's chunked stacks (see the
# riscos-mesa devkit README)
CFLAGS  := -mfpu=vfpv3 -mfloat-abi=hard -fstack-clash-protection \
           -I$(RISCOS_DEVKIT)/include -I$(RISCOS_DEVKIT)/include/SDL2
LDFLAGS := -static -L$(RISCOS_DEVKIT)/lib
ifdef RELEASE
LDFLAGS += -s
endif
LIBS    := -lopenal -lSDL2 -lEGL -lOSMesa -lstdc++ -lz -lpthread -lm

include misc/makefiles/common_config.mk


#---------------------------------------------------------------------------------
# executable generation
#---------------------------------------------------------------------------------
include misc/makefiles/common_build.mk


#---------------------------------------------------------------------------------
# common targets
#---------------------------------------------------------------------------------
include misc/makefiles/common_targets.mk
