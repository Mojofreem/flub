#ifndef _FLUB_CONFIG_HEADER_
#define _FLUB_CONFIG_HEADER_


#include <flub/app.h>


#define FLUB_CFG_FLAG_DIRTY		0x1
#define FLUB_CFG_FLAG_CLIENT    0x2
#define FLUB_CFG_FLAG_SERVER    0x4


typedef int (*cfgValidateCB_t)(const char *name, const char *value);
typedef int (*cfgEnumCB_t)(void *ctx, const char *name, const char *value,
                           const char *defValue, int flags);
typedef void (*cfgNotifyCB_t)(const char *name, const char *value);
typedef void (*cfgServerRequestCB_t)(const char *name, const char *value);


typedef struct flubCfgOptList_s {
    const char *name;
    const char *def_value;
    int flags;
    cfgValidateCB_t validate;
} flubCfgOptList_t;


#define FLUB_CFG_OPT_LIST_END {NULL, NULL, 0, NULL}


int flubCfgInit(appDefaults_t *defaults);
int flubCfgValid(void);
void flubCfgShutdown(void);

void flubCfgServerMode(int mode);
int flubCfgIsServerMode(void);
void flubCfgServerSetNotify(cfgNotifyCB_t notify);
void flubCfgServerSetRequest(cfgServerRequestCB_t request);

int flubCfgOptListAdd(flubCfgOptList_t *list);
int flubCfgOptListRemove(flubCfgOptList_t *list);
int flubCfgOptAdd(const char *name, const char *default_value,
			      int flags, cfgValidateCB_t validate);
int flubCfgOptRemove(const char *name);

int flubCfgReset(const char *name);
void flubCfgResetAll(const char *prefix);

int flubCfgLoad(const char *filename);
int flubCfgSave(const char *filename, int all);

int flubCfgOptNotifieeAdd(const char *name, cfgNotifyCB_t callback);
int flubCfgAllNotifieeAdd(cfgNotifyCB_t callback);
int flubCfgPrefixNotifieeAdd(const char *prefix, cfgNotifyCB_t callback);
int flubCfgOptNotifieeRemove(const char *name, cfgNotifyCB_t callback);
int flubCfgPrefixNotifieeRemove(const char *prefix, cfgNotifyCB_t callback);
int flubCfgAllNotifieeRemove(cfgNotifyCB_t callback);

int flubCfgOptEnum(const char *prefix, cfgEnumCB_t callback, void *arg);

int flubCfgOptStringSet(const char *name, const char *value, int from_server);
const char *flubCfgOptStringGet(const char *name);

int flubCfgOptIntSet(const char *name, int value, int from_server);
int flubCfgOptIntGet(const char *name);

int flubCfgOptBoolSet(const char *name, int value, int from_server);
int flubCfgOptBoolGet(const char *name);

int flubCfgOptFloatSet(const char *name, float value, int from_server);
float flubCfgOptFloatGet(const char *name);


#endif // _FLUB_CONFIG_HEADER_
