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
ifdef RISCOS_MIXED
# A mixed program (riscos-crossdev 1.12 or later, devkit 20.3.5-20 or later):
# UnixLib, libstdc++ and libgcc linked in, OpenAL, SDL, EGL and OSMesa
# loaded from !MesaLibs by !SharedLibs (the devkit's docs/porting/mixed.md).
# -lMesaGPU after -lOSMesa: EGL and OSMesa then draw through whatever
# graphics RISC3D (from !RISC3D) finds, or in software where it finds none.
#   make riscos RELEASE=1 RISCOS_MIXED=1 RISCOS_DEVKIT=<devkit dir>
# The same objects as the static program; the ELF isn't made an AIF.
TARGET  := ClassiCube-mixed
RISCOS_MIXED_STUBS := $(shell $(CC) -print-file-name=mixed-stubs)
LDFLAGS := -rdynamic -Wl,-rpath-link=$(RISCOS_MIXED_STUBS) \
           -L$(RISCOS_DEVKIT)/lib/shared -L$(RISCOS_DEVKIT)/lib
LIBS    := -lopenal -lSDL2 -lEGL -lOSMesa -lMesaGPU -ldl -lpthread -lm
else
LDFLAGS := -static -L$(RISCOS_DEVKIT)/lib
LIBS    := -lopenal -lSDL2 -lEGL -lOSMesa -lstdc++ -lz -lpthread -lm
endif
ifdef RELEASE
LDFLAGS += -s
endif

include misc/makefiles/common_config.mk


#---------------------------------------------------------------------------------
# executable generation
#---------------------------------------------------------------------------------
include misc/makefiles/common_build.mk


#---------------------------------------------------------------------------------
# common targets
#---------------------------------------------------------------------------------
include misc/makefiles/common_targets.mk
