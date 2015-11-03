#ifdef WIN32
#	define GLEW_STATIC
#	include <flub/3rdparty/glew/glew.h>
#endif

#include <SDL2/SDL.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <SDL2/SDL_opengl.h>

#include <flub/video.h>
#include <flub/config.h>
#include <flub/memory.h>
#include <flub/logger.h>
#include <flub/util/file.h>
#include <flub/util/parse.h>
#include <flub/util/enum.h>
#include <flub/util/color.h>
#include <flub/input.h>
#include <flub/app.h>
#include <physfs.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <flub/3rdparty/stb/stb_image_write.h>
#include <flub/data/critbit.h>


// Direct linking GLEW:
// * http://stackoverflow.com/questions/12397603/c-undefined-reference-to-imp-glewinit0

#define MAX_SCREENSHOT_FNAME_LEN    128

static int _videoWidth = 0;
static int _videoHeight = 0;

const int *const videoWidth = &_videoWidth;
const int *const videoHeight = &_videoHeight;

typedef struct videoShaderEntry_s {
    GLuint shaderId;
    GLenum shaderType;
    char *filename;
} videoShaderEntry_t;

typedef struct videoMode_s {
    int width;
    int height;
    eVideoRatio aspect;
    Uint32 format;
    struct videoMode_s *next;
} videoMode_t;

typedef struct videoNotifiee_s {
    videoNotifieeCB_t callback;
    struct videoNotifiee_s *next;
} videoNotifiee_t;

typedef struct videoCtx_s {
    int init;
    int active;
    float red, green, blue;
    videoNotifiee_t *notifiees;

    int fullscreen;
    int width;
    int height;
    int bpp;

    unsigned char *screenshotBuffer;

    SDL_GLContext glContext;
    SDL_Window *window;

    videoMode_t *modes;

    critbit_t shaders;
    critbit_t programs;
} videoCtx_t;


videoCtx_t _videoCtx = {
    .init = 0,
    .active = 0,
    .red = 0.0,
    .green = 0.0,
    .blue = 0.0,
    .notifiees = NULL,
    
    .fullscreen = 0,
    .width = 0,
    .height = 0,
    .bpp = 0,

    .screenshotBuffer = NULL,

    .glContext = NULL,
    .window = NULL,

    .modes = NULL,

    .shaders = CRITBIT_TREE_INIT,
    .programs = CRITBIT_TREE_INIT
};


void _videoAddNewEnumMode(videoMode_t **modes, int width, int height, Uint32 format) {
    videoMode_t *walk, *tmp;
    
    for(walk = *modes; walk != NULL; walk = walk->next) {
        if((walk->width == width) && (walk->height == height) && (format == format)) {
            return;
        }
    }

    tmp = util_alloc(sizeof(videoMode_t), NULL);
    memset(tmp, 0, sizeof(videoMode_t));
    tmp->width = width;
    tmp->height = height;
    tmp->aspect = videoModeGetRatio(width, height);
    tmp->format = format;

    if(*modes == NULL) {
        *modes = tmp;
    } else {
        if((width < (*modes)->width) ||
           ((width == (*modes)->width) && (height < (*modes)->height))) {
            tmp->next = *modes;
            *modes = tmp;
            return;
        } else {
            for(walk = *modes; ((walk != NULL) && (walk->next != NULL)); walk = walk->next) {
                if((width < walk->next->width) ||
                   ((width == walk->next->width) && (height < walk->next->height))) {
                    tmp->next = walk->next->next;
                    walk->next = tmp;
                    return;
                }                
            }
            walk->next = tmp;
        }
    }
}

void _videoEnumModes(void) {
    int count;
    int k;
    SDL_DisplayMode mode;
    Uint32 format;

    count = SDL_GetNumDisplayModes(0);
    for(k = 0; k < count; k++) {
        if(SDL_GetDisplayMode(0, k, &mode) != 0) {
            errorf("SDL_GetDisplayMode() failed: %s", SDL_GetError());
            return;
        }
        _videoAddNewEnumMode(&(_videoCtx.modes), mode.w, mode.h, mode.format);
    }
}

void _videoFreeVideoEnumList(void) {
    videoMode_t *walk, *next;
    
    for(walk = _videoCtx.modes; walk != NULL; walk = next) {
        next = walk->next;
        free(walk);
    }
}

void _videoCloseDisplay(void) {
    if(_videoCtx.glContext != NULL) {
        SDL_GL_DeleteContext(_videoCtx.glContext);
        _videoCtx.glContext == NULL;
    }
    if(_videoCtx.window != NULL) {
        SDL_DestroyWindow(_videoCtx.window);
        _videoCtx.window == NULL;
    }
    _videoCtx.active = 0;
}

int videoSettingsChangeCallback(const char *name, const char *value);

int videoInit(void) {
    if(_videoCtx.init) {
        warning("Ignoring attempt to re-initialize the video.");
        return 1;
    }

    if(!logValid()) {
        // The logger has not been initialized!
        return 0;
    }

    logDebugRegister("video", DBG_VIDEO, "shaders", DBG_VID_DTL_SHADERS);

    if(!flubCfgValid()) {
        fatal("Cannot initialize the video module: config was not initialized");
        return 0;
    }

    if(!flubCfgOptAdd("videomode", appDefaults.videoMode,
                      FLUB_CFG_FLAG_CLIENT,
                      videoSettingsChangeCallback)) {
        error("Failed to register config option \"videomode\".");
        return 0;
    } else if(!flubCfgOptAdd("fullscreen", appDefaults.fullscreen,
                             FLUB_CFG_FLAG_CLIENT,
                             videoSettingsChangeCallback)) {
        error("Failed to register config option \"fullscreen\".");
        return 0;
    } else if(!flubCfgOptAdd("vsync", "true", FLUB_CFG_FLAG_CLIENT,
                             videoSettingsChangeCallback)) {
        error("Failed to register config option \"vsync\".");
        return 0;
    }

    // Enumerate the fullscreen modes available
    _videoEnumModes();    

    _videoCtx.init = 1;
    
    return 1;
}

int videoValid(void) {
    return _videoCtx.init;
}

void videoShutdown(void) {
    if(!_videoCtx.init) {
        return;
    }

    _videoFreeVideoEnumList();
    _videoCloseDisplay();

    _videoCtx.init = 0;
}

static void _videoScreenshotHandler(SDL_Event *event, int pressed, int motion, int x, int y);

int videoStart(void) {
    const char *mode;
    int w, h;

    if(!inputActionRegister("General", 0, "screenshot", "Save a screen capture", _videoScreenshotHandler)) {
        return 0;
    }

    // Preload the video mode defaults
    mode = flubCfgOptStringGet("videomode");
    if(!parseValXVal(mode, &w, &h)) {
        fatalf("Video mode \"%s\" is not valid.", mode);
        return 0;
    }
    _videoCtx.fullscreen = 0;

    if(!videoModeSetByStr(flubCfgOptStringGet("videomode"), flubCfgOptBoolGet("fullscreen"))) {
        return 0;
    }

    GLuint vs, fs, oglp;
    if(((vs = videoGetShader(GL_VERTEX_SHADER, "resources/shaders/simple.vert")) == 0) ||
       ((fs = videoGetShader(GL_FRAGMENT_SHADER, "resources/shaders/simple.frag")) == 0) ||
       ((oglp = videoCreateProgram("simple")) == 0)) {
        return 0;
    }
    glAttachShader(oglp, vs);
    glAttachShader(oglp, fs);
    glLinkProgram(oglp);
    glUseProgram(oglp);
    if(!videoValidateOpenGLProgram(oglp)) {
        return 0;
    }
    glUseProgram(0);

    return 1;
}

void videoUpdate(void) {
    if(_videoCtx.active) {
        SDL_GL_SwapWindow(_videoCtx.window);
    }
}

#define VID_RATIO_VARIANCE 0.05

eVideoRatio videoModeGetRatio(int width, int height) {
    float ratio;
    float four_three = 3.0 / 4.0;
    float five_four = 4.0 / 5.0;
    float sixteen_nine = 9.0 / 16.0;
    float sixteen_ten = 10.0 / 16.0;
    
    if(width == 0) {
        return eVideoRatio_unknown;
    }
    ratio = ((float)height) / ((float)width);
    if(((ratio - VID_RATIO_VARIANCE) < four_three) &&
       ((ratio + VID_RATIO_VARIANCE) > four_three)) {
        return eVideoRatio_4_3;
    }
    if(((ratio - VID_RATIO_VARIANCE) < five_four) &&
       ((ratio + VID_RATIO_VARIANCE) > five_four)) {
        return eVideoRatio_5_4;
    }
    if(((ratio - VID_RATIO_VARIANCE) < sixteen_nine) &&
       ((ratio + VID_RATIO_VARIANCE) > sixteen_nine)) {
        return eVideoRatio_16_9;
    }
    if(((ratio - VID_RATIO_VARIANCE) < sixteen_ten) &&
       ((ratio + VID_RATIO_VARIANCE) > sixteen_ten)) {
        return eVideoRatio_16_10;
    }
    return eVideoRatio_unknown;
}


utilEnumMap_t videoRatioStrings[] = {
    {eVideoRatio_unknown, "unknown"},
    {eVideoRatio_4_3, "4:3"},
    {eVideoRatio_5_4, "5:4"},
    {eVideoRatio_16_9, "16:9"},
    {eVideoRatio_16_10, "16:10"},
    END_ENUM_MAP()
};


const char *videoModeGetRatioString(eVideoRatio ratio) {
    return utilEnum2Str(videoRatioStrings, ratio, "unknown");
}

void videoEnumModes(videoEnumCB_t callback) {   
    videoMode_t *entry;

    for(entry = _videoCtx.modes; entry != NULL; entry = entry->next) {
        if(!callback(entry->width, entry->height, entry->aspect, SDL_BITSPERPIXEL(entry->format))) {
            return;
        }
    }
}

void videoSetClearColor(float red, float green, float blue) {
    _videoCtx.red = red;
    _videoCtx.green = green;
    _videoCtx.blue = blue;
}

void videoSetClearColori(int red, int green, int blue) {
    videoSetClearColor(COLOR_ITOF(red),
                       COLOR_ITOF(green),
                       COLOR_ITOF(blue));
}

void videoClear(void) {
    if(_videoCtx.active) {
	   glClearColor(_videoCtx.red, _videoCtx.green, _videoCtx.blue, 0);
	   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
}

utilEnumMap_t videoOpenGLErrorCodeStrMap[] = {
    ENUM_MAP(GL_INVALID_ENUM),
    ENUM_MAP(GL_INVALID_VALUE),
    ENUM_MAP(GL_INVALID_OPERATION),
    ENUM_MAP(GL_NO_ERROR),
    ENUM_MAP(GL_STACK_OVERFLOW),
    ENUM_MAP(GL_STACK_UNDERFLOW),
    ENUM_MAP(GL_OUT_OF_MEMORY),
    END_ENUM_MAP(),
};

void videoLogOpenGLError(const char *prefix) {
    errorf("%s: OpenGL error %s", prefix,
           utilEnum2Str(videoOpenGLErrorCodeStrMap,
           glGetError(),"UNKNOWN"));
}

int videoSettingsChangeCallback(const char *name, const char *value) {
    int width, height;
    int fullscreen;

    if(!strcmp(name, "videomode")) {
        if(!appDefaults.allowVideoModeChange) {
            error("Attempt to change video mode failed since it is not enabled.");
            return 0;
        }
        if(!parseValXVal(value, &width, &height)) {
            errorf("Failed to set video mode, since \"%s\" is not a valid mode.", value);
            return 0;
        }
        if((_videoCtx.width != width) || (_videoCtx.height != height)) {
            if(_videoCtx.init) {
                if(!videoModeSet(width, height, _videoCtx.fullscreen)) {
                    fatal( "Failed to set the video mode." );
                    return 0;
                }
            }
        }
    }
    else if(!strcmp(name, "fullscreen")) {
        if(!appDefaults.allowFullscreenChange) {
            error("Attempt to change video fullscreen failed since it is not enabled.");
            return 0;
        }
        fullscreen = utilBoolFromString(value, _videoCtx.fullscreen);
        if(_videoCtx.fullscreen != fullscreen) {
            if(_videoCtx.init) {
                if(!videoModeSet(_videoCtx.width, _videoCtx.height, fullscreen)) {
                    fatal("Failed to set the video mode.");
                    return 0;
                }
            }
        }
    } else {
        errorf("Attempt to change unknown video attribute \"%s\" failed.",
               name);
        return 0;
    }
    return 1;
}

void _videoCallNotifiees(int width, int height, int fullscreen) {
    videoNotifiee_t *entry;
    
    for(entry = _videoCtx.notifiees; entry != NULL; entry = entry->next) {
        entry->callback(width, height, fullscreen);
    }
}

int videoModeSet(int width, int height, int fullscreen) {
    int fail = 0;
    char modeStr[ 32 ];

    if(_videoCtx.init) {
        if((width == _videoCtx.width) && (height == _videoCtx.height) &&
           (fullscreen == _videoCtx.fullscreen)) {
            // Hrm, there's no actual change
            return 1;
        }
    }

    _videoCloseDisplay();

    // use OpenGL 2.1 - has all needed features and is widely supported
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

    _videoCtx.window = SDL_CreateWindow(appDefaults.title,
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        width, height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | ((fullscreen) ? SDL_WINDOW_FULLSCREEN : 0));

    if(!_videoCtx.window) {
        fatalf("SDL could not create window: %s", SDL_GetError());
        fail = 1;
    } else {
        // GL related setup
        _videoCtx.glContext = SDL_GL_CreateContext(_videoCtx.window);
        if(!_videoCtx.glContext) {
            fatalf("SDL could not create GL context: %s", SDL_GetError());
            fail = 1;
        }
    }

    if(!fail) {
        // Initialize GLEW
        glewExperimental = GL_TRUE;
        GLenum glewError = glewInit();
        if(glewError != GLEW_OK) {
            errorf("Error initializing GLEW: %s", glewGetErrorString(glewError));
            fail = 1;
        }

        if(!(GLEW_ARB_vertex_shader && GLEW_ARB_fragment_shader)) {
            errorf("This computer does not support OpenGL vertex and fragment shaders.");
            fail = 1;
        }
    }

    if(!fail) {
        // setup the config.use_vsync option
        if (SDL_GL_GetSwapInterval() != -1) {
            if (SDL_GL_SetSwapInterval(flubCfgOptBoolGet("vsync") ? 1 : 0) < 0) {
                warning("Unable to configure VSync");
            }
        } else {
            warning("Configuring VSync is not supported!");
        }

        // orange: glClearColor(1.0, 0.75, 0.0, 1.0);
        glClearColor(_videoCtx.red, _videoCtx.green, _videoCtx.blue, 1.0);
    }

    if(fail) {
        _videoCloseDisplay();
        return 0;
    }

    glEnable(GL_TEXTURE_2D);

    _videoWidth = width;
    _videoHeight = height;

    _videoCtx.screenshotBuffer = util_alloc(((3 * width * height) + (width * 3)), NULL);

    if((_videoCtx.width != width) || (_videoCtx.height != height)) {
        snprintf(modeStr, sizeof(modeStr) - 1, "%dx%d", width, height);
        _videoCtx.width = width;
        _videoCtx.height = height;
        flubCfgOptStringSet("videomode", modeStr, 0);
    }
    if(_videoCtx.fullscreen != fullscreen) {
        _videoCtx.fullscreen = fullscreen;
        flubCfgOptBoolSet("fullscreen", fullscreen, 0);
    }
    
    infof("Video mode set to %dx%d%s", width, height, ((fullscreen) ? " fullscreen" : ""));

    _videoCtx.active = 1;
    _videoCallNotifiees(width, height, fullscreen);
    
    return 1;
}

int videoModeSetByStr(const char *mode, int fullscreen) {
	int width, height;

	if(!parseValXVal(mode, &width, &height)) {
		errorf("\"%s\" is not a valid video mode.", mode);
		return 0;
	}
	return videoModeSet(width, height, fullscreen);
}

int videoGetVideoDetails(int *width, int *height, int *fullscreen) {
    if(_videoCtx.init) {
        if(width != NULL) {
            *width = _videoCtx.width;
        }
        if(height != NULL) {
            *height = _videoCtx.height;
        }
        if(fullscreen != NULL) {
            *fullscreen = _videoCtx.fullscreen;
        }
        return 1;
    }
    return 0;
}

int videoWidthGet(void) {
    return _videoCtx.width;
}

int videoHeightGet(void) {
    return _videoCtx.height;
}

int videoIsFullscreen(void) {
    return _videoCtx.fullscreen;
}

SDL_GLContext vidoGLContextGet(void) {
    return _videoCtx.glContext;
}

SDL_Window *videoSDLWindowGet(void) {
    return _videoCtx.window;
}

void videoPushGLState(void) {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
}

void videoPopGLState(void) {
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
}

void videoOrthoMode(void) {
    // Caller should call videoPushGLState() prior, and videoPopGLState() after
    // http://stackoverflow.com/questions/5467218/opengl-2d-hud-over-3d
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, _videoCtx.width, _videoCtx.height, 0, -10, 10);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void videoOrthoModef(void) {
    // Caller should call videoPushGLState() prior, and videoPopGLState() after
    // http://stackoverflow.com/questions/5467218/opengl-2d-hud-over-3d
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-((float)_videoCtx.width), ((float)_videoCtx.width), -((float)_videoCtx.height), ((float)_videoCtx.height), -50, 50);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void videoPerspectiveMode(float fov) {
    const float znear = 1.0;
    const float zfar = 100.0;
    float ymax, xmax;
    float temp, temp2, temp3, temp4;

	glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glViewport(0, 0, _videoCtx.width, _videoCtx.height);

    ymax = znear * tanf(fov * M_PI / 360.0);
    xmax = ymax * (((float)_videoCtx.width) / ((float)_videoCtx.height));

    glLoadIdentity();
    glFrustum(-xmax, xmax, -ymax, ymax, znear, zfar);
}

int videoAddNotifiee(videoNotifieeCB_t callback) {
    videoNotifiee_t *entry;
    
    for(entry = _videoCtx.notifiees; entry != NULL; entry = entry->next) {
        if(entry->callback == callback) {
            warning("Failed to add existing video notifier callback.");
            return 0;
        }
    }
    entry = util_calloc(sizeof(videoNotifiee_t), 0, NULL);
    entry->callback = callback;
    entry->next = _videoCtx.notifiees;
    _videoCtx.notifiees = entry;
    return 1;
}

int videoRemoveNotifiee(videoNotifieeCB_t callback) {
    videoNotifiee_t *walk, *last = NULL;
    
    for(walk = _videoCtx.notifiees; walk != NULL; walk = walk->next) {
        if(walk->callback == callback) {
            if(last == NULL) {
                _videoCtx.notifiees = walk->next;
            } else {
                last->next = walk->next;
            }
            free(walk);
            return 1;
        }
        last = walk;
    }
    return 0;
}

void videoRemoveAllNotifiees(void) {
    videoNotifiee_t *walk, *swap;
    
    for(walk = _videoCtx.notifiees; walk != NULL; walk = swap) {
        swap = walk->next;
        free(walk);
    }
}

static void _videoFlipScreenshotBuffer(void) {
    int y;
    int linelen;
    unsigned char *lineA, *lineB, *swap;

    linelen = (_videoCtx.width * 3);
    swap = (unsigned char *)_videoCtx.screenshotBuffer;
    lineA = swap + linelen;
    lineB = lineA + (linelen * (_videoCtx.height - 1));

    for(y = 0; y < (_videoCtx.height / 2); y++) {
        memcpy(swap, lineB, linelen);
        memcpy(lineB, lineA, linelen);
        memcpy(lineA, swap, linelen);
        lineA += linelen;
        lineB -= linelen;
    }
}

void videoScreenshot(const char *fname) {
    char *name;
    char buffer[MAX_SCREENSHOT_FNAME_LEN];

    name = utilGenFilename(fname, "png", 9999, buffer, sizeof(buffer));
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glReadPixels(0, 0, _videoCtx.width, _videoCtx.height, GL_RGB, GL_UNSIGNED_BYTE, _videoCtx.screenshotBuffer);
    _videoFlipScreenshotBuffer();
    stbi_write_png(name, _videoCtx.width, _videoCtx.height, 3, _videoCtx.screenshotBuffer, 0);
}

static void _videoScreenshotHandler(SDL_Event *event, int pressed, int motion, int x, int y) {
    videoScreenshot("screenshot");
}

int videoValidateOpenGLProgram(GLuint object) {
    int status;

    glGetProgramiv(object, GL_LINK_STATUS, &status);
    if(status != GL_TRUE) {
        errorf("OpenGL program is not valid");
        videoPrintGLInfoLog(0, object);
        return 0;
    }
    return 1;
}

static void _videoDumpShaderSource(const char *src) {
#ifdef FLUB_DEBUG
    char buf[256];
    int len;
    int line = 1;
    const char *ptr, *next;

    infof("Length: %d", strlen(src));
    for(ptr = src; *ptr != '\0'; line++) {
        if((next = strchr(ptr, '\n')) != NULL) {
            len = next - ptr;
            if(len > 1) {
                if(len > (sizeof(buf) - 1)) {
                    len = sizeof(buf) - 1;
                }
                strncpy(buf, ptr, len);
                buf[len] = '\0';
                buf[sizeof(buf) - 1] = '\0';
                debugf(DBG_VIDEO, DBG_VID_DTL_SHADERS, "[%3d] %s", line, buf);
            }
            ptr = next;
            if(*ptr == '\n') {
                ptr++;
            }
        } else {
            debugf(DBG_VIDEO, DBG_VID_DTL_SHADERS, "[%3d] %s", line, ptr);
            break;
        }
    }
#endif // FLUB_DEBUG
}

void videoPrintGLInfoLog(int shader, GLuint object) {
    int len;
    int written;
    char *infoLog;
    char *ptr, *next, *last;

    if(shader) {
        glGetShaderiv(object, GL_INFO_LOG_LENGTH, &len);
    } else {
        glGetProgramiv(object, GL_INFO_LOG_LENGTH, &len);
    }

    if(len > 0) {
        infoLog = util_calloc(len, 0, NULL);
        if(shader) {
            glGetShaderInfoLog(object, len, &written, infoLog);
        } else {
            glGetProgramInfoLog(object, len, &written, infoLog);
        }
        last = infoLog + len;
        ptr = infoLog;
        while(1) {
            if((next = strchr(infoLog, '\n')) != NULL) {
                *next = '\0';
            }
            error(ptr);
            if(next != NULL) {
                ptr = next + 1;
                if(*ptr == '\0') {
                    break;
                }
            } else {
                break;
            }
        }
        util_free(infoLog);
    }
}

GLuint videoGetShader(GLenum shaderType, const char *filename) {
    videoShaderEntry_t *entry;
    PHYSFS_file *handle;
    int len;
    GLchar *buffer;
    int status;

    // Check if the shader has already been loaded
    if(critbitContains(&(_videoCtx.shaders), filename, (void **)(&entry))) {
        return entry->shaderId;
    }

    if((handle = PHYSFS_openRead(filename)) == NULL) {
        errorf("Unable to open the shader file \"%s\": %s", filename, PHYSFS_getLastError());
        return 0;
    }

    if((len = PHYSFS_fileLength(handle)) == 0) {
        errorf("The shader file \"%s\" is empty", filename);
        PHYSFS_close(handle);
        return 0;
    }

    debugf(DBG_VIDEO, DBG_VID_DTL_SHADERS, "Attempting to load shader \"%s\", size %d", filename, len);

    buffer = util_alloc(len + 1, NULL);

    if((PHYSFS_read(handle, buffer, len, 1) != 1)) {
        errorf("Failed to read the shader file \"%s\": %s", PHYSFS_getLastError());
        PHYSFS_close(handle);
        util_free(buffer);
        return 0;
    }
    buffer[len] = '\0';

    entry = util_calloc(sizeof(videoShaderEntry_t), 0, NULL);
    entry->shaderType = shaderType;

    entry->shaderId = glCreateShader(shaderType);

    glShaderSource(entry->shaderId, 1, (const GLchar **)&buffer, NULL);

    glCompileShader(entry->shaderId);

    glGetShaderiv(entry->shaderId, GL_COMPILE_STATUS, &status);
    if(status != GL_TRUE) {
        errorf("Failed building the shader \"%s\"", filename);
        videoPrintGLInfoLog(1, entry->shaderId);
        _videoDumpShaderSource(buffer);
        util_free(buffer);
        glDeleteShader(entry->shaderId);
        util_free(entry);
        return 0;
    }

    debugf(DBG_VIDEO, DBG_VID_DTL_SHADERS, "Loaded shader \%s\"", filename);

    util_free(buffer);
    entry->filename = strdup(filename);
    critbitInsert(&(_videoCtx.shaders), entry->filename, entry, NULL);
    return entry->shaderId;
}

GLuint videoCreateProgram(const char *name) {
    GLuint *entry;
    int len;
    char *buffer;
    const char *cbuffer;
    int status;

    // Check if the shader has already been loaded
    if(critbitContains(&(_videoCtx.programs), name, (void **)(&entry))) {
        return *entry;
    }

    entry = util_alloc(sizeof(GLuint), NULL);

    *entry = glCreateProgram();

    critbitInsert(&(_videoCtx.programs), name, entry, NULL);
    return *entry;
}

GLuint videoGetProgram(const char *name) {
    GLuint *entry;

    // Check if the shader has already been loaded
    if(critbitContains(&(_videoCtx.programs), name, (void **)(&entry))) {
        return *entry;
    }

    return 0;
}
