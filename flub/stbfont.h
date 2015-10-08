#ifndef _FLUB_STB_FONT_HEADER_
#define _FLUB_STB_FONT_HEADER_


#define FLUB_STB_BIN_FONT_VERSION       1
#define FLUB_STB_BIN_FONT_MAX_NAMELEN   64


#ifndef STB_FONTCHAR__TYPEDEF
#define STB_FONTCHAR__TYPEDEF
typedef struct
{
    // coordinates if using integer positioning
    float s0,t0,s1,t1;
    signed short x0,y0,x1,y1;
    int   advance_int;
    // coordinates if using floating positioning
    float s0f,t0f,s1f,t1f;
    float x0f,y0f,x1f,y1f;
    float advance;
} stb_fontchar;
#endif // STB_FONTCHAR__TYPEDEF


typedef struct flubStbFont_s flubStbFont_t;
struct flubStbFont_s {
    // Binary font file begins with 8 bytes: "STBFONT\0"
    int version;

    int namelen;
    char *name;
    int size;
    int bold;
    int mono;

    int bmpWidth;
    int bmpHeight;
    int bmpHeightPow2;
    int firstChar;
    int numChars;
    int lineSpacing;

    unsigned char *data; // <-- NOT part of the binary file format

    //int pixelBufLength;
    // Uint32 *pixels;

    stb_fontchar *fontData;

    // numChars entries for font metrics:
    // Sint16 x;
    // Sint16 y;
    // Uint16 w;
    // Uint16 h;
    // Uint16 s;
    // Uint16 t;
    // Uint16 a;
};


#endif // _FLUB_STB_FONT_HEADER_
