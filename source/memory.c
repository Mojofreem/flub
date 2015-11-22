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
