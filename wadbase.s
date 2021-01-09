        .text

        .align  4
doom32xwad:
        .incbin "doom32x.wad"

        .global _wadBase
_wadBase:
        .long   doom32xwad

