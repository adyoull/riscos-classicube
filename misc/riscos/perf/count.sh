#!/bin/bash
# Exact instruction counts per frame (the same every run, unlike times),
# under a qemu-arm built with plugins (QEMU_PLUGINS=<qemu build dir>) and
# insnprof.c, counting only between the program's perf_mark_start and
# perf_mark_stop (steady state: see Window_OSMesaPerf.c).
#   [OPTS=...] count.sh OUTDIR [N] [VAR=value ...]
# Prints instructions per frame and the top functions (TOP=25).
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
out=$(cd "$1" && pwd); n=${2:-10}; shift 2 || true; EXTRA=("$@")
: "${QEMU_PLUGINS:?QEMU_PLUGINS=<qemu build dir with qemu-arm>}"
PLUG=$HERE/out-plugin/libinsnprof.so
if [ ! -f "$PLUG" ] || [ "$HERE/insnprof.c" -nt "$PLUG" ]; then mkdir -p "$(dirname "$PLUG")"
  gcc -shared -fPIC -O2 -o "$PLUG" "$HERE/insnprof.c" -I"${QEMU_SRC:?QEMU_SRC=<qemu source>}/include/qemu" $(pkg-config --cflags glib-2.0); fi
on=$(arm-linux-gnueabihf-nm "$out/cc-perf" | awk '$3=="perf_mark_start"{print $1}')
off=$(arm-linux-gnueabihf-nm "$out/cc-perf" | awk '$3=="perf_mark_stop"{print $1}')
D=$out/count; rm -rf "$D"; mkdir -p "$D/run/texpacks"
python3 "$HERE/make-terrain.py" "$D/run/texpacks/default.zip"
if [ -n "${OPTS:-}" ]; then echo "$OPTS" | tr ';' '\n' > "$D/run/options.txt"; fi
tot=$( cd "$D/run" && env CC_WARM=${CC_WARM:-40} CC_FRAMES=$n CC_SHOT="$D/shot.ppm" INSNPROF="$D/prof.txt" "${EXTRA[@]}" \
      "$QEMU_PLUGINS/qemu-arm" -cpu cortex-a8 -plugin "$PLUG,on=$on,off=$off" "$out/cc-perf" --singleplayer 2>&1 \
      | grep -a "^INSNS" | awk '{print $2}' )
python3 - "$out/cc-perf" "$D/prof.txt" "$n" "$tot" <<'PY'
import sys, os, subprocess, bisect, collections
exe, f, n, tot = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
syms = []
for l in subprocess.run(['arm-linux-gnueabihf-nm', '-n', '--defined-only', exe], capture_output=True, text=True).stdout.splitlines():
    p = l.split()
    if len(p) == 3 and p[1] in 'tTwW': syms.append((int(p[0], 16), p[2]))
addrs = [x for x, _ in syms]; d = collections.Counter()
for l in open(f):
    pc, k = l.split(); i = bisect.bisect_right(addrs, int(pc, 16) & ~1) - 1
    d[syms[i][1] if i >= 0 else '?'] += int(k)
print(f'{tot/n/1e6:.2f}M instructions per frame ({n} frames)')
for name, k in d.most_common(int(os.environ.get('TOP', '25'))):
    print(f'{k/n/1e6:8.3f}M {100*k/tot:6.2f}%  {name}')
PY
