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

void videoScreenshot(const char *fname);


#endif // _FLUB_VIDEO_HEADER_

