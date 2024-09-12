/* m_main.c -- main menu */

#include "doomdef.h"

#define MOVEWAIT		(I_IsPAL() ? TICVBLS*5 : TICVBLS*6)
#define CURSORX		(80)
#define CURSORWIDTH	24
#define ITEMX		(CURSORX+CURSORWIDTH)
#define STARTY			24
#define ITEMSPACE	20
#define CURSORY(y)	(STARTY+ITEMSPACE*(y))
#define	NUMLCHARS 64
#define MAXITEMS    8

typedef struct
{
	VINT	x;
	VINT 	y;
	char 	name[64];
} menuitem_t;

typedef struct
{
	VINT numitems;
    menuitem_t items[MAXITEMS];

    VINT m_skull1lump, m_skull2lump;
    VINT m_skilllump;
    VINT numslump;

    VINT cursorframe;
    VINT cursordelay;
    VINT cursorpos;
    VINT movecount;
} menuscreen_t;

static menuscreen_t *gs_menu = NULL;

extern VINT	uchar, snums;
extern void print(int x, int y, const char* string);

void GS_Start(void)
{
    int i;
    int n;
    char *buf;

    S_StopSong();

    I_SetPalette(W_POINTLUMPNUM(W_GetNumForName("PLAYPALS")));

    if (gs_menu == NULL)
        gs_menu = Z_Malloc(sizeof(*gs_menu), PU_STATIC);
    D_memset(gs_menu, 0, sizeof(*gs_menu));

    /* cache all needed graphics	 */
    gs_menu->m_skull1lump = W_CheckNumForName("M_SKULL1");
    gs_menu->m_skull2lump = W_CheckNumForName("M_SKULL2");

    gs_menu->m_skilllump = W_CheckNumForName("SKILL0");

    gs_menu->numslump = W_CheckNumForName("NUM_0");

    uchar = W_CheckNumForName("CHAR_065");

    snums = W_CheckNumForName("NUM_0");

    gs_menu->cursorframe = -1;
    gs_menu->cursorpos = 0;
    gs_menu->cursordelay = MOVEWAIT;

    n = I_ReadCDDirectory("/");
    buf = I_GetCDFileBuffer();
    for (i = 0; i < n; i++)
    {
        char *name;
        int flags;
        int namelen;
        menuitem_t *mi;

        if (gs_menu->numitems == MAXITEMS)
            break;

        namelen = mystrlen(buf);

        name = buf;
        buf += namelen + 1;
        buf += 4; // length
        flags = (uint8_t)*buf;
        buf += 1;

        // if a directory, then bit 1 is 1, bits 2,3 and 7 are zero
        // see 9.1.6 at https://pismotec.com/cfs/iso9660-1999.html 
        if ((flags & 0x8E) == 0x2)
            continue;

        if (namelen >= (int)sizeof(mi->name))
            continue;
        if (namelen <= 4)
            continue;

        if (!D_strcasecmp(name, SOUNDS_PWAD_NAME))
            continue;
        if (!D_strcasecmp(name, MAPDEV_PWAD_NAME))
            continue;

        if (D_strncasecmp(&name[namelen - 4], ".wad", 4))
            continue;

        mi = &gs_menu->items[gs_menu->numitems];
        D_memcpy(mi->name, name, namelen);
        mi->x = ITEMX;
        mi->y = CURSORY(gs_menu->numitems);
        gs_menu->numitems++;
    }

    DoubleBufferSetup();

    if (gs_menu->numitems == 0)
        I_Error("No playable WADs found on the CD");
}

void GS_Stop(void)
{
    if (gs_menu == NULL)
        return;

    D_snprintf(cd_pwad_name, sizeof(cd_pwad_name), "%s", gs_menu->items[gs_menu->cursorpos].name);
    Z_Free(gs_menu);
    gs_menu = NULL;

    S_Clear();
}

/*
=================
=
= GS_Ticker
=
=================
*/

int GS_Ticker (void)
{
    int buttons, oldbuttons;
    menuscreen_t *menuscr = gs_menu;
    boolean newcursor = false;
    int sound = sfx_None;

	if (ticon < TICRATE)
		return ga_nothing; /* ignore accidental keypresses */
    if (menuscr->numitems <= 1)
        return ga_startnew;

    if (gs_menu->cursorframe == -1)
    {
        gs_menu->cursorframe = 0;
        gs_menu->cursordelay = MOVEWAIT + MOVEWAIT / 2;
        S_StartSound(NULL, sfx_swtchn);
    }

    /* animate skull */
    if (gametic != prevgametic && (gametic & 3) == 0)
    {
        gs_menu->cursorframe ^= 1;
    }

    buttons = ticrealbuttons & MENU_BTNMASK;
    oldbuttons = oldticrealbuttons & MENU_BTNMASK;

    /* exit menu if button press */
    if ((buttons & (BT_A | BT_LMBTN | BT_START)) && !(oldbuttons & (BT_A | BT_LMBTN | BT_START)))
        return ga_startnew;

    if (buttons == 0)
    {
        gs_menu->cursordelay = 0;
        return ga_nothing;
    }

    gs_menu->cursordelay -= vblsinframe;
    if (gs_menu->cursordelay > 0)
        return ga_nothing;

    gs_menu->cursordelay = MOVEWAIT;

    /* check for movement */
    if (!(buttons & (BT_UP | BT_DOWN | BT_LEFT | BT_RIGHT)))
        gs_menu->movecount = 0; /* move immediately on next press */
    else
    {
        gs_menu->movecount = 0; /* repeat move */

        if (++gs_menu->movecount == 1)
        {
            int oldcursorpos = gs_menu->cursorpos;

            if (buttons & BT_DOWN)
            {
                if (++gs_menu->cursorpos == menuscr->numitems)
                    gs_menu->cursorpos = 0;
            }

            if (buttons & BT_UP)
            {
                if (--gs_menu->cursorpos == -1)
                    gs_menu->cursorpos = menuscr->numitems - 1;
            }

            newcursor = gs_menu->cursorpos != oldcursorpos;

            if (newcursor)
                sound = sfx_pistol;
        }
    }

    if (sound != sfx_None)
        S_StartSound(NULL, sound);

    return ga_nothing;
}

/*
=================
=
= GS_Drawer
=
=================
*/

void GS_Drawer (void)
{
    int i;
    menuitem_t *items = gs_menu->items;
    int y_offset = 4;

    I_ClearFrameBuffer();

    /* Draw main menu */
    print(84, 4, "SELECT GAME");
    y_offset += ITEMSPACE + 4;

    /* draw new skull */
    if (gs_menu->cursorframe)
        DrawJagobjLump(gs_menu->m_skull2lump, CURSORX, y_offset + items[gs_menu->cursorpos].y - 2, NULL, NULL);
    else
        DrawJagobjLump(gs_menu->m_skull1lump, CURSORX, y_offset + items[gs_menu->cursorpos].y - 2, NULL, NULL);

    /* draw menu items */
    for (i = 0; i < gs_menu->numitems; i++)
    {
        int len;
        char name[16];
        int y = y_offset + items[i].y;

        len = mystrlen(items[i].name);
        D_memcpy(name, items[i].name, len-4);
        name[len-4] = 0; // strip file extension

        print(items[i].x, y, name);
    }
}
