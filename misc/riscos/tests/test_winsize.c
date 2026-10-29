/* The game window's default size on RISC OS (Window_RISCOS_DefaultSize in
   src/Window_SDL2.c): the real file compiled for ARM Linux with the RISC OS
   branch on. The game draws at ClassiCube$RenderSize and EGL scales it to
   the window, so the window is the largest of 3x, 2x, 1.5x, 1x the render
   size that fits on the desktop (less 40 x 140 pixels for the title bar and
   icon bar); ClassiCube$WindowSize overrides; "full" keeps 854x480. */
#include <stdio.h>
#include <stdlib.h>
#include "Core.h"
#include "Window.h"

/* what Window_SDL2.c reaches that these checks don't use */
char Input[4096], PointerEvents[4096];

static int fails, checks;
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
	printf("test_winsize: %d checks, %d failed\n", checks, fails);
	return fails != 0;
}
