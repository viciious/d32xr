# DOOM 32X: Resurrection

![Build](https://github.com/viciious/d32xr/workflows/Build/badge.svg)

## Description

The goal of the project is to have a code base from which a playable version of DOOM for the Sega 32X platform could be compiled.
It is primarily focused on preservation and education but also on fixing the original game's idiosyncrasies and improving performance.

The project is based on the original Jaguar Doom source code release with a bulk of code taken from the Calico DOOM port.
I can't thank them enough for taking up the task of translating the original assembler code to C. Without this, the project would have taken me much longer.

To compile the rom you're going to need several things:
- Chilly Willy's Sega Devkit
- Dump the IWAD file "doom32x.wad" from your Doom32x cartridge. Dump from address 0xBB000 to the end of the ROM.
- Run `make -f Makefile.mars`

Alternatively, you can download the ROM of the engine from the Github Actions page and patch it to run the original .wad file:

```bash
dd if=doom32x.wad seek=6093 bs=16 conv=notrunc of=D32XR.bin
```

## "Calico DOOM" Credits
* Programming and Reverse Engineering : James Haley
* Additional Code By : Samuel Villarreal, Rebecca Heineman
* Original Jaguar DOOM Source: John Carmack, id Software

## "DOOM 32x: Resurrection" Credits
* Programming : Victor Luchits

## Links
https://github.com/team-eternity/calico-doom

## License
All original code, as well as code derived from the 3DO source code, is
available under the MIT license. Code taken from Jaguar DOOM is still under the
original license for that release, which is unfortunately not compatible with
free software licenses (if a source file does not have a license header stating
otherwise, then it is covered by the Jaguar Doom source license).

The rights to the Jaguar DOOM source code are detailed in the provided license 
agreement from id Software. See license.txt for details.
