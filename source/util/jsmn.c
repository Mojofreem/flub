#include <flub/3rdparty/jsmn/jsmn.h>
#include <flub/util/jsmn.h>
#include <flub/util/parse.h>
#include <string.h>
#include <flub/logger.h>
#include <flub/util/enum.h>
#include <flub/memory.h>
#include <flub/util/color.h>


#define JSMN_MAX_KEY_LEN    64


char *jsmnStringDecode(const char *json, jsmntok_t *token, char *buf, int buflen) {
    int pos;
    static char fallback[JSMN_MAX_KEY_LEN];
    char *out;

    if(buf == NULL) {
        buf = fallback;
        buflen = sizeof(fallback);
    }
    out = buf;

    /*
    if(token->type != JSMN_STRING) {
        buf[0] = '\0';
        return buf;
    }
    */

    //infof("Converting [%*.*s]", token->end - token->start, token->end - token->start, json + token->start);
    // Leave room for the null terminator
    buflen--;
    for(pos = token->start; ((pos < token->end) && (json[pos] != '\0') && (buflen)); pos++, buflen--, out++) {
        if(json[pos] == '\\') {
            pos++;
            if((pos > token->end) || (json[pos] != '\0')) {
                break;
            }
            switch(json[pos]) {
                case '"':
                case '\\':
                case '/':
                default:
                    *out = json[pos];
                    break;
                case 'b':
                    *out = '\b';
                    break;
                case 'f':
                    *out = '\f';
                    break;
                case 'n':
                    *out = '\n';
                    break;
                case 'r':
                    *out = '\r';
                    break;
                case 't':
                    *out = '\t';
                    break;
                case 'u':
                    *out = '#';
                    pos += 4;
                    break;
            }
        } else {
            *out = json[pos];
        }
    }
    *out = '\0';

    return buf;
}

jsmntok_t *jsmnFindKey(const char *json, jsmntok_t *tokens, const char *name) {
    jsmnWalkState_t walk;
    jsmntok_t *token;
    char buf[JSMN_MAX_KEY_LEN];
    char buf2[JSMN_MAX_KEY_LEN];
    char *str;
    jsmntok_t *val, *key;

    if(!jsmnIsObject(tokens)) {
        return NULL;
    }

    if(!jsmnWalkInit(&walk, tokens)) {
        return NULL;
    }

    //infof("JSON: Looking for key [%s]", name);
    while(jsmnWalk(&walk, &key, &val)) {
        //infof("JSON:     Checking [%s]", jsmnStringDecode(json, key, buf, sizeof(buf)));
        if((str = jsmnStringDecode(json, key, buf, sizeof(buf))) != NULL) {
            if(!strcmp(str, name)) {
                //info("JSON:         Found!");
                return val;
            }
        }
    }

    return NULL;
}

int jsmnWalkInit(jsmnWalkState_t *state, jsmntok_t *tokens) {
    if((tokens->type != JSMN_ARRAY) && (tokens->type != JSMN_OBJECT)) {
        state->done = 1;
        return 0;
    }

    memset(state, 0, sizeof(jsmnWalkState_t));
    state->tokens = tokens;
    state->count = tokens[0].size;

    return 1;
}

static void _jsmnSkipNested(jsmnWalkState_t *state) {
    int skip;

    skip = state->tokens[state->pos].size;

    while(skip) {
        state->pos++;
        // Skip any nested components
        _jsmnSkipNested(state);
        skip--;
    }
}

int jsmnWalk(jsmnWalkState_t *state, jsmntok_t **key, jsmntok_t **val) {
    int skip;

    if(state->done) {
        return 0;
    }
    state->pos++;

    if(state->tokens[0].type == JSMN_OBJECT) {
        if(key != NULL) {
            *key = state->tokens + state->pos;
        }
        state->pos++;
        if(val != NULL) {
            *val = state->tokens + state->pos;
        }
    } else {
        if(key != NULL) {
            *key = NULL;
        }
        if(val != NULL) {
            *val = state->tokens + state->pos;
        }
    }
    state->count--;
    _jsmnSkipNested(state);

    if(state->count <= 0) {
        state->done = 1;
    }

    return 1;
}

int jsmnIsBool(const char *json, jsmntok_t *token) {
    if(token == NULL) {
        return 0;
    }
    if(token->type == JSMN_PRIMITIVE) {
        if((json[token->start] == 't') || (json[token->start] == 'f')) {
            return 1;
        }
    }
    return 0;
}

int jsmnIsNull(const char *json, jsmntok_t *token) {
    if(token == NULL) {
        return 0;
    }
    if(token->type == JSMN_PRIMITIVE) {
        if(json[token->start] == 'n') {
            return 1;
        }
    }
    return 0;
}

int jsmnIsNumber(const char *json, jsmntok_t *token) {
    if(token == NULL) {
        return 0;
    }
    if(token->type == JSMN_PRIMITIVE) {
        if((json[token->start] == '-') ||
           ((json[token->start] >= 0) && (json[token->start] <= '9'))) {
            return 1;
        }
    }
    return 0;
}

int jsmnIsString(jsmntok_t *token) {
    if(token == NULL) {
        return 0;
    }
    if(token->type == JSMN_STRING) {
        return 1;
    }
    return 0;
}

int jsmnIsObject(jsmntok_t *token) {
    if(token == NULL) {
        return 0;
    }
    if(token->type == JSMN_OBJECT) {
        return 1;
    }
    return 0;
}

int jsmnIsArray(jsmntok_t *token) {
    if(token == NULL) {
        return 0;
    }
    if(token->type == JSMN_ARRAY) {
        return 1;
    }
    return 0;
}

const char *jsmnTokenType(const char *json, jsmntok_t *token) {
    if(token == NULL) {
        return "NULL";
    }

    switch(token->type) {
        case JSMN_PRIMITIVE:
            if((json[token->start] == 't') || (json[token->start] == 'f')) {
                return "boolean";
            }
            if(json[token->start] == 'n') {
                return "null";
            }
            if((json[token->start] == '-') ||
               ((json[token->start] >= 0) && (json[token->start] <= '9'))) {
                return "number";
            }
            return "unknown";
        case JSMN_OBJECT:
            return "object";
        case JSMN_ARRAY:
            return "array";
        case JSMN_STRING:
            return "string";
        default:
            return "unknown";
    }
}

char *jsmnStringVal(const char *json, jsmntok_t *token, char *buf, int buflen,
                    const char *defValue) {
    int len;

    if((token == NULL) || (token->type != JSMN_STRING)) {
        if(defValue == NULL) {
            return NULL;
        }
        len = strlen(defValue);
        if(len > buflen) {
            len = buflen;
        }
        strncpy(buf, defValue, len);
        buf[sizeof(buf) - 1] = '\0';
        return buf;
    }

    return jsmnStringDecode(json, token, buf, buflen);
}

int jsmnBoolVal(const char *json, jsmntok_t *token, int defValue) {
    if(token == NULL) {
        return defValue;
    }
    if(token->type == JSMN_PRIMITIVE) {
        if(json[token->start] == 't') {
            return 1;
        } else if(json[token->start] == 'f') {
            return 0;
        }
    }
    return defValue;
}

int jsmnNumberVal(const char *json, jsmntok_t *token, int defValue) {
    char buf[JSMN_MAX_KEY_LEN];
    int len;

    if(token == NULL) {
        return defValue;
    }
    if(token->type == JSMN_PRIMITIVE) {
        len = token->end - token->start + 1;
        if(len >= sizeof(buf)) {
            len = sizeof(buf);
        }
        memcpy(buf, json + token->start, len);
        buf[sizeof(buf) - 1] = '\0';
        return utilIntFromString(buf, defValue);
    }
    return defValue;
}

float jsmnNumberValf(const char *json, jsmntok_t *token, float defValue) {
    char buf[JSMN_MAX_KEY_LEN];
    int len;

    if(token == NULL) {
        return defValue;
    }
    if(token->type == JSMN_PRIMITIVE) {
        len = token->end - token->start + 1;
        if(len >= sizeof(buf)) {
            len = sizeof(buf);
        }
        memcpy(buf, json + token->start, len);
        buf[sizeof(buf) - 1] = '\0';
        return utilFloatFromString(buf, defValue);
    }
    return defValue;
}

char *jsmnObjStringVal(const char *json, jsmntok_t *token, const char *name,
                       char *buf, int buflen, const char *defValue) {
    jsmntok_t *tok;
    int len;

    if((tok = jsmnFindKey(json, token, name)) == NULL) {
        //warningf("JSMN: unable to find key [%s] in object", name);
        if(defValue == NULL) {
            return NULL;
        }
        len = strlen(defValue);
        if(len > buflen) {
            len = buflen;
        }
        strncpy(buf, defValue, len);
        buf[sizeof(buf) - 1] = '\0';
        return buf;
    }

    if(!jsmnIsString(tok)) {
        if(defValue == NULL) {
            return NULL;
        }
        len = strlen(defValue);
        if(len > buflen) {
            len = buflen;
        }
        strncpy(buf, defValue, len);
        buf[sizeof(buf) - 1] = '\0';
        return buf;
    }

    return jsmnStringVal(json, tok, buf, buflen, defValue);
}

int jsmnObjBoolVal(const char *json, jsmntok_t *token, const char *name,
                   int defValue) {
    jsmntok_t *tok;

    if((tok = jsmnFindKey(json, token, name)) == NULL) {
        return defValue;
    }

    if(!jsmnIsBool(json, tok)) {
        return defValue;
    }

    return jsmnBoolVal(json, tok, defValue);
}

int jsmnObjNumberVal(const char *json, jsmntok_t *token, const char *name,
                     int defValue) {
    jsmntok_t *tok;

    if((tok = jsmnFindKey(json, token, name)) == NULL) {
        return defValue;
    }

    if(!jsmnIsNumber(json, tok)) {
        return defValue;
    }

    return jsmnNumberVal(json, tok, defValue);
}

float jsmnObjNumberValf(const char *json, jsmntok_t *token, const char *name,
                        float defValue) {
    jsmntok_t *tok;

    if((tok = jsmnFindKey(json, token, name)) == NULL) {
        return defValue;
    }

    if(!jsmnIsNumber(json, tok)) {
        return defValue;
    }

    return jsmnNumberValf(json, tok, defValue);
}

jsmntok_t *jsmnObjObjToken(const char *json, jsmntok_t *token, const char *name) {
    jsmntok_t *tok;

    if((tok = jsmnFindKey(json, token, name)) == NULL) {
        return NULL;
    }

    if(!jsmnIsObject(tok)) {
        errorf("JSON node [%s] is not an object, it's a %s", name, jsmnTokenType(json, tok));
        return NULL;
    }

    return tok;
}

jsmntok_t *jsmnObjArrayToken(const char *json, jsmntok_t *token, const char *name) {
    jsmntok_t *tok;

    if((tok = jsmnFindKey(json, token, name)) == NULL) {
        return NULL;
    }

    if(!jsmnIsArray(tok)) {
        errorf("JSON node [%s] is not an array, it's a %s", name, jsmnTokenType(json, tok));
        return NULL;
    }

    return tok;
}

int jsmnObjColorVal(const char *json, jsmntok_t *token, const char *name,
                    int *red, int *green, int *blue) {
    jsmntok_t *tok;
    char buf[64];
    flubColor4f_t color;
    int result;

    if((tok = jsmnFindKey(json, token, name)) == NULL) {
        return 0;
    }

    if(!jsmnIsString(tok)) {
        return 0;
    }

    if(jsmnStringVal(json, tok, buf, sizeof(buf), "#000") == NULL) {
        return 0;
    }

    result = flubColorParse(buf, &color);
    *red = COLOR_FTOI(color.red);
    *green = COLOR_FTOI(color.green);
    *blue = COLOR_FTOI(color.blue);

    return result;
}

utilEnumMap_t _jsonParseTypeMap[] = {
        ENUM_MAP(eJsonTypeInt),
        ENUM_MAP(eJsonTypeFloat),
        ENUM_MAP(eJsonTypeString),
        ENUM_MAP(eJsonTypeArray),
        ENUM_MAP(eJsonTypeObject),
        END_ENUM_MAP()
};

#define MAX_JSON_STRING_LEN 256

int jsonParseObject(const char *json, jsmntok_t *tokens,
                    jsonCollectionCallback_t callback, void *context) {
    jsmnWalkState_t state;
    jsmntok_t *key, *val;
    int array = 0;
    char buf[MAX_JSON_STRING_LEN];
    char *name = NULL;

    if(!jsmnIsObject(tokens)) {
        if(!jsmnIsArray(tokens)) {
            errorf("%s is not a JSON object or array [%s]",
                   jsmnTokenType(json, tokens));
            return 0;
        } else {
            array = 1;
        }
    }

    jsmnWalkInit(&state, tokens);
    while(jsmnWalk(&state, &key, &val)) {
        if(!array) {
            name = jsmnStringDecode(json, key, buf, sizeof(buf));
        }
        if(!callback(json, val, name, context)) {
            return 0;
        }
    }
    return 1;
}

int jsonParseToStruct(const char *json, jsmntok_t *tokens, jsonParseMap_t *map,
                      const char *preMsg, void *dest, void (*cleanup)(void *data)) {
    int k;
    int val;
    float valf;
    int fail = 0;
    char *ptr;
    char **ptrptr;
    char buf[MAX_JSON_STRING_LEN];

    for(k = 0; ((!fail) && (map[k].name != NULL)); k++) {
        if(map[k].required) {
            if(jsmnFindKey(json, tokens, map[k].name) == NULL) {
                errorf("%s \"%s\" was not found.", preMsg, map[k].name);
                fail = 1;
                break;
            }
        }
        switch(map[k].type) {
            case eJsonTypeInt:
                val = jsmnObjNumberVal(json, tokens, map[k].name, map[k].defInt);
                if(map[k].intValidator != NULL) {
                    if(!map[k].intValidator(val)) {
                        errorf("%s \"%s\" value %d %s", preMsg, map[k].name,
                               val, map[k].failMsg);
                        fail = 1;
                        break;
                    }
                }
                *((int *)(((unsigned char *)dest) + map[k].offset)) = val;
                break;
            case eJsonTypeFloat:
                valf = jsmnObjNumberValf(json, tokens, map[k].name, map[k].defFloat);
                if(map[k].floatValidator != NULL) {
                    if(!map[k].floatValidator(valf)) {
                        errorf("%s \"%s\" value %f %s", preMsg, map[k].name,
                               valf, map[k].failMsg);
                        fail = 1;
                        break;
                    }
                }
                *((float *)(((unsigned char *)dest) + map[k].offset)) = valf;
                break;
            case eJsonTypeString:
                jsmnObjStringVal(json, tokens, map[k].name, buf, sizeof(buf), map[k].defString);
                if(map[k].stringValidator != NULL) {
                    if(!map[k].stringValidator(buf)) {
                        errorf("%s \"%s\" value [%s] %s", preMsg, map[k].name,
                               buf, map[k].failMsg);
                        fail = 1;
                        break;
                    }
                }
                if(map[k].stringConvert != NULL) {
                    switch(map[k].convertType) {
                        case eJsonTypeInt:
                            if(!map[k].stringConvert(buf, &val, NULL)) {
                                errorf("%s \"%s\" value [%s] %s", preMsg, map[k].name,
                                       buf, map[k].failMsg);
                                fail = 1;
                                break;
                            }
                            *((int *)(((unsigned char *)dest) + map[k].offset)) = val;
                            break;
                        case eJsonTypeFloat:
                            if(!map[k].stringConvert(buf, NULL, &valf)) {
                                errorf("%s \"%s\" value [%s] %s", preMsg, map[k].name,
                                       buf, map[k].failMsg);
                                fail = 1;
                                break;
                            }
                            *((float *)(((unsigned char *)dest) + map[k].offset)) = valf;
                            break;
                        default:
                            errorf("%s \"%s\" has an invalid conversion type: %s", preMsg, map[k].name, utilEnum2Str(_jsonParseTypeMap, map[k].convertType, "<unknown>"));
                            fail = 1;
                            break;
                    }
                    break;
                } else {
                    ptrptr = (char **)(((unsigned char *)dest) + map[k].offset);
                    *ptrptr = util_strdup(buf);
                }
                break;
            default:
                // Whoa. This is an unhandled json data type.
                errorf("%s \"%s\" has an invalid type: %s", preMsg, map[k].name, utilEnum2Str(_jsonParseTypeMap, map[k].type, "<unknown>"));
                fail = 1;
                break;
        }
    }
    if(fail) {
        if(cleanup != NULL) {
            cleanup(dest);
        }
        return 0;
    }
    return 1;
}

// TODO Reimplement the jsmn JSON dump functionaility

int jsmnDump(const char *prefix, int indent, int array, const char *json, jsmntok_t *tokens) {
    int size;
    int spacing;
    static const char space[] = "                                                            ";
    int vallen;
    const char *val;
    int postvallen;
    const char *postval;
    int keylen;
    const char *keyval;
    int skip;
    int done = 0;

    size = tokens->size;
    spacing = indent * 2;
    do {
        keyval = NULL;
        if ((tokens->type == JSMN_PRIMITIVE) && (!array)) {
            keylen = tokens->end - tokens->start;
            keyval = json + tokens->start;
            tokens++;
            done++;
        }
        val = NULL;
        postval = NULL;
        switch (tokens->type) {
            default:
            case JSMN_PRIMITIVE:
                vallen = tokens->end - tokens->start;
                val = json + tokens->start;
                break;
            case JSMN_OBJECT:
                vallen = 1;
                val = "{";
                postvallen = 1;
                postval = "}";
                break;
            case JSMN_ARRAY:
                vallen = 1;
                val = "[";
                postvallen = 1;
                postval = "]";
                break;
                break;
            case JSMN_STRING:
                vallen = tokens->end - tokens->start;
                val = json + tokens->start;
                break;
        }
        if (keyval != NULL) {
            infof("%s %.*s %.*s: %.*s", prefix, spacing, space,
                  keylen, keyval, vallen, val);
        } else {
            infof("%s %.*s %.*s", prefix, spacing, space, vallen, val);
        }
        if (tokens->size > 0) {
            skip = jsmnDump(prefix, indent + 1,
                            ((tokens->type == JSMN_ARRAY) ? 1 : 0), json,
                            tokens + 1);
            tokens += skip;
            done += skip;
        }
        if (postval != NULL) {
            infof("%s %.*s %.*s", prefix, spacing, space, postvallen, postval);
        }
        tokens++;
        done++;
        size--;
    } while(size > 0);

    return done;
}
