OUTPUT_ARCH(sh)
EXTERN (_start)
ENTRY (_start)
__DYNAMIC  =  0;

/*
 * The memory map look like this:
 * +--------------------+ <- 0x02000000
 * | .text              |
 * |                    |
 * |         __text_end |
 * +--------------------+
 * .                    .
 * .                    .
 * .                    .
 * +--------------------+ <- 0x06000000
 * | .data              | initialized data goes here
 * |                    |
 * |         __data_end |
 * +--------------------+
 * | .bss               |
 * |        __bss_start | start of bss, cleared by crt0
 * |                    |
 * |         __bss__end | start of heap, used by sbrk()
 * +--------------------+
 * .                    .
 * .                    .
 * .                    .
 * |            __stack | top of stack (for Master SH2)
 * +--------------------+ <- 0x0603FC00
 */

MEMORY
{
    rom (rx) : ORIGIN = 0x02000000, LENGTH = 0x00400000
    ram (wx) : ORIGIN = 0x06000000, LENGTH = 0x0003F800
}

/*
 * Allocate the stack to be at the top of memory, since the stack
 * grows down
 */

PROVIDE (__stack = 0x0603F800);

SECTIONS
{
  .text 0x02000000 :
  AT ( 0x00000000 )
  {
    __text_start = .;
    *(.text)
    *(.text.*)
    *(.gnu.linkonce.t.*)

    . = ALIGN(16);
    __INIT_SECTION__ = .;
    KEEP (*(.init))
    SHORT (0x000B)	/* rts */
    SHORT (0x0009)	/* nop */
    . = ALIGN(16);
    __FINI_SECTION__ = .;
    KEEP (*(.fini))
    SHORT (0x000B)	/* rts */
    SHORT (0x0009)	/* nop */

    *(.eh_frame_hdr)
    KEEP (*(.eh_frame))
    *(.gcc_except_table)
    *(.gcc_except_table.*)
    KEEP (*(.jcr))

    . = ALIGN(16);
     __CTOR_LIST__ = .;
    ___CTOR_LIST__ = .;
    LONG((__CTOR_END__ - __CTOR_LIST__) / 4 - 2)
    KEEP (*(SORT(.ctors.*)))
    KEEP (*(.ctors))
    LONG(0)
    __CTOR_END__ = .;

    . = ALIGN(16);
    __DTOR_LIST__ = .;
    ___DTOR_LIST__ = .;
    LONG((__DTOR_END__ - __DTOR_LIST__) / 4 - 2)
    KEEP (*(SORT(.dtors.*)))
    KEEP (*(.dtors))
     LONG(0)
    __DTOR_END__ = .;

    *(.rdata)
    *(.rodata)
    *(.rodata.*)
    *(.gnu.linkonce.r.*)
    . = ALIGN(16);
    __text_end = .;
  } > rom
  __text_size = __text_end - __text_start;

  .data 0x06000000 :
  AT ( LOADADDR (.text) + SIZEOF (.text) )
  {
    __data_start = .;
    *(.data)
    *(.data.*)
    *(.gnu.linkonce.d.*)
    CONSTRUCTORS

    *(.lit8)
    *(.lit4)
    *(.sdata)
    *(.sdata.*)
    *(.gnu.linkonce.s.*)
    . = ALIGN(16);
    __data_end = .;
  } > ram
  __data_size = __data_end - __data_start;

  .bss :
  {
    __bss_start = .;
    *(.bss)
    *(.bss.*)
    *(.gnu.linkonce.b.*)
    *(.sbss)
    *(.sbss.*)
    *(.gnu.linkonce.sb.*)
    *(.scommon)
    *(COMMON)
    . = ALIGN(16);
    end = .;
    _end = end;
    __end = _end;
    __bss_end = .;
  } > ram
  __bss_size = __bss_end - __bss_start;

}
