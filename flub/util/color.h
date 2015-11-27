#ifndef _FLUB_UTIL_COLOR_HEADER_
#define _FLUB_UTIL_COLOR_HEADER_


typedef struct flubColor4f_s {
	float red;
	float green;
	float blue;
	float alpha;
} flubColor4f_t;


#define COLOR_FTOI(f)   ((int) ((f) * 255.0))
#define COLOR_ITOF(i)   ((float) (((float)(i)) / 255.0))

void flubColorCopy(flubColor4f_t *colorA, flubColor4f_t *colorB);

void flubColorQCCodeGet(char c, flubColor4f_t *color, flubColor4f_t *defColor);

void flubColorGLColorSet(flubColor4f_t *color);
void flubColorGLColorAlphaSet(flubColor4f_t *color);

// (0x|#)?[0-9A-Fa-f]{3}[0-9A-Fa-f]{3} |
// ((1?[1-9]\d?)|0|100|(2([0-4]\d|5[0-5])))(,((1?[1-9]\d?)|0|100|(2([0-4]\d|5[0-5]))){2,2}) |
// black | darkblue | darkgreen | cyan | darkred | magenta | brown | gray |
// brightgray | darkgray | blue | brightblue | green | brightgreen |
// brightcyan | red | brightred | pink | brightmagenta | yellow | white |
// orange
int flubColorParse(const char *str, flubColor4f_t *color);


#endif // _FLUB_UTIL_COLOR_HEADER_
