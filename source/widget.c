#include <flub/logger.h>
#include <flub/memory.h>
#include <flub/widget.h>


struct {
    int init;
} _widgetCtx = {
        .init = 0,
    };


int widgetInit(void) {
    if(_widgetCtx.init) {
        warning("Cannot initialize widget module multiple times.");
        return 1;
    }

    _widgetCtx.init = 1;

    return 1;
}

int widgetValid(void) {
    return _widgetCtx.init;
}

void widgetShutdown(void) {
    if(!_widgetCtx.init) {
        return;
    }
}

widget_t *widgetCreate(int id, int type, int x, int y, int width, int height,
                       unsigned int flags, flubGuiTheme_t *theme,
                       widgetHandlers_t *handlers, layoutNode_t *layout,
                       int dataSize) {
    widget_t *widget;

    widget = util_calloc(sizeof(widget_t) + dataSize, 0, NULL);

    widget->id = id;
    widget->type = type;
    widget->x = x;
    widget->y = y;

    widget->handlers = handlers;
    if((widget->handlers != NULL) && (widget->handlers->init != NULL)) {
        widget->handlers->init(widget);
    }

    widgetThemeApply(widget, theme);

    widgetAddToLayout(widget, layout);

    if((widget->handlers != NULL) && (widget->handlers->state != NULL)) {
        widget->handlers->state(widget, 1, flags);
    }

    widgetResize(widget, width, height);

    return widget;
}

static void _widgetOrphan(widget_t *widget) {
    widget_t *walk, *last;

    if(widget->parent != NULL) {
        last = NULL;
        for(walk = widget->parent->child;
            ((walk != NULL) && (walk != widget));
            last = walk, walk = walk->next);
        if(walk != widget) {
            error("Widget was not found in parent's child list during orphaning.");
        } else {
            if(last == NULL) {
                widget->parent->child = widget->next;
            } else {
                last->next = widget->next;
            }
            widget->next = NULL;
        }
    }
}

void widgetDestroy(widget_t *widget) {
    widget_t *walk, *last;

    if(widget == NULL) {
        return;
    }

    widgetThemeRelease(widget);

    _widgetOrphan(widget);

    while(widget->child != NULL) {
        widgetDestroy(widget->child);
    }

    if(widget->next != NULL) {
        widgetDestroy(widget->next);
    }

    if((widget->handlers != NULL) && (widget->handlers->destroy != NULL)) {
        widget->handlers->destroy(widget);
    }

    util_free(widget);
}

void widgetChildAdd(widget_t *widget, widget_t *child) {
    widget_t *walk;

    _widgetOrphan(child);
    
    if(widget->child == NULL) {
        widget->child = child;
    } else {
        for(walk = widget->child; walk->next != NULL; walk = walk->next);
        walk->next = child;

    }
    child->parent = widget;
}

void widgetNextAdd(widget_t *widget, widget_t *next) {
    widget_t *walk;

    _widgetOrphan(next);

    if(widget->next == NULL) {
        widget->next = next;
    } else {
        for(walk = widget->next; walk->next != NULL; walk = walk->next);
        walk->next = next;

    }
    next->parent = widget;
}

void widgetThemeApply(widget_t *widget, flubGuiTheme_t *theme) {
    widget_t *walk;

    if(widget->theme != NULL) {
        widgetThemeRelease(widget);
    }

    if((widget->handlers != NULL) && (widget->handlers->apply != NULL)) {
        widget->handlers->apply(widget, theme);
    } else {
        widget->theme = theme;
    }

    for(walk = widget->child; walk != NULL; walk = walk->next) {
        widgetThemeApply(walk, theme);
    }

    widgetDirty(widget);
}

void widgetThemeRelease(widget_t *widget) {
    if((widget->handlers != NULL) && (widget->handlers->apply != NULL)) {
        widget->handlers->apply(widget, NULL);
    } else {
        widget->theme = NULL;
    }
}

void widgetAddToLayout(widget_t *widget, layoutNode_t *layout) {
    widget->layout = layout;
    // TODO Register the widget with the layout
}

void widgetRemoveFromLayout(widget_t *widget) {
    // TODO Unregister the widget with the layout
    widget->layout = NULL;
}

widget_t *widgetFind(widget_t *widget, int widgetId) {
    widget_t *walk, *result;

    if(widget->id == widgetId) {
        return widget;
    }

    for(walk = widget->child; walk != NULL; walk = walk->next) {
        if((result = widgetFind(walk, widgetId)) != NULL) {
            return result;
        }
    }

    if(widget->next != NULL) {
        return widgetFind(widget->next, widgetId);
    }

    return NULL;
}

widget_t *widgetFocus(widget_t *widgets) {
    if((widgets->handlers != NULL) && (widgets->handlers->state != NULL)) {
        widgets->handlers->state(widgets, 1, WIDGET_FLAG_FOCUS);
    } else {
        widgets->flags |= WIDGET_FLAG_FOCUS;
    }

    widgetDirty(widget);

    return widgets;
}

void widgetUnfocus(widget_t *widgets) {
    if((widgets->handlers != NULL) && (widgets->handlers->state != NULL)) {
        widgets->handlers->state(widgets, 0, WIDGET_FLAG_FOCUS);
    } else {
        widgets->flags |= WIDGET_FLAG_FOCUS;
        widgets->flags ^= WIDGET_FLAG_FOCUS;
    }

    widgetDirty(widget);
}

widget_t *widgetMouseOver(widget_t *widgets, int x, int y) {
    widget_t *walk, *result;

    if((x >= widgets->x) && (y >= widgets->y) &&
       (x <= (widgets->x + widget->width)) &&
       (y <= (widgets->y + widget->height))) {
        for(walk = widgets->child; walk != NULL; walk = walk->next) {
            if((result = widgetMouseOver(walk, x, y)) != NULL) {
                return result;
            }
        }
        if(!(widgets->flags & WIDGET_FLAG_TRIGGER_HOVER)) {
            if((widgets->handlers != NULL) && (widgets->handlers->state != NULL)) {
                widgets->handlers->state(widgets, 1, WIDGET_FLAG_HOVER);
                widgetDirty(widgets);
                return widgets;
            }
        }
    }
    return NULL;
}

void widgetDirty(widget_t *widget) {
    widget->flags |= WIDGET_FLAG_DIRTY;
}

void widgetClean(widget_t *widget) {
    widget_t *walk;

    widget->flags |= WIDGET_FLAG_DIRTY;
    widget->flags ^= WIDGET_FLAG_DIRTY;

    for(walk = widget->child; walk != NULL; walk = walk->next) {
        widgetClean(walk);
    }
}

void widgetRender(widget_t *widget, gfxMeshObj_t *mesh) {
    widget_t *walk;

    if(widget == NULL) {
        return;
    }

    if((widget->handlers != NULL) && (widget->handlers->render != NULL)) {
        widget->handlers->render(widget);
    }

    for(walk = widget->child; walk != NULL; walk = walk->next) {
        widgetRender(walk, mesh);
    }
}

void widgetMove(widget_t *widget, int x, int y) {
    if(widget == NULL) {
        return;
    }

    widget->x = x;
    widget->y = y;
    widgetResize(widget, widget->width, width->height);
}

void widgetResize(widget_t *widget, int width, int height) {
    if(widget == NULL) {
        return;
    }

    if((widget->handlers != NULL) && (widget->handlers->resize != NULL)) {
        widget->handlers->resize(widget, width, height);
    } else {
        widget->width = width;
        widget->height = height;
    }
    widgetDirty(widget);
}

void widgetUpdate(widget_t *widget, Uint32 ticks) {
    widget_t *walk;

    if(widget == NULL) {
        return;
    }

    if((widget->handlers != NULL) && (widget->handlers->update != NULL)) {
        if(widget->handlers->update(widget, ticks)) {
            widgetDirty(widget);
        }
    }

    for(walk = widget->child; walk != NULL; walk = walk->next) {
        widgetUpdate(walk, ticks);
    }
}

int widgetInput(widget_t *widget, int passive, SDL_Event *event) {
    if(widget == NULL) {
        return 0;
    }

    if((widget->handlers != NULL) &&
       (widget->handlers->input != NULL)) {
        return widget->handlers->input(widget, passive, event);
    }

    return 0;
}

int widgetMessageByPtr(widget_t *widget, int signal, void *context) {
    if(widget == NULL) {
        return 0;
    }

    if((widget->handlers != NULL) &&
       (widget->handlers->message)) {
        return widget->handlers->message(widget, signal, context);
    }
    return -1;
}

int widgetMessageById(widget_t *widget, int widgetId, int signal, void *context) {
    if((widget = widgetFind(widget, widgetId)) == NULL) {
        return 0;
    }
    return widgetMessageByPtr(widget, signal, context);
}

int widgetStateByPtr(widget_t *widget, int mode, unsigned int flags) {
    if(widget == NULL) {
        return 0;
    }

    if((widget->handlers != NULL) &&
       (widget->handlers->state != NULL)) {
        return widget->handlers->state(widget, mode, flags);
    } else {
        widget->flags |= flags;
        if(!mode) {
            widget->flags ^= flags;
        }
    }
}

int widgetStateById(widget_t *widget, int widgetId, int mode, unsigned int flags) {
    if((widget = widgetFind(widget, widgetId)) == NULL) {
        return 0;
    }
    return widgetStateByPtr(widget, mode, flags);
}
