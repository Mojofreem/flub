#include <flub/flub.h>
#include <ctype.h>
#include <flub/font.h>
#include <flub/stbfont.h>
#include <flub/memory.h>
#include <flub/util/color.h>
#include <flub/module.h>


/////////////////////////////////////////////////////////////////////////////
// font module registration
/////////////////////////////////////////////////////////////////////////////

int flubFontInit(appDefaults_t *defaults);
int flubFontStart(void);
void flubFontShutdown(void);

static char *_initDeps[] = {"video", "texture", "physfs", NULL};
static char *_startDeps[] = {"video", NULL};
flubModuleCfg_t flubModuleFont = {
    .name = "font",
    .init = flubFontInit,
    .start = flubFontStart,
    .shutdown = flubFontShutdown,
    .initDeps = _initDeps,
    .startDeps = _startDeps,
};

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

#define FONT_BASE_SIZE  128
#define FNT_TRACE       1


typedef struct fontIndex_s {
    GLuint list;    // Used for simplified direct blitting
    texture_t *texture;
    int useCount;
    flubStbFont_t stbfont;
    char *filename;
    struct fontIndex_s *next;
} fontIndex_t;

typedef struct fontCtx_S {
    int inited;
    flubColor4f_t color;
    float red, green, blue, alpha;
    fontIndex_t *index;
    int x, y;
    GLuint stbfontShader;
    GLuint stbfontProgram;
} fontCtx_t;

fontCtx_t fontCtx = {
    .inited = 0,
    .color = {
        .red = 1.0,
        .green = 1.0,
        .blue = 1.0,
        .alpha = 1.0,
    },
    .index = NULL,
    .x = 0,
    .y = 0,
    .stbfontShader = 0,
    .stbfontProgram = 0,
};

#define FONT_DESC_FMT   "%s size %d%s%s"
#define FONT_DESC_PARAMS(f)    (f)->name, (f)->size, (((f)->bold) ? ", bold" : ""), (((f)->mono) ? ", monospaced" : "")


// Font manager context ///////////////////////////////////////////////////////

static void _fontLoadGLFont(fontIndex_t *font) {
    int k;
    int charCount;
    int mono = 0;

    charCount = font->stbfont.numChars + font->stbfont.firstChar;
    font->list = glGenLists(charCount);
    font->texture = texmgrCreate(NULL, GL_LINEAR, GL_LINEAR, 0, 0, 0, 0, 1,
                                 GL_ALPHA, font->stbfont.data,
                                 font->stbfont.bmpWidth,
                                 font->stbfont.bmpHeightPow2);

    for(k = 0; k < charCount; k++) {
        glNewList(font->list + k, GL_COMPILE);
        glBindTexture(GL_TEXTURE_2D, font->texture->id);
        if(k < font->stbfont.firstChar) {
            glTranslated(0, 0, 0);
        } else {
            stb_fontchar *cd = &font->stbfont.fontData[k - font->stbfont.firstChar];
            glBegin(GL_QUADS);
            glTexCoord2f(cd->s0, cd->t0); // Texture Coord (Bottom Left)
            glVertex2i(cd->x0, cd->y0);   // Vertex Coord (Bottom Left)
            glTexCoord2f(cd->s1, cd->t0); // Texture Coord (Bottom Right)
            glVertex2i(cd->x1, cd->y0);   // Vertex Coord (Bottom Right)
            glTexCoord2f(cd->s1, cd->t1); // Texture Coord (Top Right)
            glVertex2i(cd->x1, cd->y1);   // Vertex Coord (Top Right)
            glTexCoord2f(cd->s0, cd->t1); // Texture Coord (Top Left)
            glVertex2i(cd->x0, cd->y1);   // Vertex Coord (Top Left)
            glEnd();
            glTranslated(((mono > 0) ? mono : cd->advance_int), 0, 0);
        }
        glEndList();
    }
}

int flubFontInit(appDefaults_t *defaults) {
    if(fontCtx.inited) {
        return 1;
    }

    if(!logValid()) {
        // The logger has not been initialized!
        return 0;
    }

    logDebugRegister("font", DBG_FONT, "trace", FNT_TRACE);

    if(!flubFontLoad("resources/font/courier.12.stbfont")) {
        errorf("Unable to load flub default courier font, size 12.");
    }

    if(!flubFontLoad("resources/font/courier.12.bold.stbfont")) {
        errorf("Unable to load flub default courier bold font, size 12.");
    }

    if(!flubFontLoad("resources/font/consolas.12.stbfont")) {
        errorf("Unable to load flub default consolas font, size 12.");
    }

    fontCtx.inited = 1;
    
    return 1;
}

int flubFontStart(void) {
    GLuint vertexShader;

    if(((fontCtx.stbfontShader = videoGetShader(GL_FRAGMENT_SHADER, "resources/shaders/stbfont.frag")) == 0) ||
       ((vertexShader = videoGetShader(GL_VERTEX_SHADER, "resources/shaders/simple.vert")) == 0) ||
       ((fontCtx.stbfontProgram = videoCreateProgram("stbfont")) == 0)) {
        return 0;
    }

    glAttachShader(fontCtx.stbfontProgram, vertexShader);
    glAttachShader(fontCtx.stbfontProgram, fontCtx.stbfontShader);
    glLinkProgram(fontCtx.stbfontProgram);
    if(!videoValidateOpenGLProgram(fontCtx.stbfontProgram)) {
        return 0;
    }

    return 1;
}

void flubFontShutdown(void) {
}

static void _flubFontDetails(fontIndex_t *font) {
    flubStbFont_t *stb = &(font->stbfont);

    info("##################################################");
    infof("Version: %d", stb->version);
    infof("Name: [%s]", stb->name);
    infof("Size: %d", stb->size);
    infof("Bold: %s", ((stb->bold) ? "YES" : "NO"));
    infof("Mono: %s (%d)", ((stb->mono) ? "YES" : "NO"), stb->mono);
    infof("BmpWidth: %d", stb->bmpWidth);
    infof("BmpHeight: %d", stb->bmpHeight);
    infof("BmpHeightPow2: %d", stb->bmpHeightPow2);
    infof("FirstChar: %d", stb->firstChar);
    infof("NumChars: %d", stb->numChars);
    infof("lineSpacing: %d", stb->lineSpacing);
    infof("DataPtr: 0x%p", stb->data);
    info("##################################################");
}

static int _flubFontLoadHeader(const char *filename, PHYSFS_file *handle, flubStbFont_t *details) {
    char name[FLUB_STB_BIN_FONT_MAX_NAMELEN];
    int pos;
    int len;

    debugf(DBG_FONT, FNT_TRACE, "Loading font header");

    if((!PHYSFS_readSLE32(handle, &(details->version))) ||
       (!PHYSFS_readSLE32(handle, &(details->namelen)))) {
        errorf("Failed reading font \"%s\" header: %s", filename, PHYSFS_getLastError());
        return 0;
    }
    pos = PHYSFS_tell(handle);
    len = details->namelen;
    pos += len;
    if(len > FLUB_STB_BIN_FONT_MAX_NAMELEN) {
        len = FLUB_STB_BIN_FONT_MAX_NAMELEN;
    }
    if(!PHYSFS_read(handle, name, len, 1)) {
        errorf("Failed reading font \"%s\" name: %s", filename, PHYSFS_getLastError());
        return 0;
    }
    name[FLUB_STB_BIN_FONT_MAX_NAMELEN - 1] = '\0';
    if(!PHYSFS_seek(handle, pos)) {
        errorf("Failed reading font \"%s\" header: %s", filename, PHYSFS_getLastError());
        return 0;
    }
    details->name = util_strdup(name);
    if((!PHYSFS_readSLE32(handle, &(details->size))) ||
       (!PHYSFS_readSLE32(handle, &(details->bold))) ||
       (!PHYSFS_readSLE32(handle, &(details->mono))) ||
       (!PHYSFS_readSLE32(handle, &(details->bmpWidth))) ||
       (!PHYSFS_readSLE32(handle, &(details->bmpHeight))) ||
       (!PHYSFS_readSLE32(handle, &(details->bmpHeightPow2))) ||
       (!PHYSFS_readSLE32(handle, &(details->firstChar))) ||
       (!PHYSFS_readSLE32(handle, &(details->numChars))) ||
       (!PHYSFS_readSLE32(handle, &(details->lineSpacing)))) {
        errorf("Failed reading font \"%s\" header: %s", filename, PHYSFS_getLastError());
        return 0;
    }

    return 1;
}

static int _flubFontLoadPixels(const char *filename, PHYSFS_file *handle, flubStbFont_t *details) {
    int pixelBufLength;
    int pos;
    int i, j;
    unsigned int bitpack;
    unsigned int numbits = 32;
    unsigned char *data;

    debugf(DBG_FONT, FNT_TRACE, "Loading pixels");

    data = util_calloc(details->bmpWidth * details->bmpHeightPow2, 0, NULL);
    details->data = data;

    PHYSFS_readSLE32(handle, &pixelBufLength);
    pos = PHYSFS_tell(handle);
    pos += pixelBufLength;

    if(!PHYSFS_readULE32(handle, &bitpack)) {
        errorf("Failed reading font \"%s\" pixels: %s", filename, PHYSFS_getLastError());
        return 0;
    }
    for(j = 1; j < details->bmpHeight - 1; ++j) {
        for(i = 1; i < details->bmpWidth - 1; ++i) {
            unsigned int value;
            if(numbits == 0) {
                if(!PHYSFS_readULE32(handle, &bitpack)) {
                    errorf("Failed reading font \"%s\" pixels: %s", filename, PHYSFS_getLastError());
                    return 0;
                }
                numbits = 32;
            }
            value = bitpack & 1;
            bitpack >>= 1, --numbits;
            if(value) {
                if(numbits < 3) {
                    if(!PHYSFS_readULE32(handle, &bitpack)) {
                        errorf("Failed reading font \"%s\" pixels: %s", filename, PHYSFS_getLastError());
                        return 0;
                    }
                    numbits = 32;
                }
                data[(j * details->bmpWidth) + i] = (bitpack & 7) * 0x20 + 0x1f;
                bitpack >>= 3, numbits -= 3;
            } else {
                data[(j * details->bmpWidth) + i] = 0;
            }
        }
    }

    return 1;
}

static int _flubFontLoadMetrics(const char *filename, PHYSFS_file *handle, flubStbFont_t *details) {
    details->fontData = util_calloc(sizeof(stb_fontchar) * details->numChars, 0, NULL);
    stb_fontchar *font = details->fontData;
    int i;
    Sint16 x, y;
    Uint16 w, h, s, t, a;

    debugf(DBG_FONT, FNT_TRACE, "Loading metrics for %s", filename);

    // Load the font metrics values
    float recip_width = 1.0f / details->bmpWidth;
    float recip_height = 1.0f / details->bmpHeightPow2;
    for (i=0; i < details->numChars; ++i) {
        if((!PHYSFS_readSLE16(handle, &x)) ||
           (!PHYSFS_readSLE16(handle, &y)) ||
           (!PHYSFS_readULE16(handle, &w)) ||
           (!PHYSFS_readULE16(handle, &h)) ||
           (!PHYSFS_readULE16(handle, &s)) ||
           (!PHYSFS_readULE16(handle, &t)) ||
           (!PHYSFS_readULE16(handle, &a))) {
            errorf("Failed reading font \"%s\" metrics: %s", filename, PHYSFS_getLastError());
            return 0;
        }
        //infof("[%d] x:%d  y:%d  w:%d  h:%d  s:%d  t:%d  a:%d", i, x, y, w, h, s, t, a);
        // pad characters so they bilerp from empty space around each character
        font[i].s0 = s * recip_width;
        font[i].t0 = t * recip_height;
        font[i].s1 = (s + w) * recip_width;
        font[i].t1 = (t + h) * recip_height;
        font[i].x0 = x;
        font[i].y0 = y;
        font[i].x1 = x + w;
        font[i].y1 = y + h;
        font[i].advance_int = (a + 8) >> 4;
        font[i].s0f = (s - 0.5f) * recip_width;
        font[i].t0f = (t - 0.5f) * recip_height;
        font[i].s1f = (s + w + 0.5f) * recip_width;
        font[i].t1f = (t + h + 0.5f) * recip_height;
        font[i].x0f = x - 0.5f;
        font[i].y0f = y - 0.5f;
        font[i].x1f = x + w + 0.5f;
        font[i].y1f = y + h + 0.5f;
        font[i].advance = a / 16.0f;
    }

    return 1;
}

static void _flubFontEntryFree(fontIndex_t *entry) {
    if(entry->stbfont.data != NULL) {
        util_free(entry->stbfont.data);
    }
    if(entry->stbfont.fontData != NULL) {
        util_free(entry->stbfont.fontData);
    }
    util_free(entry);
}

int flubFontLoad(const char *filename) {
    PHYSFS_file *handle;
    fontIndex_t *entry;
    char fileid[10];
    int len;
    int pos;
    unsigned char *data;
    fontIndex_t *walk, *last = NULL;

    if((handle = PHYSFS_openRead(filename)) == NULL) {
        errorf("Unable to open the font file \"%s\".", filename);
        return 0;
    }

    PHYSFS_read(handle, fileid, 8, 1);
    fileid[8] = '\0';
    if(strcmp(fileid, "STBFONT")) {
        PHYSFS_close(handle);
        errorf("File \"%s\" is not a STB binary font file: [%s].", filename, fileid);
        return 0;
    }

    entry = util_calloc(sizeof(fontIndex_t), 0, NULL);

    if(!_flubFontLoadHeader(filename, handle, &(entry->stbfont))) {
        errorf("Unable to read the font file \"%s\" header.", filename);
        _flubFontEntryFree(entry);
        return 0;
    }

    // Check to ensure the font is not already registered.
    for(walk = fontCtx.index; walk != NULL; last = walk, walk = walk->next) {
        pos = strcmp(entry->stbfont.name, walk->stbfont.name);
        if(!pos) {
            // We found the same font family
            if(entry->stbfont.size == walk->stbfont.size) {
                // We found the same size
                if((entry->stbfont.bold && walk->stbfont.bold) ||
                   ((!entry->stbfont.bold) && (!walk->stbfont.bold))){
                    return 1;
                }
            }
            if(entry->stbfont.size > walk->stbfont.size) {
                // Keep checking to see if the larger font height is in the index
                continue;
            }
            if(entry->stbfont.size < walk->stbfont.size) {
                // The font is NOT yet in the table. Insert it between last
                // and current;
                break;
            }
        } else if(pos > 0) {
            // We need to insert the family here.
            break;
        }
    }

    if((!_flubFontLoadPixels(filename, handle, &(entry->stbfont))) ||
       (!_flubFontLoadMetrics(filename, handle, &(entry->stbfont)))) {
        errorf("Unable to load the font file \"%s\".", filename);
        _flubFontEntryFree(entry);
        return 0;
    }

    // Insert the font in the index, having been sorted first by name, then by size.
    entry->list = 0;
    entry->useCount = 0;
    entry->filename = util_strdup(filename);
    if(last == NULL) {
        // We're adding at the begining of the index.
        entry->next = fontCtx.index;
        fontCtx.index = entry;
    } else {
        // We're inserting somewhere in the middle of the index.
        entry->next = last->next;
        last->next = entry;
    }
    debugf(DBG_FONT, FNT_TRACE, "Loaded font \"%s\", size %d%s%s", entry->stbfont.name, entry->stbfont.size, ((entry->stbfont.bold) ? ", bold": ""), ((entry->stbfont.mono) ? ", monospace" : ""));

    return 1;
}

//////////////////////////////////////////////////////////////////////////////
//
// Font retrieval matches on fontname, size, and boldness with the following
// priority:
//
//    Font  Size  Bold
//     *     *     *
//     *     *
//           *
//     *
//
//////////////////////////////////////////////////////////////////////////////

#define FONT_DEFAULT(f)     debugf(DBG_FONT, FNT_TRACE, "...Default font is " FONT_DESC_FMT, FONT_DESC_PARAMS(&((f)->stbfont)))

// Each call to fontGet increments the font's usage counter. Call fontRelease()
// to decrement the counter. When unused, fonts will not be loaded or refreshed
// when video context changes occur.
font_t *fontGet(char *fontname, int size, int bold) {
    fontIndex_t *walk, *use;

    debugf(DBG_FONT, FNT_TRACE, "Searching for font \"%s\", size %d...", fontname, size);
    use = fontCtx.index;
    if(fontCtx.index == NULL) {
        errorf("No fonts have been loaded!");
        return NULL;
    }
    debugf(DBG_FONT, FNT_TRACE, "...Default font is \"%s\", size %d.", use->stbfont.name, ((use == NULL) ? 0 : use->stbfont.size));
    for(walk = fontCtx.index; walk != NULL; walk = walk->next) {
        debugf(DBG_FONT, FNT_TRACE, "...Checking family \"%s\"...", walk->stbfont.name);
        if(!strcmp( walk->stbfont.name, fontname)) {
            if(strcmp(use->stbfont.name, fontname)) {
                debugf(DBG_FONT, FNT_TRACE, "Found font family \"%s\"", fontname);
                use = walk;
                FONT_DEFAULT(use);
            }
            if(walk->stbfont.size == size) {
                debugf(DBG_FONT, FNT_TRACE, "...Found expected size");
                use = walk;
                FONT_DEFAULT(use);
                if((bold && walk->stbfont.bold) ||
                   ((!bold) && (!walk->stbfont.bold))) {
                    debugf(DBG_FONT, FNT_TRACE, "Found matching font!");
                    break;
                } else {
                    debugf(DBG_FONT, FNT_TRACE, "...Bold status does not match");
                }
            } else {
                debugf(DBG_FONT, FNT_TRACE, "...Font size does not match");
                if(((use->stbfont.size > size) && (walk->stbfont.size <= size)) ||
                   ((use->stbfont.size < size) && (walk->stbfont.size < size) && ((size - walk->stbfont.size) < (size - use->stbfont.size)))) {
                    use = walk;
                    FONT_DEFAULT(use);
                }
            }
        } else {
            debugf(DBG_FONT, FNT_TRACE, "...Family does not match");
            // If the default font family is not the asked for family,
            if(strcmp(use->stbfont.name, fontname)) {
                // If the default size is not the requested size, but the
                // current entry size is the requested size OR
                // If the default size is greater than the requested size, and
                // the current entry size is less than or equal to the
                // requested size OR
                // The default size is less than the requested size, and the
                // current entry size is less than the requested size, and the
                // difference between the current size and the requested size
                // is less than the difference between the default size and the
                // requested size
                if(((use->stbfont.size != size) && (walk->stbfont.size == size)) ||
                   ((use->stbfont.size > size) && (walk->stbfont.size <= size)) ||
                   ((use->stbfont.size < size) && (walk->stbfont.size < size) && ((size - walk->stbfont.size) < (size - use->stbfont.size)))) {
                    use = walk;
                    FONT_DEFAULT(use);
                }
            }
        }
    }

    if(use == NULL) {
        errorf("Failed to find suitable substitute for font \"%s\" size %d", fontname, size);
        return NULL;
    }

    if((use->texture == NULL) || (use->list == 0)) {
        debugf(DBG_FONT, FNT_TRACE, "Font \"%s\" (size %d) is not cached, loading now...", fontname, size);
        _fontLoadGLFont(use);
        //_flubFontDetails(use);
    }

    debugf(DBG_FONT, FNT_TRACE, "Returning " FONT_DESC_FMT, FONT_DESC_PARAMS(&(use->stbfont)));

    use->useCount++;
    return (font_t *)use;
}

font_t *fontGetByFilename(const char *filename) {
    fontIndex_t *walk, *use;

    debugf(DBG_FONT, FNT_TRACE, "Searching for font by filename \"%s\"...", filename);
    use = fontCtx.index;
    if(fontCtx.index == NULL) {
        errorf("No fonts have been loaded!");
        return NULL;
    }
    debugf(DBG_FONT, FNT_TRACE, "...Default font is \"%s\", size %d.", use->stbfont.name, ((use == NULL) ? 0 : use->stbfont.size));
    for(walk = fontCtx.index; walk != NULL; walk = walk->next) {
        debugf(DBG_FONT, FNT_TRACE, "...Checking \"%s\"...", walk->stbfont.name);
        if(!strcmp( walk->filename, filename)) {
                use = walk;
                FONT_DEFAULT(use);
                break;
        }
    }

    if(use == NULL) {
        errorf("Failed to find font file \"%s\" entry", filename);
        return NULL;
    }

    if((use->texture == NULL) || (use->list == 0)) {
        debugf(DBG_FONT, FNT_TRACE, "Font file \"%s\" is not cached, loading now...", filename);
        _fontLoadGLFont(use);
        //_flubFontDetails(use);
    }

    debugf(DBG_FONT, FNT_TRACE, "Returning " FONT_DESC_FMT, FONT_DESC_PARAMS(&(use->stbfont)));

    use->useCount++;
    return (font_t *)use;
}

void fontRelease(font_t *font) {
    fontIndex_t *walk = (fontIndex_t *)font;
    
    if(walk == NULL) { 
        return;
    }

    walk->useCount--;
    if(walk->useCount < 0) {
        walk->useCount = 0;
        errorf("Over-release for font \"%s\" size %d.", walk->stbfont.name, walk->stbfont.size);
    }
    // We don't free up allocated resources, they'll stay cached in memory for
    // the next use.
}

///////////////////////////////////////////////////////////////////////////////

int fontGetHeight(font_t *font) {
    if(font == NULL) { 
        return 0;
    }

    return ((fontIndex_t *)font)->stbfont.size;
}

int fontGetCharWidth(font_t *font, int c) {
    int w;
    int a;
    flubStbFont_t *sdata;

    if(font == NULL) {
        return 0;
    }

    sdata = &(((fontIndex_t *)font)->stbfont);

    if(((fontIndex_t *) font)->stbfont.mono) {
        return ((fontIndex_t *)font)->stbfont.mono;
    }

    if((c < sdata->firstChar) || (c > sdata->firstChar + sdata->numChars)) {
        return 0;
    }

    w = (sdata->fontData[c - sdata->firstChar].x1 - sdata->fontData[c - sdata->firstChar].x0);
    a = sdata->fontData[c - sdata->firstChar].advance_int;

    if(a > w) {
        return a;
    }

    return w;
}

int fontGetNarrowestWidth(font_t *font) {
    flubStbFont_t *stb;
	int width = 256;
    int size;
    int k;
    int w;
    int a;
	
    if(font == NULL) { 
        return 0;
    }

    if(((fontIndex_t *)font)->stbfont.mono) {
        return ((fontIndex_t *)font)->stbfont.mono;
    }

    stb = &(((fontIndex_t *)font)->stbfont);

    for(k = stb->firstChar; k < (stb->firstChar + stb->numChars); k++) {
        w = (stb->fontData[k - stb->firstChar].x1 - stb->fontData[k - stb->firstChar].x0);
        a = stb->fontData[k - stb->firstChar].advance_int;

        if(a < width) {
            width = a;
        }

        if(w < width) {
            width = w;
        }
    }

	return width;
}

int fontGetWidestWidth(font_t *font) {
    flubStbFont_t *stb = &(((fontIndex_t *)font)->stbfont);
    int width = 0;
    int size;
    int k;
    int w;
    int a;

    if(font == NULL) {
        return 0;
    }

    if(stb->mono) {
        return stb->mono;
    }

    for(k = stb->firstChar; k < (stb->firstChar + stb->numChars); k++) {
        w = (stb->fontData[k - stb->firstChar].x1 - stb->fontData[k - stb->firstChar].x0);
        a = stb->fontData[k - stb->firstChar].advance_int;

        if(a > width) {
            width = a;
        }

        if(w > width) {
            width = w;
        }
    }

    return width;
}

int fontGetStrWidth(font_t *font, const char *s) {
    flubStbFont_t *work = &(((fontIndex_t *)font)->stbfont);
    int w;
    int a;
	int width = 0;

    if(font == NULL) { 
        return 0;
    }

    if(work->mono) {
        for(; *s != '\0'; s++) {
            width += work->mono;
        }
    } else {
        for(; *s != '\0'; s++) {
            w = (work->fontData[*s - work->firstChar].x1 - work->fontData[*s - work->firstChar].x0);
            a = work->fontData[*s - work->firstChar].advance_int;
            if(*(s + 1) == '\0') {
                width += w;
            } else {
                width += a;
            }
        }
    }

	return width;
}

int fontGetStrNWidth(font_t *font, const char *s, int len) {
    flubStbFont_t *work = &(((fontIndex_t *)font)->stbfont);
    int w;
    int a;
    int width = 0;

    if(font == NULL) {
        return 0;
    }

    if(work->mono) {
        for(; ((len) && (*s != '\0')); s++, len--) {
            width += work->mono;
        }
    } else {
        for(; ((len) && (*s != '\0')); s++, len--) {
            w = (work->fontData[*s - work->firstChar].x1 - work->fontData[*s - work->firstChar].x0);
            a = work->fontData[*s - work->firstChar].advance_int;
            if(*(s + 1) == '\0') {
                width += w;
            } else {
                width += a;
            }
        }
    }

    return width;
}


int fontGetStrLenWrap(font_t *font, const char *s, int maxwidth) {
    flubStbFont_t *stb;
    int w;
    int a;
	int width = 0, size, pos = 0;

    if(font == NULL) { 
        return 0;
    }
    if(((fontIndex_t *)font)->stbfont.mono) {
        for(; *s != '\0'; s++) {
            w = ((fontIndex_t *)font)->stbfont.mono;
            if(((width + w) > maxwidth) || ((width + w) > maxwidth)) {
                return pos;
            }
            width += w;
            pos++;
        }
    } else {
        stb = &(((fontIndex_t *)font)->stbfont);
        for(; *s != '\0'; s++) {
            if(((*s) < stb->firstChar) || ((*s) >= (stb->firstChar + stb->numChars))) {
                pos++;
                continue;
            }
            w = (stb->fontData[*s - stb->firstChar].x1 - stb->fontData[*s - stb->firstChar].x0);
            a = stb->fontData[*s - stb->firstChar].advance_int;
            if(((width + w) > maxwidth) || ((width + a) > maxwidth)) {
                return pos;
            }
            width += a;
            pos++;
        }
    }
	return pos;
}

int fontGetStrLenWordWrap(font_t *font, char *s, int maxwidth, char **next,
                          int *width) {
    flubStbFont_t *work = &(((fontIndex_t *)font)->stbfont);
    int w;
    int a;
	char *lastp = NULL;
	int workwidth = 0, size, pos = 0, lasts = 0;

    if(font == NULL) { 
        return 0;
    }

    if(work->mono) {
        for(; *s != '\0'; s++) {
            w = work->mono;
            if(((workwidth + w) > maxwidth) || ((workwidth + w) > maxwidth)) {
                if(lastp == NULL) {
                    // The text is too long for a single line, so force a split.
                    break;
                }
                // Back this truck up
                lastp++;
                while((*lastp != '\0') && (isspace(*lastp))) {
                    lastp++;
                }
                s = lastp;
                pos = lasts;
                break;
            }
            workwidth += w;
            pos++;
            if(isspace(*s)) {
                lastp = s;
                lasts = pos;
            }
        }
    } else {
        for(; *s != '\0'; s++) {
            w = (work->fontData[*s - work->firstChar].x1 - work->fontData[*s - work->firstChar].x0);
            a = work->fontData[*s - work->firstChar].advance_int;
            if(((workwidth + a) > maxwidth) || ((workwidth + w) > maxwidth)) {
                if(lastp == NULL) {
                    // The text is too long for a single line, so force a split.
                    break;
                }
                // Back this truck up
                lastp++;
                while((*lastp != '\0') && (isspace(*lastp))) {
                    lastp++;
                }
                s = lastp;
                pos = lasts;
                break;
            }
            workwidth += a;
            pos++;
            if(isspace(*s)) {
                lastp = s;
                lasts = pos;
            }
        }
    }

	if(next != NULL) {
		*next = s;
	}
	if(width != NULL) {
		*width = workwidth;
	}
	return pos;
}

int fontGetStrLenWrapGroup(font_t *font, char *s, int maxwidth, int *lineStarts,
                           int *lineLens, int groupLen) {
    int pos = 0;
    int last = 0;
    int count = 0;

    while((s[pos] != '\0') && (groupLen)) {
        lineStarts[count] = pos;
        pos = fontGetStrLenWrap(font, (s + pos), maxwidth);
        lineLens[count] = pos - last;
        count++;
        groupLen--;
    }
    return count;
}

int fontGetStrLenWordWrapGroup(font_t *font, char *s, int maxwidth,
                               int *lineStarts, int *lineLens,
                               int groupLen) {
    int pos = 0;
    int last = 0;
    int count = 0;

    while((s[pos] != '\0') && (groupLen)) {
        lineStarts[count] = pos;
        pos = fontGetStrLenWordWrap(font, (s + pos), maxwidth, NULL, NULL);
        lineLens[count] = pos - last;
        count++;
        groupLen--;
    }
    return count;
}

int fontGetStrLenQCWrap(font_t *font, char *s, int maxwidth, int *strlen,
                        float *red, float *green, float *blue) {
    flubStbFont_t *work = &(((fontIndex_t *)font)->stbfont);
    int w;
    int a;
	int width = 0, size, pos = 0, len;
    flubColor4f_t color;

    if(font == NULL) { 
        return 0;
    }
    
	for(len = 0; s[len] != '\0'; len++) {
		if(s[len] == '^') {
			len++;
			if(s[len] == 0) {
				*strlen = len;
				return pos;
			} else if(s[len] !='^') {
				flubColorQCCodeGet(s[len], &color);
                *red = color.red;
                *green = color.green;
                *blue = color.blue;
				continue;
			}
		}
        w = (work->fontData[s[len] - work->firstChar].x1 - work->fontData[s[len] - work->firstChar].x0);
        a = work->fontData[s[len] - work->firstChar].advance_int;
		if(((width + a) > maxwidth) || ((width + w) > maxwidth)) {
			*strlen = len;
			return pos;
		}
		width += a;
		pos++;
	}
	*strlen = len;
	return pos;
}

int fontGetStrLenQCWordWrap(font_t *font, char *s, int maxwidth, int *strlen,
                            char **next, int *width,
                            float *red, float *green, float *blue) {
    flubStbFont_t *work = &(((fontIndex_t *)font)->stbfont);
    int w;
    int a;
    char *ptr, *lastp = NULL;
    int workwidth = 0, size, pos = 0, lasts = 0;
    flubColor4f_t color;

    if(font == NULL) { 
        return 0;
    }
    
    for(ptr = s; *ptr != '\0'; ptr++) {
        if(*ptr == '^') {
            ptr++;
            if(*ptr == '\0') {
                *strlen = pos;
                return pos;
            } else if(*ptr !='^') {
                flubColorQCCodeGet(*ptr, &color);
                *red = color.red;
                *green = color.green;
                *blue = color.blue;
                continue;
            }
        }
        w = (work->fontData[*ptr - work->firstChar].x1 - work->fontData[*ptr - work->firstChar].x0);
        a = work->fontData[*ptr - work->firstChar].advance_int;
        if(((workwidth + a) > maxwidth) || ((workwidth + w) > maxwidth)) {
            if(lastp == NULL) {
                // The text is too long for a single line, so force a split.
                break;
            }
            // Back this truck up
            lastp++;
            while((*lastp != '\0') && (isspace(*lastp))) {
                lastp++;
            }
            ptr = lastp;
            pos = lasts;
            break;
        }
        workwidth += a;
        pos++;
        if(isspace(ptr[1])) {
            lastp = ptr;
            lasts = pos;
        }
    }
    
    if(next != NULL) {
        *next = ptr;
    }
    if(width != NULL) {
        *width = workwidth;
    }
    return pos;
}

void fontSetColor(flubColor4f_t *color) {
    float alpha;

    gfxColorSet(color->red, color->green, color->blue);
    alpha = color->alpha;
    color->alpha = fontCtx.alpha;
    flubColorCopy(color, &(fontCtx.color));
    color->alpha = alpha;
}

void fontSetColorByVal(float red, float green, float blue) {
    flubColor4f_t color;

    color.red = red;
    color.green = green;
    color.blue = blue;

    fontSetColor(&color);
}

void fontSetColorAlpha(flubColor4f_t *color) {
    gfxColorSet(color->red, color->green, color->blue);
    gfxAlphaSet(color->alpha);
    flubColorCopy(color, &(fontCtx.color));
}

void fontSetColorAlphaByVal(float red, float green, float blue, float alpha) {
    flubColor4f_t color;

    color.red = red;
    color.green = green;
    color.blue = blue;
    color.alpha = alpha;

    fontSetColorAlpha(&color);
}

void fontGetColor(flubColor4f_t *color) {
    flubColorCopy(&(fontCtx.color), color);
}

void fontBlitC(const font_t *font, char c) {
    fontIndex_t *work = (fontIndex_t *)font;

    if(font == NULL) { 
        return;
    }

    if((c < work->stbfont.firstChar) || (c > (work->stbfont.firstChar + work->stbfont.numChars))) {
        return;
    }

	glListBase(work->list);
	
	glCallLists(1, GL_UNSIGNED_BYTE, &c);

    fontCtx.x += work->stbfont.fontData[c - work->stbfont.firstChar].advance_int;
}

void fontBlitStr(const font_t *font, const char *s) {
    fontIndex_t *work = (fontIndex_t *)font;

    if(font == NULL) { 
        return;
    }
    
	glListBase(((fontIndex_t *)font)->list);
	
	// Write The Text To The Screen
	glCallLists(strlen(s), GL_UNSIGNED_BYTE, s);

    for(; *s != '\0'; s++) {
        if((*s < work->stbfont.firstChar) || (*s > (work->stbfont.firstChar + work->stbfont.numChars))) {
            continue;
        }
        fontCtx.x += work->stbfont.fontData[*s - work->stbfont.firstChar].advance_int;
    }
}

void fontBlitStrN(font_t *font, const char *s, int len) {
    fontIndex_t *work = (fontIndex_t *)font;
	int count;

    if(font == NULL) { 
        return;
    }
    
	glListBase(((fontIndex_t *)font)->list);
	
	count = strlen(s);
	if(count > len) {
		count = len;
	}
	// Write The Text To The Screen
	glCallLists(count, GL_UNSIGNED_BYTE, s);

    for(; ((*s != '\0') && (count)); s++, count--) {
        if((*s < work->stbfont.firstChar) || (*s > (work->stbfont.firstChar + work->stbfont.numChars))) {
            continue;
        }
        fontCtx.x += work->stbfont.fontData[*s - work->stbfont.firstChar].advance_int;
    }
}

void fontBlitStrf(const font_t *font, char *fmt, ...) {
	char buf[1024];
	va_list ap;

    if(font == NULL) { 
        return;
    }
    
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	buf[sizeof(buf) - 1] = '\0';

	fontBlitStr(font, buf);
}

void fontBlitQCStr(font_t *font, char *s) {
    fontIndex_t *work = (fontIndex_t *)font;
    flubColor4f_t color;
    float red, green, blue;
	int run, k;
	char *ptr;

    if(font == NULL) { 
        return;
    }
    
	glListBase(((fontIndex_t *)font)->list);
	
	if((ptr = strchr(s, '^')) == NULL) {
		// Write The Text To The Screen
		glCallLists(strlen(s), GL_UNSIGNED_BYTE, s);

        for(; *s != '\0'; s++) {
            if((*s < work->stbfont.firstChar) || (*s > (work->stbfont.firstChar + work->stbfont.numChars))) {
                continue;
            }
            fontCtx.x += work->stbfont.fontData[*s - work->stbfont.firstChar].advance_int;
        }
	} else {
		do {
			if((run = ptr - s) != 0) {
				glCallLists(run, GL_UNSIGNED_BYTE, s);

                for(k = 0; k < run; k++) {
                    if((s[k] < work->stbfont.firstChar) || (s[k] > (work->stbfont.firstChar + work->stbfont.numChars))) {
                        continue;
                    }
                    fontCtx.x += work->stbfont.fontData[s[k] - work->stbfont.firstChar].advance_int;
                }
			}
			s = ptr + 1;
			if(*s == '^') {
				glCallLists(1, GL_UNSIGNED_BYTE, s);
                if((*s >= work->stbfont.firstChar) && (*s <= (work->stbfont.firstChar + work->stbfont.numChars))) {
                    fontCtx.x += work->stbfont.fontData[*s - work->stbfont.firstChar].advance_int;
                }
			} else if(*s == '\0') {
				break;
			} else {
                flubColorQCCodeGet(*s, &color);
                fontSetColor(&color);
			}
			s++;
		} while((ptr = strchr(s, '^')) != NULL);
		if(*s != '\0') {
			glCallLists(strlen(s), GL_UNSIGNED_BYTE, s);

            for(; *s != '\0'; s++) {
                if((*s < work->stbfont.firstChar) || (*s < (work->stbfont.firstChar + work->stbfont.numChars))) {
                    continue;
                }
                fontCtx.x += work->stbfont.fontData[*s - work->stbfont.firstChar].advance_int;
            }
		}
	}
}

void fontBlitInt(font_t *font, int num) {
    fontIndex_t *work = (fontIndex_t *)font;
	char c;
	int test, digits;

    if(font == NULL) { 
        return;
    }
    
	glListBase(((fontIndex_t *)font)->list);

	for(test = 100000000, digits = 9; num < test; test /= 10, digits--);

	if(digits == 0) {
		c = '0';
		glCallLists(1, GL_UNSIGNED_BYTE, &c);

        if((c >= work->stbfont.firstChar) || (c <= (work->stbfont.firstChar + work->stbfont.numChars))) {
            fontCtx.x += work->stbfont.fontData[c - work->stbfont.firstChar].advance_int;
        }
	} else {
		while(digits) {
			for(c = '0'; num >= test; num -= test, c++);
			glCallLists(1, GL_UNSIGNED_BYTE, &c);
			test /= 10;
			digits--;

            if((c < work->stbfont.firstChar) || (c > (work->stbfont.firstChar + work->stbfont.numChars))) {
                continue;
            }
            fontCtx.x += work->stbfont.fontData[c - work->stbfont.firstChar].advance_int;
		}
	}
}

void fontBlitFloat(font_t *font, float num, int decimals) {
    char buf[32];

    if(font == NULL) { 
        return;
    }
    
	glListBase(((fontIndex_t *)font)->list);

    snprintf(buf, sizeof(buf), "%.*f", decimals, num);
    buf[sizeof(buf) - 1] = '\0';
    fontBlitStr(font, buf);
}

///////////////////////////////////////////////////////////////////////////////
// Mesh enabled font blitters
///////////////////////////////////////////////////////////////////////////////

//#define BLIT_QUAD_TO_MESH(m,g)  gfxMeshQuadColor2((m), fontCtx.x + (g)->x0, fontCtx.y + (g)->y0, fontCtx.x + (g)->x1, fontCtx.y + (g)->y1, \
//                                                 (g)->s0, (g)->t0, (g)->s1, (g)->t1,    \
//                                                 fontCtx.red, fontCtx.green, fontCtx.blue, fontCtx.alpha)

#define BLIT_QUAD_TO_MESH(m,g)  gfxBlitQuadToMesh((m),  fontCtx.x + (g)->x0, fontCtx.y + (g)->y0, fontCtx.x + (g)->x1, fontCtx.y + (g)->y1, \
                                                 (g)->s0, (g)->t0, (g)->s1, (g)->t1)


void fontBlitCMesh(gfxMeshObj_t *mesh, font_t *font, char c) {
    fontIndex_t *work = (fontIndex_t *)font;
    stb_fontchar *glyph;

    mesh->program = fontCtx.stbfontProgram;

    if((font == NULL) || (mesh == NULL)) { 
        return;
    }

    if((c >= work->stbfont.firstChar) || (c <= (work->stbfont.firstChar + work->stbfont.numChars))) {
        glyph = &(work->stbfont.fontData[c - work->stbfont.firstChar]);
        BLIT_QUAD_TO_MESH(mesh, glyph);
        fontCtx.x += glyph->advance_int;
    }
}


void fontBlitStrMesh(gfxMeshObj_t *mesh, font_t *font, const char *s) {
    fontIndex_t *work = (fontIndex_t *)font;
    stb_fontchar *glyph;

    mesh->program = fontCtx.stbfontProgram;

    if((font == NULL) || (mesh == NULL)) {
        return;
    }

    for(; *s != '\0'; s++) {
        if((*s < work->stbfont.firstChar) || (*s > (work->stbfont.firstChar + work->stbfont.numChars))) {
            continue;
        }
        glyph = &(work->stbfont.fontData[*s - work->stbfont.firstChar]);
        BLIT_QUAD_TO_MESH(mesh, glyph);
        fontCtx.x += glyph->advance_int;
    }
}

void fontBlitStrNMesh(gfxMeshObj_t *mesh, font_t *font, char *s, int len) {
    fontIndex_t *work = (fontIndex_t *)font;
    stb_fontchar *glyph;
    int count;

    mesh->program = fontCtx.stbfontProgram;

    if((font == NULL) || (mesh == NULL)) {
        return;
    }
    
    count = strlen(s);
    if(count > len) {
        count = len;
    }

    for(; ((*s != '\0') && (count)); s++, count--) {
        if((*s < work->stbfont.firstChar) || (*s > (work->stbfont.firstChar + work->stbfont.numChars))) {
            continue;
        }
        glyph = &(work->stbfont.fontData[*s - work->stbfont.firstChar]);
        BLIT_QUAD_TO_MESH(mesh, glyph);
        fontCtx.x += glyph->advance_int;
    }
}

void fontBlitStrfMesh(gfxMeshObj_t *mesh, font_t *font, char *fmt, ...) {
    char buf[1024];
    va_list ap;

    mesh->program = fontCtx.stbfontProgram;

    if((font == NULL) || (mesh == NULL)) {
        return;
    }
    
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    buf[sizeof(buf) - 1] = '\0';

    fontBlitStrMesh(mesh, font, buf);
}

void fontBlitQCStrMesh(gfxMeshObj_t *mesh, font_t *font, char *s) {
    fontIndex_t *work = (fontIndex_t *)font;
    stb_fontchar *glyph;
    float red, green, blue;
    int run, k;
    char *ptr;
    flubColor4f_t color;

    mesh->program = fontCtx.stbfontProgram;

    if((font == NULL) || (mesh == NULL)) {
        return;
    }
    
    if((ptr = strchr(s, '^')) == NULL) {
        // Write The Text To The Screen
        for(; *s != '\0'; s++) {
            if((*s < work->stbfont.firstChar) || (*s > (work->stbfont.firstChar + work->stbfont.numChars))) {
                continue;
            }
            glyph = &(work->stbfont.fontData[*s - work->stbfont.firstChar]);
            BLIT_QUAD_TO_MESH(mesh, glyph);
            fontCtx.x += glyph->advance_int;
        }
    } else {
        do {
            if((run = ptr - s) != 0) {
                for(k = 0; k < run; k++) {
                    if((s[k] < work->stbfont.firstChar) || (s[k] > (work->stbfont.firstChar + work->stbfont.numChars))) {
                        continue;
                    }
                    glyph = &(work->stbfont.fontData[s[k] - work->stbfont.firstChar]);
                    BLIT_QUAD_TO_MESH(mesh, glyph);
                    fontCtx.x += glyph->advance_int;
                }
            }
            s = ptr + 1;
            if(*s == '^') {
                if((*s >= work->stbfont.firstChar) && (*s <= (work->stbfont.firstChar + work->stbfont.numChars))) {
                    glyph = &(work->stbfont.fontData[*s - work->stbfont.firstChar]);
                    BLIT_QUAD_TO_MESH(mesh, glyph);
                    fontCtx.x += glyph->advance_int;
                }
            } else if(*s == '\0') {
                break;
            } else {
                flubColorQCCodeGet(*s, &color);
                fontSetColor(&color);
            }
            s++;
        } while((ptr = strchr(s, '^')) != NULL);
        if(*s != '\0') {
            for(; *s != '\0'; s++) {
                if((*s < work->stbfont.firstChar) || (*s > (work->stbfont.firstChar + work->stbfont.numChars))) {
                    continue;
                }
                glyph = &(work->stbfont.fontData[*s - work->stbfont.firstChar]);
                BLIT_QUAD_TO_MESH(mesh, glyph);
                fontCtx.x += glyph->advance_int;
            }
        }
    }
}

void fontBlitIntMesh(gfxMeshObj_t *mesh, font_t *font, int num) {
    fontIndex_t *work = (fontIndex_t *)font;
    stb_fontchar *glyph;
    char c;
    int test, digits;

    mesh->program = fontCtx.stbfontProgram;

    if((font == NULL) || (mesh == NULL)) {
        return;
    }
    
    for(test = 100000000, digits = 9; num < test; test /= 10, digits--);

    if(digits == 0) {
        c = '0';

        if((c >= work->stbfont.firstChar) || (c <= (work->stbfont.firstChar + work->stbfont.numChars))) {
            glyph = &(work->stbfont.fontData[c - work->stbfont.firstChar]);
            BLIT_QUAD_TO_MESH(mesh, glyph);
            fontCtx.x += work->stbfont.fontData[c - work->stbfont.firstChar].advance_int;
        }
    } else {
        while(digits) {
            for(c = '0'; num >= test; num -= test, c++);
            if((c >= work->stbfont.firstChar) || (c <= (work->stbfont.firstChar + work->stbfont.numChars))) {
                glyph = &(work->stbfont.fontData[c - work->stbfont.firstChar]);
                BLIT_QUAD_TO_MESH(mesh, glyph);
                fontCtx.x += work->stbfont.fontData[c - work->stbfont.firstChar].advance_int;
            }
            test /= 10;
            digits--;
        }
    }
}

void fontBlitFloatMesh(gfxMeshObj_t *mesh, font_t *font, float num, int decimals) {
    char buf[32];

    mesh->program = fontCtx.stbfontProgram;

    if((font == NULL) || (mesh == NULL)) {
        return;
    }
    
    snprintf(buf, sizeof(buf), "%.*f", decimals, num);
    buf[sizeof(buf) - 1] = '\0';
    fontBlitStrMesh(mesh, font, buf);
}

void fontMode(void) {
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    glEnable(GL_TEXTURE);
}

void fontPos(int x, int y) {
    glLoadIdentity();
    glTranslated(x, y, 0);
    fontCtx.x = x;
    fontCtx.y = y;
}

texture_t *fontTextureGet(font_t *font) {
    fontIndex_t *work = (fontIndex_t *)font;

    return work->texture;
}
