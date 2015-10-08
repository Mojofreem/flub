#ifndef _FLUB_UTIL_PARSE_HEADER_
#define _FLUB_UTIL_PARSE_HEADER_


typedef struct colorName_s {
    const char *name;
    char c;
    unsigned int value;
} colorName_t;

extern colorName_t *g_colorTable;

int parseIsNumber(const char *str);

int parseHexGroup(const char *str, unsigned int *value, int *digitCount);
int parseTripleGroup(const char *str, unsigned int *value, int *digitCount);

// [0-9a-fA-F]+  0x([0-9A-Fa-f]+)  ([0-9A-Fa-f]+)h
unsigned int parseHex(const char *str);
// ((1?[1-9]\d?)|0|100|(2([0-4]\d|5[0-5])))(\.((1?[1-9]\d?)|0|100|(2([0-4]\d|5[0-5])))){3,3}:\d+
int parseIpAddrPort(const char *str, int *segments, int *port);
// \d+x\d+
int parseValXVal(const char *str, int *val1, int *val2);
// (0x|#)?[0-9A-Fa-f]{3}[0-9A-Fa-f]{3} |
// ((1?[1-9]\d?)|0|100|(2([0-4]\d|5[0-5])))(,((1?[1-9]\d?)|0|100|(2([0-4]\d|5[0-5]))){2,2}) |
// black | darkblue | darkgreen | cyan | darkred | magenta | brown | gray |
// brightgray | darkgray | blue | brightblue | green | brightgreen |
// brightcyan | red | brightred | pink | brightmagenta | yellow | white |
// orange
int parseColor(const char *str, int *red, int *green, int *blue, int *alpha);

int utilIntFromString(const char *str, int defvalue);
int utilBoolFromString(const char *str, int defvalue);
float utilFloatFromString(const char *str, float defvalue);

typedef enum {
    eParseIniNone = 0,
    eParseIniInvalid,
    eParseIniComment,
    eParseIniSection,
    eParseIniNvPair,
} eParseIniType_t;

eParseIniType_t utilIniParse(char *str, int comment, char **name, char **value);
int utilNVPair(char *str, char **name, char **value);

void utilStripCommentsFromJSON(char *buf, int size);


#endif // _FLUB_UTIL_PARSE_HEADER_
