#include <flub/font.h>
#include <flub/memory.h>
#include <flub/sdl.h>
#include <ctype.h>
#include <string.h>


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


flubSimpleEdit_t * flubSimpleEditCreate(font_t *font, int maxlen, int x, int y) {
    flubSimpleEdit_t *edit = NULL;

    edit = util_calloc(sizeof(flubSimpleEdit_t) + maxlen + 1, 0, NULL);
    edit->x = x;
    edit->y = y;
    edit->width = (fontGetWidestWidth(font) * maxlen) + (FLUB_SIMPLE_EDIT_PADDING * 2) + (FLUB_SIMPLE_EDIT_BORDER * 2);
    edit->height = fontGetHeight(font) + (FLUB_SIMPLE_EDIT_PADDING * 2) + (FLUB_SIMPLE_EDIT_BORDER * 2);
    edit->font = font;
    edit->size = maxlen;

    return edit;
}

void flubSimpleEditDraw(flubSimpleEdit_t *edit) {
    glLoadIdentity();
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE);
    if(edit->active) {
        glColor4f(1.0, 1.0, 1.0, 1.0);
    } else {
        glColor4f(0.3, 0.3, 0.3, 1.0);
    }
    glRecti(edit->x, edit->y, edit->x + edit->width, edit->y + edit->height);
    if(edit->active) {
        glColor3f(0.0, 0.0, 0.0);
    } else {
        glColor4f(0.15, 0.15, 0.15, 1.0);
    }
    glRecti(edit->x + FLUB_SIMPLE_EDIT_BORDER, edit->y + FLUB_SIMPLE_EDIT_BORDER,
            edit->x + edit->width - FLUB_SIMPLE_EDIT_BORDER, edit->y + edit->height - FLUB_SIMPLE_EDIT_BORDER);
    if(edit->active) {
        glColor3f(0.5, 1.0, 0.5);
        fontMode();
        fontPos(edit->x + FLUB_SIMPLE_EDIT_PADDING + FLUB_SIMPLE_EDIT_BORDER + edit->cursorX,
                edit->y + FLUB_SIMPLE_EDIT_PADDING + FLUB_SIMPLE_EDIT_BORDER);
        fontBlitC(edit->font, '_');
    }
    glColor3f(1.0, 1.0, 1.0);
    fontPos(edit->x + FLUB_SIMPLE_EDIT_PADDING + FLUB_SIMPLE_EDIT_BORDER,
            edit->y + FLUB_SIMPLE_EDIT_PADDING + FLUB_SIMPLE_EDIT_BORDER);
    fontBlitStr(edit->font, edit->buf);
}

void flubSimpleEditActive(flubSimpleEdit_t *edit, int mode) {
    edit->active = mode;
}

void flubSimpleEditSet(flubSimpleEdit_t *edit, const char *str) {
    strcpy(edit->buf, str);
    edit->length = strlen(edit->buf);
    edit->cursorPos = edit->length;
    edit->cursorX = 0;
}

static void _flubSimpleEditDeleteCharAtPos(flubSimpleEdit_t *edit, int pos) {
    int len;

    if(pos >= edit->length) {
        return;
    }
    len = edit->length - pos;
    if(len > 0) {
        memmove(edit->buf + pos, edit->buf + pos + 1, len);
    }
    edit->length--;
    edit->buf[edit->length] = '\0';
}

int flubSimpleEditInput(flubSimpleEdit_t *edit, SDL_Event *event) {
    char c;
    int len;
    char *ptr;

    if(flubSDLTextInputFilter(event, &c)) {
        if(edit->length < (edit->size - 1)) {
            if(edit->buf[edit->cursorPos] == '\0') {
                edit->buf[edit->cursorPos] = c;
                edit->cursorPos++;
                edit->length++;
                edit->buf[edit->cursorPos] = '\0';
            } else {
                if(edit->cursorPos < edit->length) {
                    len = edit->length - edit->cursorPos;
                    memmove(edit->buf + edit->cursorPos + 1, edit->buf + edit->cursorPos, len);
                }
                edit->buf[edit->cursorPos] = c;
                edit->cursorPos++;
                edit->length++;
            }
            edit->buf[edit->length] = '\0';
        }
    } else {
        if(event->type == SDL_KEYDOWN) {
            switch(event->key.keysym.sym) {
                case SDLK_BACKSPACE:
                    if(edit->cursorPos == 0) {
                        break;
                    }
                    _flubSimpleEditDeleteCharAtPos(edit, edit->cursorPos - 1);
                    edit->cursorPos--;
                    break;
                case SDLK_RETURN:
                    return 1;
                case SDLK_RIGHT:
                    if(event->key.keysym.mod & KMOD_CTRL) {
                        // Skip to the start of the next word
                        ptr = (edit->buf + edit->cursorPos);
                        for(; isspace(*ptr); ptr++);
                        for(; ((*ptr != '\0') && (!isspace(*ptr))); ptr++);
                        if(*ptr == '\0') {
                            // There is no next word
                            edit->cursorPos = edit->length;
                        } else {
                            edit->cursorPos = ptr - edit->buf;
                        }
                    } else if(edit->cursorPos < edit->size) {
                        edit->cursorPos++;
                    }
                    break;
                case SDLK_LEFT:
                    if(event->key.keysym.mod & KMOD_CTRL) {
                        // Skip to the start of the previous word
                        ptr = (edit->buf + edit->cursorPos);
                        for(; ((isspace(*ptr)) && (ptr > edit->buf)); ptr--);
                        for(; ((!isspace(*ptr)) && (ptr > edit->buf)); ptr--);
                        if(ptr < edit->buf) {
                            ptr = edit->buf;
                        }
                        edit->cursorPos = ptr - edit->buf;
                    } else if(edit->cursorPos > 0) {
                        edit->cursorPos--;
                    }
                    break;
                case SDLK_HOME:
                    edit->cursorPos = 0;
                    break;
                case SDLK_END:
                    edit->cursorPos = edit->length;
                    break;
                case SDLK_DELETE:
                    if(edit->cursorPos < edit->length) {
                        _flubSimpleEditDeleteCharAtPos(edit, edit->cursorPos);
                    }
                    break;
            }
        }
    }
    if(edit->cursorPos > edit->length) {
        edit->cursorPos = edit->length;
    }
    if(edit->cursorPos < 0) {
        edit->cursorPos = 0;
    }
    if(edit->cursorPos > 0) {
        edit->cursorX = fontGetStrNWidth(edit->font, edit->buf, edit->cursorPos);
    } else {
        edit->cursorX = 0;
    }
    return 0;
}
