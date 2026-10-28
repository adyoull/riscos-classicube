#!/bin/bash
# TEST ONLY: measure ClassiCube's GL1 renderer on the devkit's RISC OS
# libOSMesa.a, as an ARM Linux program under qemu-arm (Cortex-A8 with NEON,
# as riscos-mesa's run-arm.sh does). Not a Pi figure: use it to compare
# builds and to see where the time goes.
#   perf.sh build                 build out/cc-perf from ../../../src
#   [OPTS='viewdist=128;...'] perf.sh run [VAR=value ...]   run it (see Window_OSMesaPerf.c), profile
#                                 to out/prof.txt, report to out/report.txt
# Needs: arm-linux-gnueabihf-gcc/g++, qemu-arm, RISCOS_DEVKIT.
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd); SRC=${SRC:-$HERE/../../../src}; OUT=${OUT:-$HERE/out}
: "${RISCOS_DEVKIT:?set RISCOS_DEVKIT}"
mkdir -p "$OUT/obj"
case ${1:-} in
build)
  CFLAGS="-O2 -g -mfpu=vfpv3 -mfloat-abi=hard -fno-math-errno -DCC_WIN_BACKEND=99 -DCC_GFX_BACKEND=CC_GFX_BACKEND_GL1 -DCC_AUD_BACKEND=CC_AUD_BACKEND_NULL ${EXTRA_CFLAGS:-}"
  cp "$HERE/Window_OSMesaPerf.c" "$OUT/Window_OSMesaPerf.c"
  ls "$SRC"/*.c "$SRC"/../third_party/bearssl/*.c "$OUT/Window_OSMesaPerf.c" | xargs -P"$(nproc)" -I{} sh -c \
    'arm-linux-gnueabihf-gcc '"$CFLAGS"' -I'"$SRC"' -I'"$RISCOS_DEVKIT"'/include -w -c {} -o '"$OUT"'/obj/$(basename {} .c).o'
  cp "$RISCOS_DEVKIT/lib/libOSMesa.a" "$OUT/libOSMesa.a"
  arm-linux-gnueabihf-objcopy --redefine-sym errno=ro_errno_shim \
    $(for f in mutex_init mutex_lock mutex_unlock mutex_destroy mutexattr_init mutexattr_settype mutexattr_destroy; do
        echo "--redefine-sym pthread_$f=ro_pthread_${f}_shim"; done) "$OUT/libOSMesa.a"
  arm-linux-gnueabihf-gcc -c -O2 -mfloat-abi=hard "$HERE/shim.c" -o "$OUT/shim.o"
  arm-linux-gnueabihf-g++ -static -o "$OUT/cc-perf" "$OUT"/obj/*.o "$OUT/shim.o" "$OUT/libOSMesa.a" \
    "$RISCOS_DEVKIT/lib/libz.a" -lm -lpthread -Wl,--wrap=Random_SeedFromCurrentTime,--wrap=EnvRenderer_RenderSky,--wrap=EnvRenderer_RenderClouds,--wrap=EnvRenderer_RenderMapSides,--wrap=EnvRenderer_RenderMapEdges,--wrap=MapRenderer_RenderNormal,--wrap=MapRenderer_RenderTranslucent,--wrap=Entities_RenderModels,--wrap=HeldBlockRenderer_Render,--wrap=Gui_RenderGui,--wrap=Particles_Render,--wrap=Gfx_SetAlphaTest,--wrap=Gfx_SetFog,--wrap=Gfx_UpdateTexture -Wl,-Map="$OUT/cc-perf.map" 2>&1 | grep -v "warning\|NOTE" || true
  ls -la "$OUT/cc-perf";;
run)
  shift; D=$OUT/run; rm -rf "$D"; mkdir -p "$D/texpacks"
  python3 "$HERE/make-terrain.py" "$D/texpacks/default.zip"
  # OPTS="viewdist=128;..." -> options.txt
  [ -n "${OPTS:-}" ] && echo "$OPTS" | tr ';' '\n' > "$D/options.txt"
  ( cd "$D" && env CC_PROF="$OUT/prof.txt" CC_SHOT="$OUT/shot.ppm" MESA_STATS="$OUT/mesa-stats.txt" "$@" \
      qemu-arm -cpu cortex-a8 "$OUT/cc-perf" --singleplayer 2>&1 | grep -a "PERF\|rror" | tail -5 )
  python3 "$HERE/profsym.py" "$OUT/cc-perf" "$OUT/prof.txt" > "$OUT/report.txt"; head -40 "$OUT/report.txt";;
*) echo "usage: $0 build | run [VAR=value...]"; exit 2;;
esac
