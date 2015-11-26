#ifndef _FLUB_CMDLINE_HEADER_
#define _FLUB_CMDLINE_HEADER_


#include <flub/module.h>
#include <flub/app.h>
#include <flub/cmdline_handler.h>


typedef struct cmdlineOption_s {
    const char *name;
    int val;
    int has_arg;
    const char *meta;
    const char *help;
    cmdlineOptHandler_t handler;
} cmdlineOption_t;


#define END_OF_CMDLINE_OPT_LIST     { NULL, 0, 0, NULL, NULL }


int cmdlineInit(appDefaults_t *defaults);
int cmdlineValid(void);
void cmdlineShutdown(void);

void cmdlineOptAdd(const char *name, int val, int has_arg, const char *meta,
                   const char *help, cmdlineOptHandler_t handler);
void cmdlineOptEntryAdd(cmdlineOption_t *opts);
void cmdlineOptListAdd(cmdlineOption_t *opts);

const char *cmdlineGetAndRemoveOption(const char *name, int val,
                                      int has_arg, const char *meta,
                                      const char *help,
                                      const char *optfound,
                                      const char *optnotfound);

eCmdLineStatus_t cmdlineProcess(cmdlineDefHandler_t handler, void *context);


#endif // _FLUB_CMDLINE_HEADER_
