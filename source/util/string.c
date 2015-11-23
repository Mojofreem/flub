#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#define _GNU_SOURCE
#include <string.h>
#include <ctype.h>
#include <flub/memory.h>


#ifndef strndup
#ifndef MACOSX
static char *strndup(const char *s, size_t n) {
    char *ptr;
    int len = strlen(s);

    if(len > n) {
        len = n;
    }

    if((ptr = ((char *)util_alloc((len + 1), NULL))) == NULL) {
        return NULL;
    }
    memcpy(ptr, s, len);
    ptr[len] = '\0';
    return ptr;
}
#endif // MACOSX
#endif // strndup



char *strToUpper(char *str) {
    char *ptr;

    if(str) {
        for(ptr = str; *ptr != '\0'; ptr++) {
            *ptr = toupper(*ptr);

        }
    }
    return str;
}

char *strToLower(char *str) {
    char *ptr;

    if(str) {
        for(ptr = str; *ptr != '\0'; ptr++) {
            *ptr = tolower(*ptr);
        }
    }
    return str;
}

char *trimStr(char *str) {
    char *ptr, *tmp, *ret=str;

    if(str) {
        for(ptr = str; isspace(*ptr); ptr++);

        for(; (*str = *ptr) != '\0'; ptr++, str++)
        {
            if(isspace(*ptr)) {
                tmp = str;
                for(; isspace(*ptr); ptr++, str++) {
                    *str=*ptr;
                }
                if(*ptr == '\0')
                {
                    *tmp = '\0';
                    break;
                }
                *str = *ptr;
            }
        }
    }
    return ret;
}

int strNeedsQuote(const char *str) {
    if(str == NULL) {
        return 0;
    }

    for(; *str != '\0'; str++) {
        switch(*str) {
            case '\n':
            case '\r':
            case '\t':
            case '\x1b':
            case '"':
            case '\\':
                return 1;
        }
    }
    return 0;
}

int strQuoteLen(const char *str) {
    int count=1;
    const char *ptr;

    if(str == NULL) {
        return 0;
    }

    for(ptr = str; ptr != '\0'; ptr++) {
        count++;
        switch(*ptr) {
            case '\n':
            case '\r':
            case '\t':
            case '\x1b':
            case '"':
            case '\\':
                count++;
                break;
        }
    }

    return count;
}

int strQuote(char *str, int maxlen) {
    int count = 1;
    int add = 0;
    char *ptr;
    char esc;

    if(str == NULL) {
        return 0;
    }

    for(ptr = str; ptr != '\0'; ptr++) {
        count++;
        switch(*ptr) {
            case '\n':
            case '\r':
            case '\t':
            case '\x1b':
            case '"':
            case '\\':
                add++;
                break;
        }
    }

    if((add + count) > maxlen) {
        // The quoted string is too large
        return 0;
    }

    for(ptr = str; ptr != '\0'; ptr++) {
        switch(*ptr) {
            case '\n':
                esc = 'n';
                break;
            case '\r':
                esc = 'r';
                break;
            case '\t':
                esc = 't';
                break;
            case '\x1b':
                esc = 'e';
                break;
            case '"':
                esc = '"';
                break;
            case '\\':
                esc = '\\';
                break;
            default:
                esc = 0;
        }
        if(esc) {
            memmove(ptr + 1, ptr, count);
            *ptr = '\\';
            ptr++;
            *ptr = esc;
        }
        count--;
    }

    return 1;
}

int strQuoteCpy(const char *src, char *dst, int maxlen) {
    int count = 1;
    char esc;

    if((src == NULL) || (dst == NULL)) {
        return 0;
    }

    for(; src != '\0'; src++) {
        count++;
        if(count == maxlen) {
            *dst = '\0';
            return 0;
        }
        switch(*src) {
            case '\n':
                esc = 'n';
                break;
            case '\r':
                esc = 'r';
                break;
            case '\t':
                esc = 't';
                break;
            case '\x1b':
                esc = 'e';
                break;
            case '"':
                esc = '"';
                break;
            case '\\':
                esc = '\\';
                break;
            default:
                esc = 0;
        }
        if(esc) {
            if((count + 2) >= maxlen) {
                *dst = '\0';
                return 0;
            }
            *dst = '\\';
            count++;
            *dst = esc;
        } else {
            *dst=*src;
        }
    }
    *dst='\0';

    return 1;
}

char *strUnquote(char *str) {
    char *ptr, *ret = str;

    if(str == NULL) {
        return NULL;
    }

    for(ptr = str; *ptr != '\0'; ptr++, str++) {
        if(*ptr == '\\') {
            ptr++;
            switch(*ptr) {
                case 't':
                    *str = '\t';
                    break;
                case 'r':
                    *str = '\r';
                    break;
                case 'n':
                    *str = '\n';
                    break;
                case 'e':
                    *str = '\x1b';
                    break;
                default:
                    *str = *ptr;
                    break;
            }
        } else {
            *str = *ptr;
        }
    }
    *str = '\0';
    return ret;
}

char *strUnquoteCpy(char *str, char *dst, int maxlen) {
    char *ret = dst;

    if((str == NULL) || (dst == NULL)) {
        return NULL;
    }

    for(; *str != '\0'; dst++, str++) {
        maxlen--;
        if(maxlen <= 0) {
            break;
        }
        if(*str == '\\') {
            str++;
            switch(*str) {
                case 't':
                    *dst = '\t';
                    break;
                case 'r':
                    *dst = '\r';
                    break;
                case 'n':
                    *dst = '\n';
                    break;
                case 'e':
                    *dst = '\x1b';
                    break;
                default:
                    *dst = *str;
                    break;
            }
        } else {
            *dst = *str;
        }
    }
    *str = '\0';
    return ret;
}

int strTokenize(char *str, char **tokens, int maxtokens) {
    int count = 0;
    char *ptr;

    if(str == NULL) {
        return 0;
    }

    for(; (*str != '\0') && (count < maxtokens); str++) {
        for(; isspace(*str); str++ );
        if(*str == '\0') {
            return count;
        }

        if(*str == '\"') {
            str++;
            tokens[count++] = str;
            for(; (*str != '\"') && (*str != '\0'); str++) {
                if((*str == '\\') &&
                   ((*(str + 1) == '\"') || (*(str + 1) == '\\'))) {
                    str++;
                }
            }
            if(*str == '\0') {
                return count;
            }
            *str = '\0';
        } else {
            tokens[count++] = str;
            for(; (!isspace(*str)) && (*str != '\0'); str++);
            if(*str == '\0') {
                return count;
            }
            *str = '\0';
        }
    }
    return count;
}

// Just like strtokenize, but passed a string that it cannot alter. Stores the
// locations of the start of tokens in "tokens" and the lengths of the tokens
// in "lens".
int strTokenizeConst(const char *str, char **tokens, int *lens, int maxtokens) {
    int count = 0, len = 0;
    char *ptr;

    if(str == NULL) {
        return 0;
    }

    for(; (*str != '\0') && (count < maxtokens); str++) {
        for(; isspace(*str); str++);
        if(*str == '\0') {
            return count;
        }

        if(*str == '\"') {
            str++;
            tokens[count] = (char *)str;
            for(; (*str != '\"') && (*str != '\0'); str++) {
                if((*str == '\\') &&
                   ((*(str + 1) == '\"') || (*(str + 1) == '\\'))) {
                    str++;
                    len++;
                }
            }
            lens[count]=len;
            count++;
            if(*str == '\0') {
                return count;
            }
            len = 0;
        } else {
            tokens[count] = (char *)str;
            for(; (!isspace(*str)) && (*str != '\0'); str++, len++);
            lens[count] = len;
            count++;
            if(*str == '\0') {
                return count;
            }
            len = 0;
        }
    }
    return count;
}

int strFields(char *str, int fieldchar, char **fields, int maxfields) {
    int count = 0;

    if(str == NULL) {
        return 0;
    }

    for(; (*str != '\0') && (count < maxfields); str++) {
        fields[count++] = str;
        for(; (*str != fieldchar) && (*str != '\0'); str++);
        if(*str == fieldchar) {
            *str = '\0';
        } else {
            break;
        }
    }
    return count;
}

int strFieldsConst(const char *str, int fieldchar, const char **fields, int *lens, int maxfields) {
    int count = 0;

    if(str == NULL) {
        return 0;
    }

    for(; (*str != '\0') && (count < maxfields); str++) {
        fields[count++] = str;
        for(; (*str != fieldchar) && (*str != '\0'); str++);
        if(*str == fieldchar) {
            lens[count - 1] = str - fields[count - 1];
        } else {
            break;
        }
    }
    return count;
}

