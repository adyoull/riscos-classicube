# ClassiCube for RISC OS

RISC OS 5 on ARMv7 (VFPv3): Raspberry Pi 2 and later, and other Cortex-A
boards. An Absolute (AIF) program: a static ELF built with GCCSDK's GCC 10 (riscos-crossdev toolchain)
against the riscos-mesa devkit (20.3.5-17 or later), then `elf2aif -e`
(WimpSlot 20480K in `!Run`).

| Part | How |
|---|---|
| Platform | `Platform_Posix.c` on UnixLib; single process (launcher and game in one task); URLs through `URI_Dispatch`; static symbol table instead of `dlopen` (OpenAL); data in `ClassiCube$Data` (default `<Choices$Write>.ClassiCube`); updater off |
| Window, input | `Window_SDL2.c` on riscos-mesa's SDL2 (RISC OS video driver with GL) |
| Graphics | `Graphics_GL1.c`: OpenGL 1.x, riscos-mesa (software, NEON fast paths) |
| Sound | `Audio_OpenAL.c`: the devkit's OpenAL Soft, through SDL (SharedSoundBuffer or DigitalRenderer) |
| TLS | `SSL.c` `CC_SSL_BACKEND_ACORNSSL`: the AcornSSL module (`AcornSSL_CreateSession` on the connected socket, SNI, non-blocking with sleeps) |
| Sockets | `Platform_Posix.c` `Socket_Poll`: the Internet module says a still-connecting socket is writable, so on RISC OS it also checks `getpeername` / `SO_ERROR` (otherwise the first send fails with ENOTCONN, error 57) |
| HTTP | `Http_Worker.c`: non-blocking sockets with sleeps between tries, because a blocking SWI stops every thread in the task |

## Build

    export PATH=<toolchain>/bin:$PATH
    make riscos RELEASE=1 RISCOS_DEVKIT=<riscos-mesa devkit>
    RISCOS_DEVKIT=<devkit> RCD_DIR=<riscos-crossdev> misc/riscos/package.sh <version>

`dist/ClassiCube-riscos-<version>.zip` keeps the filetypes (rozip.py).

## Checks and tests

- `$RCD_DIR/checks/check-all.sh` on an unstripped link: static, stack probes,
  VFPv3 only, UnixLib ticker block.
- `$RCD_DIR/checks/check-unaligned.sh -s misc/riscos/unaligned-safe.txt src/*.c`:
  10 casts, all checked by hand (in the safe list).
- `make -C misc/riscos/tests RCD_DIR=<riscos-crossdev> DEVKIT=<devkit> check`,
  as ARM under qemu-arm-aligntrap:
  - test_acornssl: the AcornSSL backend against fake SWIs (handshake
    retries, timeouts, error text, closing);
  - test_socketpoll: Socket_Poll's connect check, with poll/getpeername/
    getsockopt faked by `--wrap` (still connecting, connected, refused).
  Both catch deliberate mutants.
- The portable code as ARM Linux (terminal window, software renderer) under
  qemu-arm-aligntrap -cpu cortex-a8: a singleplayer world generates and
  renders with no alignment trap.

## Not done yet

- FreeType is off, so text uses the built-in bitmap font.
- Plugins are off (no `dlopen`).
- DNS lookups (`gethostbyname`) are still a blocking SWI on the HTTP thread.
- No saved-password encryption (no machine ID on RISC OS).
