/* What Vorbis.c/Stream.c/Utils.c reach of the rest of ClassiCube, for the
   benchmarks (memory only; file functions are never called) */
#include <stdlib.h>
#include <string.h>
#include "Core.h"
void* Mem_TryAlloc(cc_uint32 n, cc_uint32 size) { return malloc((size_t)n * size); }
void* Mem_TryAllocCleared(cc_uint32 n, cc_uint32 size) { return calloc(n, size); }
void* Mem_Alloc(cc_uint32 n, cc_uint32 size, const char* place) { return malloc((size_t)n * size); }
void  Mem_Free(void* mem) { free(mem); }
void* Mem_Set(void* dst, cc_uint8 value, unsigned n) { return memset(dst, value, n); }
void* Mem_Copy(void* dst, const void* src, unsigned n) { return memcpy(dst, src, n); }
