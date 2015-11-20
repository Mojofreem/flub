#ifndef _FLUB_MEMORY_HEADER_
#define _FLUB_MEMORY_HEADER_


#include <stddef.h>


int memInit(void);
void memShutdown(void);


void util_outofmemory( void ) __attribute__((noreturn));

void *util_alloc( size_t size, void *src );
void *util_calloc( size_t size, size_t old, void *src );
char *util_strdup( const char *str );
char *util_strndup( const char *str, int len );
void util_free(void *ptr);


typedef struct memPoolEntry_s memPoolEntry_t;
struct memPoolEntry_s {
    memPoolEntry_t *next;
    unsigned char ptr[0];
};

typedef struct memPool_s memPool_t;
struct memPool_s {
    size_t size;
    int used;
    memPoolEntry_t *entries;
    memPoolEntry_t *available;
};

memPool_t *memPoolCreate(size_t size, int preAllocate);
void *memPoolAlloc(memPool_t *pool);
void memPoolFree(memPool_t *pool, void *ptr);


int memFrameStackInit(size_t size);
int memFrameStackBegin(void);
void memFrameStackEnd(int id);
void *memFrameStackAlloc(size_t size);
void memFrameStackStats(void);
size_t memFrameStackSize(void);
void memFrameStackReset(void);


#endif // _FLUB_MEMORY_HEADER_
