# ClassiCube for RISC OS

RISC OS 5 on ARMv7 (VFPv3): Raspberry Pi 2 and later, and other Cortex-A
boards. An Absolute (AIF) program: a static ELF built with GCCSDK's GCC 10 (riscos-crossdev toolchain)
against the riscos-mesa devkit (20.3.5-17 or later), then `elf2aif -e`
(WimpSlot 20480K in `!Run`).

| Part | How |
|---|---|
| Platform | `Platform_Posix.c` on UnixLib; single process (launcher and game in one task); URLs through `URI_Dispatch`; static symbol table instead of `dlopen` (OpenAL); data in `ClassiCube$Data` (default `<Choices$Write>.ClassiCube`); updater off |
| Window, input | `Window_SDL2.c` on riscos-mesa's SDL2 (RISC OS video driver with GL) |
| Graphics | `Graphics_GL1.c`: OpenGL 1.x, riscos-mesa (software, NEON fast paths); `CC_GL_FASTEST_HINTS`: GL_FASTEST perspective and fog hints (its fast route) |
| Cave culling | `CaveCull.c`, `Builder.c` (links per chunk), `MapRenderer.c` (flood from the camera): `CC_BUILD_CAVECULL` |
| Render size | `Window_SDL2.c`: `ClassiCube$RenderSize` (default 640x360, `full` = off) and `ClassiCube$Overlay` become `SDL_RISCOS_GL_RENDER_SIZE` / `SDL_RISCOS_GL_OVERLAY` before the game window opens; `sched_yield()` each frame; default view distance 128 |
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
  - test_cavecull: CaveCull_CalcLinks against a plain cell-by-cell flood
    fill (3000 random chunks of 5-95% see-through cells, tunnels, rooms,
    winding paths).
  All catch deliberate mutants.
- The portable code as ARM Linux (terminal window, software renderer) under
  qemu-arm-aligntrap -cpu cortex-a8: a singleplayer world generates and
  renders with no alignment trap.

## Measuring speed (container)

`misc/riscos/perf/`: the game's GL1 renderer on the devkit's own RISC OS
`libOSMesa.a`, linked into an ARM Linux program with riscos-mesa's
`shim.c` and run under qemu-arm (Cortex-A8). Headless OSMesa window,
fixed world seed, MESA_STATS. Nothing in it is part of the game.

- `perf.sh build` / `perf.sh run`: build, and a run with a SIGPROF profile
  (QEMU CPU time: noisy, compare builds only).
- `count.sh`: **exact instruction counts per frame**, the same every run,
  with a qemu-arm built with plugins and `insnprof.c`. It counts only
  between the program's `perf_mark_start`/`perf_mark_stop`, which the
  window calls once the world is loaded and chunk building has stopped;
  the camera turns 0.5° a frame from then.
- `cmpview.sh` / `views.sh`: two builds from one or several viewpoints
  (`CC_POS=x,y,z,pitch`, or `cave`): instructions per frame and whether
  the frames are identical (texture animation and clouds off, debug text
  left out). Switches: `CC_SKIP=pass,...`, `CC_NOALPHA`, `CC_NOFOG`,
  `CC_NOANIM`, `CC_W`/`CC_H`.

    RISCOS_DEVKIT=<devkit> EXTRA_CFLAGS=-DCC_GL_FASTEST_HINTS misc/riscos/perf/perf.sh build
    QEMU_PLUGINS=<qemu 8.2 build> QEMU_SRC=<qemu source> OPTS=viewdist=128 \
        misc/riscos/perf/count.sh misc/riscos/perf/out 10

Findings (instructions per frame at 640x360, view distance 128):
- the terrain pass is 93% of a frame; ~95% of a frame is inside Mesa;
- 480x270 instead of 640x360: -25%; fog off -7%; alpha test off -6%;
  view distance hardly matters on the 128x128 single-player map;
- drawing only the opaque tiles without the alpha test (an atlas split)
  made no difference: Mesa's alpha-tested route costs about the same as
  the fused one, so it was dropped;
- cave culling (`CC_BUILD_CAVECULL`, `CaveCull.c`): each chunk records
  which of its faces see each other through blocks that aren't fully
  opaque (a bit-parallel flood fill, 16 cells at a time, when the chunk
  is built: +3% chunk building); each time the camera moves to another
  chunk, or chunks change, the renderer floods out from the camera's
  chunk (from the world's sides facing it when it's outside the world)
  and skips chunks it doesn't reach. Frames are pixel-identical from six
  viewpoints (spawn, looking down, inside rock, above the map, in a cave,
  past the edge); instructions per frame -8 to -9% at ground level, about
  the same elsewhere (on this small map almost everything in view really
  is visible). Off when the camera is inside a solid block (noclip).

## Not done yet

- FreeType is off, so text uses the built-in bitmap font.
- Plugins are off (no `dlopen`).
- DNS lookups (`gethostbyname`) are still a blocking SWI on the HTTP thread.
- No saved-password encryption (no machine ID on RISC OS).
