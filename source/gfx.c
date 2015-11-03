#ifdef WIN32
#	define GLEW_STATIC
#	include <flub/3rdparty/glew/glew.h>
#endif

#include <SDL2/SDL.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <SDL2/SDL_opengl.h>

#include <stdio.h>
#include <flub/memory.h>
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
//#include <GL/glew.h>
#include <SDL2/SDL_opengl.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <flub/video.h>
#include <flub/gfx.h>
#include <flub/texture.h>
#include <flub/logger.h>
#include <flub/font.h>
#include <ctype.h>
#include <stdlib.h>


#define GFX_TRACE       1


struct gfxCtx_s {
    int init;
	const texture_t *flubMisc;
	flubSlice_t *keycapSlice;
	gfxEffect_t *activeEffects;
    gfxMeshObj_t *meshList;
    GLuint simpleProgram;
} gfxCtx = {
        .init = 0,
		.flubMisc = NULL,
		.keycapSlice = NULL,
		.activeEffects = NULL,
        .meshList = NULL,
        .simpleProgram = 0,
	};


typedef struct specialKeyMap_s {
	const char *name;
	int coords[4][4];
} specialKeyMap_t;

static int _gfxMaxSpecialKeys = 0;
#define GFX_MAX_SPECIAL_KEYS	7
static specialKeyMap_t _specialKeyTable[] = {
    {"META_APPLE", {{58, 0, 71, 13}, {0, 57, 17, 74}, {0, 75, 19, 94}, {0, 95, 23, 118}}},
    {"META_RIGHT", {{72, 0, 85, 13}, {18, 57, 35, 74}, {20, 75, 39, 94}, {48, 95, 71, 118}}},
    {"META_LEFT", {{86, 0, 99, 13}, {36, 57, 53, 74}, {40, 75, 59, 94}, {24, 95, 47, 118}}},
    {"META_UP", {{100, 0, 113, 13}, {54, 57, 71, 74}, {60, 75, 79, 94}, {72, 95, 95, 118}}},
    {"META_DOWN", {{114, 0, 127, 13}, {72, 57, 89, 74}, {80, 75, 99, 94}, {96, 95, 119, 118}}},
    {"META_WINDOWS", {{100, 14, 113, 27}, {64, 14, 81, 31}, {100, 75, 119, 94}, {65, 32, 88, 55}}},
    {"META_ALTMENU", {{114, 14, 127, 27}, {82, 14, 99, 31}, {90, 55, 109, 74}, {104, 28, 127, 51}}},
    {NULL, {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}}
};


static int _gfxIsSpecialKey(const char *key) {
	int k;

	for(k = 0; _specialKeyTable[k].name != NULL; k++) {
		if(!strcmp(_specialKeyTable[k].name, key)) {
			return k;
		}
	}
	return -1;
}

static void _gfxSpecialKeyCoords(int idx, int size, int *x1, int *y1, int *x2, int *y2) {
	if((idx < 0) | (idx > _gfxMaxSpecialKeys)) {
		if(x1 != NULL) {
			*x1 = 0;
		}
		if(y1 != NULL) {
			*y1 = 0;
		}
		if(x2 != NULL) {
			*x2 = 0;
		}
		if(y2 != NULL) {
			*x2 = 0;
		}
		return;
	}

	// Find the nearest font size match
	if(size < 16) {
		size = 0; // 12
	} else if(size < 20) {
		size = 1; // 16
	} else if(size < 24) {
		size = 2; // 20
	} else {
		size = 3; // 24
	}

	if(x1 != NULL) {
		*x1 = _specialKeyTable[idx].coords[size][0];
	}
	if(y1 != NULL) {
		*y1 = _specialKeyTable[idx].coords[size][1];
	}
	if(x2 != NULL) {
		*x2 = _specialKeyTable[idx].coords[size][2];
	}
	if(y2 != NULL) {
		*y2 = _specialKeyTable[idx].coords[size][3];
	}
}

static int _gfxCalcKeycapCaptionQuads(const char *caption) {
	int quads;

	if(_gfxIsSpecialKey(caption)) {
		return 1;
	}

	for(quads = 0; *caption != '\0'; caption++) {
		if(!isspace(*caption)) {
			quads++;
		}
	}
	return quads;
}

static void _gfxMeshReinit(gfxMeshObj_t *mesh);

static void _gfxVideoNotifieeCB(int width, int height, int fullscreen) {
    gfxMeshObj_t *mesh;
    int k;

    debug(DBG_GFX, GFX_TRACE, "Rebinding VBO's...");
    for(k = 0, mesh = gfxCtx.meshList; mesh != NULL; mesh = mesh->_gfx_mgr_next, k++) {
        debugf(DBG_GFX, GFX_TRACE, "* VBO %d", k);
        _gfxMeshReinit(mesh);
    }
}

int gfxInit(void) {
	int k;

    if(gfxCtx.init) {
        warning("Cannot initialize the gfx module multiple times.");
        return 1;
    }

    if(!logValid()) {
        // The logger has not been initialized!
        return 0;
    }

    if(!videoValid()) {
        fatal("Cannot initialize the gfx module: video is not initialized");
        return 0;
    }

    logDebugRegister("gfx", DBG_GFX, "trace", GFX_TRACE);
	for(k = 0; _specialKeyTable[k].name != NULL; k++);
	_gfxMaxSpecialKeys = k - 1;

	if(!videoAddNotifiee(_gfxVideoNotifieeCB)) {
		return 0;
	}

    gfxCtx.init = 1;

	return 1;
}

void _gfxShutdown(void) {
    if(!gfxCtx.init) {
        return;
    }

    // TODO Free up common gfx resources

    videoRemoveNotifiee(_gfxVideoNotifieeCB);

    gfxCtx.init = 1;
}

int gfxStart(void) {
	gfxCtx.flubMisc = texmgrGet("flub-keycap-misc");
	gfxCtx.keycapSlice = gfxSliceCreate(gfxCtx.flubMisc, 41, 0, 46, 5, 52, 11, 57, 16);
    gfxCtx.simpleProgram = videoGetProgram("simple");
    infof("Program id is %d", gfxCtx.simpleProgram);


    return 1;
}

#define KEYCAP_HORZ_SPACER 5
#define KEYCAP_VERT_SPACER 5

void gfxDrawKey(font_t *font, const char *caption, int x, int y, int *width, int *height) {
	int keyheight;
	int keywidth;
	int key = -1;
	int size;
	int idx;
	int kx1, ky1, kx2, ky2;

	keyheight = fontGetHeight(font) + KEYCAP_VERT_SPACER + KEYCAP_VERT_SPACER;

	if((idx = _gfxIsSpecialKey(caption)) >= 0) {
		_gfxSpecialKeyCoords(idx, fontGetHeight(font), &kx1, &ky1, &kx2, &ky2);
		keywidth = kx2 - kx1 + KEYCAP_HORZ_SPACER + KEYCAP_HORZ_SPACER;
	} else {
		keywidth = fontGetStrWidth(font, caption) + KEYCAP_HORZ_SPACER + KEYCAP_HORZ_SPACER;
	}

    fontSetColor(1.0, 1.0, 1.0);
	gfxSliceBlit(gfxCtx.keycapSlice, x, y,
				 x + keywidth,
				 y + keyheight);
	
	fontMode();
	fontSetColor(0.0, 0.0, 0.0);
	if(idx >= 0) {
		gfxTexBlitSub(gfxCtx.flubMisc,
					  kx1, ky1, kx2, ky2,
					  x + KEYCAP_HORZ_SPACER, y + KEYCAP_VERT_SPACER,
					  (x + KEYCAP_HORZ_SPACER) + (kx2 - kx1),
					  (y + KEYCAP_VERT_SPACER) + (ky2 - ky1));
	} else {
		fontPos(x + KEYCAP_HORZ_SPACER, y + KEYCAP_VERT_SPACER + 2);
		fontBlitStr(font, caption);
	}

	if(width != NULL) {
		*width = keywidth;
	}
	if(height != NULL) {
		*height = keyheight;
	}
}

void gfxKeyMetrics(font_t *font, const char *caption, int *width, int *height, int *textYOffset) {
    int keyheight;
    int keywidth;
    int key = -1;
    int size;
    int idx;
    int kx1, ky1, kx2, ky2;

    keyheight = fontGetHeight(font) + KEYCAP_VERT_SPACER + KEYCAP_VERT_SPACER;

    if((caption != NULL) && (width != NULL)) {
        if((idx = _gfxIsSpecialKey(caption)) >= 0) {
            _gfxSpecialKeyCoords(idx, fontGetHeight(font), &kx1, &ky1, &kx2, &ky2);
            keywidth = kx2 - kx1 + KEYCAP_HORZ_SPACER + KEYCAP_HORZ_SPACER;
        } else {
            keywidth = fontGetStrWidth(font, caption) + KEYCAP_HORZ_SPACER + KEYCAP_HORZ_SPACER;
        }
    }

    if(width != NULL) {
        *width = keywidth;
    }
    if(height != NULL) {
        *height = keyheight;
    }
    if(textYOffset != NULL) {
        *textYOffset = (keyheight / 2) - (fontGetHeight(font) / 2);
    }
}

flubSprite_t *gfxSpriteCreate(const texture_t *texture, int width, int height) {
	flubSprite_t *sprite;

	if(texture == NULL) {
		return NULL;
	}

	if((texture->width / width) == 0) {
		return NULL;
	}

	if(texture->height < height) {
		return NULL;
	}

	sprite = util_calloc(sizeof(flubSprite_t), 0, NULL);
	sprite->texture = texture;
	sprite->width = width;
	sprite->height = height;
	sprite->spritesPerRow = texture->width / width;

	return sprite;
}

void gfxSpriteDestroy(flubSprite_t *sprite) {
	util_free(sprite);
}

void _sortFourValues(int *a, int *b, int *c, int *d) {
	static const int max_idx = 4;
	int *ptrs[4] = {a, b, c, d};
	int swap;
	int j, k;

	for(j = 0; j < (max_idx - 1); j++) {
		for(k = j + 1; k < max_idx; k++) {
			if(*(ptrs[k]) < *(ptrs[j])) {
				swap = *(ptrs[k]);
				*(ptrs[k]) = *(ptrs[j]);
				*(ptrs[j]) = swap;
			}
		}
	}
}

/*

 +---+---+---+
 | A | B | C |
 +---+---+---+
 | D | E | F |
 +---+---+---+
 | G | H | I |
 +---+---+---+

 Configurations:
    9 slice: A B C D E F G H I
    6 slice: A B C D E F  or  A B D E G H
    3 slice: A B C  or  A D G

 Variable modes:
    stretch
    tile


 Each row and column can be fixed, stretched, tiled, or empty

    | F | S | T | E |
 ---+---+---+---+---+
  F | F | S | T | E |
 ---+---+---+---+---+
  S | S | S | T | E |
 ---+---+---+---+---+
  T | T | T | T | E |
 ---+---+---+---+---+
  E | E | E | E | E |
 ---+---+---+---+---+





 */
flubSlice_t *gfxSliceCreate(const texture_t *texture,
                            int x1, int y1, int x2, int y2, int x3,
                            int y3, int x4, int y4) {
    flubSlice_t *slice;

    slice = util_calloc(sizeof(flubSlice_t), 0, NULL);

    _sortFourValues(&x1, &x2, &x3, &x4);
    _sortFourValues(&y1, &y2, &y3, &y4);

    slice->texture = texture;
    slice->xPre = x2 - x1 + 1;
    slice->xPost = x4 - x3 + 1;
    slice->yPre = y2 - y1 + 1;
    slice->yPost = y4 - y3 + 1;

    slice->width = slice->xPre;
    slice->height = slice->yPre;

    if((x2 == x3) && (x3 == x4)) {
        slice->locked |= GFX_X_LOCKED;
    } else {
        slice->width = slice->xPre + slice->xPost;
    }
    if((y2 == y3) && (y3 == y4)) {
        slice->locked |= GFX_Y_LOCKED;
    } else {
        slice->height = slice->yPre + slice->yPost;
    }

    slice->tcoords[GFX_X_COORDS][0] = SCALED_T_COORD(texture->width, x1);
    slice->tcoords[GFX_Y_COORDS][0] = SCALED_T_COORD(texture->height, y1);
    slice->tcoords[GFX_X_COORDS][1] = SCALED_T_COORD(texture->width, x2);
    slice->tcoords[GFX_Y_COORDS][1] = SCALED_T_COORD(texture->height, y2);
    slice->tcoords[GFX_X_COORDS][2] = SCALED_T_COORD(texture->width, x3);
    slice->tcoords[GFX_Y_COORDS][2] = SCALED_T_COORD(texture->height, y3);
    slice->tcoords[GFX_X_COORDS][3] = SCALED_T_COORD(texture->width, x4);
    slice->tcoords[GFX_Y_COORDS][3] = SCALED_T_COORD(texture->height, y4);

    slice->quads = 1;
    if(x2 != x3) {
        slice->quads++;
    }
    if(x3 != x4) {
        slice->quads++;
    }
    if(y2 != y3) {
        if(y3 != y4) {
            slice->quads *= 3;
        } else {
            slice->quads *= 2;
        }
    } else if(y3 != y4) {
        slice->quads *= 2;
    }

    return slice;
}

void gfxSliceDestroy(flubSlice_t *slice) {
	util_free(slice);
}

void gfx2dClipRegionSet(int x1, int y1, int x2, int y2) {
	// TODO Validate the scissor clip logic

	// This is my expected call, defined with traditional cartesian coords.
	glScissor(x1, y1, x2 - x1, y2 - y1);

	// This is what documentation seems to indicate (origin at lower left):
	//glScissor(x1, (*videoHeight) - y1, x2 - x1, y2 - y1);

	glEnable(GL_SCISSOR_TEST);
}

void gfx2dClipRegionClear(void) {
	glDisable(GL_SCISSOR_TEST);
}

///////////////////////////////////////////////////////////////////////////////
// VBO Mesh Functions
///////////////////////////////////////////////////////////////////////////////

static void _gfxMeshReinit(gfxMeshObj_t *mesh) {
    if(mesh->flags & GFX_MESH_FLAG_COLOR) {
        glGenBuffers(3, mesh->vboId);
    } else {
        glGenBuffers(2, mesh->vboId);
    }
    mesh->dirty = 1;
}

gfxMeshObj_t *gfxMeshCreate(int triCount, int flags, const texture_t *texture) {
	gfxMeshObj_t *mesh;

	mesh = util_alloc(sizeof(gfxMeshObj_t), NULL);
	memset(mesh, 0, sizeof(gfxMeshObj_t));
	mesh->flags = flags;
	mesh->length = MESH_TRIANGLE_SIZE(triCount);
	mesh->dirty = 1;
	mesh->texture = texture;
	mesh->red = 1.0;
	mesh->green = 1.0;
	mesh->blue = 1.0;
	mesh->alpha = 1.0;
    mesh->program = gfxCtx.simpleProgram;

	if(flags & GFX_MESH_FLAG_COLOR) {
		glGenBuffers(3, mesh->vboId);
		mesh->vboColors = util_alloc((mesh->length * sizeof(VBOColor_t)), NULL);
	} else {
		glGenBuffers(2, mesh->vboId);
	}

	mesh->vboVertices = util_alloc((mesh->length * sizeof(VBOVertexPos2D_t)), NULL);
	mesh->vboTexCoords = util_alloc((mesh->length * sizeof(VBOTexCoord_t)), NULL);

    if(gfxCtx.meshList == NULL) {
        gfxCtx.meshList = mesh;
    } else {
        mesh->_gfx_mgr_next = gfxCtx.meshList;
        if(mesh->_gfx_mgr_next != NULL) {
            mesh->_gfx_mgr_next->_gfx_mgr_prev = mesh;
        }
        mesh->_gfx_mgr_prev = NULL;
        gfxCtx.meshList = mesh;
    }

	return mesh;
}

void gfxMeshDestroy(gfxMeshObj_t *mesh) {
	if(mesh == NULL) {
		return;
	}

    if(mesh->_gfx_mgr_prev == NULL) {
        gfxCtx.meshList = mesh->_gfx_mgr_next;
        if(gfxCtx.meshList != NULL) {
            gfxCtx.meshList->_gfx_mgr_prev = NULL;
        }
    } else {
        mesh->_gfx_mgr_prev->_gfx_mgr_next = mesh->_gfx_mgr_next;
        if(mesh->_gfx_mgr_next != NULL) {
            mesh->_gfx_mgr_next->_gfx_mgr_prev = mesh->_gfx_mgr_prev;
        }
    }

	if(mesh->next != NULL) {
		errorf("Mesh object freed with active chain pointer.");
	}
	if(mesh->flags & GFX_MESH_FLAG_COLOR) {
		glDeleteBuffersARB(3, mesh->vboId);
		util_free(mesh->vboColors);
	} else {
		glDeleteBuffersARB(2, mesh->vboId);
	}
	util_free(mesh->vboVertices);
	util_free(mesh->vboTexCoords);
}

void gfxMeshInitCBSet(gfxMeshObj_t *mesh, gfxMeshInitCB_t initCB, void *context) {
	mesh->initCB = initCB;
	mesh->context = context;
}

void gfxMeshClear(gfxMeshObj_t *mesh) {
	if(mesh == NULL) {
		return;
	}
	mesh->pos = 0;
	mesh->dirty = 1;
}

void gfxMeshTextureAssign(gfxMeshObj_t *mesh, const texture_t *texture) {
	if(mesh == NULL) {
		return;
	}
	mesh->texture = texture;
}

void gfxMeshDefaultColor(gfxMeshObj_t *mesh, float red, float green,
						 float blue, float alpha) {
	if(mesh == NULL) {
		return;
	}
	mesh->red = red;
	mesh->green = green;
	mesh->blue = blue;
	mesh->alpha = alpha;
}

void gfxMeshPrependToChain(gfxMeshObj_t **chain, gfxMeshObj_t *mesh) {
	gfxMeshObj_t *walk;

	if((mesh == NULL) || (chain == NULL)) {
		return;
	}

	mesh->next = *chain;
	*chain = mesh;
}

void gfxMeshAppendToChain(gfxMeshObj_t *chain, gfxMeshObj_t *mesh) {
	gfxMeshObj_t *walk;

	if((mesh == NULL) || (chain == NULL)) {
		return;
	}

	for(walk = chain; walk->next != NULL; walk = walk->next);

	walk->next = mesh;
	mesh->prev = walk;
}

void gfxMeshRemoveFromChain(gfxMeshObj_t *chain, gfxMeshObj_t *mesh) {
	gfxMeshObj_t *walk, *last = NULL;

	if(mesh == NULL) {
		return;
	}

	// Remove a mesh from the mesh chain
	if(chain == mesh) {
		warning("Cannot free mesh from the chain that it heads.");
		return;
	}
	for(walk = chain; walk != NULL; last = walk, walk = walk->next) {
		if(walk == mesh) {
			last->next = mesh->next;
			if(mesh->next != NULL) {
				mesh->next->prev = last;
			}
			return;
		}
	}
	warning("Cannot free mesh from chain it is not a member of.");
}

gfxMeshObj_t *gfxMeshFindMeshInChain(gfxMeshObj_t *mesh, const texture_t *texture) {
	gfxMeshObj_t *walk;

	if(mesh == NULL) {
		return NULL;
	}

	for(walk = mesh; walk != NULL; walk = walk->next) {
		if(walk->texture == texture) {
			return walk;
		}
	}
	return NULL;
}

void gfxMeshRender(gfxMeshObj_t *mesh) {
	if(mesh == NULL) {
		return;
	}

	gfxMeshRender(mesh->next);

	if(mesh->initCB != NULL) {
		mesh->initCB(mesh->context);
	}	

    glUseProgram(mesh->program);

	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	if(mesh->dirty) {
		glBindBuffer(GL_ARRAY_BUFFER, mesh->vboId[GFX_MESH_VBO_VERTEX_ID]);
		glBufferData(GL_ARRAY_BUFFER, mesh->pos * sizeof(VBOVertexPos2D_t), mesh->vboVertices, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		glBindBuffer(GL_ARRAY_BUFFER, mesh->vboId[GFX_MESH_VBO_TEXTURE_ID]);
		glBufferData(GL_ARRAY_BUFFER, mesh->pos * sizeof(VBOTexCoord_t), mesh->vboTexCoords, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		if(mesh->flags & GFX_MESH_FLAG_COLOR) {
			glBindBuffer(GL_ARRAY_BUFFER, mesh->vboId[GFX_MESH_VBO_COLOR_ID]);
			glBufferData(GL_ARRAY_BUFFER, mesh->pos * sizeof(VBOColor_t), mesh->vboColors, GL_DYNAMIC_DRAW);
			glBindBuffer(GL_ARRAY_BUFFER, 0);			
		}

		mesh->dirty = 0;
	}

	glEnableClientState(GL_VERTEX_ARRAY);
	glBindBuffer(GL_ARRAY_BUFFER, mesh->vboId[GFX_MESH_VBO_VERTEX_ID]);
	glVertexPointer(2, GL_INT, 0, (void*)(0));

	glBindTexture(GL_TEXTURE_2D, mesh->texture->id);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glBindBuffer(GL_ARRAY_BUFFER, mesh->vboId[GFX_MESH_VBO_TEXTURE_ID]);
	glTexCoordPointer(2, GL_FLOAT, 0, (void*)(0));

	if(mesh->flags & GFX_MESH_FLAG_COLOR) {
		glEnableClientState(GL_COLOR_ARRAY);
		glBindBuffer(GL_ARRAY_BUFFER, mesh->vboId[GFX_MESH_VBO_COLOR_ID]);
		glColorPointer(4, GL_FLOAT, 0, (void*)(0));		
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glDrawArrays(GL_TRIANGLES, 0, mesh->pos);

	if(mesh->flags & GFX_MESH_FLAG_COLOR) {
		glDisableClientState(GL_COLOR_ARRAY);
	}
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	glDisableClientState(GL_VERTEX_ARRAY);

    glUseProgram(0);
}


void gfxMeshQuad(gfxMeshObj_t *mesh, int x1, int y1, int x2, int y2,
			     float s1, float t1, float s2, float t2) {
	gfxMeshQuadAtPos(mesh, mesh->pos, &(mesh->pos), x1, y1, x2, y2, s1, t1, s2, t2);
}

void gfxMeshQuadAtPos(gfxMeshObj_t *mesh, int pos, int *last,
				 	  int x1, int y1, int x2, int y2,
			     	  float s1, float t1, float s2, float t2) {
	int k;
	VBOColor_t *color;
	VBOVertexPos2D_t *vertex;
	VBOTexCoord_t *texture;

	if(mesh->flags & GFX_MESH_FLAG_COLOR) {
		color = mesh->vboColors + pos;
		for(k = 0; k < 6; k++) {
			color->r = mesh->red;
			color->g = mesh->green;
			color->b = mesh->blue;
			color->a = mesh->alpha;
			color++;
		}
	}

	vertex = mesh->vboVertices + pos;
	texture = mesh->vboTexCoords + pos;

	// Triangle 1, vertex 1
	vertex->x = x1;
	vertex->y = y1;
	texture->s = s1;
	texture->t = t1;
	vertex++;
	texture++;

	// Triangle 1, vertex 2
	vertex->x = x2;
	vertex->y = y1;
	texture->s = s2;
	texture->t = t1;
	vertex++;
	texture++;

	// Triangle 1, vertex 3
	vertex->x = x1;
	vertex->y = y2;
	texture->s = s1;
	texture->t = t2;
	vertex++;
	texture++;

	// Triangle 2, vertex 1
	vertex->x = x2;
	vertex->y = y1;
	texture->s = s2;
	texture->t = t1;
	vertex++;
	texture++;

	// Triangle 2, vertex 2
	vertex->x = x2;
	vertex->y = y2;
	texture->s = s2;
	texture->t = t2;
	vertex++;
	texture++;

	// Triangle 2, vertex 3
	vertex->x = x1;
	vertex->y = y2;
	texture->s = s1;
	texture->t = t2;

	if(last != NULL) {
		*last = pos + 6;
	}

	mesh->dirty = 1;
}

void gfxMeshQuadColor(gfxMeshObj_t *mesh, int x1, int y1, int x2, int y2,
					  float s1, float t1, float s2, float t2,
					  float red, float green, float blue, float alpha) {
	gfxMeshQuadColorAtPos(mesh, mesh->pos, &(mesh->pos), x1, y1, x2, y2,
						  s1, t1, s2, t2, red, green, blue, alpha);
}

void gfxMeshQuadColorAtPos(gfxMeshObj_t *mesh, int pos, int *last,
						   int x1, int y1, int x2, int y2,
						   float s1, float t1, float s2, float t2,
						   float red, float green, float blue, float alpha) {
	int k;
	VBOColor_t *color;
	VBOVertexPos2D_t *vertex;
	VBOTexCoord_t *texture;

	if(mesh->flags & GFX_MESH_FLAG_COLOR) {
		color = mesh->vboColors + pos;
		for(k = 0; k < 6; k++) {
			color->r = red;
			color->g = green;
			color->b = blue;
			color->a = alpha;
			color++;
		}
	}

	vertex = mesh->vboVertices + pos;
	texture = mesh->vboTexCoords + pos;

	// Triangle 1, vertex 1
	vertex->x = x1;
	vertex->y = y1;
	texture->s = s1;
	texture->t = t1;
	vertex++;
	texture++;

	// Triangle 1, vertex 2
	vertex->x = x2;
	vertex->y = y1;
	texture->s = s2;
	texture->t = t1;
	vertex++;
	texture++;

	// Triangle 1, vertex 3
	vertex->x = x1;
	vertex->y = y2;
	texture->s = s1;
	texture->t = t2;
	vertex++;
	texture++;

	// Triangle 2, vertex 1
	vertex->x = x2;
	vertex->y = y1;
	texture->s = s2;
	texture->t = t1;
	vertex++;
	texture++;

	// Triangle 2, vertex 2
	vertex->x = x2;
	vertex->y = y2;
	texture->s = s2;
	texture->t = t2;
	vertex++;
	texture++;

	// Triangle 2, vertex 3
	vertex->x = x1;
	vertex->y = y2;
	texture->s = s1;
	texture->t = t2;

	if(last != NULL) {
		*last = pos + 6;
	}

	mesh->dirty = 1;
}

///////////////////////////////////////////////////////////////////////////////
// Blit and render
///////////////////////////////////////////////////////////////////////////////

void gfxTexBlit(const texture_t *texture, int x1, int y1) {
    glBindTexture(GL_TEXTURE_2D, texture->id);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBegin(GL_QUADS);
        glTexCoord2f(0.0, 0.0);
        glVertex2i(x1, y1);
        glTexCoord2f(1.0, 0.0);
        glVertex2i(x1 + texture->width, y1);
        glTexCoord2f(1.0, 1.0);
        glVertex2i(x1 + texture->width, y1 + texture->height);
        glTexCoord2f(0.0, 1.0);
        glVertex2i(x1, y1 + texture->height);
    glEnd();
}

void gfxTexBlitSub(const texture_t *texture, int tx1, int ty1, int tx2, int ty2,
				   int x1, int y1, int x2, int y2) {
	float u1, v1, u2, v2;

	u1 = SCALED_T_COORD(texture->width, tx1);
	v1 = SCALED_T_COORD(texture->height, ty1);
	u2 = SCALED_T_COORD(texture->width, tx2);
	v2 = SCALED_T_COORD(texture->height, ty2);

	glBindTexture(GL_TEXTURE_2D, texture->id);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBegin(GL_QUADS);
		glTexCoord2f(u1, v1);
		glVertex2i(x1, y1);
		glTexCoord2f(u2, v1);
		glVertex2i(x2, y1);
		glTexCoord2f(u2, v2);
		glVertex2i(x2, y2);
		glTexCoord2f(u1, v2);
		glVertex2i(x1, y2);
	glEnd();
}

void gfxTexTile(const texture_t *texture, int tx1, int ty1, int tx2, int ty2,
			    int x1, int y1, int x2, int y2) {
	tx2++;
	ty2++;
	int width = (x2 - x1) + 1;
	int height = (y2 - y1) + 1;
	int tileWidth = (tx2 - tx1) + 1;
	int tileHeight = (ty2 - ty1) + 1;
	int progressWidth = 0;
	int progressHeight = 0;
	float u1, v1, u2, v2;
	int a1, b1, a2, b2;
	int adj;

    u1 = SCALED_T_COORD(texture->width, tx1);
    v1 = SCALED_T_COORD(texture->height, ty1);

	glBindTexture(GL_TEXTURE_2D, texture->id);
	glEnable(GL_BLEND);
	while ((progressHeight < height) || (progressWidth < width)) {
		for(; progressHeight < height; progressHeight += tileHeight) {
			for(progressWidth = 0; progressWidth < width; progressWidth += tileWidth) {
				a1 = x1 + progressWidth;
				if((width - progressWidth) <= tileWidth) {
					// This fragment is only partial width
					a2 = x2;
					adj = tx1 + (a2 - a1) - 1;
					u2 = SCALED_T_COORD(texture->width, adj);
				} else {
					a2 = a1 + tileWidth;
					u2 = SCALED_T_COORD(texture->width, tx2);
				}
				b1 = y1 + progressHeight;
				if((height - progressHeight) <= tileHeight) {
					// This fragment is only partial height
					b2 = y2;
					adj = ty1 + (b2 - b1) - 1;
					v2 = SCALED_T_COORD(texture->height, adj);
				} else {
					b2 = b1 + tileHeight;
					v2 = SCALED_T_COORD(texture->height, ty2);
				}
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				glBegin(GL_QUADS);
					glTexCoord2f(u1, v1);
					glVertex2i(a1, b1);
					glTexCoord2f(u2, v1);
					glVertex2i(a2, b1);
					glTexCoord2f(u2, v2);
					glVertex2i(a2, b2);
					glTexCoord2f(u1, v2);
					glVertex2i(a1, b2);
				glEnd();
			}
		}
	}
}

void gfxSpriteBlit(const flubSprite_t *sprite, int num, int x, int y) {
	int row = num / sprite->spritesPerRow;
	int column = num % sprite->spritesPerRow;
	int tx1, ty1, tx2, ty2;

	tx1 = column * sprite->width;
	tx2 = tx1 + sprite->width - 1;
	ty1 = row * sprite->height;
	ty2 = ty1 + sprite->height - 1;

	gfxTexBlitSub(sprite->texture, tx1, ty1, tx2, ty2, x, y,
				  x + sprite->width, y + sprite->height);
}

void gfxSpriteBlitResize(const flubSprite_t *sprite, int num, int x1, int y1,
						 int x2, int y2) {
	int row = num / sprite->spritesPerRow;
	int column = num % sprite->spritesPerRow;
	int tx1, ty1, tx2, ty2;

	tx1 = column * sprite->width;
	tx2 = tx1 + sprite->width;
	ty1 = row * sprite->height;
	ty2 = ty1 + sprite->height;

	gfxTexBlitSub(sprite->texture, tx1, ty1, tx2, ty2, x1, y1, x2, y2);
}

void gfxSliceBlit(const flubSlice_t *slice, int x1, int y1, int x2, int y2) {
	int ref, dim[2][6], x, y;

	if(slice->texture->id == 0) {
		return;
	}

	if(slice->locked & GFX_X_LOCKED) {
		x2 = x1 + slice->width;
	} else if((x2 - x1) < slice->width) {
		x2 += slice->width - ( x2 - x1 );
	} if(slice->locked & GFX_Y_LOCKED) {
		y2 = y1 + slice->width;
	} else if((y2 - y1) < slice->height) {
		y2 += slice->height - (y2 - y1);
	}

	dim[GFX_X_COORDS][0] = x1;
	dim[GFX_Y_COORDS][0] = y1;
	dim[GFX_X_COORDS][1] = x1 + slice->xPre;
	dim[GFX_Y_COORDS][1] = y1 + slice->yPre;
	dim[GFX_X_COORDS][2] = x2 - slice->xPost;
	dim[GFX_Y_COORDS][2] = y2 - slice->yPost;
	dim[GFX_X_COORDS][3] = x2;
	dim[GFX_Y_COORDS][3] = y2;

	glBindTexture(GL_TEXTURE_2D, slice->texture->id);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glLoadIdentity();
	for(y = 0; y < 3; y++) {
		if((y > 0) && (slice->tcoords[GFX_Y_COORDS][y] == slice->tcoords[GFX_Y_COORDS][y - 1])) {
			continue;
		}
		for(x = 0; x < 3; x++) {
			if((x > 0) && (slice->tcoords[GFX_X_COORDS][x] == slice->tcoords[GFX_X_COORDS][x - 1])) {
				continue;
			}
			glBegin( GL_QUADS );
				glTexCoord2f(slice->tcoords[GFX_X_COORDS][x],
							 slice->tcoords[GFX_Y_COORDS][y]);
				glVertex2i(dim[GFX_X_COORDS][x],
						   dim[GFX_Y_COORDS][y]);
				glTexCoord2f(slice->tcoords[GFX_X_COORDS][x + 1],
							 slice->tcoords[GFX_Y_COORDS][y]);
				glVertex2i(dim[GFX_X_COORDS][x + 1],
						   dim[GFX_Y_COORDS][ y]);
				glTexCoord2f(slice->tcoords[GFX_X_COORDS][x + 1],
							 slice->tcoords[GFX_Y_COORDS][y + 1]);
				glVertex2i(dim[GFX_X_COORDS][ x + 1],
						   dim[GFX_Y_COORDS][ y + 1]);
				glTexCoord2f(slice->tcoords[GFX_X_COORDS][x],
							 slice->tcoords[GFX_Y_COORDS][y + 1]);
				glVertex2i(dim[GFX_X_COORDS][x],
						   dim[GFX_Y_COORDS][y + 1]);
			glEnd();
			if((x == 0) && (slice->locked & GFX_Y_LOCKED)) {
				break;
			}
		}
		if(slice->locked & GFX_X_LOCKED) {
			break;
		}
	}
}

#if 0
void gfxKeycapBlit(font_t *font, const char *caption, int x, int y,
				   int *width, int *height) {
	int keyheight;
	int keywidth;
    int contentWidth;
	int key = -1;
	int size;
	int idx;
	int kx1, ky1, kx2, ky2;

	keyheight = fontGetHeight(font) + KEYCAP_VERT_SPACER + KEYCAP_VERT_SPACER;

	if((idx = _gfxIsSpecialKey(caption)) >= 0) {
		_gfxSpecialKeyCoords(idx, fontGetHeight(font), &kx1, &ky1, &kx2, &ky2);
		keywidth = kx2 - kx1 + KEYCAP_HORZ_SPACER + KEYCAP_HORZ_SPACER;
	} else {
		keywidth = fontGetStrWidth(font, caption) + KEYCAP_HORZ_SPACER + KEYCAP_HORZ_SPACER;
	}

    fontSetColor(1.0, 1.0, 1.0);
	gfxSliceBlit(gfxCtx.keycapSlice, x, y,
				 x + keywidth,
				 y + keyheight);

	fontMode();
	fontSetColor(0.0, 0.0, 0.0);
	if(idx >= 0) {
		gfxTexBlitSub(gfxCtx.flubMisc,
					  kx1, ky1, kx2, ky2,
					  x + KEYCAP_HORZ_SPACER, y + KEYCAP_VERT_SPACER,
					  x + kx2 - kx1 + KEYCAP_HORZ_SPACER,
					  y + ky2 - ky1 + KEYCAP_VERT_SPACER);
	} else {
		fontPos(x + KEYCAP_HORZ_SPACER, y + KEYCAP_VERT_SPACER + 2);
		fontBlitStr(font, caption);
	}

	if(width != NULL) {
		*width = keywidth;
	}
	if(height != NULL) {
		*height = keyheight;
	}
}
#endif

void gfxKeycapBlit(font_t *font, const char *caption, int x, int y,
				   int *width, int *height) {
	int keyheight;
	int keywidth;
    int contentwidth;
    int xoffset = 0;
	int key = -1;
	int size;
	int idx;
	int kx1, ky1, kx2, ky2;

	keyheight = fontGetHeight(font) + KEYCAP_VERT_SPACER + KEYCAP_VERT_SPACER;

	if((idx = _gfxIsSpecialKey(caption)) >= 0) {
		_gfxSpecialKeyCoords(idx, fontGetHeight(font), &kx1, &ky1, &kx2, &ky2);
		contentwidth = kx2 - kx1;
        keywidth = contentwidth + KEYCAP_HORZ_SPACER + KEYCAP_HORZ_SPACER;
	} else {
		contentwidth = fontGetStrWidth(font, caption);
        keywidth = contentwidth + KEYCAP_HORZ_SPACER + KEYCAP_HORZ_SPACER;
	}
    if(keywidth < keyheight) {
        xoffset = (keyheight - keywidth) / 2;
        keywidth = keyheight;
    }

    fontSetColor(1.0, 1.0, 1.0);
	gfxSliceBlit(gfxCtx.keycapSlice, x, y,
				 x + keywidth + xoffset,
				 y + keyheight);
	
	fontMode();
	fontSetColor(0.0, 0.0, 0.0);
	if(idx >= 0) {
		gfxTexBlitSub(gfxCtx.flubMisc,
					  kx1, ky1, kx2, ky2,
					  x + + xoffset + KEYCAP_HORZ_SPACER, y + KEYCAP_VERT_SPACER,
					  x + kx2 - kx1 + KEYCAP_HORZ_SPACER + xoffset,
					  y + ky2 - ky1 + KEYCAP_VERT_SPACER);
	} else {
		fontPos(x + KEYCAP_HORZ_SPACER + xoffset, y + KEYCAP_VERT_SPACER + 2);
		fontBlitStr(font, caption);
	}

	if(width != NULL) {
		*width = keywidth;
	}
	if(height != NULL) {
		*height = keyheight;
	}
}

const char *_gfxFindMetaKey(const char *str, const char **start, const char **stop) {
    const char *ptr, *end, *walk;
    static char buf[64];

    while((ptr = strchr(str, '[')) != NULL) {
        if((end = strchr(ptr, ']')) != NULL) {
            strncpy(buf, ptr + 1, end - ptr - 1);
            buf[end - ptr - 1] = '\0';
            if(start != NULL) {
                *start = ptr;
            }
            if(stop != NULL) {
                *stop = end;
            }
            return buf;
        }
        ptr++;
    }
    return NULL;
}

void gfxBlitKeyStr(font_t *font, const char *str, int x, int y, int *width, int *height) {
    const char *ptr;
    const char *end;
    int yoffset;
    const char *key = "";
    int keywidth;
    int w = 0;
    fontColor_t colors;

    gfxKeyMetrics(font, "", NULL, height, &yoffset);

    fontGetColor3(&colors);
    for(;;) {
        if((key = _gfxFindMetaKey(str, &ptr, &end)) != NULL) {
            if(ptr != str) {
                fontPos(x + w, y + yoffset);
                fontSetColor3(&colors);
                fontBlitStrN(font, str, ptr - str);
                w += fontGetStrNWidth(font, str, ptr - str);
            }

            str = end + 1;
            gfxKeycapBlit(font, key, x + w, y, &keywidth, NULL);
            w += keywidth;
        } else {
            fontPos(x + w, y + yoffset);
            fontSetColor3(&colors);
            fontBlitStr(font, str);
            w += fontGetStrWidth(font, str);
            break;
        }
    }

    if(width != NULL) {
        *width = w;
    }
}


int gfxTexMeshBlit(gfxMeshObj_t *mesh, const texture_t *texture, int x1, int y1) {	
	gfxMeshObj_t *target;

	if((mesh == NULL) || ((target = gfxMeshFindMeshInChain(mesh, texture)) == NULL)) {
		return 0;
	}

	if((target->pos + MESH_QUAD_SIZE(1)) >= target->length) {
		return 0;
	}

	gfxTexMeshBlitSubAtPos(target, target->pos, &(target->pos), texture, 0, 0,
						   texture->width - 1, texture->height - 1,
						   x1, y1, x1 + texture->width, y1 + texture->height);

	return 1;
}

int gfxTexMeshBlitSub(gfxMeshObj_t *mesh, const texture_t *texture,
	 				  int tx1, int ty1, int tx2, int ty2,
	 				  int x1, int y1, int x2, int y2) {
	gfxMeshObj_t *target;

	if((mesh == NULL) || ((target = gfxMeshFindMeshInChain(mesh, texture)) == NULL)) {
		return 0;
	}

	if((target->pos + MESH_QUAD_SIZE(1)) >= target->length) {
		return 0;
	}

	gfxTexMeshBlitSubAtPos(target, target->pos, &(target->pos), texture,
						   tx1, ty1, tx2, ty2, x1, y1, x2, y2);

	return 1;
}

int gfxTexMeshTile(gfxMeshObj_t *mesh, const texture_t *texture,
				   int tx1, int ty1, int tx2, int ty2,
				   int x1, int y1, int x2, int y2) {
	gfxMeshObj_t *target;
	int rows;
	int columns;
	int width = x2 - x1;
	int height = y2 - y1;
	int tileWidth = tx2 + 1 - tx1;
	int tileHeight = ty2 + 1 - ty1;
	int quads;

	if((mesh == NULL) || ((target = gfxMeshFindMeshInChain(mesh, texture)) == NULL)) {
		return 0;
	}

	columns = (width / tileWidth) + ((width % tileWidth) ? 1 : 0);
	rows = (height / tileHeight) + ((height % tileHeight) ? 1 : 0);
	quads = columns * rows;

	if((target->pos + MESH_QUAD_SIZE(quads)) >= target->length) {
		return 0;
	}

	gfxTexMeshTileAtPos(target, target->pos, &(target->pos), texture,
						tx1, ty1, tx2, ty2, x1, y1, x2, y2);

	return 1;
}

int gfxSpriteMeshBlit(gfxMeshObj_t *mesh, const flubSprite_t *sprite, int num,
					  int x, int y) {
	gfxMeshObj_t *target;

	if((mesh == NULL) || ((target = gfxMeshFindMeshInChain(mesh, sprite->texture)) == NULL)) {
		return 0;
	}

	if((target->pos + MESH_QUAD_SIZE(1)) >= target->length) {
		return 0;
	}

	gfxSpriteMeshBlitAtPos(target, target->pos, &(target->pos), sprite, num, x, y);

	return 1;
}

int gfxSpriteMeshBlitResize(gfxMeshObj_t *mesh, const flubSprite_t *sprite,
							int num, int x1, int y1, int x2, int y2) {
	gfxMeshObj_t *target;

	if((mesh == NULL) || ((target = gfxMeshFindMeshInChain(mesh, sprite->texture)) == NULL)) {
		return 0;
	}

	if((target->pos + MESH_QUAD_SIZE(1)) >= target->length) {
		return 0;
	}

	gfxSpriteMeshBlitResizeAtPos(target, target->pos, &(target->pos), sprite, num,
								 x1, y1, x2, y2);

	return 1;
}

int gfxSliceMeshBlit(gfxMeshObj_t *mesh, const flubSlice_t *slice,
					 int x1, int y1, int x2, int y2) {
	gfxMeshObj_t *target;

	if((mesh == NULL) || ((target = gfxMeshFindMeshInChain(mesh, slice->texture)) == NULL)) {
		return 0;
	}

	if((target->pos + MESH_QUAD_SIZE(slice->quads)) >= target->length) {
		return 0;
	}

	gfxSliceMeshBlitAtPos(target, target->pos, &(target->pos), slice,
						  x1, y1, x2, y2);

	return 1;
}

int gfxKeycapMeshBlit(gfxMeshObj_t *mesh, font_t *font, const char *caption,
				  int x, int y, int *width, int *height) {
	int keyheight;
	int keywidth;
	int key = -1;
	int size;
	int idx;
	int kx1, ky1, kx2, ky2;
	gfxMeshObj_t *keyTarget, *fontTarget;

	if(mesh == NULL) {
		return 0;
	}
	if((keyTarget = gfxMeshFindMeshInChain(mesh, gfxCtx.flubMisc)) == NULL) {
		return 0;
	}
	if((fontTarget = gfxMeshFindMeshInChain(mesh, fontTextureGet(font))) == NULL) {
		return 0;
	}

	if(_gfxIsSpecialKey(caption)) {
		if((keyTarget->pos + MESH_QUAD_SIZE(10)) >= keyTarget->length) {
			return 0;
		}
	} else {
		if((keyTarget->pos + MESH_QUAD_SIZE(9)) >= keyTarget->length) {
			return 0;
		}
		if((fontTarget->pos + MESH_QUAD_SIZE(_gfxCalcKeycapCaptionQuads(caption))) >= fontTarget->length) {
			return 0;
		}

	}

	if((fontTarget->pos + MESH_QUAD_SIZE(9)) >= fontTarget->length) {
		return 0;
	}

	keyheight = fontGetHeight(font) + KEYCAP_VERT_SPACER + KEYCAP_VERT_SPACER;

	if((idx = _gfxIsSpecialKey(caption)) >= 0) {
		_gfxSpecialKeyCoords(idx, fontGetHeight(font), &kx1, &ky1, &kx2, &ky2);
		keywidth = kx2 - kx1 + KEYCAP_HORZ_SPACER + KEYCAP_HORZ_SPACER;
	} else {
		keywidth = fontGetStrWidth(font, caption) + KEYCAP_HORZ_SPACER + KEYCAP_HORZ_SPACER;
	}

	gfxMeshDefaultColor(keyTarget, 1.0, 1.0, 1.0, 1.0);
	gfxSliceMeshBlit(keyTarget, gfxCtx.keycapSlice, x, y, x + keywidth, y + keyheight);

	if(idx >= 0) {
		gfxMeshDefaultColor(keyTarget, 0.0, 0.0, 0.0, 1.0);
		gfxTexMeshBlitSub(keyTarget, gfxCtx.flubMisc,
					  	  kx1, ky1, kx2, ky2,
					  	  x + KEYCAP_HORZ_SPACER, y + KEYCAP_VERT_SPACER,
					  	  x + kx2 - kx1 + KEYCAP_HORZ_SPACER,
					  	  y + ky2 - ky1 + KEYCAP_VERT_SPACER);
	} else {
		gfxMeshDefaultColor(fontTarget, 0.0, 0.0, 0.0, 1.0);
		fontSetColor(0.0, 0.0, 0.0);
		fontPos(x + KEYCAP_HORZ_SPACER, y + KEYCAP_VERT_SPACER + 2);
		fontBlitStrMesh(fontTarget, font, caption);
	}

	if(width != NULL) {
		*width = keywidth;
	}
	if(height != NULL) {
		*height = keyheight;
	}

	return 1;
}

void gfxTexMeshBlitAtPos(gfxMeshObj_t *mesh, int pos, int *last,
						const texture_t *texture, int x1, int y1) {
	gfxTexMeshBlitSubAtPos(mesh, pos, last, texture, 0, 0,
						   texture->width - 1, texture->height - 1,
						   x1, y1, x1 + texture->width - 1, y1 + texture->height - 1);
}

void gfxTexMeshBlitSubAtPos(gfxMeshObj_t *mesh, int pos, int *last,
						   const texture_t *texture, int tx1, int ty1,
						   int tx2, int ty2, int x1, int y1, int x2, int y2) {	
	gfxMeshObj_t *target;
	float u1, v1, u2, v2;

	if((mesh == NULL) || ((target = gfxMeshFindMeshInChain(mesh, texture)) == NULL)) {
		return;
	}

	u1 = SCALED_T_COORD(texture->width, tx1);
	v1 = SCALED_T_COORD(texture->height, ty1);
	u2 = SCALED_T_COORD(texture->width, tx2);
	v2 = SCALED_T_COORD(texture->height, ty2);

	gfxMeshQuadAtPos(target, pos, last, x1, y1, x2, y2, u1, v1, u2, v2);
}

void gfxTexMeshTileAtPos(gfxMeshObj_t *mesh, int pos, int *last,
						const texture_t *texture, int tx1, int ty1,
						int tx2, int ty2, int x1, int y1, int x2, int y2) {
	tx2++;
	ty2++;

	gfxMeshObj_t *target;
	int width = (x2 - x1 + 1);
	int height = (y2 - y1 + 1);
	int tileWidth = (tx2 - tx1 + 1);
	int tileHeight = (ty2 - ty1 + 1);
	int progressWidth = 0;
	int progressHeight = 0;
	float u1, v1, u2, v2;
	int a1, b1, a2, b2;
	int adj;

	if((mesh == NULL) || ((target = gfxMeshFindMeshInChain(mesh, texture)) == NULL)) {
		return;
	}

    u1 = SCALED_T_COORD(texture->width, tx1);
    v1 = SCALED_T_COORD(texture->height, ty1);

	while ((progressHeight < height) || (progressWidth < width)) {
		for(; progressHeight < height; progressHeight += tileHeight) {
			for(progressWidth = 0; progressWidth < width; progressWidth += tileWidth) {
				a1 = x1 + progressWidth;
				if((width - progressWidth) <= tileWidth) {
					// This fragment is only partial width
					a2 = x2;
					adj = tx1 + (a2 - a1) - 1;
					u2 = SCALED_T_COORD(texture->width, adj);
				} else {
					a2 = a1 + tileWidth;
					u2 = SCALED_T_COORD(texture->width, tx2);
				}
				b1 = y1 + progressHeight;
				if((height - progressHeight) <= tileHeight) {
					// This fragment is only partial height
					b2 = y2;
					adj = ty1 + (b2 - b1) - 1;
					v2 = SCALED_T_COORD(texture->height, adj);
				} else {
					b2 = b1 + tileHeight;
					v2 = SCALED_T_COORD(texture->height, ty2);
				}

				gfxMeshQuadAtPos(target, pos, last, a1, b1, a2, b2, u1, v1, u2, v2);
				pos += 6;
			}
		}
	}
}

void gfxSpriteMeshBlitAtPos(gfxMeshObj_t *mesh, int pos, int *last,
						   const flubSprite_t *sprite, int num, int x, int y) {
	gfxMeshObj_t *target;

	if((mesh == NULL) || ((target = gfxMeshFindMeshInChain(mesh, sprite->texture)) == NULL)) {
		return;
	}

	int row = num / sprite->spritesPerRow;
	int column = num % sprite->spritesPerRow;
	int tx1, ty1, tx2, ty2;

	tx1 = column * sprite->width;
	tx2 = tx1 + sprite->width - 1;
	ty1 = row * sprite->height;
	ty2 = ty1 + sprite->height - 1;

	gfxTexMeshBlitSubAtPos(target, pos, last, sprite->texture,
						   tx1, ty1, tx2, ty2, x, y,
						   x + sprite->width, y + sprite->height);
}

void gfxSpriteMeshBlitResizeAtPos(gfxMeshObj_t *mesh, int pos, int *last,
								 const flubSprite_t *sprite, int num,
								 int x1, int y1, int x2, int y2) {
	gfxMeshObj_t *target;
	int row = num / sprite->spritesPerRow;
	int column = num % sprite->spritesPerRow;
	int tx1, ty1, tx2, ty2;

	if((mesh == NULL) || ((target = gfxMeshFindMeshInChain(mesh, sprite->texture)) == NULL)) {
		return;
	}

	tx1 = column * sprite->width;
	tx2 = tx1 + sprite->width;
	ty1 = row * sprite->height;
	ty2 = ty1 + sprite->height;

	gfxTexMeshBlitSubAtPos(target, pos, last, sprite->texture,
						   tx1, ty1, tx2, ty2, x1, y1, x2, y2);
}

void gfxSliceMeshBlitAtPos(gfxMeshObj_t *mesh, int pos, int *last,
						  const flubSlice_t *slice, int x1, int y1, int x2, int y2) {
	gfxMeshObj_t *target;
	int ref, dim[2][6], x, y;


	if((mesh == NULL) || ((target = gfxMeshFindMeshInChain(mesh, slice->texture)) == NULL)) {
		return;
	}

	if(slice->texture->id == 0) {
		return;
	}

	if(slice->locked & GFX_X_LOCKED) {
		x2 = x1 + slice->width;
	} else if((x2 - x1) < slice->width) {
		x2 += slice->width - ( x2 - x1 );
	} if(slice->locked & GFX_Y_LOCKED) {
		y2 = y1 + slice->width;
	} else if((y2 - y1) < slice->height) {
		y2 += slice->height - (y2 - y1);
	}

	dim[GFX_X_COORDS][0] = x1;
	dim[GFX_Y_COORDS][0] = y1;
	dim[GFX_X_COORDS][1] = x1 + slice->xPre;
	dim[GFX_Y_COORDS][1] = y1 + slice->yPre;
	dim[GFX_X_COORDS][2] = x2 - slice->xPost;
	dim[GFX_Y_COORDS][2] = y2 - slice->yPost;
	dim[GFX_X_COORDS][3] = x2;
	dim[GFX_Y_COORDS][3] = y2;

	for(y = 0; y < 3; y++) {
		if((y > 0) && (slice->tcoords[GFX_Y_COORDS][y] == slice->tcoords[GFX_Y_COORDS][y - 1])) {
			continue;
		}
		for(x = 0; x < 3; x++) {
			if((x > 0) && (slice->tcoords[GFX_X_COORDS][x] == slice->tcoords[GFX_X_COORDS][x - 1])) {
				continue;
			}
			gfxMeshQuadAtPos(target, pos, last,
							 dim[GFX_X_COORDS][x], dim[GFX_Y_COORDS][y],
							 dim[GFX_X_COORDS][x + 1], dim[GFX_Y_COORDS][ y + 1],
							 slice->tcoords[GFX_X_COORDS][x], slice->tcoords[GFX_Y_COORDS][y],
							 slice->tcoords[GFX_X_COORDS][x + 1], slice->tcoords[GFX_Y_COORDS][y + 1]);
			pos += 6;
			if((x == 0) && (slice->locked & GFX_Y_LOCKED)) {
				break;
			}
		}
		if(slice->locked & GFX_X_LOCKED) {
			break;
		}
	}
}

void gfxMeshRangeAlphaSet(gfxMeshObj_t *mesh, int start, int end, float alpha) {
	int k;
	VBOColor_t *color;

	if((mesh == NULL) || (!(mesh->flags & GFX_MESH_FLAG_COLOR))) {
		return;
	}
	if(start > mesh->pos) {
		return;
	}

	if(end > mesh->pos) {
		end = mesh->pos;
	}

	color = mesh->vboColors + start;
	for(k = start; k <= end; k++) {
		color->a = alpha;
		color++;
	}
	mesh->dirty = 1;
}

/*
typedef void (*gfxEffectCB_t)(gfxEffect_t *effect, void *context);
typedef void (*gfxEffectCleanupCB_t)(void *context);


typedef struct gfxEffect_s gfxEffect_t;
struct gfxEffect_s {
	gfxMeshObj_t *mesh;
	Uint32 tickStart;
	Uint32 tickDuration;
	Uint32 tickCurrent;
	int indexStart;
	int indexEnd;
	void *context;

	gfxEffectCleanupCB_t cleanup;

	gfxEffectCB_t handler;
	gfxEffectCB_t start;
	gfxEffectCB_t completed;

	gfxEffect_t *next;
};

static void _gfxEffectStart(gfxEffect_t *effect, void *context) {
}

static void _gfxEffectHandler(gfxEffect_t *effect, void *context) {
}

static void _gfxEffectCompleted(gfxEffect_t *effect, void *context) {
}

static void _gfxEffectCleanup(void *context) {
}

*/

static void _gfxEffectCommonCleanup(void *context) {
	util_free(context);
}

gfxEffect_t *gfxEffectMotionLinear(gfxMeshObj_t *mesh, int startPos,
								   int endPos, int xMove, int yMove, int duration) {
	gfxEffect_t *effect;

	return NULL;
}

gfxEffect_t *gfxEffectScale(gfxMeshObj_t *mesh, int startPos, int endPos,
							float xStart, float xEnd, float yStart, float yEnd,
							int xOffset, int yOffset, int duration) {
	return NULL;
}

typedef struct _gfxEffectFadeContext_s {
	float alphaStart;
	float alphaEnd;
} _gfxEffectFadeContext_t;

static void _gfxEffectFadeHandler(gfxEffect_t *effect, void *context) {
	_gfxEffectFadeContext_t *fade = (_gfxEffectFadeContext_t *)context;
	float amount;
	float value;

	amount = ((float)(effect->tickCurrent)) / ((float)(effect->tickDuration));
	if(fade->alphaEnd < fade->alphaStart) {
		value = fade->alphaStart - ((fade->alphaStart - fade->alphaEnd) * amount);
	} else {
		value = fade->alphaStart + ((fade->alphaEnd - fade->alphaStart) * amount);
	}

	gfxMeshRangeAlphaSet(effect->mesh, effect->indexStart, effect->indexEnd, value);
}

gfxEffect_t *gfxEffectFade(gfxMeshObj_t *mesh, int startPos, int endPos,
						   float alphaStart, float alphaEnd, int duration) {
	gfxEffect_t *effect;
	_gfxEffectFadeContext_t *context;

	context = util_alloc(sizeof(_gfxEffectFadeContext_t), NULL);
	context->alphaStart = alphaStart;
	context->alphaEnd = alphaEnd;

	effect = gfxEffectCreate(mesh, _gfxEffectFadeHandler, startPos, endPos,
							 NULL, NULL, NULL, 1, context);

	effect->tickDuration = duration;

	return effect;
}

gfxEffect_t *gfxEffectShade(gfxMeshObj_t *mesh, int startPos, int endPos,
							float red, float green, float blue, float alpha,
							int duration) {
	return NULL;
}

gfxEffect_t *gfxEffectPulse(gfxMeshObj_t *mesh, int startPos, int endPos,
							float initRed, float initGreen, float initBlue,
							float initAlpha, int midDuration,
							float midRed, float midGreen, float midBlue,
							float midAlpha, int duration,
							float endRed, float endGreen, float endBlue,
							float endAlpha, int loop) {
	return NULL;
}

gfxEffect_t *gfxEffectCreate(gfxMeshObj_t *mesh, gfxEffectCB_t handler,
							 int indexBegin, int indexEnd,
							 gfxEffectCB_t start, gfxEffectCB_t end,
							 gfxEffectCleanupCB_t cleanup, int autoCleanup,
							 void *context) {
	gfxEffect_t *effect;

	effect = util_calloc(sizeof(gfxEffect_t), 0, NULL);
	effect->mesh = mesh;
	effect->handler = handler;
	effect->cleanup = ((cleanup == NULL) ? _gfxEffectCommonCleanup : cleanup);
	effect->start = start;
	effect->completed = end;
	effect->autoCleanup = autoCleanup;
	effect->indexStart = indexBegin;
	effect->indexEnd = indexEnd;
	effect->context = context;

	return effect;
}

void gfxEffectDestroy(gfxEffect_t *effect) {
	if(effect->cleanup != NULL) {
		effect->cleanup(effect->context);
		effect->context = NULL;
	} else if(effect->context != NULL) {
		errorf("Memory leak in gfxEffect_t context, due to missing cleanup handler");
	}
	util_free(effect);
}

void gfxEffectRegister(gfxEffect_t *effect) {
	effect->next = gfxCtx.activeEffects;
	gfxCtx.activeEffects = effect;
}

void gfxEffectUnregister(gfxEffect_t *effect) {
	gfxEffect_t *walk, *last = NULL;

	for(walk = gfxCtx.activeEffects; walk != NULL; last = walk, walk = walk->next) {
		if(walk == effect) {
			if(last == NULL) {
				gfxCtx.activeEffects = walk->next;
			} else {
				last->next = walk->next;
			}
		}
	}
}

void gfxUpdate(Uint32 ticks) {
	gfxEffect_t *effect, *last, *next, *walk;

	for(effect = gfxCtx.activeEffects; effect != NULL; effect = next) {
		next = effect->next;
		if(effect->tickStart == 0) {
			effect->tickStart = ticks;
			if(effect->start != NULL) {
				effect->start(effect, effect->context);
			}
		}
		effect->tickCurrent = ticks - effect->tickStart;
		if(effect->handler != NULL) {
			effect->handler(effect, effect->context);
		}
		if(effect->tickCurrent >= effect->tickDuration) {
			if(effect->completed != NULL) {
				effect->completed(effect, effect->context);
			}
			for(last = NULL, walk = gfxCtx.activeEffects; walk != NULL; last = walk, walk = walk->next) {
				if(walk == effect) {
					if(last == NULL) {
						gfxCtx.activeEffects = effect->next;
					} else {
						last->next = effect->next;
					}
				}
			}
			if(effect->autoCleanup) {
				gfxEffectDestroy(effect);
			}
		}
	}
}

