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
	unsigned short data_length;
	volatile unsigned short dma_length;
	volatile unsigned char *dma_dest;
	volatile void *dma_target;
	volatile unsigned short rem_chunk_size;
	volatile unsigned short chunk_id;
	short eof, bof;
	unsigned char *backupdma_dest;
	char clear_cache; // whether the CPU cache needs to be cleared after reading a chunk
	volatile char oom;
} roq_file;

typedef void (*roq_getchunk_t)(roq_file*);
typedef void (*roq_retchunk_t)(roq_file*);

typedef struct {
	int8_t chunk_arg0, chunk_arg1;
	short vqflg_pos;
	int vqflg;
	unsigned char *buf;
	roq_cell *cells;
	roq_qcell *qcells;
	struct roq_info_s *ri;
	short *canvas;
	intptr_t canvastocopy;
	short canvas_pitch;	
} roq_parse_ctx;

typedef struct roq_info_s {
	short *canvas;
	short *canvascopy;
	roq_cell *cells_u;
	roq_qcell *qcells_u;
	roq_cell *cells;
	roq_qcell *qcells;
	roq_file *fp;
	roq_getchunk_t get_chunk;
	roq_retchunk_t ret_chunk;
	short display_height;
	short canvas_pitch;
	short width, height;
	uint8_t framerate;
	uint8_t displayrate;
	int8_t *r8clip5, *gb8clip5;
	unsigned frametics; // 16.16 fixed point value of the number of tics per frame, e.g. 0x50000 for 12fps on NTSC
} roq_info;

/* -------------------------------------------------------------------------- */

void roq_init(roq_info* ri, roq_file *fp, roq_getchunk_t getch, roq_retchunk_t retch, int displayrate, short *framebuffer);
int roq_read_info(roq_file* fp, roq_info* ri) __attribute__((optimize("Os")));
int roq_read_frame(roq_info* ri, char loop, void (*finish)(roq_info*), int (*copyscr)(roq_info* , char , int , int ))
	RoQ_ATTR_SDRAM
	;

#endif

