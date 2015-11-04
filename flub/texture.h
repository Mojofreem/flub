#ifndef _FLUB_TEXTURE_HEADER_
#define _FLUB_TEXTURE_HEADER_


#include <SDL2/SDL.h>
#include <gl/gl.h>

// Flub distinguishes between 3 types of textures:
//     * anonymous - loaded and passed to the caller immediately. this resource
//                   can be shared, but it's up to the creator to manage it's
//                   usage instances and lifecycle.
//     * named -     registered with the texture manager by a nuemonic name,
//                   this resource can be requested repeatedly by the
//                   application. The texture manager tracks it's usage, and
//                   may free up the resource at it's own discretion when it
//                   is no longer being used.
//     * derived -   derived from a subsection of another texture. This resource
//                   can be named or anonymous, but both types are still managed
//                   by the texture manager. This type is commonly used when a
//                   portion of a larger texture is to be tiled.

typedef struct texture_s
{
    GLuint id;
    int width;
    int height;
    Uint32 delay;
    struct textureAnim_s *next;
} texture_t;


#define textureWidth(tptr)      ((tptr)->width)
#define textureHeight(tptr)     ((tptr)->height)
#define textureId(tptr)         ((tptr)->id)
#define textureAnimated(tptr)   ((tptr)->delay > 0)


int texmgrInit(void);
int texmgrValid(void);
void texmgrShutdown(void);

int texmgrRegister(const char *name, const char *filename,
                   GLint minfilter, GLint magfilter, int colorkey,
                   int red, int green, int blue);

texture_t *texmgrLoad(const char *filename, const char *name,
                            GLint minfilter, GLint magfilter, int colorkey,
                            int red, int green, int blue);

texture_t *texmgrCreate(const char *name, GLint minfilter,
                              GLint magfilter, int colorkey, int red, int green,
                              int blue, int components, GLenum format,
                              void *data, int width, int height);

texture_t *texmgrSubdivideTexture(const texture_t *texture,
                                        const char *name,
                                        int x1, int y1, int x2, int y2,
                                        GLint minfilter, GLint magfilter);

// Texture configuration string:
//     name|filename|min|mag|colorkey (#rgb|#rrggbb|r,g,b)
int texmgrRegStr(const char *str);
int texmgrRegFile(const char *filename);

texture_t *texmgrGet(const char *name);
int texmgrRelease(const texture_t *tex);

const char *texmgrGLMinFilterStr(GLint filter);
const char *texmgrGLMagFilterStr(GLint filter);
const char *texmgrGLFormatStr(GLenum format);

GLint texmgrStrToGLMinFilter(const char *str);
GLint texmgrStrToGLMagFilter(const char *str);
GLenum texmgrStrToGLFormat(const char *str);


#endif // _FLUB_TEXTURE_HEADER_
