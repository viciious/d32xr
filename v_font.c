#include "v_font.h"
#include "lzss.h"

font_t menuFont;
font_t titleFont;
font_t creditFont;
font_t titleNumberFont;
font_t hudNumberFont;

void V_FontInit()
{
    menuFont.lumpStart = W_GetNumForName("STCFN022");
    menuFont.lumpStartChar = 22;
    menuFont.minChar = 22;
    menuFont.maxChar = 126;
    menuFont.fixedWidth = true;
    menuFont.fixedWidthSize = 8;
    menuFont.spaceWidthSize = 4;
    menuFont.verticalOffset = 8;

    titleFont.lumpStart = W_GetNumForName("LTFNT065");
    titleFont.lumpStartChar = 65;
    titleFont.minChar = 65;
    titleFont.maxChar = 122;
    titleFont.fixedWidth = false;
    titleFont.fixedWidth = 0;
    titleFont.spaceWidthSize = 16;
    titleFont.verticalOffset = 16;

    creditFont.lumpStart = W_GetNumForName("CRFNT065");
    creditFont.lumpStartChar = 65;
    creditFont.minChar = 46;
    creditFont.maxChar = 90;
    creditFont.fixedWidth = false;
    creditFont.fixedWidthSize = 16;
    creditFont.spaceWidthSize = 8;
    creditFont.verticalOffset = 16;

    titleNumberFont.lumpStart = W_GetNumForName("TTL01");
    titleNumberFont.lumpStartChar = '1';
    titleNumberFont.minChar = '1';
    titleNumberFont.maxChar = '3';
    titleNumberFont.fixedWidth = false;
    titleNumberFont.fixedWidthSize = 0;
    titleNumberFont.verticalOffset = 29;

    hudNumberFont.lumpStart = W_GetNumForName("STTNUM0");
    hudNumberFont.lumpStartChar = '0';
    hudNumberFont.minChar = '0';
    hudNumberFont.maxChar = '9';
    hudNumberFont.fixedWidth = true;
    hudNumberFont.fixedWidthSize = 8;
    hudNumberFont.spaceWidthSize = 4;
    hudNumberFont.spaceWidthSize = 11;
}

int V_DrawStringLeftWithColormap(const font_t *font, int x, int y, const char *string, int colormap)
{
	int i,c;
    byte *lump;
    jagobj_t *jo;
    int startX = x;

	for (i = 0; i < mystrlen(string); i++)
	{
		c = string[i];
	
        if (c == '\n') // Basic newline support
        {
            x = startX;
            y += font->verticalOffset;
        }
        else if (c == 0x20) // Space
            x += font->spaceWidthSize;
		else if (c >= font->minChar && c <= font->maxChar)
		{
			if (font->fixedWidth)
            {
                if (colormap)
    			    DrawJagobjLumpWithColormap(font->lumpStart + (c - font->lumpStartChar), x, y, NULL, NULL, colormap);
                else
                    DrawJagobjLump(font->lumpStart + (c - font->lumpStartChar), x, y, NULL, NULL);
                
			    x += font->fixedWidthSize;
            }
            else
            {
                int lumpnum = font->lumpStart + (c - font->lumpStartChar);
                lump = W_POINTLUMPNUM(lumpnum);
	            if (!(lumpinfo[lumpnum].name[0] & 0x80))
	            {
    		        jo = (jagobj_t*)lump;
                    if (colormap)
                        DrawJagobjWithColormap(jo, x, y + font->verticalOffset - jo->height, 0, 0, 0, 0, I_OverwriteBuffer(), colormap);
                    else
                        DrawJagobj(jo, x, y + font->verticalOffset - jo->height);

		            x += jo->width;
	            }
            }
		}
	}

    return x;
}

int V_DrawStringRightWithColormap(const font_t *font, int x, int y, const char *string, int colormap)
{
    int i,c;
    byte *lump;
    jagobj_t *jo;

	for (i = mystrlen(string)-1; i >= 0; i--)
	{
		c = string[i];
	
        if (c == 0x20) // Space
            x -= font->spaceWidthSize;
		else if (c >= font->minChar && c <= font->maxChar)
		{
            if (font->fixedWidth)
            {
                if (colormap)
                    DrawJagobjLumpWithColormap(font->lumpStart + (c - font->lumpStartChar), x, y, NULL, NULL, colormap);
                else
    			    DrawJagobjLump(font->lumpStart + (c - font->lumpStartChar), x, y, NULL, NULL);
                
                x -= font->fixedWidthSize;
            }
            else
            {
                int lumpnum = font->lumpStart + (c - font->lumpStartChar);
                lump = W_POINTLUMPNUM(lumpnum);
	            if (!(lumpinfo[lumpnum].name[0] & 0x80))
	            {
    		        jo = (jagobj_t*)lump;
		            x -= jo->width;

                    if (colormap)
                        DrawJagobjWithColormap(jo, x, y + font->verticalOffset - jo->height, 0, 0, 0, 0, I_OverwriteBuffer(), colormap);
                    else
                        DrawJagobj(jo, x, y + font->verticalOffset - jo->height);
	            }
            }
		}
	}

    return x;
}

int V_DrawStringLeft(const font_t *font, int x, int y, const char *string)
{
	return V_DrawStringLeftWithColormap(font, x, y, string, 0);
}

int V_DrawStringRight(const font_t *font, int x, int y, const char *string)
{
    return V_DrawStringRightWithColormap(font, x, y, string, 0);
}

int V_DrawStringCenterWithColormap(const font_t *font, int x, int y, const char *string, int colormap)
{
    int c;
    byte *lump;
    jagobj_t *jo;

    // Slow, difficult...
    for (int i = 0; i < mystrlen(string); i++)
    {
        c = string[i];
        
        if (c == 0x20) // Space
            x -= font->spaceWidthSize / 2;
		else if (c >= font->minChar && c <= font->maxChar)
		{
            if (font->fixedWidth)
            {
                x -= font->fixedWidthSize / 2;
            }
            else
            {
                int lumpnum = font->lumpStart + (c - font->lumpStartChar);
                lump = W_POINTLUMPNUM(lumpnum);
	            if (!(lumpinfo[lumpnum].name[0] & 0x80))
	            {
    		        jo = (jagobj_t*)lump;
		            x -= jo->width / 2;
	            }
                else
                {
                    // Can draw compressed characters,
                    // BUT ONLY IF THEY FIT INTO THE LZSS_BUF_SIZE!
                    lzss_state_t gfx_lzss;
	                uint8_t lzss_buf[LZSS_BUF_SIZE];
                    lzss_setup(&gfx_lzss, lump, lzss_buf, LZSS_BUF_SIZE);
                    if (lzss_read(&gfx_lzss, 16) != 16)
                        continue;

                    jo = (jagobj_t*)gfx_lzss.buf;
		            x -= jo->width / 2;
                }
            }
		}
    }

    return V_DrawStringLeftWithColormap(font, x, y, string, colormap);
}

int V_DrawStringCenter(const font_t *font, int x, int y, const char *string)
{
    return V_DrawStringCenterWithColormap(font, x, y, string, 0);
}

void V_DrawValueLeft(const font_t *font, int x, int y, int value)
{
	char	v[12];

	valtostr(v,value);

    V_DrawStringLeft(font, x, y, v);
}

void V_DrawValueRight(const font_t *font, int x, int y, int value)
{
	char	v[12];

	valtostr(v,value);

    V_DrawStringRight(font, x, y, v);
}

void V_DrawValueCenter(const font_t *font, int x, int y, int value)
{
	char	v[12];

	valtostr(v,value);

    V_DrawStringCenter(font, x, y, v);
}

// Font MUST be fixedWidth = true
void V_DrawValuePaddedRight(const font_t *font, int x, int y, int value, int pad)
{
	char	v[12];
	int		i;

	valtostr(v,value);

	for (i = mystrlen(v)-1; i >= 0; i--)
	{
		int c = v[i];

        x -= font->fixedWidthSize;
	
        if (c == 0x20) // Space
            x -= font->spaceWidthSize;
		else if (c >= font->minChar && c <= font->maxChar)
			DrawJagobjLump(font->lumpStart + (c - font->lumpStartChar), x, y, NULL, NULL);

        pad--;
	}

	while (pad > 0)
	{
		x -= font->fixedWidthSize;
		DrawJagobjLump(font->lumpStart + ('0' - font->lumpStartChar), x, y, NULL, NULL);
		pad--;
	}
}

/*================================================= */
/* */
/*	Convert an int to a string (my_itoa?) */
/* */
/*================================================= */
void valtostr(char *string, int val)
{
	char temp[12];
	int	index = 0, i, dindex = 0;
	
	do
	{
		temp[index++] = val%10 + '0';
		val /= 10;
	} while(val);
	
	string[index] = 0;
	for (i = index - 1; i >= 0; i--)
		string[dindex++] = temp[i];
}