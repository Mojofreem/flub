#ifndef _FLUB_CMDLINE_HEADER_
#define _FLUB_CMDLINE_HEADER_


typedef enum {
    eCMDLINE_OK = 1,
    eCMDLINE_EXIT_SUCCESS,
    eCMDLINE_EXIT_FAILURE
} eCmdLineStatus_t;

typedef eCmdLineStatus_t (*cmdlineOptHandler_t)(const char *name,
                                                const char *arg, void *context);

typedef struct cmdlineOption_s {
    const char *name;
    int val;
    int has_arg;
    const char *meta;
    const char *help;
    cmdlineOptHandler_t handler;
} cmdlineOption_t;


#define END_OF_CMDLINE_OPT_LIST     { NULL, 0, 0, NULL, NULL }


typedef eCmdLineStatus_t (*cmdlineDefHandler_t)(const char *arg, void *context);

int cmdlineInit(int argc, char **argv);
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
