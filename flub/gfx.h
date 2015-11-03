#ifndef _FLUB_GFX_HEADER_
#define _FLUB_GFX_HEADER_

/*
 * Shaders:
 *     o fullbright - render the buffer as is
 *     o global - render the buffer using a single color/alpha for all vertices
 *     o color - render the buffer using per vertex colo/alpha
 */

#include <GL/gl.h>
#include <flub/texture.h>
#include <flub/font_struct.h>


#define GFX_X_COORDS	0
#define GFX_Y_COORDS	1

#define GFX_X_LOCKED	1
#define GFX_Y_LOCKED	2

// http://stackoverflow.com/questions/5879403/opengl-texture-coordinates-in-pixel-space
#define SCALED_T_COORD(t,v)	(((2.0f * ((float)v)) + 1.0f)/(2.0f * ((float)t)))


// Gfx module init/shutdown

int gfxInit(void);
int gfxValid(void);
int gfxStart(void);
int gfxShutdown(void);

// Simple sprite support

typedef struct flubSprite_s {
	const texture_t *texture;
	int spritesPerRow;
	int width;
	int height;
} flubSprite_t;


flubSprite_t *gfxSpriteCreate(const texture_t *texture, int width, int height);
void gfxSpriteDestroy(flubSprite_t *sprite);

// Slice image support

typedef struct flubSlice_s {
	const texture_t *texture;
	int quads;
	int locked;
	int xPre;
	int xPost;
	int yPre;
	int yPost;
	float tcoords[2][4];
	int width;
	int height;
} flubSlice_t;

flubSlice_t *gfxSliceCreate(const texture_t *texture,
							int x1, int y1, int x2, int y2, int x3,
							int y3, int x4, int y4);
void gfxSliceDestroy(flubSlice_t *slice);

flubSlice_t *gfx3x3SliceCreate(const texture_t *texture,
                               int x1, int y1, int x2, int y2, int x3,
                               int y3, int x4, int y4);
flubSlice_t *gfx1x3SliceCreate(const texture_t *texture,
                               int x1, int y1, int x2, int y2, int y3, int y4);
flubSlice_t *gfx3x1SliceCreate(const texture_t *texture,
                               int x1, int y1, int x2, int y2, int x3, int x4);
void gfxSliceRoundUpSizeToInterval(flubSlice_t *slice, int *width, int *height);


// Keycap support

void gfxKeyMetrics(font_t *font, const char *caption, int *width, int *height, int *textYOffset);

// Clipping

void gfx2dClipRegionSet(int x1, int y1, int x2, int y2);
void gfx2dClipRegionClear(void);


#define GFX_MESH_FLAG_COLOR		0x1

#define GFX_MESH_VBO_VERTEX_ID	0
#define GFX_MESH_VBO_TEXTURE_ID	1
#define GFX_MESH_VBO_COLOR_ID	2


typedef void (*gfxMeshInitCB_t)(void *context);

// 3 vertices for each triangle
#define MESH_TRIANGLE_SIZE(x)	(x * 3)

// 2 triangles per quad
#define MESH_QUAD_SIZE(x)		MESH_TRIANGLE_SIZE(2 * x)

// Simple check if the quad has room for more entries
#define MESH_GROWTH_CHECK(m,s)	(((m)->pos + (s)) >= ((m)->length))


typedef struct VBOTexCoord_s {
	float s;
	float t;
} VBOTexCoord_t;

typedef struct VBOVertexPos2D_s {
	GLint x;
	GLint y;
} VBOVertexPos2D_t;

typedef struct VBOColor_s {
	float r;
	float g;
	float b;
	float a;
} VBOColor_t;

typedef struct gfxMeshObj_s {
	gfxMeshInitCB_t initCB;
	void *context;

    GLuint program;

	int flags;
	int length;
	int pos;
	int dirty;
	const texture_t *texture;
	float red;
	float green;
	float blue;
	float alpha;

	GLuint vboId[3];

	VBOVertexPos2D_t *vboVertices;
	VBOTexCoord_t *vboTexCoords;
	VBOColor_t *vboColors;

	struct gfxMeshObj_s *prev;
	struct gfxMeshObj_s *next;

	struct gfxMeshObj_s *_gfx_mgr_prev;
	struct gfxMeshObj_s *_gfx_mgr_next;
} gfxMeshObj_t;

gfxMeshObj_t *gfxMeshCreate(int triCount, int flags, const texture_t *texture);
void gfxMeshDestroy(gfxMeshObj_t *mesh);
void gfxMeshInitCBSet(gfxMeshObj_t *mesh, gfxMeshInitCB_t initCB, void *context);
void gfxMeshClear(gfxMeshObj_t *mesh);
void gfxMeshTextureAssign(gfxMeshObj_t *mesh, const texture_t *texture);
void gfxMeshDefaultColor(gfxMeshObj_t *mesh, float red, float green,
						 float blue, float alpha);

void gfxMeshPrependToChain(gfxMeshObj_t **chain, gfxMeshObj_t *mesh);
void gfxMeshAppendToChain(gfxMeshObj_t *chain, gfxMeshObj_t *mesh);
void gfxMeshRemoveFromChain(gfxMeshObj_t *chain, gfxMeshObj_t *mesh);

gfxMeshObj_t *gfxMeshFindMeshInChain(gfxMeshObj_t *mesh, const texture_t *texture);

void gfxMeshRender(gfxMeshObj_t *mesh);

void gfxMeshQuad(gfxMeshObj_t *mesh, int x1, int y1, int x2, int y2,
			     float s1, float t1, float s2, float t2);
void gfxMeshQuadAtPos(gfxMeshObj_t *mesh, int pos, int *last,
					  int x1, int y1, int x2, int y2,
					  float s1, float t1, float s2, float t2);
void gfxMeshQuadColor(gfxMeshObj_t *mesh, int x1, int y1, int x2, int y2,
                      float s1, float t1, float s2, float t2,
			          float red, float green, float blue, float alpha);
void gfxMeshQuadColorAtPos(gfxMeshObj_t *mesh, int pos, int *last,
						   int x1, int y1, int x2, int y2,
						   float s1, float t1, float s2, float t2,
						   float red, float green, float blue, float alpha);

void gfxTexBlit(const texture_t *texture, int x1, int y1);
void gfxTexBlitSub(const texture_t *texture, int tx1, int ty1, int tx2, int ty2,
				   int x1, int y1, int x2, int y2);
void gfxTexTile(const texture_t *texture, int tx1, int ty1, int tx2, int ty2,
			    int x1, int y1, int x2, int y2);
void gfxSpriteBlit(const flubSprite_t *sprite, int num, int x, int y);
void gfxSpriteBlitResize(const flubSprite_t *sprite, int num, int x1, int y1,
						 int x2, int y2);
void gfxSliceBlit(const flubSlice_t *slice, int x1, int y1, int x2, int y2);
void gfxKeycapBlit(font_t *font, const char *caption, int x, int y,
				   int *width, int *height);
void gfxBlitKeyStr(font_t *font, const char *str, int x, int y,
				   int *width, int *height);

int gfxTexMeshBlit(gfxMeshObj_t *mesh, const texture_t *texture, int x1, int y1);
int gfxTexMeshBlitSub(gfxMeshObj_t *mesh, const texture_t *texture,
	 				  int tx1, int ty1, int tx2, int ty2,
	 				  int x1, int y1, int x2, int y2);
int gfxTexMeshTile(gfxMeshObj_t *mesh, const texture_t *texture,
				   int tx1, int ty1, int tx2, int ty2,
				   int x1, int y1, int x2, int y2);
int gfxSpriteMeshBlit(gfxMeshObj_t *mesh, const flubSprite_t *sprite, int num,
					  int x, int y);
int gfxSpriteMeshBlitResize(gfxMeshObj_t *mesh, const flubSprite_t *sprite,
							int num, int x1, int y1, int x2, int y2);
int gfxSliceMeshBlit(gfxMeshObj_t *mesh, const flubSlice_t *slice,
					 int x1, int y1, int x2, int y2);
int gfxKeycapMeshBlit(gfxMeshObj_t *mesh, font_t *font, const char *caption,
				      int x, int y, int *width, int *height);

void gfxTexMeshBlitAtPos(gfxMeshObj_t *mesh, int pos, int *last,
						const texture_t *texture, int x1, int y1);
void gfxTexMeshBlitSubAtPos(gfxMeshObj_t *mesh, int pos, int *last,
						   const texture_t *texture, int tx1, int ty1,
						   int tx2, int ty2, int x1, int y1, int x2, int y2);
void gfxTexMeshTileAtPos(gfxMeshObj_t *mesh, int pos, int *last,
						const texture_t *texture, int tx1, int ty1,
						int tx2, int ty2, int x1, int y1, int x2, int y2);
void gfxSpriteMeshBlitAtPos(gfxMeshObj_t *mesh, int pos, int *last,
						   const flubSprite_t *sprite, int num, int x, int y);
void gfxSpriteMeshBlitResizeAtPos(gfxMeshObj_t *mesh, int pos, int *last,
								 const flubSprite_t *sprite, int num,
								 int x1, int y1, int x2, int y2);
void gfxSliceMeshBlitAtPos(gfxMeshObj_t *mesh, int pos, int *last,
						  const flubSlice_t *slice, int x1, int y1, int x2, int y2);


void gfxMeshRangeAlphaSet(gfxMeshObj_t *mesh, int start, int end, float alpha);

typedef struct gfxEffect_s gfxEffect_t;

typedef void (*gfxEffectCB_t)(gfxEffect_t *effect, void *context);
typedef void (*gfxEffectCleanupCB_t)(void *context);

struct gfxEffect_s {
	gfxMeshObj_t *mesh;
	Uint32 tickStart;
	Uint32 tickDuration;
	Uint32 tickCurrent;
	int indexStart;
	int indexEnd;
	int autoCleanup;

	void *context;

	gfxEffectCleanupCB_t cleanup;

	gfxEffectCB_t handler;
	gfxEffectCB_t start;
	gfxEffectCB_t completed;

	gfxEffect_t *next;
};


gfxEffect_t *gfxEffectMotionLinear(gfxMeshObj_t *mesh, int startPos,
								   int endPos, int xMove, int yMove, int duration);
gfxEffect_t *gfxEffectScale(gfxMeshObj_t *mesh, int startPos, int endPos,
							float xStart, float xEnd, float yStart, float yEnd,
							int xOffset, int yOffset, int duration);
gfxEffect_t *gfxEffectFade(gfxMeshObj_t *mesh, int startPos, int endPos,
						   float alphaStart, float alphaEnd, int duration);
gfxEffect_t *gfxEffectShade(gfxMeshObj_t *mesh, int startPos, int endPos,
							float red, float green, float blue, float alpha,
							int duration);
gfxEffect_t *gfxEffectPulse(gfxMeshObj_t *mesh, int startPos, int endPos,
							float initRed, float initGreen, float initBlue,
							float initAlpha, int midDuration,
							float midRed, float midGreen, float midBlue,
							float midAlpha, int duration,
							float endRed, float endGreen, float endBlue,
							float endAlpha, int loop);

gfxEffect_t *gfxEffectCreate(gfxMeshObj_t *mesh, gfxEffectCB_t handler,
							 int indexBegin, int indexEnd,
							 gfxEffectCB_t start, gfxEffectCB_t end,
							 gfxEffectCleanupCB_t cleanup, int autoCleanup,
							 void *context);
void gfxEffectDestroy(gfxEffect_t *effect);

void gfxEffectRegister(gfxEffect_t *effect);
void gfxEffectUnregister(gfxEffect_t *effect);

void gfxUpdate(Uint32 ticks);


typedef struct flubColor_s {
    float r;
    float g;
    float b;
    float a;
} flubColor_t;

typedef struct flubQuad2D_s {
    int x;
    int y;
    flubColor_t color;
} flubQuad2D_t;

typedef struct flubQuadAnim_s flubQuadAnim_t;

typedef struct flubQuadAnim_s {
    flubQuad2D_t *dest;
    flubQuad2D_t start;
    flubQuad2D_t finish;
    int duration;
    int currentTicks;
} flubQuadAnim_t;


#endif // _FLUB_GFX_HEADER_
