#include <flub/memory.h>
#include <flub/logger.h>
#include <flub/data/circbuf.h>
#include <stdio.h>
#include <stdlib.h>


//#define CIRC_BUF_DEBUG

// If defined, we create overrun markers to check whether we've written
// beyond the immediate write areas within the circular buffer.
//#define CIRC_BUF_OVERRUN

#define CIRC_BUF_END_OF_BUF             -1

#ifdef CIRC_BUF_DEBUG
//#   define DEBUG(fmt,...)      printf(fmt, ## __VA_ARGS__); fflush(stdout)
#   define DEBUG(fmt,...)       infof(fmt, ## __VA_ARGS__)
#else // CIRC_BUF_DEBUG
#   define DEBUG(fmt,...)
#endif // CIRC_BUF_DEBUG

#define CIRC_BUF_OVERRUN_MARKER     0xDEADC0DE

#ifdef CIRC_BUF_OVERRUN
#   define CIRC_BUF_OVERRUN_PAD    sizeof(unsigned int)
#else // CIRC_BUF_OVERRUN
#   define CIRC_BUF_OVERRUN_PAD    0
#endif // CIRC_BUF_OVERRUN


typedef struct circBuf_s
{
    int head;
    int tail;
    int count;
    int idx_head;
    int overrun;
    int size;
    int max;
    int *index;
    unsigned char buffer[0];
} circBuf_t;


#ifdef CIRC_BUF_OVERRUN
static void _circBufSetOverrunMarkers(circBuf_t *buf) {
    unsigned int *ptr;

    ptr = (unsigned int *)(buf->buffer + buf->size);
    *ptr = CIRC_BUF_OVERRUN_MARKER;
    if(buf->index != NULL) {
        ptr = (unsigned int *)(buf->index + (sizeof(int) * buf->max));
        *ptr = CIRC_BUF_OVERRUN_MARKER;
    }
}

static int _circBufCheckOverrunMarkers(circBuf_t *buf) {
    unsigned int marker;
    unsigned int markerB;
    unsigned int *ptr;

    ptr = (unsigned int *)(buf->buffer + buf->size);
    if(*ptr != CIRC_BUF_OVERRUN_MARKER) {
        DEBUG("CB: OVERRUN on buffer!\n");
        buf->overrun |= 1;
    }
    if(buf->index != NULL) {
        ptr = (unsigned int *)(buf->index + (sizeof(int) * buf->max));
        if(*ptr != CIRC_BUF_OVERRUN_MARKER) {
            DEBUG("CB: OVERRUN on index! 0x%8.8X\n", *ptr);
            buf->overrun |= 2;
        }
    }
    return buf->overrun;
}
#else // CIRC_BUF_OVERRUN
#define _circBufSetOverrunMarkers(buf)
#define _circBufCheckOverrunMarkers(buf)    0
#endif // CIRC_BUF_OVERRUN


circularBuffer_t *circBufInit(int size, int maxCount, int useIndex) {
    circBuf_t *buf;
    int len;

    if(maxCount <= 0) {
        return NULL;
    }

    len = sizeof(circBuf_t) + size;
    len += CIRC_BUF_OVERRUN_PAD;
    if(useIndex) {
        len += sizeof(int) * maxCount;
        len += CIRC_BUF_OVERRUN_PAD;
    }

    buf = util_calloc(len, 0, NULL);
    buf->max = maxCount;
    buf->size = size;
    if(useIndex) {
        buf->index = (int *)(buf->buffer + size + CIRC_BUF_OVERRUN_PAD);
    }

    _circBufSetOverrunMarkers(buf);

    DEBUG("CB: Init 0x%p  struct:%d  size:%d  max:%d  idx:%s  alloc:%d\n",
          buf, sizeof(circBuf_t), buf->size, buf->max,
          ((useIndex)?"YES":"NO"), len);

    return (circularBuffer_t *)buf;
}

int circBufOverrun(circularBuffer_t *buf) {
    circBuf_t *ptr = (circBuf_t *)buf;

    return ptr->overrun;
}

void circBufClear(circularBuffer_t *buf) {
    circBuf_t *ptr = (circBuf_t *)buf;

    ptr->head = 0;
    ptr->tail = 0;
    ptr->count = 0;
    ptr->overrun = 0;
    ptr->idx_head = 0;

    DEBUG("CB: Clear\n");
}

static void _circBufHeadPop(circBuf_t *ptr) {
    int len;
    unsigned int *check;

    if(ptr->count) {
#ifdef CIRC_BUF_OVERRUN
        check = (unsigned int *)(ptr->buffer + ptr->head);
        if((check[0] != CIRC_BUF_OVERRUN_MARKER) ||
           (check[2] != CIRC_BUF_OVERRUN_MARKER)) {
            ptr->overrun |= 4;
            return;
        }
        if(check[1] == CIRC_BUF_END_OF_BUF) {
            ptr->head = 0;
            // This doesn't actually pop an entry, it simply unwraps an end
            // of buffer marker. Don't update the count.
            return;
        } else {
            ptr->head = ptr->head + check[1];
        }
#else // CIRC_BUF_OVERRUN
        check = (unsigned int *)(ptr->buffer + ptr->head);
        if(*check == CIRC_BUF_END_OF_BUF) {
            ptr->head = 0;
            // This doesn't actually pop an entry, it simply unwraps an end
            // of buffer marker. Don't update the count.
            return;
        } else {
            ptr->head = ptr->head + *check;
        }
#endif // CIRC_BUF_OVERRUN
        ptr->count--;
        if(ptr->count == 0) {
            circBufClear((circularBuffer_t *)ptr);
        } else {
            ptr->idx_head++;
            if(ptr->idx_head >= ptr->max) {
                ptr->idx_head = 0;
            }
        }
    }
}

void *circBufAllocNextEntryPtr(circularBuffer_t *buf, int size) {
    circBuf_t *ptr = (circBuf_t *)buf;
    int newsize;
    int purge = 0;
    int *check;

    int len;
    int pos;
    int idx;
    int wrap;
    static int run = 0;

    // Is there room for the entry, the head location, and the tail location?
    if(size >= (ptr->size + sizeof(int) + sizeof(int) +
                CIRC_BUF_OVERRUN_PAD + CIRC_BUF_OVERRUN_PAD +
                CIRC_BUF_OVERRUN_PAD + CIRC_BUF_OVERRUN_PAD)) {
        return NULL;
    }

    // Are we at capacity?
    while(ptr->count == ptr->max) {
        _circBufHeadPop(ptr);
    }

    DEBUG("CB: Entry - head:%d  tail:%d  count:%d\n", ptr->head, ptr->tail, ptr->count);

    newsize = size + sizeof(int) + sizeof(int) + CIRC_BUF_OVERRUN_PAD +
              CIRC_BUF_OVERRUN_PAD + CIRC_BUF_OVERRUN_PAD + CIRC_BUF_OVERRUN_PAD;

    // Will the new entry overrun the end of the buffer?
    if((ptr->tail + newsize) >= ptr->size) {
        //DEBUG("CB:         wraparound\n");
        // Is the head entry between us and the end of the buffer?
        if(ptr->head > ptr->tail) {
            //DEBUG("CB:         head is in the way\n");
            // Remove all entries between here and the end of the buffer
            while(ptr->head != 0) {
                _circBufHeadPop(ptr);
                purge++;
            }
        }
        if(ptr->head == 0) {
            // Remove the head front entry, to make room for the tail
            _circBufHeadPop(ptr);
        }
        // Move the tail position to the start of the buffer
        if(ptr->count) {
            check = (int *)(ptr->buffer + ptr->tail);
#ifdef CIRC_BUF_OVERRUN
            check[0] = CIRC_BUF_OVERRUN_MARKER;
            check[1] = CIRC_BUF_END_OF_BUF;
            check[2] = CIRC_BUF_OVERRUN_MARKER;
#else // CIRC_BUF_OVERRUN
            *check = CIRC_BUF_END_OF_BUF;
#endif // CIRC_BUF_OVERRUN
        }
        ptr->tail = 0;
    }
    //DEBUG("CB:         head:%d  tail:%d\n", ptr->head, ptr->tail);
    if(ptr->tail < ptr->head) {
        //DEBUG("CB: Moving head...\n");
        wrap = ptr->head;
        // (ptr->tail != ptr->head)
        while((ptr->head >= wrap) && ((ptr->tail + newsize) >= ptr->head)) {
            _circBufHeadPop(ptr);
            if(ptr->head != 0) {
                purge++;
            }
            //DEBUG("CB:  MOVED  head:%d  tail:%d  count:%d\n", ptr->head, ptr->tail, ptr->count);
        }
    }
    if(ptr->index != NULL) {
        idx = ptr->idx_head + ptr->count;
        if(idx >= ptr->max) {
            idx -= ptr->max;
        }
        ptr->index[idx] = ptr->tail;
    }
    ptr->count++;
    check = (int *)(ptr->buffer + ptr->tail);
#ifdef CIRC_BUF_OVERRUN
    check[0] = CIRC_BUF_OVERRUN_MARKER;
    check[1] = size + sizeof(int) + CIRC_BUF_OVERRUN_PAD + CIRC_BUF_OVERRUN_PAD;
    check[2] = CIRC_BUF_OVERRUN_MARKER;
#else // CIRC_BUF_OVERRUN
    *check = size + sizeof(int) + CIRC_BUF_OVERRUN_PAD + CIRC_BUF_OVERRUN_PAD;
#endif // CIRC_BUF_OVERRUN
    pos = ptr->tail;
    ptr->tail += size + sizeof(int) + CIRC_BUF_OVERRUN_PAD + CIRC_BUF_OVERRUN_PAD;
    //DEBUG("CB:         head:%d  tail:%d  size:%d  count:%d\n", ptr->head, ptr->tail, size, ptr->count);
    if(ptr->head > ptr->size) {
        //DEBUG("CB: CRAP!\n");
        exit(1);
    }
    _circBufCheckOverrunMarkers(ptr);

    //DEBUG("CB: Purged %d to %d on run %d\n", purge, ptr->count, run);
    run++;

    return (ptr->buffer + pos + sizeof(int) + CIRC_BUF_OVERRUN_PAD + CIRC_BUF_OVERRUN_PAD);
}

static int _circBufGetEntryPos(circBuf_t *ptr, int entry) {
    int len;
    int pos;
    int idx;
    int *check;

    if(entry >= ptr->count) {
        return -1;
    }

    DEBUG("CB: Looking for entry %d\n", entry);
    if(ptr->index != NULL) {
        DEBUG("CB:     Using index...\n");
        idx = ptr->idx_head + entry;
        while(idx >= ptr->max) {
            idx -= ptr->max;
        }
        DEBUG("CB:     Position %d\n", ptr->index[idx]);
        return ptr->index[idx];
    } else {
        DEBUG("CB:     Walking the buffer...\n");
        pos = ptr->head;
        while(entry) {
            DEBUG("CB:     Skip %d\n", entry);
            check = (int *)(ptr->buffer + pos);
#ifdef CIRC_BUF_OVERRUN
            if(check[1] == CIRC_BUF_END_OF_BUF) {
#else // CIRC_BUF_OVERRUN
            if(*check == CIRC_BUF_END_OF_BUF) {
#endif // CIRC_BUF_OVERRUN
                pos = 0;
                check = (int *)(ptr->buffer + pos);
#ifdef CIRC_BUF_OVERRUN
                len = check[1];
#else // CIRC_BUF_OVERRUN
                len = *check;
#endif // CIRC_BUF_OVERRUN
            } else {
#ifdef CIRC_BUF_OVERRUN
                len = check[1];
#else // CIRC_BUF_OVERRUN
                len = *check;
#endif // CIRC_BUF_OVERRUN
            }
            pos += len;
            entry--;
        }
        DEBUG("CB:     Position %d\n", pos);
        return pos;
    }
}

int circBufEnumEntries(circularBuffer_t *buf, circBufEntryEnumCB_t callback,
                       int begin, int end) {
    circBuf_t *ptr = (circBuf_t *)buf;
    int size;
    int len;
    int *check;
    int pos;
    void *data;

    if(begin >= ptr->count) {
        return 0;
    }

    if(end >= ptr->count) {
        end = ptr->count -1;
    }

    if((pos = _circBufGetEntryPos(ptr, begin)) < 0) {
        return 0;
    }
    while(begin <= end) {
        check = (int *)(ptr->buffer + pos);


#ifdef CIRC_BUF_OVERRUN
        if((check[0] != CIRC_BUF_OVERRUN_MARKER) ||
           (check[2] != CIRC_BUF_OVERRUN_MARKER)) {
            ptr->overrun |= 4;
            return -1;
        }
        if(check[1] == CIRC_BUF_END_OF_BUF) {
            pos = 0;
            check = (int *)(ptr->buffer + pos);
            if((check[0] != CIRC_BUF_OVERRUN_MARKER) ||
               (check[2] != CIRC_BUF_OVERRUN_MARKER)) {
                ptr->overrun |= 4;
                return -1;
            }
        }
        len = check[1];
#else // CIRC_BUF_OVERRUN
        if(*check == CIRC_BUF_END_OF_BUF) {
            pos = 0;
            check = (int *)(ptr->buffer + pos);
        }
        len = *check;
#endif // CIRC_BUF_OVERRUN
        size = len - sizeof(int) - CIRC_BUF_OVERRUN_PAD - CIRC_BUF_OVERRUN_PAD;
        data = (void *)(ptr->buffer + pos + sizeof(int) + CIRC_BUF_OVERRUN_PAD + CIRC_BUF_OVERRUN_PAD);
        if(!callback(begin, data, size)) {
            return 0;
        }
        pos += len;
        begin++;
    }
    return 1;
}

int circBufGetCount(circularBuffer_t *buf) {
    return ((circBuf_t *)buf)->count;
}

int circBufGetEntry(circularBuffer_t *buf, int entry, void **bufptr, int *size) {
    circBuf_t *ptr = (circBuf_t *)buf;
    int len;
    int pos;
    int *check;

    if((pos = _circBufGetEntryPos(ptr, entry)) < 0) {
        if(bufptr != NULL) {
            *bufptr = (void *)NULL;
        }
        if(size != NULL) {
            *size = 0;
        }
        return 0;
    }

    if(bufptr != NULL) {
        *bufptr = (void *)(ptr->buffer + pos + sizeof(int) + CIRC_BUF_OVERRUN_PAD + CIRC_BUF_OVERRUN_PAD);
    }
    if(size != NULL) {
        check = (int *)(ptr->buffer + pos);
        *size = *check;
        *size -= sizeof(int) + CIRC_BUF_OVERRUN_PAD + CIRC_BUF_OVERRUN_PAD;
    }
    return 1;
}
