#include <flub/module.h>
#include <flub/memory.h>
#include <flub/logger.h>
#include <string.h>
#include <stdio.h>


#define DBG_CORE_DTL_MODULES    2

#define FLUB_MODULE_INIT    0x1
#define FLUB_MODULE_START   0x2
#define FLUB_MODULE_UPDATE  0x4
#define FLUB_MODULE_PRECEED 0x8

#define FLUB_MODULE_DEPS(cfg,flag)  ((flag == FLUB_MODULE_INIT) ? cfg->config.initDeps : ((flag == FLUB_MODULE_START) ? cfg->config.startDeps : ((flag == FLUB_MODULE_PRECEED) ? cfg->config.updatePreceed : cfg->config.updateDeps)))
#define FLUB_MODULE_DEPS_NAME(flag)  ((flag == FLUB_MODULE_INIT) ? "init" : ((flag == FLUB_MODULE_START) ? "start" : ((flag == FLUB_MODULE_PRECEED) ? "preceed" : "update")))


typedef struct flubModuleCfgEntry_s {
    flubModuleCfg_t config;
    int flags;
    int deps;
    int preceed;
    struct flubModuleCfgEntry_s *next;
    struct flubModuleCfgEntry_s *shutdownOrder;
} flubModuleCfgEntry_t;


struct {
    flubModuleCfgEntry_t *modules;
    flubModuleCfgEntry_t *shutdownOrder;
} _modulesCtx = {
    .modules = NULL,
    .shutdownOrder = NULL,
};


void flubModuleRegister(flubModuleCfg_t *module) {
    flubModuleCfgEntry_t *walk, *last = NULL;
    flubModuleCfgEntry_t *entry = util_calloc(sizeof(flubModuleCfgEntry_t), 0, NULL);

    memcpy(entry, module, sizeof(flubModuleCfg_t));
    if(_modulesCtx.modules == NULL) {
        _modulesCtx.modules = entry;
    } else {
        for(walk = _modulesCtx.modules; walk != NULL; last = walk, walk = walk->next);
        last->next = entry;
    }
}

static int _flubModulesCheckStrInArray(const char *str, char **array) {
    int k;

    if(array == NULL) {
        return 0;
    }
    for(k = 0; array[k] != NULL; k++) {
        if(!strcmp(str, array[k])) {
            return 1;
        }
    }
    return 0;
}

static int _flubModulesCalcDeps(int depsFlag) {
    int ready = 0;
    int dependent = 0;
    flubModuleCfgEntry_t *walk, *check;

    if(depsFlag & FLUB_MODULE_UPDATE) {
        for(walk = _modulesCtx.modules; walk != NULL; walk = walk->next) {
            if(walk->config.update == NULL) {
                walk->flags |= FLUB_MODULE_UPDATE;
            }
            walk->preceed = 0;
        }
    }

    for(walk = _modulesCtx.modules; walk != NULL; walk = walk->next) {
        walk->deps = 0;
        if(walk->flags & depsFlag) {
            continue;
        }
        for(check = _modulesCtx.modules; check != NULL; check = check->next) {
            if(check == walk) {
                continue;
            }
            if(check->flags & depsFlag) {
                continue;
            }
            if(_flubModulesCheckStrInArray(check->config.name,
                                           FLUB_MODULE_DEPS(walk, depsFlag))) {
                walk->deps++;
            }
            if((depsFlag & FLUB_MODULE_UPDATE) && (walk->config.updatePreceed != NULL)) {
                if(_flubModulesCheckStrInArray(check->config.name,
                                               walk->config.updatePreceed)) {
                    if(!(check->flags & FLUB_MODULE_UPDATE)) {
                        check->preceed++;
                    }
                }                
            }
        }
    }

    for(walk = _modulesCtx.modules; walk != NULL; walk = walk->next) {
        if(walk->deps == 0) {
            if(!(walk->flags & depsFlag)) {
                if(depsFlag & FLUB_MODULE_UPDATE) {
                    if(walk->preceed == 0) {
                        ready++;
                    }
                } else {
                    ready++;
                }
            }
        } else {
            dependent++;
        }
    }

    if(ready) {
        return 1;
    } else if(dependent) {
        return -1;
    } else {
        return 0;
    }
}

static flubModuleCfgEntry_t *_flubModulesGetNextReady(int depsFlag) {
    flubModuleCfgEntry_t *walk;

    for(walk = _modulesCtx.modules; walk != NULL; walk = walk->next) {
        if((walk->deps == 0) && (!(walk->flags & depsFlag))) {
            if((!(depsFlag & FLUB_MODULE_UPDATE)) || (walk->preceed == 0)) {
                return walk;
            }
        }
    }
    return NULL;
}

static int _flubIsModuleInited(const char *name, int depsFlag) {
    flubModuleCfgEntry_t *walk;

    for(walk = _modulesCtx.modules; walk != NULL; walk = walk->next) {
        if((!strcmp(name, walk->config.name)) && (walk->flags & depsFlag)) {
            return 1;
        }
    }
    return 0;
}

#define FLUB_MODULE_STATE_BUF_LEN   512

static void _flubModulesDumpState(int depsFlag) {
    char buf[FLUB_MODULE_STATE_BUF_LEN];
    int size = 0;
    int k;
    int valid = 0; //logValid();
    flubModuleCfgEntry_t *walk;

    buf[0] = '\0';
    buf[sizeof(buf) - 1] = '\0';

    if(logValid()) {
        infof("Flub module %s states:", FLUB_MODULE_DEPS_NAME(depsFlag));
    }
    
    for(walk = _modulesCtx.modules; walk != NULL; walk = walk->next) {
        if(FLUB_MODULE_DEPS(walk, depsFlag) == NULL) {
            if(logValid()) {
                infof("   Module: %s, deps: NONE", walk->config.name);                
            }
        } else {
            for(k = 0; FLUB_MODULE_DEPS(walk, depsFlag)[k] != NULL; k++) {
                snprintf(buf + size, sizeof(buf) - size - 2, "%s%s ",
                         (_flubIsModuleInited(walk->config.name, FLUB_MODULE_INIT) ? "*" : ""),
                         FLUB_MODULE_DEPS(walk, depsFlag)[k]);
                size = strlen(buf);
                if(size >= (sizeof(buf) - 1)) {
                    break;
                }
            }
            buf[sizeof(buf) - 1] = '\0';
            if(logValid()) {
                infof("   Module: %s, deps: %s", walk->config.name, buf);
            }
        }
    }
}

int flubModulesInit(appDefaults_t *defaults) {
    static int done = 0;
    static int logup = 0;
    int result;
    flubModuleCfgEntry_t *entry, *last = NULL;

    if(done) {
        return 1;
    }
    done = 1;

    debug(DBG_CORE, DBG_CORE_DTL_MODULES, "Bringing up flub modules...");
    while((result = _flubModulesCalcDeps(FLUB_MODULE_INIT)) > 0) {
        if((entry = _flubModulesGetNextReady(FLUB_MODULE_INIT)) != NULL) {
            if(logup) {
                debugf(DBG_CORE, DBG_CORE_DTL_MODULES, "Initializing module \"%s\"...", entry->config.name);
            }
            if(entry->config.init != NULL) {
                if(!entry->config.init(defaults)) {
                    return 0;
                }
                if(last == NULL) {
                    _modulesCtx.shutdownOrder = entry;
                    last = entry;
                } else {
                    last->shutdownOrder = entry;
                    last = entry;
                }
                if(logup) {
                    debugf(DBG_CORE, DBG_CORE_DTL_MODULES, "Module \"%s\" initialized.", entry->config.name);
                }
                entry->flags |= FLUB_MODULE_INIT;
                if((!logup) && logValid()) {
                    logDebugRegister("core", DBG_CORE, "modules", DBG_CORE_DTL_MODULES);
                    logup = 1;
                }
            }
        }
    }
    if(result < 0) {
        if(logup) {
            fatal("Unable to init flub modules.");
        }
        _flubModulesDumpState(FLUB_MODULE_INIT);
        return 0;
    }
    return 1;
}

int flubModulesStart(void) {
    static int done = 0;
    int result;
    flubModuleCfgEntry_t *entry;
    flubModuleCfgEntry_t *walk;

    if(done) {
        return 1;
    }
    done = 1;

    while((result = _flubModulesCalcDeps(FLUB_MODULE_START)) > 0) {
        if((entry = _flubModulesGetNextReady(FLUB_MODULE_START)) != NULL) {
            debugf(DBG_CORE, DBG_CORE_DTL_MODULES, "Starting module \"%s\"...", entry->config.name);
            if(entry->config.start != NULL) {
                if(!entry->config.start()) {
                    return 0;
                }
            }
            entry->flags |= FLUB_MODULE_START;
            debugf(DBG_CORE, DBG_CORE_DTL_MODULES, "Module \"%s\" started.", entry->config.name);
        }
    }
    if(result < 0) {
        fatal("Unable to start flub modules.");
        _flubModulesDumpState(FLUB_MODULE_START);
        return 0;
    }
    return 1;
}

void flubModulesShutdown(void) {
    static int done = 0;
    flubModuleCfgEntry_t *walk;

    if(done) {
        return;
    }
    done = 1;
    
    for(walk = _modulesCtx.shutdownOrder; walk != NULL; walk = walk->shutdownOrder) {
        if(walk->config.shutdown != NULL) {
            walk->config.shutdown();
        }
    }
}

flubModuleUpdateCB_t *flubModulesUpdateList(void) {
    int count = 0;
    int k = 0;
    int result;
    flubModuleCfgEntry_t *entry;
    flubModuleUpdateCB_t *list = NULL;

    for(entry = _modulesCtx.modules; entry != NULL; entry = entry->next) {
        if(entry->config.update != NULL) {
            count++;
        }
    }

    count++;
    list = util_calloc(sizeof(flubModuleUpdateCB_t) * count, 0, NULL);

    debug(DBG_CORE, DBG_CORE_DTL_MODULES, "Building flub module update list...");
    while((result = _flubModulesCalcDeps(FLUB_MODULE_UPDATE)) > 0) {
        if((entry = _flubModulesGetNextReady(FLUB_MODULE_UPDATE)) != NULL) {
            if(entry->config.update != NULL) {
                debugf(DBG_CORE, DBG_CORE_DTL_MODULES, "...(%d) + %s", k + 1, entry->config.name);
                list[k] = entry->config.update;
                k++;
            } else {
                debugf(DBG_CORE, DBG_CORE_DTL_MODULES, "...(-) - %s", entry->config.name);
            }
            entry->flags |= FLUB_MODULE_UPDATE;
        }
    }
    if(result < 0) {
        fatal("Unable to init flub modules.");
        _flubModulesDumpState(FLUB_MODULE_UPDATE);
        _flubModulesDumpState(FLUB_MODULE_PRECEED);
        return NULL;
    }

    list[k] = NULL;

    return list;
}

