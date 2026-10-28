#!/bin/bash
# A/B two perf builds from one viewpoint: instructions per frame for each,
# and whether the last frames match (texture animation and clouds off,
# the top 26 rows - the debug text with the vertex count - left out).
#   cmpview.sh NAME OUT_A OUT_B [VAR=value ...]     (OPTS as for count.sh)
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd); name=$1 A=$2 B=$3; shift 3
S=${SHOTS:-$HERE/out-shots}; mkdir -p "$S"
a=$("$HERE/count.sh" "$A" ${N:-10} CC_NOANIM=1 CC_SKIP=clouds "$@" 2>/dev/null | awk 'NR==1{print $1}'); cp "$A/count/shot.ppm" "$S/$name-A.ppm"
b=$("$HERE/count.sh" "$B" ${N:-10} CC_NOANIM=1 CC_SKIP=clouds "$@" 2>/dev/null | awk 'NR==1{print $1}'); cp "$B/count/shot.ppm" "$S/$name-B.ppm"
diff=$(python3 - "$S/$name-A.ppm" "$S/$name-B.ppm" <<'PY'
import sys
from PIL import Image, ImageChops
a, b = Image.open(sys.argv[1]), Image.open(sys.argv[2])
w, h = a.size; box = (0, 26, w, h)
d = ImageChops.difference(a.crop(box), b.crop(box)).convert('L')
print(sum(1 for v in d.get_flattened_data() if v) if hasattr(d, 'get_flattened_data') else sum(1 for v in d.getdata() if v))
PY
)
printf '%-14s A %-9s B %-9s  pixels different: %s\n' "$name" "$a" "$b" "$diff"
