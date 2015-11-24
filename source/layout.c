#include <flub/layout.h>
#include <flub/memory.h>
#include <flub/logger.h>
#include <string.h>
#include <flub/util/misc.h>


struct {
    int init;
    memPool_t *pool;
} _layoutCtx = {
        .init = 0,
        .pool = NULL,
    };


int layoutInit(void) {
    if(_layoutCtx.init) {
        error("Cannot initialize the layout module multiple times.");
        return 1;
    }

    _layoutCtx.pool = memPoolCreate(sizeof(layoutNode_t), 200);

    _layoutCtx.init = 1;

    return 1;
}

int layoutValid(void) {
    return _layoutCtx.init;
}

void layoutShutdown(void) {
    if(!_layoutCtx.init) {
        return;
    }

    // TODO Free a mempool

    _layoutCtx.init = 0;
}

layoutNode_t *layoutNodeCreate(int id, int width, int height,
                               int minWidth, int maxWidth,
                               int minHeight, int maxHeight,
                               int weight,
                               Sint8 padLeft, Sint8 padTop, Sint8 padRight, Sint8 padBottom,
                               unsigned int flags, layoutCallbacks_t *callbacks,
                               void *context) {
    layoutNode_t *node;

    if(utilNumBitsSet(flags & LAYOUT_WIDTH_MASK) > 1) {
        error("Multiple conflicting width specifiers in layout node create");
        return NULL;
    }

    if(utilNumBitsSet(flags & LAYOUT_HEIGHT_MASK) > 1) {
        error("Multiple conflicting height specifiers in layout node create");
        return NULL;
    }

    if((flags & LAYOUT_CONTAINER_HORIZONTAL) && (flags & LAYOUT_CONTAINER_VERTICAL)) {
        error("Layout node cannot be both a horizontal and vertical container");
        return NULL;
    }

    if(!(flags & (LAYOUT_CONTAINER_HORIZONTAL | LAYOUT_CONTAINER_VERTICAL))) {
        flags |= LAYOUT_CONTAINER_HORIZONTAL;
    }

    if(flags & LAYOUT_FIXED_WIDTH) {
        minWidth = width;
        maxWidth = width;
    }

    if(flags & LAYOUT_FIXED_HEIGHT) {
        minHeight = height;
        maxHeight = height;
    }

    if(minWidth > maxWidth) {
        errorf("Layout node min width (%d) is greater than max width (%d)", minWidth, maxWidth);
        return NULL;
    }

    if(minHeight > maxHeight) {
        errorf("Layout node min height (%d) is greater than max height (%d)", minHeight, maxHeight);
        return NULL;
    }

    node = memPoolAlloc(_layoutCtx.pool);
    node->id = id;

    node->width = width;
    node->height = height;
    node->baseWidth = width;
    node->baseHeight = height;
    node->minWidth = minWidth;
    node->maxWidth = maxWidth;
    node->minHeight = minHeight;
    node->maxHeight = maxHeight;

    node->weight = weight;

    node->padding[LAYOUT_LEFT] = padLeft;
    node->padding[LAYOUT_TOP] = padTop;
    node->padding[LAYOUT_RIGHT] = padRight;
    node->padding[LAYOUT_BOTTOM] = padBottom;

    node->flags = flags;

    node->callbacks = callbacks;
    node->context = context;

    return node;
}

void layoutNodeDestroy(layoutNode_t *node, void *context) {
    layoutNode_t *walk, *last;

    if(node->parent != NULL) {
        for(walk = node->parent, last = NULL;
            ((walk != NULL) && (walk != node));
            last = walk, walk = walk->next);
        if(walk == NULL) {
            error("Layout node's parent does not reference node as child");
        } else {
            if(last == NULL) {
                node->parent->child = node->next;
            } else {
                last->next = node->next;
            }
            node->next = NULL;
        }
    }

    while(node->child != NULL) {
        layoutNodeDestroy(node->child, context);
    }

    while(node->next != NULL) {
        layoutNodeDestroy(node->next, context);
    }

    if((node->callbacks != NULL) && (node->callbacks->destroy != NULL)) {
        node->callbacks->destroy(node, context);
    } else {
        if(node->context != NULL) {
            error("Layout node destroyed with context and without handler.");
        }
    }

    memPoolFree(_layoutCtx.pool, node);
}

static void _layoutNodeRecalcContainer(layoutNode_t *node) {
    layoutNode_t *walk;
    int wantsGrowWeight = 0;

    node->containerMinimum = 0;
    node->containerPreferred = 0;
    node->growNodes = 0;
    node->shrinkNodes = 0;
    node->growWeight = 0;
    node->wantsGrowthNodes = 0;
    node->shrinkWeight = 0;

    // Determine child sizing
    for(walk = node->child; walk != NULL; walk = walk->next) {
        if(walk->flags & LAYOUT_HIDDEN) {
            continue;
        }
        if(node->flags & LAYOUT_CONTAINER_VERTICAL) {
            switch(walk->flags & LAYOUT_HEIGHT_MASK) {
                case LAYOUT_FIXED_HEIGHT:
                    node->containerMinimum += walk->baseHeight;
                    node->containerPreferred += walk->baseHeight;
                    break;
                case LAYOUT_MINIMUM_HEIGHT:
                    node->containerMinimum += walk->minHeight;
                    node->containerPreferred += walk->baseHeight;
                    node->growNodes++;
                    node->growWeight += walk->weight;
                    break;
                case LAYOUT_MAXIMUM_HEIGHT:
                    node->containerMinimum += walk->minHeight;
                    node->containerPreferred += walk->baseHeight;
                    node->shrinkNodes++;
                    node->shrinkWeight += walk->weight;
                    break;
                case LAYOUT_PREFERRED_HEIGHT:
                    node->containerMinimum += walk->minHeight;
                    node->containerPreferred += walk->baseHeight;
                    node->shrinkNodes++;
                    node->growNodes++;
                    node->growWeight += walk->weight;
                    node->shrinkWeight += walk->weight;
                    break;
                default:
                case LAYOUT_EXPANDING_HEIGHT:
                    node->containerMinimum += walk->minHeight;
                    node->containerPreferred += walk->baseHeight;
                    node->shrinkNodes++;
                    node->growNodes++;
                    node->growWeight += walk->weight;
                    node->shrinkWeight += walk->weight;
                    node->wantsGrowthNodes++;
                    wantsGrowWeight += walk->weight;
                    break;
                case LAYOUT_MINEXPANDING_HEIGHT:
                    node->containerMinimum += walk->minHeight;
                    node->containerPreferred += walk->baseHeight;
                    node->growNodes++;
                    node->growWeight += walk->weight;
                    node->wantsGrowthNodes++;
                    wantsGrowWeight += walk->weight;
                    break;
            }
        } else if(node->flags & LAYOUT_CONTAINER_HORIZONTAL) {
            switch(walk->flags & LAYOUT_WIDTH_MASK) {
                case LAYOUT_FIXED_WIDTH:
                    node->containerMinimum += walk->baseWidth;
                    node->containerPreferred += walk->baseWidth;
                    break;
                case LAYOUT_MINIMUM_WIDTH:
                    node->containerMinimum += walk->minWidth;
                    node->containerPreferred += walk->baseWidth;
                    node->growNodes++;
                    node->growWeight += walk->weight;
                    break;
                case LAYOUT_MAXIMUM_WIDTH:
                    node->containerMinimum += walk->minWidth;
                    node->containerPreferred += walk->baseWidth;
                    node->shrinkNodes++;
                    node->shrinkWeight += walk->weight;
                    break;
                case LAYOUT_PREFERRED_WIDTH:
                    node->containerMinimum += walk->minWidth;
                    node->containerPreferred += walk->baseWidth;
                    node->shrinkNodes++;
                    node->growNodes++;
                    node->growWeight += walk->weight;
                    node->shrinkWeight += walk->weight;
                    break;
                default:
                case LAYOUT_EXPANDING_WIDTH:
                    node->containerMinimum += walk->minWidth;
                    node->containerPreferred += walk->baseWidth;
                    node->shrinkNodes++;
                    node->growNodes++;
                    node->growWeight += walk->weight;
                    node->shrinkWeight += walk->weight;
                    node->wantsGrowthNodes++;
                    wantsGrowWeight += walk->weight;
                    break;
                case LAYOUT_MINEXPANDING_WIDTH:
                    node->containerMinimum += walk->minWidth;
                    node->containerPreferred += walk->baseWidth;
                    node->growNodes++;
                    node->growWeight += walk->weight;
                    node->wantsGrowthNodes++;
                    wantsGrowWeight += walk->weight;
                    break;
            }
        }
    }
    if(node->wantsGrowthNodes > 0) {
        node->growWeight = wantsGrowWeight;
    }
}

void layoutNodeChildAdd(layoutNode_t *node, layoutNode_t *child) {
    layoutNode_t *walk;

    if(node->child == NULL) {
        node->child = child;
    } else {
        for(walk = node->child; walk->next != NULL; walk = walk->next);
        walk->next = child;
    }
    child->parent = node;
    for(walk = child->next; walk != NULL; walk = walk->next) {
        walk->parent = node;
    }

    _layoutNodeRecalcContainer(node);
}

void layoutNodeNextAdd(layoutNode_t *node, layoutNode_t *next) {
    layoutNode_t *walk, *last;

    if(node->next == NULL) {
        node->next = NULL;
    } else {
        for(walk = node->next; walk->next != NULL; walk = walk->next);
        walk->next = next;
    }
    next->parent = node->parent;

    if(node->parent != NULL) {
        _layoutNodeRecalcContainer(node->parent);
    }
}

#define LAYOUT_PARAM_GT(var,node,field)  if(var > (node->field)) { var = node->field; }
#define LAYOUT_PARAM_LT(var,node,field)  if(var < (node->field)) { var = node->field; }

typedef enum {
    eLayoutResizeMinimum = 0,
    eLayoutResizeShrink,
    eLayoutResizePreferred,
    eLayoutResizeGrow
} eLayoutResizeMode_t;

void layoutNodeResize(layoutNode_t *node, int width, int height) {
    layoutNode_t *walk;
    int spread = 0;
    int adjustingNodes = 0;
    int adjustingWeight = 0;
    int value;
    int x;
    int y;

    node->width = width;
    if(node->width < node->minWidth) {
        node->width = node->minWidth;
    } else if(node->width > node->maxWidth) {
        node->width = node->maxWidth;
    }
    node->height = height;
    if(node->height < node->minHeight) {
        node->height = node->minHeight;
    } else if(node->height > node->maxHeight) {
        node->height = node->maxHeight;
    }

    if((node->callbacks != NULL) && (node->callbacks->resize != NULL)) {
        node->callbacks->resize(node, width, height);
    }

    // If this is not a container node, we're done
    if(node->child == NULL) {
        return;
    }

    // Determine child sizing
    width -= (node->padding[LAYOUT_LEFT] + node->padding[LAYOUT_RIGHT]);
    height -= (node->padding[LAYOUT_TOP] + node->padding[LAYOUT_BOTTOM]);

    if((width <= 0) || (height <= 0)) {
        return;
    }

    eLayoutResizeMode_t mode = eLayoutResizePreferred;

    if(node->containerMinimum < width) {
        mode = eLayoutResizeMinimum;
    } else if(node->containerPreferred > width) {
        mode = eLayoutResizeShrink;
        spread = node->containerPreferred - width;
        adjustingNodes = node->shrinkNodes;
        adjustingWeight = node->shrinkWeight;
    } else if(node->containerPreferred < width) {
        mode = eLayoutResizeGrow;
        spread = width - node->containerPreferred;
        adjustingNodes = ((node->wantsGrowthNodes > 0) ? node->wantsGrowthNodes : node->growNodes);
        adjustingWeight = node->growWeight;
    }

    x = node->x + node->padding[LAYOUT_LEFT];
    y = node->y + node->padding[LAYOUT_TOP];

    for(walk = node->child; walk != NULL; walk = walk->next) {
        if(node->flags & LAYOUT_CONTAINER_VERTICAL) {
            switch(mode) {
                case eLayoutResizeMinimum:
                    value = walk->minHeight;
                    break;
                case eLayoutResizeShrink:
                default:
                    // Does this node shrink?
                    if(walk->flags & (LAYOUT_MAXIMUM_HEIGHT | LAYOUT_PREFERRED_HEIGHT | LAYOUT_EXPANDING_HEIGHT)) {
                        if(adjustingNodes <= 1) {
                            value = walk->baseHeight - spread;
                        } else {
                            value = walk->baseHeight - ((int)((((float)spread) / ((float)adjustingNodes)) * ((float)adjustingWeight)));
                            adjustingNodes--;
                            adjustingWeight -= walk->weight;
                        }
                        LAYOUT_PARAM_LT(value, walk, minHeight);
                    } else {
                        value = node->baseHeight;
                    }
                    break;
                case eLayoutResizePreferred:
                    value = walk->baseHeight;
                    break;
                case eLayoutResizeGrow:
                    // Does this node want to grow?
                    if(((node->wantsGrowthNodes > 0) &&
                        (node->flags & (LAYOUT_EXPANDING_HEIGHT | LAYOUT_MINEXPANDING_HEIGHT))) ||
                       (node->flags & (LAYOUT_MINIMUM_HEIGHT | LAYOUT_PREFERRED_HEIGHT | LAYOUT_EXPANDING_HEIGHT | LAYOUT_MINEXPANDING_HEIGHT))) {
                        if (adjustingNodes <= 1) {
                            value = walk->baseHeight + spread;
                        } else {
                            value = walk->baseHeight +
                                    ((int) ((((float) spread) /
                                             ((float) adjustingNodes)) *
                                            ((float) adjustingWeight)));
                            adjustingNodes--;
                            adjustingWeight -= walk->weight;
                        }
                        LAYOUT_PARAM_GT(value, walk, maxHeight);
                    } else {
                        value = node->baseHeight;
                    }
                    break;
            }
            walk->x = x;
            walk->y = y;
            layoutNodeResize(walk, width, value);
            y += value;
        } else if(node->flags & LAYOUT_CONTAINER_HORIZONTAL) {
            switch(mode) {
                case eLayoutResizeMinimum:
                    value = walk->minWidth;
                    break;
                case eLayoutResizeShrink:
                default:
                    // Does this node shrink?
                    if(walk->flags & (LAYOUT_MAXIMUM_WIDTH | LAYOUT_PREFERRED_WIDTH | LAYOUT_EXPANDING_WIDTH)) {
                        if(adjustingNodes <= 1) {
                            value = walk->baseWidth - spread;
                        } else {
                            value = walk->baseWidth - ((int)((((float)spread) / ((float)adjustingNodes)) * ((float)adjustingWeight)));
                            adjustingNodes--;
                            adjustingWeight -= walk->weight;
                        }
                        LAYOUT_PARAM_LT(value, walk, minWidth);
                    } else {
                        value = node->baseWidth;
                    }
                    break;
                case eLayoutResizePreferred:
                    value = walk->baseWidth;
                    break;
                case eLayoutResizeGrow:
                    // Does this node want to grow?
                    if(((node->wantsGrowthNodes > 0) &&
                        (node->flags & (LAYOUT_EXPANDING_WIDTH |
                                        LAYOUT_MINEXPANDING_WIDTH))) ||
                       (node->flags & (LAYOUT_MINIMUM_WIDTH | LAYOUT_PREFERRED_WIDTH | LAYOUT_EXPANDING_WIDTH | LAYOUT_MINEXPANDING_WIDTH))) {
                        if (adjustingNodes <= 1) {
                            value = walk->baseWidth + spread;
                        } else {
                            value = walk->baseWidth +
                                    ((int) ((((float) spread) /
                                             ((float) adjustingNodes)) *
                                            ((float) adjustingWeight)));
                            adjustingNodes--;
                            adjustingWeight -= walk->weight;
                        }
                        LAYOUT_PARAM_GT(value, walk, maxWidth);
                    } else {
                        value = node->baseWidth;
                    }
                    break;
            }
            walk->x = x;
            walk->y = y;
            layoutNodeResize(walk, value, height);
            x += value;
        }
    }
}

void layoutNodePosition(layoutNode_t *node, int x, int y) {
    layoutNode_t *walk;
    int posx, posy;

    node->x = x;
    node->y = y;
    posx = node->x + node->padding[LAYOUT_LEFT];
    posy = node->y + node->padding[LAYOUT_TOP];
    if(node->flags & LAYOUT_CONTAINER_HORIZONTAL) {
        for(walk = node->child; walk != NULL; walk = walk->next) {
            layoutNodePosition(walk, posx, posy);
            posx += walk->width;
        }
    } else if(node->flags & LAYOUT_CONTAINER_VERTICAL) {
        for(walk = node->child; walk != NULL; walk = walk->next) {
            layoutNodePosition(walk, posx, posy);
            posy += walk->height;
        }
    }
}

int layoutNodeUpdate(layoutNode_t *node, Uint32 ticks) {
    layoutNode_t *walk;
    int result = 0;

    if((node->callbacks != NULL) && (node->callbacks->update != NULL)) {
        result |= node->callbacks->update(node, ticks);
    }
    if(node->child != NULL) {
        result |= layoutNodeUpdate(node->child, ticks);
    }
    if(node->next != NULL) {
        result |= layoutNodeUpdate(node->next, ticks);
    }
    return result;
}

int layoutNodeWalk(layoutNode_t *node, void *context) {
    layoutNode_t *walk;

    if((node->callbacks != NULL) && (node->callbacks->walk != NULL)) {
        if(!node->callbacks->walk(node, context)) {
            return 0;
        }
    }
    if(node->child != NULL) {
        if(!layoutNodeWalk(node->child, context)) {
            return 0;
        }
    }
    if(node->next != NULL) {
        if(!layoutNodeWalk(node->next, context)) {
            return 0;
        }
    }
    return 1;
}

layoutNode_t *layoutNodeFindById(layoutNode_t *node, int id) {
    layoutNode_t *result;

    if(node->id == id) {
        return node;
    }
    if(node->child != NULL) {
        if((result = layoutNodeFindById(node->child, id)) != NULL) {
            return result;
        }
    }
    if(node->next != NULL) {
        if((result = layoutNodeFindById(node->next, id)) != NULL) {
            return result;
        }
    }
    return NULL;
}
