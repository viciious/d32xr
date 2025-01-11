/*
  CALICO

  Renderer phase 2 - Wall prep
*/

#include "doomdef.h"
#include "r_local.h"
#include "mars.h"

static fixed_t R_PointToDist(fixed_t x, fixed_t y) ATTR_DATA_CACHE_ALIGN;
static fixed_t R_ScaleFromGlobalAngle(fixed_t rw_distance, angle_t visangle, angle_t normalangle) ATTR_DATA_CACHE_ALIGN;
static void R_SetupCalc(viswall_t* wc, fixed_t hyp, angle_t normalangle, int angle1) ATTR_DATA_CACHE_ALIGN;
void R_WallLatePrep(viswall_t* wc, mapvertex_t *verts) ATTR_DATA_CACHE_ALIGN;
// the volatiles help gcc produce less silly SH2 assembler code
static void R_SegLoop(viswall_t* segl, unsigned short *restrict clipbounds, volatile fixed_t floorheight, 
    volatile fixed_t floornewheight, volatile fixed_t ceilingnewheight) ATTR_DATA_CACHE_ALIGN __attribute__((noinline));
void R_WallPrep(void) ATTR_DATA_CACHE_ALIGN __attribute__((noinline));

//
// Get distance to point in 3D projection
//
static fixed_t R_PointToDist(fixed_t x, fixed_t y)
{
    angle_t angle;
    fixed_t dx, dy, temp;

    dx = D_abs(x - vd->viewx);
    dy = D_abs(y - vd->viewy);

    if (dy > dx)
    {
        temp = dx;
        dx = dy;
        dy = temp;
    }

    angle = (tantoangle[(unsigned)FixedDiv(dy, dx) >> DBITS] + ANG90) >> ANGLETOFINESHIFT;

    // use as cosine
    return FixedDiv(dx, finesine(angle));
}

//
// Convert angle and distance within view frustum to texture scale factor.
//
static fixed_t R_ScaleFromGlobalAngle(fixed_t rw_distance, angle_t visangle, angle_t normalangle)
{
    angle_t anglea, angleb;
    fixed_t num, den;
    int     sinea, sineb;

    visangle += ANG90;

    anglea = visangle - vd->viewangle;
    sinea = finesine(anglea >> ANGLETOFINESHIFT);
    angleb = visangle - normalangle;
    sineb = finesine(angleb >> ANGLETOFINESHIFT);

    num = FixedMul(stretchX, sineb);
    den = FixedMul(rw_distance, sinea);

    return FixedDiv(num, den);
}

//
// Setup texture calculations for lines with upper and lower textures
//
static void R_SetupCalc(viswall_t* wc, fixed_t hyp, angle_t normalangle, int angle1)
{
    fixed_t sineval, rw_offset;
    angle_t offsetangle;

    offsetangle = normalangle - angle1;

    if (offsetangle > ANG180)
        offsetangle = 0 - offsetangle;

    if (offsetangle > ANG90)
        offsetangle = ANG90;

    sineval = finesine(offsetangle >> ANGLETOFINESHIFT);
    rw_offset = FixedMul(hyp, sineval);

    if (normalangle - angle1 < ANG180)
        rw_offset = -rw_offset;

    wc->offset += rw_offset;
    wc->centerangle = ANG90 + vd->viewangle - normalangle;
}

void R_WallLatePrep(viswall_t* wc, mapvertex_t *verts)
{
    angle_t      distangle, offsetangle, normalangle;
    seg_t       *seg = wc->seg;
    angle_t      angle1 = wc->scalestep;
    fixed_t      sineval, rw_distance;
    fixed_t      scalefrac, scale2;
    fixed_t      hyp;
    fixed_t      x1, y1, x2, y2;
    int          nv1 = SEG_UNPACK_V1(seg);
    int          nv2 = SEG_UNPACK_V2(seg);
    short        offset = SEG_UNPACK_OFFSET(seg);

    // this is essentially R_StoreWallRange
    // calculate rw_distance for scale calculation

    x1 = verts[nv1].x << FRACBITS;
    y1 = verts[nv1].y << FRACBITS;

    x2 = verts[nv2].x << FRACBITS;
    y2 = verts[nv2].y << FRACBITS;

    hyp = R_PointToDist(x1, y1);

    normalangle = R_PointToAngle(x1, y1, x2, y2);
    normalangle += ANG90;
    offsetangle = normalangle - angle1;

    if ((int)offsetangle < 0)
        offsetangle = 0 - offsetangle;

    if (offsetangle > ANG90)
        offsetangle = ANG90;

    distangle = ANG90 - offsetangle;
    sineval = finesine(distangle >> ANGLETOFINESHIFT);
    rw_distance = FixedMul(hyp, sineval);
    wc->distance = rw_distance;

    wc->offset = ((fixed_t)wc->offset + offset) << FRACBITS;

    scalefrac = scale2 = wc->scalefrac =
        R_ScaleFromGlobalAngle(rw_distance, vd->viewangle + (xtoviewangle[wc->start]<<FRACBITS), normalangle);

    if (wc->stop > wc->start)
    {
        scale2 = R_ScaleFromGlobalAngle(rw_distance, vd->viewangle + (xtoviewangle[wc->stop]<<FRACBITS), normalangle);
#ifdef MARS
        SH2_DIVU_DVSR = wc->stop - wc->start;  // set 32-bit divisor
        SH2_DIVU_DVDNT = scale2 - scalefrac;   // set 32-bit dividend, start divide
#else
        wc->scalestep = IDiv(scale2 - scalefrac, wc->stop - wc->start);
#endif
    }

    wc->scale2 = scale2;

    // does line have top or bottom textures?
    if (wc->actionbits & (AC_TOPTEXTURE | AC_BOTTOMTEXTURE | AC_MIDTEXTURE))
    {
        R_SetupCalc(wc, hyp, normalangle, angle1);// do calc setup
    }

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
static void R_SegLoop(viswall_t* segl, unsigned short * restrict clipbounds, 
    volatile fixed_t floorheight, volatile fixed_t floornewheight, volatile fixed_t ceilingnewheight)
{
    const volatile int actionbits = segl->actionbits;

    fixed_t scalefrac = segl->scalefrac;
    const fixed_t scalestep = segl->scalestep;

    int x, start = segl->start;
    const int stop = segl->stop;

    const volatile fixed_t ceilingheight = segl->ceilingheight;

    const int floorandlight = ((segl->seglightlevel & 0xff) << 16) | segl->floorpicnum;
    const int ceilandlight = ((segl->seglightlevel & 0xff) << 16) | segl->ceilingpicnum;

    unsigned short * restrict flooropen = (actionbits & AC_ADDFLOOR) ? vd->visplanes[0].open : NULL;
    unsigned short * restrict ceilopen = (actionbits & AC_ADDCEILING) ? vd->visplanes[0].open : NULL;

    unsigned short * restrict newclipbounds = segl->clipbounds;

    const int cy = centerY;
    const int vh = viewportHeight;

    if (!(actionbits & (AC_ADDFLOOR|AC_ADDCEILING)) && !newclipbounds)
        return;

    for (x = start; x <= stop; x++)
    {
        fixed_t floorclipx, ceilingclipx;
        fixed_t low, high, top, bottom;
        fixed_t scale2;

        scale2 = scalefrac;
        scalefrac += scalestep;

        //
        // get ceilingclipx and floorclipx from clipbounds
        //
        ceilingclipx = clipbounds[x];
        floorclipx = ceilingclipx & 0x00ff;
        ceilingclipx = ((unsigned)ceilingclipx & 0xff00) >> 8;

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

        if (newclipbounds)
        {
            const int newclip = actionbits & (AC_NEWFLOOR|AC_NEWCEILING);

            newclipbounds[x] = (high << 8) | low;

            if (!(newclip & AC_NEWFLOOR))
                low = floorclipx;
            if (!(newclip & AC_NEWCEILING))
                high = ceilingclipx;
            // rewrite clipbounds
            clipbounds[x] = (high << 8) | low;
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
                if (!MARKEDOPEN(flooropen[x]))
                {
                    visplane_t *floor = R_FindPlane(floorheight, floorandlight, x, stop);
                    flooropen = floor->open;
                }
                flooropen[x] = (top << 8) + (bottom-1);
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
                if (!MARKEDOPEN(ceilopen[x]))
                {
                    visplane_t *ceiling = R_FindPlane(ceilingheight, ceilandlight, x, stop);
                    ceilopen = ceiling->open;
                }
                ceilopen[x] = (top << 8) + (bottom-1);
            }
        }
    }
}

#ifdef MARS

void Mars_Sec_R_WallPrep(void)
{
    int i;
    uint16_t *vp0open;
    viswall_t *segl;
    viswallextra_t *seglex;
    viswall_t *first, *last, *verylast;
    uint32_t clipbounds_[SCREENWIDTH/2+1];
    __attribute__((aligned(16)))
		visplane_t *visplanes_hash_[NUM_VISPLANES_BUCKETS];
    uint16_t *clipbounds = (uint16_t *)clipbounds_;
    mapvertex_t *verts;
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

    first = last = vd->viswalls;
    verylast = NULL;
    seglex = vd->viswallextras;
    verts = /*W_GetLumpData(gamemaplump+ML_VERTEXES)*/vertexes;

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
#ifdef MARS
            Mars_ClearCacheLine(seglex);
#endif
            R_WallLatePrep(segl, verts);

            R_SegLoop(segl, clipbounds, seglex->floorheight, seglex->floornewheight, seglex->ceilnewheight);

            seglex++;
            *readysegs = *readysegs + 1;
        }
    }
}

#else

void R_WallPrep(void)
{
    viswall_t* segl;
    unsigned clipbounds_[SCREENWIDTH/2+1];
    unsigned short *clipbounds = (unsigned short *)clipbounds_;

    R_InitClipBounds(clipbounds_);

    for (segl = vd->viswalls; segl != vd->lastwallcmd; segl++)
    {
        fixed_t floornewheight = 0, ceilingnewheight = 0;

        R_WallLatePrep(segl, vertexes);

        R_SegLoop(segl, clipbounds, floornewheight, ceilingnewheight);
    }
}

#endif

// EOF

