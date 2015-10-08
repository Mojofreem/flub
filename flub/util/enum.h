#ifndef _FLUB_UTIL_ENUM_HEADER_
#define _FLUB_UTIL_ENUM_HEADER_


#include <flub/util/misc.h>


typedef struct utilEnumMap_s {
    int value;
    const char *str;
} utilEnumMap_t;

#define ENUM_MAP(x)   	{x, STRINGIFY(x)}
#define END_ENUM_MAP()  {0, NULL}

const char *utilEnum2Str(const utilEnumMap_t *map, int value,
                         const char *undefined);
int utilStr2Enum(const utilEnumMap_t *map, const char *str, int undefined);
void utilEnumDump(int dbgModule, int dbgDetail, const char *name,
                  const utilEnumMap_t *map);


#endif // _FLUB_UTIL_ENUM_HEADER_
