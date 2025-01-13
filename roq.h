#ifndef _av_roq_h
#define _av_roq_h

#include <stdint.h>

#define RoQ_INFO				0x1001
#define RoQ_QUAD_CODEBOOK		0x1002
#define RoQ_QUAD_VQ				0x1011
#define RoQ_SOUND_MONO			0x1020
#define RoQ_SOUND_STEREO		0x1021
#define RoQ_SIGNAGURE 			0x1084

#define RoQ_ID_MOT		0x00
#define RoQ_ID_FCC		0x01
#define RoQ_ID_SLD		0x02
#define RoQ_ID_CCC		0x03

#define RoQ_MAX_CANVAS_SIZE 288*224

#define RoQ_SAMPLE_RATE    22050

#ifdef __32X__
#define RoQ_ATTR_SDRAM
//#define RoQ_ATTR_SDRAM  __attribute__((section(".data"), aligned(16)))
#else
#define RoQ_ATTR_SDRAM
#endif

typedef struct {
	uint8_t y0123[4];
	uint8_t uvb[2];
} roq_yuvcell;

typedef struct {
	union {
		short rgb555[4];
		int rgb555x2[2];
	};
} roq_cell;

typedef struct {
	char idx[4];
} roq_qcell;

typedef struct {
	unsigned char* data;
	unsigned char* page_base;
	unsigned char* page_end;
	unsigned char *dma_base;
	unsigned char *dma_dest;
	unsigned char *snddma_base;
	unsigned char *snddma_dest;
	short eof, bof;
	unsigned char *backupdma_dest;
	volatile char in_dma;
} roq_file;

typedef void (*roq_getchunk_t)(roq_file*);
typedef void (*roq_retchunk_t)(roq_file*);

typedef struct {
	unsigned chunk_arg0, chunk_arg1;
	unsigned vqflg;
	unsigned vqflg_pos;
	unsigned vqid;
	unsigned char *buf;
	int buf_len;
	struct roq_info_s *ri;
} roq_parse_ctx;

typedef struct roq_info_s {
	short *canvascopy;
	roq_cell cells_u[256];
	roq_qcell qcells_u[256];
	roq_cell *cells;
	roq_qcell *qcells;
	roq_file *fp;
	int buf_size;
	unsigned width, height, frame_num;
	int display_height;
	roq_getchunk_t get_chunk;
	roq_retchunk_t ret_chunk;
	short *framebuffer, *canvas;
	int canvas_pitch;
	unsigned framerate, displayrate, frametics, frametics_frac;
} roq_info;

/* -------------------------------------------------------------------------- */

void roq_cleanup(void);
roq_info *roq_init(roq_info* ri, roq_file *fp, roq_getchunk_t getch, roq_retchunk_t retch, int displayrate, short *framebuffer);
int roq_read_info(roq_file* fp, roq_info* ri);
int roq_read_frame(roq_info* ri, char loop, void (*finish)(void))
	RoQ_ATTR_SDRAM
	;

#endif

