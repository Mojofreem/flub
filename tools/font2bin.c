//////////////////////////////////////////////////////////////////////////////
//
// FLUB_STB_BASE
// FONT2BIN_STB_BIN_FILE
// FLUB_STB_BIN_FONT_VERSION
// FONT2BIN_FONT_NAME
// FONT2BIN_FONT_SIZE
// FONT2BIN_FONT_BOLD
//
// To wrap the cmake custom command to build this:
//     http://www.cmake.org/cmake/help/v3.0/command/function.html
//
//////////////////////////////////////////////////////////////////////////////
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <flub/stbfont.h>

#include FONT2BIN_STB_INL_FILE
//#include <flub/font/stb_font_consolas_12_usascii.inl>


#define STB_NAME2(x,s)      stb__ ## x ## s
#define STB_NAME(x,s)       STB_NAME2(x, s)
#define STRINGIFY2( x )     #x
#define STRINGIFY( x )      STRINGIFY2( x )


int writeFontHeader(flubStbFont_t *font, FILE *fp) {
    int pos;

    fwrite("STBFONT", 8, 1, fp);
    fwrite(&font->version, sizeof(int), 1, fp);

    fwrite(&font->namelen, sizeof(int), 1, fp);
    fwrite(font->name, font->namelen, 1, fp);
    fwrite(&font->size, sizeof(int), 1, fp);
    fwrite(&font->bold, sizeof(int), 1, fp);
    pos = ftell(fp);
    fwrite(&font->mono, sizeof(int), 1, fp);

    fwrite(&font->bmpWidth, sizeof(int), 1, fp);
    fwrite(&font->bmpHeight, sizeof(int), 1, fp);
    fwrite(&font->bmpHeightPow2, sizeof(int), 1, fp);
    fwrite(&font->firstChar, sizeof(int), 1, fp);
    fwrite(&font->numChars, sizeof(int), 1, fp);
    fwrite(&font->lineSpacing, sizeof(int), 1, fp);

    return pos;
}

void writeFontPixels(int length, FILE *fp) {
    unsigned int *pixels = STB_NAME(FONT2BIN_BASE, _pixels);
    int k;

    fwrite(&length, sizeof(int), 1, fp);

    for(k = 0; k < length; k++) {
        fwrite(pixels + k, sizeof(unsigned int), 1, fp);
    }
}

int writeFontMetrics(FILE *fp) {
    int k;
    int mono = 0;
    int advance;

    signed short *font_x = STB_NAME(FONT2BIN_BASE, _x);
    signed short *font_y = STB_NAME(FONT2BIN_BASE, _y);
    unsigned short *font_w = STB_NAME(FONT2BIN_BASE, _w);
    unsigned short *font_h = STB_NAME(FONT2BIN_BASE, _h);
    unsigned short *font_s = STB_NAME(FONT2BIN_BASE, _s);
    unsigned short *font_t = STB_NAME(FONT2BIN_BASE, _t);
    unsigned short *font_z = STB_NAME(FONT2BIN_BASE, _a);

    for(k = 0; k < STB_SOMEFONT_NUM_CHARS; k++) {
        fwrite(font_x + k, sizeof(signed short), 1, fp);
        fwrite(font_y + k, sizeof(signed short), 1, fp);
        fwrite(font_w + k, sizeof(unsigned short), 1, fp);
        fwrite(font_h + k, sizeof(unsigned short), 1, fp);
        fwrite(font_s + k, sizeof(unsigned short), 1, fp);
        fwrite(font_t + k, sizeof(unsigned short), 1, fp);
        fwrite(font_z + k, sizeof(unsigned short), 1, fp);
#if 0
        printf("[%d] x:%d  y:%d  w:%d  h:%d  s:%d  t:%d  a:%d\n", k,
               font_x[k], font_y[k], font_w[k], font_h[k],
               font_s[k], font_t[k], font_z[k]);
#endif
        advance = (font_z[k] + 8) >> 4;
        if(mono < advance) {
            mono = advance;
        }
    }
    return mono;
}

void writeMonoWidth(int pos, int width, FILE *fp) {
    fseek(fp, pos, SEEK_SET);
    fwrite(&width, sizeof(int), 1, fp);
    fseek(fp, SEEK_END, 0);
}

int calculatePixelBufLength(void) {
    int i;
    int j;
    int length = 0;

    unsigned int *bits = STB_NAME(FONT2BIN_BASE, _pixels);
    unsigned int bitpack = *bits++;
    length++;
    unsigned int numbits = 32;
    for(j=1; j < STB_SOMEFONT_BITMAP_HEIGHT-1; ++j) {
        for(i=1; i < STB_SOMEFONT_BITMAP_WIDTH-1; ++i) {
            unsigned int value;
            if(numbits==0) {
                bitpack = *bits++, numbits=32;
                length++;
            }
            value = bitpack & 1;
            bitpack >>= 1, --numbits;
            if(value) {
                if(numbits < 3) {
                    bitpack = *bits++, numbits = 32;
                    length++;
                }
                bitpack >>= 3, numbits -= 3;
            }
        }
    }
    return length;
}

int main(int argc, char *argv[]) {
    flubStbFont_t font;
    FILE *fp;
    int pixelBufLength;
    int mono;
    int monoIndex;

    if((fp = fopen(STRINGIFY(FONT2BIN_STB_BIN_FILE), "wb")) == NULL) {
        fprintf(stderr, "ERROR: Failed to open output file \"%s\".\n", STRINGIFY(FONT2BIN_STB_BIN_FILE));
        return 1;
    }

    printf("INFO: Generating [%s]...\n", STRINGIFY(FONT2BIN_STB_BIN_FILE));

    memset(&font, 0, sizeof(flubStbFont_t));
    font.version = FLUB_STB_BIN_FONT_VERSION;
    font.name = STRINGIFY(FONT2BIN_FONT_NAME);
    font.namelen = strlen(font.name) + 1;
    font.size = FONT2BIN_FONT_SIZE;
    font.bold = FONT2BIN_FONT_BOLD;
    font.bmpWidth = STB_SOMEFONT_BITMAP_WIDTH;
    font.bmpHeight = STB_SOMEFONT_BITMAP_HEIGHT;
    font.bmpHeightPow2 = STB_SOMEFONT_BITMAP_HEIGHT_POW2;
    font.firstChar = STB_SOMEFONT_FIRST_CHAR;
    font.numChars = STB_SOMEFONT_NUM_CHARS;
    font.lineSpacing = STB_SOMEFONT_LINE_SPACING;

    pixelBufLength = calculatePixelBufLength();

    monoIndex = writeFontHeader(&font, fp);
    writeFontPixels(pixelBufLength, fp);
    mono = writeFontMetrics(fp);
    if(FONT2BIN_FONT_MONO) {
        writeMonoWidth(monoIndex, mono, fp);
    }

    fclose(fp);
    printf("INFO: Generated binary STB font file for %s.\n", STRINGIFY(FONT2BIN_BASE));

    return 0;
}

