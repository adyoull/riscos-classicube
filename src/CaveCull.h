#ifndef CC_CAVECULL_H
#define CC_CAVECULL_H
#include "Core.h"
CC_BEGIN_HEADER

/*
Cave culling: which faces of a 16x16x16 chunk can see each other through
blocks that aren't fully opaque. The map renderer floods out from the
camera through these links and skips chunks it can't reach (caves deep
underground, the far side of hills). As Minecraft's "advanced culling".
*/

/* pass: the chunk's see-through cells, as 256 rows of 16 cells along X */
/*  (row = y * 16 + z, bit x). links[face] gets a bit for each face that */
/*  face connects to (FACE_BIT_*). */
void CaveCull_CalcLinks(const cc_uint16* pass, cc_uint8* links);

CC_END_HEADER
#endif
