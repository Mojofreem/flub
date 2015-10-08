#ifndef _FLUB_WIDGET_HEADER_
#define _FLUB_WIDGET_HEADER_


#include <SDL2/SDL.h>
#include <flub/gfx.h>
#include <flub/theme.h>
#include <flub/layout.h>


typedef struct widget_s widget_t;

typedef void (widgetInit_t)(widget_t *widget);
typedef void (widgetDestroy_t)(widget_t *widget);
typedef void (widgetApplyTheme_t)(widget_t *widget, flubGuiTheme_t *theme);
typedef void (widgetRender_t)(widget_t *widget, gfxMeshObj_t *mesh);
typedef void (widgetResize_t)(widget_t *widget, int width, int height);
typedef int (widgetUpdate_t)(widget_t *widget, Uint32 ticks);
// passive indicates an event that is not specifically triggered for the widget
// (ie., hover action)
typedef int (widgetInput_t)(widget_t *widget, int passive, SDL_Event *event);
typedef int (widgetMessage_t)(widget_t *widget, int signal, void *context);
// mode defines whether to set or clear a flag
typedef int (widgetState_t)(widget_t *widget, int mode, unsigned int flags);

typedef struct widgetHandlers_s widgetHandlers_t;
struct widgetHandlers_s {
    widgetInit_t *init;
    widgetDestroy_t *destroy;
    widgetApplyTheme_t *apply;
    widgetRender_t *render;
    widgetResize_t *resize;
    widgetUpdate_t *update;
    widgetInput_t *input;
    widgetMessage_t *message;
    widgetState_t *state;
};

typedef struct widgetRect_s {
    int x;
    int y;
    int width;
    int height;
} widgetRect_t;

#define WIDGET_FLAG_VISIBLE     0x0001
#define WIDGET_FLAG_ENABLED     0x0002
#define WIDGET_FLAG_FOCUS       0x0004
#define WIDGET_FLAG_HOVER       0x0008
#define WIDGET_FLAG_TRIGGER_HOVER   0x0010
#define WIDGET_FLAG_DIRTY       0x0020


struct widget_s {
    int id;
    int type;

    int x;
    int y;
    int width;
    int height;

    widgetRect_t clientArea;

    unsigned int flags;

    flubGuiTheme_t *theme;
    Uint32 ticks;

    widgetHandlers_t *handlers;

    widget_t *parent;
    widget_t *next;
    widget_t *child;

    layoutNode_t *layout;

    union {
        void *vdata;
        unsigned char bdata[0];
    };
};


int widgetInit(void);
int widgetValid(void);
void widgetShutdown(void);

widget_t *widgetCreate(int id, int type, int x, int y, int width, int height,
                       unsigned int flags, flubGuiTheme_t *theme,
                       widgetHandlers_t *handlers, layoutNode_t *layout,
                       int dataSize);
void widgetDestroy(widget_t *widget);
void widgetChildAdd(widget_t *widget, widget_t *child);
void widgetNextAdd(widget_t *widget, widget_t *next);

void widgetThemeApply(widget_t *widget, flubGuiTheme_t *theme);
void widgetThemeRelease(widget_t *widget);

void widgetAddToLayout(widget_t *widget, layoutNode_t *layout);
void widgetRemoveFromLayout(widget_t *widget);

widget_t *widgetFind(widget_t *widgets, int widgetId);

widget_t *widgetFocus(widget_t *widgets);
void widgetUnfocus(widget_t *widgets);
widget_t *widgetMouseOver(widget_t *widgets, int x, int y);
void widgetDirty(widget_t *widget);
void widgetClean(widget_t *widget);

void widgetRender(widget_t *widget, gfxMeshObj_t *mesh);
void widgetMove(widget_t *widget, int x, int y);
void widgetResize(widget_t *widget, int width, int height);
void widgetUpdate(widget_t *widget, Uint32 ticks);
int widgetInput(widget_t *widget, int passive, SDL_Event *event);
int widgetMessageByPtr(widget_t *widget, int signal, void *context);
int widgetMessageById(widget_t *widget, int widgetId, int signal, void *context);
int widgetStateByPtr(widget_t *widget, int mode, unsigned int flags);
int widgetStateById(widget_t *widget, int widgetId, int mode, unsigned int flags);


#endif // _FLUB_WIDGET_HEADER_

/*
Widgets
create a widget
    widget specific data
    theme context
    widget resize (feedback to layout)
message a widget
resize a widget
    direct, or layout
input
    direct (we have focus)
    indirect (passive listener)
*/