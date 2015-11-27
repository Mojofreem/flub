#include <SDL2/SDL.h>
#include <flub/logger.h>
#include <flub/cmdline.h>
#include <flub/logger.h>
#include <flub/util/misc.h>
#include <flub/config.h>
#include <flub/memory.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#ifdef WIN32
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#	include <fcntl.h>
#	include <io.h>
#endif // WIN32
#include <flub/app.h>
#include <flub/flubconfig.h>


#define CMDLINE_OPTION_INDENT   4
#define CMDLINE_OPTION_LINE_LEN 79


/////////////////////////////////////////////////////////////////////////////
// cmdline module registration
/////////////////////////////////////////////////////////////////////////////

int flubCmdlineInit(appDefaults_t *defaults);
int flubCmdlineStart(void);
void flubCmdlineShutdown(void);

static char *_initDeps[] = {"logger", NULL};
static char *_startDeps[] = {"config", NULL};
flubModuleCfg_t flubModuleCmdline = {
	.name = "cmdline",
	.init = flubCmdlineInit,
	.start = flubCmdlineStart,
	.shutdown = flubCmdlineShutdown,
	.initDeps = _initDeps,
	.startDeps = _startDeps,
};

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

typedef struct cmdlineOptList_s {
	cmdlineOption_t option;
	const char *shortName;
	struct cmdlineOptList_s *next;
} cmdlineOptList_t;

struct {
    int init;
	int argc;
	char **argv;
	appDefaults_t *app;
	cmdlineOptList_t *options;
} _cmdlineCtx = {
        .init = 0,
		.argc = 0,
		.argv = NULL,
		.options = NULL,
	};


#define DBG_CMD_DTL_GENERAL		1
#define DBG_CMD_DTL_OPTIONS		2
#define DBG_CMD_DTL_PARSE		4


flubLogDetailEntry_t _cmdlineDbg[] = {
		{"general", DBG_CMD_DTL_GENERAL},
		{"options", DBG_CMD_DTL_OPTIONS},
		{"parse", DBG_CMD_DTL_PARSE},
		{NULL, 0},
	};


#ifdef WIN32
// maximum mumber of lines the output console should have
#define MAX_WIN32_CONSOLE_LINES	2000;

void RedirectIOToConsole(void) {
	CONSOLE_SCREEN_BUFFER_INFO coninfo;

	// allocate a console for this app
	AllocConsole();

	// set the screen buffer to be big enough to let us scroll text
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &coninfo);
	coninfo.dwSize.Y = MAX_WIN32_CONSOLE_LINES;
	SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), coninfo.dwSize);

    // Redirect the stdout, stdin, and stderr streams to the console
    freopen("CONIN$", "rb", stdin);
    freopen("CONOUT$", "wb", stdout);
    freopen("CONOUT$", "wb", stderr);

    // Set the stdout, stdin, and stderr streams to be unbuffered
	setvbuf(stdin, NULL, _IONBF, 0);
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
}
#endif // WIN32

static void _cmdlinePrintHelp(cmdlineOptList_t *option) {
    const int maxlinelen = CMDLINE_OPTION_LINE_LEN - CMDLINE_OPTION_INDENT;
    const char *ptr;
    const char *work;
    const char *lastspace;
    int inspace;

    if((option->option.name == NULL) && (option->shortName == NULL)) {
        return;
    }

    if(option->option.name != NULL) {
        printf("%s", option->option.name);
        if(option->option.meta != NULL) {
            printf(" <%s>", option->option.meta);
        }
    }
    if(option->shortName != NULL) {
        printf("%s%s", ((option->option.name != NULL) ? ", " : ""), option->shortName);
        if(option->option.meta != NULL) {
            printf(" <%s>", option->option.meta);
        }
    }
    printf("\n");

    if(option->option.help != NULL) {
        ptr = option->option.help;
        for(ptr = option->option.help; *ptr != '\0';) {
            for(; isspace(*ptr); ptr++);
            if(*ptr == '\0') {
                break;
            }
            inspace = 0;
            lastspace = NULL;
            for(work = ptr; ; work++) {
                if((*work == '\0') || ((work - ptr) >= maxlinelen)) {
                    if((*work == '\0') || (lastspace == NULL)) {
                        lastspace = work;
                    }
                    printf("    %.*s\n", (int)(lastspace - ptr), ptr);
                    ptr = lastspace;
                    break;
                }
                if(isspace(*work)) {
                    if(!inspace) {
                        lastspace = work;
                        inspace = 1;
                    }
                } else {
                    inspace = 0;
                }
            }
        }
    }
}

static eCmdLineStatus_t _cmdlineHelpHandler(const char *name, const char *arg, void *context) {
    cmdlineOptList_t *walk;

    printf("%s [OPTIONS] [<+VAR=VALUE>, ...]%s%s\n\n", appName(),
           ((appDefaults.cmdlineParamStr == NULL) ? "" : " "),
           appDefaults.cmdlineParamStr);

    for(walk = _cmdlineCtx.options; walk != NULL; walk = walk->next) {
        _cmdlinePrintHelp(walk);
    }
    return eCMDLINE_EXIT_SUCCESS;
}

static eCmdLineStatus_t _cmdlineVersionHandler(const char *name, const char *arg, void *context) {
	printf("%s version %d.%d\n", appDefaults.title, appDefaults.major, appDefaults.minor);
    printf("Flub version %s\n", FLUB_VERSION_STRING);

    return eCMDLINE_EXIT_SUCCESS;
}

void flubCmdlineShutdown(void) {
    if(!_cmdlineCtx.init) {
        return;
    }

    // TODO Free up command line option lists

	debug(DBG_CMDLINE, DBG_CMD_DTL_GENERAL, "Shutting down Flub command line module");

    _cmdlineCtx.init = 0;
}

int flubCmdlineInit(appDefaults_t *defaults) {
	const char *opt;

    if(_cmdlineCtx.init) {
        warning("Ignoring attempt to re-initialize the command line.");
        return 1;
    }

    if(!logValid()) {
        // The logger has not been initialized!
        return 0;
    }

	logDebugRegisterList("command", DBG_CMDLINE, _cmdlineDbg);

	_cmdlineCtx.argc = defaults->argc;
	_cmdlineCtx.argv = defaults->argv;
	_cmdlineCtx.app = defaults;

    opt = cmdlineGetAndRemoveOption("logconsole", 0, 0, NULL, "enable logging to console shell", "true", "false");
	if(!strcmp(opt, "true")) {
#ifdef WIN32
        if(!appLaunchedFromConsole()) {
            RedirectIOToConsole();
        }
#endif
        logStdoutInit();
#ifdef WIN32
	} else {
		if(!appLaunchedFromConsole()) {
            FreeConsole();
        }
#endif // WIN32
	}

    opt = cmdlineGetAndRemoveOption("loglevel", 0, 1, "LEVEL", "Set the log output level (DEBUG, INFO, WARNING, ERROR, FATAL)", NULL, "warning");
    logLevelSetStr(opt);

    for(opt = cmdlineGetAndRemoveOption("debug", 0, 1, "MODULE+DETAIL", "Configure debug output levels (ALL references all entries)", NULL, NULL);
        opt != NULL;
        opt = cmdlineGetAndRemoveOption("debug", 0, 1, NULL, NULL, NULL, NULL)) {
        logDebugSetByCmdline(opt);
    }

    opt = cmdlineGetAndRemoveOption("logfile", 'l', 1, "FILE", "Save log output to a file", NULL, NULL);
	if(opt != NULL) {
		logFileInit(opt);
	}

    opt = cmdlineGetAndRemoveOption("debuglines", 0, 0, NULL, "Show source code line number in log output", "true", NULL);
    if(opt != NULL) {
        logDebugLineNum(1);
    }

    opt = cmdlineGetAndRemoveOption("verbose", 'v', 0, NULL, "Use SDL debug level logging", "true", NULL);
    if(opt != NULL) {
        SDL_LogSetAllPriority(SDL_LOG_PRIORITY_DEBUG);
        debugf(DBG_CORE, DBG_CMD_DTL_GENERAL, "Using SDL debugging logs.");
    } else {
        SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_WARN);
    }

	debug(DBG_CMDLINE, DBG_CMD_DTL_GENERAL, "Initializing Flub command line module");

	// Add handler for version and usage.
    cmdlineOptAdd("version", 0, 0, NULL, "Display the Flub version number", _cmdlineVersionHandler);
    cmdlineOptAdd("help", '?', 0, NULL, "Display the command line help, and exit", _cmdlineHelpHandler);

#ifdef FLUB_DEBUG
    logCmdlineOptRegister();
#endif // FLUB_DEBUG

    _cmdlineCtx.init = 1;

	return 1;
}

int flubCmdlineStart(void) {
    eCmdLineStatus_t status;

	status = cmdlineProcess(appDefaults.cmdlineHandler, _cmdlineCtx.app->cmdlineContext);
	if(status != eCMDLINE_OK) {
		// TODO Bubble up the actual command line status
		return 0;
	}
    debug(DBG_CORE, DBG_LOG_DTL_APP, "Command line parser completed.");
    return 1;
}

static int _checkOptionName(const char *name) {
	// Check that string consists of at least one alphanumeric, and no other
	// characters except the dash.
	int k;
	int foundAlnum = 0;

	for(k = 0; name[k] != '\0'; k++) {
		if(isspace(name[k])) {
			return 0;
		}
		if(isalnum(name[k])) {
			foundAlnum = 1;
		} else {
			if(name[k] != '-') {
				return 0;
			}
		}
	}
	if((k == 0) || (!foundAlnum)) {
		return 0;
	}
	return 1;
}

static const char *cmdlineOptionDefIsValid(cmdlineOption_t *opt) {
	int k;
	int valInvalid = 0;
	int nameInvalid = 0;
	
	if(opt->val > 0) {
		if(isspace(opt->val) || ((!isalnum(opt->val)) && (opt->val != '-') && (opt->val != '?'))) {
			valInvalid = 1;
		}
	}
	
	if(opt->name == NULL) {
		if(opt->val <= 0) {
			return "no name defined";
		}
		if(!_checkOptionName(opt->name)) {
			nameInvalid = 1;
		}
	}
	if(nameInvalid && valInvalid) {
		return "no valid name";
	}
	if(nameInvalid) {
		return "invalid long name";
	}
	if(valInvalid) {
		return "invalid short name";
	}
	if(opt->handler == NULL) {
		// no handler defined, we allow this for tracking getAndRemove... calls
	}
	return NULL;
}

static const char *cmdlineOptionDefName(cmdlineOption_t *opt) {
	static char name[64];
	int k;
	
	if((opt->name == NULL) || (opt->name[0] == '\0')) {
		if((opt->val <= 0) || (!isalnum(opt->val))) {
			strcpy(name, "with no name");
		} else {
			snprintf(name, sizeof(name), "\"-%c\"", opt->val);
		}
	} else {
		snprintf(name, sizeof(name), "\"--%s\"", opt->name);
	}
	name[sizeof(name) - 1] = '\0';
	
	return name;
}

static const char *cmdlineOptionListName(cmdlineOptList_t *opt) {
	static char name[64];
	int k;
	
	if(opt->option.name == NULL) {
		snprintf(name, sizeof(name), "\"-%c\"", opt->option.val);
	} else {
		snprintf(name, sizeof(name), "\"--%s\"", opt->option.name);
		if(opt->option.val != 0) {
			k = strlen(name);
			if((sizeof(name) - k) > 10) { // _OR_"--x"\0
				snprintf(name + k, sizeof(name) - k, " OR \"-%c\"", opt->option.val);
			}
		}
	}
	name[sizeof(name) - 1] = '\0';
	
	return name;
}

void cmdlineOptAdd(const char *name, int val, int has_arg, const char *meta, const char *help,
				   cmdlineOptHandler_t handler) {
	cmdlineOption_t entry;
	
	memset(&entry, 0, sizeof(cmdlineOption_t));
	entry.name = name;
	entry.val = val;
	entry.has_arg = has_arg;
    entry.meta = meta;
	entry.help = help;
	entry.handler = handler;

	cmdlineOptEntryAdd(&entry);
}

void cmdlineOptEntryAdd(cmdlineOption_t *opt) {
	char name[64];
	cmdlineOptList_t *entry;
	const char *reason;

	if((reason = cmdlineOptionDefIsValid(opt)) != NULL) {
		errorf("Failed to register command line option %s due to %s.",
			   cmdlineOptionDefName(opt), reason);
		return;
	}

	entry = util_calloc(sizeof(cmdlineOptList_t), 0, NULL);
	if((opt->name == NULL) || (opt->name[0] == '\0')) {
		entry->option.name = NULL;
	} else {
		snprintf(name, sizeof(name), "--%s", opt->name);
		entry->option.name = util_strdup(name);
	}
	if(opt->val <= 0) {
		entry->shortName = NULL;
	} else {
		snprintf(name, sizeof(name), "-%c", opt->val);
		entry->shortName = util_strdup(name);
	}
    entry->option.val = opt->val;
	entry->option.has_arg = opt->has_arg;
    entry->option.meta = ((opt->meta == NULL) ? NULL : util_strdup(opt->meta));
	entry->option.help = ((opt->help == NULL) ? NULL : util_strdup(opt->help));
	entry->option.handler = opt->handler;
	entry->next = _cmdlineCtx.options;
	
	_cmdlineCtx.options = entry;

	if(isprint(opt->val)) {
        debugf(DBG_CMDLINE, DBG_CMD_DTL_OPTIONS, "Added option \"%s\", '-%c'%s",
               entry->option.name, opt->val, (opt->has_arg ? " with argument" : " without argument"));
	} else {
        debugf(DBG_CMDLINE, DBG_CMD_DTL_OPTIONS, "Added option \"%s\"%s", entry->option.name,
               (opt->has_arg ? " with argument" : " without argument"));
	}
}

void cmdlineOptListAdd(cmdlineOption_t *opts) {
	int k;
	
	for(k = 0; (!((opts[k].name == NULL) && (opts[k].val == 0))); k++) {
		cmdlineOptEntryAdd(opts + k);
	}
}

static int cmdlineCheckOptArg(int pos) {
	pos++;

	if(pos >= _cmdlineCtx.argc) {
		return 0;
	}

	if((_cmdlineCtx.argv[pos][0] == '-') ||
	   (_cmdlineCtx.argv[pos][0] == '+')) {
		return 0;
	}

	return 1;
}

static void cmdlineRemoveArg(int pos) {
	int k;

	if((pos >= _cmdlineCtx.argc) || (pos < 0)) {
		return;
	}

	for(k = pos; k < (_cmdlineCtx.argc - 1); k++) {
		_cmdlineCtx.argv[k] = _cmdlineCtx.argv[k + 1];
	}
	_cmdlineCtx.argc--;
}

const char *cmdlineGetAndRemoveOption(const char *name, int val,
                                      int has_arg,
                                      const char *meta, const char *help,
                                      const char *optfound,
                                      const char *optnotfound) {
	int k;
	char *opt;
	char tmp[64];

	if(isprint(val)) {
		debugf(DBG_CMDLINE, DBG_CMD_DTL_PARSE, "Checking for option \"%s\" (-%c) with%s argument", name, val, (has_arg ? "" : "out"));
	} else {
		debugf(DBG_CMDLINE, DBG_CMD_DTL_PARSE, "Checking for option \"%s\" with%s argument", name, (has_arg ? "" : "out"));
	}

    if(help != NULL) {
        cmdlineOptAdd(name, val, has_arg, meta, help, NULL);
    } else {
        debugf(DBG_CMDLINE, DBG_CMD_DTL_PARSE, "Not adding option \"%s\"", name);
    }

	for(k = 1; k < _cmdlineCtx.argc; k++) {
		snprintf(tmp, sizeof(tmp), "--%s", name);
		tmp[sizeof(tmp) - 1] = '\0';
		if(strcmp(_cmdlineCtx.argv[k], tmp)) {
			if(!isalnum(val)) {
				continue;
			} else {
				snprintf(tmp, sizeof(tmp), "-%c", val);
				tmp[sizeof(tmp) - 1] = '\0';
				if(strcmp(_cmdlineCtx.argv[k], tmp)) {
					continue;
				}				
			}
		}

		if(!has_arg) {
			cmdlineRemoveArg(k);
			debugf(DBG_CMDLINE, DBG_CMD_DTL_PARSE, "Found option \"%s\"", name);
			return optfound;
		}
		if(!cmdlineCheckOptArg(k)) {
			// No option parameter was specified!
			debugf(DBG_CMDLINE, DBG_CMD_DTL_PARSE, "Option \"%s\" was present, but missing expected argument", name);
			cmdlineRemoveArg( k );			
		} else {
			// Option parameter was specified
			opt = _cmdlineCtx.argv[k + 1];
			cmdlineRemoveArg(k + 1);
			cmdlineRemoveArg(k);
			debugf(DBG_CMDLINE, DBG_CMD_DTL_PARSE, "Option \"%s\" was present with argument \"%s\"", name, opt);
			return opt;
		}
	}
	return optnotfound;
}

eCmdLineStatus_t cmdlineProcess(cmdlineDefHandler_t handler, void *context) {
	int k;
	const char *name;
	char *value;
	char tmp[512];
	cmdlineOptList_t *entry;
	int found;
	eCmdLineStatus_t result = eCMDLINE_OK;

    debugf(DBG_CMDLINE, DBG_CMD_DTL_PARSE, "Processing command line.");
	for(k = 1; k < _cmdlineCtx.argc; k++) {
		switch(_cmdlineCtx.argv[ k ][ 0 ]) {
			case '+':
				// Setting a configuration value
				name = &_cmdlineCtx.argv[k][1];
				if((value = strchr(name, '=')) == NULL) {
					warningf("No value specified for config option "
						   	 "'%s' on command line.", name);
					break;
				}
				if(name == value) {
					warning("No name specified for config option on "
							"command line.");
					break;
				}
				memcpy(tmp, &_cmdlineCtx.argv[k][1], sizeof(tmp) - 1);
				tmp[sizeof(tmp) - 1] = '\0';
				name = tmp;
				if((value = strchr(name, '=')) == NULL) {
					warningf("Config option name exceeds maximum "
							 "size (%d) on command line.",
							 sizeof(tmp));
					break;
				}
				value[0] = '\0';
				value++;
                debugf(DBG_CMDLINE, DBG_CMD_DTL_PARSE, "Attempting to set config option \"%s\" to \"%s\".", name, value);
				if(!flubCfgOptStringSet(name, value, 0))
				{
					warningf("Failed to set config option '%s' from "
					         "command line.\n", name);
				}
				break;
			case '-':
				found = 0;
				name = NULL;
				for(entry = _cmdlineCtx.options; entry != NULL; entry = entry->next )
				{
					if((entry->option.name != NULL) && (!strcmp(_cmdlineCtx.argv[k], entry->option.name))) {
						name = entry->option.name + 2;
						found = 1;
					} else if((entry->shortName != NULL) && (!strcmp(_cmdlineCtx.argv[k], entry->shortName))) {
						name = entry->shortName + 1;
						found = 1;
					} else {
						continue;
					}

					// We found a matching option
					if(!entry->option.has_arg) {
                        if(entry->option.handler != NULL) {
                            result = entry->option.handler(name, NULL, context);
                        }
					} else {
						if(cmdlineCheckOptArg(k)) {
                            if(entry->option.handler != NULL) {
                                result = entry->option.handler(name, _cmdlineCtx.argv[k + 1], context);
                            } else {
                                infof("No handler specified for %s", entry->option.name);
                            }
							k++;
						} else {
							warningf("Not enough arguments for option %s "
									 "on the command line.\n",
									 cmdlineOptionListName(entry));
						}
					}
					break;
				}
				if(found) {
					if(result != eCMDLINE_OK) {
						return result;
					}
				} else {
					errorf("Skipping unknown command line option \"%s\".",
						   _cmdlineCtx.argv[k]);
				}
				break;
			default:
				debugf(DBG_CMDLINE, DBG_CMD_DTL_PARSE, "Passing command line parameter \"%s\" to default handler", _cmdlineCtx.argv[k]);
				result = handler(_cmdlineCtx.argv[k], context);
				break;
		}
	}
    debug(DBG_CMDLINE, DBG_CMD_DTL_GENERAL, "Done processing command line.");
	
	return result;
}
