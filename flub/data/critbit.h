#ifndef _FLUB_CRITBIT_HEADER_
#define _FLUB_CRITBIT_HEADER_


typedef struct
{
    void *root;
    void *data;
} critbit_t;


#define CRITBIT_TREE_INIT   { NULL, NULL }


int critbitContains(critbit_t *t, const char *u, void **data);
int critbitInsert(critbit_t *t, const char *u, void *data, void **old);
int critbitDelete(critbit_t *t, const char *u,
                  int (*handle)(void *, void *), void *arg);
void critbitClear(critbit_t *t, int (*handle)(void *, void *), void *arg);
int critbitAllPrefixed(critbit_t *t, const char *prefix,
                       int (*handle)(const char *, void *, void *), void *arg);


#endif  // _FLUB_CRITBIT_HEADER_

