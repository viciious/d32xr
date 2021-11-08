/*
  CALICO

  Renderer phase 2 - Wall prep
*/

#include "doomdef.h"
#include "r_local.h"
#include "mars.h"

static sector_t emptysector = { 0, 0, -2, -2, -2 };

void R_WallPrep(void) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
static void R_WallEarlyPrep(viswall_t* segl) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
static fixed_t R_PointToDist(fixed_t x, fixed_t y) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
static fixed_t R_ScaleFromGlobalAngle(fixed_t rw_distance, angle_t visangle, angle_t normalangle) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
static void R_SetupCalc(viswall_t* wc, fixed_t hyp, angle_t normalangle) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;
static void R_WallEarlyPrep2(viswall_t* wc) ATTR_DATA_CACHE_ALIGN ATTR_OPTIMIZE_SIZE;

static void R_WallEarlyPrep(viswall_t* segl)
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
   unsigned int actionbits;
   int        side;

   {
      seg  = segl->seg;
      li   = &lines[seg->linedef];
      side = seg->side;
      si   = &sides[li->sidenum[seg->side]];

      li->flags |= ML_MAPPED; // mark as seen

      front_sector    = &sectors[sides[li->sidenum[side]].sector];
      f_ceilingpic    = front_sector->ceilingpic;
      f_lightlevel    = front_sector->lightlevel;
      f_floorheight   = front_sector->floorheight   - vd.viewz;
      f_ceilingheight = front_sector->ceilingheight - vd.viewz;

      segl->floorpicnum   = flattranslation[front_sector->floorpic];
      segl->ceilingpicnum = f_ceilingpic == -1 ? -1 : flattranslation[f_ceilingpic];

      back_sector = (li->flags & ML_TWOSIDED) ? &sectors[sides[li->sidenum[side^1]].sector] : 0;
      if(!back_sector)
         back_sector = &emptysector;
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
         segl->floorheight = segl->floornewheight = f_floorheight / (1 << FIXEDTOHEIGHT);
         actionbits |= (AC_ADDFLOOR|AC_NEWFLOOR);
      }

      if(!skyhack                                         && // not a sky hack wall
         (f_ceilingheight > 0 || f_ceilingpic == -1)      && // ceiling below camera, or sky
         (f_ceilingpic    != b_ceilingpic                 || // ceiling texture changes across line?
          f_ceilingheight != b_ceilingheight              || // height changes across line?              
          f_lightlevel    != b_lightlevel                 || // light level changes across line?
          b_ceilingheight == b_floorheight))                 // backsector is closed?
      {
         segl->ceilingheight = segl->ceilingnewheight = f_ceilingheight / (1 << FIXEDTOHEIGHT);
         if(f_ceilingpic == -1)
            actionbits |= (AC_ADDSKY|AC_NEWCEILING);
         else
            actionbits |= (AC_ADDCEILING|AC_NEWCEILING);
      }

      segl->t_topheight = f_ceilingheight / (1 << FIXEDTOHEIGHT); // top of texturemap

      if(back_sector == &emptysector)
      {
         // single-sided line
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
            segl->b_texturenum = texturetranslation[si->bottomtexture];
            if(li->flags & ML_DONTPEGBOTTOM)
               b_texturemid = f_ceilingheight;
            else
               b_texturemid = b_floorheight;

            b_texturemid += si->rowoffset<<FRACBITS; // add in sidedef texture offset

            segl->b_topheight = segl->floornewheight = b_floorheight / (1 << FIXEDTOHEIGHT);
            segl->b_bottomheight = f_floorheight / (1 << FIXEDTOHEIGHT);
            actionbits |= (AC_BOTTOMTEXTURE|AC_NEWFLOOR); // generate bottom wall and floor
         }

         // is top texture visible?
         if(b_ceilingheight < f_ceilingheight && !skyhack)
         {
            segl->t_texturenum = texturetranslation[si->toptexture];
            if(li->flags & ML_DONTPEGTOP)
               t_texturemid = f_ceilingheight;
            else
               t_texturemid = b_ceilingheight + (textures[segl->t_texturenum].height << FRACBITS);

            t_texturemid += si->rowoffset<<FRACBITS; // add in sidedef texture offset

            segl->t_bottomheight = segl->ceilingnewheight = b_ceilingheight / (1 << FIXEDTOHEIGHT);
            actionbits |= (AC_NEWCEILING|AC_TOPTEXTURE); // draw top texture and ceiling
         }

         // check if this wall is solid, for sprite clipping
         if(b_floorheight >= f_ceilingheight || b_ceilingheight <= f_floorheight)
            actionbits |= AC_SOLIDSIL;
         else
         {
            if((b_floorheight > 0 && b_floorheight > f_floorheight) ||
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
       
      ++segl;  // next viswall
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
static void R_SetupCalc(viswall_t* wc, fixed_t hyp, angle_t normalangle)
{
    fixed_t sineval, rw_offset;
    angle_t offsetangle;

    offsetangle = normalangle - wc->angle1;

    if (offsetangle > ANG180)
        offsetangle = 0 - offsetangle;

    if (offsetangle > ANG90)
        offsetangle = ANG90;

    sineval = finesine(offsetangle >> ANGLETOFINESHIFT);
    FixedMul2(rw_offset, hyp, sineval);

    if (normalangle - wc->angle1 < ANG180)
        rw_offset = -rw_offset;

    wc->offset += rw_offset;
    wc->centerangle = ANG90 + vd.viewangle - normalangle;
}

static void R_WallEarlyPrep2(viswall_t* wc)
{
    angle_t      distangle, offsetangle, normalangle;
    seg_t* seg = wc->seg;
    fixed_t      sineval, rw_distance;
    fixed_t      scalefrac, scale2;
    fixed_t      hyp;

    // this is essentially R_StoreWallRange
    // calculate rw_distance for scale calculation
    normalangle = ((angle_t)seg->angle << 16) + ANG90;
    offsetangle = normalangle - wc->angle1;

    if ((int)offsetangle < 0)
        offsetangle = 0 - offsetangle;

    if (offsetangle > ANG90)
        offsetangle = ANG90;

    distangle = ANG90 - offsetangle;
    hyp = R_PointToDist(vertexes[seg->v1].x, vertexes[seg->v1].y);
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
        wc->actionbits |= AC_CALCTEXTURE; // set to calculate texture info
        R_SetupCalc(wc, hyp, normalangle);// do calc setup
    }

#ifdef MARS
    if (wc->stop > wc->start)
    {
        wc->scalestep = SH2_DIVU_DVDNT; // get 32-bit quotient
    }
#endif
}

#ifdef MARS

void Mars_Sec_R_WallPrep(void)
{
    int numsegs;
    viswall_t* segl;
    viswall_t *first, *verylast;
    volatile viswall_t* volatile* plast;
    volatile viswall_t* volatile* pfirst;

    Mars_ClearCacheLines((intptr_t)&vd & ~15, (sizeof(vd) + 15) / 16);
    pfirst = (volatile viswall_t* volatile *)((intptr_t)&viswalls | 0x20000000);
    plast = (volatile viswall_t* volatile*)((intptr_t)&lastwallcmd | 0x20000000);

    first = (viswall_t*)*pfirst;
    verylast = NULL;
    numsegs = 0;

    for (segl = first; segl != verylast; )
    {
        viswall_t* last;
        while (1)
        {
            int nextsegs = MARS_SYS_COMM6;
            if (numsegs == nextsegs)
                continue;

            // check if master CPU finished exec'ing R_BSP()
            if (nextsegs == MAXVISSSEC)
            {
                verylast = (viswall_t*)*plast;
                last = verylast;
            }
            else
            {
                last = first + nextsegs;
            }
            numsegs = nextsegs;
            break;
        }

        while (segl < last)
        {
            R_WallEarlyPrep(segl);

            R_WallEarlyPrep2(segl);

            ++segl; // next viswall
        }
    }
}

#else

void R_WallPrep(void)
{
    viswall_t* segl;

    for (segl = viswalls; segl != lastwallcmd; )
    {
        R_WallEarlyPrep(segl);

        R_WallEarlyPrep2(segl);

        ++segl; // next viswall
    }
}

#endif

// EOF

