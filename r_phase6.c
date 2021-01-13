/*
  CALICO

  Renderer phase 6 - Seg Loop 
*/

#include "r_local.h"

typedef struct drawtex_s
{
   pixel_t *data;
   int      width;
   int      height;
   int      topheight;
   int      bottomheight;
   int      texturemid;
} drawtex_t;

static drawtex_t toptex;
static drawtex_t bottomtex;

static int clipbounds[SCREENWIDTH];
static int lightmin, lightmax, lightsub, lightcoef;
static int floorclipx, ceilingclipx, x, scale, iscale, texturecol, texturelight;

//
// Check for a matching visplane in the visplanes array, or set up a new one
// if no compatible match can be found.
//
static visplane_t *R_FindPlane(visplane_t *check, fixed_t height, pixel_t *picnum, 
                               int lightlevel, int start, int stop)
{
   int i;

   while(check < lastvisplane)
   {
      if(height == check->height && // same plane as before?
         picnum == check->picnum &&
         lightlevel == check->lightlevel)
      {
         if(check->open[start] == OPENMARK)
         {
            // found a plane, so adjust bounds and return it
            if(start < check->minx) // in range of the plane?
               check->minx = start; // mark the new edge
            if(stop > check->maxx)
               check->maxx = stop;  // mark the new edge

            return check; // use the same one as before
         }
      }
      ++check;
   }

   // make a new plane
   check = lastvisplane;
   ++lastvisplane;

   check->height = height;
   check->picnum = picnum;
   check->lightlevel = lightlevel;
   check->minx = start;
   check->maxx = stop;

   for(i = 0; i < SCREENWIDTH/4; i++)
   {
      check->open[i*4  ] = OPENMARK;
      check->open[i*4+1] = OPENMARK;
      check->open[i*4+2] = OPENMARK;
      check->open[i*4+3] = OPENMARK;
   }

   return check;
}

//
// Render a wall texture as columns
//
static void R_DrawTexture(drawtex_t *tex)
{
   int top, bottom, colnum, frac;
   pixel_t *src;

   top = CENTERY - ((scale * tex->topheight) / (1 << (HEIGHTBITS + SCALEBITS)));

   if(top <= ceilingclipx)
      top = ceilingclipx + 1;

   bottom = CENTERY - 1 - ((scale * tex->bottomheight) / (1 << (HEIGHTBITS + SCALEBITS)));

   if(bottom >= floorclipx)
      bottom = floorclipx - 1;

   // column has no length?
   if(top > bottom)
      return;

   colnum = texturecol;
   frac = tex->texturemid - (CENTERY - top) * iscale;

   // DEBUG: fixes green pixels in MAP01...
   frac += (iscale + (iscale >> 5) + (iscale >> 6));

   while(frac < 0)
   {
      colnum--;
      frac += tex->height << FRACBITS;
   }

   // CALICO: comment says this, but the code does otherwise...
   // colnum = colnum - tex->width * (colnum / tex->width)
   colnum &= (tex->width - 1);

   // CALICO: Jaguar-specific GPU blitter input calculation starts here.
   // We invoke a software column drawer instead.
   src = tex->data + colnum * tex->height;
   if(tex->height & (tex->height - 1)) // height is not a power-of-2?
      I_DrawColumnNPO2(x, top, bottom, texturelight, frac, iscale, src, tex->height);
   else
      I_DrawColumn(x, top, bottom, texturelight, frac, iscale, src, tex->height);
}

//
// Main seg clipping loop
//
static void R_SegLoop(viswall_t *segl)
{
   int scalefrac, low, high, top, bottom;
   visplane_t *ceiling, *floor;

   x = segl->start;
   scalefrac = segl->scalefrac;

   // force R_FindPlane for both planes
   floor = ceiling = visplanes;

   do
   {
      scale = scalefrac / (1 << FIXEDTOSCALE);
      scalefrac += segl->scalestep;

      if(scale >= 0x7fff)
         scale = 0x7fff; // fix the scale to maximum

      //
      // get ceilingclipx and floorclipx from clipbounds
      //
      floorclipx   = clipbounds[x] & 0x00ff;
      ceilingclipx = ((clipbounds[x] & 0xff00) >> 8) - 1;

      //
      // texture only stuff
      //
      if(segl->actionbits & AC_CALCTEXTURE)
      {
         // calculate texture offset
         fixed_t r = FixedMul(segl->distance, 
                              finetangent[(segl->centerangle + xtoviewangle[x]) >> ANGLETOFINESHIFT]);

         // other texture drawing info
         texturecol = (segl->offset - r) / FRACUNIT;
         iscale = (1 << (FRACBITS+SCALEBITS)) / scale;

         // calc light level
         texturelight = ((scale * lightcoef) / FRACUNIT) - lightsub;
         if(texturelight < lightmin)
            texturelight = lightmin;
         if(texturelight > lightmax)
            texturelight = lightmax;

         // convert to a hardware value
         texturelight = -((255 - texturelight) << 14) & 0xffffff;

         //
         // draw textures
         //
         if(segl->actionbits & AC_TOPTEXTURE)
            R_DrawTexture(&toptex);
         if(segl->actionbits & AC_BOTTOMTEXTURE)
            R_DrawTexture(&bottomtex);
      }

      //
      // floor
      //
      if(segl->actionbits & AC_ADDFLOOR)
      {
         top = CENTERY - ((scale * segl->floorheight) / (1 << (HEIGHTBITS + SCALEBITS)));
         if(top <= ceilingclipx)
            top = ceilingclipx + 1;
         
         bottom = floorclipx - 1;
         
         if(top <= bottom)
         {
            if(floor->open[x] != OPENMARK)
            {
               floor = R_FindPlane(floor + 1, segl->floorheight, segl->floorpic, 
                                   segl->seglightlevel, x, segl->stop);
            }
            floor->open[x] = (unsigned short)((top << 8) + bottom);
         }
      }

      //
      // ceiling
      //
      if(segl->actionbits & AC_ADDCEILING)
      {
         top = ceilingclipx + 1;

         bottom = CENTERY - 1 - ((scale * segl->ceilingheight) / (1 << (HEIGHTBITS + SCALEBITS)));
         if(bottom >= floorclipx)
            bottom = floorclipx - 1;
         
         if(top <= bottom)
         {
            if(ceiling->open[x] != OPENMARK)
            {
               ceiling = R_FindPlane(ceiling + 1, segl->ceilingheight, segl->ceilingpic, 
                                     segl->seglightlevel, x, segl->stop);
            }
            ceiling->open[x] = (unsigned short)((top << 8) + bottom);
         }
      }

      //
      // calc high and low
      //
      low = CENTERY - ((scale * segl->floornewheight) / (1 << (HEIGHTBITS + SCALEBITS)));
      if(low < 0)
         low = 0;
      if(low > floorclipx)
         low = floorclipx;

      high = CENTERY - 1 - ((scale * segl->ceilingnewheight) / (1 << (HEIGHTBITS + SCALEBITS)));
      if(high > SCREENHEIGHT - 1)
         high = SCREENHEIGHT - 1;
      if(high < ceilingclipx)
         high = ceilingclipx;

      // bottom sprite clip sil
      if(segl->actionbits & AC_BOTTOMSIL)
         segl->bottomsil[x] = low;

      // top sprite clip sil
      if(segl->actionbits & AC_TOPSIL)
         segl->topsil[x] = high + 1;

      // sky mapping
      if(segl->actionbits & AC_ADDSKY)
      {
         top = ceilingclipx + 1;
         bottom = (CENTERY - ((scale * segl->ceilingheight) / (1 << (HEIGHTBITS + SCALEBITS)))) - 1;
         
         if(bottom >= floorclipx)
            bottom = floorclipx - 1;
         
         if(top <= bottom)
         {
            // CALICO: draw sky column
            int colnum = ((viewangle + xtoviewangle[x]) >> ANGLETOSKYSHIFT) & 0xff;
            pixel_t *data = skytexturep->data + colnum * skytexturep->height;
            I_DrawColumn(x, top, bottom, 0, (top * 18204) << 2,  FRACUNIT + 7281, data, 128);
         }
      }

      if(segl->actionbits & (AC_NEWFLOOR|AC_NEWCEILING))
      {
         // rewrite clipbounds
         if(segl->actionbits & AC_NEWFLOOR)
            floorclipx = low;
         if(segl->actionbits & AC_NEWCEILING)
            ceilingclipx = high;

         clipbounds[x] = ((ceilingclipx + 1) << 8) + floorclipx;
      }
   }
   while(++x <= segl->stop);
}

void R_SegCommands(void)
{
   int i;
   int *clip;
   viswall_t *segl;

   // initialize the clipbounds array
   clip = clipbounds;
   for(i = 0; i < SCREENWIDTH / 4; i++)
   {
      *clip++ = SCREENHEIGHT;
      *clip++ = SCREENHEIGHT;
      *clip++ = SCREENHEIGHT;
      *clip++ = SCREENHEIGHT;
   }

   /*
   ; setup blitter
   movei #15737348,r0   r0 = 15737348; // 0xf02204 - A1_FLAGS
   movei #145440,r1     r1 = 145440;   // X add ctrl = Add zero; Width = 128; Pixel size = 16
   store r1,(r0)        *r0 = r1;
                       
   movei #15737384,r0   r0 = 15737384; // 0xf02228 - A2_FLAGS
   movei #145952,r1     r1 = 145952;   // X add ctrl = Add zero; Width = 160; Pixel size = 16
   store r1,(r0)        *r0 = r1;
   */
  
   segl = viswalls;
   while(segl < lastwallcmd)
   {
      lightmin = segl->seglightlevel - (255 - segl->seglightlevel) * 2;
      if(lightmin < 0)
         lightmin = 0;

      lightmax = segl->seglightlevel;
      
      lightsub  = 160 * (lightmax - lightmin) / (800 - 160);
      lightcoef = ((lightmax - lightmin) << FRACBITS) / (800 - 160);

      if(segl->actionbits & AC_TOPTEXTURE)
      {
         texture_t *tex = segl->t_texture;

         toptex.topheight    = segl->t_topheight;
         toptex.bottomheight = segl->t_bottomheight;
         toptex.texturemid   = segl->t_texturemid;
         toptex.width        = tex->width;
         toptex.height       = tex->height;
         toptex.data         = tex->data;
      }

      if(segl->actionbits & AC_BOTTOMTEXTURE)
      {
         texture_t *tex = segl->b_texture;

         bottomtex.topheight    = segl->b_topheight;
         bottomtex.bottomheight = segl->b_bottomheight;
         bottomtex.texturemid   = segl->b_texturemid;
         bottomtex.width        = tex->width;
         bottomtex.height       = tex->height;
         bottomtex.data         = tex->data;
      }

      R_SegLoop(segl);

      ++segl;
   }
}

// EOF

/*
#define	AC_ADDFLOOR      1       000:00000001 0
#define	AC_ADDCEILING    2       000:00000010 1
#define	AC_TOPTEXTURE    4       000:00000100 2
#define	AC_BOTTOMTEXTURE 8       000:00001000 3
#define	AC_NEWCEILING    16      000:00010000 4
#define	AC_NEWFLOOR      32      000:00100000 5
#define	AC_ADDSKY        64      000:01000000 6
#define	AC_CALCTEXTURE   128     000:10000000 7
#define	AC_TOPSIL        256     001:00000000 8
#define	AC_BOTTOMSIL     512     010:00000000 9
#define	AC_SOLIDSIL      1024    100:00000000 10

typedef struct
{
0:  int  filepos; // also texture_t * for comp lumps
4:  int  size;
8:  char name[8];
} lumpinfo_t;

typedef struct
{
  0: fixed_t        height;
  4: pixel_t       *picnum;
  8: int            lightlevel;
 12: int            minx;
 16: int            maxx;
 20: int            pad1;              // leave pads for [minx-1]/[maxx+1]
 24: unsigned short open[SCREENWIDTH]; // top<<8 | bottom 
344: int            pad2;
} visplane_t; // sizeof() = 348

typedef struct
{
   // filled in by bsp
  0:   seg_t        *seg;
  4:   int           start;
  8:   int           stop;   // inclusive x coordinates
 12:   int           angle1; // polar angle to start

   // filled in by late prep
 16:   pixel_t      *floorpic;
 20:   pixel_t      *ceilingpic;

   // filled in by early prep
 24:   unsigned int  actionbits;
 28:   int           t_topheight;
 32:   int           t_bottomheight;
 36:   int           t_texturemid;
 40:   texture_t    *t_texture;
 44:   int           b_topheight;
 48:   int           b_bottomheight;
 52:   int           b_texturemid;
 56:   texture_t    *b_texture;
 60:   int           floorheight;
 64:   int           floornewheight;
 68:   int           ceilingheight;
 72:   int           ceilingnewheight;
 76:   byte         *topsil;
 80:   byte         *bottomsil;
 84:   unsigned int  scalefrac;
 88:   unsigned int  scale2;
 92:   int           scalestep;
 96:   unsigned int  centerangle;
100:   unsigned int  offset;
104:   unsigned int  distance;
108:   unsigned int  seglightlevel;
} viswall_t; // sizeof() = 112

typedef struct seg_s
{
 0:   vertex_t *v1;
 4:   vertex_t *v2;
 8:   fixed_t   offset;
12:   angle_t   angle;       // this is not used (keep for padding)
16:   side_t   *sidedef;
20:   line_t   *linedef;
24:   sector_t *frontsector;
28:   sector_t *backsector;  // NULL for one sided lines
} seg_t;

typedef struct line_s
{
 0: vertex_t     *v1;
 4: vertex_t     *v2;
 8: fixed_t      dx;
12: fixed_t      dy;                    // v2 - v1 for side checking
16: VINT         flags;
20: VINT         special;
24: VINT         tag;
28: VINT         sidenum[2];               // sidenum[1] will be -1 if one sided
36: fixed_t      bbox[4];
52: slopetype_t  slopetype;                // to aid move clipping
56: sector_t    *frontsector;
60: sector_t    *backsector;
64: int          validcount;               // if == validcount, already checked
68: void        *specialdata;              // thinker_t for reversable actions
72: int          fineangle;                // to get sine / cosine for sliding
} line_t;

typedef struct
{
 0:  fixed_t   textureoffset; // add this to the calculated texture col
 4:  fixed_t   rowoffset;     // add this to the calculated texture top
 8:  VINT      toptexture;
12:  VINT      bottomtexture;
16:  VINT      midtexture;
20:  sector_t *sector;
} side_t;

typedef	struct
{
 0:   fixed_t floorheight;
 4:   fixed_t ceilingheight;
 8:   VINT    floorpic;
12:   VINT    ceilingpic;        // if ceilingpic == -1,draw sky
16:   VINT    lightlevel;
20:   VINT    special;
24:   VINT    tag;
28:   VINT    soundtraversed;              // 0 = untraversed, 1,2 = sndlines -1
32:   mobj_t *soundtarget;                 // thing that made a sound (or null)
36:   VINT        blockbox[4];             // mapblock bounding box for height changes
52:   degenmobj_t soundorg;                // for any sounds played by the sector
76:   int     validcount;                  // if == validcount, already checked
80:   mobj_t *thinglist;                   // list of mobjs in sector
84:   void   *specialdata;                 // thinker_t for reversable actions
88:   VINT    linecount;
92:   struct line_s **lines;               // [linecount] size
} sector_t;

typedef struct subsector_s
{
0:   sector_t *sector;
4:   VINT      numlines;
8:   VINT      firstline;
} subsector_t;

typedef struct
{
 0:  char     name[8];  // for switch changing, etc
 8:  int      width;
12:  int      height;
16:  pixel_t *data;     // cached data to draw from
20:  int      lumpnum;
24:  int      usecount; // for precaching
28:  int      pad;
} texture_t;

typedef struct mobj_s
{
  0: struct mobj_s *prev;
  4: struct mobj_s *next;
  8: latecall_t     latecall;
 12: fixed_t        x;
 16: fixed_t        y;
 20: fixed_t        z;
 24: struct mobj_s *snext;
 28: struct mobj_s *sprev;
 32: angle_t        angle;
 36: VINT           sprite;
 40: VINT           frame;
 44: struct mobj_s *bnext;
 48: struct mobj_s *bprev;
 52: struct subsector_s *subsector;
 56: fixed_t        floorz;
 60: fixed_t        ceilingz;
 64: fixed_t        radius;
 68: fixed_t        height;
 72: fixed_t        momx;
 76: fixed_t        momy;
 80: fixed_t        momz;
 84: mobjtype_t     type;
 88: mobjinfo_t    *info;
 92: VINT           tics;
 96: state_t       *state;
100: int            flags;
104: VINT           health;
108: VINT           movedir;
112: VINT           movecount;
116: struct mobj_s *target;
120: VINT           reactiontime;
124: VINT           threshold;
128: struct player_s *player;
132: struct line_s *extradata;
136: short spawnx;
140: short spawny;
144: short spawntype;
148: short spawnangle;
} mobj_t;

typedef struct vissprite_s
{
 0:  int     x1;
 4:  int     x2;
 8:  fixed_t startfrac;
12:  fixed_t xscale;
16:  fixed_t xiscale;
20:  fixed_t yscale;
24:  fixed_t yiscale;
28:  fixed_t texturemid;
32:  patch_t *patch;
36:  int     colormap;
40:  fixed_t gx;
44:  fixed_t gy;
48:  fixed_t gz;
52:  fixed_t gzt;
56:  pixel_t *pixels;
} vissprite_t; // sizeof() == 60

typedef struct 
{ 
0:  short width;
2:  short height;
4:  short leftoffset;
6:  short topoffset;
8:  unsigned short columnofs[8];
} patch_t; 

typedef struct
{
 0:  state_t *state;
 4:  int      tics;
 8:  fixed_t  sx;
12:  fixed_t  sy;
} pspdef_t;

typedef struct
{
 0:  spritenum_t sprite;
 4:  long        frame;
 8:  long        tics;
12:  void        (*action)();
16:  statenum_t  nextstate;
20:  long        misc1;
24:  long        misc2;
} state_t;

typedef struct
{
 0:  boolean rotate;  // if false use 0 for any position
 4:  int     lump[8]; // lump to use for view angles 0-7
36:  byte    flip[8]; // flip (1 = flip) to use for view angles 0-7
} spriteframe_t;

typedef struct
{
0: int            numframes;
4: spriteframe_t *spriteframes;
} spritedef_t;

typedef struct memblock_s
{
 0: int     size; // including the header and possibly tiny fragments
 4: void  **user; // NULL if a free block
 8: short   tag;  // purgelevel
10: short   id;   // should be ZONEID
12: int     lockframe; // don't purge on the same frame
16: struct memblock_s *next;
20: struct memblock_s *prev;
} memblock_t; // sizeof() == 24

typedef struct
{
 0:  int         size;      // total bytes malloced, including header
 4:  memblock_t *rover;
 8:  memblock_t  blocklist; // start / end cap for linked list
      0: int     size; // including the header and possibly tiny fragments
12:   4: void  **user; // NULL if a free block
16:   8: short   tag;  // purgelevel
18:  10: short   id;   // should be ZONEID
20:  12: int     lockframe; // don't purge on the same frame
24:  16: struct memblock_s *next;
28:  20: struct memblock_s *prev;
} memzone_t; // sizeof() == 32

*/

