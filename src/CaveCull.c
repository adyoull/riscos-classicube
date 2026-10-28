#include "CaveCull.h"
#ifdef CC_BUILD_CAVECULL
#include "Constants.h"
#include "Funcs.h"
/* Copyright 2014-2025 ClassiCube | Licensed under BSD-3 */
/* (RISC OS port: cave culling for software rendering) */

#define LINK_ROWS (CHUNK_SIZE * CHUNK_SIZE)
static cc_uint16 link_seen[LINK_ROWS];   /* cells already in a region */
static cc_uint16 link_reg[LINK_ROWS];    /* cells of the region being filled */
static cc_uint8  link_queued[LINK_ROWS];
static cc_uint16 link_stack[LINK_ROWS], link_rows[LINK_ROWS];

/* Grows seed along its row through the see-through cells in pass */
static cc_uint16 Link_Spread(cc_uint32 seed, cc_uint32 pass) {
	cc_uint32 prev;
	do {
		prev = seed;
		seed = (seed | (seed << 1) | (seed >> 1)) & pass;
	} while (seed != prev);
	return (cc_uint16)seed;
}

/* Flood fills each see-through region a row (16 cells) at a time: along */
/*  a row with shifts, into the rows next to it (z +-1, y +-1) with masks */
void CaveCull_CalcLinks(const cc_uint16* pass, cc_uint8* links) {
	int row, top, nrows, faces, f, i, zz, yy, n;
	cc_uint32 free, add, m;
	for (f = 0; f < FACE_COUNT; f++) links[f] = 0;
	for (row = 0; row < LINK_ROWS; row++) link_seen[row] = 0;

	for (row = 0; row < LINK_ROWS; row++)
	{
		while ((free = pass[row] & ~link_seen[row]))
		{
			/* a new region, from the lowest free cell of this row */
			faces = 0; top = 0; nrows = 0;
			link_reg[row] = Link_Spread(free & (0u - free), pass[row]);
			link_rows[nrows++] = row; link_stack[top++] = row; link_queued[row] = 1;

			while (top)
			{
				i = link_stack[--top]; link_queued[i] = 0;
				m = link_reg[i]; zz = i & CHUNK_MASK; yy = i >> CHUNK_SHIFT;
				if (m & 1)                faces |= FACE_BIT_XMIN;
				if (m & (1 << CHUNK_MAX)) faces |= FACE_BIT_XMAX;

				/* into a neighbouring row: the cells next to this row's */
				#define Link_Row(cond, next, face) \
				if (!(cond)) { faces |= (face); } else { n = next; \
					add = m & pass[n] & ~link_reg[n]; \
					if (add) { \
						if (!link_reg[n]) link_rows[nrows++] = n; \
						link_reg[n] = Link_Spread(add | link_reg[n], pass[n]); \
						if (!link_queued[n]) { link_queued[n] = 1; link_stack[top++] = n; } \
					} \
				}
				Link_Row(zz > 0,         i - 1,          FACE_BIT_ZMIN);
				Link_Row(zz < CHUNK_MAX, i + 1,          FACE_BIT_ZMAX);
				Link_Row(yy > 0,         i - CHUNK_SIZE, FACE_BIT_YMIN);
				Link_Row(yy < CHUNK_MAX, i + CHUNK_SIZE, FACE_BIT_YMAX);
			}

			/* every face this region touches can be seen from every other */
			for (f = 0; f < FACE_COUNT; f++)
			{
				if (faces & (1 << f)) links[f] |= faces;
			}
			for (i = 0; i < nrows; i++)
			{
				n = link_rows[i];
				link_seen[n] |= link_reg[n]; link_reg[n] = 0;
			}
		}
	}
}
#endif
