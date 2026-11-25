/*
  CALICO

  Renderer phase 2 - Wall prep
*/

#include "doomdef.h"
#include "r_local.h"
#include "mars.h"

static fixed_t R_ScaleFromGlobalAngle(fixed_t invdist, fixed_t sineb, int x) ATTR_DATA_CACHE_ALIGN;
static uint16_t P_SegOffset(seg_t *seg) ATTR_DATA_CACHE_ALIGN;
// consecutive viswalls are often fragments of the same seg (long segs
// split in R_StoreWallRange, or segs cut into pieces by the solid seg
// clipper); everything that doesn't depend on the column range is then
// taken from the previous viswall. wc->seg doesn't survive R_WallLatePrep
// (it's unioned with v1i), so the caller keeps the last seg in *prevseg
static void R_WallLatePrep(viswall_t* wc, mapvertex_t *verts, seg_t **prevseg) ATTR_DATA_CACHE_ALIGN;
// the empty asm fence in R_SegLoop helps gcc produce less silly SH2 assembler code
static void R_SegLoop(viswall_t* segl, unsigned short *restrict clipbounds) ATTR_DATA_CACHE_ALIGN __attribute__((noinline));
void R_WallPrep(void) ATTR_DATA_CACHE_ALIGN __attribute__((noinline));

//
// Get distance to point in 3D projection
//

//
// Convert angle and distance within view frustum to texture scale factor.
//
// scale = stretchX * sin(visangle + ANG90 - normalangle) / (rw_distance * cos(xtoviewangle[x]))
//
// invdist is stretchX / rw_distance, computed once per wall, sineb is
// sin(visangle + ANG90 - normalangle), and 1/cos(xtoviewangle[x]) is the
// precomputed distscale[x] table (stored >> 1). The caller looks up sineb
// while the hardware divider is still working on invdist.
//
static fixed_t R_ScaleFromGlobalAngle(fixed_t invdist, fixed_t sineb, int x)
{
    return FixedMul(FixedMul(invdist, sineb), (fixed_t)distscale[x] << 1);
}

//
// Setup texture calculations for lines with upper and lower textures
//

static uint16_t P_SegOffset(seg_t *seg)
{
    int side = SEG_UNPACK_SIDE(seg);
    mapvertex_t *sv = &vertexes[lines[SEG_UNPACK_LINEDEF(seg)].v1];
    mapvertex_t *ev = &vertexes[lines[SEG_UNPACK_LINEDEF(seg)].v2];
    mapvertex_t *v1 = &vertexes[SEG_UNPACK_V1(seg)];
    uint16_t dx, dy;
    uint16_t g, c;
    unsigned n;

    if (side)
    {
        dx = D_abs(ev->x - v1->x);
        dy = D_abs(ev->y - v1->y);
    }
    else
    {
        dx = D_abs(sv->x - v1->x);
        dy = D_abs(sv->y - v1->y);
    }
 
    if (dx == 0)
        return dy;
    if (dy == 0)
        return dx;

    // integer square root
    n = dx*dx + dy*dy;
    g = c = 0x8000;
    for ( ; ; ) {
        if (g*g > n) g ^= c;
        c >>= 1;
        if (c == 0)  return g;
        g |= c;
    }
    return g;
}

static void R_WallLatePrep(viswall_t* wc, mapvertex_t *verts, seg_t **prevseg)
{
    seg_t       *seg = wc->seg;
    fixed_t      scalefrac, scale2;
    fixed_t      invdist;
    angle_t      baseangle;
    fixed_t      sineb1, sineb2;
    fixed_t      rw_distance, offset;
    const boolean newseg = seg != *prevseg;

    // this is essentially R_StoreWallRange
    if (newseg)
    {
        angle_t      normalangle;
        fixed_t      rw_offset, dx, dy;
        fixed_t      x1, y1, x2, y2;
        int          nv1 = SEG_UNPACK_V1(seg);
        int          nv2 = SEG_UNPACK_V2(seg);

        // calculate rw_distance for scale calculation
        x1 = verts[nv1].x << FRACBITS;
        y1 = verts[nv1].y << FRACBITS;

        x2 = verts[nv2].x << FRACBITS;
        y2 = verts[nv2].y << FRACBITS;

        normalangle = R_PointToAngle(x1, y1, x2, y2);
        normalangle += ANG90;

        // project the view->v1 vector onto the wall normal (distance) and
        // the wall direction (texture offset); axis-aligned walls are exact
        dx = x1 - vd->viewx;
        dy = y1 - vd->viewy;
        if (x1 == x2)
        {
            rw_distance = D_abs(dx);
            rw_offset = y2 > y1 ? -dy : dy;
        }
        else if (y1 == y2)
        {
            rw_distance = D_abs(dy);
            rw_offset = x2 > x1 ? -dx : dx;
        }
        else
        {
            fixed_t sinn = finesine(normalangle >> ANGLETOFINESHIFT);
            fixed_t cosn = finecosine(normalangle >> ANGLETOFINESHIFT);
            rw_distance = FixedMul(dx, cosn) + FixedMul(dy, sinn);
            if (rw_distance < 0)
                rw_distance = 0;
            rw_offset = FixedMul(dy, cosn) - FixedMul(dx, sinn);
        }

        baseangle = vd->viewangle + ANG90 - normalangle;
        *prevseg = seg;

        // only read by R_DrawSeg for textured walls, so set them
        // unconditionally; P_SegOffset is added while the divider runs
        offset = ((fixed_t)wc->offset << FRACBITS) + rw_offset;
        wc->centerangle = baseangle;
    }
    else
    {
        // same seg as the previous viswall, so the same sidedef texture
        // offset too; R_SegLoop doesn't modify these
        const viswall_t *prev = wc - 1;
        rw_distance = prev->distance;
        offset = prev->offset;
        wc->centerangle = baseangle = prev->centerangle;
    }
    wc->distance = rw_distance;

#ifdef MARS
    // invdist = FixedDiv(stretchX, rw_distance), started on the divider here
    // and collected after the sine lookups, which don't depend on it and
    // hide most of its 39-cycle latency. Nothing in between may use the divider.
    SH2_DIVU_DVSR = rw_distance;           // set 32-bit divisor
    SH2_DIVU_DVDNTH = stretchX>>FRACBITS;  // high long of 64-bit dividend
    SH2_DIVU_DVDNTL = stretchX<<FRACBITS;  // low long, starts the divide
#else
    invdist = FixedDiv(stretchX, rw_distance);
#endif

    if (newseg)
        offset += (fixed_t)P_SegOffset(seg) << FRACBITS;
    wc->offset = offset;

    sineb1 = finesine((baseangle + (xtoviewangle[wc->start]<<FRACBITS)) >> ANGLETOFINESHIFT);
    sineb2 = sineb1;
    if (wc->stop > wc->start)
        sineb2 = finesine((baseangle + (xtoviewangle[wc->stop]<<FRACBITS)) >> ANGLETOFINESHIFT);

#ifdef MARS
    invdist = SH2_DIVU_DVDNTL;             // get 32-bit quotient (stalls if not done)
#endif
    // the divide saturates when rw_distance is ~0 (standing on the line);
    // clamp so the multiply by distscale (up to ~1.42) can't overflow
    if ((unsigned)invdist > 0x50000000)
        invdist = 0x50000000;

    scalefrac = scale2 = wc->scalefrac =
        R_ScaleFromGlobalAngle(invdist, sineb1, wc->start);

    if (wc->stop > wc->start)
    {
        scale2 = R_ScaleFromGlobalAngle(invdist, sineb2, wc->stop);
#ifdef MARS
        SH2_DIVU_DVSR = wc->stop - wc->start;  // set 32-bit divisor
        SH2_DIVU_DVDNT = scale2 - scalefrac;   // set 32-bit dividend, start divide
#else
        wc->scalestep = IDiv(scale2 - scalefrac, wc->stop - wc->start);
#endif
    }

    wc->scale2 = scale2;

    wc->clipbounds = NULL;

    const int start = wc->start;
    const int stop = wc->stop;
    const int width = stop - start + 1;
    if (wc->actionbits & (AC_NEWFLOOR | AC_NEWCEILING | AC_TOPSIL | AC_BOTTOMSIL | AC_MIDTEXTURE))
    {
        wc->clipbounds = vd->lastsegclip - start;
        vd->lastsegclip += width;
    }
    if (wc->actionbits & AC_MIDTEXTURE)
    {
        // lighting + column
        //D_memset(vd->lastsegclip, 255, sizeof(*vd->lastsegclip)*width);
        vd->lastsegclip += width;
    }

    // unpack the vertices for walls that clip sprites in phase 8;
    // this overwrites wc->seg (union with v1i), which isn't needed past here
    if (wc->actionbits & (AC_TOPSIL | AC_BOTTOMSIL | AC_SOLIDSIL | AC_MIDTEXTURE))
    {
        wc->v2i = *(int32_t *)&verts[SEG_UNPACK_V2(seg)];
        wc->v1i = *(int32_t *)&verts[SEG_UNPACK_V1(seg)];
    }

#ifdef MARS
    if (wc->stop > wc->start)
    {
        wc->scalestep = SH2_DIVU_DVDNT; // get 32-bit quotient
    }
#endif
}

//
// Main seg clipping loop
//
#define COL(p) (*(unsigned short *)((char *)(p) + x))

static void R_SegLoop(viswall_t* segl, unsigned short * restrict clipbounds)
{
    int actionbits = segl->actionbits;

    fixed_t scalefrac = segl->scalefrac;
    const fixed_t scalestep = segl->scalestep;

    int start = segl->start;
    const int stop = segl->stop;
    // byte offset of column x, so GCC keeps a single induction variable
    unsigned x = (unsigned)start << 1;
    const unsigned stopx = (unsigned)stop << 1;

    fixed_t ceilingheight = FROMHEIGHT16(segl->ceilingheight);

    unsigned short * restrict flooropen = (actionbits & AC_ADDFLOOR) ? vd->visplanes[0].open : NULL;
    unsigned short * restrict ceilopen = (actionbits & AC_ADDCEILING) ? vd->visplanes[0].open : NULL;

    fixed_t floorheight = FROMHEIGHT16(segl->floorheight);
    fixed_t floornewheight = FROMHEIGHT16(segl->floornewheight);
    fixed_t ceilingnewheight = FROMHEIGHT16(segl->ceilingnewheight);

    unsigned short * restrict newclipbounds = segl->clipbounds;

    // which halves of clipbounds take the new values
    int newclip = ((actionbits & AC_NEWFLOOR) ? 0x00ff : 0) | ((actionbits & AC_NEWCEILING) ? 0xff00 : 0);

#ifdef MARS
    // keep these out of GCC's value tracking; yields a tighter loop than volatile
    __asm volatile ("" : "+r" (newclip), "+r" (ceilingheight), "+r" (floorheight),
                         "+r" (floornewheight), "+r" (ceilingnewheight));
#endif

    const int cy = centerY;
    const int vh = viewportHeight;

    // AC_ADDFLOOR/AC_ADDCEILING always come with AC_NEWFLOOR/AC_NEWCEILING,
    // which allocate segl->clipbounds in R_WallLatePrep
    if (!newclipbounds)
        return;

    do {
        fixed_t floorclipx, ceilingclipx;
        fixed_t low, high, top, bottom;
        fixed_t scale2;

        scale2 = scalefrac;
        scalefrac += scalestep;
#ifdef MARS
        // keep GCC from turning the FixedMul()s below into 64-bit running sums
        // (strength reduction), which the SH-2 has to keep on the stack
        __asm volatile ("" : "+r" (scale2));
#endif

        //
        // get ceilingclipx and floorclipx from clipbounds
        //
        ceilingclipx = COL(clipbounds);
        floorclipx = ceilingclipx & 0x00ff;
        ceilingclipx = ((unsigned)ceilingclipx & 0xff00) >> 8;

        {
            //
            // calc high and low
            //
            low = FixedMul(scale2, floornewheight)>>FRACBITS;
            low = cy - low;
            if (low < 0)
                low = 0;
            else if (low > floorclipx)
                low = floorclipx;

            high = FixedMul(scale2, ceilingnewheight)>>FRACBITS;
            high = cy - high;
            if (high > vh)
                high = vh;
            else if (high < ceilingclipx)
                high = ceilingclipx;

            // rewrite clipbounds; the seg keeps the merged values, so
            // that R_SegCommands can copy them back as whole words
            COL(newclipbounds) = COL(clipbounds) ^= (COL(clipbounds) ^ ((high << 8) + low)) & newclip;
        }

        //
        // floor
        //
        if (flooropen)
        {
            top = FixedMul(scale2, floorheight)>>FRACBITS;
            top = cy - top;
            if (top < ceilingclipx)
                top = ceilingclipx;
            bottom = floorclipx;

            if (top < bottom)
            {
                if (!MARKEDOPEN(COL(flooropen)))
                {
                    const int floorandlight = ((segl->seglightlevel & 0xff) << 16) | segl->floorpicnum;
                    visplane_t *floor = R_FindPlane(floorheight, floorandlight, x >> 1, stop);
                    flooropen = floor->open;
                }
                COL(flooropen) = (top << 8) + (bottom-1);
            }
        }

        //
        // ceiling
        //
        if (ceilopen)
        {
            top = ceilingclipx;
            bottom = FixedMul(scale2, ceilingheight)>>FRACBITS;
            bottom = cy - bottom;
            if (bottom > floorclipx)
                bottom = floorclipx;

            if (top < bottom)
            {
                if (!MARKEDOPEN(COL(ceilopen)))
                {
                    const int ceilandlight = ((segl->seglightlevel & 0xff) << 16) | segl->ceilingpicnum;
                    visplane_t *ceiling = R_FindPlane(ceilingheight, ceilandlight, x >> 1, stop);
                    ceilopen = ceiling->open;
                }
                COL(ceilopen) = (top << 8) + (bottom-1);
            }
        }
    } while ((x += 2) <= stopx);
}

#ifdef MARS

void Mars_Sec_R_WallPrep(void)
{
    int i;
    uint16_t *vp0open;
    viswall_t *segl;
    viswall_t *first, *last, *verylast;
    uint32_t clipbounds_[SCREENWIDTH/2+1];
    __attribute__((aligned(16)))
		visplane_t *visplanes_hash_[NUM_VISPLANES_BUCKETS];
    uint16_t *clipbounds = (uint16_t *)clipbounds_;
    mapvertex_t *verts;
    seg_t *prevseg;
    volatile uint8_t *addedsegs = (volatile uint8_t *)&MARS_SYS_COMM6;
    volatile uint8_t *readysegs = addedsegs + 1;

    // clear visplane 0
    vp0open = vd->visplanes[0].open - 1;
    for (i = 0; i < SCREENWIDTH+2; i++)
        vp0open[i] = 0;

    vd->visplanes_hash = visplanes_hash_;
    for (i = 0; i < NUM_VISPLANES_BUCKETS; i++)
        vd->visplanes_hash[i] = NULL;

    R_InitClipBounds(clipbounds_);

    prevseg = NULL;

    first = last = vd->viswalls;
    verylast = NULL;
    verts = vertexes;

    I_SetBankPage(segspage);

    for (segl = first; segl != verylast; )
    {
        int8_t nextsegs = *addedsegs;

        // check if master CPU finished exec'ing R_BSP()
        if (nextsegs == -2)
        {
            Mars_ClearCacheLine(&vd->lastwallcmd);
            verylast = vd->lastwallcmd;
            last = verylast;
        }
        else
        {
            last = first + (uint8_t)nextsegs;
        }

        for (; segl < last; segl++)
        {
            R_WallLatePrep(segl, verts, &prevseg);

            R_SegLoop(segl, clipbounds);

            *readysegs = *readysegs + 1;
        }

        SH2_Delay64();
    }
}

#else

void R_WallPrep(void)
{
    viswall_t* segl;
    unsigned clipbounds_[SCREENWIDTH/2+1];
    unsigned short *clipbounds = (unsigned short *)clipbounds_;
    seg_t *prevseg;

    R_InitClipBounds(clipbounds_);
    prevseg = NULL;

    for (segl = vd->viswalls; segl != vd->lastwallcmd; segl++)
    {
        fixed_t floornewheight = 0, ceilingnewheight = 0;

        R_WallLatePrep(segl, vertexes, &prevseg);

        R_SegLoop(segl, clipbounds, floornewheight, ceilingnewheight);
    }
}

#endif

// EOF

