#define _POSIX_C_SOURCE 200112
#define uint32_t uint32_t


#include <stdint.h> 
#include <string.h> 
#include <stdlib.h> 
#include <sys/types.h> 
#include <errno.h> 

#include <flub/data/critbit.h>


typedef struct {
    void *child[2];
    void *data[2];
    uint32_t byte;
    uint8_t otherbits;
} critbit_node;


#ifndef posix_memalign
// Note: the critbit implementation expects memory alignment on an even byte
//       alignment boundary. Most modern OSes should due this by default, due
//       to system architecture.
int posix_memalign(void **memptr, size_t alignment, size_t size) {
    if((*memptr = calloc(1, size)) == NULL) {
        return 1;
    }
    return 0;
}
#endif // posix_memalign


int critbitContains(critbit_t *t, const char *u, void **data) {
    const uint8_t *ubytes = (void*)u;
    const size_t ulen = strlen( u );
    uint8_t *p = t->root;
    
    if(data != NULL) {
        (*data) = t->data;
    }

    if(!p) {
        return 0;
    }

    while(1 & (intptr_t)p) {
        critbit_node *q = (void*)(p - 1);
        uint8_t c = 0;
        if(q->byte < ulen) {
            c = ubytes[q->byte];
        }
        const int direction = (1 + (q->otherbits | c)) >> 8;

        p = q->child[direction];
        if(data != NULL) {
            (*data) = q->data[direction];
        }
    }

    return (0 == strcmp(u, (const char*)p));
}

int critbitInsert(critbit_t *t, const char *u, void *data, void **old) {
    const uint8_t *const ubytes = (void*)u;
    const size_t ulen = strlen(u);
    uint8_t *p = t->root;
    void *d = t->data;

    if(old != NULL) {
        (*old) = NULL;
    }
    if(!p) {
        char *x;
        int a = posix_memalign((void **)&x, sizeof(void *), ulen + 1);
        if(a) {
            return 0;
        }
        memcpy(x, u, ulen + 1);
        t->root = x;
        t->data = data;
        return 2;
    }

    while(1 & (intptr_t)p) {
        critbit_node *q = (void*)(p - 1);
        uint8_t c = 0;
        if(q->byte < ulen) {
            c = ubytes[q->byte];
        }
        const int direction = (1 + (q->otherbits | c)) >> 8;
        p = q->child[direction];
        d = q->data[direction];
    }

    uint32_t newbyte;
    uint32_t newotherbits;

    for(newbyte = 0; newbyte < ulen; newbyte++) {
        if(p[newbyte] != ubytes[newbyte]) {
            newotherbits = p[newbyte] ^ ubytes[newbyte];
            goto different_byte_found;
        }
    }
    if(p[newbyte] != 0) {
        newotherbits = p[newbyte];
        goto different_byte_found;
    }
    if(old != NULL) {
        (*old) = d;
    }
    d = data;
    return 1;

different_byte_found:

    while(newotherbits & (newotherbits - 1)) {
        newotherbits&= newotherbits-1;
    }
    newotherbits ^= 255;
    uint8_t c = p[newbyte];
    int newdirection = (1 + (newotherbits | c)) >> 8;

    critbit_node *newnode;
    if(posix_memalign((void **)&newnode, sizeof(void *), sizeof(critbit_node))) {
        return 0;
    }

    char *x;
    if(posix_memalign((void **)&x, sizeof(void *), ulen + 1)) {
        free(newnode);
        return 0;
    }
    memcpy(x, ubytes, ulen + 1);

    newnode->byte = newbyte;
    newnode->otherbits = newotherbits;
    newnode->child[1 - newdirection] = x;
    newnode->data[1 - newdirection] = data;
    
    void **wherep = &t->root;
    void **datap = &t->data;
    for(;;) {
        uint8_t *p = *wherep;
        if(!(1 & (intptr_t)p)) {
            break;
        }
        critbit_node *q = (void*)(p - 1);
        if(q->byte > newbyte) {
            break;
        }
        if((q->byte == newbyte) && (q->otherbits > newotherbits)) {
            break;
        }
        uint8_t c = 0;
        if(q->byte < ulen) {
            c = ubytes[q->byte];
        }
        const int direction = (1 + (q->otherbits | c)) >> 8;
        wherep = q->child + direction;
        datap = q->data + direction;
    }
    newnode->child[newdirection] = *wherep;
    newnode->data[newdirection] = *datap;
    *wherep = (void*)(1 + (char*)newnode);
    *datap = NULL;

    return 2;
}

int critbitDelete(critbit_t *t, const char *u, int (*handle)(void *, void *), void *arg) {
    const uint8_t *ubytes = (void*)u;
    const size_t ulen = strlen(u);
    uint8_t *p = t->root;
    void **wherep = &t->root;
    void **whereq = 0;
    void **datap = &t->data;
    void **dataq = 0;
    critbit_node *q = 0;
    int direction = 0;

    if(!p) {
        return 0;
    }

    while(1 &(intptr_t)p) {
        whereq  = wherep;
        dataq = datap;
        q = (void*)(p - 1);
        uint8_t c = 0;
        if(q->byte < ulen) {
            c = ubytes[q->byte];
        }
        direction = (1 + (q->otherbits | c)) >> 8;
        wherep = q->child + direction;
        datap = q->data + direction;
        p = *wherep;
    }

    if(0 != strcmp(u, (const char*)p)) {
        return 0;
    }
    free(p);
    if(*datap != NULL) {
        handle(*datap, arg);
    }

    if(!whereq) {
        t->root = 0;
        t->data = 0;
        return 1;
    }

    *whereq = q->child[1 - direction];
    dataq = &(q->data[1 - direction]);
    free(q);
    if(*dataq != NULL) {
        handle(*datap, arg);
    }

    return 1;
}

static void traverse(void *top, void *data, int(*handle)(void *, void *), void *arg) {
    uint8_t *p = top;

    if(1 & (intptr_t)p) {
        critbit_node *q = (void*)(p - 1);
        traverse(q->child[0], q->data[0], handle, arg);
        traverse(q->child[1], q->data[0], handle, arg);
        free(q);
    } else {
        if(data != NULL) {
            handle(data, arg);
        }
        free(p);
    }
}

void critbitClear(critbit_t *t, int(*handle)(void *, void *), void *arg) {
    if(t->root) {
        traverse(t->root, t->data, handle, arg);
    }
    t->root = NULL;
    t->data = NULL;
}

static int allprefixed_traverse(uint8_t *top, void *data,
                                int(*handle)(const char *, void *, void *),
                                void *arg) {
    int direction;
    
    if(1 & (intptr_t)top) {
        critbit_node *q = (void *)(top - 1);
        for(direction = 0; direction < 2; direction++) {
            switch(allprefixed_traverse(q->child[direction],
                                        q->data[direction],
                                        handle, arg)) {
                case 1:
                    break;
                case 0:
                    return 0;
                default:
                    return -1;
            }
        }
        return 1;
    }

    return handle((const char *)top, data, arg);
}

int critbitAllPrefixed(critbit_t *t, const char *prefix,
                       int(*handle)(const char *, void *, void *),
                       void *arg) {
    const uint8_t *ubytes = (void*)prefix;
    const size_t ulen = strlen(prefix);
    uint8_t *p = t->root;
    uint8_t *top = p;
    void *data = t->data;
    size_t i;

    if(!p) {
        return 1;
    }

    while(1 & (intptr_t)p) {
        critbit_node *q = (void*)(p - 1);
        uint8_t c = 0;
        if(q->byte < ulen)
        {
            c = ubytes[q->byte];
        }
        const int direction = (1 + (q->otherbits | c)) >> 8;
        p = q->child[direction];
        if(q->byte < ulen) {
            top = p;
            data = q->data[direction];
        }
    }

    for(i = 0; i < ulen; i++) {
        if(p[i] != ubytes[i])
        {
            return 1;
        }
    }

    return allprefixed_traverse(top, data, handle, arg);
}

