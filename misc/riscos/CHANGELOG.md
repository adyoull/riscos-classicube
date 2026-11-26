# ClassiCube for RISC OS: changes

## 1.3.9-riscos3 (2026-11-26)

The same source as 1.3.9-riscos2, rebuilt with the newest libraries.

- Built with riscos-crossdev 1.14 (UnixLib 5.0.3.12, linked in) and
  riscos-mesa devkit 20.3.5-24; ships !MesaLibs 20.3.5-24.
- From that !MesaLibs: with !RISC3D drawing on a Pi 4's GPU, the GPU
  also shows the game's window (no overlay or plot), and scales it when
  `ClassiCube$RenderSize` is smaller than the window; SDL's mouse look
  keeps the same speed at any render size and doesn't drift; the
  launcher is drawn at full width in alpha desktop modes (C16M LABGR);
  16 bpp desktops draw with the right shape and colours.
- The 1.3.9-riscos2 program also works with !MesaLibs 20.3.5-24
  (check-mixed-package).

## 1.3.9-riscos2 (2026-11-16)

The same program as 1.3.9-riscos1, with riscos-mesa's released !MesaLibs
and a PackMan package.

- Ships !MesaLibs 20.3.5-21 (riscos1 had the 20.3.5-20gpu8 test build),
  and is linked against devkit 20.3.5-21. This !MesaLibs gives
  !SharedLibs the ld-riscos/so/2 loader as it boots, so a !SharedLibs from
  PackMan is enough.
- `!Run` checks for !MesaLibs before the loader, and its messages say
  where !MesaLibs comes from: the release zip, or PackMan.
- In PackMan as ClassiCube (Games), which brings MesaLibs with it.
- The readme is about the RISC OS port only, with screenshots from
  RISC OS (misc/riscos/images).

## 1.3.9-riscos1 (2026-11-16)

The first release of the RISC OS port, ClassiCube 1.3.9 (upstream 44ba25a)
for RISC OS 5 on ARMv7 (Raspberry Pi 2 and later). It follows the
0.1-test1 to 0.1-test14 test builds.

A *mixed* program: UnixLib is linked in, and OpenGL (OSMesa), EGL, SDL 2
and OpenAL come from riscos-mesa's !MesaLibs (in the zip) through
!SharedLibs. !RISC3D, when it's in !Boot.Resources, decides how the game
is drawn; without it the game draws in software.

- Built with riscos-crossdev 1.12 and riscos-mesa devkit 20.3.5-20gpu4,
  linked with `-lMesaGPU`; ships !MesaLibs 20.3.5-20gpu8.
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
