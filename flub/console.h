#ifndef _FLUB_CONSOLE_HEADER_
#define _FLUB_CONSOLE_HEADER_


#include <stddef.h>
#include <SDL2/SDL.h>


typedef void (*consoleCmdHandler_t)(const char *name, int paramc, char **paramv);

typedef struct consoleCmd_s {
    const char *name;
    const char *help;
    consoleCmdHandler_t handler;
    consoleCmdHandler_t hint;
} consoleCmd_t;


int consoleInit(void);
int consoleStart(void);
int consoleUpdate(Uint32 ticks);

int consoleCmdRegister(const char *name, const char *help, consoleCmdHandler_t handler, consoleCmdHandler_t hint);
int consoleCmdUnregister(const char *name);
int consoleCmdListRegister(consoleCmd_t *list);

void consoleShow(int show);
void consoleClear(void);
int consoleVisible(void);
void consolePrint(const char *str);
void consolePrintf(const char *fmt, ...);

/*
 * Command hinting/completion
 * Basic parameter authentication
 * Columnar output
 * Table output
 */


#endif // _FLUB_CONSOLE_HEADER_
