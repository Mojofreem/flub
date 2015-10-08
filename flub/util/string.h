#ifndef _FLUB_UTIL_STRING_HEADER_
#define _FLUB_UTIL_STRING_HEADER_


#include <string.h>


#ifndef strndup
char *strndup(const char *s, size_t n);
#endif // strndup

char *strToUpper(char *str);
char *strToLower(char *str);
char *trimStr(char *str);

int strNeedsQuote(const char *str);
int strQuoteLen(const char *str);
int strQuote(char *str, int maxlen);
int strQuoteCpy(const char *src, char *dst, int maxlen);
char *strUnquote(char *str);
char *strUnquoteCpy(char *str, char *dst, int maxlen);

int strTokenize(char *str, char **tokens, int maxtokens);
int strTokenizeConst(const char *str, char **tokens, int *lens,
                     int maxtokens);

int strFields(char *str, int fieldchar, char **fields, int maxfields);
int strFieldsConst(const char *str, int fieldchar, const char **fields, int *lens,
                   int maxfields);


#endif // _FLUB_UTIL_STRING_HEADER_
