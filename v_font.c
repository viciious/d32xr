#include "v_font.h"

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

    titleFont.lumpStart = W_GetNumForName("LTFNT065");
    titleFont.lumpStartChar = 65;
    titleFont.minChar = 65;
    titleFont.maxChar = 122;
    titleFont.fixedWidth = false;
    titleFont.fixedWidth = 16;

    creditFont.lumpStart = W_GetNumForName("CRFNT065");
    creditFont.lumpStartChar = 65;
    creditFont.minChar = 46;
    creditFont.maxChar = 90;
    creditFont.fixedWidth = true;
    creditFont.fixedWidthSize = 16;

    titleNumberFont.lumpStart = W_GetNumForName("TTL01");
    titleNumberFont.lumpStartChar = '1';
    titleNumberFont.minChar = '1';
    titleNumberFont.maxChar = '3';
    titleNumberFont.fixedWidth = false;
    titleNumberFont.fixedWidthSize = 0;

    hudNumberFont.lumpStart = W_GetNumForName("STTNUM0");
    hudNumberFont.lumpStartChar = '0';
    hudNumberFont.minChar = '0';
    hudNumberFont.maxChar = '9';
    hudNumberFont.fixedWidth = true;
    hudNumberFont.fixedWidthSize = 8;
}

int V_DrawStringLeft(const font_t *font, int x, int y, const char *string)
{
	int i,c;
	int w;

	for (i = 0; i < mystrlen(string); i++)
	{
		c = string[i];
	
        if (c == 0x20) // Space
            x += font->fixedWidth ? font->fixedWidthSize / 2 : w;
		else if (c >= font->minChar && c <= font->maxChar)
		{
			DrawJagobjLump(font->lumpStart + (c - font->lumpStartChar), x, y, &w, NULL);
			x += font->fixedWidth ? font->fixedWidthSize : w;
		}
	}

    return x;
}

int V_DrawStringRight(const font_t *font, int x, int y, const char *string)
{
    int i,c;
	int w;

	for (i = mystrlen(string)-1; i >= 0; i--)
	{
		c = string[i];
	
        if (c == 0x20) // Space
            x -= font->fixedWidth ? font->fixedWidthSize / 2 : w;
		else if (c >= font->minChar && c <= font->maxChar)
		{
			DrawJagobjLump(font->lumpStart + (c - font->lumpStartChar), x, y, &w, NULL);
			x -= font->fixedWidth ? font->fixedWidthSize : w;
		}
	}

    return x;
}

int V_DrawStringCenter(const font_t *font, int x, int y, const char *string)
{
    int c;
	int w;

    // Slow, difficult...
    for (int i = 0; i < mystrlen(string); i++)
    {
        c = string[i];
        
        if (c == 0x20) // Space
            x -= (font->fixedWidth ? font->fixedWidthSize / 2 : w) / 2;
        else if (c >= font->minChar && c <= font->maxChar)
        {
            if (font->fixedWidth)
                x -= font->fixedWidth / 2;
            else
            {
                DrawJagobjLump(font->lumpStart + (c - font->lumpStartChar), x, y, &w, NULL);
                x -= w / 2;
            }
        }
    }

    return V_DrawStringLeft(font, x, y, string);
}

// Font MUST be fixedWidth = true
int V_DrawValuePaddedRight(const font_t *font, int x, int y, int value, int pad)
{
	char	v[12];
	int		i;

	valtostr(v,value);

	for (i = mystrlen(v)-1; i >= 0; i--)
	{
		int c = v[i];

        x -= font->fixedWidthSize;
	
        if (c == 0x20) // Space
            x -= font->fixedWidthSize / 2;
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

    return x;
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