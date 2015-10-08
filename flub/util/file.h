#ifndef _FLUB_UTIL_FILE_HEADER_
#define _FLUB_UTIL_FILE_HEADER_


#include <stddef.h>


int utilFexists(const char *fname);

char *utilGenFilename(const char *prefix, const char *suffix,
                      int max_attempts, char *buffer, size_t len);

typedef int (*utilEnumDirCB_t)(const char *path, void *context);

void utilEnumDir(const char *path, utilEnumDirCB_t *callback, void *context);


#endif // _FLUB_UTIL_FILE_HEADER_
