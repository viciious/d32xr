!  The MIT License (MIT)
!
!  Permission is hereby granted, free of charge, to any person obtaining a copy
!  of this software and associated documentation files (the "Software"), to deal
!  in the Software without restriction, including without limitation the rights
!  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
!  copies of the Software, and to permit persons to whom the Software is
!  furnished to do so, subject to the following conditions:
!
!  The above copyright notice and this permission notice shall be included in all
!  copies or substantial portions of the Software.
!
!  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
!  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
!  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
!  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
!  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
!  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
!  SOFTWARE.

.data

! Perform a signed 16.16 by 16.16 multiply ! On entry: r4 = a, r5 = b ! On exit: r0 = (a * b) >> 16

    .align 4
    .global _FixedMul
_FixedMul:
    dmuls.l r4,r5
    sts mach,r1
    sts macl,r0
    rts
    xtrct r1,r0

! Perform a signed 16.16 by 16.16 divide ! On entry: r4 = a, r5 = b ! On exit: r0 = (a << 16) / b

    .align 4
    .global _FixedDiv
_FixedDiv:
    mov.l _sh2_div_unit,r2
    swap.w r4,r0
    mov.l r5,@r2 /* set 32-bit divisor */
    exts.w r0, r0
    mov.l r0,@(16,r2) /* set high long of 64-bit dividend */
    shll16 r4
    mov.l r4,@(20,r2) /* set low long of 64-bit dividend, start divide */
    /* note - overflow returns 0x7FFFFFFF or 0x80000000 after 6 cycles
    no overflow returns the quotient after 39 cycles */
    rts
    mov.l @(20,r2),r0 /* return 32-bit quotient */

! Perform a signed 32 by 32 divide ! On entry: r4 = a, r5 = b ! On exit: r0 = a / b

    .align 4
    .global _IDiv
_IDiv:
    mov.l _sh2_div_unit,r2
    mov.l r5,@r2 /* set 32-bit divisor */
    mov.l r4,@(4,r2) /* set 32-bit dividend, start divide */
    /* note - overflow returns 0x7FFFFFFF or 0x80000000 after 6 cycles
    no overflow returns the quotient after 39 cycles */
    rts
    mov.l @(4,r2),r0 /* return 32-bit quotient */

    .align 2

_sh2_div_unit:
    .long 0xFFFFFF00
