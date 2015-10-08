#ifndef _FLUB_SIMPLEEDIT_HEADER_
#define _FLUB_SIMPLEEDIT_HEADER_


#include <flub/font_struct.h>


typedef struct flubSimpleEdit_s {
    int x;
    int y;
    int width;
    int height;
    font_t *font;
    int cursorPos;
    int cursorX;
    int active;
    int size;
    int length;
    char buf[0];
} flubSimpleEdit_t;


#define FLUB_SIMPLE_EDIT_PADDING    2
#define FLUB_SIMPLE_EDIT_BORDER     2


flubSimpleEdit_t * flubSimpleEditCreate(font_t *font, int maxlen, int x, int y);
void flubSimpleEditDraw(flubSimpleEdit_t *edit);
void flubSimpleEditActive(flubSimpleEdit_t *edit, int mode);
void flubSimpleEditSet(flubSimpleEdit_t *edit, const char *str);
int flubSimpleEditInput(flubSimpleEdit_t *edit, SDL_Event *event);


#endif // _FLUB_SIMPLEEDIT_HEADER_
