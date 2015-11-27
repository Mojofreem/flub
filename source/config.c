#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <flub/physfsutil.h>
#include <SDL2/SDL.h>

#include <flub/data/critbit.h>
#include <flub/util/parse.h>
#include <flub/config.h>
#include <flub/logger.h>
#include <flub/cmdline.h>
#include <flub/memory.h>
#include <flub/module.h>

/////////////////////////////////////////////////////////////////////////////
// config module registration
/////////////////////////////////////////////////////////////////////////////

int flubCfgInit(appDefaults_t *defaults);
int flubCfgStart(void);
void flubCfgShutdown(void);

static char *_initDeps[] = {"logger", "cmdline", NULL};
flubModuleCfg_t flubModuleConfig = {
    .name = "config",
    .init = flubCfgInit,
    .start = flubCfgStart,
    .shutdown = flubCfgShutdown,
    .initDeps = _initDeps,
};

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

// TODO Config should be able to export subtrees, and filter substrees
//      This will help for logically splitting out certain functionality,
//      like key bindings, or game specific settings.


#define CFG_MAX_LINE_LEN    1024

#define DBG_CFG_DTL_GENERAL		1
#define DBG_CFG_DTL_VARS		2

flubLogDetailEntry_t _configDbg[] = {
        {"general", DBG_CFG_DTL_GENERAL},
        {"vars",    DBG_CFG_DTL_VARS},
        {NULL, 0},
};


typedef struct flubCfgOptNotifiee_s {
	cfgNotifyCB_t callback;
	struct flubCfgOptNotifiee_s *next;
} flubCfgOptNotifiee_t;

typedef struct flubCfgOpt_s {
	char *value;
	char *default_value;
	char *name;
	uint16_t flags;

	cfgValidateCB_t validate;
	flubCfgOptNotifiee_t *notifiees;
} flubCfgOpt_t;

typedef struct flubCfgPrefixNotifiee_s {
	char *prefix;
	int len;
	flubCfgOptNotifiee_t *notifiees;
	struct flubCfgPrefixNotifiee_s *next;
} flubCfgPrefixNotifiee_t;

struct {
    int init;
	critbit_t options;
	int dirty;
    appDefaults_t *app;
	int enum_in_progress;

	int server_mode;
	cfgServerRequestCB_t server_request;
	cfgNotifyCB_t server_notify;

	flubCfgPrefixNotifiee_t *prefix_notifiees;
	flubCfgOptNotifiee_t *all_notifiees;
} _flubCfgCtx = {
    .init = 0,
	.options = CRITBIT_TREE_INIT,
	.dirty = 0,
	.enum_in_progress = 0,
};

static int _flubCfgVarListEnumCB(void *ctx, const char *name, const char *value, const char *defValue, int flags) {
    printf("  %s = %s [%s]\n", name, value, defValue);

    return 1;
}

static eCmdLineStatus_t _flubCfgConfigListHandler(const char *name, const char *arg, void *context) {
    printf("Configuration variables:\n");

    flubCfgOptEnum(NULL, _flubCfgVarListEnumCB, NULL);

    return eCMDLINE_EXIT_SUCCESS;
}

int flubCfgInit(appDefaults_t *defaults) {
    if(_flubCfgCtx.init) {
        warning("Ignoring attempt to re-initialize the logger.");
        return 1;
    }

    logDebugRegisterList("config", DBG_CMDLINE, _configDbg);

    cmdlineOptAdd("config-list", 0, 0, NULL, "List all configuration variables",
                  _flubCfgConfigListHandler);

    _flubCfgCtx.app = defaults;

    _flubCfgCtx.init = 1;

    return 1;
}

int flubCfgStart(void) {
    const char *opt;

    opt = cmdlineGetAndRemoveOption("config", 'c', 1, "FILE",
                                    "specify the configuration option file", NULL,
                                    _flubCfgCtx.app->configFile);
    if(opt != NULL) {
        flubCfgLoad(opt);
    }
    debug(DBG_CORE, DBG_LOG_DTL_APP, "Configuration system started.");

    return 1;
}

int flubCfgValid(void) {
    return _flubCfgCtx.init;
}

void flubCfgShutdown(void) {
    if(!_flubCfgCtx.init) {
        return;
    }

    // TODO Free all config variables

    _flubCfgCtx.init = 0;

}

void flubCfgServerMode(int mode) {
	_flubCfgCtx.server_mode = mode;
}

int flubCfgIsServerMode(void) {
	return _flubCfgCtx.server_mode;
}

void flubCfgServerSetNotify(cfgNotifyCB_t notify) {
	_flubCfgCtx.server_notify = notify;
}

void flubCfgServerSetRequest(cfgServerRequestCB_t request) {
	_flubCfgCtx.server_request = request;
}

int flubCfgOptListAdd(flubCfgOptList_t *list) {
    int k;
    int result = 1;
    
    for(k = 0; list[k].name != NULL; k++) {
        if(!flubCfgOptAdd(list[k].name, list[k].def_value,
                          list[k].flags, list[k].validate)) {
            result = 0;
        }
    }
    return result;
}

int flubCfgOptListRemove(flubCfgOptList_t *list) {
    int k;
    int result = 1;
    
    for(k = 0; list[k].name != NULL; k++) {
        if(!flubCfgOptRemove(list[k].name)) {
            result = 0;
        }
    }
    return result;
}

int flubCfgOptAdd(const char *name, const char *default_value,
			      int flags, cfgValidateCB_t validate) {
    flubCfgOpt_t *option;
    int result = 0;
    void *data;
    
    if(!critbitContains(&_flubCfgCtx.options, name, &data)) {
        option = util_calloc(sizeof(flubCfgOpt_t), 0, NULL);
        option->default_value = util_strdup(default_value);
        option->flags = flags;
        option->validate = validate;
        
        if(critbitInsert(&_flubCfgCtx.options, name, option, NULL)) {
            result = 1;
        }
    }

    if(result) {
        debugf(DBG_CMDLINE, DBG_CFG_DTL_VARS, "Added config option \"%s\".", name);
    } else {
        infof("Failed to add existing config option \"%s\".", name);
    }
    return result;
}

static int _flubCfgRemoveCallback(void *data, void *extra) {
    flubCfgOpt_t *option = (flubCfgOpt_t *)data;
    flubCfgOptNotifiee_t *walk, *swap;
    
    if(data != NULL) {
        for(walk = option->notifiees; walk != NULL; walk = swap) {
            swap = walk->next;
            free( walk );
        }
        free(option->default_value);
        free(option);
    }
    return 1;
}

int flubCfgOptRemove(const char *name) {
    int result;
    
    result = critbitDelete(&_flubCfgCtx.options, name, _flubCfgRemoveCallback, NULL);
    
    if(!result) {
		errorf("Cannot remove nonexistent config option \"%s\".", name);
        return 0;
    }
    debugf(DBG_CMDLINE, DBG_CFG_DTL_VARS, "Removed config option \"%s\".", name);

    return result;
}

static void _flubCfgOptNotify(flubCfgOpt_t *option, const char *name,
                              const char *value) {
    flubCfgPrefixNotifiee_t *walkPrefix;
    flubCfgOptNotifiee_t *walkNotifiee;

    // Notify the option specific watchers first
    for(walkNotifiee = option->notifiees;
        walkNotifiee != NULL;
        walkNotifiee = walkNotifiee->next) {
        walkNotifiee->callback(name, value);
    }
    
    // Now notify all of the prefix watchers
    for(walkPrefix = _flubCfgCtx.prefix_notifiees;
        walkPrefix != NULL;
        walkPrefix = walkPrefix->next) {
        if(!strncasecmp(name, walkPrefix->prefix, walkPrefix->len)) {
            // This is a prefix match for the option name
            for(walkNotifiee = walkPrefix->notifiees;
                walkNotifiee != NULL;
                walkNotifiee = walkNotifiee->next) {
                walkNotifiee->callback(name, value);
            }
        }
    }
    
    // Now notify all "ALL" watchers
    for(walkNotifiee = _flubCfgCtx.all_notifiees;
        walkNotifiee != NULL;
        walkNotifiee = walkNotifiee->next) {
        walkNotifiee->callback(name, value);
    }
}

int flubCfgReset(const char *name) {
    return flubCfgOptStringSet(name, NULL, 0);
}

static int _flubCfgResetCallback(const char *name, void *data, void *extra) {
    flubCfgOpt_t *option = (flubCfgOpt_t *)data;
    
    if(option->value != NULL) {
    	flubCfgOptStringSet(name, NULL, 0);
    }
    
    return 1;
}

void flubCfgResetAll(const char *prefix) {
    critbitAllPrefixed(&_flubCfgCtx.options, ((prefix == NULL) ? "" : prefix),
                        _flubCfgResetCallback, NULL);

    debugf(DBG_CMDLINE, DBG_CFG_DTL_VARS, "Reset all config options to default values.");
}

int flubCfgLoad(const char *filename) {
	PHYSFS_File *fp;
	char buf[CFG_MAX_LINE_LEN], *ptr, *val, *tmp;

	if(!PHYSFS_exists(filename)) {
		warningf("Cannot find config file \"%s\".", filename);
		return 0;
	}

	if((fp = PHYSFS_openRead(filename)) == NULL) {
		errorf("Unable to open the config file \"%s\".", filename);
		return 0;
	}

	while(!PHYSFS_fixedEof(fp)) {
		if(PHYSFS_gets(buf, sizeof(buf), fp)) {
			// Skip preceeding whitespace
			for(ptr = buf; isspace(*ptr); ptr++);
			
			// Skip comments and blank lines
			if((*ptr == ';') || (*ptr == '#') || (*ptr == '\0') ||
			   ((*ptr == '/') && (*(ptr + 1) == '/'))) {
				continue;
			}
	
			// Find the equals sign
			if((val = strchr(ptr, '=')) == NULL) {
				// Not a valid NV pair
				continue;
			}
			
			if(val == ptr) {
				// No name was specified
				continue;
			}
	
			*val = '\0';
			val++;
	
			for(tmp = ptr; (*tmp != '\0') && (!isspace(*tmp)); tmp++);
			*tmp = '\0';
	
			for(tmp = val;
				(*tmp != '\0') && (*tmp != '\n') && (*tmp != '\r');
				tmp++);
			*tmp = '\0';
	
			// Set the NV pair value
			flubCfgOptStringSet(ptr, val, 0);
		}
	}

	PHYSFS_close(fp);

	return 1;
}

typedef struct flubCfgSaveCtx_s {
    FILE *fp;
    int all;
} flubCfgSaveCtx_t;

static int _flubCfgSaveCallback(const char *name, void *data, void *extra) {
    flubCfgSaveCtx_t *context = (flubCfgSaveCtx_t *)extra;
    flubCfgOpt_t *option = (flubCfgOpt_t *)data;
 
    if((option->value != NULL) || (context->all)) {
        fprintf(context->fp, "%s=%s\n", name,
                ((option->value == NULL) ? option->default_value : option->value));
    }

    return 1;
}

int flubCfgSave(const char *filename, int all) {
    flubCfgSaveCtx_t context;
	FILE *fp;
    
	if((fp = fopen(filename, "wt")) == NULL) {
		errorf("Unable to write the config file \"%s\".", filename);
		return 0;
	}

    context.fp = fp;
    context.all = all;

    critbitAllPrefixed(&_flubCfgCtx.options, "",
                        _flubCfgSaveCallback, &context);

	fclose(fp);

    debugf(DBG_CMDLINE, DBG_CFG_DTL_GENERAL, "Wrote config options to '%s'.\n", filename);

    return 1;
}

int flubCfgOptNotifieeAdd(const char *name, cfgNotifyCB_t callback) {
    flubCfgOpt_t *option;
    flubCfgOptNotifiee_t *notifiee;
    
    if(!critbitContains(&_flubCfgCtx.options, name, ((void *)&option))) {
		// The option was not found
		errorf("Cannot add notification to nonexistent config option \"%s\".", name);
        return 0;
    }

    for(notifiee = option->notifiees; notifiee != NULL; notifiee = notifiee->next) {
        if(notifiee->callback == callback) {
            break;
        }
    }
    if(notifiee == NULL) {
        // The notifiee does not already exist for the option
        notifiee = util_calloc(sizeof(flubCfgOptNotifiee_t), 0, NULL);
        notifiee->callback = callback;
        notifiee->next = option->notifiees;
        option->notifiees = notifiee;
    } else {
        notifiee = NULL;
    }
    
    if(notifiee) {
        debugf(DBG_CMDLINE, DBG_CFG_DTL_GENERAL, "Added notification to config option \"%s\".", name);
    } else {
        debugf(DBG_CMDLINE, DBG_CFG_DTL_GENERAL, "Failed to add existing notification to config option \"%s\".", name);
    }
    
    return 0;
}

int flubCfgAllNotifieeAdd(cfgNotifyCB_t callback) {
    flubCfgOptNotifiee_t *entry;
    
    for(entry = _flubCfgCtx.all_notifiees; entry != NULL; entry = entry->next) {
        if(entry->callback == callback) {
            // This callback is already registered.
            warning("Failed to add existing notification to all notifiee.");
            return 0;
        }
    }
    entry = util_calloc(sizeof(flubCfgOptNotifiee_t), 0, NULL);
    entry->callback = callback;
    entry->next = _flubCfgCtx.all_notifiees;
    _flubCfgCtx.all_notifiees = entry;

    debug(DBG_CMDLINE, DBG_CFG_DTL_GENERAL, "Added notification to all notifiee.");
    return 1;
}

int flubCfgPrefixNotifieeAdd(const char *prefix, cfgNotifyCB_t callback) {
    flubCfgOptNotifiee_t *notifiee;
    flubCfgPrefixNotifiee_t *entry;
    
    for(entry = _flubCfgCtx.prefix_notifiees; entry != NULL; entry = entry->next) {
        if(!strcasecmp(prefix, entry->prefix)) {
            // We found a matching prefix entry
            for(notifiee = entry->notifiees; notifiee != NULL; notifiee = notifiee->next) {
                if(notifiee->callback == callback) {
                    warningf("Failed to add existing notification to config prefix \"%s\".", prefix);
                    return 0;
                }
            }
            break;
        }
    }
    if(entry == NULL) {
        entry = util_calloc(sizeof(flubCfgPrefixNotifiee_t), 0, NULL);
        entry->prefix = util_strdup(prefix);
        entry->len = strlen(prefix);
        entry->next = _flubCfgCtx.prefix_notifiees;
        _flubCfgCtx.prefix_notifiees = entry;
    }
    notifiee = util_calloc(sizeof(flubCfgOptNotifiee_t), 0, NULL);
    notifiee->callback = callback;
    notifiee->next = entry->notifiees;
    entry->notifiees = notifiee;
    debugf(DBG_CMDLINE, DBG_CFG_DTL_GENERAL, "Added notification to config prefix \"%s\".", prefix);
    return 1;
}

int flubCfgOptNotifieeRemove(const char *name, cfgNotifyCB_t callback) {
    flubCfgOpt_t *option;
    flubCfgOptNotifiee_t *notifiee, *last = NULL;
    
    if(critbitContains(&_flubCfgCtx.options, name, ((void *)&option))) {
		// The option was not found
		errorf("Cannot remove notification to nonexistent config option \"%s\".", name);
        return 0;
    }

    for(notifiee = option->notifiees; notifiee != NULL; notifiee = notifiee->next) {
        if(notifiee->callback == callback) {
            if(last == NULL) {
                option->notifiees = notifiee->next;
            } else {
                last->next = notifiee->next;
            }
            free(notifiee);
            debugf(DBG_CMDLINE, DBG_CFG_DTL_VARS, "Removed notification to config option \"%s\".", name);
            
            return 1;
        }
        last = notifiee;
    }
    
    warningf("Failed to remove nonexistent notification to config option \"%s\".", name);

    return 0;
}

int flubCfgPrefixNotifieeRemove(const char *prefix, cfgNotifyCB_t callback) {
    flubCfgOptNotifiee_t *notifiee, *lastNotifiee = NULL;
    flubCfgPrefixNotifiee_t *entry, *lastPrefix = NULL;
    
    for(entry = _flubCfgCtx.prefix_notifiees; entry != NULL; entry = entry->next) {
        if(!strcasecmp(prefix, entry->prefix)) {
            // We found a matching prefix entry
            for(notifiee = entry->notifiees; notifiee != NULL; notifiee = notifiee->next) {
                if(notifiee->callback == callback) {
                    if(lastNotifiee == NULL) {
                        entry->notifiees = notifiee->next;
                    } else {
                        lastNotifiee->next = notifiee->next;
                    }
                    free(notifiee);
                    if(entry->notifiees == NULL) {
                        if(lastPrefix == NULL) {
                            _flubCfgCtx.prefix_notifiees = entry->next;
                        } else {
                            lastPrefix->next = entry->next;
                        }
                        free(entry->prefix);
                        free(entry);
                    }

                    debugf(DBG_CMDLINE, DBG_CFG_DTL_VARS, "Removed notification to config prefix \"%s\".", prefix);
                    return 1;
                }
                lastNotifiee = notifiee;
            }
            warningf("Failed to remove nonexistent notification to config prefix \"%s\".", prefix);
            return 0;
        }
        lastPrefix = entry;
    }
    warningf("Failed to remove nonexistent notification to nonexistent config prefix \"%s\".", prefix);
    return 0;
}

int flubCfgAllNotifieeRemove(cfgNotifyCB_t callback)
{
    flubCfgOptNotifiee_t *walk, *last = NULL;

    for(walk = _flubCfgCtx.all_notifiees; walk != NULL; walk = walk->next) {
        if(walk->callback == callback) {
            if(last != NULL) {
                last->next = walk->next;
            } else {
                _flubCfgCtx.all_notifiees = walk->next;
            }
            free(walk);
            return 1;
        }
        last = walk;
    }
    return 0;
}

typedef struct _flubCfgEnumCtx_s
{
    cfgEnumCB_t callback;
    void *arg;
} _flubCfgEnumCtx_t;

static int _flubCfgEnumCallback(const char *name, void *data, void *extra) {
    _flubCfgEnumCtx_t *context = (_flubCfgEnumCtx_t *)extra;
    flubCfgOpt_t *option = (flubCfgOpt_t *)data;
    int result;
    
    result = context->callback(context->arg,
    						   name,
                               ((option->value == NULL) ? option->default_value : option->value),
                               option->default_value,
                               option->flags);

    return result;
}

int flubCfgOptEnum(const char *prefix, cfgEnumCB_t callback, void *arg) {
    _flubCfgEnumCtx_t context;
    int result;
   
    context.callback = callback;
    context.arg = arg;

    result = critbitAllPrefixed(&_flubCfgCtx.options, ((prefix == NULL) ? "" : prefix),
                                _flubCfgEnumCallback, &context);

    return result;
}

int flubCfgOptStringSet(const char *name, const char *value, int from_server) {
    flubCfgOpt_t *option;

    debugf(DBG_CMDLINE, DBG_CFG_DTL_VARS, "Checking for option in critbit tree...");
	// Locate the option
    if(!critbitContains(&_flubCfgCtx.options, name, ((void *)&option))) {
		// The option was not found
		errorf("Cannot set nonexistent config option \"%s\".", name);
        return 0;
    }

    debugf(DBG_CMDLINE, DBG_CFG_DTL_VARS, "Checking for an actual change...");
    if(value == NULL) {
    	// Requesting a reset to the default value
    	if(option->value == NULL) {
    		// We're already the default value.
    		return 1;
    	}
    } else {
	    // Is this actually a change?
	    if(!strcmp(value, ((option->value == NULL) ? option->default_value : option->value))) {
	        // This is the same value we're currently set to.
	        return 1;
	    }
	}
    
    // If this is a client only option, make sure the server doesn't try to
    // set it, and if it does, lie to the server.
    if((option->flags & FLUB_CFG_FLAG_CLIENT) && from_server) {
    	warningf("Ignored server request to change client only var \"%s\" to \"%s\"", name, value);
        return 1;
    }

	// If this is a server locked option, and we're in server mode
	if((option->flags & FLUB_CFG_FLAG_SERVER) && _flubCfgCtx.server_request && _flubCfgCtx.server_mode) {
		// If the request was local
		if(!from_server) {
			// Send the request to the server
            debugf(DBG_CMDLINE, DBG_CFG_DTL_VARS, "Requesting server change for var \"%s\" to \"%s\"", name, value);
			_flubCfgCtx.server_request(name, value);
			return 0;
		}
		// Pass through since it is from the server
        debugf(DBG_CMDLINE, DBG_CFG_DTL_VARS, "Server set request for var \"%s\" to \"%s\"", name, value);
	}

	if(value != NULL)
	{
		// Attempt to validate and set the option
	    if((option->validate != NULL) && (!option->validate(name, value))) {
	        // The option value is not valid
	        if(from_server) {
	            // Craptastic. The value was set from the server. Since it is not
	            // valid, we cannot recover. This is so very bad.
	            errorf("Failed to validate server set config option \"%s\" to value \"%s\".", name, value);
	            // TODO At this point, we are no longer synchronized with the server
	            return 0;
	        }
	        errorf("Failed to validate set config option \"%s\" to value \"%s\".", name, value);
	        return 0;        
	    }
    }

    // Set the option value
    if(option->value != NULL) {
    	free(option->value);    	
    }
    option->value = NULL;
    
    if(value != NULL) {
	    if(strcmp(value, option->default_value)) {
    	    option->value = util_strdup(value);
    	}
   	}
    
    // Notify watchers
    _flubCfgOptNotify(option, name, value);

    debugf(DBG_CMDLINE, DBG_CFG_DTL_VARS, "Config option \"%s\" changed to \"%s\"%s", name, value,
           (from_server ? " from server" : ""));

	return 1;
}

const char *flubCfgOptStringGet(const char *name) {
	flubCfgOpt_t *option;
    
    if(!critbitContains(&_flubCfgCtx.options, name, ((void *)&option))) {
		// The option was not found
		errorf( "Cannot read nonexistent config option \"%s\".", name );
        return NULL;
    }
    return ((option->value == NULL) ? option->default_value : option->value);
}

int flubCfgOptIntSet(const char *name, int value, int from_server) {
	char tmp[32];

	snprintf(tmp, sizeof(tmp), "%d", value);
	tmp[sizeof(tmp) - 1] = '\0';

	return flubCfgOptStringSet(name, tmp, from_server);
}

int flubCfgOptIntGet(const char *name) {
	return utilIntFromString(flubCfgOptStringGet(name), 0);
}

int flubCfgOptBoolSet(const char *name, int value, int from_server) {
	if(value) {
		return flubCfgOptStringSet(name, "true", from_server);
	}
	return flubCfgOptStringSet(name, "false", from_server);
}

int flubCfgOptBoolGet(const char *name) {
	return utilBoolFromString(flubCfgOptStringGet(name), 0);
}

int flubCfgOptFloatSet(const char *name, float value, int from_server) {
	char tmp[32];

	snprintf(tmp, sizeof(tmp), "%f", value);
	tmp[sizeof(tmp) - 1] = '\0';

	return flubCfgOptStringSet(name, tmp, from_server);
}

float flubCfgOptFloatGet(const char *name) {
    return utilFloatFromString(flubCfgOptStringGet(name), 0.0);
}

