/*
  Victor Luchits

  The MIT License (MIT)

  Copyright (c) 2021 Victor Luchits

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include "doomdef.h"

int D_snprintf(char *buf, size_t nsize, const char *fmt, ...)
{
	int ret;
	va_list ap;

	buf[0] = '\0';

	va_start(ap, fmt);
	ret = D_vsnprintf(buf, nsize, fmt, ap);
	va_end(ap);

	return ret;
}

int D_strcasecmp (const char *s1, const char *s2)
{
	size_t s1len = mystrlen(s1);
	size_t s2len = mystrlen(s2);
	int cmp = D_strncasecmp(s1, s2, s1len < s2len ? s1len : s2len);

	if (cmp != 0)
		return cmp;
	if (s1len < s2len)
		return -1;
	if (s1len > s2len)
		return 1;
	return 0;
}

int D_atoi(const char* str)
{
	int i;

	if (!str) return 0;

	i = 0;
	while (*str && (*str >= '0' && *str <= '9')) {
		i = i * 10 + (*str - '0');
		str++;
	}
	return i;
}

char* D_strchr(const char* str, char chr)
{
	const char* p;

	if (!str) return NULL;

	for (p = str; *p != '\0'; p++) {
		if (*p == chr)
			return (char*)p;
	}

	return NULL;
}

