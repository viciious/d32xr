/*
  CALICO

  Renderer phase 5 - Graphics caching
*/

#include "doomdef.h"
#include "r_local.h"
#ifdef MARS
#include "mars.h"
#endif

#ifdef JAGUAR
// Doom palette to CRY lookup (hardcoded for efficiency on the Jag ASIC?)
static pixel_t vgatojag[] =
{
       1, 51487, 55319, 30795, 30975, 30747, 30739, 30731, 30727, 43831, 44075, 48415, 53015, 47183, 47175, 51263, 
   38655, 38647, 42995, 42731, 42727, 42719, 46811, 46803, 46795, 46535, 46527, 46523, 46515, 50607, 50599, 50339, 
   50331, 54423, 54415, 54155, 54147, 54143, 53879, 57971, 57963, 57703, 57695, 57691, 57683, 61519, 61511, 61507, 
   34815, 34815, 39167, 38911, 38911, 43263, 43007, 43007, 47359, 47351, 47087, 47079, 47071, 51415, 51407, 51147, 
   51391, 51379, 51371, 51363, 51355, 51343, 51335, 51327, 51319, 51307, 51295, 51539, 51531, 51519, 51763, 51755, 
   30959, 30951, 30943, 30939, 30931, 30923, 30919, 30911, 30903, 30899, 30891, 30887, 30879, 30871, 30867, 30859, 
   30851, 30847, 30839, 30831, 30827, 30819, 30811, 30807, 30799, 30791, 30787, 30779, 30775, 30767, 30759, 30755, 
   36095, 36079, 36063, 36047, 36031, 36015, 35999, 35987, 35971, 35955, 35939, 35923, 35907, 40243, 35875, 40215, 
   39103, 39095, 39087, 39079, 39071, 43163, 43155, 43147, 43139, 43131, 43127, 43119, 43111, 43103, 43095, 47187, 
   43167, 43151, 43139, 47479, 47463, 47451, 47183, 51523, 39295, 39283, 39275, 35171, 43607, 39503, 39495, 39487, 
   48127, 52203, 56279, 56003, 60079, 59547, 63367, 63091, 30975, 34815, 38911, 42751, 46591, 50431, 54271, 58111, 
   61695, 61679, 61667, 61655, 61643, 61631, 61619, 61607, 61595, 61579, 61567, 61555, 61543, 61531, 61519, 61507, 
   30719, 26623, 22527, 18175, 17919, 13567,  9215,  4607,   255,   227,   203,   179,   155,   131,   107,    83, 
   30975, 34815, 39167, 43263, 47359, 51455, 55295, 59391, 59379, 63467, 59103, 63447, 63179, 63171, 63159, 63151, 
   30975, 35071, 39423, 48127, 52479, 56831, 61183, 65535, 63143, 62879, 62867, 62599, 47183, 51267, 51255, 55087,
      83,    71,    59,    47,    35,    23,    11,     1, 30975, 30975, 29951, 28927, 28879, 32927, 32879, 42663
};

#define MINFRAGMENT 64

static void *R_Malloc(int size, void **user)
{
   int extra;
   memblock_t *start, *rover, *base, *newblock;
  
   //
   // scan through the block list looking for the first free block
   // of sufficient size, throwing out any purgable blocks along the way
   //
   size += sizeof(memblock_t); // account for size of block header
   size = (size + 7) & ~7;     // phrase align everything

   start = base = refzone->rover;

   while(base->user || base->size < size)
   {
      if(base->user)
         rover = base;
      else
         rover = base->next;

      if(!rover)
         goto backtostart;

      if(rover->user && rover->lockframe == framecount)
      {
         // hit an in use block, so move base past it
         base = rover->next;
         if(!base)
         {
backtostart:
            base = &refzone->blocklist;
         }

         // CALICO_FIXME: if this actually runs out of RAM, it will loop forever.
         // CALICO_FIXME: also, if it frees something the game still needs, it may crash!
         if(base == start) // scanned all the way around the list
            ++framecount;  // increment framecount to try again
         continue;
      }

      // nullify user if valid
      // CALICO_FIXME: non-portable pointer comparison
      if(rover->user > (void **)1024)
         *rover->user = NULL;

      //
      // free the rover block (adding the size to base)
      //
      rover->id   = 0;
      rover->user = NULL; // mark as free

      if(base != rover)
      { 
         // merge with base
         base->size += rover->size;
         base->next = rover->next;
         if(rover->next)
            rover->next->prev = base;
      }
   }

   //
   // found a block big enough
   //
   extra = base->size - size;
   if(extra > MINFRAGMENT)
   {
      // there will be a free fragment after the allocated block
      newblock = (memblock_t *)((byte *)base + size);
      newblock->size = extra;
      newblock->user = (void **)1; // free block - CALICO_FIXME: non-portable... idiom...
      newblock->lockframe = 0;
      newblock->tag = 0;
      newblock->prev = base;
      newblock->next = base->next;
      if(newblock->next)
         newblock->next->prev = newblock;
      base->next = newblock;
      base->size = size;
   }

   base->user = user; // mark as an in use block
   base->lockframe = framecount; // mark as in use for this frame
   base->id = ZONEID;
   base->tag = PU_CACHE;

   refzone->rover = base->next; // next allocation will start looking here
   if(!refzone->rover)
      refzone->rover = &refzone->blocklist;

   *user = (void *)((byte *)base + sizeof(memblock_t));
   
   return (void *)((byte *)base + sizeof(memblock_t));
}

#define LENSHIFT 4 // this must be log2(LOOKAHEAD_SIZE)

//
// Decompress an lzss-compressed lump
//
static void R_decode(byte *input, pixel_t *output)
{
   int getidbyte = 0;
   int len;
   int pos;
   int i;
   pixel_t *source;
   int idbyte = 0;

   while(1)
   {
      // get a new idbyte if necessary
      if(!getidbyte)
         idbyte = *input++;
      getidbyte = (getidbyte + 1) & 7;

      if(idbyte&1)
      {
         // decompress
         pos = *input++ << LENSHIFT;
         pos = pos | (*input >> LENSHIFT);
         source = output - pos - 1;
         len = (*input++ & 0xf)+1;
         if(len == 1)
            break;
         for(i = 0; i < len; i++)
            *output++ = *source++;
      } 
      else 
         *output++ = vgatojag[*input++];
      idbyte = idbyte >> 1;
   }
}

//
// Load and decode a compressed graphic resource and store it in the lumpcache
//
static pixel_t *R_LoadPixels(int lumpnum)
{
   void       *rdest;
   byte       *rsrc;
   lumpinfo_t *info;
   int         count;

   // already cached?
   rdest = lumpcache[lumpnum];
   if(rdest != NULL)
      return rdest;

   info  = &lumpinfo[lumpnum];

   count = BIGLONG(info->size); // CALICO: endianness correction required

   // allocate at doubled lump size, as translates from 8-bit paletted to 
   // 16-bit CRY while decompressing
   rdest = R_Malloc(count * 2, &lumpcache[lumpnum]);
   rsrc  = wadfileptr + BIGLONG(info->filepos); // CALICO: ditto
   // decompress
   R_decode(rsrc, rdest);

   lumpcache[lumpnum] = rdest;

   return rdest;
}

//
// Cache all graphics needed to render the current frame
//
void R_Cache(void)
{
   viswall_t   *wall;
   vissprite_t *spr;

   wall = viswalls;
   while(wall < lastwallcmd)
   {
      // load upper or middle texture if needed
      if(wall->actionbits & AC_TOPTEXTURE)
      {
         if(wall->t_texture->data == NULL)
            wall->t_texture->data = R_LoadPixels(wall->t_texture->lumpnum);
      }

      // load lower texture if needed
      if(wall->actionbits & AC_BOTTOMTEXTURE)
      {
         if(wall->b_texture->data == NULL)
            wall->b_texture->data = R_LoadPixels(wall->b_texture->lumpnum);
      }

      int floorpicnum = wall->floorpicnum;
      int ceilingpicnum = wall->ceilingpicnum;
      
      // load floorpic
      if(flatpixels[floorpicnum] == NULL)
          flatpixels[floorpicnum] = R_LoadPixels(firstflat + floorpicnum);

      // load sky or normal ceilingpic
      if(ceilingpicnum == -1) // sky 
      {
         if(skytexturep->data == NULL)
            skytexturep->data = R_LoadPixels(skytexturep->lumpnum);
      }
      else if(flatpixels[ceilingpicnum] == NULL)
          flatpixels[ceilingpicnum] = R_LoadPixels(firstflat + ceilingpicnum);

      ++wall;
   }

   spr = vissprites;
   while(spr < vissprite_p)
   {
      if(spr->pixels == NULL)
         spr->pixels = R_LoadPixels(spr->patchnum + 1);

      ++spr;
   }
}

#endif

#ifdef MARS

void R_Cache(void)
{
    const int id = r_texcache.bestobj;

    if (id == -1)
        return;
    if (r_texcache.bestcount < 128)
    {
        /* ignore extremely small fragments */
        return;
    }

    if (id < numtextures)
    {
        texture_t* tex = &textures[id];
        int pixels = (int)tex->width * tex->height;
        R_AddToTexCache(&r_texcache, id, pixels, tex->lumpnum, (void**)&tex->data);
    }
    else
    {
        int flatnum = id - numtextures;
        int pixels = 64 * 64;
        R_AddToTexCache(&r_texcache, id, pixels, firstflat + flatnum, (void**)&flatpixels[flatnum]);
    }
}

#endif

// EOF

