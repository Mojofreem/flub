#ifndef _FLUB_FONT_HEADER_
#define _FLUB_FONT_HEADER_

#ifdef MACOSX
#   include <OpenGL/gl.h>
#else // MACOSX
#   include <gl/gl.h>
#endif // MACOSX
#include <flub/font_struct.h>
#include <flub/gfx.h>
#include <flub/app.h>


typedef struct fontColor_s {
    float red;
    float green;
    float blue;
} fontColor_t;


int flubFontInit(appDefaults_t *defaults);
int flubFontValid(void);
int flubFontStart(void);
void flubFontShutdown(void);

int flubFontLoad(const char *filename);

// Each call to fontGet increments the font's usage counter. Call fontRelease()
// to decrement the counter. When unused, fonts will not be loaded or refreshed
// when video context changes occur.
font_t *fontGet(char *fontname, int size, int bold);
font_t *fontGetByFilename(const char *filename);
void fontRelease(font_t *font);

int fontGetHeight(font_t *font);
int fontGetCharWidth(font_t *font, int c);
int fontGetNarrowestWidth(font_t *font);
int fontGetWidestWidth(font_t *font);

int fontGetStrWidth(font_t *font, const char *s);
int fontGetStrNWidth(font_t *font, const char *s, int len);
int fontGetStrLenWrap(font_t *font, const char *s, int maxwidth);
int fontGetStrLenWordWrap(font_t *font, char *s, int maxwidth, char **next,
                          int *width);
int fontGetStrLenWrapGroup(font_t *font, char *s, int maxwidth, int *lineStarts, int *lineLens, int groupLen);
int fontGetStrLenWordWrapGroup(font_t *font, char *s, int maxwidth,
       						   int *lineStarts, int *lineLens, int groupLen);
int fontGetStrLenQCWrap(font_t *font, char *s, int maxwidth, int *strlen,
                        float *red, float *green, float *blue);
int fontGetStrLenQCWordWrap(font_t *font, char *s, int maxwidth, int *strlen,
                            char **next, int *width,
                            float *red, float *green, float *blue);

void fontSetColor(float red, float green, float blue);
void fontSetColorAlpha(float red, float green, float blue, float alpha);
void fontGetColor3(fontColor_t *colors);
void fontSetColor3(fontColor_t *colors);

void fontBlitC(const font_t *font, char c);
void fontBlitStr(const font_t *font, const char *s);
void fontBlitStrN(font_t *font, const char *s, int len);
void fontBlitStrf(const font_t *font, char *fmt, ...);
void fontBlitQCStr(font_t *font, char *s);
void fontBlitInt(font_t *font, int num);
void fontBlitFloat(font_t *font, float num, int decimals);

void fontBlitCMesh(gfxMeshObj_t *mesh, font_t *font, char c);
void fontBlitStrMesh(gfxMeshObj_t *mesh, font_t *font, const char *s);
void fontBlitStrNMesh(gfxMeshObj_t *mesh, font_t *font, char *s, int len);
void fontBlitStrfMesh(gfxMeshObj_t *mesh, font_t *font, char *fmt, ...);
void fontBlitQCStrMesh(gfxMeshObj_t *mesh, font_t *font, char *s);
void fontBlitIntMesh(gfxMeshObj_t *mesh, font_t *font, int num);
void fontBlitFloatMesh(gfxMeshObj_t *mesh, font_t *font, float num, int decimals);

void fontMode(void);
void fontPos(int x, int y);

texture_t *fontTextureGet(font_t *font);

/*
 * Update the font generator tool to allow splicing fonts
 * Find/create keyboard supplemental font:
 *      windows key
 *      menu key
 *      option key (osx)
 *      command key (osx)
 *      tab arrow
 *      return arrow
 *      arrow keys
 *      caps glyph
 *      scroll lock glyph
 *      numlock glyph
 */

#endif // _FLUB_FONT_HEADER_
