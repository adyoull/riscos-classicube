#!/usr/bin/env python3
"""PC samples -> functions (nm), top list with percentages."""
import sys, subprocess, bisect, collections
syms = []
for line in subprocess.run(['arm-linux-gnueabihf-nm', '-n', '--defined-only', sys.argv[1]], capture_output=True, text=True).stdout.splitlines():
    p = line.split()
    if len(p) == 3 and p[1] in 'tTwW': syms.append((int(p[0], 16), p[2]))
addrs = [a for a, _ in syms]
c = collections.Counter()
pcs = [int(l, 16) for l in open(sys.argv[2]) if l.strip()]
for pc in pcs:
    i = bisect.bisect_right(addrs, pc & ~1) - 1
    c[syms[i][1] if i >= 0 else '?'] += 1
n = len(pcs) or 1
print(f'{len(pcs)} samples')
for name, k in c.most_common(45): print(f'{100*k/n:6.2f}%  {name}')
