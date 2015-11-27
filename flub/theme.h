#ifndef _FLUB_GUI_THEME_
#define _FLUB_GUI_THEME_


#include <flub/gfx.h>
#include <flub/font.h>
#include <flub/texture.h>
#include <flub/data/critbit.h>


typedef void *flubGuiTheme_t;

#if 0
/*
    {
        file: "flubtheme",
        name: "basic",
        version: 1,
        colors: {
            textactive: {
                id: 1001,
                rgb: "#0AF"
            }
        },
        font: {
            default: {
                id: 1101,
                file: "resources/font/consolas.12.stbfont",
                font: "consolas",
                size: "12"
            }
        },
        textures: {
            basic: {
                id: 1201,
                filename: "resources/images/flub-simple-gui.gif",
                minfilter: "GL_NEAREST",
                magfilter: "GL_NEAREST",
                colorkey: "magenta"
            }
        },
        fragments: {
            dialog_bg: {
                type: "tile",
                id: 1301,
                texid: 1201,
                width: 32,
                height: 32,
                x: 15,
                y: 15
            },
            super_icon: {
                type: "bitmap",
                id: 1302,
                texid: 1201,
                width: 32,
                height: 32,
                x: 55,
                y: 66
            },
            dialog: {
                type: "slice",
                id: 1303,
                texid: 1201,
                x1: 15,
                y1: 15,
                x2: 30,
                y2: 30,
                x3: 45,
                y3: 45,
                x4: 60,
                y4: 60
            },
            dialog_bg: {
                type: "animation",
                id: 1305,
                texid: 1201,
                width: 16,
                height: 16,
                frames: [
                    { x: 0, y: 8, delay: 110  },
                    { x: 16, y: 24, delay: 120 },
                    { x: 32, y: 40, delay: 130 },
                    { x: 48, y: 56, delay: 140 }
                ]
            },
            dialog_anim: {
                type: "animslice",
                id: 1306,
                texid: 1201,
                frames: [
                    { x1: 15, y1: 15, x2: 30, y2: 30, x3: 45, y3: 45, x4: 60, y4: 60, delay: 50 },
                    { x1: 15, y1: 15, x2: 30, y2: 30, x3: 45, y3: 45, x4: 60, y4: 60, delay: 50 },
                    { x1: 15, y1: 15, x2: 30, y2: 30, x3: 45, y3: 45, x4: 60, y4: 60, delay: 50 }
                ]
            }
        },
        components: {
            dialog: {
                id: 1401,
                maxwidth: 32767,
                minwidth:  0,
                maxheight: 32767,
                minheight: 0,
                padleft: 0,
                padright: 0,
                padtop: 0,
                padbottom: 0,
                marginleft: 0,
                marginright: 0,
                margintop: 0,
                marginbottom: 0,
                states: [
                    { mode: "disable|enable|focus|press|hover", fragid: 1303 }
                ]
            }
        }
    }

    Colors
        color-caption-active
        color-caption-default

    Fonts
        font-caption-active
        font-window-default

    Components
        window-header
        window-body
*/

/*
default     D
disabled    X
focus       F
pressed     P
focus press S

Component         D X F P S
---------------------------
window            D . . . .
caption bar       D . F . .
frame             D . . . .
horz divider      D . . . .
vert divider      D . . . .
menubutton        D X F P S
window button     ---------
	close         D X . . .
	min           D X . . .
	max           D X . . .
	pin           D X . . .
	help		  D X . . .
button            D X F P S
Checkbox   		  D X F . .
checlbox (x)      D X F . .
option            D X F . .
option (x)        D X F . .
textbox           D X F . .
listbox           D X F P S
combobox          D X F P S
combo button      D X F P S
horz scrollbar    D X F . .
vert scrollbar    D X F . .
raised panel      D . . . .
inset panel       D . . . .
toolbar button    D . F P S
slider            ---------
	track		  D . . . .
	marks         D . . . .
	knob          D X F . .
spin buttons      D X . . .
mouse cursors     ---------
	default       D . . . .
	busy          D . . . .
	resize N/S    D . . . .
	resize W/E    D . . . .
	resize NW/SE  D . . . .
	resize SW/NE  D . . . .
	text          D . . . .
	nop           D . . . .
tabs              D . F . .
status bar	      D . . . .
*/

typedef enum {
    eFlubGuiThemeColor = 0,
    eFlubGuiThemeFont,
    eFlubGuiThemeTexture,
    eFlubGuiThemeFragment,
    eFlubGuiThemeComponent
} eFlubGuiThemeType_t;

typedef struct flubGuiThemeColor_s {
    eFlubGuiThemeType_t type;
    int id;
    float red;
    float green;
    float blue;
} flubGuiThemeColor_t;

typedef struct flubGuiThemeFont_s {
    eFlubGuiThemeType_t type;
    int id;
    font_t *font;
} flubGuiThemeFont_t;

typedef struct flubGuiThemeTexture_s {
    eFlubGuiThemeType_t type;
    int id;
    texture_t *texture;
} flubGuiThemeTexture_t;

typedef enum {
    eFlubGuiBitmap = 1,
    eFlubGuiAnimBitmap,
    eFlubGuiTile,
    eFlubGuiSlice,
    eFlubGuiAnimSlice,
} eFlubGuiThemeFragmentType_t;

typedef struct flubGuiThemeFragmentBitmap_s {
    int x1;
    int y1;
    int x2;
    int y2;
    texture_t *texture;
} flubGuiThemeFragmentBitmap_t;

typedef struct flubGuiThemeFragmentAnimBitmapFrame_s {
    int x;
    int y;
    int delayMs;
    struct flubGuiThemeFragmentAnimBitmapFrame_s *next;
} flubGuiThemeFragmentAnimBitmapFrame_t;

typedef struct flubGuiThemeFragmentAnimBitmap_s {
    int count;
    int width;
    int height;
    flubGuiThemeFragmentAnimBitmapFrame_t *frames;
    texture_t *texture;
} flubGuiThemeFragmentAnimBitmap_t;

typedef struct flubGuiThemeFragmentSliceFrame_s {
    flubSlice_t *slice;
    int delayMs;
    struct flubGuiThemeFragmentSliceFrame_s *next;
} flubGuiThemeFragmentSliceFrame_t;

typedef struct flubGuiThemeFragmentAnimSlice_s {
    int count;
    texture_t *texture;
    struct flubGuiThemeFragmentSliceFrame_s *frames;
} flubGuiThemeFragmentAnimSlice_t;

typedef struct flubGuiThemeFragment_s {
    eFlubGuiThemeType_t type;
    int id;
    eFlubGuiThemeFragmentType_t fragmentType;
    union {
        flubGuiThemeFragmentBitmap_t bitmap;
        flubGuiThemeFragmentAnimBitmap_t animBitmap;
        flubSlice_t *slice;
        flubGuiThemeFragmentAnimSlice_t animSlice;
    };
} flubGuiThemeFragment_t;

#define FLUB_GUI_THEME_ENABLED	0x01
#define FLUB_GUI_THEME_FOCUS	0x02
#define FLUB_GUI_THEME_PRESSED	0x04
#define FLUB_GUI_THEME_HOVER	0x08

typedef struct flubGuiThemeComponents_s {
    eFlubGuiThemeFragmentType_t type;
    int id;
    int minWidth;
    int maxWidth;
    int minHeight;
    int maxHeight;
    int padLeft;
    int padRight;
    int padTop;
    int padBottom;
    int marginLeft;
    int marginRight;
    int marginTop;
    int marginBottom;
    // 0x1 enabled, 0x2 focus, 0x4 pressed, 0x8 hover
    flubGuiThemeFragment_t *fragmentIndex[16];
} flubGuiThemeComponents_t;

typedef struct flubGuiThemeContextEntry_s {
    int id;
    void *context;
    struct flubGuiThemeContextEntry_s *next;
} flubGuiThemeContextEntry_t;

typedef struct flubGuiTheme_s {
    const char *name;
    critbit_t colors;
    critbit_t fonts;
    critbit_t textures;
    critbit_t fragments;
    critbit_t components;
    int maxId;
    void **idMap;

    flubGuiThemeContextEntry_t *contextList;

    struct flubGuiTheme_s *next;
} flubGuiTheme_t;

#endif

flubGuiTheme_t *flubGuiThemeLoad(const char *filename);
flubGuiTheme_t *flubGuiThemeGet(const char *name);

#if 0

// TODO - implement saving a theme
//int flubGuiThemeSave(flubGuiTheme_t *theme, const char *name);

flubGuiThemeColor_t *flubGuiThemeColorGetByName(flubGuiTheme_t *theme, const char *name);
flubGuiThemeColor_t *flubGuiThemeColorGetById(flubGuiTheme_t *theme, int id);

font_t *flubGuiThemeFontGetByName(flubGuiTheme_t *theme, const char *name);
font_t *flubGuiThemeFontGetById(flubGuiTheme_t *theme, int id);

const texture_t *flubGuiThemeTextureGetByName(flubGuiTheme_t *theme, const char *name);
const texture_t *flubGuiThemeTextureGetById(flubGuiTheme_t *theme, int id);

flubGuiThemeFragment_t *flubGuiThemeFragmentGetByName(flubGuiTheme_t *theme, const char *name);
flubGuiThemeFragment_t *flubGuiThemeFragmentGetById(flubGuiTheme_t *theme, int id);

flubGuiThemeComponents_t *flubGuiThemeComponentGetByName(flubGuiTheme_t *theme, const char *name);
flubGuiThemeComponents_t *flubGuiThemeComponentGetById(flubGuiTheme_t *theme, int id);

void flubGuiThemeDraw(flubGuiThemeFragment_t *fragment, gfxMeshObj_t *mesh,
                      Uint32 *ticks, int x1, int y1, int x2, int y2);


int flubGuiThemeContextRegister(flubGuiTheme_t *theme, int id, void *context);
void *flubGuiThemeContextRetrieve(flubGuiTheme_t *theme, int id);

#endif

#endif // _FLUB_GUI_THEME_
