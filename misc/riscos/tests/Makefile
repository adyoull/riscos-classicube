# Fake RISC OS tests for the RISC OS port's own code, as 32-bit ARM Linux
# programs under riscos-crossdev's qemu-arm-aligntrap (alignment traps on,
# Cortex-A8, as RISC OS). Needs arm-linux-gnueabihf-gcc and RCD_DIR.
#   make -C misc/riscos/tests RCD_DIR=<riscos-crossdev> check
RCD_DIR ?= ../../../../riscos-crossdev
CC      := arm-linux-gnueabihf-gcc
CFLAGS  := -O1 -g -Wall -mfpu=vfpv3 -mfloat-abi=hard -no-pie \
           -D__riscos__ -D__FAKE_RISCOS -I$(RCD_DIR)/fake -I../../../src
OUT     := out
DEVKIT  ?= $(RISCOS_DEVKIT)

check: $(OUT)/test_acornssl $(OUT)/test_socketpoll $(OUT)/test_cavecull $(OUT)/test_vorbis $(OUT)/test_winsize $(OUT)/test_freemouse
	bash $(RCD_DIR)/qemu/aligntrap.sh $(OUT)/test_acornssl
	bash $(RCD_DIR)/qemu/aligntrap.sh $(OUT)/test_socketpoll
	bash $(RCD_DIR)/qemu/aligntrap.sh $(OUT)/test_cavecull
	bash $(RCD_DIR)/qemu/aligntrap.sh $(OUT)/test_vorbis
	bash $(RCD_DIR)/qemu/aligntrap.sh $(OUT)/test_winsize
	bash $(RCD_DIR)/qemu/aligntrap.sh $(OUT)/test_freemouse

$(OUT)/test_acornssl: test_acornssl.c ../../../src/SSL.c $(RCD_DIR)/fake/riscos.c
	@mkdir -p $(OUT)
	$(CC) $(CFLAGS) -o $@ $^

# Platform_Posix.c with the RISC OS branch on; the functions Socket_Poll
# doesn't reach are left unresolved (never called)
$(OUT)/platform_posix.o: ../../../src/Platform_Posix.c
	@mkdir -p $(OUT)
	$(CC) $(CFLAGS) -I$(DEVKIT)/include -w -c -o $@ $<
	arm-linux-gnueabihf-objcopy --redefine-sym main=cc_main $@

$(OUT)/test_socketpoll: test_socketpoll.c $(OUT)/platform_posix.o $(RCD_DIR)/fake/riscos.c
	$(CC) $(CFLAGS) -o $@ $^ \
	  -Wl,--wrap=poll,--wrap=getpeername,--wrap=getsockopt,--wrap=__getsockopt64 \
	  -Wl,--unresolved-symbols=ignore-all -lpthread

$(OUT)/test_cavecull: test_cavecull.c ../../../src/CaveCull.c
	@mkdir -p $(OUT)
	$(CC) $(CFLAGS) -DCC_BUILD_CAVECULL -o $@ $^

# Window_SDL2.c with the RISC OS branch on; what the window size code
# doesn't reach is left unresolved (never called)
$(OUT)/window_sdl2.o: ../../../src/Window_SDL2.c
	@mkdir -p $(OUT)
	$(CC) $(CFLAGS) -I$(DEVKIT)/include -w -c -o $@ $<

$(OUT)/test_winsize: test_winsize.c $(OUT)/window_sdl2.o $(RCD_DIR)/fake/riscos.c
	$(CC) $(CFLAGS) -I$(DEVKIT)/include -o $@ $^ -Wl,--unresolved-symbols=ignore-all

$(OUT)/test_freemouse: test_freemouse.c $(OUT)/window_sdl2.o
	$(CC) $(CFLAGS) -I$(DEVKIT)/include -o $@ $^ -Wl,--unresolved-symbols=ignore-all

# Vorbis.c (included by the test) with the RISC OS branch on, so with
# CC_BUILD_NEON; its NEON code is compiled for NEON by its own pragmas
$(OUT)/test_vorbis: test_vorbis.c ../../../src/Vorbis.c ../../../src/Vorbis.h $(RCD_DIR)/fake/riscos.c
	@mkdir -p $(OUT)
	$(CC) $(CFLAGS) -fno-math-errno -o $@ test_vorbis.c ../../../src/Stream.c ../../../src/ExtMath.c \
	  ../../../src/Utils.c $(RCD_DIR)/fake/riscos.c -lm -Wl,--unresolved-symbols=ignore-all

clean:
	rm -rf $(OUT)
.PHONY: check clean
