#include "doomdef.h"
#include "mars.h"

//
//                       SCREEN WIPE PACKAGE
//

int wipe_InitMelt(short *y)
{
    int i, r;
    int width = WIPEWIDTH;

    // setup initial column positions
    // (y<0 => not ready to scroll yet)
    y[0] = -(M_Random()%16);
    for (i=1;i<width;i++)
    {
        r = (M_Random()%3) - 1;
        y[i] = y[i-1] + r;
        if (y[i] > 0) y[i] = 0;
        else if (y[i] == -16) y[i] = -15;
    }


	while (!I_RefreshCompleted());

	return 0;
}

#ifdef MARS
static char wipe_lock = 0;

static void wipe_lockMelt(void)
{
    int res;
    do {
        __asm volatile (\
        "tas.b %1\n\t" \
            "movt %0\n\t" \
            : "=&r" (res) \
            : "m" (wipe_lock) \
            );
    } while (res == 0);
}

static void wipe_unlockMelt(void)
{
   wipe_lock = 0;
}

static int wipe_getNextMeltCol(void)
{
    int col;
    wipe_lockMelt();
    col = MARS_SYS_COMM6;
    MARS_SYS_COMM6 = col + 1;
    wipe_unlockMelt();
    return col;
}
#endif

static void wipe_meltBottomUp(short *yy)
{
    int i;
    int     width = WIPEWIDTH;
    int     height = I_FrameBufferHeight();
    short   *fb = (short *)I_FrameBuffer();

#ifdef MARS
    __asm volatile("mov %1,%0\n\t" : "=&r" (width) : "r"(WIPEWIDTH));
#endif

   for (i=0;i<width;i++)
   {
        int j;
        unsigned dy, oy;
        unsigned n;
        short*	s;
        short*	d;

#ifdef MARS
        i = wipe_getNextMeltCol();
        if (i >= width)
            break;
#endif

        dy = yy[i];
        if (dy == 0)
            continue;

        oy = (dy >> 8) & 0xff;
        dy = dy & 0xff;

        d = &fb[(height -      1)*width + i];
        s = &fb[(height - dy - 1)*width + i];

        j = height - oy - dy - 1;
        if (j <= 0)
            continue;

        n = ((unsigned)j + 3) >> 2;
        switch (j & 3)
        {
        case 0: do { *d = *s, d -= width, s -= width;
        case 3:      *d = *s, d -= width, s -= width;
        case 2:      *d = *s, d -= width, s -= width;
        case 1:      *d = *s, d -= width, s -= width;
        } while (--n > 0);
        }
    }
}

static int wipe_doMelt(short *y, short *yy, int ticks, int step)
{
    int		i;
    int     width = WIPEWIDTH;
    int     height = I_FrameBufferHeight();
#ifdef MARS
    int     movy = 4; // half the original movement to account for double buffering
#else
    int     movy = 8;
#endif
    boolean	done;

    done = true;
    for (i=0;i<width;i++)
    {
        int t;
        int dy, oy;

        if (step > 0)
        {
            /* restore columns from the previous step */
            dy = (uint16_t)yy[i];
            if (dy > 0)
            {
                done = false;

                oy = ((unsigned)dy >> 8) & 0xff;
                dy = (unsigned)dy & 0xff;
#ifdef MARS
                Mars_LoadWordColumnFromMDVRAM(i, oy, dy);
#endif
            }
        }

        oy = y[i];

        for (t = 0; t < ticks; t++)
        {
            if (y[i]<0)
            {
                y[i]++;
                done = false;
            }
            else if (y[i] < height)
            {
                dy = (y[i] < 16) ? y[i]+1 : movy;
                if (y[i]+dy >= height) dy = height - y[i];

                y[i] += dy;
                done = false;
            }
        }

        dy = y[i];
        if (oy < 0)
            oy = 0;
        if (dy <= oy)
            dy = oy = 0;
        dy = dy - oy;

        yy[i] = ((unsigned)oy<<8)|dy;
    }

#ifdef MARS
    Mars_Finish();
#endif

    UpdateBuffer();

#ifdef MARS
    Mars_melt_BeginWipe(yy);
#endif

    wipe_meltBottomUp(yy);

#ifdef MARS
    Mars_melt_EndWipe();
#endif

    return done;
}

#ifdef MARS
void Mars_Sec_wipe_doMelt(void)
{
    short   *yy;

    yy = (short*)(*(volatile uintptr_t *)&MARS_SYS_COMM8);
    Mars_ClearCacheLines(yy, (sizeof(*yy)*WIPEWIDTH+31)/16);

    wipe_meltBottomUp(yy);
}
#endif

int wipe_ExitMelt(void)
{
#ifdef MARS
	// re-init the VDP and VRAM on the MD
	Mars_CtlMDVDP(1);
#endif
    return 0;
}

int wipe_StartScreen(void)
{
#ifdef MARS
	Mars_CtlMDVDP(0);
#endif

	I_StoreScreenCopy();
	UpdateBuffer();
	I_RestoreScreenCopy();

    return 0;
}

int wipe_EndScreen(void)
{
    I_SwapScreenCopy();

    // the new screen is now stored on the MD
    // the start screen is back in 32X framebuffer

    return 0;
}

int wipe_ScreenWipe(short *y, short *yy, int ticks, int step)
{
   return wipe_doMelt(y, yy, ticks, step);
}
