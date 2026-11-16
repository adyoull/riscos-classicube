#!/bin/bash
# Make dist/!ClassiCube and dist/ClassiCube-riscos.zip from a RISC OS build.
#   misc/riscos/package.sh [VERSION]
#   MIXED=1 misc/riscos/package.sh [VERSION]   the mixed program (below)
# Needs: ClassiCube,e1f (make riscos RELEASE=1 RISCOS_DEVKIT=...; stripped),
# or for MIXED=1 ClassiCube-mixed,e1f (... RISCOS_MIXED=1) and the devkit's
# riscos/!MesaLibs (or MESALIBS=<a !MesaLibs with ,xxx file types>, for a
# newer !MesaLibs than the devkit's),
# elf2aif (riscos-crossdev toolchain: ELF2AIF, or on the PATH),
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
if [ "$VER" != dev ]; then
  # the port's version under !Help's title
  sed -i "3i\\Version $VER" "$APP/!Help,fff"
fi
if [ -n "${MIXED:-}" ]; then
  # A mixed program: an ELF file run by !SharedLibs' loader, loading OpenAL,
  # SDL, EGL and OSMesa from !MesaLibs (the devkit's docs/porting/mixed.md).
  # Its loader settings go in ClassiCube$LD$Env, named after the file.
  cp ClassiCube-mixed,e1f "$APP/ClassiCube,e1f"
  python3 - "$APP/!Run,feb" <<'PY'
import sys
p = sys.argv[1]; s = open(p).read()
i = s.index('| The program is about')
s = s[:i] + '''| This is the mixed build: the game (about 2 MB) loads OpenGL, EGL, SDL
| and OpenAL from !MesaLibs, through !SharedLibs' ELF loader, so programs
| using the same !MesaLibs share one copy of them. !RISC3D, if it's in
| !Boot.Resources, works out which graphics hardware to draw with; without
| it the game draws in software. All three go in !Boot.Resources.
If "<SharedLibs$Dir>" = "" Then IfThere Boot:Resources.!SharedLibs.!Boot Then Run Boot:Resources.!SharedLibs.!Boot
RMEnsure RISC3D 0.04 IfThere Boot:Resources.!RISC3D.!Boot Then Run Boot:Resources.!RISC3D.!Boot
If "<MesaLibs$Dir>" = "" Then IfThere Boot:Resources.!MesaLibs.!Boot Then Run Boot:Resources.!MesaLibs.!Boot
Unset ClassiCube$SO2
IfThere SharedLibs:lib.ld-riscos-eabihf/so Then Set ClassiCube$SO2 1
If "<ClassiCube$SO2>" = "" Then Error !ClassiCube needs a !SharedLibs with ld-riscos/so/2 (GCCSDK 8 or later, as in Iris) in !Boot.Resources
If "<MesaLibs$Dir>" = "" Then Error !ClassiCube needs !MesaLibs (in this zip) in !Boot.Resources
Set ClassiCube$LD$Env "LD_LIBRARY_PATH=/MesaLibs:"
|
| The heap is in a dynamic area: the slot holds the program
WimpSlot -min 16000K -max 16000K
Run <ClassiCube$Dir>.ClassiCube > <Wimp$ScrapDir>.ClassiCubeLog 2>&1
'''
open(p, 'w').write(s)
PY
  cat >> "$APP/!Help,fff" <<'HLP'

Mixed build
-----------
This copy loads OpenGL, EGL, SDL and OpenAL from !MesaLibs instead of
having them built in. Put !MesaLibs (in this zip) in !Boot.Resources, and
have a !SharedLibs with ld-riscos/so/2 (GCCSDK 8 or later, as in Iris) in
!Boot.Resources too. With !RISC3D in !Boot.Resources as well, the game
draws with whatever graphics hardware RISC3D finds; without it, in
software. After replacing !MesaLibs or !RISC3D, restart the machine.
HLP
else
  # An Absolute (AIF) file: runs without the !SharedLibs ELF loader, and
  # !Run's WimpSlot applies to it
  "${ELF2AIF:-elf2aif}" -e ClassiCube,e1f "$APP/!RunImage,ff8"
fi
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
if [ -n "${MIXED:-}" ]; then
  rm -rf 'dist/!MesaLibs' && cp -r "${MESALIBS:-$RISCOS_DEVKIT/riscos/!MesaLibs}" dist/
  python3 "$RCD_DIR/package/rozip.py" "dist/ClassiCube-riscos-$VER-mixed.zip" -C dist '!ClassiCube' '!MesaLibs'
  echo "made dist/ClassiCube-riscos-$VER-mixed.zip"
else
  python3 "$RCD_DIR/package/rozip.py" "dist/ClassiCube-riscos-$VER.zip" -C dist '!ClassiCube'
  echo "made dist/ClassiCube-riscos-$VER.zip"
fi
