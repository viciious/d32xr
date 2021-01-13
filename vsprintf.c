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

//
// CALICO: Replaced with call to C library function to eliminate non-portable
// argument-passing idiom.
//
int D_vsnprintf(char *str, size_t nmax, const char *format, va_list ap)
{
   int result;

   if(nmax < 1)
   {
      return 0;
   }

   // vsnprintf() in Windows (and possibly other OSes) doesn't always
   // append a trailing \0. We have the responsibility of making this
   // safe by writing into a buffer that is one byte shorter ourselves.
   result = vsnprintf(str, nmax, format, ap);

   // If truncated, change the final char in the buffer to a \0.
   // In Windows, a negative result indicates a truncated buffer.
   if(result < 0 || (size_t)result >= nmax)
   {
      str[nmax - 1] = '\0';
      result = (int)(nmax - 1);
   }

   return result;
}

// EOF

