#include <flub/util/color.h>
#include <flub/util/parse.h>
#ifdef MACOSX
#   include <OpenGL/gl.h>
#else // MACOSX
#   include <gl/gl.h>
#endif // MACOSX


typedef struct _utilCtx_s {
    int inited;
    flubColor3f_t colorTable[256];
} utilCtx_t;

utilCtx_t g_utilCtx = {
    .inited = 0,
};


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

void utilColor3fTo3i(flubColor3f_t *fColor, flubColor3i_t *iColor) {
    iColor->red = COLOR_FTOI(fColor->red);
    iColor->green = COLOR_FTOI(fColor->green);
    iColor->blue = COLOR_FTOI(fColor->blue);
}

void utilColor3iTo3f(flubColor3i_t *iColor, flubColor3f_t *fColor) {
    fColor->red = COLOR_ITOF(iColor->red);
    fColor->green = COLOR_ITOF(iColor->green);
    fColor->blue = COLOR_ITOF(iColor->blue);
}

void utilGetQCColorCode(char c, float *red, float *green, float *blue) {
    if(!g_utilCtx.inited) {
        _utilQCColorCodeTableInit();
    }

    *red = g_utilCtx.colorTable[c].red;
    *green = g_utilCtx.colorTable[c].green;
    *blue = g_utilCtx.colorTable[c].blue;
}

void utilGetQCColorCodeStruct3f(char c, flubColor3f_t *color) {
    if(!g_utilCtx.inited) {
        _utilQCColorCodeTableInit();
    }

    color->red = g_utilCtx.colorTable[c].red;
    color->green = g_utilCtx.colorTable[c].green;
    color->blue = g_utilCtx.colorTable[c].blue;
}

void utilGetQCColorCodeStruct3i(char c, flubColor3f_t *color) {
    if(!g_utilCtx.inited) {
        _utilQCColorCodeTableInit();
    }

    color->red = COLOR_FTOI(g_utilCtx.colorTable[c].red);
    color->green = COLOR_FTOI(g_utilCtx.colorTable[c].green);
    color->blue = COLOR_FTOI(g_utilCtx.colorTable[c].blue);
}

void utilSetQCColorCode(char c) {
    if(!g_utilCtx.inited) {
        _utilQCColorCodeTableInit();
    }

    glColor3f(g_utilCtx.colorTable[c].red,
              g_utilCtx.colorTable[c].green,
              g_utilCtx.colorTable[c].blue);
}
