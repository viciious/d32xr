Atari Jaguar DOOM Source Archive

Provided by Carl Forhan / Songbird Productions April 2003

1. The rights to the Jaguar DOOM source code are detailed
   in the provided license agreement from id Software.
   See license.txt for details.

2. Songbird Productions makes no guarantees or warranties
   about the state of this code; it is provided on an
   "as is" basis only. Songbird Productions is not liable
   for any damages under any circumstances.

3. Technical info:

   This makefile will produce doom.abs. I used v2.6 of
   the Jaguar M68k C compiler to do this. Make sure
   your include/executable paths are set up properly in
   your Jaguar development shell and also in the provided
   makefile.

   You will need to dump the jagdoom.wad from your Jaguar
   DOOM cartridge. Dump from $840000 to the end of the
   cartridge ROM.

   doom.abs loads at    $  4000 (Jaguar RAM)
   jagdoom.wad loads at $840000 (Jaguar cartridge ROM)

   Note the jagdoom.wad file is endian-swapped as compared
   to a PC DOOM WAD file. It also has some entries unique
   to the Jag version. Theoretically Jaguar DOOM could use
   other WAD files, but a simple cut and paste will not
   work.

4. Have fun! It's my hope that providing this code will
   help stimulate further Jaguar development for years
   to come, to the benefit of Jaguar fans worldwide.


Carl Forhan
Songbird Productions
http://songbird-productions.com
