/* Z_zone.c */

#include "doomdef.h"

/* 
============================================================================== 
 
						ZONE MEMORY ALLOCATION 
 
There is never any space between memblocks, and there will never be two
contiguous free memblocks.

The rover can be left pointing at a non-empty block

It is of no value to free a cachable block, because it will get overwritten
automatically if needed

============================================================================== 
*/ 
 
memzone_t	*mainzone;
memzone_t	*refzone;

 
/*
========================
=
= Z_InitZone
=
========================
*/

memzone_t *Z_InitZone (byte *base, int size)
{
	memzone_t *zone;
	
	zone = (memzone_t *)base;
	
	zone->size = size;
	zone->rover = &zone->blocklist;
	zone->blocklist.size = size - 8;
	zone->blocklist.tag = 0;
	zone->blocklist.id = ZONEID;
	zone->blocklist.next = NULL;
	zone->blocklist.prev = NULL;
#ifndef MARS
	zone->blocklist.lockframe = -1;
#endif
	return zone;
}

/*
========================
=
= Z_Init
=
========================
*/

void Z_Init (void)
{
	byte	*mem;
	int		size;

	mem = I_ZoneBase (&size);
	
#ifdef MARS
/* mars doesn't have a refzone */
	D_memset(mem, 0, size);
	mainzone = Z_InitZone (mem,size);
#else
	mainzone = Z_InitZone (mem,0x80000);
	refzone = Z_InitZone (mem+0x80000,size-0x80000);
#endif
}


/*
========================
=
= Z_Free2
=
========================
*/

void Z_Free2 (memzone_t *mainzone, void *ptr)
{
	memblock_t	*block, *adj;
	
	block = (memblock_t *) ( (byte *)ptr - sizeof(memblock_t));
	if (block->id != ZONEID)
		I_Error ("Z_Free: freed a pointer without ZONEID");

	block->tag = 0;
	block->id = 0;

	// merge with adjacent blocks
	adj = block->prev;
	if (adj && !adj->tag)
	{
		adj->next = block->next;
		adj->next->prev = adj;
		adj->size += block->size;
		if (mainzone->rover == block)
			mainzone->rover = adj;
		block = adj;
	}

	adj = block->next;
	if (adj && !adj->tag)
	{
		block->next = adj->next;
		if (block->next)
			block->next->prev = block;
		block->size += adj->size;
		if (mainzone->rover == adj)
			mainzone->rover = block;
	}
}

 
/*
========================
=
= Z_Malloc2
=
========================
*/

#define MINFRAGMENT	64

void *Z_Malloc2 (memzone_t *mainzone, int size, int tag, boolean err)
{
	int		extra;
	memblock_t	*start, *rover, *new, *base;

#if 0
Z_CheckHeap (mainzone);	/* DEBUG */
#endif

/* */
/* scan through the block list looking for the first free block */
/* of sufficient size, throwing out any purgable blocks along the way */
/* */
	size += sizeof(memblock_t);	/* account for size of block header */
	size = (size+3)&~3;			/* longword align everything */
	
	
	start = base = mainzone->rover;
	
	while (base->tag || base->size < size)
	{
		if (base->tag)
			rover = base;
		else
			rover = base->next;
			
		if (!rover)
			goto backtostart;
		
		if (rover->tag)
		{
		/* hit an in use block, so move base past it */
			base = rover->next;
			if (!base)
			{
backtostart:
				base = &mainzone->blocklist;
			}
			
			if (base == start)	/* scaned all the way around the list */
			{
				if (err)
					I_Error("Z_Malloc: failed on %i", size);
				return NULL;
			}
			continue;
		}
	}
	
/* */
/* found a block big enough */
/* */
	extra = base->size - size;
	if (extra >  MINFRAGMENT)
	{	/* there will be a free fragment after the allocated block */
		new = (memblock_t *) ((byte *)base + size );
		new->size = extra;
		new->tag = 0;		/* free block */
		new->prev = base;
		new->next = base->next;
		if (new->next)
			new->next->prev = new;
		base->next = new;
		base->size = size;
	}
	
	base->tag = tag;
	base->id = ZONEID;
#ifndef MARS
	base->lockframe = -1;
#endif	
	mainzone->rover = base->next;	/* next allocation will start looking here */
	if (!mainzone->rover)
		mainzone->rover = &mainzone->blocklist;
		
	return (void *) ((byte *)base + sizeof(memblock_t));
}


/*
========================
=
= Z_FreeTags
=
========================
*/

void Z_FreeTags (memzone_t *mainzone)
{
	memblock_t	*block, *next;
	
	for (block = &mainzone->blocklist ; block ; block = next)
	{
		next = block->next;		/* get link before freeing */
		if (!block->tag)
			continue;			/* free block */
		if (block->tag == PU_LEVEL || block->tag == PU_LEVSPEC)
			Z_Free2 (mainzone, (byte *)block+sizeof(memblock_t));
	}
}


/*
========================
=
= Z_CheckHeap
=
========================
*/

memblock_t	*checkblock;

void Z_CheckHeap (memzone_t *mainzone)
{
	
	for (checkblock = &mainzone->blocklist ; checkblock; checkblock = checkblock->next)
	{
		if (!checkblock->next)
		{
			if ((byte *)checkblock + checkblock->size - (byte *)mainzone
			!= mainzone->size)
				I_Error ("Z_CheckHeap: zone size changed\n");
			continue;
		}
		
		if ( (byte *)checkblock + checkblock->size != (byte *)checkblock->next)
			I_Error ("Z_CheckHeap: block size does not touch the next block\n");
		if ( checkblock->next->prev != checkblock)
			I_Error ("Z_CheckHeap: next block doesn't have proper back link\n");
	}
}


/*
========================
=
= Z_ChangeTag
=
========================
*/

void Z_ChangeTag (void *ptr, int tag)
{
	memblock_t	*block;
	
	block = (memblock_t *) ( (byte *)ptr - sizeof(memblock_t));
	if (block->id != ZONEID)
		I_Error ("Z_ChangeTag: freed a pointer without ZONEID");
	block->tag = tag;
}


/*
========================
=
= Z_FreeMemory
=
========================
*/

int Z_FreeMemory (memzone_t *mainzone)
{
	memblock_t	*block;
	int			free;
	
	free = 0;
	for (block = &mainzone->blocklist ; block ; block = block->next)
		if (!block->tag)
			free += block->size;
	return free;
}

/*
========================
=
= Z_LargestFreeBlock
=
========================
*/
int Z_LargestFreeBlock(memzone_t *mainzone)
{
	memblock_t	*block;
	int			free;
	
	free = 0;
	for (block = &mainzone->blocklist ; block ; block = block->next)
		if (!block->tag)
			if (block->size > free) free = block->size;
	return free;
}

/*
========================
=
= Z_ForEachBlock
=
========================
*/
void Z_ForEachBlock(memzone_t *mainzone, memblockcall_t cb, void *p)
{
	memblock_t	*block, *next;

	for (block = &mainzone->blocklist ; block ; block = next)
	{
		next = block->next;
		if (block->tag)
			cb((byte *)block + sizeof(memblock_t), p);
	}
}

/*
========================
=
= Z_FreeBlocks
=
========================
*/
int Z_FreeBlocks(memzone_t* mainzone)
{
	int total = 0;
	memblock_t* block, * next;

	for (block = &mainzone->blocklist; block; block = next)
	{
		next = block->next;
		if (!block->tag)
			total += block->size;
	}
	return total;
}

/*
========================
=
= Z_DumpHeap
=
========================
*/

#ifdef NeXT

void Z_DumpHeap (memzone_t *mainzone)
{
	memblock_t	*block;
	
	printf ("zone size: %i  location: %p\n",mainzone->size,mainzone);
	
	for (block = &mainzone->blocklist ; block ; block = block->next)
	{
		printf ("block:%p    size:%7i    tag:%3i    frame:%i\n",
			block, block->size, block->tag,block->lockframe);
		if (!block->next)
			continue;
			
		if ( (byte *)block + block->size != (byte *)block->next)
			printf ("ERROR: block size does not touch the next block\n");
		if ( block->next->prev != block)
			printf ("ERROR: next block doesn't have proper back link\n");
	}
}

#endif

