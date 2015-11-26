#ifndef _FLUB_CMDLINE_HANDLER_HEADER_
#define _FLUB_CMDLINE_HANDLER_HEADER_


typedef enum {
    eCMDLINE_OK = 1,
    eCMDLINE_EXIT_SUCCESS,
    eCMDLINE_EXIT_FAILURE
} eCmdLineStatus_t;

typedef eCmdLineStatus_t (*cmdlineOptHandler_t)(const char *name,
                                                const char *arg, void *context);

typedef eCmdLineStatus_t (*cmdlineDefHandler_t)(const char *arg, void *context);


#endif // _FLUB_CMDLINE_HANDLER_HEADER_
