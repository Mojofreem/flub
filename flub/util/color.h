#ifndef _FLUB_UTIL_COLOR_HEADER_
#define _FLUB_UTIL_COLOR_HEADER_


typedef struct flubColor3f_s {
    float red;
    float green;
    float blue;
} flubColor3f_t;

typedef struct flubColor3i_s {
    int red;
    int green;
    int blue;
} flubColor3i_t;


#define COLOR_FTOI(f)   ((int) ((f) * 255.0))
#define COLOR_ITOF(i)   ((float) (((float)(i)) / 255.0))


void utilColor3fTo3i(flubColor3f_t *fColor, flubColor3i_t *iColor);
void utilColor3iTo3f(flubColor3i_t *iColor, flubColor3f_t *fColor);

void utilGetQCColorCode(char c, float *red, float *green, float *blue);
void utilGetQCColorCodeStruct3f(char c, flubColor3f_t *color);
void utilGetQCColorCodeStruct3i(char c, flubColor3f_t *color);
void utilSetQCColorCode(char c);


#endif // _FLUB_UTIL_COLOR_HEADER_
