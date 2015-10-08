#include <flub/sdl.h>
#include <flub/logger.h>
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static struct {
    int init;
    char shiftMap[127];
} _sdlCtx = {
    .init = 0,
    // .shiftMap
};


static void _flubSDLShiftMapInit(void) {
    int k;

    memset(_sdlCtx.shiftMap, 0, sizeof(_sdlCtx.shiftMap));
    for(k = 'a'; k <='z'; k++) {
        _sdlCtx.shiftMap[k] = k + ('A' - 'a');
    }
    _sdlCtx.shiftMap['1'] = '!';
    _sdlCtx.shiftMap['2'] = '@';
    _sdlCtx.shiftMap['3'] = '#';
    _sdlCtx.shiftMap['4'] = '$';
    _sdlCtx.shiftMap['5'] = '%';
    _sdlCtx.shiftMap['6'] = '^';
    _sdlCtx.shiftMap['7'] = '&';
    _sdlCtx.shiftMap['8'] = '*';
    _sdlCtx.shiftMap['9'] = '(';
    _sdlCtx.shiftMap['0'] = ')';
    _sdlCtx.shiftMap['-'] = '_';
    _sdlCtx.shiftMap['='] = '+';
    _sdlCtx.shiftMap['['] = '{';
    _sdlCtx.shiftMap[']'] = '}';
    _sdlCtx.shiftMap['\\'] = '|';
    _sdlCtx.shiftMap[';'] = ':';
    _sdlCtx.shiftMap['\''] = '"';
    _sdlCtx.shiftMap[','] = '<';
    _sdlCtx.shiftMap['.'] = '>';
    _sdlCtx.shiftMap['/'] = '?';
}

int flubSDLInit(void) {
    if(_sdlCtx.init) {
        warning("Ignoring attempt to re-initialize SDL.");
        return 1;
    }

    if(!logValid()) {
        // The logger has not been initialized!
        return 0;
    }

    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO) < 0) {
        fatalf("Failed to initialize SDL: %s", SDL_GetError());
        return 0;
    }

    logRedirectSDLLogs();

    _flubSDLShiftMapInit();

    _sdlCtx.init = 1;

    return 1;
}

int flubSDLValid(void) {
    return _sdlCtx.init;
}

void flubSDLShutdown(void) {
    if(!_sdlCtx.init) {
        return;
    }

    logStopRedirectSDLLogs();

    SDL_Quit();

    _sdlCtx.init = 0;
}

int flubSDLTextInputFilter(SDL_Event *event, char *c) {
    int k;

    if(event->type == SDL_KEYDOWN) {
        switch(event->key.keysym.sym) {
            case SDLK_BACKQUOTE:
            case SDLK_TAB:
            case SDLK_CAPSLOCK:
            case SDLK_BACKSPACE:
            case SDLK_RETURN:
            case SDLK_UP:
            case SDLK_DOWN:
            case SDLK_RIGHT:
            case SDLK_LEFT:
            case SDLK_HOME:
            case SDLK_END:
            case SDLK_DELETE:
            case SDLK_PAGEUP:
            case SDLK_PAGEDOWN:
                return 0;
            default:
                // Process the key
                if((isprint(event->key.keysym.sym) && (event->key.keysym.sym >= 32))) {
                    k = event->key.keysym.sym;
                    if(event->key.keysym.mod & KMOD_SHIFT) {
                        if(_sdlCtx.shiftMap[k] != 0) {
                            k = _sdlCtx.shiftMap[k];
                        }
                    } else if(event->key.keysym.mod & KMOD_CAPS) {
                        if((k >= 'a') && (k <= 'z')) {
                            k += 'A' - 'a';
                        }
                    }
                    if(c != NULL) {
                        *c = k;
                    }
                    return 1;
                }
                break;
        }
    }
    return 0;
}
