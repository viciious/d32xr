/* m_main.c -- main menu */

#include "doomdef.h"
#ifdef MARS
#include "mars.h"
#endif

#define MOVEWAIT		(I_IsPAL() ? TICVBLS*5 : TICVBLS*6)
#define CURSORX		(80)
#define CURSORWIDTH	24
#define ITEMX		(CURSORX+CURSORWIDTH)
#define STARTY			4
#define ITEMSPACE	19
#define CURSORY(y)	(STARTY+ITEMSPACE/2+ITEMSPACE*((y)+1))
#define	NUMLCHARS 64
#define MAXITEMS    128

#define SMALL_STARTX    20
#define SMALL_PAGEWDTH  160
#define SMALL_PAGEITMS  20

typedef struct
{
	VINT	x;
	VINT 	y;
    int     size;
	char 	name[64];
} menuitem_t;

typedef struct
{
    VINT mode;
	VINT numitems;
    menuitem_t items[MAXITEMS];

    VINT m_skull1lump, m_skull2lump;
    VINT m_skilllump;
    VINT numslump;

    VINT cursorframe;
    VINT cursordelay;
    VINT cursorpos;
    VINT movecount;

    char path[256];
} menuscreen_t;

static menuscreen_t *gs_menu = NULL;

extern VINT	uchar, snums;
extern void print(int x, int y, const char* string);
static void GS_PathChange(const char *newpath, int newmode);

void GS_Start(void)
{
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

    GS_PathChange("/", 0);

    DoubleBufferSetup();

    if (gs_menu->numitems == 0)
        I_Error("No playable WADs found on the CD");
}

void GS_Stop(void)
{
    if (gs_menu == NULL)
        return;

    if (gs_menu->path[0])
        D_snprintf(cd_pwad_name, sizeof(cd_pwad_name), "%s/%s", gs_menu->path, gs_menu->items[gs_menu->cursorpos].name);
    else
        D_snprintf(cd_pwad_name, sizeof(cd_pwad_name), "%s", gs_menu->items[gs_menu->cursorpos].name);
    Z_Free(gs_menu);
    gs_menu = NULL;

    S_Clear();
}

static void GS_PathChange(const char *dir, int newmode)
{
    int i;
    int n;
    char *buf;
    menuitem_t *mi;
    char realpath[sizeof(gs_menu->path)];

#ifdef MARS
    Mars_StopTrack();
#endif

    /* a directory */
    if (!D_strcasecmp(dir, ".."))
    {
        // parent directory
        int i;
        int len = mystrlen(gs_menu->path);

        D_memcpy(realpath, gs_menu->path, len);

        if (len == 0)
            return;

        for (i = len - 1; i > 0; i--)
        {
            if (realpath[i] == '/')
                break;
        }

        if (i == 0)
            D_memcpy(realpath, "/", 2);
        else
            realpath[i] = '\0';
    }
    else if (dir[0] == '/')
        D_strncpy(realpath, dir, sizeof(realpath));
    else
    {
        int len = mystrlen(gs_menu->path);
        int rem = sizeof(gs_menu->path) - len - 1;
        int alen = mystrlen(dir);

        if (rem <= alen+2)
            return;

        D_memcpy(realpath, gs_menu->path, len);

        if (len > 1)
        {
            D_memcpy(realpath + len, "/", 1);
            len++;
        }

        D_memcpy(realpath + len, dir, alen);
        realpath[len + alen] = '\0';
    }

    gs_menu->cursorframe = -1;
    gs_menu->cursorpos = 0;
    gs_menu->cursordelay = MOVEWAIT;
    gs_menu->mode = newmode;
    D_strncpy(gs_menu->path, realpath, sizeof(gs_menu->path));
    gs_menu->numitems = 0;

    dir = gs_menu->path[0] ? gs_menu->path : "/";
    if (newmode)
    {
        // file browser
        n = I_ReadCDDirectory(dir);
        buf = I_GetCDFileBuffer();

        for (i = 0; i < n; i++)
        {
            char *name;
            int flags;
            int namelen, size;

            if (gs_menu->numitems == MAXITEMS)
                break;

            namelen = mystrlen(buf);

            name = buf;
            buf += namelen + 1;

            size = ((uint8_t)buf[0] << 24) | ((uint8_t)buf[1] << 16) | ((uint8_t)buf[2] << 8) | ((uint8_t)buf[3] << 0);
            buf += 4; // size

            flags = (uint8_t)*buf;
            buf += 1;

            if (namelen >= (int)sizeof(mi->name))
                continue;
            if (!D_strcasecmp(name, "..") && !D_strcasecmp(gs_menu->path, "/"))
                continue;

            mi = &gs_menu->items[gs_menu->numitems];
            D_memcpy(mi->name, name, namelen+1);
            mi->x = 0;
            mi->y = (flags & 0x8E) == 0x2; // is a directory
            mi->size = size;
            gs_menu->numitems++;
        }

        return;
    }

    n = I_ReadCDDirectory(dir);
    buf = I_GetCDFileBuffer();
    for (i = 0; i < n; i++)
    {
        char *name;
        int flags;
        int namelen;

        if (gs_menu->numitems == MAXITEMS)
            break;
        if (CURSORY(gs_menu->numitems+1) > 200)
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
        D_memcpy(mi->name, name, namelen+1);
        mi->x = ITEMX;
        mi->y = CURSORY(gs_menu->numitems);
        gs_menu->numitems++;
    }
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

    if (!menuscr->mode)
    {
        if (menuscr->numitems <= 1)
            return ga_startnew;
    }

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

    if ((buttons & (BT_C)) && !(oldbuttons & (BT_C)))
    {
        GS_PathChange("/", gs_menu->mode^1);
        return ga_nothing;
    }

    /* exit menu if button press */
    if ((buttons & (BT_A | BT_LMBTN | BT_START)) && !(oldbuttons & (BT_A | BT_LMBTN | BT_START)))
    {
        if (gs_menu->mode)
        {
            menuitem_t *mi = &gs_menu->items[gs_menu->cursorpos];
            char newpath[sizeof(gs_menu->path)];

            if (mi->y)
            {
                /* a directory */
                GS_PathChange(mi->name, 1);
                return ga_nothing;
            }
            else
            {
                /* a file */
                int len = mystrlen(mi->name);

#ifdef MARS
                Mars_StopTrack();

                Mars_UpdateCD();

                if (len > 4 && !D_strcasecmp(mi->name + len - 4, ".pcm"))
                {
                    // start SPCM playback
                    D_snprintf(newpath, sizeof(newpath), "%s/%s", gs_menu->path, mi->name);
                    Mars_SetMusicVolume(255);
                    Mars_PlayTrack(0, 1, newpath, -1, 0, 0);
                    return ga_nothing;
                }
#endif
                if (len > 4 && !D_strcasecmp(mi->name + len - 4, ".wad"))
                {
                    if (!D_strcasecmp(mi->name, SOUNDS_PWAD_NAME))
                        return ga_nothing;
                    if (!D_strcasecmp(mi->name, MAPDEV_PWAD_NAME))
                        return ga_nothing;
                    return ga_startnew;
                }

                if (len > 4 && !D_strcasecmp(mi->name + len - 4, ".zgm"))
                {
#ifdef MARS
                    Mars_StopTrack();

                    Mars_UpdateCD();

                    D_snprintf(newpath, sizeof(newpath), "%s/%s", gs_menu->path, mi->name);
                    Mars_PlayTrack(0, 1, newpath, 0, mi->size, 1);
#endif
                    return ga_nothing;
                }
            }

            return ga_nothing;
        }

        return ga_startnew;
    }

    if ((buttons & (BT_B | BT_RMBTN)) && !(oldbuttons & (BT_B | BT_RMBTN)))
    {
        if (gs_menu->mode)
        {
            if (D_strcasecmp(gs_menu->path, "/"))
                GS_PathChange("..", 1);
        }
        return ga_nothing;
    }

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

            if (buttons & BT_LEFT)
            {
                if (gs_menu->mode)
                    gs_menu->cursorpos -= SMALL_PAGEITMS;
                if (gs_menu->cursorpos < 0)
                    gs_menu->cursorpos = 0;
            }

            if (buttons & BT_RIGHT)
            {
                if (gs_menu->mode)
                    gs_menu->cursorpos += SMALL_PAGEITMS;
                if (gs_menu->cursorpos >= gs_menu->numitems)
                    gs_menu->cursorpos = gs_menu->numitems-1;
            }

            newcursor = gs_menu->cursorpos != oldcursorpos;

            if (newcursor)
                sound = sfx_pistol;
        }
    }

    if (sound != sfx_None && !gs_menu->mode)
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
    int i, y = 0;
    menuitem_t *items = gs_menu->items;
    int y_offset = STARTY;

    I_ClearFrameBuffer();

    if (gs_menu->mode)
    {
        // file browser
        int oldpage = -1;

        /* draw menu items */
        for (i = 0; i < gs_menu->numitems; i++)
        {
            int page = i >= SMALL_PAGEITMS ? 1 : 0;
            int x = SMALL_STARTX + page * SMALL_PAGEWDTH;
            char name[72];

            if (page != oldpage)
            {
                oldpage = page;
                y = 2;
            }

            if (gs_menu->cursorpos == i)
            {
                I_Print8(x-10, y, ">");
            }

            if (items[i].y)
            {
                // a directory
                D_snprintf(name, sizeof(name), "^%02X%s", 161, items[i].name);
            }
            else
            {
                // a file
                D_snprintf(name, sizeof(name), "%s", items[i].name);
            }

            // truncate if too long
            if (mystrlen(name) > 17 && gs_menu->numitems > SMALL_PAGEITMS)
            {
                D_memcpy(&name[14], "...", 4);
            }

            I_Print8(x, y, name);

            y++;
        }
    }
    else
    {
        /* Draw main menu */
        print(84, y_offset, "SELECT GAME");

        /* draw new skull */
        if (gs_menu->cursorframe)
            DrawJagobjLump(gs_menu->m_skull2lump, CURSORX, y_offset + items[gs_menu->cursorpos].y - 2, NULL, NULL);
        else
            DrawJagobjLump(gs_menu->m_skull1lump, CURSORX, y_offset + items[gs_menu->cursorpos].y - 2, NULL, NULL);

        /* draw menu items */
        for (i = 0; i < gs_menu->numitems; i++)
        {
            int len;
            char name[72];

            y = y_offset + items[i].y;
            len = mystrlen(items[i].name);
            if (len <= 4)
                continue;

            D_memcpy(name, items[i].name, len-4);
            name[len-4] = 0; // strip file extension

            print(items[i].x, y, name);
        }
    }

    I_Print8(14, 24, "Press A to select an item");
    if (gs_menu->mode && gs_menu->path[0] != '\0')
        I_Print8(14, 25, "Press B to return to parent directory");
    I_Print8(14, 26, "Press C to toggle the file browser");
}
