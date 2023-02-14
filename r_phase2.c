/*
  CALICO

  Renderer phase 2 - Wall prep
*/

#include "doomdef.h"
#include "r_local.h"
#include "mars.h"

static sector_t emptysector = { 0, 0, -2, -2, -2 };

static void R_WallEarlyPrep(viswall_t* segl, fixed_t *floorheight, 
    fixed_t *floornewheight, fixed_t *ceilingnewheight) ATTR_DATA_CACHE_ALIGN;
static fixed_t R_PointToDist(fixed_t x, fixed_t y) ATTR_DATA_CACHE_ALIGN;
static fixed_t R_ScaleFromGlobalAngle(fixed_t rw_distance, angle_t visangle, angle_t normalangle) ATTR_DATA_CACHE_ALIGN;
static void R_SetupCalc(viswall_t* wc, fixed_t hyp, angle_t normalangle, int angle1) ATTR_DATA_CACHE_ALIGN;
static void R_WallLatePrep(viswall_t* wc, vertex_t *verts) ATTR_DATA_CACHE_ALIGN;
static void R_SegLoop(viswall_t* segl, unsigned short* clipbounds, fixed_t floorheight, 
    fixed_t floornewheight, fixed_t ceilingnewheight) ATTR_DATA_CACHE_ALIGN __attribute__((noinline));

static void R_WallEarlyPrep(viswall_t* segl, fixed_t *floorheight, 
    fixed_t *floornewheight, fixed_t *ceilingnewheight)
{
   seg_t     *seg;
   line_t    *li;
   side_t    *si;
   sector_t  *front_sector, *back_sector;
   fixed_t    f_floorheight, f_ceilingheight;
   fixed_t    b_floorheight, b_ceilingheight;
   int        f_lightlevel, b_lightlevel;
   int        f_ceilingpic, b_ceilingpic;
   int        b_texturemid, t_texturemid;
   boolean    skyhack;
   int        actionbits;
   int        side;

   {
      seg  = segl->seg;
      li   = &lines[seg->linedef];
      side = seg->side;
      si   = &sides[li->sidenum[seg->side]];

      li->flags |= ML_MAPPED; // mark as seen

      front_sector    = &sectors[sides[li->sidenum[side]].sector];
#ifdef MARS
      Mars_ClearCacheLines(front_sector, (sizeof(sector_t)+31)/16);
#endif

      f_ceilingpic    = front_sector->ceilingpic;
      f_lightlevel    = front_sector->lightlevel;
      f_floorheight   = front_sector->floorheight   - vd.viewz;
      f_ceilingheight = front_sector->ceilingheight - vd.viewz;

#ifdef MARS
      Mars_ClearCacheLine(&flattranslation[front_sector->floorpic]);
#endif
      segl->floorpicnum   = flattranslation[front_sector->floorpic];

      if (f_ceilingpic != -1)
      {
#ifdef MARS
          Mars_ClearCacheLine(&flattranslation[f_ceilingpic]);
#endif
          segl->ceilingpicnum = flattranslation[f_ceilingpic];
      }
      else
      {
          segl->ceilingpicnum = -1;
      }

      back_sector = (li->flags & ML_TWOSIDED) ? &sectors[sides[li->sidenum[side^1]].sector] : 0;
      if(!back_sector)
         back_sector = &emptysector;
#ifdef MARS
      else
         Mars_ClearCacheLines(back_sector, (sizeof(sector_t)+31)/16);
#endif

      b_ceilingpic    = back_sector->ceilingpic;
      b_lightlevel    = back_sector->lightlevel;
      b_floorheight   = back_sector->floorheight   - vd.viewz;
      b_ceilingheight = back_sector->ceilingheight - vd.viewz;

      t_texturemid = b_texturemid = 0;
      actionbits = 0;

      // deal with sky ceilings (also missing in 3DO)
      if(f_ceilingpic == -1 && b_ceilingpic == -1)
         skyhack = true;
      else 
         skyhack = false;

      // add floors and ceilings if the wall needs them
      if(f_floorheight < 0 &&                                // is the camera above the floor?
         (front_sector->floorpic != back_sector->floorpic || // floor texture changes across line?
          f_floorheight   != b_floorheight                || // height changes across line?
          f_lightlevel    != b_lightlevel                 || // light level changes across line?
          b_ceilingheight == b_floorheight))                 // backsector is closed?
      {
         *floorheight = *floornewheight = f_floorheight / (1 << FIXEDTOHEIGHT);
         actionbits |= (AC_ADDFLOOR|AC_NEWFLOOR);
      }

      if(!skyhack                                         && // not a sky hack wall
         (f_ceilingheight > 0 || f_ceilingpic == -1)      && // ceiling below camera, or sky
         (f_ceilingpic    != b_ceilingpic                 || // ceiling texture changes across line?
          f_ceilingheight != b_ceilingheight              || // height changes across line?              
          f_lightlevel    != b_lightlevel                 || // light level changes across line?
          b_ceilingheight == b_floorheight))                 // backsector is closed?
      {
         segl->ceilingheight = *ceilingnewheight = f_ceilingheight / (1 << FIXEDTOHEIGHT);
         if(f_ceilingpic == -1)
            actionbits |= (AC_ADDSKY|AC_NEWCEILING);
         else
            actionbits |= (AC_ADDCEILING|AC_NEWCEILING);
      }

      segl->t_topheight = f_ceilingheight / (1 << FIXEDTOHEIGHT); // top of texturemap

      if(back_sector == &emptysector)
      {
         // single-sided line
#ifdef MARS
         Mars_ClearCacheLine(&texturetranslation[si->midtexture]);
#endif
         segl->t_texturenum = texturetranslation[si->midtexture];

         // handle unpegging (bottom of texture at bottom, or top of texture at top)
         if(li->flags & ML_DONTPEGBOTTOM)
            t_texturemid = f_floorheight + (textures[segl->t_texturenum].height << FRACBITS);
         else
            t_texturemid = f_ceilingheight;

         t_texturemid += si->rowoffset<<FRACBITS;                               // add in sidedef texture offset
         segl->t_bottomheight = f_floorheight / (1 << FIXEDTOHEIGHT); // set bottom height
         actionbits |= (AC_SOLIDSIL|AC_TOPTEXTURE);                   // solid line; draw middle texture only
      }
      else
      {
         // two-sided line

         // is bottom texture visible?
         if(b_floorheight > f_floorheight)
         {
#ifdef MARS
            Mars_ClearCacheLine(&texturetranslation[si->bottomtexture]);
#endif
            segl->b_texturenum = texturetranslation[si->bottomtexture];
            if(li->flags & ML_DONTPEGBOTTOM)
               b_texturemid = f_ceilingheight;
            else
               b_texturemid = b_floorheight;

            b_texturemid += si->rowoffset<<FRACBITS; // add in sidedef texture offset

            segl->b_topheight = *floornewheight = b_floorheight / (1 << FIXEDTOHEIGHT);
            segl->b_bottomheight = f_floorheight / (1 << FIXEDTOHEIGHT);
            actionbits |= (AC_BOTTOMTEXTURE|AC_NEWFLOOR); // generate bottom wall and floor
         }

         // is top texture visible?
         if(b_ceilingheight < f_ceilingheight && !skyhack)
         {
#ifdef MARS
            Mars_ClearCacheLine(&texturetranslation[si->toptexture]);
#endif
            segl->t_texturenum = texturetranslation[si->toptexture];
            if(li->flags & ML_DONTPEGTOP)
               t_texturemid = f_ceilingheight;
            else
               t_texturemid = b_ceilingheight + (textures[segl->t_texturenum].height << FRACBITS);

            t_texturemid += si->rowoffset<<FRACBITS; // add in sidedef texture offset

            segl->t_bottomheight = *ceilingnewheight = b_ceilingheight / (1 << FIXEDTOHEIGHT);
            actionbits |= (AC_NEWCEILING|AC_TOPTEXTURE); // draw top texture and ceiling
         }

         // check if this wall is solid, for sprite clipping
         if(b_floorheight >= f_ceilingheight || b_ceilingheight <= f_floorheight)
            actionbits |= AC_SOLIDSIL;
         else
         {
            if((b_floorheight >= 0 && b_floorheight > f_floorheight) ||
               (f_floorheight < 0 && f_floorheight > b_floorheight))
            {
               actionbits |= AC_BOTTOMSIL; // set bottom mask
            }

            if(!skyhack)
            {
               if((b_ceilingheight <= 0 && b_ceilingheight < f_ceilingheight) ||
                  (f_ceilingheight >  0 && b_ceilingheight > f_ceilingheight))
               {
                  actionbits |= AC_TOPSIL; // set top mask
               }
            }
         }
      }

      // save local data to the viswall structure
      segl->actionbits    = actionbits;
      segl->t_texturemid  = t_texturemid;
      segl->b_texturemid  = b_texturemid;
      segl->seglightlevel = f_lightlevel;
      segl->offset        = ((fixed_t)si->textureoffset + seg->offset) << 16;
   }
}

//
// Get distance to point in 3D projection
//
static fixed_t R_PointToDist(fixed_t x, fixed_t y)
{
    int angle;
    fixed_t dx, dy, temp;

    dx = D_abs(x - vd.viewx);
    dy = D_abs(y - vd.viewy);

    if (dy > dx)
    {
        temp = dx;
        dx = dy;
        dy = temp;
    }

    angle = (tantoangle[FixedDiv(dy, dx) >> DBITS] + ANG90) >> ANGLETOFINESHIFT;

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

    anglea = visangle - vd.viewangle;
    sinea = finesine(anglea >> ANGLETOFINESHIFT);
    angleb = visangle - normalangle;
    sineb = finesine(angleb >> ANGLETOFINESHIFT);

    FixedMul2(num, stretchX, sineb);
    FixedMul2(den, rw_distance, sinea);

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
    FixedMul2(rw_offset, hyp, sineval);

    if (normalangle - angle1 < ANG180)
        rw_offset = -rw_offset;

    wc->offset += rw_offset;
    wc->centerangle = ANG90 + vd.viewangle - normalangle;
}

static void R_WallLatePrep(viswall_t* wc, vertex_t *verts)
{
    angle_t      distangle, offsetangle, normalangle;
    seg_t* seg = wc->seg;
    int          angle1 = wc->scalestep;
    fixed_t      sineval, rw_distance;
    fixed_t      scalefrac, scale2;
    fixed_t      hyp;

    // this is essentially R_StoreWallRange
    // calculate rw_distance for scale calculation
    normalangle = ((angle_t)seg->angle << 16) + ANG90;
    offsetangle = normalangle - angle1;

    if ((int)offsetangle < 0)
        offsetangle = 0 - offsetangle;

    if (offsetangle > ANG90)
        offsetangle = ANG90;

    distangle = ANG90 - offsetangle;
    hyp = R_PointToDist(verts[seg->v1].x, verts[seg->v1].y);
    sineval = finesine(distangle >> ANGLETOFINESHIFT);
    FixedMul2(rw_distance, hyp, sineval);
    wc->distance = rw_distance;

    scalefrac = scale2 = wc->scalefrac =
        R_ScaleFromGlobalAngle(rw_distance, vd.viewangle + xtoviewangle[wc->start], normalangle);

    if (wc->stop > wc->start)
    {
        scale2 = R_ScaleFromGlobalAngle(rw_distance, vd.viewangle + xtoviewangle[wc->stop], normalangle);
#ifdef MARS
        SH2_DIVU_DVSR = wc->stop - wc->start;  // set 32-bit divisor
        SH2_DIVU_DVDNT = scale2 - scalefrac;   // set 32-bit dividend, start divide
#else
        wc->scalestep = IDiv(scale2 - scalefrac, wc->stop - wc->start);
#endif
    }

    wc->scale2 = scale2;

    // does line have top or bottom textures?
    if (wc->actionbits & (AC_TOPTEXTURE | AC_BOTTOMTEXTURE))
    {
        R_SetupCalc(wc, hyp, normalangle, angle1);// do calc setup
    }

#ifdef MARS
    if (wc->stop > wc->start)
    {
        wc->scalestep = SH2_DIVU_DVDNT; // get 32-bit quotient
    }
#endif

    int rw_x = wc->start;
    int rw_stopx = wc->stop + 1;
    int width = (rw_stopx - rw_x + 1) / 2;

    wc->sil = (byte*)lastopening - rw_x;

    if (wc->actionbits & AC_TOPSIL)
        lastopening += width;
    if (wc->actionbits & AC_BOTTOMSIL)
        lastopening += width;
}

//
// Main seg clipping loop
//
static void R_SegLoop(viswall_t* segl, unsigned short* clipbounds, 
    fixed_t floorheight, fixed_t floornewheight, fixed_t ceilingnewheight)
{
    visplane_t* ceiling, * floor;
    unsigned short* ceilopen, * flooropen;

    const volatile int actionbits = segl->actionbits;
    const int lightlevel = segl->seglightlevel;

    unsigned scalefrac = segl->scalefrac;
    unsigned scalestep = segl->scalestep;

    int x, start = segl->start;
    const int stop = segl->stop;
    const int width = stop - start + 1;

    const fixed_t ceilingheight = segl->ceilingheight;

    const int floorpicnum = segl->floorpicnum;
    const int ceilingpicnum = segl->ceilingpicnum;

    byte *topsil, *bottomsil;

    int floorplhash = 0;
    int ceilingplhash = 0;

    // force R_FindPlane for both planes
    floor = ceiling = visplanes;
    flooropen = ceilopen = visplanes[0].open;

    if (actionbits & AC_ADDFLOOR)
        floorplhash = R_PlaneHash(floorheight, floorpicnum, lightlevel);
    if (actionbits & AC_ADDCEILING)
        ceilingplhash = R_PlaneHash(ceilingheight, ceilingpicnum, lightlevel);

    if (actionbits & AC_TOPSIL)
        topsil = segl->sil;
    else
        topsil = NULL;
    if (actionbits & AC_BOTTOMSIL)
        bottomsil = segl->sil + (actionbits & AC_TOPSIL ? width : 0);
    else
        bottomsil = NULL;

    if (actionbits & AC_ADDFLOOR)
        flooropen = visplanes[0].open;
    else
        flooropen = NULL;
    if (actionbits & AC_ADDCEILING)
        ceilopen = visplanes[0].open;
    else
        ceilopen = NULL;

    unsigned short *newclipbounds = NULL;
    if (actionbits & (AC_NEWFLOOR | AC_NEWCEILING))
    {
        newclipbounds = lastsegclip - start;
        lastsegclip += width;
    }

    for (x = start; x <= stop; x++)
    {
        int floorclipx, ceilingclipx;
        int low, high, top, bottom;
        unsigned scale2;

        scale2 = (unsigned)scalefrac >> HEIGHTBITS;
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
        FixedMul2(low, scale2, floornewheight);
        low = centerY - low;
        if (low < 0)
            low = 0;
        else if (low > floorclipx)
            low = floorclipx;

        FixedMul2(high, scale2, ceilingnewheight);
        high = centerY - high;
        if (high > viewportHeight)
            high = viewportHeight;
        else if (high < ceilingclipx)
            high = ceilingclipx;

        // top sprite clip sil
        if (topsil)
            topsil[x] = high+1;

        // bottom sprite clip sil
        if (bottomsil)
            bottomsil[x] = low+1;

        int newclip = actionbits & (AC_NEWFLOOR|AC_NEWCEILING);
        if (newclipbounds)
        {
            if (!(newclip & AC_NEWFLOOR))
                low = floorclipx;
            if (!(newclip & AC_NEWCEILING))
                high = ceilingclipx;
            newclip = (high << 8) | low;

            // rewrite clipbounds
            clipbounds[x] = newclip;
            newclipbounds[x] = newclip;
        }

        //
        // floor
        //
        if (flooropen)
        {
            FixedMul2(top, scale2, floorheight);
            top = centerY - top;
            if (top < ceilingclipx)
                top = ceilingclipx;
            bottom = floorclipx;

            if (top < bottom)
            {
                if (!MARKEDOPEN(flooropen[x]))
                {
                    floor = R_FindPlane(floorplhash, floorheight, 
                        floorpicnum, lightlevel, x, stop);
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
            FixedMul2(bottom, scale2, ceilingheight);
            bottom = centerY - bottom;
            if (bottom > floorclipx)
                bottom = floorclipx;

            if (top < bottom)
            {
                if (!MARKEDOPEN(ceilopen[x]))
                {
                    ceiling = R_FindPlane(ceilingplhash, ceilingheight, 
                        ceilingpicnum, lightlevel, x, stop);
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
    viswall_t *segl;
    viswall_t *first, *verylast;
    uint32_t clipbounds_[SCREENWIDTH/2+1];
    uint16_t *clipbounds = (uint16_t *)clipbounds_;
    fixed_t floorheight = 0;
    fixed_t floornewheight = 0, ceilingnewheight = 0;
    vertex_t *verts;

    R_InitClipBounds(clipbounds_);

#ifdef MARS
    verts = W_GetLumpData(gamemaplump+ML_VERTEXES);
#endif

    first = viswalls;
    verylast = NULL;

    for (segl = first; segl != verylast; )
    {
        int nextsegs;
        viswall_t* last;

        nextsegs = MARS_SYS_COMM6;

        // check if master CPU finished exec'ing R_BSP()
        if (nextsegs == 0xffff)
        {
            Mars_ClearCacheLine(&lastwallcmd);
            verylast = lastwallcmd;
            nextsegs = verylast - first;
        }

        for (last = first + nextsegs; segl < last; segl++)
        {
            R_WallEarlyPrep(segl, &floorheight, &floornewheight, &ceilingnewheight);

            R_WallLatePrep(segl, verts);

            R_SegLoop(segl, clipbounds, floorheight, floornewheight, ceilingnewheight);

            // FIXME: optimize this

            if (segl->actionbits & AC_TOPTEXTURE)
            {
                texture_t* tex = &textures[segl->t_texturenum];
                Mars_ClearCacheLine(&tex->data);
            }
            if (segl->actionbits & AC_BOTTOMTEXTURE)
            {
                texture_t* tex = &textures[segl->b_texturenum];
                Mars_ClearCacheLine(&tex->data);
            }

            if (segl->actionbits & AC_ADDFLOOR)
            {
                Mars_ClearCacheLine(&flatpixels[segl->floorpicnum]);
            }
            if (segl->actionbits & AC_ADDCEILING)
            {
                Mars_ClearCacheLine(&flatpixels[segl->ceilingpicnum]);
            }

            MARS_SYS_COMM8++;
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

    for (segl = viswalls; segl != lastwallcmd; segl++)
    {
        fixed_t floornewheight = 0, ceilingnewheight = 0;

        R_WallEarlyPrep(segl, &floornewheight, &ceilingnewheight);

        R_WallLatePrep(segl, vertexes);

        R_SegLoop(segl, clipbounds, floornewheight, ceilingnewheight);
    }
}

#endif

// EOF

