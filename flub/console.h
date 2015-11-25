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
//void consoleBgImageSet(texture_t *tex);

void consolePrint(const char *str);
void consolePrintf(const char *fmt, ...);
void consolePrintQC(const char *str);
void consolePrintfQC(const char *fmt, ...);

int consoleWindowCharWidth(void);
void consoleColorSet(int red, int green, int blue);

void consoleTabularDivider(int border, int columns, int *colWidths);
void consoleTabularPrint(int border, int columns, int *colWidths, const char *colStrs);

typedef int (*consoleCmdParamValidator_t)(void *context, int paramPos,
                                          const char *param);
typedef int (*consoleCmdParamCB_t)(void *context, int paramPos,
                                   const char **params, char *acBuffer,
                                   int acLen);

typedef struct consoleCmdItem_s {
    const char *name;
    const char *help;
    const char *detailedHelp;
    consoleCmdParamCB_t validator;
    consoleCmdParamCB_t hinter;
    consoleCmdHandler_t handler;
} consoleCmdItem_t;

/*
 * Command hinting/completion
 * Basic parameter authentication

    columnar/tabular output
    add console bg color var
    add console bg alpha var

    console command API
    color coded output

    tab completion
        command hinting
        param hinting
    input feedback
        color coded input validation

 */


#endif // _FLUB_CONSOLE_HEADER_
