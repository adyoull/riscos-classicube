/* Mouse look on RISC OS (src/Window_SDL2.c, RISC OS branch, compiled for
   ARM Linux) with SDL calls stubbed here. riscos-mesa's SDL (devkit 18a and
   later) holds the pointer in relative mode while the window has the
   input focus. Checks: mouse look turns relative mode on and off; the
   title bar says "Escape frees the mouse" only while the mouse is held,
   alongside the game's own title; the view turns by SDL's xrel/yrel;
   nothing moves the pointer each frame (the driver re-centres it). */
#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "Core.h"
#include "String_.h"
#include "Window.h"
#include "Input.h"

/* ---- what Window_SDL2.c reaches ---- */
struct _InputState Input;
char PointerEvents[4096];
static int moves; static float movedX, movedY;
void Event_RaiseRawMove(void* handlers, float x, float y) { moves++; movedX += x; movedY += y; }
void Pointer_SetPosition(int idx, int x, int y) { }
void Event_RaiseVoid(void* h) { }
void Event_RaiseInt(void* h, int a) { }
int String_EncodeUtf8(void* data, const cc_string* src) {
	char* dst = (char*)data; int i;
	for (i = 0; i < src->length; i++) dst[i] = src->buffer[i];
	dst[i] = '\0'; return i;
}
size_t SDL_strlcpy(char* d, const char* s, size_t n) { snprintf(d, n, "%s", s); return strlen(s); }
size_t SDL_strlcat(char* d, const char* s, size_t n) { size_t l = strlen(d); snprintf(d + l, n - l, "%s", s); return l + strlen(s); }

static SDL_Event queue[8]; static int queued, taken;
int SDL_PollEvent(SDL_Event* e) {
	if (taken >= queued) { queued = taken = 0; return 0; }
	*e = queue[taken++]; return 1;
}
static int relative = -1, relativeCalls, warps;
static char title[512];
static int dummyWindow;
int  SDL_SetRelativeMouseMode(SDL_bool on) { relative = on; relativeCalls++; return 0; }
void SDL_WarpMouseInWindow(SDL_Window* w, int x, int y) { warps++; }
Uint32 SDL_GetMouseState(int* x, int* y) { if (x) *x = 0; if (y) *y = 0; return 0; }
void SDL_SetWindowTitle(SDL_Window* w, const char* t) { snprintf(title, sizeof(title), "%s", t); }
SDL_Window* SDL_CreateWindow(const char* t, int x, int y, int w, int h, Uint32 f) { return (SDL_Window*)&dummyWindow; }
void SDL_GetWindowSize(SDL_Window* win, int* w, int* h) { *w = 640; *h = 360; }
SDL_Surface* SDL_CreateRGBSurfaceFrom(void* p, int w, int h, int d, int pitch, Uint32 r, Uint32 g, Uint32 b, Uint32 a) { return NULL; }
void SDL_SetWindowIcon(SDL_Window* w, SDL_Surface* s) { }

static void Motion(int xrel, int yrel) {
	SDL_Event* e = &queue[queued++];
	memset(e, 0, sizeof(*e));
	e->type = SDL_MOUSEMOTION; e->motion.xrel = xrel; e->motion.yrel = yrel;
}
static void Run(void) { moves = 0; movedX = movedY = 0; Window_ProcessEvents(0.0f); }

static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define HINT "Escape frees the mouse"

int main(void) {
	cc_string name  = String_FromConst("ClassiCube 1.3.7 (player)");
	cc_string other = String_FromConst("ClassiCube 1.3.7 (other)");
	Window_Create2D(640, 360);
	Window_Main.Focused = true;
	Window_SetTitle(&name);
	CHECK(strcmp(title, "ClassiCube 1.3.7 (player)") == 0, "title as set: '%s'", title);

	/* playing: held, title says how to get out */
	Window_EnableRawMouse();
	CHECK(relative == 1, "relative mode on");
	CHECK(Input.RawMode, "raw mode on");
	CHECK(strstr(title, "ClassiCube 1.3.7 (player)") == title && strstr(title, HINT), "title while held: '%s'", title);
	warps = 0;
	Window_UpdateRawMouse(); Window_UpdateRawMouse();
	CHECK(warps == 0, "nothing moves the pointer each frame (%d)", warps);
	Motion(7, -3); Motion(5, 1); Run();
	CHECK(moves == 2 && movedX == 12 && movedY == -2, "view turns by xrel/yrel (%d, %g,%g)", moves, movedX, movedY);

	/* a new title while held keeps the hint */
	Window_SetTitle(&other);
	CHECK(strstr(title, "(other)") && strstr(title, HINT), "renamed while held: '%s'", title);

	/* Escape: let go, hint gone */
	Window_DisableRawMouse();
	CHECK(relative == 0, "relative mode off");
	CHECK(!Input.RawMode, "raw mode off");
	CHECK(strcmp(title, "ClassiCube 1.3.7 (other)") == 0, "title after Escape: '%s'", title);
	Motion(9, 9); Run();
	CHECK(moves == 0, "no turning once let go (%d)", moves);

	/* back to game */
	Window_EnableRawMouse();
	CHECK(relative == 1 && strstr(title, HINT), "held again: '%s'", title);
	Window_DisableRawMouse();
	CHECK(relativeCalls == 4, "relative mode set 4 times (%d)", relativeCalls);

	printf("test_mousegrab: %d checks, %d failed\n", checks, fails);
	return fails != 0;
}
