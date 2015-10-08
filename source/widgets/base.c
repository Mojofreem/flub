#include <flub/widget.h>
#include <flub/widgets/base.h>


void widgetBaseInit(widget_t *widget) {
    return;
}

void widgetBaseDestroy(widget_t *widget) {
    return;
}

void widgetBaseApplyTheme(widget_t *widget, flubGuiTheme_t *theme) {
    return;
}

void widgetBaseRender(widget_t *widget, gfxMeshObj_t *mesh) {
    return;
}

void widgetBaseResize(widget_t *widget, int width, int height) {
    widget->width = width;
    widget->height = height;
}

int widgetBaseUpdate(widget_t *widget, Uint32 ticks) {
    return 0;
}

int widgetBaseInput(widget_t *widget, int passive, SDL_Event *event) {
    return 0;
}

int widgetBaseMessage(widget_t *widget, int signal, void *context) {
    return 0;
}

int widgetBaseState(widget_t *widget, int mode, unsigned int flags) {
    if(mode) {
        widget->flags |= flags;
    } else {
        widget->flags |= flags;
        widget->flags ^= flags;
    }
    return 1;
}

widgetHandlers_t _widgetBaseHandlers = {
        .init    = widgetBaseInit,
        .destroy = widgetBaseDestroy,
        .apply   = widgetBaseApplyTheme,
        .render  = widgetBaseRender,
        .resize  = widgetBaseResize,
        .update  = widgetBaseUpdate,
        .input   = widgetBaseInput,
        .message = widgetBaseMessage,
        .state   = widgetBaseState,
};

widgetHandlers_t *widgetBaseHandlers = &_widgetBaseHandlers;