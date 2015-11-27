#include <flub/util/color.h>
#include <flub/util/parse.h>
#ifdef MACOSX
#   include <OpenGL/gl.h>
#else // MACOSX
#   include <gl/gl.h>
#endif // MACOSX
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <flub/logger.h>


typedef struct colorName_s {
    const char *name;
    char c;
    unsigned int value;
} colorName_t;

typedef struct _utilCtx_s {
    int inited;
    flubColor4f_t colorTable[256];
} utilCtx_t;

utilCtx_t g_utilCtx = {
    .inited = 0,
};

colorName_t _colorTableTbl[] = {
        "black",        '0',    0x000000, //Black
        "red",          '1',    0xDA0120, //Red
        "green",        '2',    0x00B906, //Green
        "yellow",       '3',    0xE8FF19, //Yellow
        "blue",         '4',    0x170BDB, //Blue
        "cyan",         '5',    0x23C2C6, //Cyan
        "pink",         '6',    0xE201DB, //Pink
        "white",        '7',    0xFFFFFF, //White
        "orange",       '8',    0xCA7C27, //Orange
        "gray",         '9',    0x757575, //Gray
        "orange",       'a',    0xEB9F53, //Orange
        "turquoise",    'b',    0x106F59, //Turquoise
        "purple",       'c',    0x5A134F, //Purple
        "lightblue",    'd',    0x035AFF, //Light Blue
        "purple",       'e',    0x681EA7, //Purple
        "lightblue",    'f',    0x5097C1, //Light Blue
        "lightgreen",   'g',    0xBEDAC4, //Light Green
        "darkgreen",    'h',    0x024D2C, //Dark Green
        "darkred",      'i',    0x7D081B, //Dark Red
        "claret",       'j',    0x90243E, //Claret
        "brown",        'k',    0x743313, //Brown
        "lightbrown",   'l',    0xA7905E, //Light Brown
        "olive",        'm',    0x555C26, //Olive
        "beige",        'n',    0xAEAC97, //Beige
        "beige",        'o',    0xC0BF7F, //Beige
        "black",        'p',    0x000000, //Black
        "red",          'q',    0xDA0120, //Red
        "green",        'r',    0x00B906, //Green
        "yellow",       's',    0xE8FF19, //Yellow
        "blue",         't',    0x170BDB, //Blue
        "cyan",         'u',    0x23C2C6, //Cyan
        "pink",         'v',    0xE201DB, //Pink
        "white",        'w',    0xFFFFFF, //White
        "orange",       'x',    0xCA7C27, //Orange
        "gray",         'y',    0x757575, //Gray
        "orange",       'z',    0xCC8034, //Orange
        "beige",        '/',    0xDBDF70, //Beige
        "gray",         '*',    0xBBBBBB, //Gray
        "olive",        '-',    0x747228, //Olive
        "foxyred",      '+',    0x993400, //Foxy Red
        "darkbrown",    '?',    0x670504, //Dark Brown
        "brown",        '@',    0x623307, //Brown
        "magenta",      '\0',   0xFF00FF, //Magenta
        NULL,           '\0',   0x000000  // End of list
};

colorName_t *g_colorTable = _colorTableTbl;


void _utilQCColorCodeTableInit(void) {
    int k, red, green, blue, n;
    unsigned int value;
    char c;

    for(k = 0; k < 256; k++) {
        g_utilCtx.colorTable[k].red = 1.0;
        g_utilCtx.colorTable[k].green = 1.0;
        g_utilCtx.colorTable[k].blue = 1.0;
    }

    for(k = 0; g_colorTable[k].c != '\0'; k++) {
        c = g_colorTable[k].c;
        value = g_colorTable[k].value;

        red = (value & 0xFF0000) >> 16;
        green = (value & 0xFF00) >> 8;
        blue = (value & 0xFF);

        g_utilCtx.colorTable[c].red = ((float)red) / 255.0;
        g_utilCtx.colorTable[c].green = ((float)green) / 255.0;
        g_utilCtx.colorTable[c].blue = ((float)blue) / 255.0;
    }

    g_utilCtx.inited = 1;
}

void flubColorCopy(flubColor4f_t *colorA, flubColor4f_t *colorB) {
    colorB->red = colorA->red;
    colorB->green = colorA->green;
    colorB->blue = colorA->blue;
    colorB->alpha = colorA->alpha;
}

void flubColorQCCodeGet(char c, flubColor4f_t *color) {
    if(!g_utilCtx.inited) {
        _utilQCColorCodeTableInit();
    }

    color->red = g_utilCtx.colorTable[c].red;
    color->green = g_utilCtx.colorTable[c].green;
    color->blue = g_utilCtx.colorTable[c].blue;
    color->alpha = 1.0;
}

// ^\s*(0x|#)?(?P<simple>[0-9A-Fa-f]{3})(?P<extended>[0-9A-Fa-f]{3})?\s*$
// ^\s*(?P<first>[0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\s*,\s*(?P<second>[0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\s*,\s*(?P<third>[0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\s*$

// (0x|#)?[0-9A-Fa-f]{3}[0-9A-Fa-f]{3} |
// ((1?[1-9]\d?)|0|100|(2([0-4]\d|5[0-5])))(,((1?[1-9]\d?)|0|100|(2([0-4]\d|5[0-5]))){2,2}) |
// black | darkblue | darkgreen | cyan | darkred | magenta | brown | gray |
// brightgray | darkgray | blue | brightblue | green | brightgreen |
// brightcyan | red | brightred | pink | brightmagenta | yellow | white |
// orange
int flubColorParse(const char *str, flubColor4f_t *color) {
    const char *orig = str;
    unsigned int value;

    int offset;
    int checkHex;
    int pos;
    int nibbles;

    int decnum;
    int hexnum;
    int k;
    int mask;
    int isHex;
    int isTriplet;

    int digit;
    int dummy;
    int red, green, blue, alpha;
    int result = 0;
    int *ref[4];

    // Preset component variables with defaults, and redirect NULLs to a
    // dummy local var, so we don't need to keep checking for NULL later
    ref[0] = &red;
    ref[1] = &green;
    ref[2] = &blue;
    ref[3] = &alpha;

    // Color defaults to black
    for(digit = 0; digit < 3; digit++) {
        *(ref[digit]) = 0;
    }
    // Alpha defaults to full opacity
    *(ref[3]) = 1;

    // Skip preceeding whitespace
    for(; isspace(*str); str++);

    // Is this a hex value?
    if(parseHexGroup(str, &value, &digit)) {
        switch(digit) {
            case 3:
                // RGB
                (*(ref[0])) = ((value >> 8) & 0xF) * 16;
                (*(ref[1])) = ((value >> 4) & 0xF) * 16;
                (*(ref[2])) = (value & 0xF) * 16;
                (*(ref[3])) = 255;
                goto done;
            case 4:
                // RGBA
                (*(ref[0])) = ((value >> 12) & 0xF) * 16;
                (*(ref[1])) = ((value >> 8) & 0xF) * 16;
                (*(ref[2])) = ((value >> 4) & 0xF) * 16;
                (*(ref[3])) = (value & 0xF) * 16;
                goto done;
            case 6:
                // RRGGBB
                (*(ref[0])) = (value >> 16) & 0xFF;
                (*(ref[1])) = (value >> 8) & 0xFF;
                (*(ref[2])) = value & 0xFF;
                (*(ref[3])) = 255;
                goto done;
            case 8:
                // RRGGBBAA
                (*(ref[0])) = (value >> 24) & 0xFF;
                (*(ref[1])) = (value >> 16) & 0xFF;
                (*(ref[2])) = (value >> 8) & 0xFF;
                (*(ref[3])) = value & 0xFF;
                goto done;
            default:
                break;
        }
    }

    // Is this a color triple (or quadruple) value?
    if(parseTripleGroup(str, &value, &digit)) {
        switch(digit) {
            case 3:
                (*(ref[0])) = (value >> 16) & 0xFF;
                (*(ref[1])) = (value >> 8) & 0xFF;
                (*(ref[2])) = value & 0xFF;
                (*(ref[3])) = 255;
                goto done;
            case 4:
                (*(ref[0])) = (value >> 24) & 0xFF;
                (*(ref[1])) = (value >> 16) & 0xFF;
                (*(ref[2])) = (value >> 8) & 0xFF;
                (*(ref[3])) = value & 0xFF;
                goto done;
            default:
                break;
        }
    }

    for(pos = 0; ((!isspace(str[pos])) && (str[pos] != '\0')); pos++);

    // Is this a text color name
    for(k = 0; g_colorTable[k].name != NULL; k++) {
        if(!strncasecmp(g_colorTable[k].name, str, pos)) {
            mask = 0xFF0000;
            for(digit = 0, pos = 16; digit < 3; digit++, pos -= 8) {
                *(ref[digit]) = (g_colorTable[k].value & mask) >> pos;
                mask >>= 8;
            }
            goto done;
        }
    }
    errorf("flubColorParse(): unknown color name \"%s\"", str);
    return 0;

done:
    color->red = red;
    color->green = green;
    color->blue = blue;
    color->alpha = alpha;

    return 1;
}

