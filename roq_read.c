/* ------------------------------------------------------------------------
 * Id Software's RoQ video file format decoder
 *
 * Dr. Tim Ferguson, 2001.
 * For more details on the algorithm:
 *         http://www.csse.monash.edu.au/~timf/videocodec.html
 *
 * This is a simple decoder for the Id Software RoQ video format.  In
 * this format, audio samples are DPCM coded and the video frames are
 * coded using motion blocks and vector quantisation.
 *
 * Note: All information on the RoQ file format has been obtained through
 *   pure reverse engineering.  This was achieved by giving known input
 *   audio and video frames to the roq.exe encoder and analysing the
 *   resulting output text and RoQ file.  No decompiling of the Quake III
 *   Arena game was required.
 *
 * You may freely use this source code.  I only ask that you reference its
 * source in your projects documentation:
 *       Tim Ferguson: http://www.csse.monash.edu.au/~timf/
 * ------------------------------------------------------------------------ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "roq.h"
#include "32x.h"

/* -------------------------------------------------------------------------- */

static inline int roq_fgetc(roq_file* fp) {
	return *fp->data++;
}

static inline int roq_fgetsc(roq_file* fp) {
	return *(int8_t *)fp->data++;
}

static inline int roq_feof(roq_file* fp) {
	return fp->eof && fp->data == NULL;
}

static inline unsigned short get_word(roq_file* fp)
{
	unsigned short ret;
	ret = (fp->data[0]);
	ret |= (fp->data[1]) << 8;
	fp->data += 2;
	return ret;
}

/* -------------------------------------------------------------------------- */
static inline unsigned int get_long(roq_file* fp)
{
	unsigned int ret;
	ret = (fp->data[0]);
	ret |= (fp->data[1]) << 8;
	ret |= (fp->data[2]) << 16;
	ret |= (fp->data[3]) << 24;
	fp->data += 4;
	return ret;
}


/* -------------------------------------------------------------------------- */
int roq_read_info(roq_file* fp, roq_info* ri)
{
	unsigned int head1;
	int head2;
	unsigned framerate;
	unsigned chunk_id;

	ri->get_chunk(fp);

	head1 = get_word(fp);
	head2 = get_long(fp);
	framerate = get_word(fp);

	ri->ret_chunk(ri->fp);

	if (head1 != RoQ_SIGNAGURE && head2 != -1)
	{
		//printf("Not an RoQ file.\n");
		return 1;
	}

	ri->framerate = framerate;
	ri->frametics = (ri->displayrate * 0x10000 / framerate) >> 16;
	ri->frametics_frac = (ri->displayrate * 0x10000 / framerate) & 0xffff;

	while (!roq_feof(fp))
	{
		ri->get_chunk(fp);

		if (roq_feof(fp)) {
			break;
		}

		chunk_id = get_word(fp);
		/*chunk_size = */get_long(fp);
		roq_fgetc(fp);
		roq_fgetc(fp);

		if (chunk_id == RoQ_INFO)		/* video info */
		{
			ri->width = get_word(fp);
			ri->height = get_word(fp);
		}

		ri->ret_chunk(ri->fp);

		if (chunk_id == RoQ_INFO)
			break;
	}

	if (roq_feof(fp))
	{
		// no info chunk
		return 1;
	}

	ri->canvas = ri->framebuffer;

	ri->display_height = ri->height;
	if (ri->canvas_pitch * ri->display_height < RoQ_MAX_CANVAS_SIZE)
	{
		if (ri->width < 320)
			ri->canvas += (320 - ri->width) / 2;
		ri->canvas = (void *)((uintptr_t)ri->canvas & ~15);
		ri->canvas_pitch = (160 + ri->width / 2 + 15) & ~15;
	}
	else
	{
		ri->canvas_pitch = ri->width;
	}
	while (ri->canvas_pitch * ri->display_height > RoQ_MAX_CANVAS_SIZE || ri->display_height > 224)
		ri->display_height -= 16;

	ri->frame_num = 0;

	return 0;
}

/* -------------------------------------------------------------------------- */
static inline void apply_motion_4x4(roq_parse_ctx* ctx, unsigned x, unsigned y, unsigned char mv, char mean_x, char mean_y)
{
	int mx, my, i;
	short *src, *dst;
	roq_info *ri = ctx->ri;

	mx = x + 8 - (mv / 16) - mean_x;
	my = y + 8 - (mv & 0xf) - mean_y;

	dst = ri->canvas + y * ri->canvas_pitch + x;
	src = ri->canvascopy + my * ri->canvas_pitch + mx;

	for (i = 0; i < 4; i++)
	{
		dst[0] = src[0];
		dst[1] = src[1];
		dst[2] = src[2];
		dst[3] = src[3];
		src += ri->canvas_pitch;
		dst += ri->canvas_pitch;
	}
}

/* -------------------------------------------------------------------------- */
static inline void apply_motion_8x8(roq_parse_ctx* ctx, unsigned x, unsigned y, unsigned char mv, char mean_x, char mean_y)
{
	int mx, my, i;
	short *src, *dst;
	roq_info *ri = ctx->ri;

	mx = x + 8 - (mv / 16) - mean_x;
	my = y + 8 - (mv & 0xf) - mean_y;
	
	dst = ri->canvas + y * ri->canvas_pitch + x;
	src = ri->canvascopy + my * ri->canvas_pitch + mx;

	for (i = 0; i < 8; i++)
	{
		dst[0] = src[0];
		dst[1] = src[1];
		dst[2] = src[2];
		dst[3] = src[3];
		dst[4] = src[4];
		dst[5] = src[5];
		dst[6] = src[6];
		dst[7] = src[7];
		src += ri->canvas_pitch;
		dst += ri->canvas_pitch;
	}
}

roq_info* roq_init(roq_info* ri, roq_file* fp, roq_getchunk_t getch, roq_retchunk_t retch, int displayrate, short *framebuffer)
{
	ri->fp = fp;
	ri->get_chunk = getch;
	ri->ret_chunk = retch;
	ri->framebuffer = framebuffer;
	ri->displayrate = displayrate;
	ri->cells = ri->cells_u + 128;
	ri->qcells = ri->qcells_u + 128;
	fp->backupdma_dest = (unsigned char *)framebuffer;
	return ri;
}

/* -------------------------------------------------------------------------- */

#define RMASK ((1<<5)-1)
#define GMASK (((1<<10)-1) & ~RMASK)
#define BMASK (((1<<15)-1) & ~(RMASK|GMASK))

#define YUVClip8(v) (__builtin_expect((v) & ~YUV_MASK2, 0) ? (__builtin_expect((int)(v) < 0, 0) ? 0 : YUV_MASK2) : (v))
#define YUVRGB555(r,g,b) ((((((r)) >> (10+(YUV_FIX2-7))))) | (((((g)) >> (5+(YUV_FIX2-7)))) & GMASK) | (((((b)) >> (0+(YUV_FIX2-7)))) & BMASK))

#define YUV_FIX2 8                   // fixed-point precision for YUV->RGB
const int YUV_MUL2 = (1 << YUV_FIX2);
const int YUV_NUDGE2 = (1 << (YUV_FIX2 - 1));
const int YUV_MASK2 = (256 << YUV_FIX2) - 1;

const int v1402C_ = 1.402000 * YUV_MUL2;
const int v0714C_ = 0.714136 * YUV_MUL2;

const int u0344C_ = 0.344136 * YUV_MUL2;
const int u1772C_ = 1.772000 * YUV_MUL2;

#define yuv2rgb555(y,u,v) \
	YUVRGB555( \
		YUVClip8(((unsigned)y << YUV_FIX2) + (v1402C_ * (v-128) + YUV_NUDGE2)), \
		YUVClip8(((unsigned)y << YUV_FIX2) - (u0344C_ * (u-128) + v0714C_ * (v-128) - YUV_NUDGE2)), \
		YUVClip8(((unsigned)y << YUV_FIX2) + (u1772C_ * (u-128) + YUV_NUDGE2)) \
	)

/* -------------------------------------------------------------------------- */

typedef int (*roq_applier)(roq_parse_ctx* ctx, unsigned x, unsigned y, char* buf);

static int roq_apply_mot(roq_parse_ctx* ctx, unsigned x, unsigned y, char* buf) RoQ_ATTR_SDRAM;
static int roq_apply_fcc(roq_parse_ctx* ctx, unsigned x, unsigned y, char* buf) RoQ_ATTR_SDRAM;
static int roq_apply_sld(roq_parse_ctx* ctx, unsigned x, unsigned y, char* buf) RoQ_ATTR_SDRAM;
static int roq_apply_cc(roq_parse_ctx* ctx, unsigned x, unsigned y, char* buf) RoQ_ATTR_SDRAM;

static int roq_apply_fcc2(roq_parse_ctx* ctx, unsigned x, unsigned y, char* buf) RoQ_ATTR_SDRAM;
static int roq_apply_sld2(roq_parse_ctx* ctx, unsigned x, unsigned y, char* buf) RoQ_ATTR_SDRAM;
static int roq_apply_cc2(roq_parse_ctx* ctx, unsigned x, unsigned y, char* buf) RoQ_ATTR_SDRAM;

roq_applier appliers[] = { &roq_apply_mot, &roq_apply_fcc, &roq_apply_sld, &roq_apply_cc };
roq_applier appliers2[] = { &roq_apply_mot, &roq_apply_fcc2, &roq_apply_sld2, &roq_apply_cc2 };

static inline int roq_read_vqid(roq_parse_ctx* ctx, unsigned char* buf, unsigned* pvqid)
{
	int adv = 0;
	unsigned vqid;

#ifdef MARS
	if (ctx->vqflg_pos == 0)
	{	
		ctx->vqflg = ((buf[1] << 8) | buf[0]) << 16;
		ctx->vqflg_pos = 8;
		adv = 2;
	}

	__asm volatile(
		"shll %1\n\t"
		"movt r0\n\t"
		"shll %1\n\t"
		"movt %0\n\t"
		"add r0, r0\n\t"
		"add r0, %0\n\t"
		: "=r"(vqid), "+r"(ctx->vqflg) : : "r0"
	);
#else
	if (ctx->vqflg_pos == 0)
	{
		unsigned shf;
		unsigned qfl = buf[0] | (buf[1] << 8);

		ctx->vqflg = 0;
		for (shf = 0; shf < 16; shf += 2) {
			ctx->vqflg <<= 2;
			ctx->vqflg |= (qfl >> shf) & 0x3;
		}

		ctx->vqflg_pos = 8;
		adv = 2;
	}

	vqid = ctx->vqflg & 0x3;
	ctx->vqflg >>= 2;
#endif

	*pvqid = vqid;
	ctx->vqflg_pos--;

	return adv;
}

static int roq_apply_mot(roq_parse_ctx* ctx, unsigned x, unsigned y, char* buf)
{
	return 0;
}

static int roq_apply_fcc(roq_parse_ctx* ctx, unsigned x, unsigned y, char* buf)
{
	apply_motion_8x8(ctx, x, y, buf[0], ctx->chunk_arg1, ctx->chunk_arg0);
	return 1;
}

static int roq_apply_sld(roq_parse_ctx* ctx, unsigned x, unsigned y, char* buf)
{
	int i, j;
	roq_info* ri = ctx->ri;
	unsigned pitch = ri->canvas_pitch;
	roq_qcell *qcell = ri->qcells + buf[0];
	short *dst = ri->canvas + y * pitch + x;
	short *dst2 = dst + pitch;

	pitch *= 2;

	for (i = 0; i < 4; i += 2)
	{
		roq_cell *cell0 = ri->cells + qcell->idx[i];
		roq_cell *cell1 = ri->cells + qcell->idx[i+1];

		for (j = 0; j < 2; j++)
		{
#ifdef MARS
			__asm volatile(
				"swap.w %2, r0\n\t"
				"mov.w r0, @(0, %0)\n\t" "mov.w r0, @( 2, %0)\n\t" "mov.w r0, @(0, %1)\n\t" "mov.w r0, @( 2, %1)\n\t"
				"swap.w r0, r0\n\t"
				"mov.w r0, @(8, %0)\n\t" "mov.w r0, @(10, %0)\n\t" "mov.w r0, @(8, %1)\n\t" "mov.w r0, @(10, %1)\n\t"
				: :  "r"(dst), "r"(dst2), "r"(cell0->rgb555x2[j]) : "r0");

			__asm volatile(
				"swap.w %2, r0\n\t"
				"mov.w r0, @( 4, %0)\n\t" "mov.w r0, @( 6, %0)\n\t" "mov.w r0, @( 4, %1)\n\t" "mov.w r0, @( 6, %1)\n\t"
				"swap.w r0, r0\n\t"
				"mov.w r0, @(12, %0)\n\t" "mov.w r0, @(14, %0)\n\t" "mov.w r0, @(12, %1)\n\t" "mov.w r0, @(14, %1)\n\t"
				: : "r"(dst), "r"(dst2), "r"(cell1->rgb555x2[j]) : "r0");
#else
			int a, b;

 			a = cell0->rgb555x2[j];
			dst[4] = dst[5] = dst2[4] = dst2[5] = a;
			a >>= 16;
			dst[0] = dst[1] = dst2[0] = dst2[1] = a;

			b = cell1->rgb555x2[j];
			dst[6] = dst[7] = dst2[6] = dst2[7] = b;
			b >>= 16;
			dst[2] = dst[3] = dst2[2] = dst2[3] = b;
#endif

			dst += pitch;
			dst2 += pitch;
		}
	}

	return 1;
}

static int roq_apply_cc(roq_parse_ctx* ctx, unsigned xp, unsigned yp, char* buf)
{
	unsigned k;
	unsigned x, y;
	int bpos = 0;

	for (k = 0; k < 4; k++)
	{
		unsigned vqid;

		x = (k & 1) * 4;
		x += xp;

		y = (k & 2) * 2;
		y += yp;

		bpos += roq_read_vqid(ctx, (uint8_t *)&buf[bpos], &vqid);

		bpos += appliers2[vqid](ctx, x, y, &buf[bpos]);
	}

	return bpos;
}

static int roq_apply_fcc2(roq_parse_ctx* ctx, unsigned x, unsigned y, char* buf)
{
	apply_motion_4x4(ctx, x, y, buf[0], ctx->chunk_arg1, ctx->chunk_arg0);
	return 1;
}

static int roq_apply_sld2(roq_parse_ctx* ctx, unsigned x, unsigned y, char* buf)
{
	roq_qcell *qcell = ctx->ri->qcells + buf[0];
	roq_apply_cc2(ctx, x, y, qcell->idx);
	return 1;
}

static int roq_apply_cc2(roq_parse_ctx* ctx, unsigned x, unsigned y, char* buf)
{
	roq_info* ri = ctx->ri;
	unsigned pitch = ri->canvas_pitch;
	int *dst = (int *)(ri->canvas + y * pitch + x);
	roq_cell *cell0, *cell1, *cell2, *cell3;

	pitch /= 2;

	cell0 = ri->cells + buf[0];
	cell1 = ri->cells + buf[1];

	dst[0] = cell0->rgb555x2[0];
	dst[1] = cell1->rgb555x2[0];
	dst += pitch;

	dst[0] = cell0->rgb555x2[1];
	dst[1] = cell1->rgb555x2[1];
	dst += pitch;

	cell2 = ri->cells + buf[2];
	cell3 = ri->cells + buf[3];

	dst[0] = cell2->rgb555x2[0];
	dst[1] = cell3->rgb555x2[0];
	dst += pitch;

	dst[0] = cell2->rgb555x2[1];
	dst[1] = cell3->rgb555x2[1];

	return 4;
}

/* -------------------------------------------------------------------------- */

void roq_read_vq(roq_parse_ctx *ctx, unsigned char *buf, int chunk_size, int dodma)
{
	int bpos = 0;
	int xpos, ypos;
	roq_info *ri = ctx->ri;
	int width = ri->width;
	int dmafrom = 0, dmato = 0;
	int finalxfer = 0;

	ypos = 0;
	for (bpos = 0; bpos < chunk_size; )
	{
		int xp, yp;

		for (xpos = 0; xpos < width; xpos += 16)
		{
			for (yp = ypos; yp < ypos + 16; yp += 8)
			{
				for (xp = xpos; xp < xpos + 16; xp += 8)
				{
					bpos += roq_read_vqid(ctx, &buf[bpos], &ctx->vqid);
					bpos += appliers[ctx->vqid](ctx, xp, yp, (char *)&buf[bpos]);
					if (bpos >= chunk_size)
						goto checknext;
				}
			}
		}

checknext:
		if (dodma)
		{
			int othery;

xfer:
			othery = ypos;
			if (othery > dmato)
			{
				if (dmato > 0) 
				{
					if (finalxfer)
					{
						while (!(SH2_DMA_CHCR1 & SH2_DMA_CHCR_TE)) ; // wait on TE
					}
					else
					{
						if (!(SH2_DMA_CHCR1 & SH2_DMA_CHCR_TE))
							goto next; // do not wait on TE
					}
					SH2_DMA_CHCR1 = 0; // clear TE
				}

				dmafrom = dmato;
				dmato = othery;

				// start DMA
				SH2_DMA_SAR1 = (uint32_t)(ri->canvas + ri->canvas_pitch*dmafrom/2*sizeof(short));
				SH2_DMA_DAR1 = (uint32_t)(ri->canvascopy + ri->canvas_pitch*dmafrom/2*sizeof(short));
				// xfer count (4 * # of 16 byte units)
				SH2_DMA_TCR1 = ((((dmato - dmafrom)*ri->canvas_pitch*sizeof(short)) >> 4) << 2);
				SH2_DMA_CHCR1 = SH2_DMA_CHCR_DM_INC|SH2_DMA_CHCR_SM_INC|SH2_DMA_CHCR_TS_16BU|SH2_DMA_CHCR_AR_ARM|SH2_DMA_CHCR_TB_CS|SH2_DMA_CHCR_DS_EDGE|SH2_DMA_CHCR_AL_AH|SH2_DMA_CHCR_DL_AH|SH2_DMA_CHCR_TA_DA|SH2_DMA_CHCR_DE;
			}

			if (finalxfer)
			{
				while (!(SH2_DMA_CHCR1 & SH2_DMA_CHCR_TE)) ; // wait on TE
				return;
			}
		}

next:
		xpos = 0;
		ypos += 16;
		if (ypos >= ri->display_height)
			break;
	}

	if (dodma && dmato < ri->display_height)
	{
		finalxfer = 1;
		goto xfer;
	}
}

int roq_read_frame(roq_info* ri, char loop, void (*finish)(void))
{
	int i, nv1, nv2;
	roq_file* fp = ri->fp;
	int chunk_id = 0, chunk_arg0 = 0, chunk_arg1 = 0;
	int chunk_size = 0;
	roq_parse_ctx ctx;

	while (1)
	{
		chunk_id = 0;

		ri->get_chunk(fp);

		if (roq_feof(fp)) {
			break;
		}

		chunk_id = get_word(fp);
		chunk_size = get_long(fp);
		chunk_arg0 = roq_fgetc(fp);
		chunk_arg1 = roq_fgetc(fp);

		if (chunk_id == RoQ_QUAD_VQ)
			break;

		switch (chunk_id)
		{
		case RoQ_QUAD_CODEBOOK:
			if ((nv1 = chunk_arg1) == 0) nv1 = 256;
			if ((nv2 = chunk_arg0) == 0 && (nv1 * 6 < chunk_size)) nv2 = 256;

			for (i = 0; i < nv1; i++)
			{
				uint8_t y[4], u, v;
				roq_cell *cell = ri->cells + (int8_t)i;

				y[0] = roq_fgetsc(fp);
				y[1] = roq_fgetsc(fp);
				y[2] = roq_fgetsc(fp);
				y[3] = roq_fgetsc(fp);
				u = roq_fgetsc(fp);
				v = roq_fgetsc(fp);

				cell->rgb555[0] = yuv2rgb555(y[0], u, v);
				cell->rgb555[1] = yuv2rgb555(y[1], u, v);
				cell->rgb555[2] = yuv2rgb555(y[2], u, v);
				cell->rgb555[3] = yuv2rgb555(y[3], u, v);
			}

			if (nv2 > 127)
			{
				memcpy(ri->qcells, fp->data, 128*4);
				memcpy(ri->qcells - 128, fp->data + 128 * 4, (nv2 - 128)*4);
			}
			else
			{
				memcpy(ri->qcells, fp->data, nv2*4);
			}
			break;
		}

		ri->ret_chunk(ri->fp);
	}

	finish();

	if (chunk_id != RoQ_QUAD_VQ)
	{
		ri->ret_chunk(ri->fp);
		return 0;
	}

	ri->frame_num++;

	ctx.ri = ri;
	ctx.chunk_arg0 = chunk_arg0;
	ctx.chunk_arg1 = chunk_arg1;
	ctx.vqflg = 0;
	ctx.vqflg_pos = 0;
	ctx.vqid = RoQ_ID_MOT;

	roq_read_vq(&ctx, fp->data, chunk_size, 1);

	ri->ret_chunk(ri->fp);
	return 1;
}
