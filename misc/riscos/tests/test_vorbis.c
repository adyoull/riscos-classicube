/* NEON music decoding on RISC OS (src/Vorbis.c, CC_BUILD_NEON): the real
   file included here and compiled for ARM Linux with the RISC OS branch on,
   against riscos-crossdev's fake SWIs, run with alignment traps on.
   Checks:
    - NEON is used only when VFPSupport_Features says the CPU has it (MVFR1),
      not when the SWI fails (no VFPSupport) or ClassiCube$NoNEON is set
    - three streams (stereo 44.1 kHz with channel coupling, mono 22.05 kHz,
      stereo 48 kHz at a high bitrate) decode with and without NEON to the
      same samples, bit for bit, as upstream's decoder (CRC32s taken from
      the unchanged upstream Vorbis.c) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fake.h"
#include "../../../src/Vorbis.c"
#include "../../../src/Stream.h"

/* ---- what Vorbis.c reaches of the rest of ClassiCube ---- */
void* Mem_TryAlloc(cc_uint32 n, cc_uint32 size) { return malloc((size_t)n * size); }
void* Mem_TryAllocCleared(cc_uint32 n, cc_uint32 size) { return calloc(n, size); }
void* Mem_Alloc(cc_uint32 n, cc_uint32 size, const char* place) { return malloc((size_t)n * size); }
void  Mem_Free(void* mem) { free(mem); }
void* Mem_Set(void* dst, cc_uint8 value, unsigned n) { return memset(dst, value, n); }
void* Mem_Copy(void* dst, const void* src, unsigned n) { return memcpy(dst, src, n); }

static int fails, checks;
#undef  CHECK /* fake.h's, without printf arguments */
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* ---- fake VFPSupport ---- */
#define VFPSupport_Features 0x58EC8
static unsigned mvfr1; static int noVFP, featureCalls;
static const _kernel_oserror noSWI = { 0x1E6, "SWI not known" };
static const _kernel_oserror* swi_features(int swi, int r[10]) {
	featureCalls++;
	if (noVFP) return &noSWI;
	CHECK(r[0] == 0, "VFPSupport_Features reason 0 (R0 = %d)", r[0]);
	r[0] = 0x11111111; r[1] = 0x10110222; r[2] = mvfr1; /* FPSID, MVFR0, MVFR1 */
	return NULL;
}
static int HasNEON(unsigned features, int fail, const char* noNEON) {
	fake_reset(); fake_swi(VFPSupport_Features, "VFPSupport_Features", swi_features);
	mvfr1 = features; noVFP = fail; featureCalls = 0;
	if (noNEON) setenv("ClassiCube$NoNEON", noNEON, 1); else unsetenv("ClassiCube$NoNEON");
	vorbis_neon = -1;
	return Vorbis_HasNEON();
}

/* ---- decoding ---- */
static cc_uint32 Crc32(cc_uint32 crc, const cc_uint8* p, int n) {
	int k; crc = ~crc;
	while (n--) { crc ^= *p++; for (k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1)); }
	return ~crc;
}
static cc_uint32 Decode(const char* path, int neon, int* samples, int* rate, int* chans) {
	static struct VorbisState vorbis; static struct OggState ogg;
	static cc_int16 pcm[VORBIS_MAX_BLOCK_SIZE * VORBIS_MAX_CHANS];
	struct Stream mem; FILE* f; long len; cc_uint8* data; cc_uint32 crc = 0;
	*samples = 0; *rate = 0; *chans = 0;
	if (!(f = fopen(path, "rb"))) { CHECK(0, "can't open %s", path); return 0; }
	fseek(f, 0, SEEK_END); len = ftell(f); fseek(f, 0, SEEK_SET);
	data = malloc(len);
	if (fread(data, 1, len, f) != (size_t)len) CHECK(0, "can't read %s", path);
	fclose(f);

	vorbis_neon = neon;
	Stream_ReadonlyMemory(&mem, data, len);
	Ogg_Init(&ogg, &mem);
	Vorbis_Init(&vorbis);
	vorbis.source = &ogg;
	CHECK(!Vorbis_DecodeHeaders(&vorbis), "%s: headers", path);
	while (!Vorbis_DecodeFrame(&vorbis)) {
		int n = Vorbis_OutputFrame(&vorbis, pcm);
		crc = Crc32(crc, (const cc_uint8*)pcm, n * 2); *samples += n;
	}
	*rate = vorbis.sampleRate; *chans = vorbis.channels;
	Vorbis_Free(&vorbis); free(data);
	return crc;
}

/* The C inverse coupling, as in Vorbis_DecodeFrame */
static void UncoupleC(float* mag, float* ang, int count) {
	int j; float m, a;
	for (j = 0; j < count; j++) {
		m = mag[j]; a = ang[j];
		if (m > 0.0f) {
			if (a > 0.0f) { ang[j] = m - a; } else { ang[j] = m; mag[j] = m + a; }
		} else {
			if (a > 0.0f) { ang[j] = m + a; } else { ang[j] = m; mag[j] = m - a; }
		}
	}
}

/* Every pair of awkward values (+-0, the smallest and largest normal
   floats, infinities) through NEON and C, compared bit for bit: streams
   rarely have a zero magnitude with a non-zero angle, or infinities.
   Not denormals or NaNs: NEON arithmetic always flushes denormals to zero
   and gives the default NaN. Residue values are codebook values, never
   below 1e-38, and a valid stream gives no NaNs (and either way the sample
   comes out as 0 or clamped, the same as C). */
static void CheckUncouple(void) {
	static const cc_uint32 bits[] = { 0x00000000, 0x80000000, 0x00800000, 0x80800000,
		0x3F800000, 0xBF800000, 0x3EAAAAAB, 0xC2C80000, 0x7F7FFFFF, 0xFF7FFFFF,
		0x7F800000, 0xFF800000 };
	enum { N = sizeof(bits) / sizeof(bits[0]) };
	static union { float f[N * N]; cc_uint32 u[N * N]; } m1, a1, m2, a2;
	int i, j, bad = 0;
	for (i = 0; i < N; i++)
		for (j = 0; j < N; j++) { m1.u[i * N + j] = bits[i]; a1.u[i * N + j] = bits[j]; }
	m2 = m1; a2 = a1;
	UncoupleC(m1.f, a1.f, N * N);
	Vorbis_UncoupleNEON(m2.f, a2.f, N * N);
	for (i = 0; i < N * N; i++) {
		if (m1.u[i] == m2.u[i] && a1.u[i] == a2.u[i]) continue;
		if (bad++ < 4) printf("  m=%08x a=%08x: C %08x %08x, NEON %08x %08x\n", bits[i / N], bits[i % N], m1.u[i], a1.u[i], m2.u[i], a2.u[i]);
	}
	CHECK(bad == 0, "inverse coupling: NEON = C for awkward values (%d differ)", bad);
}

static const struct { const char* file; int samples, rate, chans; cc_uint32 crc; } streams[] = {
	{ "data/stereo44.ogg", 353024, 44100, 2, 0xbb26467d },
	{ "data/mono22.ogg",    66560, 22050, 1, 0x2644bb27 },
	{ "data/stereo48.ogg", 192000, 48000, 2, 0xa19fefd5 },
};

int main(int argc, char** argv) {
	int i, neon;
	/* NEON: MVFR1 bits 8-19 = single precision, integer, load/store all 1 */
	CHECK(HasNEON(0x11111111, 0, NULL) == 1, "Cortex-A8/A53/A72 MVFR1: NEON");
	CHECK(featureCalls == 1, "VFPSupport_Features called once (%d)", featureCalls);
	Vorbis_HasNEON(); Vorbis_HasNEON();
	CHECK(featureCalls == 1, "and remembered (%d calls)", featureCalls);
	CHECK(HasNEON(0x01111111 & ~0xFFF00, 0, NULL) == 0, "VFP only (no Advanced SIMD): no NEON");
	CHECK(HasNEON(0x11101111, 0, NULL) == 0, "no single precision SIMD: no NEON");
	CHECK(HasNEON(0x11110111, 0, NULL) == 0, "no integer SIMD: no NEON");
	CHECK(HasNEON(0x11111011, 0, NULL) == 0, "no SIMD load/store: no NEON");
	CHECK(HasNEON(0x00011100, 0, NULL) == 1, "only the three SIMD fields: NEON");
	CHECK(HasNEON(0x11121111, 0, NULL) == 0, "an unknown SIMD level: no NEON");
	CHECK(HasNEON(0x11111111, 1, NULL) == 0, "no VFPSupport module: no NEON");
	CHECK(HasNEON(0x11111111, 0, "1") == 0,  "ClassiCube$NoNEON set: no NEON");

	CheckUncouple();
	for (i = 0; i < (int)(sizeof(streams) / sizeof(streams[0])); i++) {
		for (neon = 0; neon <= 1; neon++) {
			int samples, rate = 0, chans = 0;
			cc_uint32 crc = Decode(streams[i].file, neon, &samples, &rate, &chans);
			CHECK(rate == streams[i].rate && chans == streams[i].chans, "%s: %d Hz, %d channels", streams[i].file, rate, chans);
			CHECK(samples == streams[i].samples, "%s %s: %d samples, not %d", streams[i].file, neon ? "NEON" : "C", samples, streams[i].samples);
			CHECK(crc == streams[i].crc, "%s %s: CRC32 %08x, not %08x (upstream)", streams[i].file, neon ? "NEON" : "C", crc, streams[i].crc);
		}
	}
	printf("test_vorbis: %d checks, %d failed\n", checks, fails);
	return fails != 0;
}
