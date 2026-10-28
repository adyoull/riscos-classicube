# Fake RISC OS tests for the RISC OS port's own code, as 32-bit ARM Linux
# programs under riscos-crossdev's qemu-arm-aligntrap (alignment traps on,
# Cortex-A8, as RISC OS). Needs arm-linux-gnueabihf-gcc and RCD_DIR.
#   make -C misc/riscos/tests RCD_DIR=<riscos-crossdev> check
RCD_DIR ?= ../../../../riscos-crossdev
CC      := arm-linux-gnueabihf-gcc
CFLAGS  := -O1 -g -Wall -mfpu=vfpv3 -mfloat-abi=hard -no-pie \
           -D__riscos__ -D__FAKE_RISCOS -I$(RCD_DIR)/fake -I../../../src
OUT     := out

check: $(OUT)/test_acornssl
	bash $(RCD_DIR)/qemu/aligntrap.sh $(OUT)/test_acornssl

$(OUT)/test_acornssl: test_acornssl.c ../../../src/SSL.c $(RCD_DIR)/fake/riscos.c
	@mkdir -p $(OUT)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -rf $(OUT)
.PHONY: check clean
