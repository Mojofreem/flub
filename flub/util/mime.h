#ifndef _FLUB_UTIL_MIME_HEADER_
#define _FLUB_UTIL_MIME_HEADER_


#include <stddef.h>


typedef struct mimeContext_s {
    int bits;
    int count;
    int pos;
    size_t size;
    int done;
    unsigned char *buffer;
} mimeContext_t;

void mimeInit(mimeContext_t *ctx, unsigned char *buf, size_t size);
int mimeIsDone(mimeContext_t *ctx);
int mimeLength(mimeContext_t *ctx);
int mimeChunk(mimeContext_t *ctx, const char *str);
int mimeChunkn(mimeContext_t *ctx, const char *str, int len);


#endif // _FLUB_UTIL_MIME_HEADER_
