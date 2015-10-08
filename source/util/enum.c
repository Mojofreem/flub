#include <flub/util/enum.h>
#include <flub/logger.h>
#include <string.h>


const char *utilEnum2Str(const utilEnumMap_t *map, int value,
                         const char *undefined) {
    int k;

    for(k = 0; map[k].str != NULL; k++) {
        if(map[k].value == value) {
            return map[k].str;
        }
    }
    return undefined;
}

int utilStr2Enum(const utilEnumMap_t *map, const char *str, int undefined) {
    int k;

    if(str == NULL) {
        return undefined;
    }

    for(k = 0; map[k].str != NULL; k++) {
        if(!strcasecmp(map[k].str, str)) {
            return map[k].value;
        }
    }
    return undefined;
}

void utilEnumDump(int dbgModule, int dbgDetail, const char *name,
                  const utilEnumMap_t *map) {
    int k;

    debugf(dbgModule, dbgDetail, "Enum map [%s]", name);
    for(k = 0; map[k].str != NULL; k++) {
        debugf(dbgModule, dbgDetail, "...[%s] = %d", map[k].str, map[k].value);
    }
}
