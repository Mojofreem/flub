#include <flub/util/mime.h>
#include <flub/logger.h>
#include <ctype.h>


static int g_mimeInit = 0;
static char g_mimeDecTbl[256];


void mimeInit(mimeContext_t *ctx, unsigned char *buf, size_t size) {
    const char encTbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
            "0123456789+/";
    int k;

    ctx->buffer = buf;
    ctx->size = size;
    ctx->bits = 0;
    ctx->count = 0;
    ctx->pos = 0;
    ctx->done = 0;

    if(!g_mimeInit) {
        for(k = 0; k < 256; k++) {
            g_mimeDecTbl[k] = 0;
        }
        for(k = 0; k < 64; k++) {
            g_mimeDecTbl[encTbl[k]] = k;
        }
        g_mimeInit = 1;
    }
}

int mimeIsDone(mimeContext_t *ctx) {
    return ctx->done;
}

int mimeLength(mimeContext_t *ctx) {
    return ctx->pos;
}

int mimeChunk(mimeContext_t *ctx, const char *str) {
    unsigned char byte;

    if(str == NULL) {
        if(ctx->count) {
            errorf("Mime datastream incomplete.");
            return 0;
        }
        ctx->done = 1;
        return 1;
    }
    while(*str != '\0') {
        if(*str == '=') {
            // This is the end of the datastream. Any remaining bits are filler.
            ctx->count = 0;
            ctx->done = 1;
        } else if(!isspace(*str)) {
            byte = g_mimeDecTbl[*str];
            ctx->bits <<= 6;
            ctx->bits |= byte;
            ctx->count += 6;

            while(ctx->count >= 8) {
                if(ctx->pos < ctx->size) {
                    ctx->buffer[ctx->pos] = ((ctx->bits >> (ctx->count - 8)) & 0xFF);
                    ctx->pos++;
                } else {
                    errorf("Attempted to decode mime data past allocated buffer size.");
                    return 0;
                }
                ctx->count -= 8;
            }
        }
        str++;
    }
    return 1;
}

int mimeChunkn(mimeContext_t *ctx, const char *str, int len) {
    unsigned char byte;

    if(str == NULL) {
        if(ctx->count) {
            errorf("Mime datastream incomplete.");
            return 0;
        }
        ctx->done = 1;
        return 1;
    }
    while(len) {
        if(*str == '=') {
            // This is the end of the datastream. Any remaining bits are filler.
            ctx->count=0;
            ctx->done = 1;
        }
        else if((!isspace(*str)) && (*str != '=')) {
            byte = g_mimeDecTbl[*str];
            ctx->bits <<= 6;
            ctx->bits |= byte;
            ctx->count += 6;

            while(ctx->count >= 8) {
                if(ctx->pos < ctx->size) {
                    ctx->buffer[ctx->pos] = ((ctx->bits >> (ctx->count - 8)) & 0xFF);
                    ctx->pos++;
                } else {
                    errorf("Attempted to decode mime data past allocated buffer size.");
                    return 1;
                }
                ctx->count -= 8;
            }
        }
        len--;
        str++;
    }
    return 1;
}
