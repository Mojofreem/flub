#ifndef _FLUB_VIDEO_HEADER_
#define _FLUB_VIDEO_HEADER_


#include <SDL2/SDL.h>
#ifdef MACOSX
#	include <OpenGL/gl.h>
#	include <OpenGL/glext.h>
#	include <OpenGL/glu.h>
#else // MACOSX
#	include <gl/gl.h>
#	include <gl/glext.h>
#	include <gl/glu.h>
#endif // MACOSX

#define DBG_VID_DTL_SHADERS     1
#define DBG_VID_DTL_CAPTURE		2

extern const int *const videoWidth;
extern const int *const videoHeight;


int videoInit(void);
int videoValid(void);
void videoShutdown(void);
int videoStart(void);
void videoUpdate(void);

typedef enum {
    eVideoRatio_unknown = 0,
    eVideoRatio_4_3,
    eVideoRatio_5_4,
    eVideoRatio_16_9,
    eVideoRatio_16_10,
} eVideoRatio;

typedef int (*videoEnumCB_t)(int width, int height, eVideoRatio aspect, int bpp);

eVideoRatio videoModeGetRatio(int width, int height);
const char *videoModeGetRatioString(eVideoRatio ratio);

void videoEnumModes(videoEnumCB_t callback);

void videoSetClearColor(float red, float green, float blue);
void videoSetClearColori(int red, int green, int blue);
void videoClear(void);

void videoLogOpenGLError(const char *prefix);

int videoModeSet(int width, int height, int fullscreen);
int videoModeSetByStr(const char *mode, int fullscreen);

int videoGetVideoDetails(int *width, int *height, int *fullscreen);
int videoWidthGet(void);
int videoHeightGet(void);
int videoIsFullscreen(void);
SDL_GLContext vidoGLContextGet(void);
SDL_Window *videoSDLWindowGet(void);

void videoPushGLState(void);
void videoPopGLState(void);

void videoOrthoMode(void);
void videoOrthoModef(void);
void videoPerspectiveMode(float fov);

typedef void (*videoNotifieeCB_t)(int width, int height, int fullscreen);
int videoAddNotifiee(videoNotifieeCB_t callback);
int videoRemoveNotifiee(videoNotifieeCB_t callback);
void videoRemoveAllNotifiees(void);

void videoRatioResize(int refWidth, int refHeight, int *width, int *height);
// Given width and height, determine the largest size that fits within
// the initial values that matches the current video size ratio
void videoScreenRatioResize(int *width, int *height);
size_t videoFrameMemSize(int width, int height);
void videoImageResize(unsigned char *srcData, int srcWidth, int srcHeight,
                      unsigned char *destData, int destWidth, int destHeight,
                      int bytesPerPixel, int forceRatio,
                      int red, int green, int blue, int alpha);
void videoScreenCapture(unsigned char *data, int width, int height);
void videoScreenshot(const char *fname);

int videoValidateOpenGLProgram(GLuint object);
void videoPrintGLInfoLog(int shader, GLuint object);

// GL_VERTEX_SHADER || GL_FRAGMENT_SHADER
GLuint videoGetShader(GLenum shaderType, const char *filename);
GLuint videoCreateProgram(const char *name);
GLuint videoGetProgram(const char *name);


#endif // _FLUB_VIDEO_HEADER_

