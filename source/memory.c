#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#define _GNU_SOURCE
#include <string.h>
#include <flub/logger.h>
#include <flub/memory.h>
#include <stddef.h>

#ifdef WIN32
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#endif


#define DBG_MEM_DTL_FRAME       1


#define MEM_FRAME_MARKER_COUNT              20
#define MEM_FRAME_STACK_INCREASE_PADDING    8192


typedef struct _memFrameDangle_s {
    struct _memFrameDangle_s *next;
    unsigned char buf[0];
} _memFrameDangle_t;

typedef struct _memFrameMarker_s {
    int id;
    size_t available;
    void *ptr;
} _memFrameMarker_t;

typedef struct _memFrameGrowthMarker_s {
    int increase;
    struct _memFrameGrowthMarker_s *next;
} _memFrameGrowthMarker_t;

typedef struct _memFrameCtx_s {
    int init;
    void *ptr;
    void *pos;
    int notFirstFrame;
    struct {
        size_t size;
        size_t low;
        size_t high;
        size_t frameHigh;
        size_t average;
        size_t used;
        size_t available;
        size_t shortfall;
        int shortCount;
        int grows;
    } mem;
    struct {
        size_t low;
        size_t high;
        size_t average;
        int count;
    } entries;
    int count;

    _memFrameDangle_t *danglers;
    size_t overrun;
    _memFrameGrowthMarker_t *growList;
    _memFrameGrowthMarker_t *growLast;

    _memFrameMarker_t markers[MEM_FRAME_MARKER_COUNT];
    int markNumber;

    int id;
} _memFrameCtx_t;


struct {
    _memFrameCtx_t frameCtx;
} _memCtx = {
        .frameCtx = {
                .mem = {
                        .low = 999999,
                },
                .entries = {
                        .low = 999999,
                },
                .id = 1337,
        },
};

int memInit(appDefaults_t *defaults) {
    logDebugRegister("profile", DBG_PROFILE, "frame", DBG_MEM_DTL_FRAME);
    if(!memFrameStackInit(defaults->frameStackSize)) {
        return 0;
    }
    return 1;
}

void memShutdown(void) {
    memFrameStackStats();
}

flubModuleCfg_t flubModuleMemory = {
    .name = "memory",
    .init = memInit,
    .start = NULL,
    .shutdown = memShutdown,
    .initDeps = NULL,
    .startDeps = NULL,
};


#ifndef strndup
#ifndef MACOSX
static char *strndup(const char *s, size_t n) {
    char *ptr;
    int len = strlen(s);

    if(len > n) {
        len = n;
    }

    if((ptr = ((char *)malloc(len + 1))) == NULL) {
        return NULL;
    }
    memcpy(ptr, s, len);
    ptr[len] = '\0';
    return ptr;
}
#endif // MACOSX
#endif // strndup


void util_outofmemory(void) {
    // Maybe we should do some better error reporting here?
    // Make sure that whatever we do won't get stuck in the logger.
#ifdef WIN32
    MessageBox(NULL, "Out of memory - aborting", "Fatal Error",
               MB_ICONEXCLAMATION|MB_OK);
#endif
    error("Out of memory - aborting");
    exit( -1 );
}

void *util_alloc(size_t size, void *src) {
    if(src == NULL) {
        if((src = malloc(size)) == NULL) {
            util_outofmemory();
        }
        return src;
    } else {
        if((src = realloc(src, size)) == NULL) {
            util_outofmemory();
        }
        return src;
    }
}

void *util_calloc(size_t size, size_t old, void *src) {
    if(src == NULL) {
        if((src = calloc(1, size)) == NULL) {
            util_outofmemory();
        }
        return src;
    } else {
        if((src = realloc(src, size)) == NULL) {
            util_outofmemory();
        }
        memset(((char *)src) + old, 0, size - old);
        return src;
    }
}

char *util_strdup(const char *str) {
    char *ptr;

    if((ptr = strdup(str)) == NULL) {
        util_outofmemory();
    }
    return ptr;
}

char *util_strndup(const char *str, int len) {
    char *ptr;

    if((ptr = strndup(str, len)) == NULL) {
        util_outofmemory();
    }
    return ptr;
}

// This is a simple wrapper around free. We can implement memory tracking
// and management later, and this provides for hooks in place already within
// the library.
void util_free(void *ptr) {
    free(ptr);
}

memPool_t *memPoolCreate(size_t size, int preAllocate) {
    memPool_t *pool;
    memPoolEntry_t *entry;
    int k;
    int len;
    unsigned char *ptr;

    pool = util_calloc(sizeof(memPool_t), 0, NULL);
    pool->size = size;

    if(preAllocate) {
        for(k = 0; k < preAllocate; k++) {
            entry = util_calloc(sizeof(memPoolEntry_t) + size, 0, NULL);
            entry->next = pool->available;
            pool->available = entry;
        }
    }

    return pool;
}

void *memPoolAlloc(memPool_t *pool) {
    memPoolEntry_t *entry;

    if(pool->available != NULL) {
        entry = pool->available;
        pool->available = entry->next;
    } else {
        entry = util_calloc(sizeof(memPoolEntry_t) + pool->size, 0, NULL);
    }
    entry->next = pool->entries;
    pool->entries = entry;

    pool->used++;

    return entry->ptr;
}

void memPoolFree(memPool_t *pool, void *ptr) {
    // Note: Attempting to free a pool entry not obtained via pool alloc
    // is a very bad thing.
    memPoolEntry_t *entry;

    entry = (memPoolEntry_t *)(((unsigned char *)ptr) - offsetof(memPoolEntry_t, ptr));
    entry->next = pool->available;
    pool->available = entry;
    pool->used--;
}

void memFrameStackStats(void) {
    _memFrameGrowthMarker_t *walk;
    int k;

    debug(DBG_PROFILE, DBG_MEM_DTL_FRAME, "Memory frame stack usage details:");
    debugf(DBG_PROFILE, DBG_MEM_DTL_FRAME, "--- Size          : %d", _memCtx.frameCtx.mem.size);
    debugf(DBG_PROFILE, DBG_MEM_DTL_FRAME, "--- Low usage     : %d", _memCtx.frameCtx.mem.low);
    debugf(DBG_PROFILE, DBG_MEM_DTL_FRAME, "--- Average usage : %d", _memCtx.frameCtx.mem.average);
    debugf(DBG_PROFILE, DBG_MEM_DTL_FRAME, "--- High usage    : %d", _memCtx.frameCtx.mem.high);
    debugf(DBG_PROFILE, DBG_MEM_DTL_FRAME, "--- Low allocs    : %d", _memCtx.frameCtx.entries.low);
    debugf(DBG_PROFILE, DBG_MEM_DTL_FRAME, "--- Average allocs: %d", _memCtx.frameCtx.entries.average);
    debugf(DBG_PROFILE, DBG_MEM_DTL_FRAME, "--- High allocs   : %d", _memCtx.frameCtx.entries.high);
    debugf(DBG_PROFILE, DBG_MEM_DTL_FRAME, "--- Shortfall     : %d", _memCtx.frameCtx.mem.shortfall);
    debugf(DBG_PROFILE, DBG_MEM_DTL_FRAME, "--- Short count   : %d", _memCtx.frameCtx.mem.shortCount);
    debugf(DBG_PROFILE, DBG_MEM_DTL_FRAME, "--- Grows         : %d", _memCtx.frameCtx.mem.grows);
    for(walk = _memCtx.frameCtx.growList, k = 1; walk != NULL; walk = walk->next, k++) {
        debugf(DBG_PROFILE, DBG_MEM_DTL_FRAME, "---     #%2d: %d", k, walk->increase);
    }
}


int memFrameStackInit(size_t size) {
    if(_memCtx.frameCtx.init) {
        return 0;
    }

    if(size <= 0) {
        size = MEM_FRAME_STACK_INCREASE_PADDING;
    }

    _memCtx.frameCtx.ptr = util_alloc(size, NULL);
    _memCtx.frameCtx.mem.size = size;
    _memCtx.frameCtx.mem.available = size;

    _memCtx.frameCtx.init = 1;

    return 1;
}

int memFrameStackBegin(void) {
    int id;

    if(_memCtx.frameCtx.markNumber >= MEM_FRAME_MARKER_COUNT) {
        return -1;
    }

    id = _memCtx.frameCtx.id;
    _memCtx.frameCtx.id++;
    if(_memCtx.frameCtx.id < 0) {
        _memCtx.frameCtx.id = 1337;
    }

    return id;
}

void memFrameStackEnd(int id) {
    int k;

    if(_memCtx.frameCtx.markNumber == 0) {
        // There are no markers set
        return;
    }

    for(k = _memCtx.frameCtx.markNumber - 1; k >= 0; k--) {
        if(_memCtx.frameCtx.markers[k].id == id) {
            // Rewind to the marked position
            _memCtx.frameCtx.mem.available = _memCtx.frameCtx.markers[k].available;
            _memCtx.frameCtx.pos = _memCtx.frameCtx.markers[k].ptr;
            _memCtx.frameCtx.markNumber = k;
            return;
        }
    }
    // TODO - note marker fall through
}

void *memFrameStackAlloc(size_t size) {
    _memFrameDangle_t *dangle;
    void *ptr;
    // TODO - investigate padding to memory boundary

    if(size < _memCtx.frameCtx.mem.available) {
        ptr = _memCtx.frameCtx.pos;
        _memCtx.frameCtx.pos = (void *)(((unsigned char *)(_memCtx.frameCtx.pos)) + size);
        _memCtx.frameCtx.mem.available -= size;
    } else {
        dangle = util_alloc(sizeof(_memFrameDangle_t) + size, NULL);
        ptr = (void *)(dangle->buf);
        _memCtx.frameCtx.overrun += size;
        _memCtx.frameCtx.mem.shortCount++;
        _memCtx.frameCtx.mem.shortfall += size;
    }

    _memCtx.frameCtx.mem.used += size;
    _memCtx.frameCtx.entries.count++;

    return ptr;
}

size_t memFrameStackSize(void) {
    return _memCtx.frameCtx.mem.size;
}

void memFrameStackReset(void) {
    _memFrameDangle_t *walk, *next;
    _memFrameGrowthMarker_t *grow;
    // Check for overrun
    if(_memCtx.frameCtx.overrun) {
        // We exceeded our capacity this frame, so let's get bigger
        grow = util_alloc(sizeof(_memFrameGrowthMarker_t), NULL);
        grow->next = NULL;
        grow->increase = _memCtx.frameCtx.overrun + MEM_FRAME_STACK_INCREASE_PADDING;
        if(_memCtx.frameCtx.growLast == NULL) {
            _memCtx.frameCtx.growLast = grow;
            _memCtx.frameCtx.growList = grow;
        } else {
            _memCtx.frameCtx.growLast->next = grow;
            _memCtx.frameCtx.growLast = grow;
        }
        _memCtx.frameCtx.mem.size += _memCtx.frameCtx.overrun + MEM_FRAME_STACK_INCREASE_PADDING;
        _memCtx.frameCtx.ptr = util_alloc(_memCtx.frameCtx.mem.size, _memCtx.frameCtx.ptr);

        for(walk = _memCtx.frameCtx.danglers, next = NULL; walk != NULL; walk = next) {
            next = walk->next;
            util_free(walk);
        }

        _memCtx.frameCtx.danglers = NULL;
        _memCtx.frameCtx.overrun = 0;
        _memCtx.frameCtx.mem.grows++;
    }
    _memCtx.frameCtx.pos = _memCtx.frameCtx.ptr;
    _memCtx.frameCtx.mem.average += _memCtx.frameCtx.mem.frameHigh;
    if(_memCtx.frameCtx.notFirstFrame) {
        _memCtx.frameCtx.mem.average /= 2;
    }
    if(_memCtx.frameCtx.mem.low > _memCtx.frameCtx.mem.used) {
        _memCtx.frameCtx.mem.low = _memCtx.frameCtx.mem.used;
    }
    if(_memCtx.frameCtx.mem.high < _memCtx.frameCtx.mem.used) {
        _memCtx.frameCtx.mem.high = _memCtx.frameCtx.mem.used;
    }
    _memCtx.frameCtx.mem.used = 0;
    _memCtx.frameCtx.mem.available = _memCtx.frameCtx.mem.size;
    _memCtx.frameCtx.mem.frameHigh = 0;

    _memCtx.frameCtx.entries.average += _memCtx.frameCtx.entries.count;
    if(_memCtx.frameCtx.notFirstFrame) {
        _memCtx.frameCtx.entries.average /= 2;
    }
    if(_memCtx.frameCtx.entries.low > _memCtx.frameCtx.entries.count) {
        _memCtx.frameCtx.entries.low = _memCtx.frameCtx.entries.count;
    }
    if(_memCtx.frameCtx.entries.high < _memCtx.frameCtx.entries.count) {
        _memCtx.frameCtx.entries.high = _memCtx.frameCtx.entries.count;
    }
    _memCtx.frameCtx.entries.count = 0;
    _memCtx.frameCtx.notFirstFrame = 1;
    _memCtx.frameCtx.markNumber = 0;
    _memCtx.frameCtx.pos = _memCtx.frameCtx.ptr;
}

