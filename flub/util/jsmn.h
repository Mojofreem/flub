#ifndef _FLUB_UTIL_JSMN_HEADER_
#define _FLUB_UTIL_JSMN_HEADER_


#include <flub/3rdparty/jsmn/jsmn.h>


char *jsmnStringDecode(const char *json, jsmntok_t *token, char *buf, int buflen);
jsmntok_t *jsmnFindKey(const char *json, jsmntok_t *tokens, const char *name);

typedef struct jsmnWalkState_s {
    jsmntok_t *tokens;
    int count;
    int pos;
    int done;
} jsmnWalkState_t;

int jsmnWalkInit(jsmnWalkState_t *state, jsmntok_t *tokens);
int jsmnWalk(jsmnWalkState_t *state, jsmntok_t **key, jsmntok_t **val);

int jsmnIsBool(const char *json, jsmntok_t *token);
int jsmnIsNull(const char *json, jsmntok_t *token);
int jsmnIsNumber(const char *json, jsmntok_t *token);
int jsmnIsString(jsmntok_t *token);
int jsmnIsObject(jsmntok_t *token);
int jsmnIsArray(jsmntok_t *token);

const char *jsmnTokenType(const char *json, jsmntok_t *token);

char *jsmnStringVal(const char *json, jsmntok_t *token, char *buf, int buflen,
                    const char *defValue);
int jsmnBoolVal(const char *json, jsmntok_t *token, int defValue);
int jsmnNumberVal(const char *json, jsmntok_t *token, int defValue);
float jsmnNumberValf(const char *json, jsmntok_t *token, float defValue);

char *jsmnObjStringVal(const char *json, jsmntok_t *token, const char *name,
                       char *buf, int buflen, const char *defValue);
int jsmnObjBoolVal(const char *json, jsmntok_t *token, const char *name,
                   int defValue);
int jsmnObjNumberVal(const char *json, jsmntok_t *token, const char *name,
                     int defValue);
float jsmnObjNumberValf(const char *json, jsmntok_t *token, const char *name,
                        float defValue);
jsmntok_t *jsmnObjObjToken(const char *json, jsmntok_t *token, const char *name);
jsmntok_t *jsmnObjArrayToken(const char *json, jsmntok_t *token, const char *name);

int jsmnObjColorVal(const char *json, jsmntok_t *token, const char *name,
                    int *red, int *green, int *blue);

typedef enum {
    eJsonTypeInt = 0,
    eJsonTypeFloat,
    eJsonTypeString,
    eJsonTypeArray,
    eJsonTypeObject
} eJsonParseType_t;

typedef struct jsonParseMap_s jsonParseMap_t;

typedef int (*jsonIntValValidator)(int val);
typedef int (*jsonFloatValValidator)(float val);
typedef int (*jsonStringValValidator)(const char *str);
typedef int (*jsonStringConversion)(const char *str, int *val, float *valf);

struct jsonParseMap_s {
    const char *name;
    eJsonParseType_t type;
    int offset;
    int required;
    union {
        int defInt;
        float defFloat;
        char *defString;
    };
    union {
        jsonIntValValidator intValidator;
        jsonFloatValValidator floatValidator;
        jsonStringValValidator stringValidator;
    };
    eJsonParseType_t convertType;
    jsonStringConversion stringConvert;
    char *failMsg;
};

#define END_JSON_PARSE_MAP()    {.name = NULL}

typedef int (*jsonCollectionCallback_t)(const char *json, jsmntok_t *tokens, const char *name, void *context);

int jsonParseObject(const char *json, jsmntok_t *tokens,
                    jsonCollectionCallback_t callback, void *context);
int jsonParseToStruct(const char *json, jsmntok_t *tokens, jsonParseMap_t *map,
                      const char *preMsg, void *dest, void (*cleanup)(void *data));
int jsmnDump(const char *prefix, int indent, int array, const char *json, jsmntok_t *tokens);


#endif // _FLUB_UTIL_JSMN_HEADER_
