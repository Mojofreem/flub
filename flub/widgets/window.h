#ifndef _FLUB_WIDGET_WINDOW_HEADER_
#define _FLUB_WIDGET_WINDOW_HEADER_


#include <flub/widget.h>


#define FLUB_WIDGET_TYPE_WINDOW     1

#define FLUB_GUI_WINDOW_MINBTN      0x01
#define FLUB_GUI_WINDOW_MAXBTN      0x02
#define FLUB_GUI_WINDOW_CLOSEBTN    0x04
#define FLUB_GUI_WINDOW_PINBTN      0x08
#define FLUB_GUI_WINDOW_HELPBTN     0x10
#define FLUB_GUI_WINDOW_SYSMENU     0x20


#define FLUB_GUI_WINDOW_MSG_SET_CAPTION     1001


int flubWidgetWindowRegister(void);
widget_t *flubWidgetWindowCreate(int id, int x, int y, int width, int height,
                                 unsigned int flags, unsigned int windowFlags,
                                 flubGuiTheme_t *theme, layoutNode_t *layout,
                                 const char *caption);


#endif // _FLUB_WIDGET_WINDOW_HEADER_
