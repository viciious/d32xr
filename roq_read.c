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

static inline int roq_fgetc(roq_file* fp) {
	return *fp->data++;
}

static inline int roq_fgetsc(roq_file* fp) {
	return *(int8_t *)fp->data++;
}

static inline int roq_feof(roq_file* fp) {
	return fp->eof && fp->data == NULL;
}

static inline int get_word(roq_file* fp)
{
	int ret;
	ret = (fp->data[0]);
	ret |= (fp->data[1]) << 8;
	fp->data += 2;
	return ret;
}

static inline int get_long(roq_file* fp)
{
	int ret;
	ret = (fp->data[0]);
	ret |= (fp->data[1]) << 8;
	ret |= (fp->data[2]) << 16;
	ret |= (fp->data[3]) << 24;
	fp->data += 4;
	return ret;
}

int roq_read_info(roq_file* fp, roq_info* ri)
{
	int16_t head1;
	int head2;
	uint8_t framerate;
	int16_t chunk_id;

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
	ri->frametics = (ri->displayrate * 0x10000 / framerate);

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

	return 0;
}

static inline void apply_motion_4x4(roq_parse_ctx* ctx, short * restrict dst, int mv)
{
	int mypitch, i;
	short * restrict src;
	int16_t mean_x = ctx->chunk_arg1;
	int16_t mean_y = ctx->chunk_arg0;
	int pitch = ctx->canvas_pitch;

	mypitch = (8 - (mv & 0xf) - mean_y) * pitch;
	mypitch += 8 - (mv >> 4) - mean_x;
	src = (short *)((intptr_t)dst + ctx->canvastocopy) + mypitch;

	for (i = 0; i < 4; i++)
	{
		dst[0] = src[0];
		dst[1] = src[1];
		dst[2] = src[2];
		dst[3] = src[3];
		src += pitch;
		dst += pitch;
	}
}

static inline void apply_motion_8x8(roq_parse_ctx* ctx, short * restrict dst, int mv)
{
	int mypitch, i;
	short * restrict src;
	int16_t mean_x = ctx->chunk_arg1;
	int16_t mean_y = ctx->chunk_arg0;
	int pitch = ctx->canvas_pitch;

	mypitch = (8 - (mv & 0xf) - mean_y) * pitch;
	mypitch += 8 - (mv >> 4) - mean_x;
	src = (short *)((intptr_t)dst + ctx->canvastocopy) + mypitch;

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
		src += pitch;
		dst += pitch;
	}
}

void roq_init(roq_info* ri, roq_file* fp, roq_getchunk_t getch, roq_retchunk_t retch, int displayrate, short *framebuffer)
{
	ri->fp = fp;
	ri->get_chunk = getch;
	ri->ret_chunk = retch;
	ri->canvas = framebuffer;
	ri->displayrate = displayrate;
	ri->cells = ri->cells_u + 128;
	ri->qcells = ri->qcells_u + 128;
	fp->backupdma_dest = (unsigned char *)framebuffer;
}

/* -------------------------------------------------------------------------- */

static char *roq_apply_fcc(roq_parse_ctx* ctx, short * restrict dst, char* buf) RoQ_ATTR_SDRAM;
static char *roq_apply_sld(roq_parse_ctx* ctx, short * restrict dst, char* buf) RoQ_ATTR_SDRAM;
static char *roq_apply_cc(roq_parse_ctx* ctx, short * restrict dst, char* buf) RoQ_ATTR_SDRAM;

static char *roq_apply_fcc2(roq_parse_ctx* ctx, short * restrict dst, char* buf) RoQ_ATTR_SDRAM;
static char *roq_apply_sld2(roq_parse_ctx* ctx,  short * restrict dst, char* buf) RoQ_ATTR_SDRAM;
static char *roq_apply_cc2(roq_parse_ctx* ctx, short * restrict dst, char* buf) RoQ_ATTR_SDRAM;

static void roq_read_vq(roq_parse_ctx *ctx, char *buf, char *chunk_end, int (*copyscr)(roq_info* , char , int , int )) RoQ_ATTR_SDRAM;

#define roq_read_vqid(ctx,buf,vqid) do { \
	if (ctx->vqflg_pos == 0) { \
		ctx->vqflg = ((buf[1] << 8) | (uint8_t)buf[0]) << 16; \
		ctx->vqflg_pos = 8; \
		buf = buf + 2; \
	} \
	ctx->vqflg_pos--; \
	vqid = (((unsigned)ctx->vqflg >> 16) << 2) >> 16; \
	ctx->vqflg <<= 2; \
} while(0)

static char *roq_apply_fcc(roq_parse_ctx* ctx, short * restrict dst, char* buf)
{
	apply_motion_8x8(ctx, dst, (uint8_t)buf[0]);
	return buf+1;
}

static char *roq_apply_sld(roq_parse_ctx* ctx, short * restrict _dst, char* buf)
{
	int i, j;
	int pitch = ctx->canvas_pitch;
	roq_qcell *qcell = ctx->qcells + buf[0];
	int *restrict dst = (int *)_dst;
	int *restrict dst2 = (int *)(_dst + pitch);

	for (i = 0; i < 4; i += 2)
	{
		roq_cell *cell0 = ctx->cells + qcell->idx[i];
		roq_cell *cell1 = ctx->cells + qcell->idx[i+1];

		for (j = 0; j < 2; j++)
		{
			int a;

 			a = cell0->rgb555x2[j];
			dst[2] = dst2[2] = a;
			a = (((uint16_t)a)<<16)|((uint16_t)(a>>16)); // swap.w a,a
			dst[0] = dst2[0] = a;

			a = cell1->rgb555x2[j];
			dst[1] = dst2[1] = a;
			a = (((uint16_t)a)<<16)|((uint16_t)(a>>16)); // swap.w a,a
			dst[3] = dst2[3] = a;

			dst += pitch;
			dst2 += pitch;
		}
	}

	return buf+1;
}

static char *roq_apply_cc(roq_parse_ctx* ctx, short * restrict dst, char* buf)
{
	int i;
	int16_t pitch = ctx->canvas_pitch;
	int vqid;

	for (i = 0; i < 4; i++) {
		short * restrict idst;

		idst = dst + ((i & 1) << 2) + ((i & 2) << 1) * pitch;

		roq_read_vqid(ctx, buf, vqid);

		switch (vqid) {
			case RoQ_ID_MOT:
				break;
			case RoQ_ID_FCC:
				buf = roq_apply_fcc2(ctx, idst, buf);
				break;
			case RoQ_ID_SLD:
				buf = roq_apply_sld2(ctx, idst, buf);
				break;
			case RoQ_ID_CCC:
				buf = roq_apply_cc2(ctx, idst, buf);
				break;
		}
	}

	return buf;
}

static char *roq_apply_fcc2(roq_parse_ctx* ctx, short * restrict dst, char* buf)
{
	apply_motion_4x4(ctx, dst, (uint8_t)buf[0]);
	return buf+1;
}

static char *roq_apply_sld2(roq_parse_ctx* ctx, short * restrict dst, char* buf)
{
	roq_qcell *qcell = ctx->qcells + buf[0];
	roq_apply_cc2(ctx, dst, qcell->idx);
	return buf+1;
}

static inline char *roq_apply_cc2(roq_parse_ctx* ctx, short * restrict _dst, char* buf)
{
	int * restrict dst = (void*)_dst;
	int pitch = ctx->canvas_pitch;
	roq_cell *cell0, *cell1, *cell2, *cell3;

	pitch >>= 1;

	cell0 = ctx->cells + buf[0];
	cell1 = ctx->cells + buf[1];

	dst[0] = cell0->rgb555x2[0];
	dst[1] = cell1->rgb555x2[0];
	dst += pitch;

	dst[0] = cell0->rgb555x2[1];
	dst[1] = cell1->rgb555x2[1];
	dst += pitch;

	cell2 = ctx->cells + buf[2];
	cell3 = ctx->cells + buf[3];

	dst[0] = cell2->rgb555x2[0];
	dst[1] = cell3->rgb555x2[0];
	dst += pitch;

	dst[0] = cell2->rgb555x2[1];
	dst[1] = cell3->rgb555x2[1];

	return buf+4;
}

static void roq_read_vq(roq_parse_ctx *ctx, char *buf, char *chunk_end, int (*copyscr)(roq_info* , char , int , int ))
{
	int16_t xpos, ypos;
	roq_info *ri = ctx->ri;
	int height = ri->display_height;
	int width = ri->width;
	int pitch = ri->canvas_pitch;
	int8_t vqid = RoQ_ID_MOT;
	int16_t dmafrom = 0, dmato = 0;

	for (ypos = 0; ypos < height; ypos += 16)
	{
		int xp;

		if (copyscr(ri, 0, dmafrom, dmato)) {
			dmafrom = dmato;
		}

		for (xpos = 0; xpos < width; xpos += 16)
		{
			int yp;
			short *dst = ctx->canvas + ypos * pitch;

			for (yp = 0; yp < 16; yp += 8)
			{
				for (xp = xpos; xp < xpos + 16; xp += 8)
				{
					if (buf >= chunk_end)
					{
						ypos += 16;
						goto done;
					}

					roq_read_vqid(ctx, buf, vqid);

					switch (vqid) {
						case RoQ_ID_MOT:
							break;
						case RoQ_ID_FCC:
							buf = roq_apply_fcc(ctx, dst + xp, buf);
							break;
						case RoQ_ID_SLD:
							buf = roq_apply_sld(ctx, dst + xp, buf);
							break;
						case RoQ_ID_CCC:
							buf = roq_apply_cc(ctx, dst + xp, buf);
							break;
					}
				}

				dst += 8 * pitch;
			}
		}

		dmato = ypos;
	}

done:
	copyscr(ri, 1, dmafrom, ypos);
}

int roq_read_frame(roq_info* ri, char loop, void (*finish)(roq_info*), int (*copyscr)(roq_info* , char , int , int ))
{
	int i;
	int16_t nv1, nv2;
	roq_file* fp = ri->fp;
	int16_t chunk_id = 0, chunk_arg0 = 0, chunk_arg1 = 0;
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
			if ((nv2 = chunk_arg0) == 0 && ((int)nv1 * 6 < chunk_size)) nv2 = 256;

			for (i = 0; i < nv1; i++)
			{
				int j;
				uint8_t y[4], u, v;
				roq_cell *cell = ri->cells + (int8_t)i;

				y[0] = roq_fgetsc(fp);
				y[1] = roq_fgetsc(fp);
				y[2] = roq_fgetsc(fp);
				y[3] = roq_fgetsc(fp);
				u = roq_fgetsc(fp);
				v = roq_fgetsc(fp);

				for (j = 0; j < 4; j++) {
					int r, g, b;
					int rgb555;

					int Y = (y[j]   ) * (256 * 32);
					int U  = (u - 128) * (256 * 11);
					int V  = (v - 128) * (256 * 23);

					// this isn't the exact conversion formula, but it's good enough
					r = (int)(Y + V + V) >> 16;
					g = (int)(Y - U - V) >> 16;
					b = (int)(Y + (U << 2) + U) >> 16;

					rgb555  = ri->gb8clip5[b] << 8;
					rgb555 |= ri->gb8clip5[g] << 3;
					rgb555 |= ri->r8clip5 [r];

					cell->rgb555[j] = rgb555;
				}
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

	if (finish) {
		finish(ri);
	}

	if (chunk_id != RoQ_QUAD_VQ)
	{
		ri->ret_chunk(ri->fp);
		return 0;
	}

	ctx.ri = ri;
	ctx.chunk_arg0 = chunk_arg0;
	ctx.chunk_arg1 = chunk_arg1;
	ctx.vqflg = 0;
	ctx.vqflg_pos = 0;
	ctx.canvas = ri->canvas;
	ctx.canvastocopy = (intptr_t)ri->canvascopy - (intptr_t)ri->canvas;
	ctx.cells = ri->cells;
	ctx.qcells = ri->qcells;
	ctx.canvas_pitch = ri->canvas_pitch;

	roq_read_vq(&ctx, (char *)fp->data, (char *)fp->data+chunk_size, copyscr);

	ri->ret_chunk(ri->fp);
	return 1;
}
