#ifndef _FLUB_LAYOUT_HEADER_
#define _FLUB_LAYOUT_HEADER_


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
#define LAYOUT_GRID_ROW             0x4000
#define LAYOUT_GRID_COLUMN          0x8000

#define LAYOUT_WIDTH_MASK           (LAYOUT_FIXED_WIDTH | LAYOUT_MINIMUM_WIDTH | LAYOUT_MAXIMUM_WIDTH | LAYOUT_PREFERRED_WIDTH | LAYOUT_EXPANDING_WIDTH | LAYOUT_MINEXPANDING_WIDTH)
#define LAYOUT_HEIGHT_MASK          (LAYOUT_FIXED_HEIGHT | LAYOUT_MINIMUM_HEIGHT | LAYOUT_MAXIMUM_HEIGHT | LAYOUT_PREFERRED_HEIGHT | LAYOUT_EXPANDING_HEIGHT | LAYOUT_MINEXPANDING_HEIGHT)


typedef struct layoutNode_s layoutNode_t;

typedef void (*layoutCallbackResize_t)(layoutNode_t *node, int x, int y, int width, int height, void *context);
typedef void (*layoutCallbackCleanup_t)(layoutNode_t *node, void *context);

struct layoutNode_s {
    int x;
    int y;
    int width;
    int height;

    int minWidth;
    int maxWidth;
    int minHeight;
    int maxHeight;
    int baseWidth;
    int baseHeight;

    int marginWidth;
    int marginHeight;

    int containerMinimum;
    int containerPreferred;
    int shrinkNodes;
    int growNodes;
    int wantsGrowthNodes;
    int growWeight;
    int shrinkWeight;

    int weight;

    unsigned int flags;

    void *data;

    layoutNode_t *parent;
    layoutNode_t *children;
    layoutNode_t *next;

    layoutCallbackResize_t resize;
    layoutCallbackCleanup_t cleanup;
};


layoutNode_t *layoutNodeCreate(int width, int height, unsigned int flags,
                               int minWidth, int maxWidth,
                               int minHeight, int maxHeight,
                               int weight,
                               void *data,
                               layoutCallbackResize_t resize,
                               layoutCallbackCleanup_t cleanup);
int layoutAddChildToNode(layoutNode_t *parent, layoutNode_t *child);
void layoutOrphanNode(layoutNode_t *node);
void layoutNodeDestroy(layoutNode_t *node, void *context);
void layoutNodeResize(layoutNode_t *node, int width, int height, void *context);


#endif // _FLUB_LAYOUT_HEADER_
