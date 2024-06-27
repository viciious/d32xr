#include "doomdef.h"

typedef struct
{
    short lumpStart;
    char lumpStartChar;
    boolean fixedWidth;
    int8_t fixedWidthSize;
    char minChar;
    char maxChar;
} font_t;

extern font_t menuFont;
extern font_t titleFont;
extern font_t creditFont;
extern font_t titleNumberFont;
extern font_t hudNumberFont;

void V_FontInit();
int V_DrawStringLeft(const font_t *font, int x, int y, const char *string);
int V_DrawStringRight(const font_t *font, int x, int y, const char *string);
int V_DrawStringCenter(const font_t *font, int x, int y, const char *string);
int V_DrawValuePaddedRight(const font_t *font, int x, int y, int value, int pad);

void valtostr(char *string,int val);
