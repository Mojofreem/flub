#ifndef _FLUB_LAYOUT_HEADER_
#define _FLUB_LAYOUT_HEADER_


#include <SDL2/SDL.h>


typedef struct layoutNode_s layoutNode_t;

typedef void (*layoutResizeCallback_t)(layoutNode_t *node, int width, int height);
typedef void (*layoutDestroyCallback_t)(layoutNode_t *node, void *context);
typedef int (*layoutUpdateCallback_t)(layoutNode_t *node, Uint32 ticks);
typedef int (*layoutWalkCallback_t)(layoutNode_t *node, void *context);

typedef struct layoutCallbacks_s {
    layoutResizeCallback_t resize;
    layoutDestroyCallback_t destroy;
    layoutUpdateCallback_t update;
    layoutWalkCallback_t walk;
} layoutCallbacks_t;


/*
            CanGrow   CanShrink   WantsToGrow
Fixed         No         No           No
Minimum       Yes        No           No
Maximum       No         Yes          No
Preferred     Yes        Yes          No
Expanding     Yes        Yes          Yes
MinExpanding  Yes        No           Yes
Ignored       Yes        Yes          Yes

*/

#define LAYOUT_FIXED_WIDTH          0x0001
#define LAYOUT_MINIMUM_WIDTH        0x0002
#define LAYOUT_MAXIMUM_WIDTH        0x0004
#define LAYOUT_PREFERRED_WIDTH      0x0008
#define LAYOUT_EXPANDING_WIDTH      0x0010
#define LAYOUT_MINEXPANDING_WIDTH   0x0020
#define LAYOUT_FIXED_HEIGHT         0x0040
#define LAYOUT_MINIMUM_HEIGHT       0x0080
#define LAYOUT_MAXIMUM_HEIGHT       0x0100
#define LAYOUT_PREFERRED_HEIGHT     0x0200
#define LAYOUT_EXPANDING_HEIGHT     0x0400
#define LAYOUT_MINEXPANDING_HEIGHT  0x0800
#define LAYOUT_CONTAINER_HORIZONTAL 0x1000
#define LAYOUT_CONTAINER_VERTICAL   0x2000
#define LAYOUT_VISIBLE              0x4000
#define LAYOUT_HIDDEN               0x8000

// Note that visible occupies space, but is not actually rendered, whereas
// hidden does NOT occupy space and is also not rendered.

#define LAYOUT_WIDTH_MASK           (LAYOUT_FIXED_WIDTH | LAYOUT_MINIMUM_WIDTH | LAYOUT_MAXIMUM_WIDTH | LAYOUT_PREFERRED_WIDTH | LAYOUT_EXPANDING_WIDTH | LAYOUT_MINEXPANDING_WIDTH)
#define LAYOUT_HEIGHT_MASK          (LAYOUT_FIXED_HEIGHT | LAYOUT_MINIMUM_HEIGHT | LAYOUT_MAXIMUM_HEIGHT | LAYOUT_PREFERRED_HEIGHT | LAYOUT_EXPANDING_HEIGHT | LAYOUT_MINEXPANDING_HEIGHT)

#define LAYOUT_LEFT     0
#define LAYOUT_TOP      1
#define LAYOUT_RIGHT    2
#define LAYOUT_BOTTOM   3


struct layoutNode_s {
    int id;

    int x;
    int y;
    int width;
    int height;

    int baseWidth;
    int baseHeight;

    int minWidth;
    int maxWidth;
    int minHeight;
    int maxHeight;

    int weight;

    Sint8 padding[4];

    int containerMinimum;
    int containerPreferred;
    int shrinkNodes;
    int growNodes;
    int wantsGrowthNodes;
    int growWeight;
    int shrinkWeight;

    unsigned int flags;

    layoutNode_t *child;
    layoutNode_t *next;
    layoutNode_t *parent;

    layoutCallbacks_t *callbacks;

    void *context;
    void *state;
};


int layoutInit(void);
int layoutValid(void);
void layoutShutdown(void);

layoutNode_t *layoutNodeCreate(int id, int width, int height,
                               int minWidth, int maxWidth,
                               int minHeight, int maxHeight,
                               int weight,
                               Sint8 padLeft, Sint8 padTop, Sint8 padRight, Sint8 padBottom,
                               unsigned int flags, layoutCallbacks_t *callbacks,
                               void *context);
void layoutNodeDestroy(layoutNode_t *node, void *context);

void layoutNodeChildAdd(layoutNode_t *node, layoutNode_t *child);
void layoutNodeNextAdd(layoutNode_t *node, layoutNode_t *next);

void layoutNodeResize(layoutNode_t *node, int w, int h);
void layoutNodePosition(layoutNode_t *node, int x, int y);

int layoutNodeUpdate(layoutNode_t *node, Uint32 ticks);
int layoutNodeWalk(layoutNode_t *node, void *context);

layoutNode_t *layoutNodeFindById(layoutNode_t *node, int id);


#endif // _FLUB_LAYOUT_HEADER_

