#include <flub/module.h>
#include <flub/memory.h>
#include <flub/logger.h>


#define DBG_CORE_DTL_MODULES    2


typedef struct flubModuleCfgEntry_s {
    flubModuleCfg_t config;
    int inited;
    int deps;
    struct flubModuleCfgEntry_s *next;
    struct flubModuleCfgEntry_s *shutdownOrder;
} flubModuleCfgEntry_t;


struct {
    int init;
} _modulesCtx = {
    .init = 0,
};

static flubModuleCfgEntry_t *_flubRegisteredModules = NULL;
static flubModuleCfgEntry_t *_flubModuleShutdownOrder = NULL;


static void _flubModulesInit(void) {
    if(!_modulesCtx.init) {
        logDebugRegister("core", DBG_CORE, "modules", DBG_CORE_DTL_MODULES);
        _modulesCtx.init = 1;
    }
}

void flubModuleRegister(flubModuleCfg_t *module) {
    flubModuleCfgEntry_t *walk, *last = NULL;
    flubModuleCfgEntry_t *entry = util_calloc(sizeof(flubModuleCfgEntry_t), 0, NULL);

    memcpy(entry, module, sizeof(flubModuleCfg_t));
    if(_flubRegisteredModules == NULL) {
        _flubRegisteredModules = entry;
    } else {
        for(walk = _flubRegisteredModules; walk != NULL; last = walk, walk = walk->next);
        last->next = entry;
    }
}

static int _flubModulesCheckStrInArray(const char *str, char **array) {
    int k;

    for(k = 0; array[k] != NULL; k++) {
        if(!strcmp(str, array[k])) {
            return 1;
        }
    }
    return 0;
}

static int _flubModulesCalcDeps(void) {
    int ready = 0;
    int dependent = 0;
    flubModuleCfgEntry_t *walk, *check;

    for(walk = _flubRegisteredModules; walk != NULL; walk = walk->next) {
        walk->deps = 0;
        if(walk->inited) {
            continue;
        }
        if(walk->config.dependencies == NULL) {
            ready++;
            continue;
        }
        for(check = _flubRegisteredModules; check != NULL; check = check->next) {
            if(_flubModulesCheckStrInArray(check->config.name, walk->config.dependencies)) {
                walk->deps++;
            }
        }
        if(walk->deps == 0) {
            ready++;
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

static flubModuleCfgEntry_t *_flubModulesGetNextReady(void) {
    flubModuleCfgEntry_t *walk;

    for(walk = _flubRegisteredModules; walk != NULL; walk = walk->next) {
        if(walk->deps == 0) {
            return walk;
        }
    }
    return NULL;
}

static int _flubIsModuleInited(const char *name) {
    flubModuleCfgEntry_t *walk;

    for(walk = _flubRegisteredModules; walk != NULL; walk = walk->next) {
        if(walk->inited) {
            return 1;
        }
    }
    return 0;
}

#define FLUB_MODULE_STATE_BUF_LEN   512

static void _flubModulesDumpState(void) {
    char buf[FLUB_MODULE_STATE_BUF_LEN];
    int size = 0;
    int k;
    flubModuleCfgEntry_t *walk;

    buf[0] = '\0';
    buf[sizeof(buf) - 1] = '\0';

    infof("Flub module init states:");
    for(walk = _flubRegisteredModules; walk != NULL; walk = walk->next) {
        if(walk->config.dependencies == NULL) {
            infof("   Module: %s, deps: NONE", walk->config.name);
        } else {
            for(k = 0; walk->config.dependencies[k] != NULL; k++) {
                snprintf(buf + size, sizeof(buf) - size - 2, "%s%s ",
                         (_flubIsModuleInited ? "*" : ""),
                         walk->config.dependencies[k]);
                size = strlen(buf);
            }
            infof("   module: %s, deps: %s", walk->config.name, buf);
        }
    }
}

int flubModulesInit(appDefaults_t *defaults, int argc, char **argv) {
    static int done = 0;
    int result;
    flubModuleCfgEntry_t *entry, *last = NULL;

    if(done) {
        return 0;
    }
    done = 1;

    _flubModulesInit();

    while((result = _flubModulesCalcDeps()) > 0) {
        entry = _flubModulesGetNextReady();
        debugf(DBG_CORE, DBG_CORE_DTL_MODULES, "Initializing module \"%s\"...", entry->config.name);
        if(entry->config.init != NULL) {
            if(!entry->config.init(argc, argv, defaults)) {
                return 0;
            }
            if(last == NULL) {
                _flubModuleShutdownOrder = entry;
                last = entry;
            } else {
                last->shutdownOrder = entry;
                last = entry;
            }
            debugf(DBG_CORE, DBG_CORE_DTL_MODULES, "Module \"%s\" initialized.", entry->config.name);
            entry->inited = 1;
        }
    }
    if(result < 0) {
        fatal("Unable to init flub modules.");
        _flubModulesDumpState();
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
        return 0;
    }
    done = 1;

    for(walk = _flubRegisteredModules; walk != NULL; walk = walk->next) {
        walk->inited = 0;
    }

    while((result = _flubModulesCalcDeps()) > 0) {
        if((entry = _flubModulesGetNextReady()) != NULL) {
            debugf(DBG_CORE, DBG_CORE_DTL_MODULES, "Starting module \"%s\"...", entry->config.name);
            if(entry->config.start != NULL) {
                if(!entry->config.start()) {
                    return 0;
                }
                entry->inited = 1;
            }
            debugf(DBG_CORE, DBG_CORE_DTL_MODULES, "Module \"%s\" started.", entry->config.name);
        }
    }
    if(result < 0) {
        fatal("Unable to start flub modules.");
        _flubModulesDumpState();
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
    
    for(walk = _flubModuleShutdownOrder; walk != NULL; walk = walk->shutdownOrder) {
        if(walk->config.shutdown != NULL) {
            walk->config.shutdown();
        }
    }
}
