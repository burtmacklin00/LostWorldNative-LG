# Native port architecture

Goal: a PC executable that runs the original logic of The Lost World
translated to C++, with its own engine for graphics, sound and controls. The game
still needs your `lostwsga.zip` to load its data; the repository never
contains ROM data (`data/` is in `.gitignore`).

## Original hardware (Model 3 Step 1.5)

| Part | Original | In the port |
|---|---|---|
| CPU | PowerPC 603e, 100 MHz | Code translated to C++ (static recompilation) |
| GPU | Real3D Pro-1000 (scene graph in memory, matrices, textures in VROM) | Own Vulkan 1.3 renderer that reads the same structures |
| 2D layer | Tilegen (HUD, text) | Own 2D layer over the 3D scene |
| Sound | 68000 + 2× SCSP | Phase 1: emulate the 68000 and the SCSP. Phase 2: play the sound commands directly |
| Guns | Analog position read | Mouse, gamepad or VR controller |

## What we already know about the program

- The reset vector (`0xFFF00100`) initialises the hardware, clears RAM and copies
  three blocks from CROM to RAM (table at `0xFFF00058`):
  - `FFE00000 → 00000000` (1 MB)
  - `FFF20000 → 000FFFFC` (237 KB)
  - `FFF9C000 → 0019C000` (27 KB)
- Then it jumps to `0x00000000`: **the whole game runs from RAM**, which makes
  recompilation easier (fixed code, no dynamic loading).
- About **240,000 instructions (938 KB)** reachable in **1,636 functions**.
- The decoder recognises 100% of the reachable code (the 4 "unknowns"
  are empty exception vectors).
- 95 indirect `bctr` jumps (switches/jump tables) to resolve.
- Heavy floating-point use (`fmuls`, `fadds`, `fctiwz`...): 3D logic and
  matrices on the CPU, which gives us direct access to camera and projection for
  widescreen and VR.
- Hardware regions accessed: `0x84000000`–`0x9C000000` (Real3D),
  `0xC0000000`–`0xC1000000`, `0xF0040000` (inputs), `0xF0100000` (system
  control and interrupts), `0xF0800000`/`0xF0C00000` (PCI bridge).

## Pipeline

```
lostwsga.zip ──romtool.py──▶ data/*.bin   (LostWorld.exe also reads the zip directly: rom_zip.cpp)
data/crom.bin ──analyze.py──▶ out/listing.asm, functions.txt, report.txt
              ──recomp.py──▶ build/gen/*.cpp  (one C++ function per PPC function)
gen/*.cpp + runtime/ (memory, Real3D, SCSP, input) ──CMake/MSVC──▶ LostWorld.exe
```

## Phases

1. **Tools** (done): extractor, PPC decoder, function analysis.
2. **Recompiler**: generate C++ per function, resolve jump tables,
   interrupts (vblank) and hardware accesses through runtime functions.
3. **Minimal runtime**: memory, timers, inputs; boot up to the test menu.
4. **Real3D renderer**: interpret the scene graph and draw with a modern API
   at any resolution, native widescreen, MSAA or NVIDIA DLAA, HD textures.
5. **Sound** and **gun** with mouse/gamepad.
6. **VR (OpenXR)**: stereo camera and head tracking injected into the
   view matrix; gun aimed with the VR controller.
