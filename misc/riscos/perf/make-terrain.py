#!/usr/bin/env python3
"""Stand-in texture pack for measuring only (the container can't download
the real default.zip): terrain.png 256x256 with 16x16 noise tiles, and
alpha holes in the tiles that have them in the real one (leaves, glass,
flowers, saplings, mushrooms), so the alpha test passes and fails about
as often. Writes default.zip."""
import random, zipfile, io, sys
from PIL import Image
random.seed(1)
im = Image.new('RGBA', (256, 256))
holes = {12: .6, 13: .6, 15: .6, 28: .7, 29: .7, 49: .85, 52: .35}
for t in range(256):
    tx, ty = (t % 16) * 16, (t // 16) * 16
    base = [random.randrange(40, 220) for _ in range(3)]
    for y in range(16):
        for x in range(16):
            c = [max(0, min(255, b + random.randrange(-30, 30))) for b in base]
            a = 0 if (t in holes and random.random() < holes[t]) else 255
            im.putpixel((tx + x, ty + y), (*c, a))
buf = io.BytesIO(); im.save(buf, 'PNG')
with zipfile.ZipFile(sys.argv[1] if len(sys.argv) > 1 else 'default.zip', 'w') as z:
    z.writestr('terrain.png', buf.getvalue())
