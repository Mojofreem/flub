#include <flub/theme.h>
#include <flub/gfx.h>
#include <flub/font.h>
#include <flub/texture.h>
#include <flub/logger.h>
#include <stdlib.h>
#include <string.h>
#include <flub/memory.h>
#include <flub/texture.h>
#include <ctype.h>
#include <physfs.h>
#include <flub/physfsutil.h>
#include <flub/util/parse.h>
#include <flub/3rdparty/jsmn/jsmn.h>
#include <flub/util/jsmn.h>
#include <flub/util/enum.h>
#include <flub/util/string.h>
#include <stdio.h>
#include <flub/util/color.h>

#define FLUB_THEME_TOK_NUM      512
#define FLUB_GUI_THEME_REV			1
#define DBG_GUI_DTL_THEME          1
#define FLUB_THEME_MAX_ANIM_TICKS   (60 * 1000)

/*
#define FLUB_GUI_THEME_MAX_LINE_LEN	1024

#define FLUB_THEME_MAX_STR_LEN  64
*/

struct {
    int init;
    flubGuiTheme_t *themes;
} _guiThemeCtx = {
        .init = 0,
        .themes = NULL,
};

utilEnumMap_t _themePartMap[] = {
    {eFlubGuiBitmap,     "bitmap"},
    {eFlubGuiAnimBitmap, "animation"},
    {eFlubGuiTile,       "tile"},
    {eFlubGuiSlice,      "slice"},
    {eFlubGuiAnimSlice,  "animslice"},
    END_ENUM_MAP()
};

const char *themePartStr(eFlubGuiThemeFragmentType_t part) {
    return utilEnum2Str(_themePartMap, part, "<unknown>");
}

eFlubGuiThemeFragmentType_t themeStrToPart(const char *str) {
    return utilStr2Enum(_themePartMap, str, eFlubGuiBitmap);
}

static void _flubGuiThemeFree(flubGuiTheme_t *theme) {
    flubGuiTheme_t *walk;

    if(_guiThemeCtx.themes == theme) {
        _guiThemeCtx.themes = theme->next;
    } else if(_guiThemeCtx.themes != NULL) {
        for(walk = _guiThemeCtx.themes; ((walk->next != NULL) && (walk->next != theme)); walk = walk->next);
        if(walk->next == theme) {
            walk->next = theme->next;
        }
    }

    // TODO

    util_free(theme);
}

static void _flubGuiThemeCleanup(flubGuiTheme_t *theme) {
    // TODO
}

int flubGuiThemeInit(void) {
    if(_guiThemeCtx.init) {
        error("Cannot initialize the theme module multiple times.");
        return 1;
    }

    logDebugRegister("gui", DBG_GUI, "theme", DBG_GUI_DTL_THEME);

    _guiThemeCtx.init = 1;

    return 1;
}

int flubGuiThemeValid(void) {
    return _guiThemeCtx.init;
}

void flubGuiThemeShutdown(void) {
    if(!_guiThemeCtx.init) {
        return;
    }

    flubGuiTheme_t *walk, *next;

    for(walk = _guiThemeCtx.themes; walk != NULL; walk = next) {
        next = walk->next;
        _flubGuiThemeFree(walk);
    }

    _guiThemeCtx.themes = NULL;
    _guiThemeCtx.init = 0;
}

static void _flubGuiThemeRegister(flubGuiTheme_t *theme) {
    theme->next = _guiThemeCtx.themes;
    _guiThemeCtx.themes = theme;

    _flubGuiThemeCleanup(theme);
}

static void _flubGuiThemeIndexAdd(flubGuiTheme_t *theme, int id, void *ptr) {
    int count;

    count = (((id + 1) / 1024) + 1) * 1024;

    if(theme->idMap == NULL) {
        debug(DBG_GUI, DBG_GUI_DTL_THEME, "Initializing gui theme index to 1024 entries.");
        theme->idMap = util_calloc((sizeof(void *) * count), 0, NULL);
        theme->maxId = count;
    } else if(id >= theme->maxId) {
        debugf(DBG_GUI, DBG_GUI_DTL_THEME, "Resizing gui theme index to %d entries.", theme->maxId);
        theme->idMap = util_calloc((sizeof(void *) * count), theme->maxId, theme->idMap);
        theme->maxId = count;
    }

    theme->idMap[id] = ptr;
}

#define MAX_FLUB_THEME_PRE_MSG_LEN  64

static const char *_jsonPreMsg(const char *part, const char *name) {
    static char buf[MAX_FLUB_THEME_PRE_MSG_LEN];

    snprintf(buf, sizeof(buf), "Theme %s \"%s\" entry", part, name);

    return buf;
}

typedef struct _jsonColorEntry_s {
    int id;
    char *color;
} _jsonColorEntry_t;

static int _jsonColorValidate(const char *str) {
    return parseColor(str, NULL, NULL, NULL, NULL);
}

static void _jsonColorCleanup(void *data) {
    _jsonColorEntry_t *entry = (_jsonColorEntry_t *)data;

    if(entry->color != NULL) {
        util_free(entry->color);
        entry->color = NULL;
    }
}

jsonParseMap_t _themeColorParseMap[] = {
    {
        .name     = "id",
        .type     = eJsonTypeInt,
        .offset   = offsetof(_jsonColorEntry_t, id),
        .required = 1,
    },
    {
        .name = "color",
        .type = eJsonTypeString,
        .offset = offsetof(_jsonColorEntry_t, color),
        .required = 1,
        .stringValidator = _jsonColorValidate,
        .failMsg = "is not a valid color",
    },
    END_JSON_PARSE_MAP(),
};

static int _themeParseColorEntry(const char *json, jsmntok_t *tokens, const char *name, void *context) {
    flubGuiTheme_t *theme = (flubGuiTheme_t *)context;
    _jsonColorEntry_t entry;
    flubGuiThemeColor_t *color;
    int red;
    int green;
    int blue;

    memset(&entry, 0, sizeof(_jsonColorEntry_t));
    if(!jsonParseToStruct(json, tokens,_themeColorParseMap, "Theme color entry", &entry, _jsonColorCleanup)) {
        return 0;
    }

    color = util_calloc(sizeof(flubGuiThemeColor_t), 0, NULL);
    color->type = eFlubGuiThemeColor;
    color->id = entry.id;
    parseColor(entry.color, &red, &green, &blue, NULL);
    color->red = COLOR_ITOF(red);
    color->green = COLOR_ITOF(green);
    color->blue = COLOR_ITOF(blue);
    critbitInsert(&theme->colors, name, color, NULL);
    _flubGuiThemeIndexAdd(theme, entry.id, color);
    _jsonColorCleanup(&entry);
    debugf(DBG_GUI, DBG_GUI_DTL_THEME, "Found color \"%s\" as %d, value #%02x%02x%02x", name, entry.id, red, green, blue);
    return 1;
}

typedef struct _jsonFontEntry_s {
    int id;
    char *filename;
    char *font;
    int size;
} _jsonFontEntry_t;

static void _jsonFontCleanup(void *data) {
    _jsonFontEntry_t *entry = (_jsonFontEntry_t *)data;

    if(entry->filename != NULL) {
        util_free(entry->filename);
        entry->filename;
    }
    if(entry->font != NULL) {
        util_free(entry->font);
        entry->font = NULL;
    }
}

jsonParseMap_t _themeFontParseMap[] = {
        {
                .name = "id",
                .type = eJsonTypeInt,
                .offset = offsetof(_jsonFontEntry_t, id),
                .required = 1,
        },
        {
                .name = "file",
                .type = eJsonTypeString,
                .offset = offsetof(_jsonFontEntry_t, filename),
                .required = 1,
        },
        {
                .name = "font",
                .type = eJsonTypeString,
                .offset = offsetof(_jsonFontEntry_t, font),
                .required = 1,
        },
        {
                .name = "size",
                .type = eJsonTypeInt,
                .offset = offsetof(_jsonFontEntry_t, size),
                .required = 1,
        },
        END_JSON_PARSE_MAP(),
};

static int _themeParseFontEntry(const char *json, jsmntok_t *tokens, const char *name, void *context) {
    flubGuiTheme_t *theme = (flubGuiTheme_t *)context;
    _jsonFontEntry_t entry;
    flubGuiThemeFont_t *font;

    memset(&entry, 0, sizeof(_jsonFontEntry_t));
    if(!jsonParseToStruct(json, tokens, _themeFontParseMap, "Theme font entry", &entry, _jsonFontCleanup)) {
        return 0;
    }
    font = util_calloc(sizeof(flubGuiThemeFont_t), 0, NULL);
    font->type = eFlubGuiThemeFont;
    font->id = entry.id;
    if(entry.filename != NULL) {
        if(!flubFontLoad(entry.filename)) {
            _jsonFontCleanup(&entry);
            util_free(font);
            return 0;
        }
    }
    if((font->font = fontGet(entry.font, entry.size, 0)) == NULL) {
        _jsonFontCleanup(&entry);
        util_free(font);
        return 0;
    }
    critbitInsert(&theme->fonts, name, font, NULL);
    _flubGuiThemeIndexAdd(theme, entry.id, font);
    debugf(DBG_GUI, DBG_GUI_DTL_THEME, "Found font \"%s\" as %d, file [%s], %s size %d", name, entry.id,
          entry.filename, entry.font, entry.size);
    _jsonFontCleanup(&entry);
    return 1;
}

typedef struct _jsonTextureEntry_s {
    int id;
    char *filename;
    int minfilter;
    int magfilter;
    char *colorkey;
} _jsonTextureEntry_t;

static int _jsonGLMinfilterConvert(const char *str, int *val, float *valf) {
    if(val != NULL) {
        *val = texmgrStrToGLMinFilter(str);
    }
    return 1;
}

static int _jsonGLMagfilterConvert(const char *str, int *val, float *valf) {
    if(val != NULL) {
        *val = texmgrStrToGLMagFilter(str);
    }
    return 1;
}

static void _jsonTextureCleanup(void *data) {
    _jsonTextureEntry_t *entry = (_jsonTextureEntry_t *)data;

    if(entry->filename != NULL) {
        util_free(entry->filename);
        entry->filename = NULL;
    }
}

jsonParseMap_t _themeTextureParseMap[] = {
        {
                .name = "id",
                .type = eJsonTypeInt,
                .offset = offsetof(_jsonTextureEntry_t, id),
                .required = 1,
        },
        {
                .name = "filename",
                .type = eJsonTypeString,
                .offset = offsetof(_jsonTextureEntry_t, filename),
                .required = 1,
        },
        {
                .name = "minfilter",
                .type = eJsonTypeString,
                .offset = offsetof(_jsonTextureEntry_t, minfilter),
                .defInt = GL_NEAREST,
                .stringValidator = texmgrStrToGLMinFilter,
                .convertType = eJsonTypeInt,
                .stringConvert = _jsonGLMinfilterConvert,
                .failMsg = "is not a valid OpenGL minfilter",
        },
        {
                .name = "magfilter",
                .type = eJsonTypeString,
                .offset = offsetof(_jsonTextureEntry_t, magfilter),
                .defInt = GL_NEAREST,
                .stringValidator = texmgrStrToGLMagFilter,
                .convertType = eJsonTypeInt,
                .stringConvert = _jsonGLMagfilterConvert,
                .failMsg = "is not a valid OpenGL magfilter",
        },
        {
                .name = "colorkey",
                .type = eJsonTypeString,
                .offset = offsetof(_jsonTextureEntry_t, colorkey),
                .stringValidator = _jsonColorValidate,
                .failMsg = "is not a valid color",
        },
        END_JSON_PARSE_MAP(),
};

static int _themeParseTextureEntry(const char *json, jsmntok_t *tokens, const char *name, void *context) {
    flubGuiTheme_t *theme = (flubGuiTheme_t *)context;
    _jsonTextureEntry_t entry;
    flubGuiThemeTexture_t *texture;
    int red = 0;
    int green = 0;
    int blue = 0;

    memset(&entry, 0, sizeof(_jsonTextureEntry_t));
    if(!jsonParseToStruct(json, tokens, _themeTextureParseMap, "Theme texture entry", &entry, _jsonTextureCleanup)) {
        return 0;
    }
    texture = util_calloc(sizeof(flubGuiThemeTexture_t), 0, NULL);
    texture->type = eFlubGuiThemeTexture;
    texture->id = entry.id;
    if(entry.colorkey != NULL) {
        parseColor(entry.colorkey, &red, &green, &blue, NULL);
    }
    if((texture->texture = texmgrLoad(entry.filename, NULL, entry.minfilter,
                                      entry.magfilter,
                                      ((entry.colorkey != NULL) ? 1 : 0),
                                      red, green, blue)) == NULL) {
        _jsonTextureCleanup(&entry);
        util_free(texture);
        return 0;
    }
    critbitInsert(&theme->textures, name, texture, NULL);
    _flubGuiThemeIndexAdd(theme, entry.id, texture);
    debugf(DBG_GUI, DBG_GUI_DTL_THEME, "Found texture \"%s\" as %d, file [%s], %s %s %s", name, entry.id,
          entry.filename, texmgrGLMinFilterStr(entry.minfilter),
          texmgrGLMagFilterStr(entry.magfilter), ((entry.colorkey == NULL) ? "No colorkey" : entry.colorkey));
    _jsonTextureCleanup(&entry);
    return 1;
}

typedef struct _jsonComponentEntry_s {
    int id;
    int maxwidth;
    int minwidth;
    int maxheight;
    int minheight;
    int padleft;
    int padright;
    int padtop;
    int padbottom;
    int marginleft;
    int marginright;
    int margintop;
    int marginbottom;
    int states[16];
} _jsonComponentEntry_t;

jsonParseMap_t _themeComponentParseMap[] = {
        {
                .name = "id",
                .type = eJsonTypeInt,
                .offset = offsetof(_jsonComponentEntry_t, id),
                .required = 1,
        },
        {
                .name = "maxwidth",
                .type = eJsonTypeInt,
                .offset = offsetof(_jsonComponentEntry_t, maxwidth),
        },
        {
                .name = "minwidth",
                .type = eJsonTypeInt,
                .offset = offsetof(_jsonComponentEntry_t, minwidth),
        },
        {
                .name = "maxheight",
                .type = eJsonTypeInt,
                .offset = offsetof(_jsonComponentEntry_t, maxheight),
        },
        {
                .name = "minheight",
                .type = eJsonTypeInt,
                .offset = offsetof(_jsonComponentEntry_t, minheight),
        },
        {
                .name = "padleft",
                .type = eJsonTypeInt,
                .offset = offsetof(_jsonComponentEntry_t, padleft),
        },
        {
                .name = "padright",
                .type = eJsonTypeInt,
                .offset = offsetof(_jsonComponentEntry_t, padright),
        },
        {
                .name = "padtop",
                .type = eJsonTypeInt,
                .offset = offsetof(_jsonComponentEntry_t, padtop),
        },
        {
                .name = "padbottom",
                .type = eJsonTypeInt,
                .offset = offsetof(_jsonComponentEntry_t, padbottom),
        },
        {
                .name = "marginleft",
                .type = eJsonTypeInt,
                .offset = offsetof(_jsonComponentEntry_t, marginleft),
        },
        {
                .name = "marginright",
                .type = eJsonTypeInt,
                .offset = offsetof(_jsonComponentEntry_t, marginright),
        },
        {
                .name = "margintop",
                .type = eJsonTypeInt,
                .offset = offsetof(_jsonComponentEntry_t, margintop),
        },
        {
                .name = "marginbottom",
                .type = eJsonTypeInt,
                .offset = offsetof(_jsonComponentEntry_t, marginbottom),
        },
        END_JSON_PARSE_MAP(),
};

#define FLUB_GUI_THEME_DISABLED_PLACEHOLDER 0x0

typedef struct _jsonComponentStateEntry_s {
    int mode;
    int fragment;
} _jsonComponentStateEntry_t;

static int _jsonComponentStateModeConvertor(const char *str, int *val, float *valf) {
    int lens[10];
    const char *fields[10];
    const char *ptr;
    int count;
    int k, j;
    int value = 0;
    static const struct {
        const char *name;
        int value;
    } fieldMap[] = {
            { "disable", FLUB_GUI_THEME_DISABLED_PLACEHOLDER },
            { "enable",  FLUB_GUI_THEME_ENABLED },
            { "hover",   FLUB_GUI_THEME_HOVER },
            { "focus",   FLUB_GUI_THEME_FOCUS },
            { "pressed", FLUB_GUI_THEME_PRESSED },
            { NULL,      0 },
    };

    count = strFieldsConst(str, '|', fields, lens, 10);
    if(count == 0) {
        return 0;
    }
    for(k = 0; k < count; k++) {
        ptr = fields[k];
        while(isspace(*ptr)) {
            ptr++;
        }
        for(j = 0; fieldMap[j].name != NULL; j++) {
            if(!strncasecmp(ptr, fieldMap[j].name, strlen(fieldMap[j].name))) {
                value |= fieldMap[j].value;
                break;
            }
        }
        if(fieldMap[j].name == NULL) {
            errorf("Theme component state mode \"%.*s\" is not valid", lens[k], fields[k]);
            return 0;
        }
    }
    if(val != NULL) {
        *val = value;
    }
    return 1;
}

static int _jsonComponentStateModeValidator(const char *str) {
    return _jsonComponentStateModeConvertor(str, NULL, NULL);
}

jsonParseMap_t _themeComponentStateParseMap[] = {
        {
                .name = "mode",
                .type = eJsonTypeString,
                .offset = offsetof(_jsonComponentStateEntry_t, mode),
                .required = 1,
                .stringValidator = _jsonComponentStateModeValidator,
                .convertType = eJsonTypeInt,
                .stringConvert = _jsonComponentStateModeConvertor,
                .failMsg = "is not a valid state",
        },
        {
                .name = "fragid",
                .type = eJsonTypeInt,
                .offset = offsetof(_jsonComponentStateEntry_t, fragment),
                .required = 1,
        },
        END_JSON_PARSE_MAP(),
};

static int _themeParseComponentStateEntry(const char *json, jsmntok_t *tokens, const char *name, void *context) {
    _jsonComponentStateEntry_t entry;

    memset(&entry, 0, sizeof(_jsonComponentStateEntry_t));
    if(!jsonParseToStruct(json, tokens, _themeComponentStateParseMap, "Theme component state entry", &entry, NULL)) {
        return 0;
    }
    ((_jsonComponentEntry_t *)context)->states[entry.mode] = entry.fragment;
    debugf(DBG_GUI, DBG_GUI_DTL_THEME, "Found component state for mode %d, using fragment %d",
          entry.mode, entry.fragment);
}

static int _themeParseComponentEntry(const char *json, jsmntok_t *tokens, const char *name, void *context) {
    flubGuiTheme_t *theme = (flubGuiTheme_t *)context;
    _jsonComponentEntry_t entry;
    flubGuiThemeComponents_t *component;
    int k;
    int id;
    flubGuiThemeFragment_t *fragment;

    memset(&entry, 0, sizeof(_jsonComponentEntry_t));
    if(!jsonParseToStruct(json, tokens, _themeComponentParseMap, "Theme component entry", &entry, NULL)) {
        return 0;
    }
    debugf(DBG_GUI, DBG_GUI_DTL_THEME, "Found component \"%s\" as %d, max (%dx%d) min(%dx%d) pad(%d,%d,%d,%d) margin(%d,%d,%d,%d)",
          name, entry.id,
          entry.maxwidth, entry.maxheight, entry.minwidth, entry.minheight,
          entry.padleft, entry.padtop, entry.padright, entry.padbottom,
          entry.marginleft, entry.margintop, entry.marginright, entry.marginbottom);

    component = util_calloc(sizeof(flubGuiThemeComponents_t), 0, NULL);
    component->type = eFlubGuiThemeComponent;
    component->id = entry.id;
    component->minWidth = entry.minwidth;
    component->maxWidth = entry.maxwidth;
    component->minHeight = entry.minheight;
    component->maxHeight = entry.maxheight;
    component->padLeft = entry.padleft;
    component->padRight = entry.padright;
    component->padTop = entry.padtop;
    component->padBottom = entry.padbottom;
    component->marginLeft = entry.marginleft;
    component->marginRight = entry.marginright;
    component->marginTop = entry.margintop;
    component->marginBottom = entry.marginbottom;

    if(!jsonParseObject(json, jsmnObjArrayToken(json, tokens, "states"),
                        _themeParseComponentStateEntry, (void *)(&entry))) {
        util_free(component);
        return 0;
    }

    for(k = 0; k < 16; k++) {
        id = entry.states[k];
        if(id == 0) {
            continue;
        }
        if((id >= theme->maxId) || (theme->idMap[id] == NULL)) {
            errorf("Theme component \"%s\" state (%d) specifies an invalid fragment id %d", name, k, id);
            util_free(component);
            return 0;
        }
        fragment = (flubGuiThemeFragment_t *)(theme->idMap[id]);
        if(fragment->type != eFlubGuiThemeFragment) {
            errorf("Theme component \"%s\" state (%d) specifies fragment id %d, but it is not a fragment",
                   name, k, id);
            util_free(component);
            return 0;
        }
        component->fragmentIndex[k] = fragment;
    }

    critbitInsert(&theme->components, name, component, NULL);
    _flubGuiThemeIndexAdd(theme, entry.id, component);

    return 1;
}

utilEnumMap_t _fragmentTypeMap[] = {
        { eFlubGuiBitmap,     "bitmap" },
        { eFlubGuiTile,       "tile" },
        { eFlubGuiSlice,      "slice" },
        { eFlubGuiAnimBitmap, "animation" },
        { eFlubGuiAnimSlice,  "animslice" },
        END_ENUM_MAP(),
};

typedef struct _jsonFragmentEntry_s {
    int id;
    int type;
    int texid;
    int width;
    int height;
    int x1;
    int y1;
    int x2;
    int y2;
    int x3;
    int y3;
    int x4;
    int y4;
} _jsonFragmentEntry_t;

static int _jsonFragmentTypeConversion(const char *str, int *val, float *valf) {
    int value;

    if((value = utilStr2Enum(_fragmentTypeMap, str, -1)) < 0) {
        return 0;
    }
    if(val != NULL) {
        *val = value;
    }
    return 1;
}

static int _jsonFragmentTypeValidator(const char *str) {
    return ((_jsonFragmentTypeConversion(str, NULL, NULL) >= 0) ? 1 : 0);
}

static int _jsonFragmentTextidValidator(int texid) {
    // TODO
    return 1;
}

jsonParseMap_t _themeFragmentParseMap[] = {
        {
                .name = "id",
                .type = eJsonTypeInt,
                .offset = offsetof(_jsonFragmentEntry_t, id),
                .required = 1,
        },
        {
                .name = "type",
                .type = eJsonTypeString,
                .offset = offsetof(_jsonFragmentEntry_t, type),
                .required = 1,
                .stringValidator = _jsonFragmentTypeValidator,
                .convertType = eJsonTypeInt,
                .stringConvert = _jsonFragmentTypeConversion,
                .failMsg = "is not a valid fragment type",
        },
        {
                .name = "texid",
                .type = eJsonTypeInt,
                .offset = offsetof(_jsonFragmentEntry_t, texid),
                .required = 1,
                .intValidator = _jsonFragmentTextidValidator,
                .failMsg = "is not a valid texture id",
        },
        {
                .name = "width",
                .type = eJsonTypeInt,
                .offset = offsetof(_jsonFragmentEntry_t, width),
        },
        {
                .name = "height",
                .type = eJsonTypeInt,
                .offset = offsetof(_jsonFragmentEntry_t, height),
        },
        {
                .name = "x1",
                .type = eJsonTypeInt,
                .offset = offsetof(_jsonFragmentEntry_t, x1),
        },
        {
                .name = "y1",
                .type = eJsonTypeInt,
                .offset = offsetof(_jsonFragmentEntry_t, y1),
        },
        {
                .name = "x2",
                .type = eJsonTypeInt,
                .offset = offsetof(_jsonFragmentEntry_t, x2),
        },
        {
                .name = "y2",
                .type = eJsonTypeInt,
                .offset = offsetof(_jsonFragmentEntry_t, y2),
        },
        {
                .name = "x3",
                .type = eJsonTypeInt,
                .offset = offsetof(_jsonFragmentEntry_t, x3),
        },
        {
                .name = "y3",
                .type = eJsonTypeInt,
                .offset = offsetof(_jsonFragmentEntry_t, y3),
        },
        {
                .name = "x4",
                .type = eJsonTypeInt,
                .offset = offsetof(_jsonFragmentEntry_t, x4),
        },
        {
                .name = "y4",
                .type = eJsonTypeInt,
                .offset = offsetof(_jsonFragmentEntry_t, y4),
        },
        END_JSON_PARSE_MAP(),
};

typedef struct _jsonBitmapFrameEntry_s {
    int x;
    int y;
    int delay;
} _jsonBitmapFrameEntry_t;

jsonParseMap_t _themeBitmapFrameParseMap[] = {
        {
                .name = "x",
                .type = eJsonTypeInt,
                .offset = offsetof(_jsonBitmapFrameEntry_t, x),
                .required = 1,
        },
        {
                .name = "y",
                .type = eJsonTypeInt,
                .offset = offsetof(_jsonBitmapFrameEntry_t, y),
                .required = 1,
        },
        {
                .name = "delay",
                .type = eJsonTypeInt,
                .offset = offsetof(_jsonBitmapFrameEntry_t, delay),
                .required = 1,
        },
        END_JSON_PARSE_MAP(),
};

static int _themeParseBitmapFrameEntry(const char *json, jsmntok_t *tokens, const char *name, void *context) {
    flubGuiThemeFragment_t *fragment = (flubGuiThemeFragment_t *)context;
    _jsonBitmapFrameEntry_t entry;
    flubGuiThemeFragmentAnimBitmapFrame_t *frame, *walk;

    memset(&entry, 0, sizeof(_jsonBitmapFrameEntry_t));
    if(!jsonParseToStruct(json, tokens, _themeBitmapFrameParseMap, _jsonPreMsg("bitmap frame", name), &entry, NULL)) {
        return 0;
    }
    frame = util_calloc(sizeof(flubGuiThemeFragmentAnimBitmapFrame_t), 0, NULL);
    frame->x = entry.x;
    frame->y = entry.y;
    frame->delayMs = entry.delay;
    if(fragment->animBitmap.frames == NULL) {
        fragment->animBitmap.frames = frame;
    } else {
        for(walk = fragment->animBitmap.frames; walk->next != NULL; walk = walk->next);
        walk->next = frame;
    }
    debugf(DBG_GUI, DBG_GUI_DTL_THEME, "Found animated bitmap frame, pos (%d,%d), delay %dms",
           frame->x, frame->y, frame->delayMs);
           return 1;
}

typedef struct _jsonAnimSliceFrameEntry_s {
    int x1;
    int y1;
    int x2;
    int y2;
    int x3;
    int y3;
    int x4;
    int y4;
    int delay;
} _jsonAnimSliceFrameEntry_t;

jsonParseMap_t _themeAnimSliceFrameParseMap[] = {
        {
                .name = "x1",
                .type = eJsonTypeInt,
                .offset = offsetof(_jsonAnimSliceFrameEntry_t, x1),
        },
        {
                .name = "y1",
                .type = eJsonTypeInt,
                .offset = offsetof(_jsonAnimSliceFrameEntry_t, y1),
        },
        {
                .name = "x2",
                .type = eJsonTypeInt,
                .offset = offsetof(_jsonAnimSliceFrameEntry_t, x2),
        },
        {
                .name = "y2",
                .type = eJsonTypeInt,
                .offset = offsetof(_jsonAnimSliceFrameEntry_t, y2),
        },
        {
                .name = "x3",
                .type = eJsonTypeInt,
                .offset = offsetof(_jsonAnimSliceFrameEntry_t, x3),
        },
        {
                .name = "y3",
                .type = eJsonTypeInt,
                .offset = offsetof(_jsonAnimSliceFrameEntry_t, y3),
        },
        {
                .name = "x4",
                .type = eJsonTypeInt,
                .offset = offsetof(_jsonAnimSliceFrameEntry_t, x4),
        },
        {
                .name = "y4",
                .type = eJsonTypeInt,
                .offset = offsetof(_jsonAnimSliceFrameEntry_t, y4),
        },
        {
                .name = "delay",
                .type = eJsonTypeInt,
                .offset = offsetof(_jsonAnimSliceFrameEntry_t, delay),
        },
        END_JSON_PARSE_MAP(),
};

static int _themeParseAnimSliceFrameEntry(const char *json, jsmntok_t *tokens, const char *name, void *context) {
    flubGuiThemeFragment_t *fragment = (flubGuiThemeFragment_t *)context;
    _jsonAnimSliceFrameEntry_t entry;
    flubGuiThemeFragmentSliceFrame_t *frame, *walk;

    memset(&entry, 0, sizeof(_jsonAnimSliceFrameEntry_t));
    if(!jsonParseToStruct(json, tokens, _themeAnimSliceFrameParseMap, _jsonPreMsg("slice frame", name), &entry, NULL)) {
        return 0;
    }
    frame = util_calloc(sizeof(flubGuiThemeFragmentSliceFrame_t), 0, NULL);
    frame->delayMs = entry.delay;
    frame->slice = gfxSliceCreate(fragment->animSlice.texture, GFX_SLICE_NOTILE_ALL,
                                  entry.x1, entry.y1,
                                  entry.x2, entry.y2,
                                  entry.x3, entry.y3,
                                  entry.x4, entry.y4);
    frame->delayMs = entry.delay;
    if(fragment->animSlice.frames == NULL) {
        fragment->animSlice.frames = frame;
    } else {
        for(walk = fragment->animSlice.frames; walk->next != NULL; walk = walk->next);
        walk->next = frame;
    }
    debugf(DBG_GUI, DBG_GUI_DTL_THEME, "Found animated slice frame (%d,%d)-(%d,%d)-(%d,%d)-(%d,%d), delay %dms",
           entry.x1, entry.y1, entry.x2, entry.y2, entry.x3, entry.y3,
           entry.x4, entry.y4, frame->delayMs);
    return 1;
}

static int _themeParseFragmentEntry(const char *json, jsmntok_t *tokens, const char *name, void *context) {
    flubGuiTheme_t *theme = (flubGuiTheme_t *)context;
    _jsonFragmentEntry_t entry;
    flubGuiThemeFragment_t *fragment;
    flubGuiThemeFragmentAnimBitmapFrame_t *bmpWalk, *bmpNext;
    flubGuiThemeFragmentSliceFrame_t *sliceWalk, *sliceNext;

    memset(&entry, 0, sizeof(_jsonFragmentEntry_t));

    entry.width = -1;
    entry.height = -1;
    entry.x1 = -1;
    entry.y1 = -1;
    entry.x2 = -1;
    entry.y2 = -1;
    entry.x3 = -1;
    entry.y3 = -1;
    entry.x4 = -1;
    entry.y4 = -1;

    if(!jsonParseToStruct(json, tokens, _themeFragmentParseMap, _jsonPreMsg("fragment", name), &entry, NULL)) {
        return 0;
    }

    // Bitmaps, tiles, and animations require width and height
    if((entry.type == eFlubGuiBitmap) || (entry.type == eFlubGuiTile) || (entry.type == eFlubGuiAnimBitmap)) {
        if(entry.width < 0) {
            errorf("Theme fragment entry \"%s\" is missing width", name);
            return 0;
        }
        if(entry.height < 0) {
            errorf("Theme fragment entry \"%s\" is missing height", name);
            return 0;
        }
    }
    if((entry.type != eFlubGuiAnimBitmap) && (entry.type != eFlubGuiAnimSlice)) {
        if(entry.x1 < 0) {
            errorf("Theme fragment entry \"%s\" is missing an initial x value", name);
            return 0;
        }
        if(entry.y1 < 0) {
            errorf("Theme fragment entry \"%s\" is missing an initial y value", name);
            return 0;
        }
    }
    if(entry.type == eFlubGuiSlice) {
        if(entry.x2 < 0) {
            errorf("Theme fragment entry \"%s\" is missing an x2 value", name);
            return 0;
        }
        if(entry.y2 < 0) {
            errorf("Theme fragment entry \"%s\" is missing a y2 value", name);
            return 0;
        }
        if(entry.x3 < 0) {
            errorf("Theme fragment entry \"%s\" is missing an x3 value", name);
            return 0;
        }
        if(entry.y3 < 0) {
            errorf("Theme fragment entry \"%s\" is missing a y3 value", name);
            return 0;
        }
        if(entry.x4 < 0) {
            errorf("Theme fragment entry \"%s\" is missing an x4 value", name);
            return 0;
        }
        if(entry.y4 < 0) {
            errorf("Theme fragment entry \"%s\" is missing a y4 value", name);
            return 0;
        }
    }

    if((entry.texid > theme->maxId) || (theme->idMap[entry.texid] == NULL)) {
        errorf("Theme fragment entry \"%s\" has an unknown texture id %d (%d)", name, entry.texid, theme->maxId);
        return 0;
    }
    if(((flubGuiThemeTexture_t *)(theme->idMap[entry.texid]))->type != eFlubGuiThemeTexture) {
        errorf("Theme fragment entry \"%s\" specifies texture id %d, but it's not a texture", name, entry.texid);
        return 0;
    }

    debugf(DBG_GUI, DBG_GUI_DTL_THEME, "Found fragment \"%s\" as %d, %s %dx%d, tex %d, (%d,%d) (%d,%d) (%d,%d) (%d,%d)",
          name, entry.id, utilEnum2Str(_fragmentTypeMap, entry.type, "<unknown>"), entry.width, entry.height, entry.texid,
          entry.x1, entry.y1, entry.x2, entry.y2, entry.x3, entry.y3, entry.x4, entry.y4);

    fragment = util_calloc(sizeof(flubGuiThemeFragment_t), 0, NULL);
    fragment->type = eFlubGuiThemeFragment;
    fragment->id = entry.id;
    fragment->fragmentType = entry.type;
    switch(entry.type) {
        default:
        case eFlubGuiBitmap:
        case eFlubGuiTile:
            fragment->bitmap.texture = ((flubGuiThemeTexture_t *)(theme->idMap[entry.texid]))->texture;
            fragment->bitmap.x1 = entry.x1;
            fragment->bitmap.y1 = entry.y1;
            fragment->bitmap.x2 = entry.x2;
            fragment->bitmap.y2 = entry.y2;
            break;
        case eFlubGuiAnimBitmap:
            fragment->bitmap.texture = ((flubGuiThemeTexture_t *)(theme->idMap[entry.texid]))->texture;
            fragment->animBitmap.width = entry.width;
            fragment->animBitmap.height = entry.height;
            break;
        case eFlubGuiSlice:
            fragment->slice = gfxSliceCreate(((flubGuiThemeTexture_t *)(theme->idMap[entry.texid]))->texture,
                                             GFX_SLICE_NOTILE_ALL,
                                             entry.x1, entry.y1,
                                             entry.x2, entry.y2,
                                             entry.x3, entry.y3,
                                             entry.x4, entry.y4);
            break;
        case eFlubGuiAnimSlice:
            fragment->animSlice.texture = ((flubGuiThemeTexture_t *)(theme->idMap[entry.texid]))->texture;
            break;
    }

    if(entry.type == eFlubGuiAnimBitmap) {
        if(!jsonParseObject(json, jsmnObjArrayToken(json, tokens, "frames"),
                            _themeParseBitmapFrameEntry, (void *)fragment)) {
            if(fragment->animBitmap.frames != NULL) {
                for(bmpWalk = fragment->animBitmap.frames; bmpWalk != NULL; bmpWalk = bmpNext) {
                    bmpNext = bmpWalk;
                    util_free(bmpWalk);
                }
            }
            util_free(fragment);
            return 0;
        }
    } else if(entry.type == eFlubGuiAnimSlice) {
        if(!jsonParseObject(json, jsmnObjArrayToken(json, tokens, "frames"),
                            _themeParseAnimSliceFrameEntry, (void *)fragment)) {
            if(fragment->animSlice.frames != NULL) {
                for(sliceWalk = fragment->animSlice.frames; sliceWalk != NULL; sliceWalk = sliceNext) {
                    sliceNext = sliceWalk;
                    util_free(sliceWalk);
                }
            }
            util_free(fragment);
            return 0;
        }
    }

    critbitInsert(&theme->fragments, name, fragment, NULL);
    _flubGuiThemeIndexAdd(theme, entry.id, fragment);

    return 1;
}

flubGuiTheme_t *flubGuiThemeLoad(const char *filename) {
    PHYSFS_file *file;
    int len;
    char *json;
    jsmn_parser parser;
    jsmnerr_t err;
    jsmntok_t tokens[FLUB_THEME_TOK_NUM];
    flubGuiTheme_t *theme;
    char *str;
    char buf[128];
    int fail = 0;
    jsmntok_t *tok;
    int rev;

    if((file = PHYSFS_openRead(filename)) == NULL) {
        errorf("Unable to open the gui theme file '%s'.", filename);
        return NULL;
    }

    // Load the entire contents of the file
    len = PHYSFS_fileLength(file);
    json = util_alloc(len + 1, NULL);
    memset(json, 0, len + 1);

    PHYSFS_read(file, json, len, 1);
    PHYSFS_close(file);
    json[len] = '\0';

    utilStripCommentsFromJSON(json, len);

    jsmn_init(&parser);

    // Parse the theme file
    err = jsmn_parse(&parser, json, len, tokens, FLUB_THEME_TOK_NUM);
    if(err < 0) {
        if(err == JSMN_ERROR_INVAL) {
            errorf("Failed to parse theme \"%s\": JSON is corrupted", filename);
        } else if(err == JSMN_ERROR_NOMEM) {
            errorf("Failed to parse theme \"%s\": insufficient tokens to parse JSON", filename);
        } else if(err == JSMN_ERROR_PART) {
            errorf("Failed to parse theme \"%s\": JSON is incomplete", filename);
        }
        util_free(json);
        return NULL;
    }

    theme = util_alloc(sizeof(flubGuiTheme_t), NULL);
    memset(theme, 0, sizeof(flubGuiTheme_t));

    if(!jsmnIsObject(tokens)) {
        errorf("Failed to parse theme \"%s\": JSON does not contain root object", filename);
        fail = 1;
    }

    // Validate the file type
    if(!fail) {
        str = jsmnObjStringVal(json, tokens, "file", buf, sizeof(buf), "NotATheme");
        if(strcmp(jsmnObjStringVal(json, tokens, "file", buf, sizeof(buf), "NotATheme"), "flubtheme")) {
            errorf("Failed to load theme file \"%s\": not a theme file [%s]", filename, str);
            fail = 1;
        }
    }

    // Validate the revision
    if(!fail) {
        if((rev = jsmnObjNumberVal(json, tokens, "version", 0)) != FLUB_GUI_THEME_REV) {
            errorf("Failed to load theme file \"%s\": invalid revision %d != %d", filename, rev, FLUB_GUI_THEME_REV);
            fail = 1;
        }
    }

    // Get the theme name
    if(!fail) {
        str = jsmnObjStringVal(json, tokens, "name", buf, sizeof(buf), "default");
        theme->name = util_strdup(str);
    }

    // Parse the color values
    if(!fail) {
        if(!jsonParseObject(json, jsmnObjObjToken(json, tokens, "colors"),
                            _themeParseColorEntry, (void *)theme)) {
            fail = 1;
        }
    }

    // Parse the fonts
    if(!fail) {
        if(!jsonParseObject(json, jsmnObjObjToken(json, tokens, "fonts"),
                            _themeParseFontEntry, (void *)theme)) {
            fail = 1;
        }
    }

    // Parse the textures
    if(!fail) {
        if(!jsonParseObject(json, jsmnObjObjToken(json, tokens, "textures"),
                            _themeParseTextureEntry, (void *)theme)) {
            fail = 1;
        }
    }

    // Parse the theme fragments
    if(!fail) {
        if(!jsonParseObject(json, jsmnObjObjToken(json, tokens, "fragments"),
                            _themeParseFragmentEntry, (void *)theme)) {
            fail = 1;
        }
    }

    // Parse the theme components
    if(!fail) {
        if(!jsonParseObject(json, jsmnObjObjToken(json, tokens, "components"),
                            _themeParseComponentEntry, (void *)theme)) {
            fail = 1;
        }
    }

    if(fail) {
        // Cleanup the failed theme load
        _flubGuiThemeCleanup(theme);
        return NULL;
    } else {
        _flubGuiThemeRegister(theme);
        return theme;
    }
}

flubGuiThemeColor_t *flubGuiThemeColorGetByName(flubGuiTheme_t *theme, const char *name) {
    flubGuiThemeColor_t *color;

    if(!critbitContains(&(theme->colors), name, (void **)&color)) {
        errorf("Theme \"%s\" does not have a color entry \"%s\"", theme->name, name);
        return NULL;
    }
    return color;
}

flubGuiThemeColor_t *flubGuiThemeColorGetById(flubGuiTheme_t *theme, int id) {
    flubGuiThemeColor_t *color;

    if((id >= theme->maxId) || (theme->idMap[id] == NULL)) {
        errorf("Theme \"%s\" does not have a color entry with id %d", theme->name, id);
        return NULL;
    }
    color = ((flubGuiThemeColor_t *)(theme->idMap[id]));
    if(color->type != eFlubGuiThemeColor) {
        errorf("Theme \"%s\" id %d is not a color entry", theme->name, id);
        return 0;
    }
    return color;
}

font_t *flubGuiThemeFontGetByName(flubGuiTheme_t *theme, const char *name) {
    flubGuiThemeFont_t *font;

    if(!critbitContains(&(theme->fonts), name, (void **)&font)) {
        errorf("Theme \"%s\" does not have a font entry \"%s\"", theme->name, name);
        return NULL;
    }
    return font->font;
}

font_t *flubGuiThemeFontGetById(flubGuiTheme_t *theme, int id) {
    flubGuiThemeFont_t *font;

    if((id >= theme->maxId) || (theme->idMap[id] == NULL)) {
        errorf("Theme \"%s\" does not have a font entry with id %d", theme->name, id);
        return NULL;
    }
    font = ((flubGuiThemeFont_t *)(theme->idMap[id]));
    if(font->type != eFlubGuiThemeFont) {
        errorf("Theme \"%s\" id %d is not a font entry", theme->name, id);
        return 0;
    }
    return font->font;
}

const texture_t *flubGuiThemeTextureGetByName(flubGuiTheme_t *theme, const char *name) {
    flubGuiThemeTexture_t *texture;

    if(!critbitContains(&(theme->textures), name, (void **)&texture)) {
        errorf("Theme \"%s\" does not have a texture entry \"%s\"", theme->name, name);
        return NULL;
    }
    return texture->texture;
}

const texture_t *flubGuiThemeTextureGetById(flubGuiTheme_t *theme, int id) {
    flubGuiThemeTexture_t *texture;

    if((id >= theme->maxId) || (theme->idMap[id] == NULL)) {
        errorf("Theme \"%s\" does not have a texture entry with id %d", theme->name, id);
        return NULL;
    }
    texture = ((flubGuiThemeTexture_t *)(theme->idMap[id]));
    if(texture->type != eFlubGuiThemeTexture) {
        errorf("Theme \"%s\" id %d is not a texture entry", theme->name, id);
        return 0;
    }
    return texture->texture;
}

flubGuiThemeFragment_t *flubGuiThemeFragmentGetByName(flubGuiTheme_t *theme, const char *name) {
    flubGuiThemeFragment_t *fragment;

    if(!critbitContains(&(theme->fragments), name, (void **)&fragment)) {
        errorf("Theme \"%s\" does not have a fragment entry \"%s\"", theme->name, name);
        return NULL;
    }
    return fragment;
}

flubGuiThemeFragment_t *flubGuiThemeFragmentGetById(flubGuiTheme_t *theme, int id) {
    flubGuiThemeFragment_t *fragment;

    if((id >= theme->maxId) || (theme->idMap[id] == NULL)) {
        errorf("Theme \"%s\" does not have a fragment entry with id %d", theme->name, id);
        return NULL;
    }
    fragment = ((flubGuiThemeFragment_t *)(theme->idMap[id]));
    if(fragment->type != eFlubGuiThemeFragment) {
        errorf("Theme \"%s\" id %d is not a fragment entry", theme->name, id);
        return 0;
    }
    return fragment;
}

flubGuiThemeComponents_t *flubGuiThemeComponentGetByName(flubGuiTheme_t *theme, const char *name) {
    flubGuiThemeComponents_t *component;

    if(!critbitContains(&(theme->components), name, (void **)&component)) {
        errorf("Theme \"%s\" does not have a component entry \"%s\"", theme->name, name);
        return NULL;
    }
    return component;
}

flubGuiThemeComponents_t *flubGuiThemeComponentGetById(flubGuiTheme_t *theme, int id) {
    flubGuiThemeComponents_t *component;

    if((id >= theme->maxId) || (theme->idMap[id] == NULL)) {
        errorf("Theme \"%s\" does not have a component entry with id %d", theme->name, id);
        return NULL;
    }
    component = ((flubGuiThemeComponents_t *)(theme->idMap[id]));
    if(component->type != eFlubGuiThemeComponent) {
        errorf("Theme \"%s\" id %d is not a component entry", theme->name, id);
        return 0;
    }
    return component;
}

void flubGuiThemeDraw(flubGuiThemeFragment_t *fragment, gfxMeshObj2_t *mesh,
                      Uint32 *ticks, int x1, int y1, int x2, int y2) {
    int w, h;
    int delay, walk;
    flubGuiThemeFragmentAnimBitmapFrame_t *bmpFrame;
    flubGuiThemeFragmentSliceFrame_t *sliceFrame;

    switch(fragment->fragmentType) {
        default:
            return;
        case eFlubGuiBitmap:
            gfxTexMeshBlitSub2(mesh, fragment->bitmap.texture,
                              fragment->bitmap.x1, fragment->bitmap.y1,
                              fragment->bitmap.x2, fragment->bitmap.y2,
                              x1, y1, x2, y2);
            break;
        case eFlubGuiAnimBitmap:
            bmpFrame = fragment->animBitmap.frames;
            if(bmpFrame == NULL) {
                return;
            }
            if(*ticks > FLUB_THEME_MAX_ANIM_TICKS) {
                *ticks = 0;
            }
            for(walk = 0, delay = *ticks; delay > bmpFrame->delayMs;) {
                delay -= bmpFrame->delayMs;
                walk += bmpFrame->delayMs;
                if(bmpFrame->next == NULL) {
                    *ticks -= walk;
                    walk = 0;
                    bmpFrame = fragment->animBitmap.frames;
                }
            }
            gfxTexMeshBlitSub2(mesh, fragment->animBitmap.texture,
                              bmpFrame->x, bmpFrame->y,
                              bmpFrame->x + fragment->animBitmap.width,
                              bmpFrame->y + fragment->animBitmap.height,
                              x1, y1, x2, y2);
            break;
        case eFlubGuiTile:
            gfxTexMeshTile2(mesh, fragment->bitmap.texture,
                           fragment->bitmap.x1, fragment->bitmap.y1,
                           fragment->bitmap.x2, fragment->bitmap.y2,
                           x1, y1, x2, y2);
            break;
        case eFlubGuiSlice:
            gfxSliceMeshBlit2(mesh, fragment->slice, x1, y1, x2, y2);
            break;
        case eFlubGuiAnimSlice:
            sliceFrame = fragment->animSlice.frames;
            if(sliceFrame == NULL) {
                return;
            }
            if(*ticks > FLUB_THEME_MAX_ANIM_TICKS) {
                *ticks = 0;
            }
            for(walk = 0, delay = *ticks; delay > sliceFrame->delayMs;) {
                delay -= sliceFrame->delayMs;
                walk += sliceFrame->delayMs;
                if(sliceFrame->next == NULL) {
                    *ticks -= walk;
                    walk = 0;
                    sliceFrame = fragment->animSlice.frames;
                }
            }
            gfxSliceMeshBlit2(mesh, sliceFrame->slice, x1, y1, x2, y2);
            break;
    }
}

int flubGuiThemeContextRegister(flubGuiTheme_t *theme, int id, void *context) {
    flubGuiThemeContextEntry_t *walk, *last, *entry;

    for(last = NULL, walk = theme->contextList;
        ((walk != NULL) && (walk->id != id));
        walk = walk->next);

    if(walk != NULL) {
        errorf("Theme \"%s\" already has a context registered for ID %d", theme->name, id);
        return 0;
    }

    entry = util_alloc(sizeof(flubGuiThemeContextEntry_t), NULL);
    entry->id = id;
    entry->context = context;

    if(last == NULL) {
        theme->contextList = entry;
    } else {
        last->next = entry;
    }

    return 1;
}

void *flubGuiThemeContextRetrieve(flubGuiTheme_t *theme, int id) {
    flubGuiThemeContextEntry_t *walk;

    for(walk = theme->contextList; walk != NULL; walk = walk->next) {
        if(walk->id == id) {
            return walk->context;
        }
    }
    return NULL;
}

