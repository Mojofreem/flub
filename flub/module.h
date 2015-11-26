#ifndef _FLUB_MODULE_HEADER_
#define _FLUB_MODULE_HEADER_


#include <flub/app.h>
#include <stdint.h>


typedef int (*flubModuleInitCB_t)(appDefaults_t *appDefaults);
typedef int (*flubModuleStart_t)(void);
typedef void (*flubModuleShutdown_t)(void);
typedef int (*flubModuleUpdateCB_t)(uint32_t ticks, uint32_t elapsed);

typedef struct flubModuleCfg_s {
    const char *name;
    flubModuleInitCB_t init;
    flubModuleStart_t start;
    flubModuleUpdateCB_t update;
    flubModuleShutdown_t shutdown;
    char **initDeps;
    char **updateDeps;
    char **updatePreceed;
    char **startDeps;
} flubModuleCfg_t;


void flubModuleRegister(flubModuleCfg_t *module);
int flubModulesInit(appDefaults_t *defaults);
int flubModulesStart(void);
void flubModulesShutdown(void);
flubModuleUpdateCB_t *flubModulesUpdateList(void);


#endif // _FLUB_MODULE_HEADER_
