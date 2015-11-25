#ifndef _FLUB_MODULE_HEADER_
#define _FLUB_MODULE_HEADER_


#include <flub/app.h>
#include <SDL2/SDL.h>


typedef int (*flubModuleInitCB_t)(int argc, char **argv, appDefaults_t *appDefaults);
typedef int (*flubModuleStart_t)(void);
typedef void (*flubModuleShutdown_t)(void);
typedef int (*flubModuleUpdateCB_t)(Uint32 ticks, Uint32 elapsed);

typedef struct flubModuleCfg_s {
    const char *name;
    flubModuleInitCB_t init;
    flubModuleStart_t start;
    flubModuleUpdateCB_t update;
    flubModuleShutdown_t shutdown;
    char **dependencies;
} flubModuleCfg_t;


void flubModuleRegister(flubModuleCfg_t *module);
int flubModulesInit(appDefaults_t *defaults, int argc, char **argv);
int flubModulesStart(void);
void flubModulesShutdown(void);


#endif // _FLUB_MODULE_HEADER_
