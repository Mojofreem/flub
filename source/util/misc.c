#include <flub/util/misc.h>
#include <flub/logger.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#ifdef WIN32
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#endif // WIN32


#ifdef WIN32
void MsgBox(const char *title, const char *fmt, ...) {
    va_list ap;
    char msg[ 512 ];

    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg) - 1, fmt, ap);
    va_end(ap);
    info(msg);
	MessageBox(NULL, msg, title, MB_ICONEXCLAMATION|MB_OK);
}
#endif // WIN32


const char *utilBasePath(const char *path) {
    const char *last;

    if((last = strrchr(path, '/')) != NULL) {
        return last + 1;
    }
    return path;
}

int utilNextPowerOfTwo(int x) {
    int k = 2;

    while(x > k) {
        k <<= 1;
    }
    return k;
}

// TODO Implement passable output function pointer
// 0000  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  00000000  00000000
void hexDump(void *ptr, int len)
{
    int pos;
    int rowStart;
    int k;
    unsigned char *buf = (unsigned char *)ptr;

    for(pos = 0; pos < len; pos++)
    {
        if(!(pos % 16))
        {
            rowStart = pos;
            if(pos != 0)
            {
                printf("\n");
            }
            printf("%04d", pos);
        }
        printf("%s%02X", ((pos % 8) ? " " : "  "), buf[pos]);
        if((pos == (len - 1)) || (!((pos + 1) % 16)))
        {
            printf(" ");
            if((pos + 1) % 16)
            {
                for(k = ((pos + 1) % 16); k < 16; k++)
                {
                    printf("  %s", ((k % 8) ? " " : "  "));
                }
            }
            for(k = rowStart; k <= pos; k++)
            {
                printf("%s%c", ((k % 8) ? "" : " "), ((isprint(buf[k])) ? ((isspace(buf[k])) ? ' ' : buf[k]) : '.'));
            }
        }
    }
    printf("\n");
}

// http://stackoverflow.com/questions/109023/how-to-count-the-number-of-set-bits-in-a-32-bit-integer
int numberOfSetBits(unsigned int value) {
    value = value - ((value >> 1) & 0x55555555);
    value = (value & 0x33333333) + ((value >> 2) & 0x33333333);
    return (((value + (value >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
}
