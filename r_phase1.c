/*
  CALICO

  Renderer phase 1 - BSP traversal
*/

#include "doomdef.h"
#include "r_local.h"
#ifdef MARS
#include "mars.h"
#endif

typedef struct
{
   VINT first;
   VINT last;
} cliprange_t
__attribute__((aligned(4)))
;

#define MAXSEGS 32

typedef struct
{
   cliprange_t *newend;
   cliprange_t solidsegs[MAXSEGS];
   seg_t       *curline;
   angle_t    lineangle1;
   int        splitspans; /* generate wall splits until this reaches 0 */
   VINT       lastv1;
   VINT       lastv2;
   angle_t    lastangle1;
   angle_t    lastangle2;
} rbspWork_t;

static int R_ClipToViewEdges(angle_t angle1, angle_t angle2) ATTR_DATA_CACHE_ALIGN;
static void R_AddLine(rbspWork_t *rbsp, sector_t *frontsector, seg_t* line) ATTR_DATA_CACHE_ALIGN;
static void R_ClipWallSegment(rbspWork_t *rbsp, fixed_t first, fixed_t last, boolean solid) ATTR_DATA_CACHE_ALIGN;
static boolean R_CheckBBox(rbspWork_t *rbsp, fixed_t bspcoord[4]) ATTR_DATA_CACHE_ALIGN;
static void R_Subsector(rbspWork_t *rbsp, int num) ATTR_DATA_CACHE_ALIGN;
static void R_StoreWallRange(rbspWork_t *rbsp, int start, int stop) ATTR_DATA_CACHE_ALIGN;
static void R_RenderBSPNode(rbspWork_t *rbsp, int bspnum) ATTR_DATA_CACHE_ALIGN;
static void R_WallEarlyPrep(viswall_t* segl, fixed_t *floorheight, 
    fixed_t *floornewheight, fixed_t *ceilingnewheight) ATTR_DATA_CACHE_ALIGN;

#ifdef MARS
__attribute__((aligned(4)))
#endif
static VINT checkcoord[12][4] =
{
   { 3, 0, 2, 1 },
   { 3, 0, 2, 0 },
   { 3, 1, 2, 0 },
   { 0, 0, 0, 0 },
   { 2, 0, 2, 1 },
   { 0, 0, 0, 0 },
   { 3, 1, 3, 0 },
   { 0, 0, 0, 0 },
   { 2, 0, 3, 1 },
   { 2, 1, 3, 1 },
   { 2, 1, 3, 0 },
   { 0, 0, 0, 0 }
};

static sector_t emptysector = { 0, 0, -2, -2, -2 };

static int R_ClipToViewEdges(angle_t angle1, angle_t angle2)
{
   angle_t span, tspan;
   int x1, x2;

   // clip to view edges
   span = angle1 - angle2;
   if(span >= ANG180)
      return 0;

   angle1 -= vd.viewangle;
   angle2 -= vd.viewangle;

   tspan = angle1 + vd.clipangle;
   if(tspan > vd.doubleclipangle)
   {
      tspan -= vd.doubleclipangle;
      // totally off the left edge?
      if(tspan >= span)
         return -1;
      angle1 = vd.clipangle;
   }

   tspan = vd.clipangle - angle2;
   if(tspan > vd.doubleclipangle)
   {
      tspan -= vd.doubleclipangle;
      if(tspan >= span)
         return -1;
      angle2 = 0 - vd.clipangle;
   }

   // find the first clippost that touches the source post (adjacent pixels
   // are touching).
   angle1 = (angle1 + ANG90) >> ANGLETOFINESHIFT;
   angle2 = (angle2 + ANG90) >> ANGLETOFINESHIFT;
   x1    = vd.viewangletox[angle1];
   x2    = vd.viewangletox[angle2];

   // does not cross a pixel?
   if(x1 == x2)
      return -1;

   return (x1<<16) | x2;
}

//
// Checks BSP node/subtree bounding box. Returns true if some part of the bbox
// might be visible.
//
static boolean R_CheckBBox(rbspWork_t *rbsp, fixed_t bspcoord[4])
{
   int boxx;
   int boxy;
   int boxpos;

   fixed_t x1, y1, x2, y2;

   angle_t angle1, angle2;

   cliprange_t *start;

   int sx1, sx2;

   // find the corners of the box that define the edges from current viewpoint
   if(vd.viewx <= bspcoord[BOXLEFT])
      boxx = 0;
   else if(vd.viewx < bspcoord[BOXRIGHT])
      boxx = 1;
   else
      boxx = 2;

   if(vd.viewy >= bspcoord[BOXTOP])
      boxy = 0;
   else if(vd.viewy > bspcoord[BOXBOTTOM])
      boxy = 1;
   else
      boxy = 2;

   boxpos = (boxy << 2) + boxx;
   if(boxpos == 5)
      return true;

   x1 = bspcoord[checkcoord[boxpos][0]];
   y1 = bspcoord[checkcoord[boxpos][1]];
   x2 = bspcoord[checkcoord[boxpos][2]];
   y2 = bspcoord[checkcoord[boxpos][3]];

   // check clip list for an open space
   angle1 = R_PointToAngle(x1, y1);
   angle2 = R_PointToAngle(x2, y2);

   sx1 = R_ClipToViewEdges(angle1, angle2);
   if (sx1 == 0)
      return true;
   if (sx1 == -1)
      return false;
   sx2 = (sx1 & 0xffff) - 1;
   sx1 = sx1 >>    16;

   start = rbsp->solidsegs;
   while(start->last < sx2)
      ++start;

   // does the clippost contain the new span?
   if(sx1 >= start->first && sx2 <= start->last)
      return false;

   return true;
}

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

      f_ceilingpic    = front_sector->ceilingpic;
      f_lightlevel    = front_sector->lightlevel;
      f_floorheight   = front_sector->floorheight   - vd.viewz;
      f_ceilingheight = front_sector->ceilingheight - vd.viewz;

      segl->floorpicnum   = flattranslation[front_sector->floorpic];

      if (f_ceilingpic != -1)
      {
          segl->ceilingpicnum = flattranslation[f_ceilingpic];
      }
      else
      {
          segl->ceilingpicnum = -1;
      }

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
         *floorheight = *floornewheight = f_floorheight;
         actionbits |= (AC_ADDFLOOR|AC_NEWFLOOR);
      }

      if(!skyhack                                         && // not a sky hack wall
         (f_ceilingheight > 0 || f_ceilingpic == -1)      && // ceiling below camera, or sky
         (f_ceilingpic    != b_ceilingpic                 || // ceiling texture changes across line?
          f_ceilingheight != b_ceilingheight              || // height changes across line?              
          f_lightlevel    != b_lightlevel                 || // light level changes across line?
          b_ceilingheight == b_floorheight))                 // backsector is closed?
      {
         segl->ceilingheight = *ceilingnewheight = f_ceilingheight;
         if(f_ceilingpic == -1)
            actionbits |= (AC_ADDSKY|AC_NEWCEILING);
         else
            actionbits |= (AC_ADDCEILING|AC_NEWCEILING);
      }

      segl->t_topheight = f_ceilingheight; // top of texturemap

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
         segl->t_bottomheight = f_floorheight; // set bottom height
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

            segl->b_topheight = *floornewheight = b_floorheight;
            segl->b_bottomheight = f_floorheight;
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

            segl->t_bottomheight = *ceilingnewheight = b_ceilingheight;
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
// Store information about the clipped seg range into the viswall array.
//
static void R_StoreWallRange(rbspWork_t *rbsp, int start, int stop)
{
   viswall_t *rw;
   viswallextra_t *rwex;
   int newstop;
   int numwalls = lastwallcmd - viswalls;
   const int maxlen = centerX/2;
   // split long segments
   int len = stop - start + 1;

   if (numwalls == MAXWALLCMDS)
      return;
   if (rbsp->splitspans <= 0 || len < maxlen)
      newstop = stop;
   else
      newstop = start + maxlen - 1;

   rwex = viswallextras + numwalls;
   do {
      rw = lastwallcmd;
      rw->seg = rbsp->curline;
      rw->start = start;
      rw->stop = newstop;
      rw->scalestep = rbsp->lineangle1;
      rw->actionbits = 0;
      ++lastwallcmd;

      R_WallEarlyPrep(rw, &rwex->floorheight, &rwex->floornewheight, &rwex->ceilnewheight);

#ifdef MARS
      Mars_R_WallNext();
#endif

      rwex++;
      numwalls++;
      if (numwalls == MAXWALLCMDS)
         return;

      start = newstop + 1;
      newstop += maxlen;
      if (newstop > stop)
         newstop = stop;
   } while (start <= stop);

   rbsp->splitspans -= len;
}

//
// Clips the given range of columns, but does not include it in the clip list.
// Does handle windows, e.g., linedefs with upper and lower textures.
//
static void R_ClipWallSegment(rbspWork_t *rbsp, fixed_t first, fixed_t last, boolean solid)
{
    fixed_t      scratch;
    cliprange_t* start, * next;

    // find the first range that touches the range (adjacent pixels are touching)
    scratch = first - 1;
    start = rbsp->solidsegs;
    while (start->last < scratch)
        ++start;

    // add visible pieces and close up holes
    if (first < start->first)
    {
        if (last < start->first - 1)
        {
            // post is entirely visible (above start)
            R_StoreWallRange(rbsp, first, last);

            if (solid)
            {
                next = rbsp->newend;
                ++rbsp->newend;

               if (next != start)
               {
                  int *s = (int *)start;
                  int n = *s;
                  int cnt = next - start;

                  next = start;
                  do {
                     int nn = *++s;
                     *s = n, n = nn;
                  } while (--cnt);
               }

                next->first = first;
                next->last = last;
            }

            return;
        }

        // there is a fragment above *start
        R_StoreWallRange(rbsp, first, start->first - 1);

        // now adjust the clip size
        if (solid)
        {
            start->first = first;
        }
    }

    // bottom contained in start?
    if (last <= start->last)
        return;

    next = start;
    while (last >= (next + 1)->first - 1)
    {
        // there is a fragment between two posts
        R_StoreWallRange(rbsp, next->last + 1, (next + 1)->first - 1);
        ++next;

        if (last <= next->last)
        {
            // bottom is contained in next
            last = next->last;
            goto crunch;
        }
    }

    // there is a fragment after *next
    R_StoreWallRange(rbsp, next->last + 1, last);
crunch:
    if (solid)
    {
        // adjust the clip size
        start->last = last;

        // remove start+1 to next from the clip list, because start now covers
        // their area
        if (next == start) // post just extended past the bottom of one post
            return;

        if (next != rbsp->newend)
        {
            int cnt = rbsp->newend - next;
            do
            {
               start++, next++;
               *((int*)start) = *((int*)next);
            } while (--cnt);
        }

        rbsp->newend = start + 1;
    }
}

//
// Clips the given segment and adds any visible pieces to the line list.
//
static void R_AddLine(rbspWork_t *rbsp, sector_t *frontsector, seg_t *line)
{
   angle_t angle1, angle2;
   fixed_t x1, x2;
   sector_t *backsector;
   vertex_t *v1 = &vertexes[line->v1], *v2 = &vertexes[line->v2];
   int side;
   line_t *ldef;
   side_t *sidedef;
   boolean solid;

   if (line->v1 == rbsp->lastv2)
      angle1 = rbsp->lastangle2;
   else
      angle1 = R_PointToAngle(v1->x, v1->y);
   if (line->v2 == rbsp->lastv1)
      angle2 = rbsp->lastangle1;
   else
      angle2 = R_PointToAngle(v2->x, v2->y);

   rbsp->lastv1 = line->v1;
   rbsp->lastv2 = line->v2;
   rbsp->lastangle1 = angle1;
   rbsp->lastangle2 = angle2;

   x1 = R_ClipToViewEdges(angle1, angle2);
   if (x1 <= 0)
      return;
   x2 = (x1 & 0xffff) - 1;
   x1 = x1 >>    16;

   // decide which clip routine to use
   side = line->side;
   ldef = &lines[line->linedef];
   backsector = (ldef->flags & ML_TWOSIDED) ? &sectors[sides[ldef->sidenum[side^1]].sector] : 0;
   sidedef = &sides[ldef->sidenum[side]];
   solid = false;

   if (!backsector ||
       backsector->ceilingheight <= frontsector->floorheight ||
       backsector->floorheight >= frontsector->ceilingheight)
   {
       solid = true;
   }
   else if (backsector->ceilingheight == frontsector->ceilingheight &&
       backsector->floorheight == frontsector->floorheight)
   {
       // reject empty lines used for triggers and special events
       if (backsector->ceilingpic == frontsector->ceilingpic &&
           backsector->floorpic == frontsector->floorpic &&
           *(int8_t *)&backsector->lightlevel == *(int8_t *)&frontsector->lightlevel && // hack to get rid of the extu.w on SH-2
           sidedef->midtexture == 0)
           return;
   }

   rbsp->curline = line;
   rbsp->lineangle1 = angle1;
   R_ClipWallSegment(rbsp, x1, x2, solid);
}

//
// Determine floor/ceiling planes, add sprites of things in sector,
// draw one or more segments.
//
static void R_Subsector(rbspWork_t *rbsp, int num)
{
   subsector_t *sub = &subsectors[num];
   seg_t       *line, *stopline;
   int          count;
   sector_t    *frontsector = sub->sector;
      
   if (frontsector->thinglist)
   {
      if(frontsector->validcount != validcount[0]) // not already processed?
      {
         frontsector->validcount = validcount[0];  // mark it as processed
         if (lastvissector < vissectors + MAXVISSSEC)
         {
           *lastvissector++ = frontsector;
         }
      }
   }

   line     = &segs[sub->firstline];
   count    = sub->numlines;
   stopline = line + count;

   while(line != stopline)
      R_AddLine(rbsp, frontsector, line++);
}

//
// Recursively descend through the BSP, classifying nodes according to the
// player's point of view, and render subsectors in view.
//
static void R_RenderBSPNode(rbspWork_t *rbsp, int bspnum)
{
   node_t *bsp;
   int     side;

check:
#ifdef MARS
   if((int16_t)bspnum < 0) // reached a subsector leaf?
#else
   if(bspnum & NF_SUBSECTOR) // reached a subsector leaf?
#endif
   {
      R_Subsector(rbsp, bspnum == -1 ? 0 : bspnum & ~NF_SUBSECTOR);
      return;
   }

   bsp = &nodes[bspnum];

   // decide which side the view point is on
   side = R_PointOnSide(vd.viewx, vd.viewy, bsp);

   // recursively render front space
   R_RenderBSPNode(rbsp, bsp->children[side]);

   // possibly divide back space
   if(R_CheckBBox(rbsp, bsp->bbox[side^1])) {
      bspnum = bsp->children[side^1];
      goto check;
   }
}

//
// Kick off the rendering process by initializing the solidsegs array and then
// starting the BSP traversal.
//
void R_BSP(void)
{
   rbspWork_t rbsp;
   cliprange_t *solidsegs = rbsp.solidsegs;

#ifdef MARS
   W_GetLumpData(gamemaplump);
#endif

   solidsegs[0].first = -2;
   solidsegs[0].last  = -1;
   solidsegs[1].first = viewportWidth;
   solidsegs[1].last  = viewportWidth+1;
   rbsp.newend = &solidsegs[2];
   rbsp.splitspans = viewportWidth + viewportWidth/2;
   rbsp.lastv1 = -1;
   rbsp.lastv2 = -1;

   R_RenderBSPNode(&rbsp, numnodes - 1);
}

// EOF

