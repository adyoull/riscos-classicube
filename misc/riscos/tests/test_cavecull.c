/* CaveCull_CalcLinks (src/CaveCull.c, bit-parallel flood fill by rows)
   against a plain flood fill one cell at a time, on random chunks of many
   densities and on hand-made ones. Run as ARM under qemu-arm-aligntrap. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../src/CaveCull.h"
#include "../../../src/Constants.h"

static int checks, fails;
#define CHECK(c, what) do { checks++; if (!(c)) { fails++; if (fails < 10) fprintf(stderr, "FAIL: %s\n", what); } } while (0)

static unsigned char cells[16][16][16]; /* [y][z][x], 1 = see-through */

static void reference(unsigned char links[6]) {
	static unsigned char seen[16][16][16];
	static int stack[4096][3];
	int x, y, z, top, faces, f;
	memset(seen, 0, sizeof seen); memset(links, 0, 6);
	for (y = 0; y < 16; y++) for (z = 0; z < 16; z++) for (x = 0; x < 16; x++) {
		if (!cells[y][z][x] || seen[y][z][x]) continue;
		seen[y][z][x] = 1; top = 0; faces = 0;
		stack[top][0] = x; stack[top][1] = y; stack[top][2] = z; top++;
		while (top) {
			int cx, cy, cz, d;
			static const int D[6][3] = { {-1,0,0},{1,0,0},{0,0,-1},{0,0,1},{0,-1,0},{0,1,0} };
			top--; cx = stack[top][0]; cy = stack[top][1]; cz = stack[top][2];
			for (d = 0; d < 6; d++) {
				int nx = cx + D[d][0], ny = cy + D[d][1], nz = cz + D[d][2];
				if (nx < 0 || ny < 0 || nz < 0 || nx > 15 || ny > 15 || nz > 15) { faces |= 1 << d; continue; }
				if (!cells[ny][nz][nx] || seen[ny][nz][nx]) continue;
				seen[ny][nz][nx] = 1;
				stack[top][0] = nx; stack[top][1] = ny; stack[top][2] = nz; top++;
			}
		}
		for (f = 0; f < 6; f++) if (faces & (1 << f)) links[f] |= faces;
	}
}

static int compare(const char* what) {
	cc_uint16 pass[256];
	unsigned char a[6], b[6];
	int y, z, x;
	for (y = 0; y < 16; y++) for (z = 0; z < 16; z++) {
		int m = 0;
		for (x = 0; x < 16; x++) if (cells[y][z][x]) m |= 1 << x;
		pass[y * 16 + z] = m;
	}
	reference(a);
	CaveCull_CalcLinks(pass, b);
	CHECK(!memcmp(a, b, 6), what);
	return !memcmp(a, b, 6);
}

int main(void) {
	int i, x, y, z;
	unsigned seed = 1;
	/* the face order the renderer relies on */
	CHECK(FACE_XMIN == 0 && FACE_XMAX == 1 && FACE_ZMIN == 2 && FACE_ZMAX == 3 && FACE_YMIN == 4 && FACE_YMAX == 5, "face order");

	/* all solid, all see-through */
	memset(cells, 0, sizeof cells); compare("all solid");
	memset(cells, 1, sizeof cells); compare("all see-through");
	{ cc_uint16 p[256]; unsigned char l[6]; memset(p, 0xFF, sizeof p); CaveCull_CalcLinks(p, l);
	  CHECK(l[0] == 0x3F && l[5] == 0x3F, "all see-through links every face"); }

	/* a straight tunnel along X at y=8, z=8: XMIN <-> XMAX only */
	memset(cells, 0, sizeof cells); for (x = 0; x < 16; x++) cells[8][8][x] = 1;
	compare("tunnel x");
	{ cc_uint16 p[256]; unsigned char l[6]; memset(p, 0, sizeof p); p[8 * 16 + 8] = 0xFFFF; CaveCull_CalcLinks(p, l);
	  CHECK(l[FACE_XMIN] == (FACE_BIT_XMIN | FACE_BIT_XMAX) && l[FACE_YMIN] == 0, "tunnel links only its ends"); }
	/* a sealed room in the middle: no links */
	memset(cells, 0, sizeof cells);
	for (y = 4; y < 12; y++) for (z = 4; z < 12; z++) for (x = 4; x < 12; x++) cells[y][z][x] = 1;
	compare("sealed room");
	/* a winding path: up the Y edge, along Z, across X (needs many row passes) */
	memset(cells, 0, sizeof cells);
	for (y = 0; y < 16; y++) cells[y][0][0] = 1;
	for (z = 0; z < 16; z++) cells[15][z][0] = 1;
	for (x = 0; x < 16; x++) cells[15][15][x] = 1;
	compare("winding path");
	/* a zigzag in a row: x cells joined only through other rows */
	memset(cells, 0, sizeof cells);
	for (x = 0; x < 16; x += 2) { cells[0][0][x] = 1; cells[0][1][x] = 1; if (x < 15) cells[0][1][x + 1] = 1; }
	compare("zigzag");

	/* random chunks, see-through share from 5% to 95% */
	for (i = 0; i < 3000; i++) {
		int share = 5 + (i % 19) * 5;
		for (y = 0; y < 16; y++) for (z = 0; z < 16; z++) for (x = 0; x < 16; x++) {
			seed = seed * 1103515245u + 12345u;
			cells[y][z][x] = (int)((seed >> 16) % 100) < share;
		}
		if (!compare("random chunk") && fails < 3) fprintf(stderr, "  (random %d, %d%%)\n", i, share);
	}
	printf("test_cavecull: %d checks, %d failed\n", checks, fails);
	return fails != 0;
}
