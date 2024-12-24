# Sonic Robo Blast 32X
## Forked from Doom 32X: Resurrection / CD32X Fusion

![Build](https://github.com/viciious/d32xr/workflows/Build/badge.svg)

## Description

The goal of the project is to have a code base from which a playable version of DOOM for the Sega 32X platform could be compiled.
It is primarily focused on preservation and education but also on fixing the original game's idiosyncrasies and improving performance.

The project is based on the original Jaguar Doom source code release with a bulk of code taken from the Calico DOOM port.
I can't thank them enough for taking up the task of translating the original assembler code to C. Without this, the project would have taken me much longer.

To compile the rom you're going to need several things:
- Chilly Willy's Sega Devkit
- Dump the IWAD file from the Doom 32X Resurrection ROM
- Run `make`

## Features
- VGM music playback support
- Support for monster rotations and in-fighting
- Partially invisible spectres as in the origina game
- Uses both SH-2 CPUs for threaded rendering to improve performance
- Support for larger levels: runs MAP20 from the 24 Level Expansion ROM hack
- Stereo sound panning
- Support for multiple screen resolutions: from 128x144 (double width) and up to 252x144 (native)
- Save checkpoints and global options to SRAM
- Distance lighting effect as in the original game
- "Potato" mode that renders floors and ceilings in solid color
- New title screen reminiscent of PSX and Saturn versions
- Sega Mouse support
- Statusbar assets are no longer limited to 16 colors
- DMAPINFO lump support for naming and sequencing levels

Building
============

**Prerequisites**

A posix system and [Chilly Willy's Sega MD/CD/32X devkit](https://github.com/viciious/32XDK/releases) is requred to build this demo.

**VSCode + Remote Dev Container**

Unless you already have the devkit built, using VSCode with Docker is one of the faster ways to get started. VSCode will offer to set up a Development Container the first time the project folder is opened.

[More information on Developing with VSCode inside a Container](https://code.visualstudio.com/docs/remote/containers)

If you're using Windows 10/11, you must also [install WSL 2](https://docs.docker.com/desktop/windows/wsl/) and Linux distribution before attempting to build from VSCode.

***VSCode settings***

We recommend using the following VSCode settings.json for the project:

```json
{
    "C_Cpp.default.includePath": [
        "/opt/toolchains/sega/sh-elf/include/",
        "/opt/toolchains/sega/m68k-elf/include/"
    ],
    "C_Cpp.default.compilerPath": "/opt/toolchains/sega/sh-elf/bin/sh-elf-gcc",
    "C_Cpp.default.configurationProvider": "ms-vscode.makefile-tools",

    "makefile.configurations": [
        {
            "name": "release",
            "makeArgs": ["-j", "release"]
        },
        {
            "name": "debug",
            "makeArgs": ["-j", "debug"]
        },
        {
            "name": "clean",
            "makeArgs": ["clean"]
        }
    ]
}
```

## "DOOM 32x: Resurrection" Credits
* Programming : Victor Luchits
* Programming : Chilly Willy
* Testing : Matt B (Matteusbeus), TrekkiesUnite118
* VGM music: Spoony Bard

## "Calico DOOM" Credits
* Programming and Reverse Engineering : James Haley
* Additional Code By : Samuel Villarreal, Rebecca Heineman
* Original Jaguar DOOM Source: John Carmack, id Software

## Links
https://www.doomworld.com/forum/topic/119202-32x-resurrection/

https://github.com/team-eternity/calico-doom

## License
All original code, as well as code derived from the 3DO source code, is
available under the MIT license. Code taken from Jaguar DOOM is still under the
original license for that release, which is unfortunately not compatible with
free software licenses (if a source file does not have a license header stating
otherwise, then it is covered by the Jaguar Doom source license).

The rights to the Jaguar DOOM source code are detailed in the provided license 
agreement from id Software. See license.txt for details.
