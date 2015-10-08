#include <flub/app.h>
#include <flub/font.h>
#include <flub/logger.h>
#include <flub/input.h>
#include <flub/video.h>
#include <flub/texture.h>
#include <flub/simpleedit.h>
#include <flub/gfx.h>
#include <stdio.h>
#include <string.h>
#include <flub/memory.h>


#define BULK_EVENT_LIMIT    15

typedef struct appCmdlineCtx_s {
    const char *path;
} appCmdlineCtx_t;

eCmdLineStatus_t appCmdlineHandler(const char *arg, void *context) {
    appCmdlineCtx_t *ctx = (appCmdlineCtx_t *)context;

    ctx->path = arg;

    return eCMDLINE_OK;
}

appDefaults_t appDefaults = {
        .major = 1,
        .minor = 0,
        .title = "FlubThemer",
        .configFile = NULL,
        .bindingFile = NULL,
        .archiveFile = NULL,
        .videoMode = "640x480",
        .fullscreen = "false",
        .allowVideoModeChange = 0,
        .allowFullscreenChange = 1,
        .cmdlineHandler = appCmdlineHandler,
        .cmdlineParamStr = "[outfile]",
        .resources = NULL,
};

typedef enum {
    eThemerStateType = 0,
    eThemerStatePoints,
    eThemerStateName,
    eThemerStateConfirm,
} eThemerState_t;

typedef enum {
    eCompBitmap = 0,
    eCompAnimation,
    eCompTile,
    eCompSlice,
} eCompType_t;

typedef struct tsMenuEntry_s {
    char *name;
    int value;
} tsMenuEntry_t;

#define TS_MENU_LAST    {NULL, 0}

typedef struct tsMenuState_s {
    int pos;
    int length;
    int x;
    int y;
    font_t *font;
    tsMenuEntry_t *menu;
} tsMenuState_t;

void tsMenuInit(tsMenuState_t *state, font_t *font, tsMenuEntry_t *menu, int x, int y) {
    int k;

    for(k = 0; menu[k].name != NULL; k++);

    state->pos = 0;
    state->length = k;
    state->x = x;
    state->y = y;
    state->font = font;
    state->menu = menu;
}

void tsMenuDraw(tsMenuState_t *state) {
    int k;
    int y = state->y;
    int x = state->x;

    glLoadIdentity();
    glEnable(GL_BLEND);
    for(k = 0; (state->menu[k].name != NULL); k++) {
        if(state->pos == (k)) {
            glColor3f(0.5, 1.0, 0.5);
        } else {
            glColor3f(0.2, 0.2, 0.2);
        }
        fontPos(x, y);
        fontBlitStr(state->font, state->menu[k].name);
        y += fontGetHeight(state->font) + 2;
    }
}

int tsMenuInput(tsMenuState_t *state, SDL_Event *event) {
    int result = 0;

    if(event->type == SDL_KEYDOWN) {
        switch(event->key.keysym.sym) {
            case SDLK_ESCAPE:
                result = -1;
                break;
            case SDLK_RETURN:
                result = 1;
                break;
            case SDLK_UP:
                state->pos--;
                break;
            case SDLK_DOWN:
                state->pos++;
                break;
        }
    }
    if(state->pos < 0) {
        state->pos = 0;
    }
    if(state->pos >= state->length) {
        state->pos = state->length - 1;
    }
    return result;
}

void drawTexture(const texture_t *tex, int x, int y) {
    glDisable(GL_TEXTURE);
    glDisable(GL_BLEND);
    glColor3f(1.0, 1.0, 1.0);
    glRecti(x - 2, y - 2, x + tex->width + 2, y + tex->height + 2);
    glColor3f(0.0, 0.0, 0.0);
    glRecti(x - 1, y - 1, x + tex->width + 1, y + tex->height + 1);
    glEnable(GL_TEXTURE);
    glEnable(GL_BLEND);
    glColor3f(1.0, 1.0, 1.0);
    gfxTexBlit(tex, x, y);
}

void drawCrosshair(const texture_t *tex, int x, int y) {
    const int center_x = 7;
    const int center_y = 7;

    glColor3f(1.0, 1.0, 1.0);
    x -= center_x;
    y -= center_y;
    gfxTexBlitSub(tex, 110, 57, 127, 74, x, y, x + 17, y + 17);
}

typedef struct compState_s {
    const texture_t *texture;
    int pos[4][2];
    int valid[4];
    int active;
    int frame;
    int total;
    int delay;
    eCompType_t type;
    int isDirty;
    int isValid;
    int animFrame;
    int animDuration;
    int animGo;
    flubSlice_t *slice;
    struct compState_s *next;
    struct compState_s *parent;
} compState_t;

void tsCompStateInit(compState_t *state, const texture_t *texture, eCompType_t type) {
    memset(state, 0, sizeof(compState_t));
    state->texture = texture;
    state->type = type;
    if(type == eCompAnimation) {
        state->total = 1;
    }
    state->valid[0] = 1;
    state->valid[1] = 1;
    state->valid[2] = 1;
    state->valid[3] = 1;
};

compState_t *tsCompStateClear(compState_t *state) {
    compState_t *parent, *walk, *next;

    if(state->parent != NULL) {
        parent = state->parent;
        state = parent;
        if(parent->next != NULL) {
            for(walk = parent->next; walk != NULL; walk = next) {
                next = walk->next;
                util_free(walk);
            }
        }
    }
    tsCompStateInit(state, state->texture, state->type);
    return state;
}

compState_t *tsCompStateAddFrame(compState_t *state) {
    compState_t *tmp, *walk, *last, *parent;

    if(state->type != eCompAnimation) {
        return state;
    }
    if(state->parent == NULL) {
        parent = state;
    } else {
        parent = state->parent;
    }
    tmp = util_alloc(sizeof(compState_t), NULL);
    tsCompStateInit(tmp, state->texture, state->type);
    tmp->parent = parent;

    last = parent;
    for(walk = parent; walk != NULL; walk = walk->next) {
        last = walk;
    }
    last->next = tmp;
    tmp->frame = last->frame + 1;
    parent->total = tmp->frame + 1;

    return tmp;
}

compState_t *tsCompFrameNext(compState_t *state) {
    if(state->type != eCompAnimation) {
        return state;
    }
    if(state->next == NULL) {
        return state->parent;
    }
    return state->next;
}

void tsCompAnimSet(compState_t *state, int mode) {
    compState_t *walk, *parent;
    int delay = 0;

    if(state->type != eCompAnimation) {
        return;
    }
    if(state->parent == NULL) {
        parent = state;
    } else {
        parent = state->parent;
    }
    if(!mode) {
        parent->animGo = 0;
    } else {
        parent->animGo = 1;
        parent->animFrame = 0;
        parent->animDuration = 0;
    }
}

int tsCompAnimIsGo(compState_t *state) {
    if(state->type != eCompAnimation) {
        return 0;
    }

    if(state->parent == NULL) {
        return state->animGo;
    } else {
        return state->parent->animGo;
    }
}

int tsCompAnimIsValid(compState_t *state) {
    compState_t *walk, *parent;
    int noanim = 0;
    int delay;

    if(state->type != eCompAnimation) {
        return 0;
    }

    if(state->parent == NULL) {
        parent = state;
    } else {
        parent = state->parent;
    }
    if(parent->next == NULL) {
        return 0;
    } else {
        for(walk = parent; walk != NULL; walk = walk->next) {
            if(walk->delay <= 0) {
                return 0;
            }
        }
    }
    return 1;
}

void tsCompAnimToggle(compState_t *state) {
    compState_t *walk, *parent;
    int delay = 0;

    if(state->type != eCompAnimation) {
        return;
    }

    if(tsCompAnimIsGo(state)) {
        tsCompAnimSet(state, 0);
    } else {
        if(tsCompAnimIsValid(state)) {
            tsCompAnimSet(state, 1);
        }
    }
}

void tsCompAnimAdjust(compState_t *state, int adj) {
    compState_t *walk, *parent;
    int noanim = 0;
    int delay;

    if(state->type != eCompAnimation) {
        return;
    }
    state->delay += adj;
    if(state->delay < 0) {
        state->delay = 0;
    }

    if(!tsCompAnimIsValid(state)) {
        tsCompAnimSet(state, 0);
    }
}

void tsCompAnimUpdate(compState_t *state, Uint32 ticks) {
    compState_t *walk, *parent;

    if(state->type != eCompAnimation) {
        return;
    }
    if(state->parent == NULL) {
        parent = state;
    } else {
        parent = state->parent;
    }
    if(parent->animGo) {
        parent->animDuration += ticks;
    } else {
        return;
    }

    for(walk = parent; walk != NULL; walk = walk->next) {
        if(walk->frame == parent->animFrame) {
            while(parent->animDuration >= walk->delay) {
                parent->animDuration -= walk->delay;
                if(walk->next != NULL) {
                    walk = walk->next;
                } else {
                    walk = walk->parent;
                }
                parent->animFrame = walk->frame;
            }
            return;
        }
    }
    parent->animFrame = 0;
    parent->animDuration = 0;
}

compState_t *tsCompAnimFrame(compState_t *state) {
    compState_t *walk, *parent;

    if(state->type != eCompAnimation) {
        return state;
    }
    if(state->parent == NULL) {
        parent = state;
    } else {
        parent = state->parent;
    }
    for(walk = parent; walk != NULL; walk = walk->next) {
        if(walk->frame == parent->animFrame) {
            return walk;
        }
    }
    return state;
}

void tsCompPointInfo(compState_t *state, font_t *font, int x, int y) {
    int k;
    int w, h;
    int valid = 1;

    fontMode();
    fontPos(x, y);
    glColor3f(0.3, 0.3, 0.3);
    fontBlitStr(font, "Frame ");
    glColor3f(1.0, 1.0, 1.0);
    fontBlitInt(font, state->frame + 1);
    glColor3f(0.3, 0.3, 0.3);
    fontBlitStr(font, "/");
    glColor3f(1.0, 1.0, 1.0);
    if(state->parent == NULL) {
        fontBlitInt(font, state->total);
    } else {
        fontBlitInt(font, state->parent->total);
    }

    y += (fontGetHeight(font) * 2);

    for(k = 0; k < ((state->type == eCompSlice) ? 4 : 2); k++) {
        if(k == state->active) {
            if(state->valid[k]) {
                glColor3f(0.0, 1.0, 0.0);
            } else {
                glColor3f(5.0, 1.0, 0.0);
            }
        } else {
            if(state->valid[k]) {
                glColor3f(0.3, 0.5, 0.3);
            } else {
                glColor3f(0.5, 0.3, 0.3);
            }
        }
        fontPos(x, y);
        fontBlitStrf(font, "%d: (%3d, %3d)", k + 1, state->pos[k][0], state->pos[k][1]);
        y += fontGetHeight(font);
    }
    y += fontGetHeight(font);
    fontPos(x, y);
    glColor3f(0.3, 0.3, 0.3);
    if(state->type == eCompSlice) {
        if((!state->valid[1]) || (!state->valid[2]) || (!state->valid[3])) {
            valid = 0;
        }
        w = state->pos[3][0] - state->pos[0][0];
        h = state->pos[3][1] - state->pos[0][1];
    } else {
        if(!state->valid[1]) {
            valid = 0;
        }
        w = state->pos[1][0] - state->pos[0][0];
        h = state->pos[1][1] - state->pos[0][1];
    }
    w++;
    h++;
    if(valid) {
        fontBlitStrf(font, "Size: %dx%d", w, h);
    } else {
        fontBlitStrf(font, "Size: --x--");
    }
    if(state->type == eCompAnimation) {
        y += (fontGetHeight(font) * 2);
        fontPos(x, y);
        fontBlitStr(font, "Delay: ");
        glColor3f(1.0, 1.0, 1.0);
        fontBlitStrf(font, "%4d ms", state->delay);
    }
}

void tsCompCheckPosValid(compState_t *state, int pos) {
    if((pos <= 0) || (pos >= 4)) {
        return;
    }
    if((!state->valid[pos - 1]) ||
       (state->pos[pos - 1][0] > state->pos[pos][0]) ||
       (state->pos[pos - 1][1] > state->pos[pos][1])) {
        state->valid[pos] = 0;
    } else {
        state->valid[pos] = 1;
    }
}

void tsCompCheckValid(compState_t *state) {
    int a, b;

    tsCompCheckPosValid(state, 1);
    tsCompCheckPosValid(state, 2);
    tsCompCheckPosValid(state, 3);
}

void tsCompPosUpdate(compState_t *state, int adjX, int adjY, int shift, int ctrl, int *x, int *y) {
    state->pos[state->active][0] += (adjX * (shift ? 10 : (ctrl ? 16 : 1)));
    state->pos[state->active][1] += (adjY * (shift ? 10 : (ctrl ? 16 : 1)));

    if(state->pos[state->active][0] < 0) {
        state->pos[state->active][0] = 0;
    }
    if(state->pos[state->active][1] < 0) {
        state->pos[state->active][1] = 0;
    }
    if(state->pos[state->active][0] >= state->texture->width) {
        state->pos[state->active][0] = state->texture->width - 1;
    }
    if(state->pos[state->active][1] >= state->texture->height) {
        state->pos[state->active][1] = state->texture->height - 1;
    }
    tsCompCheckValid(state);
    *x = state->pos[state->active][0];
    *y = state->pos[state->active][1];

    if(((adjX) || (adjY)) && (state->type == eCompSlice)) {
        state->isDirty = 1;
    }
}

void tsCompNextPoint(compState_t *state, int *x, int *y) {
    int last = state->active;
    state->active++;
    if(state->active >= ((state->type == eCompSlice) ? 4 : 2)) {
        state->active = 0;
    }
    if((state->pos[state->active][0] == 0) &&
               (state->pos[state->active][1] == 0)) {
        state->pos[state->active][0] = state->pos[last][0];
        state->pos[state->active][1] = state->pos[last][1];
    }
    *x = state->pos[state->active][0];
    *y = state->pos[state->active][1];
}

void drawFragmentBackground(int pos, int invert) {
    int x, y;

    x = (pos * 10) + 5 + (pos * 150);
    y = 480 - 150 - 5;

    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    glColor4f(1.0, 1.0, 1.0, 1.0);
    glRecti(x - 2, y - 2, x + 150 + 2, y + 150 + 2);
    glColor3f(0.0, 0.0, 0.0);
    glRecti(x - 1, y - 1, x + 150 + 1, y + 150 + 1);

    if(invert) {
        glColor3f(1.0, 1.0, 1.0);
    } else {
        glColor3f(0.0, 0.0, 0.0);
    }
    glRecti(x, y, x + 150, y + 150);
    glEnable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
}

void drawFragmentView(compState_t *state, int x, int y, int scale) {
    int w, h;

    state = tsCompAnimFrame(state);

    if(!state->valid[1]) {
        glDisable(GL_BLEND);
        glDisable(GL_TEXTURE_2D);
        glColor3f(1.0, 0.0, 0.0);
        glRecti(x - 10, y - 10, x + 10, y + 10);
        glEnable(GL_BLEND);
        glEnable(GL_TEXTURE_2D);
        return;
    }
    glColor3f(1.0, 1.0, 1.0);

    if(state->type == eCompSlice) {
        if(state->isDirty) {
            if(state->slice != NULL) {
                gfxSliceDestroy(state->slice);
                state->slice = NULL;
            }
            state->slice = gfxSliceCreate(state->texture,
                                          state->pos[0][0], state->pos[0][1],
                                          state->pos[1][0], state->pos[1][1],
                                          state->pos[2][0], state->pos[2][1],
                                          state->pos[3][0], state->pos[3][1]);
            state->isDirty = 0;
        }
        w = state->pos[3][0] - state->pos[0][0] + 1;
        h = state->pos[3][1] - state->pos[0][1] + 1;
        if((w < 50) && (state->slice != NULL) && (!(state->slice->locked & GFX_X_LOCKED))) {
            if(scale) {
                w = 100;
            } else {
                w = 50;
            }
        }
        if((h < 50) && (state->slice != NULL) && (!(state->slice->locked & GFX_Y_LOCKED))) {
            if(scale) {
                h = 100;
            } else {
                h = 50;
            }
        }
    } else {
        w = state->pos[1][0] - state->pos[0][0] + 1;
        h = state->pos[1][1] - state->pos[0][1] + 1;
    }
    if(state->type == eCompTile) {
        if((50 / w) > 1) {
            w = (50 / w) * w;
        }
        if((50 / h) > 1) {
            h = (50 / h) * h;
        }
    }
    if((scale) && (state->type != eCompSlice)) {
        x -= w;
        y -= h;
        w *= 2;
        h *= 2;
    } else {
        x -= (w / 2);
        y -= (h / 2);
    }
    if(state->type == eCompSlice) {
        if(state->slice != NULL) {
            gfxSliceBlit(state->slice, x, y, x + w, y + h);
        }
    } else if(state->type == eCompTile) {
        gfxTexTile(state->texture, state->pos[0][0], state->pos[0][1],
                   state->pos[1][0], state->pos[1][1], x, y, x + w, y + h);
    } else {
        gfxTexBlitSub(state->texture, state->pos[0][0], state->pos[0][1],
                      state->pos[1][0], state->pos[1][1], x, y, x + w, y + h);
    }
}

void drawFragment(const texture_t *tex, flubSlice_t *slice, compState_t *state) {
    int k;
    int x;
    int y;

    // Draw border
    gfxSliceBlit(slice, 0, 479 - 150 - 10, 639, 479);

    // Draw sections
    y = 480;
    y -= 5;
    y -= (150 / 2);
    for(k = 0; k < 4; k++) {
        drawFragmentBackground(k, k % 2);
        x = 5;
        x += (k * 150);
        x += (10 * k);
        x += (150 / 2);
        drawFragmentView(state, x, y, ((k >= 2) ? 1 : 0));
    }
}

int main(int argc, char *argv[]) {
    font_t *font;
    const texture_t *texture;
    const texture_t *misc;
    flubSlice_t *dlg_body;
    flubSlice_t *dlg_title;
    eCmdLineStatus_t status;
    int keepGoing = 1;
    Uint32 lastTick;
    Uint32 current;
    Uint32 elapsed;
    SDL_Event ev;
    int eventCount;
    FILE *fp;
    eThemerState_t state = eThemerStateType;
    tsMenuState_t menuState;
    tsMenuEntry_t menuEntries[] = {
            {"Bitmap", eCompBitmap},
            {"Animation", eCompAnimation},
            {"Tile", eCompTile},
            {"Slice", eCompSlice},
            {NULL, 0},
        };
    int pos = 0;
    int curx = 0;
    int cury = 0;
    compState_t parentComp, *comp, *walk;
    flubSimpleEdit_t *edit;
    const char *path = "flub-theme.txt";
    appCmdlineCtx_t appCtx = {.path = NULL};

    if(!appInit(argc, argv)) {
        return 1;
    }

    // Register command line params and config vars

    if((status = appStart(&appCtx)) != eCMDLINE_OK) {
        return ((status == eCMDLINE_EXIT_SUCCESS) ? 0 : 1);
    }
    if(appCtx.path != NULL) {
        path = appCtx.path;
    }

    if((fp = fopen(path, "a+t")) == NULL) {
        errorf("Failed to open theme output file.");
        return 1;
    } else {
        infof("Opened output file \"%s\"", path);
    }

    if((font = fontGet("consolas", 12, 0)) == NULL) {
        infof("Unable to get font");
        return 1;
    }

    texture = texmgrGet("flub-simple-gui");
    misc = texmgrGet("flub-keycap-misc");
    dlg_body = gfxSliceCreate(misc, 41, 29, 43, 30, 60, 40, 62, 42);
    dlg_title = gfxSliceCreate(misc, 41, 17, 43, 19, 60, 26, 62, 28);

    inputActionBind("KEY_BACKQUOTE", "showconsole");

    tsMenuInit(&menuState, font, menuEntries, 310, 45);
    edit = flubSimpleEditCreate(font, 30, 310, 70);
    tsCompStateInit(&parentComp, texture, menuState.pos);
    comp = &parentComp;

    lastTick = SDL_GetTicks();
    while (keepGoing) {
        current = SDL_GetTicks();
        elapsed = current - lastTick;
        lastTick = current;

        tsCompAnimUpdate(comp, elapsed);

        eventCount = 0;
        while(inputPollEvent(&ev)) {
            eventCount++;
            if(ev.type == SDL_QUIT) {
                keepGoing = 0;
                break;
            }

            if(ev.type == SDL_KEYDOWN) {
                if(ev.key.keysym.sym == SDLK_ESCAPE) {
                    state = eThemerStateConfirm;
                    break;
                } else if(ev.key.keysym.sym == SDLK_HOME) {
                    videoScreenshot("screenshot");
                    break;
                }
            }

            switch(state) {
                default:
                case eThemerStateType:
                    if(tsMenuInput(&menuState, &ev)) {
                        state = eThemerStatePoints;
                        comp = tsCompStateClear(comp);
                        curx = 0;
                        cury = 0;
                        tsCompStateInit(comp, texture, menuState.pos);
                    }
                    break;
                case eThemerStatePoints:
                    if(ev.type == SDL_KEYDOWN) {
                        switch (ev.key.keysym.sym) {
                            case SDLK_END:
                                flubSimpleEditSet(edit, "comp");
                                flubSimpleEditActive(edit, 1);
                                state = eThemerStateName;
                                break;
                            case SDLK_INSERT:
                                comp = tsCompStateAddFrame(comp);
                                break;
                            case SDLK_EQUALS:
                                comp = tsCompFrameNext(comp);
                                break;
                            case SDLK_BACKSLASH:
                                tsCompAnimToggle(comp);
                                break;
                            case SDLK_PAGEUP:
                                if(ev.key.keysym.mod & KMOD_SHIFT) {
                                    tsCompAnimAdjust(comp, 10);
                                } else {
                                    tsCompAnimAdjust(comp, 1);
                                }
                                break;
                            case SDLK_PAGEDOWN:
                                if(ev.key.keysym.mod & KMOD_SHIFT) {
                                    tsCompAnimAdjust(comp, -10);
                                } else {
                                    tsCompAnimAdjust(comp, -1);
                                }
                                if(comp->delay < 0) {
                                    comp->delay = 0;
                                }
                                break;
                            case SDLK_UP:
                                tsCompPosUpdate(comp, 0, -1,
                                                ((ev.key.keysym.mod & KMOD_SHIFT) ? 1 : 0),
                                                ((ev.key.keysym.mod & KMOD_CTRL) ? 1 : 0),
                                                &curx, &cury);
                                break;
                            case SDLK_DOWN:
                                tsCompPosUpdate(comp, 0, 1,
                                                ((ev.key.keysym.mod & KMOD_SHIFT) ? 1 : 0),
                                                ((ev.key.keysym.mod & KMOD_CTRL) ? 1 : 0),
                                                &curx, &cury);
                                break;
                            case SDLK_LEFT:
                                tsCompPosUpdate(comp, -1, 0,
                                                ((ev.key.keysym.mod & KMOD_SHIFT) ? 1 : 0),
                                                ((ev.key.keysym.mod & KMOD_CTRL) ? 1 : 0),
                                                &curx, &cury);
                                break;
                            case SDLK_RIGHT:
                                tsCompPosUpdate(comp, 1, 0,
                                                ((ev.key.keysym.mod & KMOD_SHIFT) ? 1 : 0),
                                                ((ev.key.keysym.mod & KMOD_CTRL) ? 1 : 0),
                                                &curx, &cury);
                                break;
                            case SDLK_RETURN:
                                tsCompNextPoint(comp, &curx, &cury);
                                break;
                        }
                    }
                    break;
                case eThemerStateName:
                    if(flubSimpleEditInput(edit, &ev)) {
                        // Save the entry
                        if(comp->type == eCompSlice) {
                            fprintf(fp,"%s %d %d %d %d %d %d %d %d\n", edit->buf,
                                    comp->pos[0][0], comp->pos[0][1],
                                    comp->pos[1][0], comp->pos[1][1],
                                    comp->pos[2][0], comp->pos[2][1],
                                    comp->pos[3][0], comp->pos[3][1]);
                        } else if(comp->type == eCompAnimation) {
                            fprintf(fp, "%s %d %d ", edit->buf,
                                    (comp->pos[1][0] - comp->pos[0][0] + 1),
                                    (comp->pos[1][1] - comp->pos[0][1]));
                            if(comp->parent == NULL) {
                                walk = comp;
                            } else {
                                walk = comp->parent;
                            }
                            for(; walk != NULL; walk = walk->next) {
                                fprintf(fp, "%d %d %d ", walk->pos[0][0], walk->pos[0][1], walk->delay);
                            }
                        } else {
                            fprintf(fp,"%s %d %d %d %d\n", edit->buf,
                                    comp->pos[0][0], comp->pos[0][1],
                                    comp->pos[1][0], comp->pos[1][1]);
                        }
                        state = eThemerStateType;
                        break;
                    }
                    break;
                case eThemerStateConfirm:
                    if(ev.type == SDL_KEYDOWN) {
                        switch (ev.key.keysym.sym) {
                            case SDLK_END:
                                keepGoing = 0;
                                break;
                            case SDLK_RETURN:
                                state = eThemerStateType;
                                break;
                        }
                        break;
                    }
                    break;
            }
            if(eventCount >= BULK_EVENT_LIMIT) {
                break;
            }
        }
        videoClear();

        videoPushGLState();
        videoOrthoMode();

        glLoadIdentity();
        glColor3f(1.0, 1.0, 1.0);

        // Title
        gfxSliceBlit(dlg_title, 0, 0, 639, 20);
        fontMode();
        fontPos(5, 5);
        fontSetColor(0.0, 0.0, 0.0);
        fontBlitStr(font, "Flub Themer");

        // Image
        glLoadIdentity();
        drawTexture(texture, 10, 30);

        // State name
        fontMode();
        fontSetColor(1.0, 1.0, 1.0);
        fontPos(300, 25);

        switch(state) {
            default:
            case eThemerStateType:
                fontBlitStr(font, "Select component type:");
                tsMenuDraw(&menuState);
                break;
            case eThemerStatePoints:
                fontBlitStr(font, "Select points:");
                drawFragment(texture, dlg_body, comp);
                drawCrosshair(misc, curx + 10, cury + 30);
                fontMode();
                fontPos(5, 300);
                fontSetColor(1.0, 1.0, 1.0);
                fontBlitStrf(font, "X: %3d  Y: %3d", curx, cury);
                tsCompPointInfo(comp, font, 310, 50);
                if((comp->type == eCompAnimation) &&
                   (((comp->parent != NULL) && (comp->parent->animGo)) ||
                    (comp->animGo))) {
                    fontPos(310, 250);
                    fontSetColor(1.0, 1.0, 1.0);
                    fontBlitStrf(font, "Animating");
                }
                break;
            case eThemerStateName:
                fontBlitStr(font, "Select component type:");
                fontMode();
                fontPos(310, 50);
                fontSetColor(1.0, 1.0, 1.0);
                fontBlitStr(font, "Enter component name");
                flubSimpleEditDraw(edit);
                break;
            case eThemerStateConfirm:
                fontBlitStr(font, "Are you sure you want to quit?");
                gfxBlitKeyStr(font, "Press [End] to exit, [Return] to continue.", 310, 50, NULL, NULL);
                break;
        }

        if(!appUpdate(current)) {
            keepGoing = 0;
        }
    }
    fclose(fp);
    return 0;
}