#ifndef _FLUB_GUI_HEADER_
#define _FLUB_GUI_HEADER_


#include <flub/widget.h>
#include <flub/theme.h>
#include <flub/layout.h>
#include <flub/app.h>


int guiInit(appDefaults_t *defaults);
int guiValid(void);
void guiShutdown(void);

void guiWidgetAdd(widget_t *widget);
void guiWidgetRemove(widget_t *widget);

int guiUpdate(Uint32 ticks, Uint32 elapsed);
int guiInput(SDL_Event *event);

int guiMessage(int widgetId, int signal, void *context);
void guiState(int widgetId, int mode, unsigned int flags);
void guiFocus(int widgetId);
void guiDirty(int widgetId);

widget_t *guiGetFocus(void);
void guiSetFocus(widget_t *widget);

widget_t *guiWidgetFind(int widgetId);



#endif // _FLUB_GUI_HEADER_
