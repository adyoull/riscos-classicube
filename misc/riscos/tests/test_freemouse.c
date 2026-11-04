/* Mouse look on RISC OS without holding the pointer (src/Window_SDL2.c,
   RISC OS branch, compiled for ARM Linux): Window_ProcessEvents fed with
   SDL events from a queue here. Checks: mouse look never asks SDL for
   relative mode or moves the pointer; it hides the pointer (SDL hides it
   only over the window) and shows it again; movements turn the view by
   SDL's xrel/yrel; the first movement after mouse look starts, and after
   the pointer comes back into the window, only sets the position (no jump
   by the distance it moved outside); no turning when mouse look is off. */
#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "Core.h"
#include "Window.h"
#include "Input.h"

/* ---- what Window_SDL2.c reaches ---- */
struct _InputState Input;
char PointerEvents[4096];
static int moves; static float movedX, movedY;
void Event_RaiseRawMove(void* handlers, float x, float y) { moves++; movedX += x; movedY += y; }
static int pointerX, pointerY;
void Pointer_SetPosition(int idx, int x, int y) { pointerX = x; pointerY = y; }

static SDL_Event queue[16]; static int queued, taken;
int SDL_PollEvent(SDL_Event* e) {
	if (taken >= queued) { queued = taken = 0; return 0; }
	*e = queue[taken++]; return 1;
}
static int relativeCalls, warps, cursorShown = 1;
int  SDL_SetRelativeMouseMode(SDL_bool on) { relativeCalls++; return 0; }
void SDL_WarpMouseInWindow(SDL_Window* w, int x, int y) { warps++; }
int  SDL_ShowCursor(int toggle) { if (toggle >= 0) cursorShown = toggle; return cursorShown; }

/* ---- the queue ---- */
static void Motion(int x, int y, int xrel, int yrel) {
	SDL_Event* e = &queue[queued++];
	memset(e, 0, sizeof(*e));
	e->type = SDL_MOUSEMOTION;
	e->motion.x = x; e->motion.y = y; e->motion.xrel = xrel; e->motion.yrel = yrel;
}
static void WinEvent(int what) {
	SDL_Event* e = &queue[queued++];
	memset(e, 0, sizeof(*e));
	e->type = SDL_WINDOWEVENT; e->window.event = what;
}
static void Run(void) { moves = 0; movedX = movedY = 0; Window_ProcessEvents(0.0f); }

static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main(void) {
	/* menus open: the pointer moves, nothing turns */
	Motion(100, 100, 5, 5); Run();
	CHECK(moves == 0, "no turning without mouse look (%d)", moves);
	CHECK(pointerX == 100 && pointerY == 100, "pointer position still followed");

	/* playing: pointer hidden over the window, never held or moved */
	Window_EnableRawMouse();
	CHECK(cursorShown == 0, "pointer hidden (over the window) while playing");
	CHECK(Input.RawMode, "raw mode on");
	Motion(300, 200, 200, 100); Run();
	CHECK(moves == 0, "first movement after mouse look starts only sets the position (%d)", moves);
	Motion(310, 196, 10, -4); Motion(312, 197, 2, 1); Run();
	CHECK(moves == 2 && movedX == 12 && movedY == -3, "view turns by xrel/yrel (%d moves, %g,%g)", moves, movedX, movedY);
	Window_UpdateRawMouse(); Window_UpdateRawMouse();

	/* out of the window and back in somewhere else: no jump */
	WinEvent(SDL_WINDOWEVENT_LEAVE); WinEvent(SDL_WINDOWEVENT_ENTER);
	Motion(50, 40, -262, -157); Motion(53, 40, 3, 0); Run();
	CHECK(moves == 1 && movedX == 3 && movedY == 0, "coming back in: only the movement after (%d, %g,%g)", moves, movedX, movedY);
	WinEvent(SDL_WINDOWEVENT_LEAVE);
	Motion(0, 0, -53, -40); Run();
	CHECK(moves == 0, "the movement that left the window isn't turned by either (%d)", moves);
	Motion(1, 1, 1, 1); Run();
	CHECK(moves == 1, "then turning again (%d)", moves);

	/* Escape: pointer back, no turning */
	Window_DisableRawMouse();
	CHECK(cursorShown == 1, "pointer shown again");
	CHECK(!Input.RawMode, "raw mode off");
	Motion(5, 5, 4, 4); Run();
	CHECK(moves == 0, "no turning after mouse look stops (%d)", moves);

	/* back to game: the first movement only sets the position again */
	Window_EnableRawMouse();
	Motion(600, 300, 595, 295); Motion(601, 300, 1, 0); Run();
	CHECK(moves == 1 && movedX == 1, "after Back to game: no jump (%d, %g)", moves, movedX);
	Window_DisableRawMouse();

	CHECK(relativeCalls == 0, "SDL relative mode never used (%d)", relativeCalls);
	CHECK(warps == 0, "the pointer is never moved (%d warps)", warps);
	printf("test_freemouse: %d checks, %d failed\n", checks, fails);
	return fails != 0;
}
