#include <flub/widget.h>
#include <flub/widgets/base.h>
#include <flub/widgets/window.h>


#define WIDGET_WINDOW_CAPTION_MAX_LEN   128

#define FLUB_GUI_WINDOW_MINBTN      0x01
#define FLUB_GUI_WINDOW_MAXBTN      0x02
#define FLUB_GUI_WINDOW_CLOSEBTN    0x04
#define FLUB_GUI_WINDOW_PINBTN      0x08
#define FLUB_GUI_WINDOW_HELPBTN     0x10
#define FLUB_GUI_WINDOW_SYSMENU     0x20

#define FLUB_GUI_WINDOW_MSG_SET_CAPTION     1001


typedef struct widgetWindowTheme_s {
    flubGuiThemeColor_t *colorDefault;
    flubGuiThemeColor_t *colorActive;

    font_t *fontDefault;
    font_t *fontActive;

    flubGuiThemeComponents_t *compHeader;
    flubGuiThemeComponents_t *compBody;
} widgetWindowTheme_t;


#define WIDGET_FLAG_VISIBLE     0x0001
#define WIDGET_FLAG_ENABLED     0x0002
#define WIDGET_FLAG_FOCUS       0x0004
#define WIDGET_FLAG_HOVER       0x0008
#define WIDGET_FLAG_TRIGGER_HOVER   0x0010
#define WIDGET_FLAG_DIRTY       0x0020


typedef struct widgetWindow_s {
    widget_t widget;
    unsigned int windowFlags;
    char caption[WIDGET_WINDOW_CAPTION_MAX_LEN];
    widgetWindowTheme_t *windowTheme;
    widgetRect_t headerArea;
    struct {
        int x;
        int y;
    } caption;
} widgetWindow_t;


void widgetWindowApplyTheme(widget_t *widget, flubGuiTheme_t *theme) {
    ((widgetWindow_t *)widget)->windowTheme = (widgetWindowTheme_t *)flubGuiThemeContextRetrieve(theme, FLUB_WIDGET_TYPE_WINDOW);
}

void widgetWindowRender(widget_t *widget, gfxMeshObj_t *mesh) {

    return;
}

void widgetWindowResize(widget_t *widget, int width, int height) {
    widgetWindow_t *window = (widgetWindow_t *)widget;

    if(window->windowTheme->compHeader != NULL) {
        window->headerArea->x = widget->x +

    }
    widgetDirty(widget);
}

int widgetWindowUpdate(widget_t *widget, Uint32 ticks) {
    widget->ticks += ticks;
}

int widgetWindowInput(widget_t *widget, int passive, SDL_Event *event) {
    // TODO Handle window input events
    return 0;
}

int widgetWindowMessage(widget_t *widget, int signal, void *context) {
    // TODO Handle window message
    return 0;
}

int widgetWindowState(widget_t *widget, int mode, unsigned int flags) {
    // TODO Handle window state
    return 0;
}

widgetHandlers_t _widgetWindowHandlers = {
        .init = NULL,
        .destroy = NULL,
        .apply = widgetWindowApplyTheme,
        .render = widgetWindowRender,
        .resize = widgetWindowResize,
        .update = widgetWindowUpdate,
        .input = widgetWindowInput,
        .message = widgetWindowMessage,
        .state = widgetWindowState,
};

widgetHandlers_t *widgetWindowHandlers = &_widgetWindowHandlers;


widgetWindowTheme_t *_flubWidgetWindowRegisterTheme(flubGuiTheme_t *theme) {
    widgetWindowTheme_t *context;

    if((context = flubGuiThemeContextRetrieve(theme, FLUB_WIDGET_TYPE_WINDOW)) != NULL) {
        return context;
    }

    context = util_alloc(sizeof(widgetWindowTheme_t), NULL);

    context->colorDefault = flubGuiThemeColorGetByName(theme, "color-caption-default");
    context->colorActive = flubGuiThemeColorGetByName(theme, "color-caption-active");
    if(context->colorActive == NULL) {
        context->colorActive = context->colorDefault;
    }

    context->fontDefault = flubGuiThemeFontGetByName(theme, "font-caption-default");
    context->fontActive = flubGuiThemeFontGetByName(theme, "font-caption-active");
    if(context->fontActive == NULL) {
        context->fontActive = context->fontDefault;
    }

    context->compHeader = flubGuiThemeComponentGetByName(theme, "window-header");
    context->compBody = flubGuiThemeComponentGetByName(theme, "window-body");

    flubGuiThemeContextRegister(theme, FLUB_WIDGET_TYPE_WINDOW, context);

    return context;
}

int flubWidgetWindowRegister(void) {
    // TODO Register the widget with the GUI module
    return 1;
}

widget_t *flubWidgetWindowCreate(int id, int x, int y, int width, int height,
                                 unsigned int flags, unsigned int windowFlags,
                                 flubGuiTheme_t *theme, layoutNode_t *layout,
                                 const char *caption) {
    int size;
    widget_t *widget;

    size = sizeof(widgetWindow_t) - sizeof(widget_t);

    _flubWidgetWindowRegisterTheme(theme);

    widget = widgetCreate(id, FLUB_WIDGET_TYPE_WINDOW, x, y, width, height,
                          flags, theme, widgetWindowHandlers, layout,
                          size);

    ((widgetWindow_t *)widget)->windowFlags = windowFlags;
    strncpy(((widgetWindow_t *)widget)->caption,
            ((caption == NULL) ? "" : caption), WIDGET_WINDOW_CAPTION_MAX_LEN);

    return widget;
}


