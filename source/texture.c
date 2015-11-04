#include <ctype.h>
#include <flub/physfsutil.h>
#include <flub/memory.h>
#include <flub/texture.h>
#include <flub/thread.h>
#include <flub/data/critbit.h>
#include <flub/logger.h>
#include <flub/video.h>
#include <flub/util/string.h>
#include <flub/util/parse.h>
#include <flub/util/enum.h>

#define STB_IMAGE_IMPLEMENTATION
#include <flub/3rdparty/stb/stb_image.h>

#define TEX_TRACE   1


#define TEX_FLAG_COLORKEY   0x01
#define TEX_FLAG_NAMED      0x02


// TODO Incorporate loading animated GIF's
// TODO Subdivide from an animated GIF
// TODO provide animated texture API support


typedef struct texEntry_s {
    texture_t texture;
    int flags;
    char *filename;
    GLint minfilter;
    GLint magfilter;
    int red;
    int green;
    int blue;
    unsigned char *data;
    GLenum format;
    int components;
    int count;
    struct texEntry_s *next;
} texEntry_t;


struct
{
    int init;
    mutex_t *mutex;
    critbit_t registry;
    texEntry_t *anonEntries;
} g_texmgrCtx = {
        .init = 0,
        .mutex = NULL,
        .registry = CRITBIT_TREE_INIT,
        .anonEntries = NULL,
    };


utilEnumMap_t _texmgrGLMinFilters[] = {
        {GL_NEAREST,                "GL_NEAREST"},
        {GL_LINEAR,                 "GL_LINEAR"},
        {GL_NEAREST_MIPMAP_NEAREST, "GL_NEAREST_MIPMAP_NEAREST"},
        {GL_LINEAR_MIPMAP_NEAREST,  "GL_LINEAR_MIPMAP_NEAREST"},
        {GL_NEAREST_MIPMAP_LINEAR,  "GL_NEAREST_MIPMAP_LINEAR"},
        {GL_LINEAR_MIPMAP_LINEAR,   "GL_LINEAR_MIPMAP_LINEAR"},
        END_ENUM_MAP(),
};

utilEnumMap_t _texmgrGLMagFilters[] = {
        {GL_NEAREST, "GL_NEAREST"},
        {GL_LINEAR,  "GL_LINEAR"},
        END_ENUM_MAP(),
};

utilEnumMap_t _texmgrGLFormats[] = {
        {GL_ALPHA,           "GL_ALPHA"},
        {GL_RGB,             "GL_RGB"},
        {GL_RGBA,            "GL_RGBA"},
        {GL_LUMINANCE,       "GL_LUMINANCE"},
        {GL_LUMINANCE_ALPHA, "GL_LUMINANCE_ALPHA"},
        END_ENUM_MAP(),
};


static int _loadTextureIntoOpenGL(texEntry_t *entry) {
    // Have OpenGL generate a texture object handle for us
    glGenTextures(1, &entry->texture.id);

    // Bind the texture object
    glBindTexture(GL_TEXTURE_2D, entry->texture.id);

    // Set the texture's stretching properties
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, entry->minfilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, entry->magfilter);

    // TODO Investigate use case of specifying texture wrap settings
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexImage2D(GL_TEXTURE_2D, 0, entry->format, entry->texture.width, entry->texture.height, 0,
                 entry->format, GL_UNSIGNED_BYTE, entry->data);

    return 1;
}

static int _texmgrEntryIter(const char *name, void *data, void *arg) {
    texEntry_t *entry = (texEntry_t *)data;

    if(entry->data != NULL) {
        if(!_loadTextureIntoOpenGL(entry)) {
            entry->texture.id = 0;
            debugf(DBG_TEXTURE, TEX_TRACE, "Failed to reload texture \"%s\"", name);
        } else {
            debugf(DBG_TEXTURE, TEX_TRACE, "Reloading texture \"%s\"", name);
        }
    } else {
        debugf(DBG_TEXTURE, TEX_TRACE, "Skiped texture \"%s\", since it is not loaded", name);
    }

    return 1;
}

static void _texmgrVideoNotifieeCB(int width, int height, int fullscreen) {
    texEntry_t *walk;

    // Video context has been changed, reload textures
    mutexGrab(g_texmgrCtx.mutex);

    // Walk all named textures in the texture registry
    critbitAllPrefixed(&g_texmgrCtx.registry, "", _texmgrEntryIter, NULL);

    // Walk all anonymous texturs
    for(walk = g_texmgrCtx.anonEntries; walk != NULL; walk = walk->next) {
        _loadTextureIntoOpenGL(walk);
    }

    mutexRelease(g_texmgrCtx.mutex);
}

int texmgrInit(void) {
    if(g_texmgrCtx.init) {
        warning("Ignoring attempt to re-initialize the texture manager.");
        return 1;
    }

    if(!logValid()) {
        // The logger has not been initialized!
        fatal("Cannot initialize texture module: logger has not been initialized");
        return 0;
    }

    if(!videoValid()) {
        fatal("Cannot initialize texture module: video has not been initialized");
        return 0;
    }

    logDebugRegister("texture", DBG_TEXTURE, "trace", TEX_TRACE);

    debug(DBG_TEXTURE, TEX_TRACE, "Texture manager init");
    g_texmgrCtx.mutex = mutexCreate();

    videoAddNotifiee(_texmgrVideoNotifieeCB);

    if(!texmgrRegFile("resources/data/texlist.txt")) {
        debug(DBG_TEXTURE, TEX_TRACE, "Failed to load texture resource descriptor file.");
        return 0;
    }

    g_texmgrCtx.init = 1;
    
    return 1;
}

int texmgrValid(void) {
    return g_texmgrCtx.init;
}

void texmgrShutdown(void) {
    if(!g_texmgrCtx.init) {
        return;
    }

    // TODO Cleanup texture registry on shutdown

    g_texmgrCtx.init = 0;
}

static void _texmgrAnonStore(texEntry_t *entry) {
    entry->next = g_texmgrCtx.anonEntries;
    g_texmgrCtx.anonEntries = entry;
}

static texEntry_t *_texmgrEntryCreate(const char *name, const char *filename,
                                      GLint minfilter, GLint magfilter, int colorkey,
                                      int red, int green, int blue) {
    texEntry_t *entry;

    debugf(DBG_TEXTURE, TEX_TRACE, "Creating texture entry [%s][%s] %s(%d) %s(%d) %s 0x%2.2X 0x%2.2X 0x%2.2X",
           ((name != NULL) ? name : "<anon>"),
           ((filename != NULL) ? filename : "<derived>"),
           texmgrGLMinFilterStr(minfilter), minfilter,
           texmgrGLMagFilterStr(magfilter), magfilter,
           ((colorkey)?"ColorKey":"Opaque"),
           red, green, blue);

    mutexGrab(g_texmgrCtx.mutex);

    if(name != NULL) {
        if (critbitContains(&g_texmgrCtx.registry, name, (void *) &entry)) {
            mutexRelease(g_texmgrCtx.mutex);
            debugf(DBG_TEXTURE, TEX_TRACE, "Ignoring attempt to overwrite existing texture entry \"%s\".", name);
            return NULL;
        }
    }

    entry = util_calloc(sizeof(texEntry_t), 0, NULL);
    if(filename != NULL) {
        entry->filename = util_strdup(filename);
    }
    entry->minfilter = minfilter;
    entry->magfilter = magfilter;
    if(colorkey) {
        entry->flags |= TEX_FLAG_COLORKEY;
        entry->red = red;
        entry->green = green;
        entry->blue = blue;
    }

    if(name != NULL) {
        if(!critbitInsert(&g_texmgrCtx.registry, name, entry, NULL)) {
            mutexRelease(g_texmgrCtx.mutex);
            debugf(DBG_TEXTURE, TEX_TRACE, "Failed to add texture entry \"%s\" into texture registry.", name);
            util_free(entry);
            return NULL;
        }
    }

    mutexRelease(g_texmgrCtx.mutex);
    if(name != NULL) {
        debugf(DBG_TEXTURE, TEX_TRACE, "Created texture entry \"%s\".", name);
    } else {
        debugf(DBG_TEXTURE, TEX_TRACE, "Created anonymous texture entry.", name);
    }
    return entry;
}

int texmgrRegister(const char *name, const char *filename,
                   GLint minfilter, GLint magfilter, int colorkey,
                   int red, int green, int blue) {
    if(_texmgrEntryCreate(name, filename, minfilter, magfilter, colorkey, red, green, blue) == NULL) {
        return 0;
    }
    return 1;
}

static void _applyImageColorKey(texEntry_t *entry) {
    int x, y;
    unsigned char red, green, blue;
    int index;

    if(entry->data == NULL) {
        return;
    }

    switch(entry->components) {
        case 1:
        case 2:
        case 3:
        default:
            warning("Cannot apply alpha to image without alpha channel.");
            break;
        case 4:
            index = 0;
            for(y = 0; y < entry->texture.height; y++) {
                for(x = 0; x < entry->texture.width; x++) {
                    red = entry->data[index];
                    green = entry->data[index + 1];
                    blue = entry->data[index + 2];
                    if((red == entry->red) && (green == entry->green) && (blue == entry->blue)) {
                        entry->data[index + 3] = 0;
                    }
                    index += 4;
                }
            }
            break;
    }
}

int _physfsReadHandler(void *user, char *data, int size) {
    // fill 'data' with 'size' bytes.  return number of bytes actually read
    PHYSFS_File *handle = (PHYSFS_File *)user;

    return PHYSFS_read(handle, data, 1, size);
}

void _physfsSkipHandler(void *user, int n) {
    // skip the next 'n' bytes
    PHYSFS_File *handle = (PHYSFS_File *)user;

    PHYSFS_seek(handle, PHYSFS_tell(handle) + n);
}

int _physfsEofHandler(void *user) {
    // returns nonzero if we are at end of file/data
    PHYSFS_File *handle = (PHYSFS_File *)user;

    return PHYSFS_fixedEof(handle);
}

stbi_io_callbacks _physfsTexLoadCallbacks = {
        _physfsReadHandler,
        _physfsSkipHandler,
        _physfsEofHandler
};

int _texmgrFileLoad(const char *name, const char *filename, texEntry_t *entry) {
    PHYSFS_file *imgfile;
    char buf[32];

    if(filename == NULL) {
        errorf("Unable to load texture without a filename.");
        return 0;
    }

    mutexGrab(g_texmgrCtx.mutex);

    if(name != NULL) {
        debugf(DBG_TEXTURE, TEX_TRACE, "Loading registered texture \"%s\" from \"%s\"", name, filename);
    } else {
        debugf(DBG_TEXTURE, TEX_TRACE, "Loading anonymous texture from \"%s\"", filename);
    }
    debugf(DBG_TEXTURE, TEX_TRACE, "[%s][%s] %s(%d) %s(%d) %s 0x%2.2X 0x%2.2X 0x%2.2X",
           ((name != NULL) ? name : "<anon>"),
           entry->filename,
           utilEnum2Str(_texmgrGLMinFilters, entry->minfilter, "<unknown>"), entry->minfilter,
           utilEnum2Str(_texmgrGLMagFilters, entry->magfilter, "<unknown>"), entry->magfilter,
           ((entry->flags & TEX_FLAG_COLORKEY) ? "Colorkey" : "Opaque"),
           entry->red, entry->green, entry->blue,
           entry->minfilter, entry->magfilter);

    if((imgfile = PHYSFS_openRead(entry->filename))) {
        entry->data = stbi_load_from_callbacks(&_physfsTexLoadCallbacks, imgfile,
                                               &entry->texture.width, &entry->texture.height, &entry->components, 4);
        PHYSFS_close(imgfile);
        debugf(DBG_TEXTURE, TEX_TRACE, "Loaded data from image file \"%s\".", entry->filename);
        switch(entry->components) {
            case 4:
                entry->format = GL_RGBA;
                break;
            case 3:
                entry->format = GL_RGB;
                break;
            default:
                mutexRelease(g_texmgrCtx.mutex);
                errorf("Image \"%s\" is not truecolor.", entry->filename);
                util_free(entry->data);
                return 0;
        }
    } else {
        mutexRelease(g_texmgrCtx.mutex);
        if(name != NULL) {
            errorf("Failed to load texture \"%s\", unable to open file '%s'.", name, entry->filename);
        } else {
            errorf("Failed to load anonymous texture, unable to open file '%s'.", entry->filename);
        }
        return 0;
    }

    // Check that the image's width and height are a power of 2
    if((((entry->texture.width != 0) && (entry->texture.width & (entry->texture.width - 1))) != 0) &&
       (((entry->texture.height != 0) && (entry->texture.height & (entry->texture.height - 1))) != 0)) {
        errorf("The image (%dx%d) is not a power of 2 in size.", entry->texture.width, entry->texture.height);
        util_free(entry->data);
        return 0;
    }

    // Prepare the "colorkey" for the image
    if(entry->flags & TEX_FLAG_COLORKEY) {
        debugf(DBG_TEXTURE, TEX_TRACE, "Applying colorkey to alpha channel");
        _applyImageColorKey(entry);
    }

    entry->count++;

    if(!_loadTextureIntoOpenGL(entry)) {
        util_free(entry->data);
        entry->count--;
        mutexRelease(g_texmgrCtx.mutex);
        return 0;
    }

    mutexRelease(g_texmgrCtx.mutex);

    return 1;
}

texture_t *texmgrLoad(const char *filename, const char *name,
                            GLint minfilter, GLint magfilter, int colorkey,
                            int red, int green, int blue) {
    texEntry_t *entry;

    if((entry = _texmgrEntryCreate(name, filename, minfilter, magfilter,
                                   colorkey, red, green, blue)) == NULL) {
        return NULL;
    }

    if(_texmgrFileLoad(name, filename, entry)) {
        if(name == NULL) {
            _texmgrAnonStore(entry);
        }
        return (texture_t *)entry;
    }

    return NULL;
}

texture_t *texmgrCreate(const char *name, GLint minfilter,
                              GLint magfilter, int colorkey, int red, int green,
                              int blue, int components, GLenum format,
                              void *data, int width, int height) {
    texEntry_t *entry;

#if 0
    // Check that the image's width and height are a power of 2
    if((((width != 0) && (width & (width - 1))) != 0) &&
       (((height != 0) && (height & (height - 1))) != 0)) {
        errorf("The image (%dx%d) is not a power of 2 in size.", width, height);
        return NULL;
    }
#endif

    if((entry = _texmgrEntryCreate(name, NULL, minfilter, magfilter, colorkey, red, green, blue)) == NULL) {
        return NULL;
    }

    entry->components = components;
    entry->data = data;
    entry->format = format;
    entry->texture.width = width;
    entry->texture.height = height;

    mutexGrab(g_texmgrCtx.mutex);

    // Prepare the "colorkey" for the image
    if(entry->flags & TEX_FLAG_COLORKEY) {
        debugf(DBG_TEXTURE, TEX_TRACE, "Applying colorkey to alpha channel");
        _applyImageColorKey(entry);
    }

    entry->count++;

    if(!_loadTextureIntoOpenGL(entry)) {
        entry->count--;
        mutexRelease(g_texmgrCtx.mutex);
        return NULL;
    }

    if(name == NULL) {
        _texmgrAnonStore(entry);
    }
    mutexRelease(g_texmgrCtx.mutex);

    return &entry->texture;
}

texture_t *texmgrSubdivideTexture(const texture_t *texture,
                                        const char *name,
                                        int x1, int y1, int x2, int y2,
                                        GLint minfilter, GLint magfilter) {
    texEntry_t *orig;
    texture_t *entry;
    int pitch;
    unsigned char *data;
    int width;
    int height;
    unsigned char *ptr, *base, *src;
    int size;
    int x, y;

    debugf(DBG_TEXTURE, TEX_TRACE, "ENTER: texmgrSubdivideTexture(0x%p, %s, ...)", texture, ((name != NULL) ? name : "<anon>"));
    orig = (texEntry_t *)texture;
    pitch = texture->width * orig->components;

    width = x2 - x1 + 1;
    height = y2 - y1 + 1;
    size = width * height * orig->components;
    data = util_alloc(size, NULL);

    debugf(DBG_TEXTURE, TEX_TRACE, "Creating subtexture \"%s\" from (%d,%d)-(%d,%d) [%dx%d]",
           name, x1, y1, x2, y2, width, height);

    ptr = data;
    base = orig->data + (y1 * pitch) + (x1 * orig->components);
    for(y = 0; y < height; y++) {
        src = base;
        for(x = 0; x < width; x++) {
            switch(orig->components) {
                case 4:
                    *ptr = *src;
                    ptr++;
                    src++;
                case 3:
                    *ptr = *src;
                    ptr++;
                    src++;
                case 2:
                    *ptr = *src;
                    ptr++;
                    src++;
                case 1:
                default:
                    *ptr = *src;
                    ptr++;
                    src++;
            }
        }
        base += pitch;
    }

    entry = texmgrCreate(name, orig->minfilter, orig->magfilter,
                         orig->flags & TEX_FLAG_COLORKEY, orig->red, orig->green,
                         orig->blue, orig->components, orig->format,
                         data, width, height);
    if(entry == NULL) {
        util_free(data);
    }
    debugf(DBG_TEXTURE, TEX_TRACE, "EXIT: texmgrSubdivideTexture(0x%p, %s, ...)", texture, ((name != NULL) ? name : "<anon>"));
    return entry;
}

// Texture configuration string:
//     name|filename|min|mag|colorkey (#rgb|#rrggbb|r,g,b)
int texmgrRegStr(const char *str) {
    char data[512];
    char *fields[5];
    int count;
    int result = 0;
    int minfilter = 0;
    int magfilter = 0;
    int colorkey = 0;
    int red = 0;
    int green = 0;
    int blue = 0;
    
    strncpy(data, str, sizeof(data));
    count = strFields(data, '|', fields, 5);
    switch(count) {
        case 5:
            if(parseColor(fields[4], &red, &green, &blue, NULL)) {
                colorkey = 1;
            } else {
                errorf("Failed to parse the colorkey for texture \"%s\".", fields[0]);
            }
        case 4:
            if(parseIsNumber(fields[3])) {
                magfilter = atoi(fields[3]);
            } else {
                magfilter = utilStr2Enum(_texmgrGLMagFilters, fields[3], GL_LINEAR);
            }
        case 3:
            if(parseIsNumber(fields[2])) {
                minfilter = atoi(fields[2]);
            } else {
                minfilter = utilStr2Enum(_texmgrGLMinFilters, fields[2], GL_NEAREST_MIPMAP_LINEAR);
            }
        case 2:
            result = texmgrRegister(fields[0], fields[1], minfilter,
                                    magfilter, colorkey, red, green, blue);
            break;
        case 1:
            errorf("Failed add texture \"%s\", no filename specified.", fields[0]);
            break;
    }
    return result;
}

int texmgrRegFile(const char *filename) {
    char data[512];
    PHYSFS_file *handle;

    if((handle = PHYSFS_openRead(filename))) {
        while(PHYSFS_gets(data, sizeof(data) - 1, handle) != NULL) {
            trimStr(data);
            if((*data == '\0') || (*data == '#')) {
                continue;
            }
            texmgrRegStr(data);
        }
        PHYSFS_close(handle);
    } else {
        errorf("Unable to open the texture registry file '%s'.", filename);
        return 0;
    }

    return 1;
}

const texture_t *texmgrGet(const char *name) {
    texEntry_t *entry = NULL;

    mutexGrab(g_texmgrCtx.mutex);

    if(critbitContains(&g_texmgrCtx.registry, name, ((void *)&entry))) {
        if(entry->texture.id > 0) {
            entry->count++;
            mutexRelease(g_texmgrCtx.mutex);
            debugf(DBG_TEXTURE, TEX_TRACE, "Returning cached texture \"%s\".", name);
            return &entry->texture;
        } else {
            mutexRelease(g_texmgrCtx.mutex);
            debugf(DBG_TEXTURE, TEX_TRACE, "Loading texture \"%s\".", name);
            if(!_texmgrFileLoad(name, entry->filename, entry)) {
                return NULL;
            }
            return ((texture_t *)entry);
        }
    }
    mutexRelease(g_texmgrCtx.mutex);

    errorf("Unknown texture entry \"%s\".", name);

    return NULL;
}

int texmgrRelease(const texture_t *tex) {
    mutexGrab(g_texmgrCtx.mutex);
    ((texEntry_t *)tex)->count--;
    if(((texEntry_t *)tex)->count <= 0) {
        // TODO Free anon entries
        // TODO Free the texture registry entry
    }
    mutexRelease( g_texmgrCtx.mutex );
    return 1;
}

const char *texmgrGLMinFilterStr(GLint filter) {
    return utilEnum2Str(_texmgrGLMinFilters, filter, "<unknown>");
}

const char *texmgrGLMagFilterStr(GLint filter) {
    return utilEnum2Str(_texmgrGLMagFilters, filter, "<unknown>");
}

const char *texbgrGLFormatStr(GLenum format) {
    return utilEnum2Str(_texmgrGLFormats, format, "<unknown>");
}

GLint texmgrStrToGLMinFilter(const char *str) {
    return utilStr2Enum(_texmgrGLMinFilters, str, 0);
}

GLint texmgrStrToGLMagFilter(const char *str) {
    return utilStr2Enum(_texmgrGLMagFilters, str, 0);
}

GLenum textmsgStrToGLFormat(const char *str) {
    return utilStr2Enum(_texmgrGLFormats, str, 0);
}
