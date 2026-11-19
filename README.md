# VIZO TOP DOWN SHOOTER — PS1 prototype

This is a native PlayStation 1 homebrew project written in C for PSn00bSDK.
It is not an Android game and does not run as an APK. It compiles to a PS1
executable (`.exe`) that can be packaged into a disc image for VIZO PSX.

## Current playable prototype

- Isometric-style 2D battlefield drawn with native PS1 GPU primitives.
- Player movement with the D-pad.
- Cross fires in the last facing direction.
- Enemies spawn around the arena and chase the player.
- Bullets, enemy health, player health, kills and increasing waves.
- Start pauses/resumes.

## Build environment

Use a Windows or Linux PC, or a GitHub Actions runner. Install a PSn00bSDK
prebuilt release and CMake 3.21+ (Ninja is used by the preset).

Set `PSN00BSDK_LIBS` to the SDK's `lib/libpsn00b` directory and add the SDK
`bin` directory to PATH. Then run:

```sh
cmake --preset default
cmake --build build
```

The PS1 executable should be produced in `build/` as
`VIZO_TOPDOWN_SHOOTER.exe` (alongside the ELF and map outputs).

## Important next milestone

This first project package is the source + native PS1 executable build setup.
It does not yet include final art, sound effects, a title screen, memory-card
saving, or a finished CD image. Those are subsequent milestones. Once the
first executable builds and is tested in VIZO PSX, the next step is to add
the disc layout (`SYSTEM.CNF`, executable and `mkpsxiso` project) to produce
a bootable BIN/CUE or ISO.

## Controls

| Controller | Action |
|---|---|
| D-pad | Move |
| Cross | Fire |
| Start | Pause / resume |

## SDK

PSn00bSDK: https://github.com/Lameguy64/PSn00bSDK
