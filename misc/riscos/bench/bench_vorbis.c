/* Benchmark and reference for ClassiCube's Vorbis decoder (src/Vorbis.c):
   decodes a whole .ogg from memory, writes the 16-bit PCM to a file (to
   compare builds sample for sample) and prints the seconds decoded.
   Built for ARM Linux and run under qemu-arm (with insnprof.c for exact
   instruction counts):  bench_vorbis IN.ogg OUT.pcm
     S=src; arm-linux-gnueabihf-gcc -O2 -mfpu=vfpv3 -mfloat-abi=hard \
       -fno-math-errno -no-pie -I$S -DCC_GFX_BACKEND=CC_GFX_BACKEND_GL1 \
       -DCC_WIN_BACKEND=CC_WIN_BACKEND_TERMINAL [-DCC_BUILD_NEON] \
       misc/riscos/bench/bench_vorbis.c misc/riscos/bench/stubs.c $S/Vorbis.c \
       $S/Stream.c $S/ExtMath.c $S/Utils.c -o bench_vorbis -lm \
       -Wl,--unresolved-symbols=ignore-all
     qemu-arm -L /usr/arm-linux-gnueabihf -cpu cortex-a8 \
       [-plugin misc/riscos/perf/out-plugin/libinsnprof.so] bench_vorbis IN.ogg OUT.pcm
   With the plugin: "INSNS n" at the end, and per-block counts in $INSNPROF. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Core.h"
#include "Vorbis.h"
#include "Stream.h"

int main(int argc, char** argv) {
	static struct VorbisState vorbis; static struct OggState ogg;
	static cc_int16 pcm[VORBIS_MAX_BLOCK_SIZE * VORBIS_MAX_CHANS];
	struct Stream mem; FILE* f; long len; cc_uint8* data; long long total = 0;
	cc_result res;
	if (argc < 3) { fprintf(stderr, "usage: bench_vorbis IN.ogg OUT.pcm\n"); return 2; }
	f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); len = ftell(f); fseek(f, 0, SEEK_SET);
	data = malloc(len); fread(data, 1, len, f); fclose(f);

	Stream_ReadonlyMemory(&mem, data, len);
	Ogg_Init(&ogg, &mem);
	Vorbis_Init(&vorbis);
	vorbis.source = &ogg;
	if ((res = Vorbis_DecodeHeaders(&vorbis))) { fprintf(stderr, "headers: %x\n", res); return 1; }

	f = fopen(argv[2], "wb");
	for (;;) {
		int n;
		if (Vorbis_DecodeFrame(&vorbis)) break;
		n = Vorbis_OutputFrame(&vorbis, pcm);
		fwrite(pcm, 2, n, f); total += n;
	}
	fclose(f);
	printf("decoded %.2f s (%d Hz, %d channels)\n", (double)total / vorbis.channels / vorbis.sampleRate, vorbis.sampleRate, vorbis.channels);
	Vorbis_Free(&vorbis);
	return 0;
}
