/* Mouse look on RISC OS (src/Window_SDL2.c, CC_BUILD_RISCOS): the real
   file compiled for ARM Linux with the RISC OS branch on, against
   riscos-crossdev's fake SWIs; the few SDL calls and ClassiCube globals it
   reaches are stubbed here. Checks: movement read with OS_Word 21,4 and
   sent as pixels (y down), the mouse put back in the middle of the game's
   window with OS_Word 21,3, a window that moves, full screen (the screen's
   middle), and a window that's gone (follow the pointer instead). */
#include <stdio.h>
#include <string.h>
#include "fake.h"

/* ---- what Window_SDL2.c reaches ---- */
char Input[4096], PointerEvents[4096];          /* only written to here */
static int events; static float evX, evY;
void Event_RaiseRawMove(void* handlers, float x, float y) { events++; evX += x; evY += y; }
static unsigned winFlags; static int cursorShown = 1;
void* SDL_GetMouseFocus(void) { return 0; }       /* = win_handle (never created here) */
unsigned SDL_GetWindowFlags(void* w) { return winFlags; }
int SDL_ShowCursor(int toggle) { cursorShown = toggle; return 0; }
void Window_EnableRawMouse(void); void Window_UpdateRawMouse(void); void Window_DisableRawMouse(void);

/* ---- fake RISC OS ---- */
#define OS_Word             0x07
#define OS_ReadModeVariable 0x35
#define Wimp_GetWindowState 0x400CB
#define Wimp_GetPointerInfo 0x400CF
static int mouseX, mouseY, setX, setY, sets, winX0, winY0, winX1, winY1, winGone, pointerWindow = 0x5A5A;
static const _kernel_oserror badWindow = { 0x288, "Illegal window handle" };

static const _kernel_oserror* swi_word(int swi, int r[10]) {
	unsigned char* b = (unsigned char*)r[1];
	CHECK(r[0] == 21, "only OS_Word 21");
	if (b[0] == 4) { b[1] = mouseX & 0xFF; b[2] = (mouseX >> 8) & 0xFF; b[3] = mouseY & 0xFF; b[4] = (mouseY >> 8) & 0xFF; }
	else if (b[0] == 3) { setX = (short)(b[1] | b[2] << 8); setY = (short)(b[3] | b[4] << 8); mouseX = setX; mouseY = setY; sets++; }
	else CHECK(0, "OS_Word 21: only reasons 3 and 4");
	return NULL;
}
static const _kernel_oserror* swi_modevar(int swi, int r[10]) {
	switch (r[1]) { case 4: case 5: r[2] = 1; break; case 11: r[2] = 1919; break; case 12: r[2] = 1079; break; default: r[2] = 0; }
	return NULL;
}
static const _kernel_oserror* swi_state(int swi, int r[10]) {
	int* b = (int*)r[1];
	if (winGone) return &badWindow;
	CHECK(b[0] == pointerWindow, "state of the window found under the pointer");
	b[1] = winX0; b[2] = winY0; b[3] = winX1; b[4] = winY1; return NULL;
}
static const _kernel_oserror* swi_pointer(int swi, int r[10]) {
	int* b = (int*)r[1]; b[0] = mouseX; b[1] = mouseY; b[2] = 0; b[3] = pointerWindow; b[4] = 0; return NULL;
}

int main(void) {
	fake_reset();
	fake_swi(OS_Word, "OS_Word", swi_word);
	fake_swi(OS_ReadModeVariable, "OS_ReadModeVariable", swi_modevar);
	fake_swi(Wimp_GetWindowState, "Wimp_GetWindowState", swi_state);
	fake_swi(Wimp_GetPointerInfo, "Wimp_GetPointerInfo", swi_pointer);
	winX0 = 200; winY0 = 100; winX1 = 1000; winY1 = 700;   /* centre 600,400 */
	mouseX = 650; mouseY = 420;

	Window_EnableRawMouse();
	CHECK(cursorShown == 0, "pointer hidden");
	Window_UpdateRawMouse();
	CHECK(events == 0, "first frame: no movement yet");
	CHECK(sets == 1 && setX == 600 && setY == 400, "mouse put in the window's middle");

	mouseX += 20; mouseY -= 10;                 /* right 20, down 10 OS units */
	Window_UpdateRawMouse();
	CHECK(events == 1 && evX == 10 && evY == 5, "movement in pixels, y down");
	CHECK(setX == 600 && setY == 400, "back to the middle");

	Window_UpdateRawMouse();
	CHECK(events == 1, "no movement, no event");

	winX0 += 100; winX1 += 100;                 /* window dragged right */
	mouseX = 600; mouseY = 400;
	Window_UpdateRawMouse();
	CHECK(setX == 700 && setY == 400, "follows the window");

	winFlags = 0x00000001;                      /* SDL_WINDOW_FULLSCREEN */
	Window_UpdateRawMouse();
	CHECK(setX == 1920 && setY == 1080, "full screen: the screen's middle (OS units)");
	winFlags = 0x00001001;                      /* FULLSCREEN_DESKTOP: a desktop window */
	Window_UpdateRawMouse();
	CHECK(setX == 700 && setY == 400, "full window: the window's middle");

	winGone = 1; events = 0; evX = evY = 0; sets = 0;
	mouseX = 700; mouseY = 400; Window_UpdateRawMouse();
	mouseX += 8; Window_UpdateRawMouse();
	CHECK(sets == 0, "window gone: mouse left alone");
	CHECK(events == 1 && evX == 4, "and movement still read");

	Window_DisableRawMouse();
	CHECK(cursorShown == 1, "pointer shown again");
	return fake_done("test_rawmouse");
}
