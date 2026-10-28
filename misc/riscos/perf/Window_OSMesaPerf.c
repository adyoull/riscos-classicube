/* TEST ONLY (not part of the game): a headless "window" for measuring
   ClassiCube's GL1 renderer on riscos-mesa's own libOSMesa.a (the devkit's
   RISC OS build) as an ARM Linux program under qemu-arm (see perf.sh).
   Build with -DCC_WIN_BACKEND=99 -DCC_GFX_BACKEND=CC_GFX_BACKEND_GL1.
   Environment:
     CC_W, CC_H      frame size (default 640x360)
     CC_WARM         frames before measuring (default 150)
     CC_FRAMES       frames measured (default 60), then it quits
     CC_PROF=file    PC samples (SIGPROF, 1 ms of CPU) while measuring
     CC_SHOT=file    the last frame as a PPM
     CC_TURN=deg     camera turn per frame (default 0.5), so frames differ */
#include "Core.h"
#if CC_WIN_BACKEND == 99
#include "_WindowBase.h"
#include "String_.h"
#include "Funcs.h"
#include "Bitmap.h"
#include "Errors.h"
#include "Camera.h"
#include "Game.h"
#include "Entity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <ucontext.h>
#include <GL/osmesa.h>
#include "ExtMath.h"

/* Same world and same randomness every run (linked with
   --wrap=Random_SeedFromCurrentTime), so runs can be compared */
void __wrap_Random_SeedFromCurrentTime(RNGState* rnd) { Random_Seed(rnd, 20261028); }

static int win_w = 640, win_h = 360, warm = 150, frames = 60, frameNo;
static float turn = 0.5f;
static unsigned char* pixels;
static OSMesaContext osm;
static FILE* profFile;
static double measureStart;
static unsigned* samples; static int nsamples, maxsamples = 1 << 22;

static int envint(const char* n, int d) { const char* v = getenv(n); return v ? atoi(v) : d; }
/* CPU time of the process (less noisy than wall time on a shared host) */
static double now(void) { struct timespec t; clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t); return t.tv_sec + t.tv_nsec / 1e9; }

static void on_prof(int sig, siginfo_t* si, void* uc_) {
	ucontext_t* uc = (ucontext_t*)uc_;
	if (nsamples < maxsamples) samples[nsamples++] = uc->uc_mcontext.arm_pc;
}

void Window_PreInit(void) { DisplayInfo.CursorVisible = true; }
void Window_Init(void) {
	Input.Sources = INPUT_SOURCE_NORMAL;
	win_w = envint("CC_W", 640); win_h = envint("CC_H", 360);
	warm = envint("CC_WARM", 150); frames = envint("CC_FRAMES", 60);
	if (getenv("CC_TURN")) turn = atof(getenv("CC_TURN"));
	DisplayInfo.Width = win_w; DisplayInfo.Height = win_h;
	DisplayInfo.Depth = 4; DisplayInfo.ScaleX = 1; DisplayInfo.ScaleY = 1;
	Window_Main.Width = win_w; Window_Main.Height = win_h;
	Platform_Flags |= PLAT_FLAG_SINGLE_PROCESS;
}
void Window_Free(void) { }
static void DoCreateWindow(int width, int height) {
	Window_Main.Exists = true; Window_Main.Focused = true;
	Window_Main.Width = win_w; Window_Main.Height = win_h;
	Window_Main.UIScaleX = DEFAULT_UI_SCALE_X; Window_Main.UIScaleY = DEFAULT_UI_SCALE_Y;
}
void Window_Create2D(int width, int height) { DoCreateWindow(width, height); }
void Window_Create3D(int width, int height) { DoCreateWindow(width, height); }
void Window_Destroy(void) { }
void Window_SetTitle(const cc_string* title) { }
void Clipboard_GetText(cc_string* value) { }
void Clipboard_SetText(const cc_string* value) { }
int Window_GetWindowState(void) { return WINDOW_STATE_NORMAL; }
cc_result Window_EnterFullscreen(void) { return 0; }
cc_result Window_ExitFullscreen(void) { return 0; }
int Window_IsObscured(void) { return 0; }
void Window_Show(void) { }
void Window_SetSize(int width, int height) { }
void Window_RequestClose(void) { Window_Main.Exists = false; Event_RaiseVoid(&WindowEvents.Closing); }
void Window_ProcessEvents(float delta) { }
void Gamepads_PreInit(void) { }
void Gamepads_Init(void) { }
void Gamepads_Process(float delta) { }
static void Cursor_GetRawPos(int* x, int* y) { *x = 0; *y = 0; }
void Cursor_SetPosition(int x, int y) { }
static void Cursor_DoSetVisible(cc_bool visible) { }
static void ShowDialogCore(const char* title, const char* msg) { Platform_LogConst(title); Platform_LogConst(msg); }
cc_result Window_OpenFileDialog(const struct OpenFileDialogArgs* args) { return ERR_NOT_SUPPORTED; }
cc_result Window_SaveFileDialog(const struct SaveFileDialogArgs* args) { return ERR_NOT_SUPPORTED; }
void Window_AllocFramebuffer(struct Bitmap* bmp, int width, int height) {
	bmp->scan0 = (BitmapCol*)Mem_Alloc(width * height, BITMAPCOLOR_SIZE, "window pixels");
	bmp->width = width; bmp->height = height;
}
void Window_DrawFramebuffer(Rect2D r, struct Bitmap* bmp) { }
void Window_FreeFramebuffer(struct Bitmap* bmp) { Mem_Free(bmp->scan0); }
void OnscreenKeyboard_Open(struct OpenKeyboardArgs* args) { }
void OnscreenKeyboard_SetText(const cc_string* text) { }
void OnscreenKeyboard_Close(void) { }
void Window_EnableRawMouse(void) { DefaultEnableRawMouse(); }
void Window_UpdateRawMouse(void) { }
void Window_DisableRawMouse(void) { DefaultDisableRawMouse(); }

void GLContext_Create(void) {
	osm = OSMesaCreateContextExt(OSMESA_RGBA, 24, 0, 0, NULL);
	pixels = malloc(win_w * win_h * 4);
	if (!osm || !OSMesaMakeCurrent(osm, pixels, GL_UNSIGNED_BYTE, win_w, win_h)) {
		fprintf(stderr, "OSMesa context failed\n"); exit(1);
	}
	OSMesaPixelStore(OSMESA_Y_UP, 0);
}
void GLContext_Update(void) { }
cc_bool GLContext_TryRestore(void) { return true; }
void GLContext_Free(void) { }
void* GLContext_GetAddress(const char* function) { return (void*)OSMesaGetProcAddress(function); }
void GLContext_SetVSync(cc_bool vsync) { }
void GLContext_GetApiInfo(cc_string* info) { }

static void save_shot(void) {
	const char* f = getenv("CC_SHOT"); FILE* o; int y;
	if (!f || !(o = fopen(f, "wb"))) return;
	fprintf(o, "P6\n%d %d\n255\n", win_w, win_h);
	for (y = 0; y < win_h; y++) {
		unsigned char* p = pixels + y * win_w * 4; int x;
		for (x = 0; x < win_w; x++, p += 4) fwrite(p, 1, 3, o);
	}
	fclose(o);
}

cc_bool GLContext_SwapBuffers(void) {
	struct LocalPlayer* p = Entities.CurPlayer;
	glFinish();
	frameNo++;
	if (p && turn) { p->Base.Yaw += turn; p->Base.next.yaw = p->Base.Yaw; p->Base.prev.yaw = p->Base.Yaw; }
	if (frameNo == warm) {
		struct sigaction sa; struct itimerval it = { { 0, 1000 }, { 0, 1000 } };
		measureStart = now();
		if (getenv("CC_PROF")) {
			samples = malloc(maxsamples * sizeof(unsigned));
			memset(&sa, 0, sizeof sa); sa.sa_sigaction = on_prof; sa.sa_flags = SA_SIGINFO | SA_RESTART;
			sigaction(SIGPROF, &sa, NULL); setitimer(ITIMER_PROF, &it, NULL);
		}
	}
	if (frameNo == warm + frames) {
		struct itimerval off = { { 0, 0 }, { 0, 0 } };
		double t = now() - measureStart;
		setitimer(ITIMER_PROF, &off, NULL);
		fprintf(stderr, "PERF: %d frames %dx%d in %.2f s CPU (qemu): %.1f ms/frame\n", frames, win_w, win_h, t, t * 1000 / frames);
		if (getenv("CC_PROF") && (profFile = fopen(getenv("CC_PROF"), "w"))) {
			int i; for (i = 0; i < nsamples; i++) fprintf(profFile, "%x\n", samples[i]);
			fclose(profFile);
		}
		save_shot();
		exit(0);
	}
	return true;
}
#endif
