/* The game window's default size on RISC OS (Window_RISCOS_DefaultSize in
   src/Window_SDL2.c): the real file compiled for ARM Linux with the RISC OS
   branch on, against riscos-crossdev's fake SWIs. The game draws at
   ClassiCube$RenderSize; with an overlay possible (VideoOverlay loaded,
   ClassiCube$Overlay not 0, EGL$Overlay not off) the display stretches it
   for nothing, so the window is the largest of 3x, 2x, 1.5x, 1x the render
   size that fits on the desktop (less 40 x 140 pixels for the title bar and
   icon bar). Without an overlay, or drawing at full size: 854x480.
   ClassiCube$WindowSize overrides. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "Core.h"
#include "Window.h"
#include "fake.h"

/* what Window_SDL2.c reaches that these checks don't use */
char Input[4096], PointerEvents[4096];
int SDL_strcasecmp(const char* a, const char* b) { return strcasecmp(a, b); }
/* getenv as UnixLib's is for a name with a '$': one buffer, freed and
   remade at each call. The old value is scribbled on first, so code that
   keeps a value past the next getenv reads rubbish here (riscos-mesa found
   ClassiCube doing that, 2026-11-13). */
extern char** environ;
static char* ul_buf;
char* getenv(const char* name) {
	size_t n = strlen(name); char** e;
	if (ul_buf) { memset(ul_buf, '#', strlen(ul_buf)); free(ul_buf); ul_buf = NULL; }
	for (e = environ; *e; e++)
		if (!strncmp(*e, name, n) && (*e)[n] == '=') { ul_buf = strdup(*e + n + 1); return ul_buf; }
	return NULL;
}
size_t SDL_strlcpy(char* d, const char* s, size_t n) { size_t l = strlen(s); if (n) { size_t c = l < n - 1 ? l : n - 1; memcpy(d, s, c); d[c] = 0; } return l; }
int SDL_strcmp(const char* a, const char* b) { return strcmp(a, b); }

/* fake OS_Module 18 (look up module): VideoOverlay loaded or not */
#define OS_Module 0x1E
static int haveOverlayModule = 1, lookups;
static const _kernel_oserror notFound = { 0x102, "Module 'VideoOverlay' not found" };
static const _kernel_oserror* swi_module(int swi, int r[10]) {
	lookups++;
	if (r[0] != 18 || strcmp((const char*)r[1], "VideoOverlay")) return &notFound;
	return haveOverlayModule ? NULL : &notFound;
}

static int fails, checks;
#define CHECK2(c, what) do { checks++; if (!(c)) { fails++; printf("FAIL %s\n", what); } } while (0)
static void Expect(int dw, int dh, const char* render, const char* window, int ew, int eh) {
	int w = -1, h = -1;
	DisplayInfo.Width = dw; DisplayInfo.Height = dh;
	if (render) setenv("ClassiCube$RenderSize", render, 1); else unsetenv("ClassiCube$RenderSize");
	if (window) setenv("ClassiCube$WindowSize", window, 1); else unsetenv("ClassiCube$WindowSize");
	Window_RISCOS_DefaultSize(&w, &h);
	checks++;
	if (w != ew || h != eh) {
		fails++;
		printf("FAIL desktop %dx%d, render %s, window %s: %dx%d, not %dx%d\n",
			dw, dh, render ? render : "-", window ? window : "-", w, h, ew, eh);
	}
}

int main(void) {
	fake_reset();
	fake_swi(OS_Module, "OS_Module", swi_module);
	/* default render size 640x360 */
	Expect(1920, 1080, NULL, NULL, 1280,  720); /* 3x = 1920x1080 doesn't fit */
	Expect(2560, 1440, NULL, NULL, 1920, 1080); /* 3x */
	Expect(3840, 2160, NULL, NULL, 1920, 1080); /* 3x is the most */
	Expect(1280, 1024, NULL, NULL,  960,  540); /* 2x too wide: 1.5x */
	Expect(1320,  860, NULL, NULL, 1280,  720); /* 2x exactly fits: 1280 x 720 */
	Expect(1319, 1000, NULL, NULL,  960,  540); /* one pixel too narrow for 2x */
	Expect(1600,  859, NULL, NULL,  960,  540); /* one pixel too low for 2x */
	Expect(1024,  768, NULL, NULL,  960,  540);
	Expect( 800,  600, NULL, NULL,  640,  360); /* 1x */
	Expect( 640,  480, NULL, NULL,  640,  360); /* doesn't fit even at 1x: 1x */
	Expect(1920, 1080, "",   NULL, 1280,  720); /* empty = default */
	/* other render sizes */
	Expect(1920, 1080, "480x270", NULL, 1440, 810); /* 3x */
	Expect(1920, 1080, "800x600", NULL, 1200, 900); /* 1.5x */
	Expect(1920, 1200, "800X450", NULL, 1600, 900); /* 2x, capital X */
	/* drawing at full size, or a render size that isn't WxH: upstream's */
	Expect(1920, 1080, "full",    NULL, 854, 480);
	Expect(1920, 1080, "FULL",    NULL, 854, 480);
	Expect(1920, 1080, "640",     NULL, 854, 480);
	Expect(1920, 1080, "640x",    NULL, 854, 480);
	Expect(1920, 1080, "640x360z",NULL, 854, 480);
	Expect(1920, 1080, "10x10",   NULL, 854, 480);
	/* ClassiCube$WindowSize wins (when it's WxH) */
	Expect(1920, 1080, NULL, "1600x900", 1600, 900);
	Expect(1920, 1080, "full", "1024x576", 1024, 576);
	Expect(1920, 1080, NULL, "big",      1280, 720);
	Expect(1920, 1080, NULL, "",         1280, 720);
	CHECK2(lookups > 0, "VideoOverlay looked up with OS_Module 18");

	/* each variable read on its own, with others set too (UnixLib getenv) */
	setenv("ClassiCube$Overlay", "1", 1); setenv("EGL$Overlay", "on", 1);
	Expect(1920, 1080, "480x270", NULL, 1440, 810);
	setenv("EGL$Overlay", "off", 1);
	Expect(1920, 1080, "480x270", NULL, 854, 480);
	unsetenv("ClassiCube$Overlay"); unsetenv("EGL$Overlay");

	/* no overlay possible: the CPU stretches, so upstream's size */
	haveOverlayModule = 0;
	Expect(1920, 1080, NULL, NULL, 854, 480);              /* VideoOverlay not loaded */
	Expect(1920, 1080, NULL, "1600x900", 1600, 900);       /* the override still wins */
	haveOverlayModule = 1;
	setenv("ClassiCube$Overlay", "0", 1);
	Expect(1920, 1080, NULL, NULL, 854, 480);
	unsetenv("ClassiCube$Overlay");
	setenv("ClassiCube$Overlay", "1", 1);
	Expect(1920, 1080, NULL, NULL, 1280, 720);
	unsetenv("ClassiCube$Overlay");
	{
		static const char* offs[] = { "off", "OFF", "no", "0" };
		static const char* ons[]  = { "on", "yes", "1", "" };
		int i;
		for (i = 0; i < 4; i++) { setenv("EGL$Overlay", offs[i], 1); Expect(1920, 1080, NULL, NULL, 854, 480); }
		for (i = 0; i < 4; i++) { setenv("EGL$Overlay", ons[i], 1);  Expect(1920, 1080, NULL, NULL, 1280, 720); }
		unsetenv("EGL$Overlay");
	}
	printf("test_winsize: %d checks, %d failed\n", checks, fails);
	return fails != 0;
}
