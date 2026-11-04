/* TEST ONLY (not part of the game): a headless "window" for measuring
   ClassiCube's GL1 renderer on riscos-mesa's own libOSMesa.a (the devkit's
   RISC OS build) as an ARM Linux program under qemu-arm (see perf.sh).
   Build with -DCC_WIN_BACKEND=99 -DCC_GFX_BACKEND=CC_GFX_BACKEND_GL1.
   Environment:
     CC_W, CC_H      frame size (default 640x360)
     CC_WARM         at least this many frames before measuring (default
                     150); measuring also waits until the world is loaded
                     and no chunk has been built for 10 frames, and the
                     camera only turns from then, so measured frames are
                     the same every run
     CC_FRAMES       frames measured (default 60), then it quits
     CC_PROF=file    PC samples (SIGPROF, 1 ms of CPU) while measuring
     CC_SHOT=file    the last frame as a PPM
     CC_LAUNCHER_SHOT=file  the launcher (run without --singleplayer) as a PPM
     CC_TURN=deg     camera turn per frame (default 0.5), so frames differ
     CC_SKIP=a,b     render passes left out, to see what each costs (names
                     below: sky clouds sides edges normal translucent
                     entities held gui particles)
     CC_NOALPHA=1    no alpha test; CC_NOFOG=1 no fog (cost of each)
     CC_NOANIM=1     no texture animation (lava, water: they follow real
                     time), for comparing frames between builds
     CC_POS=x,y,z[,pitch]  put the camera there (flying, noclip) once the
                     world is loaded, before measuring: other viewpoints;
                     CC_POS=cave: the first air pocket low down with rock
                     above it (a cave), looking along it */
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
#include "EntityComponents.h"
#include "World.h"
#include "Block.h"
#include "BlockID.h"
#include "TexturePack.h"
#include "Graphics.h"
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
static int measuring, measuredFrames, quietFrames, lastChunkUpdates = -1;
static float startYaw; static int posSet;
/* Markers for insnprof.c (on=/off= addresses): count only in between */
__attribute__((noinline)) void perf_mark_start(void) { __asm__ volatile(""); }
__attribute__((noinline)) void perf_mark_stop(void) { __asm__ volatile(""); }

static int envint(const char* n, int d) { const char* v = getenv(n); return v ? atoi(v) : d; }
/* CPU time of the process (less noisy than wall time on a shared host) */
static double now(void) { struct timespec t; clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t); return t.tv_sec + t.tv_nsec / 1e9; }

static void on_prof(int sig, siginfo_t* si, void* uc_) {
	ucontext_t* uc = (ucontext_t*)uc_;
	if (nsamples < maxsamples) samples[nsamples++] = uc->uc_mcontext.arm_pc;
}


/* Render passes wrapped (-Wl,--wrap=...) so CC_SKIP can leave them out */
static const char* skipList; static int skipRead, noAlpha = -1, noFog = -1;
static int skipped(const char* name) {
	const char* s; size_t n = strlen(name);
	if (!skipRead) { skipList = getenv("CC_SKIP"); skipRead = 1; }
	s = skipList;
	while (s && *s) {
		if (!strncmp(s, name, n) && (s[n] == ',' || !s[n])) return 1;
		s = strchr(s, ','); if (s) s++;
	}
	return 0;
}
void __real_EnvRenderer_RenderSky(void);
void __wrap_EnvRenderer_RenderSky(void) { if (!skipped("sky")) __real_EnvRenderer_RenderSky(); }
void __real_EnvRenderer_RenderClouds(void);
void __wrap_EnvRenderer_RenderClouds(void) { if (!skipped("clouds")) __real_EnvRenderer_RenderClouds(); }
void __real_EnvRenderer_RenderMapSides(void);
void __wrap_EnvRenderer_RenderMapSides(void) { if (!skipped("sides")) __real_EnvRenderer_RenderMapSides(); }
void __real_EnvRenderer_RenderMapEdges(void);
void __wrap_EnvRenderer_RenderMapEdges(void) { if (!skipped("edges")) __real_EnvRenderer_RenderMapEdges(); }
void __real_MapRenderer_RenderNormal(float delta);
void __wrap_MapRenderer_RenderNormal(float delta) { if (!skipped("normal")) __real_MapRenderer_RenderNormal(delta); }
void __real_MapRenderer_RenderTranslucent(float delta);
void __wrap_MapRenderer_RenderTranslucent(float delta) { if (!skipped("translucent")) __real_MapRenderer_RenderTranslucent(delta); }
void __real_Entities_RenderModels(float delta, float t);
void __wrap_Entities_RenderModels(float delta, float t) { if (!skipped("entities")) __real_Entities_RenderModels(delta, t); }
void __real_HeldBlockRenderer_Render(float delta);
void __wrap_HeldBlockRenderer_Render(float delta) { if (!skipped("held")) __real_HeldBlockRenderer_Render(delta); }
void __real_Gui_RenderGui(float delta);
void __wrap_Gui_RenderGui(float delta) { if (!skipped("gui")) __real_Gui_RenderGui(delta); }
void __real_Particles_Render(float t);
void __wrap_Particles_Render(float t) { if (!skipped("particles")) __real_Particles_Render(t); }
void __real_Gfx_SetAlphaTest(cc_bool enabled);
void __wrap_Gfx_SetAlphaTest(cc_bool enabled) { if (noAlpha < 0) noAlpha = getenv("CC_NOALPHA") != NULL; __real_Gfx_SetAlphaTest(noAlpha ? 0 : enabled); }
void __real_Gfx_UpdateTexture(GfxResourceID texId, int x, int y, struct Bitmap* part, int rowWidth, cc_bool mipmaps);
void __wrap_Gfx_UpdateTexture(GfxResourceID texId, int x, int y, struct Bitmap* part, int rowWidth, cc_bool mipmaps) {
	int i;
	if (getenv("CC_NOANIM")) for (i = 0; i < Atlas1D.Count; i++) if (Atlas1D.TexIds[i] == texId) return;
	__real_Gfx_UpdateTexture(texId, x, y, part, rowWidth, mipmaps);
}
void __real_Gfx_SetFog(cc_bool enabled);
void __wrap_Gfx_SetFog(cc_bool enabled) { if (noFog < 0) noFog = getenv("CC_NOFOG") != NULL; __real_Gfx_SetFog(noFog ? 0 : enabled); }

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
/* CC_LAUNCHER_SHOT=file: the launcher's picture (the Nth time it's drawn,
   CC_LAUNCHER_FRAME, default 3) as a PPM, then quit */
void Window_DrawFramebuffer(Rect2D r, struct Bitmap* bmp) {
	static int draws;
	const char* f = getenv("CC_LAUNCHER_SHOT"); const char* n = getenv("CC_LAUNCHER_FRAME");
	FILE* o; int x, y;
	if (!f || ++draws < (n ? atoi(n) : 3) || !(o = fopen(f, "wb"))) return;
	fprintf(o, "P6\n%d %d\n255\n", bmp->width, bmp->height);
	for (y = 0; y < bmp->height; y++)
		for (x = 0; x < bmp->width; x++) {
			BitmapCol c = Bitmap_GetPixel(bmp, x, y);
			fputc(BitmapCol_R(c), o); fputc(BitmapCol_G(c), o); fputc(BitmapCol_B(c), o);
		}
	fclose(o); exit(0);
}
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
	if (!measuring && World.Loaded && p && !posSet) {
		const char* pos = getenv("CC_POS"); posSet = 1;
		if (pos) {
			struct LocationUpdate u = { 0 }; float pitch = 0;
			if (!strcmp(pos, "cave")) {
				int x, y, z, k, found = 0;
				for (y = 4; y < 24 && !found; y++)
				for (z = 8; z < World.Length - 8 && !found; z += 2)
				for (x = 8; x < World.Width - 8 && !found; x += 2) {
					if (World_GetBlock(x, y, z) != BLOCK_AIR || World_GetBlock(x, y + 1, z) != BLOCK_AIR) continue;
					for (k = 4; k < 12 && y + k < World.Height; k++)
						if (Blocks.FullOpaque[World_GetBlock(x, y + k, z)]) { found = 1; break; }
					if (found) { u.pos.x = x + 0.5f; u.pos.y = y; u.pos.z = z + 0.5f;
						fprintf(stderr, "PERF: cave at %d,%d,%d\n", x, y, z); }
				}
			} else
			sscanf(pos, "%f,%f,%f,%f", &u.pos.x, &u.pos.y, &u.pos.z, &pitch);
			p->Hacks.Flying = true; p->Hacks.Noclip = true; p->Hacks.Floating = true;
			u.flags = LU_HAS_POS | LU_POS_ABSOLUTE_INSTANT | LU_HAS_PITCH; u.pitch = pitch;
			p->Base.VTABLE->SetLocation(&p->Base, &u);
			quietFrames = 0;
		}
	}
	if (!measuring) {
		/* steady state: world loaded, nothing built for 10 frames */
		if (World.Loaded && Game.ChunkUpdates == lastChunkUpdates) quietFrames++; else quietFrames = 0;
		lastChunkUpdates = Game.ChunkUpdates;
		if (frameNo >= warm && quietFrames >= 10 && p) {
			struct sigaction sa; struct itimerval it = { { 0, 1000 }, { 0, 1000 } };
			measuring = 1; startYaw = p->Base.Yaw;
			fprintf(stderr, "PERF: measuring from frame %d\n", frameNo);
			measureStart = now();
			if (getenv("CC_PROF")) {
				samples = malloc(maxsamples * sizeof(unsigned));
				memset(&sa, 0, sizeof sa); sa.sa_sigaction = on_prof; sa.sa_flags = SA_SIGINFO | SA_RESTART;
				sigaction(SIGPROF, &sa, NULL); setitimer(ITIMER_PROF, &it, NULL);
			}
			perf_mark_start();
		}
	} else {
		measuredFrames++;
	}
	if (measuring && p && turn) {
		float yaw = startYaw + turn * measuredFrames;
		p->Base.Yaw = yaw; p->Base.next.yaw = yaw; p->Base.prev.yaw = yaw;
	}
	if (measuring && measuredFrames == frames) {
		struct itimerval off = { { 0, 0 }, { 0, 0 } };
		double t;
		perf_mark_stop();
		t = now() - measureStart;
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
