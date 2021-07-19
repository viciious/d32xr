#include <stdint.h>
#include <string.h>
#include "lzss.h"

#define LENSHIFT 4		/* this must be log2(LOOKAHEAD_SIZE) */

static inline void lzss_reset(lzss_state_t* lzss);

static void lzss_reset(lzss_state_t* lzss)
{
	lzss->eof = 0;
	lzss->run = 0;
	lzss->runlen = 0;
	lzss->runpos = 0;
	lzss->input = lzss->base;
	lzss->idbyte = 0;
	lzss->getidbyte = 0;
}

void lzss_setup(lzss_state_t* lzss, uint8_t* base)
{
	lzss->outpos = 0;
	lzss->base = base;
	lzss_reset(lzss);
}

int lzss_read(lzss_state_t* lzss, uint16_t chunk)
{
	uint16_t i = lzss->run;
	uint16_t len = lzss->runlen;
	uint32_t source = lzss->runpos;
	uint8_t* input = lzss->input;
	uint8_t* output = lzss->buf;
	uint32_t outpos = lzss->outpos;
	uint8_t idbyte = lzss->idbyte;
	uint8_t getidbyte = lzss->getidbyte;
	uint16_t left = chunk;

	if (!lzss->input)
		return 0;
	if (!left)
		return 0;
	if (lzss->eof)
		return 0;

	if (len)
		goto runcopy;
	if (getidbyte)
		goto crunch;

	i = 0;
	len = 0;

	while (left > 0)
	{
		/* get a new idbyte if necessary */
		if (!getidbyte) idbyte = *input++;

	crunch:
		if (idbyte & 1)
		{
			uint16_t j, limit;
			uint16_t pos;
			uint32_t outpos_masked, source_masked;

			/* decompress */
#if LZSS_BUF_SIZE <= 0x1000
			pos = *input++ << LENSHIFT;
			pos = pos | (*input >> LENSHIFT);
			len = (*input++ & 0xf) + 1;
#else
			pos = *input++ << 8;
			pos = pos | (*input++);
			len = (*input++ & 0xff) + 1;
#endif
			source = outpos - pos - 1;

			if (len == 1) {
				/* end of stream */
				lzss->eof = 1;
				return chunk - left;
			}

		runcopy:
			limit = len - i;
			if (limit > left) limit = left;

			outpos_masked = outpos & LZSS_BUF_MASK;
			source_masked = source & LZSS_BUF_MASK;
			if ((outpos_masked < ((outpos + limit) & LZSS_BUF_MASK)) &&
				(source_masked < ((source + limit) & LZSS_BUF_MASK)))
			{
				for (j = 0; j < limit; j++)
					output[outpos_masked++] = output[source_masked++];
				outpos += limit;
				source += limit;
			}
			else
			{
				for (j = 0; j < limit; j++) {
					output[outpos & LZSS_BUF_MASK] = output[source & LZSS_BUF_MASK];
					outpos++;
					source++;
				}
			}
			left -= limit;
			i += limit;

			if (i == len) {
				i = len = 0;
				idbyte = idbyte >> 1;
				getidbyte = (getidbyte + 1) & 7;
			}
		}
		else {
			output[outpos & LZSS_BUF_MASK] = *input++;
			outpos++;
			left--;
			idbyte = idbyte >> 1;
			getidbyte = (getidbyte + 1) & 7;
		}
	}

	lzss->getidbyte = getidbyte;
	lzss->idbyte = idbyte;
	lzss->run = i;
	lzss->runlen = len;
	lzss->runpos = source;
	lzss->input = input;
	lzss->outpos = outpos;

	return chunk - left;
}
