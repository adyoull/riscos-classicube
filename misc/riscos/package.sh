#!/bin/bash
# Make dist/!ClassiCube and dist/ClassiCube-riscos.zip from a RISC OS build.
#   misc/riscos/package.sh [VERSION]
# Needs: ClassiCube,e1f (make riscos RELEASE=1 RISCOS_DEVKIT=...),
# RISCOS_DEVKIT (for PThreadTicker), RCD_DIR (riscos-crossdev: png2sprite,
# rozip) and python3 with PIL (to scale the icon).
set -e
cd "$(dirname "$0")/../.."
VER=${1:-dev}
: "${RISCOS_DEVKIT:?set RISCOS_DEVKIT to the riscos-mesa devkit}"
: "${RCD_DIR:?set RCD_DIR to riscos-crossdev}"
APP='dist/!ClassiCube'
rm -rf "$APP" && mkdir -p "$APP"
cp misc/riscos/app/* "$APP/"
cp ClassiCube,e1f "$APP/!RunImage,e1f"
cp license.txt "$APP/Licence,fff"
cp "$RISCOS_DEVKIT/riscos/PThrTicker,ffa" "$RISCOS_DEVKIT/riscos/PThreadTicker-Licence,fff" "$APP/"
python3 - <<'PY'
from PIL import Image
im = Image.open('misc/CCicon.png').convert('RGBA')
for n, s in (('big', 34),):
    im.resize((s, s), Image.LANCZOS).save(f'build/icon-{n}.png')
PY
# (sm!classicube would be 13 characters, over the 12 a sprite name allows:
#  the Filer scales the big icon for small views)
python3 "$RCD_DIR/package/png2sprite.py" "$APP/!Sprites,ff9" '!classicube=build/icon-big.png'
python3 "$RCD_DIR/package/rozip.py" "dist/ClassiCube-riscos-$VER.zip" -C dist '!ClassiCube'
echo "made dist/ClassiCube-riscos-$VER.zip"
