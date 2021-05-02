
		.text
		.align	4

		/* signature */
		dc.w	0x4EF9,0x5555,0xAAAA,0x4EF9

vgm_tbl:
		dc.l	vgm_file0 - vgm_tbl
		dc.l	vgm_file1 - vgm_tbl
		dc.l	vgm_file2 - vgm_tbl
		dc.l	vgm_file3 - vgm_tbl
		dc.l	vgm_file4 - vgm_tbl
		dc.l	vgm_file5 - vgm_tbl
		dc.l	vgm_file6 - vgm_tbl
		dc.l	vgm_file7 - vgm_tbl
		dc.l	vgm_file8 - vgm_tbl
		dc.l	vgm_file9 - vgm_tbl
		dc.l	vgm_file10 - vgm_tbl

/*
 * from sounds.h
 *  mus_None, 
 *  mus_e1m1,
 *  mus_e1m2,
 *  mus_e1m4,
 *  mus_e1m6,
 *  mus_e2m1,
 *  mus_e2m2,
 *  mus_e2m3,
 *  mus_e2m6,
 *  mus_e2m8,
 *  mus_e3m2,
 *  mus_intro,
 */
		.align	4

vgm_file0:
		.incbin	"../VGM/E1M1.vgm"
vgm_file1:
		.incbin	"../VGM/E1M2.vgm"
vgm_file2:
		.incbin	"../VGM/E1M4.vgm"
vgm_file3:
		.incbin	"../VGM/E1M6.vgm"
vgm_file4:
		.incbin	"../VGM/E2M1.vgm"
vgm_file5:
		.incbin	"../VGM/E2M2.vgm"
vgm_file6:
		.incbin	"../VGM/Inter.vgm"
vgm_file7:
		.incbin	"../VGM/E2M6.vgm"
vgm_file8:
		.incbin	"../VGM/E2M8.vgm"
vgm_file9:
		.incbin	"../VGM/E3M2.vgm"
vgm_file10:
		.incbin	"../VGM/Intro.vgm"

		.align	4
