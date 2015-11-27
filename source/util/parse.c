#include <flub/util/parse.h>
#include <flub/util/string.h>
#include <flub/logger.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>


int parseIsNumber(const char *str) {
    int found = 0;

    for(; ((*str != '\0') && (isspace(*str))); str++);
    if(*str == '\0') {
        return 0;
    }
    if(*str == '-') {
        str++;
    }
    for(; ((*str != '\0') && (isdigit(*str))); str++) {
        found = 1;
    }
    if(!found) {
        return 0;
    }
    if(*str == '\0') {
        return 1;
    }
    if(*str == '.') {
        str++;
    }
    for(; ((*str != '\0') && (isdigit(*str))); str++);
    for(; ((*str != '\0') && (isspace(*str))); str++);
    if(*str == '\0') {
        return 1;
    }
    // Hrm, there's something trailing
    return 0;
}

eParseIniType_t utilIniParse(char *str, int comment,
                             char **name, char **value) {
    char *ptr, *tmp;

    // Skip preceeding whitespace
    for(ptr = str; isspace(*ptr); ptr++);

    // Blank line?
    if(*ptr == '\0') {
        return eParseIniNone;
    }

    // Comment character?
    if(*ptr == comment) {
        return eParseIniComment;
    }

    // Possible section?
    if(*ptr == '[') {
        for(ptr++; isspace(*ptr); ptr++);
        if(!isalnum(*ptr)) {
            return eParseIniInvalid;
        }
        *name = ptr;
        for(ptr++; (*ptr != ']') && (*ptr != '\0'); ptr++) {
            if(isspace(*ptr)) {
                for(tmp = ptr; isspace(*ptr); ptr++);
                if(*ptr == '\0') {
                    return eParseIniInvalid;
                }
                if(*ptr == ']') {
                    *tmp = '\0';
                    return eParseIniSection;
                }
            }
        }
        if(*ptr == '\0') {
            return eParseIniInvalid;
        }
        *ptr = '\0';
        return eParseIniSection;
    }
    else // Possible NV pair
    {
        if(!isalnum(*ptr)) {
            return eParseIniInvalid;
        }
        *name = ptr;
        for(ptr++; (*ptr != '=') && (*ptr != '\0'); ptr++) {
            if(isspace(*ptr)) {
                for(tmp = ptr; isspace(*ptr); ptr++);
                if(*ptr == '\0') {
                    return eParseIniInvalid;
                }
                if(*ptr == '=') {
                    *tmp = '\0';
                    ptr++;
                    *value = trimStr(ptr);
                    return eParseIniNvPair;
                }
            }
        }
        if(*ptr == '\0') {
            return eParseIniInvalid;
        }
        *ptr = '\0';
        ptr++;
        *value = trimStr(ptr);
        return eParseIniNvPair;
    }
}

int utilNVPair(char *str, char **name, char **value) {
    char *ptr, *tmp;

    for(ptr = str; isspace(*ptr); ptr++);
    if((*ptr == '=') || (*ptr == '\0')) {
        return 0;
    }
    *name = ptr;
    for(ptr++; (*ptr != '=') && (*ptr != '\0'); ptr++) {
        if(isspace(*ptr)) {
            for(tmp = ptr; isspace(*ptr); ptr++);
            if(*ptr == '\0') {
                return 0;
            }
            if(*ptr == '=') {
                *tmp = '\0';
                ptr++;
                *value = trimStr(ptr);
                return 1;
            }
        }
    }
    if(*ptr == '\0') {
        return 0;
    }
    *ptr = '\0';
    ptr++;
    *value = trimStr(ptr);
    return 1;
}

// [0-9a-fA-F]+  0x([0-9A-Fa-f]+)  ([0-9A-Fa-f]+)h
unsigned int parseHex(const char *str) {
    unsigned int num = 0;
    int pos = 0;
    const char *orig = str;

    for(; isspace(*str); str++);

    // Skip preceeding 0x, if any
    for(; *str == '0'; str++) {
        if((str[1] == 'x') || (str[1] == 'X')) {
            str += 2;
            break;
        }
    }

    // Skip any preceeding zeros
    for(; *str == '0'; str++);

    // Convert the number
    for(; isxdigit(*str); str++) {
        if(pos == 8) {
            errorf("Cannot convert string \"%s\" to hexidecimal, uint overflow.", orig);
            return 0;
        }
        num *= 16;
        if((*str >= '0') && (*str <= '9')) {
            num += *str - '0';
        } else if((*str >= 'a') && (*str <= 'f')) {
            num += *str - 'a';
        } else if((*str >= 'A') && (*str <= 'F')) {
            num += *str - 'A';
        }
        pos++;
    }
    for(; isspace(*str); str++);
    if((*str == 'h') || (*str == 'H')) {
        str++;
        for(; isspace(*str); str++);
    }
    if(*str != '\0') {
        errorf("Cannot convert string \"%s\" to hexidecimal.", orig);
        return 0;
    }
    return num;
}

// ((1?[1-9]\d?)|0|100|(2([0-4]\d|5[0-5])))(\.((1?[1-9]\d?)|0|100|(2([0-4]\d|5[0-5])))){3,3}:\d+
int parseIpAddrPort(const char *str, int *segments, int *port) {
    const char *orig = str;
    int digit;
    int segment;
    int value;

    for(segment = 0; segment < 4; segment++) {
        for(; isspace(*str); str++);
        for(value = 0, digit = 0;
            (isdigit(*str) && (digit < 3));
             str++, digit++) {
            value *= 10;
            value += *str - '0';
        }
        if(!digit) {
            errorf("parseIpAddrPort(): invalid IP address \"%s\"", orig);
            return 0;
        }
        for(; isspace(*str); str++);
        if((segment != 3) && (*str != '.')) {
            errorf("parseIpAddrPort(): invalid IP address \"%s\"", orig);
            return 0;
        }
        segments[segment] = value;
        str++;
    }
    if(*str == ':') {
        str++;
        for(; isspace(*str); str++);
        for(value = 0, digit = 0; isdigit(*str); str++, digit++) {
            value *= 10;
            value += *str - '0';
        }
        if(!digit) {
            errorf("parseIpAddrPort(): invalid port IP address \"%s\"", orig);
            return 0;
        }
        if(port != NULL) {
            *port = value;
        }
        for(; isspace(*str); str++);
    }
    if(*str != '\0') {
        warningf("parseIpAddrPort(): ignoring extraneous data after IP address \"%s\"", str);
    }
    return 1;
}

// \d+x\d+
int parseValXVal(const char *str, int *val1, int *val2) {
    char tmp[32], *ptr;
    int k, w, h;

    if((str != NULL) || (strlen(str) < ( sizeof(tmp) - 1))) {
        strcpy(tmp, str);
        if(((ptr = strchr(tmp, 'x')) != NULL) ||
           ((ptr = strchr(tmp, 'X')) != NULL)) {
            *ptr = '\0';
            ptr++;
            for(k = 0; ((tmp[k] != '\0') && (isdigit(tmp[k]))); k++);
            if(tmp[k] == '\0') {
                for(k = 0; ((ptr[k] != '\0') && (isdigit(ptr[k]))); k++);
                if(ptr[k] == '\0') {
                    w = atoi(tmp);
                    h = atoi(ptr);
                    *val1=w;
                    *val2=h;
                    return 1;
                }
            }
        }
    }
    errorf("String '%s' is not a valid VxV value.\n", str);

    return 0;
}

int parseHexGroup(const char *str, unsigned int *value, int *digitCount) {
    int digits;
    int nibble;
    unsigned int result = 0;

    if((str == NULL) || (*str == '\0')) {
        return 0;
    }

    for(; isspace(*str); str++);

    if(str[0] == '#') {
        str++;
    } else if((str[0] == '0') && ((str[1] == 'x') || (str[1] == 'X'))) {
        str += 2;
    }

    for(digits = 0; isxdigit(*str); str++, digits++) {
        result <<= 4;
        if((*str >= '0') && (*str <= '9')) {
            nibble = (*str - '0');
        } else {
            nibble = ((tolower(*str) - 'a') + 10);
        }
        result |= nibble;
    }

    if(digits) {
        if((*str == '\0') || (isspace(*str))) {
            if(digitCount != NULL) {
                *digitCount = digits;
            }
            if(value != NULL) {
                *value = result;
            }
            if((digits != 3) && (digits != 4) && (digits != 6) && (digits != 8)) {
                return 0;
            }
            return 1;
        }
    }
    return 0;
}

int parseTripleGroup(const char *str, unsigned int *value, int *digitCount) {
    int group;
    int count;
    int segment;
    int space;
    unsigned int result = 0;

    if((str == NULL) || (*str == '\0')) {
        return 0;
    }

    for(; isspace(*str); str++);

    group = 0;
    segment = 0;
    count = 0;
    space = 0;
    for(; *str != '\0'; str++) {
        if(space) {
            // Ugh, we shouldn't be here
            return 0;
        }
        if(isdigit(*str)) {
            segment *= 10;
            segment += (*str - '0');
            count++;
        }
        if(isspace(*str)) {
            space = 1;
            while(isspace(*str)) {
                str++;
            }
        }
        if(*str == ',') {
            if(count == 0) {
                // Empty digit group
                return 0;
            }
            result <<= 8;
            if(segment > 255) {
                result |= 0xFF;
            } else {
                result |= (0xFF & segment);
            }
            group++;
            segment = 0;
            count = 0;
            space = 0;
            while(isspace(*str)) {
                str++;
            }
        }
    }

    if(count) {
        group++;
    }

    if(value != NULL) {
        *value = result;
    }

    if(digitCount != NULL) {
        *digitCount = group;
    }

    if((group < 3) || (group > 4)) {
        return 0;
    }

    return 1;
}

int utilIntFromString(const char *str, int defvalue) {
    if(str == NULL) {
        return defvalue;
    }
    return atoi(str);
}

int utilBoolFromString(const char *str, int defvalue) {
    if(str == NULL) {
        return defvalue;
    }
    if((!strcasecmp(str, "true")) ||
       (!strcasecmp(str, "on")) ||
       (!strcasecmp(str, "yes"))) {
        return 1;
    }
    return 0;
}

float utilFloatFromString(const char *str, float defvalue) {
    if(str == NULL) {
        return defvalue;
    }
    return (float) atof(str);
}

// Strip C/C++ comments from a text buffer (in place)
void utilStripCommentsFromJSON(char *buf, int size) {
    int pos;
    int start = 0;
    int quoted = 0;
    int multiline = 0;

    for(pos = 0; pos < size; pos++) {
        switch(buf[pos]) {
            case '"':
                if(!multiline) {
                    if(quoted) {
                        quoted = 0;
                    } else {
                        quoted = 1;
                    }
                }
                break;
            case '\\':
                if(!multiline) {
                    if(quoted) {
                        if(((pos + 1) < size) && ((buf[pos + 1] == '"') || (buf[pos + 1] == '\\'))) {
                            pos++;
                        }
                    } else {
                        if(((pos + 1) < size) && (buf[pos + 1] == '\\')) {
                            pos++;
                        }
                    }
                }
                break;
            case '/':
                if(!quoted) {
                    if((pos + 1) < size) {
                        if(buf[pos + 1] == '/') {
                            // Single line comment
                            start = pos;
                            pos++;
                            // Look for the end of line
                            while((pos < size) && (buf[pos] != '\n')) {
                                pos++;
                            }
                            if(pos < size) {
                                memcpy(buf + start, buf + pos, size - pos);
                                size -= (pos - start);
                                buf[size] = '\0';
                                pos = start - 1;
                            } else {
                                buf[start] = '\0';
                                return;
                            }
                        } else if(buf[pos + 1] == '*') {
                            // Multiline comment
                            start = pos;
                            pos++;
                            multiline = 1;
                        }
                    }
                }
                break;
            case '*':
                if(multiline) {
                    if(((pos + 1) < size) && (buf[pos + 1] == '/')) {
                        // End of multiline
                        multiline = 0;
                        pos++;
                        if((pos + 1) < size) {
                            memcpy(buf + start, buf + pos + 1, size - (pos + 1));
                            size -= ((pos + 1) - start);
                            buf[size] = '\0';
                            pos = start - 1;
                        } else {
                            buf[start] = '\0';
                            return;
                        }
                    }
                }
                break;
            default:
                break;
        }
    }
}
