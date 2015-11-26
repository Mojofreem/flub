#include <flub/logger.h>
#include <flub/gui.h>
#include <flub/widget.h>
#include <flub/memory.h>
#include <flub/module.h>


typedef struct _guiRootNode_s {
    widget_t *widget;
    struct _guiRootNode_s *next;
    struct _guiRootNode_t *prev;
} _guiRootNode_t;

static struct {
    int init;
    _guiRootNode_t *widgets;
    widget_t *focus;
    widget_t *hover;
    int active;
} _guiCtx = {
        .init = 0,
        .widgets = NULL,
        .focus = NULL,
        .active = 0,
    };

int guiInit(appDefaults_t *defaults) {
    if(_guiCtx.init) {
        error("Cannot initialize gui module multiple times.");
        return 1;
    }

    if(!widgetValid()) {
        if(!widgetInit()) {
            return 0;
        }
    }

    if(!layoutValid()) {
        if(layoutInit()) {
            return 0;
        }
    }

    if(!flubGuiThemeValid()) {
        if(!flubGuiThemeInit()) {
            return 0;
        }
    }

    _guiCtx.init = 1;
}
int guiValid(void) {
    return _guiCtx.init;
}

void guiShutdown(void) {
    if(!_guiCtx.init) {
        return;
    }

    _guiCtx.init = 0;
}

static char *_initDeps[] = {"video", "texture", "font", NULL};
static char *_updatePreceed[] = {"video", NULL};
flubModuleCfg_t flubModuleGUI = {
    .name = "gui",
    .init = guiInit,
    .update = guiUpdate,
    .shutdown = guiShutdown,
    .initDeps = _initDeps,
    .updatePreceed = _updatePreceed,
};

void guiWidgetAdd(widget_t *widget) {
    _guiRootNode_t *node;

    // TODO Use memory pool for gui root nodes?
    node = util_calloc(sizeof(_guiRootNode_t), 0, NULL);
    node->widget = widget;
    node->next = _guiCtx.widgets;
    _guiCtx.widgets = node;
}

void guiWidgetRemove(widget_t *widget) {
    _guiRootNode_t *walk, *last;

    for(last = NULL, walk = _guiCtx.widgets;
        ((walk != NULL) && (walk->widget != widget));
        last = walk, walk = walk->next);

    if(walk == NULL) {
        error("Could not remove nonexistent widget from gui module.");
        return;
    }

    if(last == NULL) {
        _guiCtx.widgets = walk->next;
    } else {
        last->next = walk->next;
    }
    util_free(walk);
}

static void _guiClean(void) {
    _guiRootNode_t *node;

    for(node = _guiCtx.widgets; node != NULL; node = node->next) {
        widgetClean(node->widget);
    }
}

int guiUpdate(Uint32 ticks, Uint32 elapsed) {
    _guiRootNode_t *walk;

    for(walk = _guiCtx.widgets; walk != NULL; walk = walk->next) {
        widgetUpdate(walk->widget, ticks);
    }

    for(walk = _guiCtx.widgets; walk != NULL; walk = walk->next) {
        widgetRender(walk->widget);
    }
    return 1;
}

static void _guiInputWalk(widget_t *widget, SDL_Event *event) {
    widget_t *walk;

    if(widget == NULL) {
        return;
    }

    if((widget->handlers != NULL) && (widget->handlers->input != NULL)) {
        widget->handlers->input(widget, 1, event);
    }

    for(walk = widget->child; walk != NULL; walk = walk->next) {
        _guiInputWalk(walk, event);
    }

    for(walk = widget->next; walk != NULL; walk = walk->next) {
        _guiInputWalk(walk, event);
    }
}

int guiInput(SDL_Event *event) {
    _guiRootNode_t *node;
    widget_t *widget;
    int result = 0;
    int x, y;

    // Check mouse over regions for gui elements
#if 0
    if(0) { // Check for mouse events
        // TODO - Obtain the mouse position
        for(node = _guiCtx.widgets; node != NULL; node = node->next) {
            if((x >= node->widget->x) &&
               (x <= (node->widget->x + node->widget->width)) &&
               (y >= node->widget->y) &&
               (y <= (node->widget->y + node->widget->height))) {
                widget = widgetMouseOver(node->widget, x, y);
                if((_guiCtx.hover != NULL) &&
                   (_guiCtx.hover != widget)) {
                    _guiCtx.hover |= WIDGET_FLAG_HOVER;
                    _guiCtx.hover ^= WIDGET_FLAG_HOVER;
                    if(_guiCtx.hover->flags & WIDGET_FLAG_TRIGGER_HOVER) {
                        guiDirty(_guiCtx.hover);
                    }
                    _guiCtx.hover = NULL;
                }
                if(widget->flags & WIDGET_FLAG_TRIGGER_HOVER) {
                    widget->flags |= WIDGET_FLAG_HOVER;
                    guiDirty(widget);
                    _guiCtx.hover = widget;
                }
                break;
            }
        }
    }
#endif

    // TODO - Check global gui commands

    // Walk the focus stack (child to parent)
    widget = _guiCtx.focus;
    while(widget != NULL) {
        if((widget->handlers != NULL) &&
           (widget->handlers->input != NULL)) {
            result |= widget->handlers->input(widget, result, event);
        }
        widget = widget->parent;
    }

    // Walk all widgets, for any passive listeners
    for(node = _guiCtx.widgets; node != NULL; node = node->next) {
        _guiInputWalk(node->widget, event);
    }

    return result;
}

int guiMessage(int widgetId, int signal, void *context) {
    widget_t *widget;

    widget = guiWidgetFind(widgetId);
    if(widget != NULL) {
        return widgetMessageByPtr(widget, signal, context);
    }
    return -1;
}

void guiState(int widgetId, int mode, unsigned int flags) {
    widget_t *widget;

    widget = guiWidgetFind(widgetId);
    if(widget != NULL) {
        widgetStateByPtr(widget, mode, flags);
    }
}

widget_t *guiWidgetFind(int widgetId) {
    _guiRootNode_t *walk;
    widget_t *result;

    for(walk = _guiCtx.widgets; walk != NULL; walk = walk->next) {
        if((result = widgetFind(walk->widget, widgetId)) != NULL) {
            return result;
        }
    }
    return NULL;
}

void guiFocus(int widgetId) {
    widget_t *widget;

    widget = guiWidgetFind(widgetId);
    guiSetFocus(widget);
}

void guiDirty(int widgetId) {
    widget_t *widget;

    widget = guiWidgetFind(widgetId);
    if(widget != NULL) {
        widgetDirty(widget);
    }
}

widget_t *guiGetFocus(void) {
    return _guiCtx.focus;
}

void guiSetFocus(widget_t *widget) {
    if(widget != NULL) {
        if(_guiCtx.focus != NULL) {
            widgetUnfocus(_guiCtx.focus);
            _guiCtx.focus = NULL;
        }
        _guiCtx.focus = widgetFocus(widget);
    }
}
