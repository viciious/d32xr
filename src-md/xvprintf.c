#include <stdint.h>
#include <string.h>

#ifndef TEST
static char* xvprintfs(char* buf, char* str) __attribute__((section(".data"), aligned(16)));
static char* xvprintfd(char* buf, int val) __attribute__((section(".data"), aligned(16)));
static char* xvprintfx(char* buf, unsigned int val) __attribute__((section(".data"), aligned(16)));
void xvprintf(char* buf, char* fmt, int val) __attribute__((section(".data"), aligned(16)));
#endif

/*
 * vprintf function support for MD side
 */

static inline char* xvputc(char* buf, char c)
{
	*buf++ = c;
	return buf;
}

static char* xvprintfs(char* buf, char* str)
{
	while (*str)
		buf = xvputc(buf, *str++);

	return buf;
}

static char* xvprintfd(char* buf, int val)
{
	int dvsr = 1000000000L;
	int quot;
	short leading = 1;

	if (!val)
	{
		buf = xvputc(buf, '0');
		return buf;
	}

	if (val < 0)
	{
		buf = xvputc(buf, '-');
		val = -val;
	}

	while (dvsr)
	{
		if (val)
		{
			quot = val / dvsr;
			val = val - (quot * dvsr);
		}
		else
		{
			quot = 0;
		}
		dvsr = dvsr / 10;

		if (quot)
		{
			buf = xvputc(buf, (char)quot + '0');
			leading = 0;
		}
		else if (!leading)
			buf = xvputc(buf, '0');
	}
	return buf;
}

static char* xvprintfx(char* buf, unsigned int val)
{
	int i = 28;
	char tbl[16] = { '0','1','2','3',
					 '4','5','6','7',
					 '8','9','A','B',
					 'C','D','E','F' };

	buf = xvputc(buf, '0');
	buf = xvputc(buf, 'x');
	while (i >= 0)
	{
		char c = tbl[(val >> i) & 15];
		buf = xvputc(buf, c);
		i -= 4;
	}
	return buf;
}

void xvprintf(char* buf, char* fmt, int val)
{
	char tmp[64];
	char* s = fmt; /* format strings are in rom */
	char* d = tmp;

	while (*s)
	{
		if (*s == '%')
			break;
		*d++ = *s++;
	}
	*d = 0;

	if (*s == '%')
	{
		int len, width = 0;
		char widthfield = ' ';
		char* pbuf;

		/* format string has a value */
		buf = xvprintfs(buf, tmp);
		s++;

		/* get width */
		if (*s == '0')
		{
			/* zero fill */
			widthfield = '0';
			s++;
		}

		while (*s >= '0' && *s <= '9')
			width = width * 10 + *s++ - '0';

		pbuf = buf;
		if (*s == 's')
			buf = xvprintfs(buf, (char*)val);
		else if (*s == 'd')
			buf = xvprintfd(buf, val);
		else if (*s == 'x')
			buf = xvprintfx(buf, (unsigned int)val);

		len = buf - pbuf;
		if (len < width) {
			int i;
			for (i = 0; i < len; i++)
				pbuf[width - 1 - i] = pbuf[len - 1 - i];
			for (i = 0; i < width - len; i++)
				pbuf[i] = widthfield;
			buf = pbuf + width;
		}
	}
	else
	{
		/* format string has no value */
		buf = xvprintfs(buf, fmt);
	}

	*buf = 0; /* make sure string is null terminated */
}

#ifdef TEST
int main(void)
{
	char buf[64];

	xvprintf(buf, "xa%2d", 3);
	printf("%s\n", buf);
}
#endif

