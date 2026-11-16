# ClassiCube for RISC OS

ClassiCube is a custom Minecraft Classic compatible client written in C from scratch. This is its port to **RISC OS 5** on ARMv7 machines: the Raspberry Pi 2 and later, and other Cortex-A boards with VFP.

**Download** the latest RISC OS release from this repository's [Releases](../../releases) page.

> [!IMPORTANT]
**ClassiCube is not affiliated with (or supported by) Mojang AB, Minecraft, or Microsoft in any way.** <br><br>
**ClassiCube is not trying to replicate modern Minecraft.**<br>
**It will never support survival, Minecraft accounts, or modern Minecraft servers.**<br><br>
This RISC OS port is not made or supported by the ClassiCube team. Please report problems with it here, not upstream.

# What ClassiCube is

ClassiCube aims to replicate the 2009 Minecraft Classic client while offering **optional** enhancements to improve gameplay.
<details>
<summary><b>Features (click to expand)</b></summary>

* Single player, and multiplayer on classicube.net and other ClassiCube servers
* Much better performance and minimal memory usage compared to original Minecraft Classic
* Optional enhancements including custom blocks, models, and environment colors
* Partially supports some features of Minecraft Classic versions before 0.30

</details>

![classic](https://github.com/ClassiCube/ClassiCube/assets/6509348/eedee53f-f53e-456f-b51c-92c62079eee0)

![enhanced](https://github.com/ClassiCube/ClassiCube/assets/6509348/b2fe0e2b-5d76-41ab-909f-048d0ad15f37)

# Installing

**With PackMan:** add the package source `https://adyoull.github.io/riscos-packages/pkg/index`, then install **ClassiCube** (in Games). PackMan brings MesaLibs and the other libraries it needs. Restart once afterwards, then run !ClassiCube from `Apps.Games`.

**From the zip:**
The release zip holds two applications:
* **!ClassiCube**, the game;
* **!MesaLibs**, riscos-mesa's shared OpenGL, EGL, SDL 2 and OpenAL libraries, which the game loads.

1. Copy **!MesaLibs** into `!Boot.Resources` (open !Boot, then Resources). If you already have one, replace it with the newer copy, then restart the machine.
2. Copy **!ClassiCube** anywhere you like and double-click it.

You also need:
* !SharedLibs, in `!Boot.Resources` (!MesaLibs 20.3.5-21 and later give it the `ld-riscos/so/2` loader the game needs)
* SharedUnixLibrary 1.16 or later
* ARMEABISupport
* the AcornSSL module, to sign in and to download from classicube.net

**!RISC3D** (0.04 or later, in `!Boot.Resources`) is optional. When it is there, it decides how the game is drawn. Without it, the game draws in software. Restart after adding or replacing !RISC3D.

The game uses about 16 MB of memory, plus the libraries. Its settings, worlds and downloaded files go in `<Choices$Write>.ClassiCube`.

# How to play

The first time you run ClassiCube, it offers to download the textures, sounds and music it needs from classicube.net (about 20 MB). Click 'Yes'.

**Singleplayer mode:**
run !ClassiCube, then click Singleplayer.

**Multiplayer mode:**
run !ClassiCube. You can connect to servers on your network, and to classicube.net servers if you have a [ClassiCube account](https://www.classicube.net/).

A **Keys and mouse** window lists the controls when the game starts. Untick "Show this when ClassiCube starts" to stop it appearing; Settings > Keys and mouse in the launcher shows it again.

#### Keys and mouse

| Key | Does | Key | Does |
|---|---|---|---|
| W A S D | walk | Escape | pause, and free the mouse |
| Space | jump | T | chat |
| Shift / Ctrl | run / slow | Tab | player list |
| Z | fly | F | view distance |
| Q / E | fly up / down | F1 | hide the HUD |
| X | go through blocks | F3 | FPS and position |
| R | respawn | F5 | third person |
| Enter | set spawn point | F7 | axis lines |
| Select | break block | F8 | smooth camera |
| Adjust | place block | F11 | full screen |
| Menu | copy the block looked at | F12 | screenshot |
| 1-9 or wheel | choose block | C + wheel | zoom |
| B | all blocks | G | drop |

To change the keys: Escape > Options > Controls, in the game.

#### The mouse

While you play, ClassiCube holds the pointer so you can look around, and the title bar says **"Escape frees the mouse"**. Escape pauses the game and lets the pointer go. Click "Back to game" to carry on.

#### Settings in !Run

Shift-double-click !ClassiCube to open it, then load !Run into a text editor. These lines are there, commented out:

* `Set ClassiCube$WindowSize 1280x720` sets the game window's size.
* `Set ClassiCube$RenderSize 480x270` sets the size the game draws at (640x360 by default; `full` draws at the window's size).
* `Set ClassiCube$Overlay 0` stops the display hardware stretching the picture to the window.

The title bar shows how the picture reaches the screen:
* **[overlay]**: the display scales it up, at no cost.
* **[plotted]**: it is copied to the screen.

When an overlay is possible, the window opens at 2x or 3x the drawing size.

#### Speed tips
* Press F to change the view distance. Shorter distances run faster.
* A smaller `ClassiCube$RenderSize` runs faster.
* In software, most of the time goes on drawing textured blocks. With !RISC3D, it can use the GPU where one is available.
* If the server has disabled hacks, key combinations such as fly and speed will not do anything.
* To see the list of built in commands, type `/client`. For help on one, type `/client help <command name>`.

# Compiling

ClassiCube for RISC OS is cross-compiled on Linux or macOS with:
* the [riscos-crossdev](https://github.com/adyoull/riscos-crossdev) toolchain (GCCSDK GCC 10, UnixLib; 1.12 or later);
* the riscos-mesa devkit (20.3.5-21 or later): OpenGL (OSMesa), EGL, SDL 2 and OpenAL for RISC OS.

```
export PATH=<toolchain>/bin:$PATH
make riscos RELEASE=1 RISCOS_MIXED=1 RISCOS_DEVKIT=<devkit>
MIXED=1 RISCOS_DEVKIT=<devkit> RCD_DIR=<riscos-crossdev> misc/riscos/package.sh <version>
```

This makes `dist/ClassiCube-riscos-<version>-mixed.zip`, with !ClassiCube and !MesaLibs, keeping RISC OS file types.

Leave out `RISCOS_MIXED=1` and `MIXED=1` for a static program, with every library linked in (no !MesaLibs needed, but about 11 MB and no shared copies).

[misc/riscos/README.md](misc/riscos/README.md) explains:
* how the port works;
* the checks and tests;
* the speed measurements.

[misc/riscos/CHANGELOG.md](misc/riscos/CHANGELOG.md) lists the releases.

## Documentation

Functions and variables in .h files are mostly documented.

Further information (e.g. style) for ClassiCube's source code can be found in the doc and misc folders.

<details>
<summary><h2>Open source technologies (click to expand)</h2></summary>

* [ClassiCube](https://github.com/ClassiCube/ClassiCube) - the game this port is built from
* [FreeType](https://www.freetype.org/) - Font handling
* [GCCSDK](https://www.riscos.info/index.php/GCCSDK) - GCC and UnixLib for RISC OS
* [Mesa](https://www.mesa3d.org/) - OpenGL, through riscos-mesa
* [SDL 2](https://www.libsdl.org/) - Window, keyboard and mouse
* [OpenAL Soft](https://openal-soft.org/) - Sound
* AcornSSL - TLS on RISC OS
* [QEMU](https://www.qemu.org/) - Used to test and measure the port

</details>

## Sound Credits
ClassiCube uses sounds from [Freesound.org](https://freesound.org)<br>
Full credits are listed in [doc/sound-credits.md](doc/sound-credits.md)

## Licence
ClassiCube is under the licence in [license.txt](license.txt). The RISC OS port's changes are under the same licence.
