OUTPUT_ARCH(m68k)
EXTERN (_start)
ENTRY (_start)
__DYNAMIC  =  0;

/*
 * The memory map looks like this:
 * +--------------------+ <- 0x00006000
 * | .text              |
 * |                    |
 * |          _text_end |
 * +--------------------+
 * | .data              | initialized data goes here
 * |                    |
 * |          _data_end |
 * +--------------------+
 * | .bss               |
 * |         _bss_start | start of bss, cleared by crt0
 * |                    |
 * |          _bss__end | start of heap, used by sbrk()
 * +--------------------+
 * .                    .
 * .                    .
 * .                    .
 * |             _stack | top of stack
 * +--------------------+ <- 0x00080000
 */

MEMORY
{
    ram (wx) : ORIGIN = 0x00006000, LENGTH = 0x0007A000
}

/*
 * Allocate the stack to be at the top of memory, since the stack
 * grows down
 */

PROVIDE (__stack = 0x00080000);

SECTIONS
{
  .text 0x00006000 :
  {
    __text_start = .;
    *(.text)
    *(.text.*)
    *(.gnu.linkonce.t.*)

    . = ALIGN(16);
    __INIT_SECTION__ = .;
    KEEP (*(.init))
    SHORT (0x4E75)  /* rts */
    . = ALIGN(16);
    __FINI_SECTION__ = .;
    KEEP (*(.fini))
    SHORT (0x4E75)  /* rts */

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
  } > ram
  __text_size = __text_end - __text_start;

  .data :
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
    . = ALIGN(8);
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
    . = ALIGN(8);
    end = .;
    _end = end;
    __end = _end;
    __bss_end = .;
  } > ram
  __bss_size = __bss_end - __bss_start;

}
