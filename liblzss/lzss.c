#include <stdint.h>
#include <string.h>
#include "lzss.h"

#define LENSHIFT 4		/* this must be log2(LOOKAHEAD_SIZE) */

#ifdef __m68k__
#define SSHORT int16_t
#define USHORT uint16_t
static void lzss_copy(int8_t *dst, const int8_t *src, USHORT size) __attribute__((optimize("O2"))) __attribute__((noinline));
#else
#define SSHORT int
#define USHORT int
static void lzss_copy(int8_t *dst, const int8_t *src, USHORT size) __attribute__((optimize("Os"))) __attribute__((noinline));
#endif

void lzss_reset(lzss_state_t* lzss)
{
	lzss->outpos = 0;
	lzss->eof = 0;
	lzss->run = 0;
	lzss->runlen = 0;
	lzss->runpos = 0;
	lzss->input = lzss->base;
	lzss->idbyte = 0;
	lzss->getidbyte = 0;
}

void lzss_setup(lzss_state_t* lzss, uint8_t* base, uint8_t* buf, uint32_t buf_size)
{
	lzss->base = base;
	lzss->buf = buf;
	lzss->buf_size = buf_size;
	lzss->buf_mask = buf_size - 1;
	lzss_reset(lzss);
}

static void lzss_copy(int8_t *dst, const int8_t *src, USHORT size)
{
	if ( ((intptr_t)src & 1) == ((intptr_t)dst & 1) ) {
		SSHORT r;

		if ( ((intptr_t)src & 1) ) {
			*dst++ = *src++;
			size--;
		}

		for (r = size / 2; r > 0; r--) {
			*((int16_t *)dst) = *((int16_t *)src);
			dst += 2, src += 2;
		}

		size = size & 1;
	}

	while (size-- > 0) {
		*dst++ = *src++;
	}
}

int lzss_read(lzss_state_t* lzss, uint16_t chunk)
{
	USHORT i = lzss->run;
	USHORT len = lzss->runlen;
	int32_t source = lzss->runpos;
	uint8_t* input = lzss->input;
	int8_t* output = (int8_t *)lzss->buf;
	int32_t outpos = lzss->outpos;
	uint8_t idbyte = lzss->idbyte;
	uint8_t getidbyte = lzss->getidbyte;
	USHORT left = chunk;
	USHORT buf_size = lzss->buf_size;
	USHORT buf_mask = lzss->buf_mask;

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
			USHORT limit, rem;
			USHORT pos;

			/* decompress */
			if (buf_size <= 0x1000)
			{
				pos = *input++ << LENSHIFT;
				pos = pos | (*input >> LENSHIFT);
				len = (*input++ & 0xf) + 1;
			}
			else
			{
				pos = *input++ << 8;
				pos = pos | (*input++);
				len = (*input++ & 0xff) + 1;
			}
			source = outpos - pos - 1;

			if (len == 1) {
				/* end of stream */
				lzss->eof = 1;
				return chunk - left;
			}

		runcopy:
			limit = len - i;
			if (limit > left) limit = left;

			rem = limit;
			while (rem > 0)
			{
				USHORT chunk;
				USHORT source_masked, outpos_masked;

				chunk = rem;

				source_masked = source & buf_mask;
				if (source_masked + chunk > buf_size)
					chunk = buf_size - source_masked;

				outpos_masked = outpos & buf_mask;
				if (outpos_masked + chunk > buf_size)
					chunk = buf_size - outpos_masked;

				source += chunk;
				outpos += chunk;
				rem -= chunk;

				lzss_copy(&output[outpos_masked], &output[source_masked], chunk);
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
			output[outpos & buf_mask] = *input++;
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

int lzss_read_all(lzss_state_t* lzss)
{
	USHORT len;
	uint8_t getidbyte = 0;
	uint32_t source = 0;
	uint8_t* input = lzss->input;
	uint8_t* output = lzss->buf;
	uint32_t outpos = 0;
	uint32_t buf_size = lzss->buf_size;

	if (!lzss->input)
		return 0;

	len = 0;
	getidbyte = 0;
	while (1)
	{
		uint8_t idbyte;

		/* get a new idbyte if necessary */
		if (!getidbyte) idbyte = *input++;

		if (idbyte & 1)
		{
			USHORT j;
			USHORT pos;

			/* decompress */
			if (buf_size <= 0x1000)
			{
				pos = *input++ << LENSHIFT;
				pos = pos | (*input >> LENSHIFT);
				len = (*input++ & 0xf) + 1;
			}
			else
			{
				pos = *input++ << 8;
				pos = pos | (*input++);
				len = (*input++ & 0xff) + 1;
			}
			source = outpos - pos - 1;

			if (len == 1) {
				/* end of stream */
				return output - lzss->buf;
			}

			for (j = 0; j < len; j++)
				output[outpos++] = output[source++];
		}
		else {
			output[outpos++] = *input++;
		}
		idbyte = idbyte >> 1;
		getidbyte = (getidbyte + 1) & 7;
	}

	return output - lzss->buf;
}

uint32_t lzss_decompressed_size(lzss_state_t* lzss)
{
	USHORT len;
	uint8_t getidbyte = 0;
	uint8_t* input = lzss->input;
	uint32_t outpos = 0;
	uint32_t buf_size = lzss->buf_size;

	if (!lzss->input)
		return 0;

	len = 0;
	getidbyte = 0;
	while (1)
	{
		uint8_t idbyte;

		/* get a new idbyte if necessary */
		if (!getidbyte) idbyte = *input++;

		len = 1;
		if (idbyte & 1)
		{
			/* decompress */
			if (buf_size <= 0x1000)
			{
				input++;
				len = (*input & 0xf) + 1;
			}
			else
			{
				input++, input++;
				len = (*input & 0xff) + 1;
			}

			if (len == 1) {
				/* end of stream */
				return outpos;
			}
		}

		input++;
		outpos += len;
		idbyte = idbyte >> 1;
		getidbyte = (getidbyte + 1) & 7;
	}

	return outpos;
}

uint32_t lzss_compressed_size(lzss_state_t* lzss)
{
	USHORT len;
	uint8_t getidbyte;
	uint8_t* input = lzss->input;
	uint32_t buf_size = lzss->buf_size;

	if (!lzss->input)
		return 0;

	getidbyte = 0;
	while (1)
	{
		uint8_t idbyte;

		/* get a new idbyte if necessary */
		if (!getidbyte) idbyte = *input++;

		len = 1;
		if (idbyte & 1)
		{
			/* decompress */
			if (buf_size <= 0x1000)
			{
				input++;
				len = (*input & 0xf) + 1;
			}
			else
			{
				input++, input++;
				len = (*input & 0xff) + 1;
			}

			if (len == 1) {
				/* end of stream */
				return input - lzss->input + 1;
			}
		}

		input++;
		idbyte = idbyte >> 1;
		getidbyte = (getidbyte + 1) & 7;
	}

	return input - lzss->input;
}

int lzss_decompress(uint8_t* input, uint8_t* output, uint32_t size)
{
	lzss_state_t lzss;
	lzss_setup(&lzss, input, output, size);
	return lzss_read_all(&lzss);
}
