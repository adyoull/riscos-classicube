# ClassiCube for RISC OS: changes

## 1.3.9-riscos1 (2026-11-16)

The first release of the RISC OS port, ClassiCube 1.3.9 (upstream 44ba25a)
for RISC OS 5 on ARMv7 (Raspberry Pi 2 and later). It follows the
0.1-test1 to 0.1-test14 test builds.

A *mixed* program: UnixLib is linked in, and OpenGL (OSMesa), EGL, SDL 2
and OpenAL come from riscos-mesa's !MesaLibs (in the zip) through
!SharedLibs. !RISC3D, when it's in !Boot.Resources, decides how the game
is drawn; without it the game draws in software.

- Built with riscos-crossdev 1.12 and riscos-mesa devkit 20.3.5-21,
  linked with `-lMesaGPU`; ships !MesaLibs 20.3.5-21, which gives
  !SharedLibs the ld-riscos/so/2 loader as it boots.
- In PackMan as ClassiCube (Apps.Games), which brings MesaLibs with it.
- Plays single player and connects to servers (HTTPS through AcornSSL).
- Mouse look holds the pointer while playing; the title bar says
  "Escape frees the mouse". Escape pauses and frees it, "Back to game"
  takes it again.
- A "Keys and mouse" window at start-up, with a "Show this when ClassiCube
  starts" switch, and a button for it in the launcher's Settings.
- The title bar shows whether the picture is [plotted] or an [overlay].
- `ClassiCube$WindowSize`, `ClassiCube$RenderSize` and `ClassiCube$Overlay`
  in !Run set the window size, the size the game draws at, and whether the
  display may stretch the picture. With an overlay possible the window is
  2x or 3x the drawing size and the display scales it up.
- Speed: GL_FASTEST hints, cave culling, a shorter default view distance,
  NEON music decoding (44% fewer instructions than upstream, same sound).
- Needs: !SharedLibs and !MesaLibs in !Boot.Resources, SharedUnixLibrary
  1.16 or later and ARMEABISupport. !RISC3D 0.04 or later
  is optional. About 16 MB of memory for the game, plus the libraries.

Known: still a few frames a second in heavy scenes when drawn in
software; most of the time goes on Mesa's GL_NEAREST textured spans
(handoff to riscos-mesa). Not yet tested on a Pi as a release; tested on
the RISC OS on Linux testbed (ROM 5.31-24), where the game draws in
software.
