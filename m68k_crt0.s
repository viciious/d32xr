|  The MIT License (MIT)
|
|  Permission is hereby granted, free of charge, to any person obtaining a copy
|  of this software and associated documentation files (the "Software"), to deal
|  in the Software without restriction, including without limitation the rights
|  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
|  copies of the Software, and to permit persons to whom the Software is
|  furnished to do so, subject to the following conditions:
|
|  The above copyright notice and this permission notice shall be included in all
|  copies or substantial portions of the Software.
|
|  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
|  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
|  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
|  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
|  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
|  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
|  SOFTWARE.

| SEGA 32X support code for the 68000
| by Chilly Willy
| First part of rom header

        .text

| Initial exception vectors. When the console is first turned on, it is
| in MegaDrive mode. All vectors just point to the code to start up the
| Mars adapter. After the adapter is enabled, none of these vectors will
| appear as the adapter uses its own vector table to route exceptions to
| the jump table. 0x3F0 is where the 68000 starts at for the 32X.

        .long   0x01000000,0x000003F0,0x000003F0,0x000003F0,0x000003F0,0x000003F0,0x000003F0,0x000003F0
        .long   0x000003F0,0x000003F0,0x000003F0,0x000003F0,0x000003F0,0x000003F0,0x000003F0,0x000003F0
        .long   0x000003F0,0x000003F0,0x000003F0,0x000003F0,0x000003F0,0x000003F0,0x000003F0,0x000003F0
        .long   0x000003F0,0x000003F0,0x000003F0,0x000003F0,0x000003F0,0x000003F0,0x000003F0,0x000003F0
        .long   0x000003F0,0x000003F0,0x000003F0,0x000003F0,0x000003F0,0x000003F0,0x000003F0,0x000003F0
        .long   0x000003F0,0x000003F0,0x000003F0,0x000003F0,0x000003F0,0x000003F0,0x000003F0,0x000003F0
        .long   0x000003F0,0x000003F0,0x000003F0,0x000003F0,0x000003F0,0x000003F0,0x000003F0,0x000003F0
        .long   0x000003F0,0x000003F0,0x000003F0,0x000003F0,0x000003F0,0x000003F0,0x000003F0,0x000003F0

| Standard MegaDrive ROM header at 0x100

        .ascii  "SEGA 32X D32XR  "      /* SEGA must be the first four chars for TMSS */
        .ascii  "                "
        .ascii  "D32XR           "      /* export name */
        .ascii  "                "
        .ascii  "                "
        .ascii  "D32XR           "      /* domestic (Japanese) name */
        .ascii  "                "
        .ascii  "                "
        .ascii  "GM MK-0000 -00"
        .word   0x0000                  /* checksum - not needed */
        .ascii  "J6              "
        .long   0x00000000,0x0007FFFF   /* ROM start, end */
        .long   0x00FF0000,0x00FFFFFF   /* RAM start, end */

        .ifdef  HAS_SAVE_RAM
        .ascii  "RA"                    /* External RAM */
        .byte   0xF8                    /* don't clear + odd bytes */
        .byte   0x20                    /* SRAM */
        .long   0x00200001,0x0020FFFF   /* SRAM start, end */
        .else
        .ascii  "            "          /* no SRAM */
        .endif

        .ascii  "    "
        .ascii  "        "

        .ifdef  MYTH_HOMEBREW
        .ascii  "MYTH3900"              /* memo indicates Myth native executable */
        .else
        .ascii  "        "              /* memo */
        .endif

        .ascii  "                "
        .ascii  "                "
        .ascii  "F               "      /* enable any hardware configuration */

| Mars exception vector jump table at 0x200

        jmp     0x880800.l  /* reset = hot start */
        jsr     0x880806.l  /* EX_BusError */
        jsr     0x880806.l  /* EX_AddrError */
        jsr     0x880806.l  /* EX_IllInstr */
        jsr     0x880806.l  /* EX_DivByZero */
        jsr     0x880806.l  /* EX_CHK */
        jsr     0x880806.l  /* EX_TrapV */
        jsr     0x880806.l  /* EX_Priviledge */
        jsr     0x880806.l  /* EX_Trace */
        jsr     0x880806.l  /* EX_LineA */
        jsr     0x880806.l  /* EX_LineF */
        .space  72          /* reserved */
        jsr     0x880806.l  /* EX_Spurious */
        jsr     0x880806.l  /* EX_Level1 */
        jsr     0x880806.l  /* EX_Level2 */
        jsr     0x880806.l  /* EX_Level3 */
        jmp     0x88080C.l  /* EX_Level4 HBlank */
        jsr     0x880806.l  /* EX_Level5 */
        jmp     0x880812.l  /* EX_Level6 VBlank */
        jsr     0x880806.l  /* EX_Level7 */
        jsr     0x880806.l  /* EX_Trap0 */
        jsr     0x880806.l  /* EX_Trap1 */
        jsr     0x880806.l  /* EX_Trap2 */
        jsr     0x880806.l  /* EX_Trap3 */
        jsr     0x880806.l  /* EX_Trap4 */
        jsr     0x880806.l  /* EX_Trap5 */
        jsr     0x880806.l  /* EX_Trap6 */
        jsr     0x880806.l  /* EX_Trap7 */
        jsr     0x880806.l  /* EX_Trap8 */
        jsr     0x880806.l  /* EX_Trap9 */
        jsr     0x880806.l  /* EX_TrapA */
        jsr     0x880806.l  /* EX_TrapB */
        jsr     0x880806.l  /* EX_TrapC */
        jsr     0x880806.l  /* EX_TrapD */
        jsr     0x880806.l  /* EX_TrapE */
        jsr     0x880806.l  /* EX_TrapF */
        .space  166         /* reserved */
