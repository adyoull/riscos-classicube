#!/bin/bash
# best of N runs: ab.sh NAME OUTDIR OPTS [VAR=value...]
name=$1 out=$2 opts=$3; shift 3; best=
for i in $(seq ${N:-2}); do
  ms=$(OPTS="$opts" OUT=$out ./perf.sh run "$@" 2>/dev/null | sed -n 's/.*: \([0-9.]*\) ms\/frame/\1/p')
  [ -z "$best" ] || [ "$(echo "$ms < $best" | bc)" = 1 ] && best=$ms
done
spans=$(sed -n '/fast route:/{n;p}' $out/mesa-stats.txt | awk '{print $2}')
printf '%-28s %8s ms/frame (best of %s)  fast textured spans %s\n' "$name" "$best" "${N:-2}" "$spans"
