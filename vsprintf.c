/*
  CALICO

  String utilites, formatted printing

  The MIT License (MIT)

  Copyright (c) 2016 James Haley

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

#include <stdio.h>
#include "doomdef.h"

#if defined(_MSC_VER) && (_MSC_VER < 1400) /* not needed for Visual Studio 2008 */
#define vsnprintf _vsnprintf
#endif

/* prints number of characters printed. */

int mystrlen(const char *string)
{
   int rc = 0;
   
   if(string) 
   {
      while(*(string++)) 
         rc++;
   }
   else 
      rc = -1;
   
   return rc;
}

int D_vsnprintf(char* string, size_t nmax, const char* format, va_list argptr)
{
    int len, i, div, uselong;
    int fieldsize;
    unsigned long num;
    long snum;
    char padchar;
    char* str;
    char* origstring = string;

    while (*format)
    {
        if (*format != '%') *(string++) = *(format++);
        else
        {
            format++;

            /* set field pad character to 0 if necessary */
            if (*format == '0')
            {
                padchar = '0';
                format++;
            }
            else padchar = ' ';

            /* get the fieldwidth if any */
            fieldsize = 0;
            while (*format >= '0' && *format <= '9')
                fieldsize = fieldsize * 10 + *(format++) - '0';

            /* get rid of 'l' if present */
            if (*format == 'l')
            {
                uselong = 1;
                format++;
            }
            else uselong = 0;

            div = 10;
            if (*format == 'c')
            {
                *(string++) = va_arg(argptr, int);
                format++;
            }
            else if (*format == 's')
            {
                str = (char*)va_arg(argptr, int);
                len = mystrlen(str);
                while (fieldsize-- > len) *(string++) = padchar; /* do field pad */
                while (*str) *(string++) = *(str++); /* copy string */
                format++;
            }
            else
            {
                if (*format == 'o') /* octal */
                {
                    div = 8;
                    if (uselong)
                        num = va_arg(argptr, int);
                    else
                        num = va_arg(argptr, int);
                    /*	  printf("o=0%o\n", num); */
                }
                else if (*format == 'x' || *format == 'X' || *format == 'p' || *format == 'P')  /* hex */
                {
                    div = 16;
                    if (uselong)
                        num = va_arg(argptr, int);
                    else
                        num = va_arg(argptr, int);
                    /*	  printf("x=%x\n", num); */
                }
                else if (*format == 'i' || *format == 'd' || *format == 'u') /* decimal */
                {
                    div = 10;
                    if (uselong)
                        snum = va_arg(argptr, int);
                    else
                        snum = va_arg(argptr, int);
                    if (snum < 0 && *format != 'u') /* handle negative %i or %d */
                    {
                        *(string++) = '-';
                        num = -snum;
                        if (fieldsize) fieldsize--;
                    }
                    else num = snum;
                }
                else return -1; /* unrecognized format specifier */

                /* print any decimal or hex integer */
                len = 0;
                while (num || fieldsize || !len)
                {
                    for (i = len; i; i--) string[i] = string[i - 1]; /* shift right */
                    if (len && fieldsize && !num) *string = padchar; /* pad out */
                    else
                    {
                        /* put in a hex or decimal digit */
                        *string = num % div;
                        *string += *string > 9 ? 'A' - 10 : '0';
                        /*	    printf("d = %c\n", *string); */
                        num /= div;
                    }
                    len++;
                    if (fieldsize) fieldsize--;
                }
                string += len;
                format++;
            }
        }
    }
    *string = 0;

    return origstring - string;
}

// EOF

