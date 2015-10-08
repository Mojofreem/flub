#include _FLUB_WIDGET_BASE_HEADER_
#define _FLUB_WIDGET_BASE_HEADER_


#include <flub/widget.h>


void widgetBaseInit(widget_t *widget);
void widgetBaseDestroy(widget_t *widget);
void widgetBaseApplyTheme(widget_t *widget, flubGuiTheme_t *theme);
void widgetBaseRender(widget_t *widget, gfxMeshObj_t *mesh);
void widgetBaseResize(widget_t *widget, int width, int height);
int widgetBaseUpdate(widget_t *widget, Uint32 ticks);
int widgetBaseInput(widget_t *widget, int passive, SDL_Event *event);
int widgetBaseMessage(widget_t *widget, int signal, void *context);
int widgetBaseState(widget_t *widget, int mode, unsigned int flags);

extern widgetHandlers_t *widgetBaseHandlers;


#endif // _FLUB_WIDGET_BASE_HEADER_
