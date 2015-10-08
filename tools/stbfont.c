// Public Domain by stb

#include <stdio.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include "flub/3rdparty/stb/stb_truetype.h"
#define STB_DEFINE
#include "flub/3rdparty/stb/stb.h"
#include "flub/stbfont.h"

// determine whether to write the png image
#define WRITE_FILE

#ifdef WRITE_FILE // debugging
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "flub/3rdparty/stb/stb_image_write.h"
#endif

#define MAX_OPT_NAME_LEN    64

typedef struct
{
   unsigned char *bitmap;
   int index;
   int w,h;
   int x0,y0;
   int s,t;
   int advance;
} textchar;

textchar fontchar[65536];
unsigned int glyphmap[65536];
unsigned int glyph_to_textchar[65536];

int try_packing(int w, int h, int numchar)
{
   int i,j;
   int *front = malloc(h*sizeof(*front));
   memset(front, 0, h*sizeof(*front));
   h -= 1;
   for (i=0; i < numchar; ++i) {
      int c_height = fontchar[i].h;
      int c_width = fontchar[i].w;
      int pos, best_pos = -1, best_waste = (c_height+12) * 4096, best_left;
      for (pos = 0; pos+c_height+1 < h; ++pos) {
         int j, waste=0, leftedge=0;
         for (j=0; j < c_height+1; ++j)
            if (front[pos+j] > leftedge)
               leftedge = front[pos+j];
         if (leftedge + c_width > w-2)
            continue;
         for (j=0; j < c_height+1; ++j) {
            assert(leftedge >= front[pos+j]);
            waste += leftedge - front[pos+j];
         }
         if (waste < best_waste) {
            best_waste = waste;
            best_pos = pos;
            best_left = leftedge;
         }
      }
      if (best_pos == -1) {
         free(front);
         return 0;
      }
      // leave top row and left column empty to avoid needing border color
      fontchar[i].s = best_left+1;
      fontchar[i].t = best_pos+1;
      assert(best_pos >= 0);
      assert(best_pos+c_height <= h+1);
      for (j=0; j < c_height+1; ++j)
         front[best_pos + j] = best_left + c_width+1;
   }
   free(front);
   return 1;
}

typedef struct appOptions_s {
    const char *fontFile;
    const char *fontName;
    int fontHeight;
    int highestUnicodeCodepoint;
    const char *symbol;
    const char *generateInl;
    const char *generateBin;
    const char *generatePng;
    int mono;
    int bold;
} appOptions_t;

void cmdlineRemoveOpt(int pos, int *argc, char **argv) {
    int k;
    if(pos < *argc) {
        for(k = pos; (k + 1) < *argc; k++) {
            argv[k] = argv[k + 1];
        }
        argc--;
    }
}

int cmdlineFindOpt(const char *name, int val, int argc, char **argv) {
    char buf[MAX_OPT_NAME_LEN];
    int k;

    if(name != NULL) {
        snprintf(buf, sizeof(buf) - 1, "--%s", name);
        buf[sizeof(buf) - 1] = '\0';
        for(k = 1; k < argc; k++) {
            if(!strcmp(buf, argv[k])) {
                return k;
            }
        }
    }
    if(isalnum(val)) {
        snprintf(buf, sizeof(buf) - 1, "-%c", val);
        buf[sizeof(buf) - 1] = '\0';
        for(k = 1; k < argc; k++) {
            if(!strcmp(buf, argv[k])) {
                return k;
            }
        }
    }
    return 0;
}

int cmdlineCheckOptBool(const char *name, int val, int *argc, char **argv) {
    int pos;

    if((pos = cmdlineFindOpt(name, val, *argc, argv)) != 0) {
        cmdlineRemoveOpt(pos, argc, argv);
        return 1;
    }
    return 0;
}

const char *cmdlineCheckOptStr(const char *name, int val, const char *defValue, int *argc, char **argv) {
    int pos;
    const char *str = defValue;

    if((pos = cmdlineFindOpt(name, val, *argc, argv)) != 0) {
        if((pos + 1) < *argc) {
            if(argv[pos + 1][0] != '-') {
                str = argv[pos + 1];
                cmdlineRemoveOpt(pos + 1, argc, argv);
            }
        }
        cmdlineRemoveOpt(pos, argc, argv);
    }
    return str;
}

int cmdlineCheckOptInt(const char *name, int val, int defValue, int *argc, char **argv) {
    int pos;
    int result = defValue;

    if((pos = cmdlineFindOpt(name, val, *argc, argv)) != 0) {
        if((pos + 1) < *argc) {
            if(argv[pos + 1][0] != '-') {
                result = atoi(argv[pos + 1]);
                cmdlineRemoveOpt(pos + 1, argc, argv);
            }
        }
        cmdlineRemoveOpt(pos, argc, argv);
    }
    return result;
}

void printUsage(void) {
    printf("stbfont <options>\n\n");
    printf("  --font <FILE>, -f <FILE>    specify the TTF file\n");
    printf("  --name <NAME>, -n <NAME>    specify the font name\n");
    printf("  --height <SIZE>, -h <SIZE>\n");
    printf("  --max-codepoint <NUM>, -c <NUM>\n");
    printf("  --symbol <NAME>, -s <NAME>\n");
    printf("  --inl <FILE>, -i <FILE>\n");
    printf("  --bin <FILE>, -i <FILE>\n");
    printf("  --png <FILE>, -p <FILE>\n");
    printf("  --mono, -m\n");
    printf("  --bold, -b\n");
    printf("  --help, -?\n\n");
}

int parseCmdLineArgs(int argc, char **argv, appOptions_t *opts) {
    if(cmdlineCheckOptBool("help", '?', &argc, argv)) {
        printUsage();
        return 0;
    }
    opts->fontFile = cmdlineCheckOptStr("font", 'f', NULL, &argc, argv);
    opts->fontName = cmdlineCheckOptStr("name", 'n', NULL, &argc, argv);
    opts->fontHeight = cmdlineCheckOptInt("height", 'h', -1, &argc, argv);
    opts->highestUnicodeCodepoint = cmdlineCheckOptInt("max-codepoint", 'c', 127, &argc, argv);
    opts->symbol = cmdlineCheckOptStr("symbol", 's', NULL, &argc, argv);
    opts->generateInl = cmdlineCheckOptStr("inl", 'i', NULL, &argc, argv);
    opts->generateBin = cmdlineCheckOptStr("bin", 'b', NULL, &argc, argv);
    opts->generatePng = cmdlineCheckOptStr("png", 'p', NULL, &argc, argv);
    opts->mono = cmdlineCheckOptBool("mono", 'm', &argc, argv);
    opts->bold = cmdlineCheckOptBool("bold", 'b', &argc, argv);

    if(opts->fontFile == NULL) {
        fprintf(stderr, "ERROR: No font file was specified.\n");
        return 0;
    }
    if((opts->fontHeight < 2) || (opts->fontHeight > 200)) {
        if(opts->fontHeight == -1) {
            fprintf(stderr, "ERROR: Font height was not specified.\n");
        } else {
            fprintf(stderr, "ERROR: Font height is not valid.\n");
        }
        return 0;
    }
    if((opts->generateInl == NULL) && (opts->generateBin == NULL) && (opts->generatePng == NULL)) {
        fprintf(stderr, "ERROR: No output specified.\n");
        return 0;
    }
    return 1;
}

void writeFontHeader(FILE *fp, int version, const char *name, int size,
                     int bold, int mono, int width, int height, int heightPow2,
                     int firstChar, int numChars, int lineSpacing) {
    int namelen;

    fwrite("STBFONT", 8, 1, fp);
    fwrite(&version, sizeof(int), 1, fp);

    namelen = strlen(name) + 1;
    fwrite(&namelen, sizeof(int), 1, fp);
    fwrite(name, namelen, 1, fp);
    fwrite(&size, sizeof(int), 1, fp);
    fwrite(&bold, sizeof(int), 1, fp);
    fwrite(&mono, sizeof(int), 1, fp);

    fwrite(&width, sizeof(int), 1, fp);
    fwrite(&height, sizeof(int), 1, fp);
    fwrite(&heightPow2, sizeof(int), 1, fp);
    fwrite(&firstChar, sizeof(int), 1, fp);
    fwrite(&numChars, sizeof(int), 1, fp);
    fwrite(&lineSpacing, sizeof(int), 1, fp);
}

void writeFontPixels(FILE *fp, unsigned int *pixels, int length) {
    int k;

    fwrite(&length, sizeof(int), 1, fp);

    for(k = 0; k < length; k++) {
        fwrite(pixels + k, sizeof(unsigned int), 1, fp);
    }
}

void writeFontMetric(FILE *fp, signed short x, signed short y,
                     unsigned short w, unsigned short h,
                     unsigned short s, unsigned short t,
                     unsigned short a) {
    fwrite(&x, sizeof(signed short), 1, fp);
    fwrite(&y, sizeof(signed short), 1, fp);
    fwrite(&w, sizeof(unsigned short), 1, fp);
    fwrite(&h, sizeof(unsigned short), 1, fp);
    fwrite(&s, sizeof(unsigned short), 1, fp);
    fwrite(&t, sizeof(unsigned short), 1, fp);
    fwrite(&a, sizeof(unsigned short), 1, fp);
}

#define INL_OUT(p,fmt,...)  do{ if((p) != NULL) { fprintf((p), (fmt), ## __VA_ARGS__); } }while(0)

int main(int argc, char **argv)
{
   unsigned char *packed;
   int zero_compress = 0;
   int area,spaces,words=0;
   int i,j;
   int packed_width, packed_height, packing_step, numbits,bitpack;
   float height, scale;
   unsigned char *data;
   const char *name = "stbfont";
   stbtt_fontinfo font;
   int numchars;
   int ascent, descent, linegap, spacing;
   int numglyphs=0;
   appOptions_t opts;
   int adv;
   unsigned int *pixels = NULL;
   FILE *inl = NULL, *bin = NULL;

   if(!parseCmdLineArgs(argc, argv, &opts)) {
       return 1;
   }

   data = stb_file((char *)opts.fontFile, NULL);
   if (data == NULL) { fprintf(stderr, "Couldn't open '%s'\n", opts.fontFile); return 1; }
   height = (float) opts.fontHeight;
   if (height < 1 || height > 600) { fprintf(stderr, "Font-height must be between 1 and 200\n"); return 1; }
   numchars = opts.highestUnicodeCodepoint - 32 + 1;
   if (numchars < 0 || numchars > 66000) { fprintf(stderr, "Highest codepoint must be between 33 and 65535\n"); return 1; }
   name = opts.symbol;

   if(opts.generateInl != NULL) {
       if((inl = fopen(opts.generateInl, "wt")) == NULL) {
           perror("Failed to open inl output file");
           fprintf(stderr, "ERROR: Failed to open the inl output file \"%s\".", opts.generateInl);
           return 1;
       }
   }

    if(opts.generateBin != NULL) {
        if((bin = fopen(opts.generateBin, "wb")) == NULL) {
            perror("Failed to open binary output file");
            fprintf(stderr, "ERROR: Failed to open the binary output file \"%s\".", opts.generateBin);
            return 1;
        }
    }

   stbtt_InitFont(&font, data, 0);
   scale = stbtt_ScaleForPixelHeight(&font, height);
   stbtt_GetFontVMetrics(&font, &ascent,&descent,&linegap);

   spacing = (int) (scale * (ascent + descent + linegap) + 0.5);
   ascent = (int) (scale * ascent);

   for (i=0; i < 65536; ++i)
      glyph_to_textchar[i] = -1;

   area = 0;
   for (i=0; i < numchars; ++i) {
      int glyph = stbtt_FindGlyphIndex(&font, 32+i);
      if (glyph_to_textchar[glyph] == -1) {
         int n = numglyphs++;
         int advance, left_bearing;
         fontchar[n].bitmap = stbtt_GetGlyphBitmap(&font, scale,scale, glyph, &fontchar[n].w, &fontchar[n].h, &fontchar[n].x0, &fontchar[n].y0);
         fontchar[n].index = n;
         stbtt_GetGlyphHMetrics(&font, glyph, &advance, &left_bearing);
         fontchar[n].advance = advance;
         glyph_to_textchar[glyph] = n;
         area += (fontchar[n].w+1) * (fontchar[n].h+1);
      }
      glyphmap[i] = glyph_to_textchar[glyph];
      // compute the total area covered by the font
   }

   #if 0 // check that all digits are same width
   for (i='1'; i <= '9'; ++i) {
      assert(fontchar[glyphmap[i-32]].advance == fontchar[glyphmap['0'-32]].advance);
   }
   #endif

   // we sort before packing, but this may actually not be ideal for the current
   // packing algorithm -- it misses the opportunity to pack in some tiny characters
   // into gaps early on... maybe it needs to choose the best character greedily instead
   // of presorted? that will be hugely inefficient to compute

   if (height < 20 && numglyphs < 128)
      packed_width = 128;
   else
      packed_width = 256;

   if (numglyphs > 256 && height > 25)
      packed_width = 512;

   packed_height = area / packed_width + 4;

   packing_step = 2;
   if (packed_height % packing_step)
      packed_height += packing_step - (packed_height % packing_step);

   // sort by height
   qsort(fontchar, numglyphs, sizeof(fontchar[0]), stb_intcmp(offsetof(textchar,h)));
   // reverse
   for (i=0; i < numglyphs/2; ++i)
      stb_swap(&fontchar[i], &fontchar[numglyphs-1 - i], sizeof(fontchar[0]));

   for(;;) {
      if (try_packing(packed_width, packed_height, numchars))
         break;
      packed_height += packing_step;
      if (packed_height > 256) {
         if (packing_step <= 16)
            ++packing_step;
      }
   }

   // sort by index so we can lookup by index
   qsort(fontchar, numglyphs, sizeof(fontchar[0]), stb_intcmp(offsetof(textchar,index)));

   packed = malloc(packed_width * packed_height);
   memset(packed, 0, packed_width * packed_height);
   for (i=0; i < numglyphs; ++i) {
      int j;
      for (j=0; j < fontchar[i].h; ++j)
         memcpy(packed + (fontchar[i].t+j)*packed_width + fontchar[i].s, fontchar[i].bitmap + j*fontchar[i].w, fontchar[i].w);
      assert(fontchar[i].s + fontchar[i].w + 1 <= packed_width);
   }

   {
      int zc_bits=0;
      int db_bits=0;
      for (i=0; i < packed_width * packed_height; ++i) {
         if (packed[i] < 16)
            zc_bits += 1;
         else
            zc_bits += 4;
         if (packed[i] < 16 || packed[i] > 240)
            db_bits += 2;
         else
            db_bits += 4;
      }
      if (zc_bits < packed_width * packed_height * 32 / 10)
         zero_compress = 1;

      // always force zero_compress on, as the other path hasn't been fully tested,
      // as in my test fonts they never mattered
      zero_compress = 1;
   }

   if (zero_compress) {
      // squeeze to 9 distinct values
      for (i=0; i < packed_width * packed_height; ++i) {
         int n = (packed[i] + (255/8)/2) / (255/8);
         packed[i] = n;
      }
   } else {
      // UNTESTED
      // squeeze to 3 bits per pixel (2 bits didn't look good enough)
      for (i=0; i < packed_width * packed_height; ++i) {
         int n = (packed[i] >> 5);
         packed[i] = n;
      }
   }

   INL_OUT(inl, "// Font generated by stb_font_inl_generator.c ");
   if (zero_compress)
       INL_OUT(inl, "(4/1 bpp)\n");
   else
       INL_OUT(inl, "(3.2 bpp)\n");
   INL_OUT(inl, "//\n");
   INL_OUT(inl, "// Following instructions show how to use the only included font, whatever it is, in\n");
   INL_OUT(inl, "// a generic way so you can replace it with any other font by changing the include.\n");
   INL_OUT(inl, "// To use multiple fonts, replace STB_SOMEFONT_* below with STB_FONT_%s_*,\n", name);
   INL_OUT(inl, "// and separately install each font. Note that the CREATE function call has a\n");
   INL_OUT(inl, "// totally different name; it's just 'stb_font_%s'.\n", name);
   INL_OUT(inl, "//\n");
   INL_OUT(inl, "/* // Example usage:\n");
   INL_OUT(inl, "\n");
   INL_OUT(inl, "static stb_fontchar fontdata[STB_SOMEFONT_NUM_CHARS];\n");
   INL_OUT(inl, "\n");
   INL_OUT(inl, "static void init(void)\n");
   INL_OUT(inl, "{\n");
   INL_OUT(inl, "    // optionally replace both STB_SOMEFONT_BITMAP_HEIGHT with STB_SOMEFONT_BITMAP_HEIGHT_POW2\n");
   INL_OUT(inl, "    static unsigned char fontpixels[STB_SOMEFONT_BITMAP_HEIGHT][STB_SOMEFONT_BITMAP_WIDTH];\n");
   INL_OUT(inl, "    STB_SOMEFONT_CREATE(fontdata, fontpixels, STB_SOMEFONT_BITMAP_HEIGHT);\n");
   INL_OUT(inl, "    ... create texture ...\n");
   INL_OUT(inl, "    // for best results rendering 1:1 pixels texels, use nearest-neighbor sampling\n");
   INL_OUT(inl, "    // if allowed to scale up, use bilerp\n");
   INL_OUT(inl, "}\n");
   INL_OUT(inl, "\n");
   INL_OUT(inl, "// This function positions characters on integer coordinates, and assumes 1:1 texels to pixels\n");
   INL_OUT(inl, "// Appropriate if nearest-neighbor sampling is used\n");
   INL_OUT(inl, "static void draw_string_integer(int x, int y, char *str) // draw with top-left point x,y\n");
   INL_OUT(inl, "{\n");
   INL_OUT(inl, "    ... use texture ...\n");
   INL_OUT(inl, "    ... turn on alpha blending and gamma-correct alpha blending ...\n");
   INL_OUT(inl, "    glBegin(GL_QUADS);\n");
   INL_OUT(inl, "    while (*str) {\n");
   INL_OUT(inl, "        int char_codepoint = *str++;\n");
   INL_OUT(inl, "        stb_fontchar *cd = &fontdata[char_codepoint - STB_SOMEFONT_FIRST_CHAR];\n");
   INL_OUT(inl, "        glTexCoord2f(cd->s0, cd->t0); glVertex2i(x + cd->x0, y + cd->y0);\n");
   INL_OUT(inl, "        glTexCoord2f(cd->s1, cd->t0); glVertex2i(x + cd->x1, y + cd->y0);\n");
   INL_OUT(inl, "        glTexCoord2f(cd->s1, cd->t1); glVertex2i(x + cd->x1, y + cd->y1);\n");
   INL_OUT(inl, "        glTexCoord2f(cd->s0, cd->t1); glVertex2i(x + cd->x0, y + cd->y1);\n");
   INL_OUT(inl, "        // if bilerping, in D3D9 you'll need a half-pixel offset here for 1:1 to behave correct\n");
   INL_OUT(inl, "        x += cd->advance_int;\n");
   INL_OUT(inl, "    }\n");
   INL_OUT(inl, "    glEnd();\n");
   INL_OUT(inl, "}\n");
   INL_OUT(inl, "\n");
   INL_OUT(inl, "// This function positions characters on float coordinates, and doesn't require 1:1 texels to pixels\n");
   INL_OUT(inl, "// Appropriate if bilinear filtering is used\n");
   INL_OUT(inl, "static void draw_string_float(float x, float y, char *str) // draw with top-left point x,y\n");
   INL_OUT(inl, "{\n");
   INL_OUT(inl, "    ... use texture ...\n");
   INL_OUT(inl, "    ... turn on alpha blending and gamma-correct alpha blending ...\n");
   INL_OUT(inl, "    glBegin(GL_QUADS);\n");
   INL_OUT(inl, "    while (*str) {\n");
   INL_OUT(inl, "        int char_codepoint = *str++;\n");
   INL_OUT(inl, "        stb_fontchar *cd = &fontdata[char_codepoint - STB_SOMEFONT_FIRST_CHAR];\n");
   INL_OUT(inl, "        glTexCoord2f(cd->s0f, cd->t0f); glVertex2f(x + cd->x0f, y + cd->y0f);\n");
   INL_OUT(inl, "        glTexCoord2f(cd->s1f, cd->t0f); glVertex2f(x + cd->x1f, y + cd->y0f);\n");
   INL_OUT(inl, "        glTexCoord2f(cd->s1f, cd->t1f); glVertex2f(x + cd->x1f, y + cd->y1f);\n");
   INL_OUT(inl, "        glTexCoord2f(cd->s0f, cd->t1f); glVertex2f(x + cd->x0f, y + cd->y1f);\n");
   INL_OUT(inl, "        // if bilerping, in D3D9 you'll need a half-pixel offset here for 1:1 to behave correct\n");
   INL_OUT(inl, "        x += cd->advance;\n");
   INL_OUT(inl, "    }\n");
   INL_OUT(inl, "    glEnd();\n");
   INL_OUT(inl, "}\n");
   INL_OUT(inl, "*/\n\n");

   INL_OUT(inl, "%s",
      "#ifndef STB_FONTCHAR__TYPEDEF\n"
      "#define STB_FONTCHAR__TYPEDEF\n"
      "typedef struct\n"
      "{\n"
      "    // coordinates if using integer positioning\n"
      "    float s0,t0,s1,t1;\n"
      "    signed short x0,y0,x1,y1;\n"
      "    int   advance_int;\n"
      "    // coordinates if using floating positioning\n"
      "    float s0f,t0f,s1f,t1f;\n"
      "    float x0f,y0f,x1f,y1f;\n"
      "    float advance;\n"
      "} stb_fontchar;\n"
      "#endif\n\n");

   INL_OUT(inl, "#define STB_FONT_%s_BITMAP_WIDTH        %4d\n", name, packed_width);
   INL_OUT(inl, "#define STB_FONT_%s_BITMAP_HEIGHT       %4d\n", name, packed_height);
   INL_OUT(inl, "#define STB_FONT_%s_BITMAP_HEIGHT_POW2  %4d\n\n", name, 1 << stb_log2_ceil(packed_height));
   INL_OUT(inl, "#define STB_FONT_%s_FIRST_CHAR            32\n", name);
   INL_OUT(inl, "#define STB_FONT_%s_NUM_CHARS           %4d\n\n", name, numchars);
   INL_OUT(inl, "#define STB_FONT_%s_LINE_SPACING        %4d\n\n", name, spacing);

   INL_OUT(inl, "static unsigned int stb__%s_pixels[]={\n", name);
   spaces = 0;
   numbits = 0;
   bitpack=0;

   if(bin != NULL) {
       pixels = malloc(packed_width * packed_height + 16);
       memset(pixels, 0, (packed_width * packed_height + 16));
   }
   if (zero_compress) {
      for (j=1; j < packed_height-1; ++j) {
         for (i=1; i < packed_width-1; ++i) {
            int v = packed[j*packed_width+i];
            if (v)
               bitpack |= (1 << numbits);
            ++numbits;
            if (numbits == 32 || (v && numbits > 29)) {
               if (spaces == 0) {
                  INL_OUT(inl, "    ");
                  spaces += 4;
               }
               INL_OUT(inl, "0x%08x,", bitpack);
               if(bin != NULL) {
                   pixels[words] = bitpack;
               }
               ++words;
               spaces += 11;
               if (spaces+11 >= 79) {
                  INL_OUT(inl, "\n");
                  spaces=0;
               }
               bitpack = 0;
               numbits=0;
            }
            if (v) {
               bitpack |= ((v-1) << numbits);
               numbits += 3;
               assert(numbits <= 32);
               if (numbits == 32) {
                  if (spaces == 0) {
                     INL_OUT(inl, "    ");
                     spaces += 4;
                  }
                  INL_OUT(inl, "0x%08x,", bitpack);
                  if(bin != NULL) {
                      pixels[words] = bitpack;
                  }
                  ++words;
                  spaces += 11;
                  if (spaces+11 >= 79) {
                     INL_OUT(inl, "\n");
                     spaces=0;
                  }
                  bitpack = 0;
                  numbits=0;
               }                  
            }
         }
      }
      if (numbits) {
         if (spaces == 0) {
            INL_OUT(inl, "    ");
            spaces += 4;
         }
         INL_OUT(inl, "0x%08x,", bitpack);
         if(bin != NULL) {
             pixels[words] = bitpack;
         }
         ++words;
      }
   } else {
      // UNTESTED
      // pack the bitmap, 10 pixels per 32-bit word
      for (j=1; j < packed_height-1; ++j) {
         for (i=1; i < packed_width-1; i += 10) {
            int k,bits=0;
            for (k=0; k < 10; ++k)
               if (i+k < packed_width) {
                  bits += packed[j*packed_width + i+k] << (k*3);
               }
            if (spaces == 0) {
               INL_OUT(inl, "    ");
               spaces += 4;
            }
            INL_OUT(inl, "0x%08x,", bits);
            if(bin != NULL) {
                pixels[words] = bitpack;
            }
            ++words;
            spaces += 11;
            if (spaces+11 >= 79) {
               INL_OUT(inl, "\n");
               spaces=0;
            }
         }
      }
   }
   
   if (spaces) INL_OUT(inl, "\n");
   INL_OUT(inl, "};\n\n");

   INL_OUT(inl, "static signed short stb__%s_x[%d]={ ", name, numchars);
   for (i=0; i < numchars; ++i) {
      INL_OUT(inl, "%d,", fontchar[glyphmap[i]].x0);
      if (i % 30 == 13)
         INL_OUT(inl, "\n");
   }
   INL_OUT(inl, " };\n");

   INL_OUT(inl, "static signed short stb__%s_y[%d]={ ", name, numchars);
   for (i=0; i < numchars; ++i) {
      INL_OUT(inl, "%d,", fontchar[glyphmap[i]].y0+ascent);
      if (i % 30 == 13)
         INL_OUT(inl, "\n");
   }
   INL_OUT(inl, " };\n");

   INL_OUT(inl, "static unsigned short stb__%s_w[%d]={ ", name, numchars);
   for (i=0; i < numchars; ++i) {
      INL_OUT(inl, "%d,", fontchar[glyphmap[i]].w);
      if (i % 30 == 13)
         INL_OUT(inl, "\n");
   }
   INL_OUT(inl, " };\n");

   INL_OUT(inl, "static unsigned short stb__%s_h[%d]={ ", name, numchars);
   for (i=0; i < numchars; ++i) {
      INL_OUT(inl, "%d,", fontchar[glyphmap[i]].h);
      if (i % 30 == 13)
         INL_OUT(inl, "\n");
   }
   INL_OUT(inl, " };\n");

   INL_OUT(inl, "static unsigned short stb__%s_s[%d]={ ", name, numchars);
   for (i=0; i < numchars; ++i) {
      INL_OUT(inl, "%d,", fontchar[glyphmap[i]].s);
      if (i % 20 == 10)
         INL_OUT(inl, "\n");
   }
   INL_OUT(inl, " };\n");

   INL_OUT(inl, "static unsigned short stb__%s_t[%d]={ ", name, numchars);
   for (i=0; i < numchars; ++i) {
      INL_OUT(inl, "%d,", fontchar[glyphmap[i]].t);
      if (i % 20 == 10)
         INL_OUT(inl, "\n");
   }
   INL_OUT(inl, " };\n");

   INL_OUT(inl, "static unsigned short stb__%s_a[%d]={ ", name,numchars);
   for (i=0; i < numchars; ++i) {
      INL_OUT(inl, "%d,", (int) (fontchar[glyphmap[i]].advance * scale * 16+0.5));
      if (i % 16 == 7)
         INL_OUT(inl, "\n");
   }
   INL_OUT(inl, " };\n\n");

   INL_OUT(inl, "// Call this function with\n");
   INL_OUT(inl, "//    font: NULL or array length\n");
   INL_OUT(inl, "//    data: NULL or specified size\n");
   INL_OUT(inl, "//    height: STB_FONT_%s_BITMAP_HEIGHT or STB_FONT_%s_BITMAP_HEIGHT_POW2\n", name,name);
   INL_OUT(inl, "//    return value: spacing between lines\n");
   INL_OUT(inl, "static void stb_font_%s(stb_fontchar font[STB_FONT_%s_NUM_CHARS],\n", name,name);
   INL_OUT(inl, "                unsigned char data[STB_FONT_%s_BITMAP_HEIGHT][STB_FONT_%s_BITMAP_WIDTH],\n", name,name);
   INL_OUT(inl, "                int height)\n");
   INL_OUT(inl, "{\n");
   INL_OUT(inl, "    int i,j;\n");

   if (zero_compress) {
      INL_OUT(inl, "    if (data != 0) {\n");
      INL_OUT(inl, "        unsigned int *bits = stb__%s_pixels;\n", name);
      INL_OUT(inl, "        unsigned int bitpack = *bits++, numbits = 32;\n");
      INL_OUT(inl, "        for (i=0; i < STB_FONT_%s_BITMAP_WIDTH*height; ++i)\n", name);
      INL_OUT(inl, "            data[0][i] = 0;  // zero entire bitmap\n");
      INL_OUT(inl, "        for (j=1; j < STB_FONT_%s_BITMAP_HEIGHT-1; ++j) {\n", name);
      INL_OUT(inl, "            for (i=1; i < STB_FONT_%s_BITMAP_WIDTH-1; ++i) {\n", name);
      INL_OUT(inl, "                unsigned int value;\n");
      INL_OUT(inl, "                if (numbits==0) bitpack = *bits++, numbits=32;\n");
      INL_OUT(inl, "                value = bitpack & 1;\n");
      INL_OUT(inl, "                bitpack >>= 1, --numbits;\n");
      INL_OUT(inl, "                if (value) {\n");
      INL_OUT(inl, "                    if (numbits < 3) bitpack = *bits++, numbits = 32;\n");
      INL_OUT(inl, "                    data[j][i] = (bitpack & 7) * 0x20 + 0x1f;\n");
      INL_OUT(inl, "                    bitpack >>= 3, numbits -= 3;\n");
      INL_OUT(inl, "                } else {\n");
      INL_OUT(inl, "                    data[j][i] = 0;\n");
      INL_OUT(inl, "                }\n");
      INL_OUT(inl, "            }\n");
      INL_OUT(inl, "        }\n");
      INL_OUT(inl, "    }\n\n");
   } else {
      // UNTESTED
      INL_OUT(inl, "    if (data != 0) {\n");
      INL_OUT(inl, "        unsigned int *bits = stb__%s_pixels;\n", name);
      INL_OUT(inl, "        unsigned int bitpack = *bits++, numbits = 30;\n");
      INL_OUT(inl, "        for (i=0; i < STB_FONT_%s_BITMAP_WIDTH*height; ++i)\n", name);
      INL_OUT(inl, "            data[0][i] = 0;  // zero entire bitmap\n");
      INL_OUT(inl, "        for (j=1; j < STB_FONT_%s_BITMAP_HEIGHT-1; ++j) {\n", name);
      INL_OUT(inl, "            numbits=0;\n");
      INL_OUT(inl, "            for (i=1; i < STB_FONT_%s_BITMAP_WIDTH-1; ++i) {\n", name);
      INL_OUT(inl, "                unsigned int value;\n");
      INL_OUT(inl, "                if (numbits==0) bitpack = *bits++, numbits=30;\n");
      INL_OUT(inl, "                value = bitpack & 7;\n");
      INL_OUT(inl, "                data[j][i] = (value<<5) + (value << 2) + (value >> 1);\n");
      INL_OUT(inl, "                bitpack >>= 3, numbits -= 3;\n");
      INL_OUT(inl, "            }\n");
      INL_OUT(inl, "        }\n");
      INL_OUT(inl, "    }\n\n");
   }

   INL_OUT(inl, "    // build font description\n");
   INL_OUT(inl, "    if (font != 0) {\n");
   INL_OUT(inl, "        float recip_width = 1.0f / STB_FONT_%s_BITMAP_WIDTH;\n", name);
   INL_OUT(inl, "        float recip_height = 1.0f / height;\n");
   INL_OUT(inl, "        for (i=0; i < STB_FONT_%s_NUM_CHARS; ++i) {\n", name);
   INL_OUT(inl, "            // pad characters so they bilerp from empty space around each character\n");
   INL_OUT(inl, "            font[i].s0 = (stb__%s_s[i]) * recip_width;\n", name);
   INL_OUT(inl, "            font[i].t0 = (stb__%s_t[i]) * recip_height;\n", name);
   INL_OUT(inl, "            font[i].s1 = (stb__%s_s[i] + stb__%s_w[i]) * recip_width;\n", name, name);
   INL_OUT(inl, "            font[i].t1 = (stb__%s_t[i] + stb__%s_h[i]) * recip_height;\n", name, name);
   INL_OUT(inl, "            font[i].x0 = stb__%s_x[i];\n", name);
   INL_OUT(inl, "            font[i].y0 = stb__%s_y[i];\n", name);
   INL_OUT(inl, "            font[i].x1 = stb__%s_x[i] + stb__%s_w[i];\n", name, name);
   INL_OUT(inl, "            font[i].y1 = stb__%s_y[i] + stb__%s_h[i];\n", name, name);
   INL_OUT(inl, "            font[i].advance_int = (stb__%s_a[i]+8)>>4;\n", name);
   INL_OUT(inl, "            font[i].s0f = (stb__%s_s[i] - 0.5f) * recip_width;\n", name);
   INL_OUT(inl, "            font[i].t0f = (stb__%s_t[i] - 0.5f) * recip_height;\n", name);
   INL_OUT(inl, "            font[i].s1f = (stb__%s_s[i] + stb__%s_w[i] + 0.5f) * recip_width;\n", name, name);
   INL_OUT(inl, "            font[i].t1f = (stb__%s_t[i] + stb__%s_h[i] + 0.5f) * recip_height;\n", name, name);
   INL_OUT(inl, "            font[i].x0f = stb__%s_x[i] - 0.5f;\n", name);
   INL_OUT(inl, "            font[i].y0f = stb__%s_y[i] - 0.5f;\n", name);
   INL_OUT(inl, "            font[i].x1f = stb__%s_x[i] + stb__%s_w[i] + 0.5f;\n", name, name);
   INL_OUT(inl, "            font[i].y1f = stb__%s_y[i] + stb__%s_h[i] + 0.5f;\n", name, name);
   INL_OUT(inl, "            font[i].advance = stb__%s_a[i]/16.0f;\n", name);
   INL_OUT(inl, "        }\n");
   INL_OUT(inl, "    }\n");
   INL_OUT(inl, "}\n\n");

   INL_OUT(inl, "#ifndef STB_SOMEFONT_CREATE\n");
   INL_OUT(inl, "#define STB_SOMEFONT_CREATE              stb_font_%s\n", name);
   INL_OUT(inl, "#define STB_SOMEFONT_BITMAP_WIDTH        STB_FONT_%s_BITMAP_WIDTH\n",name);
   INL_OUT(inl, "#define STB_SOMEFONT_BITMAP_HEIGHT       STB_FONT_%s_BITMAP_HEIGHT\n",name);
   INL_OUT(inl, "#define STB_SOMEFONT_BITMAP_HEIGHT_POW2  STB_FONT_%s_BITMAP_HEIGHT_POW2\n",name);
   INL_OUT(inl, "#define STB_SOMEFONT_FIRST_CHAR          STB_FONT_%s_FIRST_CHAR\n",name);
   INL_OUT(inl, "#define STB_SOMEFONT_NUM_CHARS           STB_FONT_%s_NUM_CHARS\n",name);
   INL_OUT(inl, "#define STB_SOMEFONT_LINE_SPACING        STB_FONT_%s_LINE_SPACING\n", name);
   INL_OUT(inl, "#endif\n\n");

    if(inl != NULL) {
        fclose(inl);
    }

    if(bin != NULL) {
        if(opts.mono) {
            // Calculate the monospace distance, if requested
            for (i=0; i < numchars; ++i) {
                adv = (int)(fontchar[glyphmap[i]].advance * scale * 16 + 0.5);
                adv += 8;
                adv >>= 4;
                if(opts.mono < adv) {
                    opts.mono = adv;
                }
            }
        }
        writeFontHeader(bin, FLUB_STB_BIN_FONT_VERSION, opts.fontName, opts.fontHeight,
                        opts.bold, opts.mono, packed_width, packed_height,
                        (1 << stb_log2_ceil(packed_height)), 32,
                        numchars, spacing);

        writeFontPixels(bin, pixels, words);

        for(i = 0; i < numchars; i++) {
            writeFontMetric(bin, fontchar[glyphmap[i]].x0,
                            fontchar[glyphmap[i]].y0 + ascent,
                            fontchar[glyphmap[i]].w,
                            fontchar[glyphmap[i]].h,
                            fontchar[glyphmap[i]].s,
                            fontchar[glyphmap[i]].t,
                            (int) (fontchar[glyphmap[i]].advance * scale * 16+0.5));
        }
        fclose(bin);
    }

#ifdef WRITE_FILE
   if(opts.generatePng != NULL) {
       for (i = 0; i < packed_width * packed_height; ++i) {
           if (zero_compress)
               // unpack with same math
               packed[i] = (packed[i] ? 0x20 * (packed[i] - 1) + 0x1f : 0);
           else
               packed[i] =
                       (packed[i] << 5) | (packed[i] << 2) | (packed[i] >> 1);
           // gamma correct, use gamma=2.2 rather than sRGB out of laziness
           packed[i] = (unsigned char) (pow(packed[i] / 255.0, 1.0f / 2.2f) *
                                        255);
       }
       stbi_write_png(stb_sprintf(opts.generatePng, name), packed_width,
                      packed_height, 1, packed, packed_width);
   }
#endif

   return 0;
}

// 591


// NOTE: Rework to modularize font generation, to enable reuse within other tools
#if 0


typedef struct ttfFontData_s {
    char *fontfile;
    float height;
    int maxCodepoint;
    int numchars;
    int numglyphs;
    float scale;
    int packed_width;
    int packed_height;
    int zero_compress;
    int mono;
    int ascent;
    int descent;
    int linegap;
    int spacing;
    textchar *fontchar;
    int *glyphmap;
    int *glyph_to_textchar;
    unsigned int *pixels;
    int words;
    unsigned char *packed;
} ttfFontData_t;


#define CHECK_AND_FREE(s,f) do { if(s->f != NULL) { free(s->f); s->f = NULL; } }while(0)

void flubFontDataDestroy(ttfFontData_t *fdata) {
    if(fdata == NULL) {
        return;
    }
    CHECK_AND_FREE(fdata, fontfile);
    CHECK_AND_FREE(fdata, fontchar);
    CHECK_AND_FREE(fdata, glyphmap);
    CHECK_AND_FREE(fdata, glyph_to_textchar);
    CHECK_AND_FREE(fdata, pixels);
    CHECK_AND_FREE(fdata, packed);
    free(fdata);
}

typedef void (*flubErrMsgCallback)(const char *format, ...);

#define ERRMSG(cb,fmt,...)  do{ if((cb) != NULL) { (cb)((fmt), ##__VA_ARGS__); } }while(0)

ttfFontData_t *flubFontDataLoad(const char *filename, int fontHeight, int maxCodepoint, int mono, flubErrMsgCallback errmsg) {
    unsigned char *data;
    stbtt_fontinfo font;
    int i;
    int area;
    ttfFontData_t *fdata;
    int packing_step;
    int adv;

    data = stb_file((char *)filename, NULL);
    if(data == NULL) {
        ERRMSG(errmsg, "Couldn't open '%s'\n", filename);
        return NULL;
    }

    fdata = malloc(sizeof(ttfFontData_t));
    memset(fdata, 0, sizeof(ttfFontData_t));

    fdata->fontfile = strdup(filename);

    fdata->height = (float)fontHeight;
    if((fdata->height < 1) || (fdata->height > 600)) {
        ERRMSG(errmsg, "Font-height must be between 1 and 600\n");
        flubFontDataDestroy(fdata);
        free(data);
        return NULL;
    }
    fdata->maxCodepoint = maxCodepoint;
    fdata->numchars = maxCodepoint - 32 + 1;
    if((fdata->numchars < 0) || (fdata->numchars > 66000)) {
        ERRMSG(errmsg, "Highest codepoint must be between 33 and 65535\n");
        flubFontDataDestroy(fdata);
        free(data);
        return NULL;
    }

    fdata->fontchar = malloc(sizeof(textchar) * fdata->numchars);
    memset(fdata->fontchar, 0, sizeof(textchar) * fdata->numchars);
    fdata->glyphmap = malloc(sizeof(int) * fdata->numchars);
    memset(fdata->glyphmap, 0, sizeof(int) * fdata->numchars);
    fdata->glyph_to_textchar = malloc(sizeof(int) * fdata->numchars);
    memset(fdata->glyph_to_textchar, 0, sizeof(int) * fdata->numchars);

    stbtt_InitFont(&font, data, 0);
    fdata->scale = stbtt_ScaleForPixelHeight(&font, fdata->height);
    stbtt_GetFontVMetrics(&font, &(fdata->ascent), &(fdata->descent), &(fdata->linegap));

    fdata->spacing = (int)(fdata->scale * (fdata->ascent + fdata->descent + fdata->linegap) + 0.5);
    fdata->ascent = (int)(fdata->scale * fdata->ascent);

    for(i=0; i < fdata->numchars; ++i) {
        fdata->glyph_to_textchar[i] = -1;
    }

    area = 0;
    for(i=0; i < fdata->numchars; ++i) {
        int glyph = stbtt_FindGlyphIndex(&font, 32 + i);
        if(fdata->glyph_to_textchar[glyph] == -1) {
            int n = fdata->numglyphs++;
            int advance, left_bearing;
            fdata->fontchar[n].bitmap = stbtt_GetGlyphBitmap(&font, fdata->scale, fdata->scale, glyph, &(fdata->fontchar[n].w), &(fdata->fontchar[n].h), &(fdata->fontchar[n].x0), &(fdata->fontchar[n].y0));
            fdata->fontchar[n].index = n;
            stbtt_GetGlyphHMetrics(&font, glyph, &advance, &left_bearing);
            fdata->fontchar[n].advance = advance;
            fdata->glyph_to_textchar[glyph] = n;
            area += (fdata->fontchar[n].w + 1) * (fdata->fontchar[n].h + 1);
        }
        fdata->glyphmap[i] = fdata->glyph_to_textchar[glyph];
        // compute the total area covered by the font
    }

    // we sort before packing, but this may actually not be ideal for the current
    // packing algorithm -- it misses the opportunity to pack in some tiny characters
    // into gaps early on... maybe it needs to choose the best character greedily instead
    // of presorted? that will be hugely inefficient to compute

    if((fdata->height < 20) && (fdata->numglyphs < 128)) {
        fdata->packed_width = 128;
    } else if((fdata->numglyphs > 256) && (fdata->height > 25)) {
        fdata->packed_width = 512;
    } else {
        fdata->packed_width = 256;
    }

    fdata->packed_height = area / fdata->packed_width + 4;

    packing_step = 2;
    if(fdata->packed_height % packing_step) {
        fdata->packed_height += packing_step - (fdata->packed_height % packing_step);
    }

    // sort by height
    qsort(fdata->fontchar, fdata->numglyphs, sizeof(textchar), stb_intcmp(offsetof(textchar, h)));
    // reverse
    for (i=0; i < ((fdata->numglyphs) / 2); ++i) {
        stb_swap(&(fdata->fontchar[i]), &(fdata->fontchar[fdata->numglyphs - 1 - i]),
                 sizeof(textchar));
    }

    for(;;) {
        if(try_packing(fdata->packed_width, fdata->packed_height, fdata->numchars)) {
            break;
        }
        fdata->packed_height += packing_step;
        if(fdata->packed_height > 256) {
            if(packing_step <= 16) {
                ++packing_step;
            }
        }
    }

    // sort by index so we can lookup by index
    qsort(fdata->fontchar, fdata->numglyphs, sizeof(textchar), stb_intcmp(offsetof(textchar, index)));

    fdata->packed = malloc(fdata->packed_width * fdata->packed_height);
    memset(fdata->packed, 0, fdata->packed_width * fdata->packed_height);
    for(i = 0; i < fdata->numglyphs; ++i) {
        int j;
        for(j = 0; j < fdata->fontchar[i].h; ++j) {
            memcpy(fdata->packed + (fdata->fontchar[i].t + j) * fdata->packed_width + fdata->fontchar[i].s,
                   fdata->fontchar[i].bitmap + j * fdata->fontchar[i].w, fdata->fontchar[i].w);
        }
        assert(fdata->fontchar[i].s + fdata->fontchar[i].w + 1 <= fdata->packed_width);
    }

    int zc_bits=0;
    int db_bits=0;
    for(i = 0; i < (fdata->packed_width * fdata->packed_height); ++i) {
        if(fdata->packed[i] < 16) {
            zc_bits += 1;
        } else {
            zc_bits += 4;
        }
        if(fdata->packed[i] < 16 || fdata->packed[i] > 240) {
            db_bits += 2;
        } else {
            db_bits += 4;
        }
    }
    if(zc_bits < (fdata->packed_width * fdata->packed_height * 32 / 10)) {
        fdata->zero_compress = 1;
    }

    // always force zero_compress on, as the other path hasn't been fully tested,
    // as in my test fonts they never mattered
    fdata->zero_compress = 1;

    if(fdata->zero_compress) {
        // squeeze to 9 distinct values
        for(i = 0; i < (fdata->packed_width * fdata->packed_height); ++i) {
            int n = (fdata->packed[i] + (255 / 8) / 2) / (255 / 8);
            fdata->packed[i] = n;
        }
    } else {
        // squeeze to 3 bits per pixel (2 bits didn't look good enough)
        for(i = 0; i < (fdata->packed_width * fdata->packed_height; ++i) {
            int n = (fdata->packed[i] >> 5);
            fdata->packed[i] = n;
        }
    }

    // Calculate the monospace distance, if requested
    if(mono) {
        for (i = 0; i < fdata->numchars; ++i) {
            adv = (int) (
                    fdata->fontchar[fdata->glyphmap[i]].advance * fdata->scale *
                    16 + 0.5);
            adv += 8;
            adv >>= 4;
            if (mono < adv) {
                mono = adv;
            }
        }
    }
    fdata->mono = mono;

    free(data);
    return fdata;
}

void flubFontDataGenerateInline(FILE *fp, ttfFontData_t *fdata, const char *name) {
    int spaces;
    int i, j;
    int adv;
    int numbits, bitpack;

    fprintf(fp, "// Font generated by stb_font_inl_generator.c ");
    if(fdata->zero_compress) {
        fprintf(fp, "(4/1 bpp)\n");
    } else {
        fprintf(fp, "(3.2 bpp)\n");
    }
    fprintf(fp, "//\n");
    fprintf(fp, "// Following instructions show how to use the only included font, whatever it is, in\n");
    fprintf(fp, "// a generic way so you can replace it with any other font by changing the include.\n");
    fprintf(fp, "// To use multiple fonts, replace STB_SOMEFONT_* below with STB_FONT_%s_*,\n", name);
    fprintf(fp, "// and separately install each font. Note that the CREATE function call has a\n");
    fprintf(fp, "// totally different name; it's just 'stb_font_%s'.\n", name);
    fprintf(fp, "//\n");
    fprintf(fp, "/* // Example usage:\n");
    fprintf(fp, "\n");
    fprintf(fp, "static stb_fontchar fontdata[STB_SOMEFONT_NUM_CHARS];\n");
    fprintf(fp, "\n");
    fprintf(fp, "static void init(void)\n");
    fprintf(fp, "{\n");
    fprintf(fp, "    // optionally replace both STB_SOMEFONT_BITMAP_HEIGHT with STB_SOMEFONT_BITMAP_HEIGHT_POW2\n");
    fprintf(fp, "    static unsigned char fontpixels[STB_SOMEFONT_BITMAP_HEIGHT][STB_SOMEFONT_BITMAP_WIDTH];\n");
    fprintf(fp, "    STB_SOMEFONT_CREATE(fontdata, fontpixels, STB_SOMEFONT_BITMAP_HEIGHT);\n");
    fprintf(fp, "    ... create texture ...\n");
    fprintf(fp, "    // for best results rendering 1:1 pixels texels, use nearest-neighbor sampling\n");
    fprintf(fp, "    // if allowed to scale up, use bilerp\n");
    fprintf(fp, "}\n");
    fprintf(fp, "\n");
    fprintf(fp, "// This function positions characters on integer coordinates, and assumes 1:1 texels to pixels\n");
    fprintf(fp, "// Appropriate if nearest-neighbor sampling is used\n");
    fprintf(fp, "static void draw_string_integer(int x, int y, char *str) // draw with top-left point x,y\n");
    fprintf(fp, "{\n");
    fprintf(fp, "    ... use texture ...\n");
    fprintf(fp, "    ... turn on alpha blending and gamma-correct alpha blending ...\n");
    fprintf(fp, "    glBegin(GL_QUADS);\n");
    fprintf(fp, "    while (*str) {\n");
    fprintf(fp, "        int char_codepoint = *str++;\n");
    fprintf(fp, "        stb_fontchar *cd = &fontdata[char_codepoint - STB_SOMEFONT_FIRST_CHAR];\n");
    fprintf(fp, "        glTexCoord2f(cd->s0, cd->t0); glVertex2i(x + cd->x0, y + cd->y0);\n");
    fprintf(fp, "        glTexCoord2f(cd->s1, cd->t0); glVertex2i(x + cd->x1, y + cd->y0);\n");
    fprintf(fp, "        glTexCoord2f(cd->s1, cd->t1); glVertex2i(x + cd->x1, y + cd->y1);\n");
    fprintf(fp, "        glTexCoord2f(cd->s0, cd->t1); glVertex2i(x + cd->x0, y + cd->y1);\n");
    fprintf(fp, "        // if bilerping, in D3D9 you'll need a half-pixel offset here for 1:1 to behave correct\n");
    fprintf(fp, "        x += cd->advance_int;\n");
    fprintf(fp, "    }\n");
    fprintf(fp, "    glEnd();\n");
    fprintf(fp, "}\n");
    fprintf(fp, "\n");
    fprintf(fp, "// This function positions characters on float coordinates, and doesn't require 1:1 texels to pixels\n");
    fprintf(fp, "// Appropriate if bilinear filtering is used\n");
    fprintf(fp, "static void draw_string_float(float x, float y, char *str) // draw with top-left point x,y\n");
    fprintf(fp, "{\n");
    fprintf(fp, "    ... use texture ...\n");
    fprintf(fp, "    ... turn on alpha blending and gamma-correct alpha blending ...\n");
    fprintf(fp, "    glBegin(GL_QUADS);\n");
    fprintf(fp, "    while (*str) {\n");
    fprintf(fp, "        int char_codepoint = *str++;\n");
    fprintf(fp, "        stb_fontchar *cd = &fontdata[char_codepoint - STB_SOMEFONT_FIRST_CHAR];\n");
    fprintf(fp, "        glTexCoord2f(cd->s0f, cd->t0f); glVertex2f(x + cd->x0f, y + cd->y0f);\n");
    fprintf(fp, "        glTexCoord2f(cd->s1f, cd->t0f); glVertex2f(x + cd->x1f, y + cd->y0f);\n");
    fprintf(fp, "        glTexCoord2f(cd->s1f, cd->t1f); glVertex2f(x + cd->x1f, y + cd->y1f);\n");
    fprintf(fp, "        glTexCoord2f(cd->s0f, cd->t1f); glVertex2f(x + cd->x0f, y + cd->y1f);\n");
    fprintf(fp, "        // if bilerping, in D3D9 you'll need a half-pixel offset here for 1:1 to behave correct\n");
    fprintf(fp, "        x += cd->advance;\n");
    fprintf(fp, "    }\n");
    fprintf(fp, "    glEnd();\n");
    fprintf(fp, "}\n");
    fprintf(fp, "*/\n\n");

    fprintf(fp, "%s",
            "#ifndef STB_FONTCHAR__TYPEDEF\n"
                    "#define STB_FONTCHAR__TYPEDEF\n"
                    "typedef struct\n"
                    "{\n"
                    "    // coordinates if using integer positioning\n"
                    "    float s0,t0,s1,t1;\n"
                    "    signed short x0,y0,x1,y1;\n"
                    "    int   advance_int;\n"
                    "    // coordinates if using floating positioning\n"
                    "    float s0f,t0f,s1f,t1f;\n"
                    "    float x0f,y0f,x1f,y1f;\n"
                    "    float advance;\n"
                    "} stb_fontchar;\n"
                    "#endif\n\n");

    fprintf(fp, "#define STB_FONT_%s_BITMAP_WIDTH        %4d\n", name, fdata->packed_width);
    fprintf(fp, "#define STB_FONT_%s_BITMAP_HEIGHT       %4d\n", name, fdata->packed_height);
    fprintf(fp, "#define STB_FONT_%s_BITMAP_HEIGHT_POW2  %4d\n\n", name, 1 << stb_log2_ceil(fdata->packed_height));
    fprintf(fp, "#define STB_FONT_%s_FIRST_CHAR            32\n", name);
    fprintf(fp, "#define STB_FONT_%s_NUM_CHARS           %4d\n\n", name, fdata->numchars);
    fprintf(fp, "#define STB_FONT_%s_LINE_SPACING        %4d\n\n", name, fdata->spacing);

    fprintf(fp,  "static unsigned int stb__%s_pixels[]={\n", name);
    spaces = 0;
    numbits = 0;
    bitpack=0;

    fdata->pixels = malloc(fdata->packed_width * fdata->packed_height + 16);
    memset(fdata->pixels, 0, (fdata->packed_width * fdata->packed_height + 16));

    if(fdata->zero_compress) {
        for(j = 1; j < fdata->packed_height-1; ++j) {
            for(i = 1; i < fdata->packed_width-1; ++i) {
                int v = fdata->packed[j * fdata->packed_width+i];
                if(v) {
                    bitpack |= (1 << numbits);
                }
                ++numbits;
                if(numbits == 32 || (v && numbits > 29)) {
                    if (spaces == 0) {
                        fprintf(fp,  "    ");
                        spaces += 4;
                    }
                    fprintf(fp, "0x%08x,", bitpack);
                    fdata->pixels[fdata->words] = bitpack;
                    ++fdata->words;
                    spaces += 11;
                    if (spaces+11 >= 79) {
                        fprintf(fp,  "\n");
                        spaces=0;
                    }
                    bitpack = 0;
                    numbits=0;
                }
                if(v) {
                    bitpack |= ((v-1) << numbits);
                    numbits += 3;
                    assert(numbits <= 32);
                    if(numbits == 32) {
                        if(spaces == 0) {
                            fprintf(fp, "    ");
                            spaces += 4;
                        }
                        fprintf(fp, "0x%08x,", bitpack);
                        fdata->pixels[fdata->words] = bitpack;
                        ++fdata->words;
                        spaces += 11;
                        if(spaces+11 >= 79) {
                            fprintf(fp, "\n");
                            spaces=0;
                        }
                        bitpack = 0;
                        numbits=0;
                    }
                }
            }
        }
        if(numbits) {
            if(spaces == 0) {
                fprintf(fp, "    ");
                spaces += 4;
            }
            fprintf(fp, "0x%08x,", bitpack);
            fdata->pixels[fdata->words] = bitpack;
            ++fdata->words;
        }
    } else {
        // UNTESTED
        // pack the bitmap, 10 pixels per 32-bit word
        for(j = 1; j < fdata->packed_height-1; ++j) {
            for(i = 1; i < fdata->packed_width-1; i += 10) {
                int k, bits = 0;
                for(k = 0; k < 10; ++k) {
                    if (i + k < fdata->packed_width) {
                        bits += fdata->packed[j * fdata->packed_width + i + k] << (k * 3);
                    }
                }
                if(spaces == 0) {
                    fprintf(fp, "    ");
                    spaces += 4;
                }
                fprintf(fp, "0x%08x,", bits);
                fdata->pixels[fdata->words] = bitpack;
                ++fdata->words;
                spaces += 11;
                if(spaces + 11 >= 79) {
                    fprintf(fp, "\n");
                    spaces=0;
                }
            }
        }
    }

    if(spaces) {
        fprintf(fp,  "\n");
    }
    fprintf(fp, "};\n\n");

    fprintf(fp, "static signed short stb__%s_x[%d]={ ", name, fdata->numchars);
    for(i = 0; i < fdata->numchars; ++i) {
        fprintf(fp,  "%d,", fdata->fontchar[fdata->glyphmap[i]].x0);
        if(i % 30 == 13) {
            fprintf(fp, "\n");
        }
    }
    fprintf(fp, " };\n");

    fprintf(fp, "static signed short stb__%s_y[%d]={ ", name, fdata->numchars);
    for(i = 0; i < fdata->numchars; ++i) {
        fprintf(fp,  "%d,", fdata->fontchar[fdata->glyphmap[i]].y0 + fdata->ascent);
        if(i % 30 == 13) {
            fprintf(fp, "\n");
        }
    }
    fprintf(fp, " };\n");

    fprintf(fp, "static unsigned short stb__%s_w[%d]={ ", name, fdata->numchars);
    for(i = 0; i < fdata->numchars; ++i) {
        fprintf(fp, "%d,", fdata->fontchar[fdata->glyphmap[i]].w);
        if(i % 30 == 13) {
            fprintf(fp, "\n");
        }
    }
    fprintf(fp, " };\n");

    fprintf(fp, "static unsigned short stb__%s_h[%d]={ ", name, fdata->numchars);
    for(i = 0; i < fdata->numchars; ++i) {
        fprintf(fp, "%d,", fdata->fontchar[fdata->glyphmap[i]].h);
        if(i % 30 == 13) {
            fprintf(fp, "\n");
        }
    }
    fprintf(fp, " };\n");

    fprintf(fp, "static unsigned short stb__%s_s[%d]={ ", name, fdata->numchars);
    for(i = 0; i < fdata->numchars; ++i) {
        fprintf(fp, "%d,", fdata->fontchar[fdata->glyphmap[i]].s);
        if(i % 20 == 10) {
            fprintf(fp, "\n");
        }
    }
    fprintf(fp, " };\n");

    fprintf(fp, "static unsigned short stb__%s_t[%d]={ ", name, fdata->numchars);
    for(i = 0; i < fdata->numchars; ++i) {
        fprintf(fp, "%d,", fdata->fontchar[fdata->glyphmap[i]].t);
        if(i % 20 == 10) {
            fprintf(fp, "\n");
        }
    }
    fprintf(fp, " };\n");

    fprintf(fp, "static unsigned short stb__%s_a[%d]={ ", name, fdata->numchars);
    for(i = 0; i < fdata->numchars; ++i) {
        fprintf(fp, "%d,", (int) (fdata->fontchar[fdata->glyphmap[i]].advance * fdata->scale * 16+0.5));
        if(i % 16 == 7) {
            fprintf(fp, "\n");
        }
    }
    fprintf(fp, " };\n\n");

    fprintf(fp, "// Call this function with\n");
    fprintf(fp, "//    font: NULL or array length\n");
    fprintf(fp, "//    data: NULL or specified size\n");
    fprintf(fp, "//    height: STB_FONT_%s_BITMAP_HEIGHT or STB_FONT_%s_BITMAP_HEIGHT_POW2\n", name,name);
    fprintf(fp, "//    return value: spacing between lines\n");
    fprintf(fp, "static void stb_font_%s(stb_fontchar font[STB_FONT_%s_NUM_CHARS],\n", name,name);
    fprintf(fp, "                unsigned char data[STB_FONT_%s_BITMAP_HEIGHT][STB_FONT_%s_BITMAP_WIDTH],\n", name,name);
    fprintf(fp, "                int height)\n");
    fprintf(fp, "{\n");
    fprintf(fp, "    int i,j;\n");

    if(fdata->zero_compress) {
        fprintf(fp, "    if (data != 0) {\n");
        fprintf(fp, "        unsigned int *bits = stb__%s_pixels;\n", name);
        fprintf(fp, "        unsigned int bitpack = *bits++, numbits = 32;\n");
        fprintf(fp, "        for (i=0; i < STB_FONT_%s_BITMAP_WIDTH*height; ++i)\n", name);
        fprintf(fp, "            data[0][i] = 0;  // zero entire bitmap\n");
        fprintf(fp, "        for (j=1; j < STB_FONT_%s_BITMAP_HEIGHT-1; ++j) {\n", name);
        fprintf(fp, "            for (i=1; i < STB_FONT_%s_BITMAP_WIDTH-1; ++i) {\n", name);
        fprintf(fp, "                unsigned int value;\n");
        fprintf(fp, "                if (numbits==0) bitpack = *bits++, numbits=32;\n");
        fprintf(fp, "                value = bitpack & 1;\n");
        fprintf(fp, "                bitpack >>= 1, --numbits;\n");
        fprintf(fp, "                if (value) {\n");
        fprintf(fp, "                    if (numbits < 3) bitpack = *bits++, numbits = 32;\n");
        fprintf(fp, "                    data[j][i] = (bitpack & 7) * 0x20 + 0x1f;\n");
        fprintf(fp, "                    bitpack >>= 3, numbits -= 3;\n");
        fprintf(fp, "                } else {\n");
        fprintf(fp, "                    data[j][i] = 0;\n");
        fprintf(fp, "                }\n");
        fprintf(fp, "            }\n");
        fprintf(fp, "        }\n");
        fprintf(fp, "    }\n\n");
    } else {
        // UNTESTED
        fprintf(fp, "    if (data != 0) {\n");
        fprintf(fp, "        unsigned int *bits = stb__%s_pixels;\n", name);
        fprintf(fp, "        unsigned int bitpack = *bits++, numbits = 30;\n");
        fprintf(fp, "        for (i=0; i < STB_FONT_%s_BITMAP_WIDTH*height; ++i)\n", name);
        fprintf(fp, "            data[0][i] = 0;  // zero entire bitmap\n");
        fprintf(fp, "        for (j=1; j < STB_FONT_%s_BITMAP_HEIGHT-1; ++j) {\n", name);
        fprintf(fp, "            numbits=0;\n");
        fprintf(fp, "            for (i=1; i < STB_FONT_%s_BITMAP_WIDTH-1; ++i) {\n", name);
        fprintf(fp, "                unsigned int value;\n");
        fprintf(fp, "                if (numbits==0) bitpack = *bits++, numbits=30;\n");
        fprintf(fp, "                value = bitpack & 7;\n");
        fprintf(fp, "                data[j][i] = (value<<5) + (value << 2) + (value >> 1);\n");
        fprintf(fp, "                bitpack >>= 3, numbits -= 3;\n");
        fprintf(fp, "            }\n");
        fprintf(fp, "        }\n");
        fprintf(fp, "    }\n\n");
    }

    fprintf(fp, "    // build font description\n");
    fprintf(fp, "    if (font != 0) {\n");
    fprintf(fp, "        float recip_width = 1.0f / STB_FONT_%s_BITMAP_WIDTH;\n", name);
    fprintf(fp, "        float recip_height = 1.0f / height;\n");
    fprintf(fp, "        for (i=0; i < STB_FONT_%s_NUM_CHARS; ++i) {\n", name);
    fprintf(fp, "            // pad characters so they bilerp from empty space around each character\n");
    fprintf(fp, "            font[i].s0 = (stb__%s_s[i]) * recip_width;\n", name);
    fprintf(fp, "            font[i].t0 = (stb__%s_t[i]) * recip_height;\n", name);
    fprintf(fp, "            font[i].s1 = (stb__%s_s[i] + stb__%s_w[i]) * recip_width;\n", name, name);
    fprintf(fp, "            font[i].t1 = (stb__%s_t[i] + stb__%s_h[i]) * recip_height;\n", name, name);
    fprintf(fp, "            font[i].x0 = stb__%s_x[i];\n", name);
    fprintf(fp, "            font[i].y0 = stb__%s_y[i];\n", name);
    fprintf(fp, "            font[i].x1 = stb__%s_x[i] + stb__%s_w[i];\n", name, name);
    fprintf(fp, "            font[i].y1 = stb__%s_y[i] + stb__%s_h[i];\n", name, name);
    fprintf(fp, "            font[i].advance_int = (stb__%s_a[i]+8)>>4;\n", name);
    fprintf(fp, "            font[i].s0f = (stb__%s_s[i] - 0.5f) * recip_width;\n", name);
    fprintf(fp, "            font[i].t0f = (stb__%s_t[i] - 0.5f) * recip_height;\n", name);
    fprintf(fp, "            font[i].s1f = (stb__%s_s[i] + stb__%s_w[i] + 0.5f) * recip_width;\n", name, name);
    fprintf(fp, "            font[i].t1f = (stb__%s_t[i] + stb__%s_h[i] + 0.5f) * recip_height;\n", name, name);
    fprintf(fp, "            font[i].x0f = stb__%s_x[i] - 0.5f;\n", name);
    fprintf(fp, "            font[i].y0f = stb__%s_y[i] - 0.5f;\n", name);
    fprintf(fp, "            font[i].x1f = stb__%s_x[i] + stb__%s_w[i] + 0.5f;\n", name, name);
    fprintf(fp, "            font[i].y1f = stb__%s_y[i] + stb__%s_h[i] + 0.5f;\n", name, name);
    fprintf(fp, "            font[i].advance = stb__%s_a[i]/16.0f;\n", name);
    fprintf(fp, "        }\n");
    fprintf(fp, "    }\n");
    fprintf(fp, "}\n\n");

    fprintf(fp, "#ifndef STB_SOMEFONT_CREATE\n");
    fprintf(fp, "#define STB_SOMEFONT_CREATE              stb_font_%s\n", name);
    fprintf(fp, "#define STB_SOMEFONT_BITMAP_WIDTH        STB_FONT_%s_BITMAP_WIDTH\n",name);
    fprintf(fp, "#define STB_SOMEFONT_BITMAP_HEIGHT       STB_FONT_%s_BITMAP_HEIGHT\n",name);
    fprintf(fp, "#define STB_SOMEFONT_BITMAP_HEIGHT_POW2  STB_FONT_%s_BITMAP_HEIGHT_POW2\n",name);
    fprintf(fp, "#define STB_SOMEFONT_FIRST_CHAR          STB_FONT_%s_FIRST_CHAR\n",name);
    fprintf(fp, "#define STB_SOMEFONT_NUM_CHARS           STB_FONT_%s_NUM_CHARS\n",name);
    fprintf(fp, "#define STB_SOMEFONT_LINE_SPACING        STB_FONT_%s_LINE_SPACING\n", name);
    fprintf(fp, "#endif\n\n");
}

static void _flubWriteFontHeader(FILE *fp, int version, const char *name, int size,
                                 int mono, int width, int height, int heightPow2,
                                 int firstChar, int numChars, int lineSpacing) {
    int namelen;
    int dummy;

    fwrite("STBFONT", 8, 1, fp);
    fwrite(&version, sizeof(int), 1, fp);

    namelen = strlen(name) + 1;
    fwrite(&namelen, sizeof(int), 1, fp);
    fwrite(name, namelen, 1, fp);
    fwrite(&size, sizeof(int), 1, fp);
    fwrite(&dummy, sizeof(int), 1, fp);
    fwrite(&mono, sizeof(int), 1, fp);

    fwrite(&width, sizeof(int), 1, fp);
    fwrite(&height, sizeof(int), 1, fp);
    fwrite(&heightPow2, sizeof(int), 1, fp);
    fwrite(&firstChar, sizeof(int), 1, fp);
    fwrite(&numChars, sizeof(int), 1, fp);
    fwrite(&lineSpacing, sizeof(int), 1, fp);
}

static void _flubWriteFontPixels(FILE *fp, unsigned int *pixels, int length) {
    int k;

    fwrite(&length, sizeof(int), 1, fp);

    for(k = 0; k < length; k++) {
        fwrite(pixels + k, sizeof(unsigned int), 1, fp);
    }
}

static void _flubWriteFontMetric(FILE *fp, signed short x, signed short y,
                                 unsigned short w, unsigned short h,
                                 unsigned short s, unsigned short t,
                                 unsigned short a) {
    fwrite(&x, sizeof(signed short), 1, fp);
    fwrite(&y, sizeof(signed short), 1, fp);
    fwrite(&w, sizeof(unsigned short), 1, fp);
    fwrite(&h, sizeof(unsigned short), 1, fp);
    fwrite(&s, sizeof(unsigned short), 1, fp);
    fwrite(&t, sizeof(unsigned short), 1, fp);
    fwrite(&a, sizeof(unsigned short), 1, fp);
}

void flubFontDataGenerateBinFile(FILE *fp, ttfFontData_t *fdata, const char *name) {
    int i;

    _flubWriteFontHeader(fp, FLUB_STB_BIN_FONT_VERSION, name, fdata->height,
                         fdata->mono, fdata->packed_width, fdata->packed_height,
                         (1 << stb_log2_ceil(fdata->packed_height)), 32,
                         fdata->numchars, fdata->spacing);

    _flubWriteFontPixels(fp, fdata->pixels, fdata->words);

    for(i = 0; i < fdata->numchars; i++) {
        _flubWriteFontMetric(fp, fdata->fontchar[fdata->glyphmap[i]].x0,
                             fdata->fontchar[fdata->glyphmap[i]].y0 + fdata->ascent,
                             fdata->fontchar[fdata->glyphmap[i]].w,
                             fdata->fontchar[fdata->glyphmap[i]].h,
                             fdata->fontchar[fdata->glyphmap[i]].s,
                             fdata->fontchar[fdata->glyphmap[i]].t,
                             (int) (fdata->fontchar[fdata->glyphmap[i]].advance * fdata->scale * 16+0.5));
    }

}

// TODO: Generate PNG to memory buffer, for immediate tool usage
void flubFontDataGeneratePNG(const char *filename, ttfFontData_t *fdata) {
    int i;

    for(i = 0; i < fdata->packed_width * fdata->packed_height; ++i) {
        if(fdata->zero_compress) {
            // unpack with same math
            fdata->packed[i] = (fdata->packed[i] ?
                                0x20 * (fdata->packed[i] - 1) + 0x1f : 0);
        } else {
            fdata->packed[i] =
                    (fdata->packed[i] << 5) | (fdata->packed[i] << 2) |
                                              (fdata->packed[i] >> 1);
        }
        // gamma correct, use gamma=2.2 rather than sRGB out of laziness
        fdata->packed[i] = (unsigned char) (pow(fdata->packed[i] / 255.0, 1.0f / 2.2f) *
                                     255);
    }
    stbi_write_png(filename, fdata->packed_width,
                   fdata->packed_height, 1, fdata->packed, fdata->packed_width);
}

#endif
