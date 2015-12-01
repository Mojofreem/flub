#ifndef _FLUB_CONSOLE_HEADER_
#define _FLUB_CONSOLE_HEADER_


#include <stddef.h>
#include <SDL2/SDL.h>
#include <flub/texture.h>


typedef void (*consoleCmdHandler_t)(const char *name, int paramc, char **paramv);

typedef struct consoleCmd_s {
    const char *name;
    const char *help;
    consoleCmdHandler_t handler;
} consoleCmd_t;

#define CONSOLE_COMMAND_LIST_END()  {NULL, NULL, NULL}

int consoleCmdRegister(const char *name, const char *help, consoleCmdHandler_t handler);
int consoleCmdUnregister(const char *name);
int consoleCmdListRegister(consoleCmd_t *list);

void consoleShow(int show);
void consoleClear(void);
int consoleVisible(void);
void consoleBgImageSet(texture_t *tex);

void consolePrint(const char *str);
void consolePrintf(const char *fmt, ...);
void consolePrintQC(const char *str);
void consolePrintfQC(const char *fmt, ...);

int consoleWindowCharWidth(void);

void consoleTabularDivider(int border, int columns, int *colWidths);
void consoleTabularPrint(int border, int columns, int *colWidths, const char * const * const colStrs);


#endif // _FLUB_CONSOLE_HEADER_
