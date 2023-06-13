        .text

        .align  4

        .global z80_vgm_start
z80_vgm_start:
        .incbin "z80_vgm.o80"
        .global z80_vgm_end
z80_vgm_end:

        .align  4

