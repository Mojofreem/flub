#include <SDL2/SDL.h>
#include <flub/logger.h>
#include <flub/thread.h>
//#include <flub/util.h>
#include <flub/memory.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <stdarg.h>
#ifdef WIN32
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#endif // WIN32
#include <flub/cmdline.h>


#define LOG_DBG_PLACEHOLDER 256

flubLogDetailEntry_t _loggerDbg[] = {
        {"general", DBG_LOG_DTL_LOGGER},
        {"physfs",  DBG_LOG_DTL_PHYSFS},
        {"app",     DBG_LOG_DTL_APP},
        {NULL, 0},
    };


typedef struct logNotifiee_s
{
    logCallback_t callback;
    struct logNotifiee_s *next;
} logNotifiee_t;

#ifdef FLUB_DEBUG

typedef struct logDebugDetail_s
{
    char *str;
    int number;
    struct logDebugDetail_s *next;
} logDebugDetail_t;

typedef struct logDebugModule_s
{
    char *str;
    int number;
    int setAll;
    logDebugDetail_t *details;
    struct logDebugModule_s *next;
} logDebugModule_t;


typedef struct _logDebugEntry_s {
    char *module;
    char *detail;
    struct _logDebugEntry_s *next;
} _logDebugEntry_t;

#endif // FLUB_DEBUG

struct
{
    int init;
    eLogLevel_t level;
    mutex_t *mutex;
    char timestamp[LOG_MAX_TIMESTAMP_SIZE];
    char buffer[LOG_MAX_MSG_SIZE];
    logMessage_t msgStruct;
    logNotifiee_t *notifiees;
    int inProgress;
#ifdef FLUB_DEBUG
    int linenums;
    logDebugModule_t *debugDetails;
    int debugMode;

    // Debug settings (when set prior to declaration)
    _logDebugEntry_t *debugSettings;
#endif // FLUB_DEBUG
    
    // log file
    FILE *fp;
    
    // log standard out
    int toStdout;

    // SDL redirect handle
    SDL_LogOutputFunction redirectSDL;

} g_logCtx = {
    .init = 0,
    .level = eLogWarning,
    .mutex = NULL,
    .notifiees = NULL,
    .inProgress = 0,
#ifdef FLUB_DEBUG
    .linenums = 0,
    .debugDetails = NULL,
    .debugMode = 0,
#endif // FLUB_DEBUG
    .fp = NULL,
    .toStdout = 0,
    .redirectSDL = NULL,
};

#ifdef FLUB_DEBUG
unsigned char g_LogDebugMask[256];
#endif // FLUB_DEBUG

#ifdef WIN32
void logWin32FatalCallback(const logMessage_t *msg) {
    if(msg->level == eLogFatal) {
        MessageBox(NULL, msg->message, "FATAL ERROR", MB_OK | MB_ICONEXCLAMATION);
    }
}
#endif // WIN32

int logInit(void) {
    int k;

    if(g_logCtx.init) {
        warning("Ignoring attempt to re-initialize the logger.");
        return 1;
    }

    g_logCtx.mutex = mutexCreate();
#ifdef FLUB_DEBUG
    logDebugClearAll();
#endif // FLUB_DEBUG
    g_logCtx.msgStruct.timestamp = g_logCtx.timestamp;
    g_logCtx.msgStruct.message = g_logCtx.buffer;
    g_logCtx.inProgress = 0;

    // Initialize the logging subsystem
    logDebugRegisterList("core", DBG_CORE, _loggerDbg);

#ifdef WIN32
    logAddNotifiee(logWin32FatalCallback);
#endif

    g_logCtx.init = 1;

    return 1;
}

int logValid(void) {
    return g_logCtx.init;
}

void logShutdown(void) {
    logNotifiee_t *walk, *next;

    mutexGrab(g_logCtx.mutex);
    g_logCtx.init = 0;

    logStdoutShutdown();
    logFileShutdown();
    logStopRedirectSDLLogs();

    for(next = NULL, walk = g_logCtx.notifiees; walk != NULL; walk = next) {
        next = walk->next;
        util_free(walk);
    }
    g_logCtx.notifiees = NULL;
    mutexDestroy(g_logCtx.mutex);
    g_logCtx.mutex = NULL;
}

void logLevelSet(eLogLevel_t level) {
    mutexGrab(g_logCtx.mutex);
    g_logCtx.level = level;
    mutexRelease(g_logCtx.mutex);
}

void logLevelSetStr(const char *str) {
    struct {
        char *str;
        eLogLevel_t level;
    } levels[] = {
        { "dbg", eLogDebug },
        { "debug", eLogDebug },
        { "inf", eLogInfo },
        { "info", eLogInfo },
        { "wrn", eLogWarning },
        { "warn", eLogWarning },
        { "warning", eLogWarning },
        { "err", eLogError },
        { "error", eLogError },
        { "ftl", eLogFatal },
        { "fatal", eLogFatal },
        { NULL, 0 }
    };
    int k;
    eLogLevel_t level = eLogWarning;

    for(k = 0; levels[k].str != NULL; k++) {
        if(!strcasecmp(levels[k].str, str)) {
            level = levels[k].level;
            break;
        }
    }
    logLevelSet(level);
}

eLogLevel_t logLevelGet(void) {
    eLogLevel_t value;
    
    mutexGrab(g_logCtx.mutex);
    value = g_logCtx.level;
    mutexRelease(g_logCtx.mutex);
    
    return value;
}

const char *logLevelStr(eLogLevel_t level) {
	const char *levelStr[] = { "DBG", "INF", "WRN", "ERR", "FTL" };

	if(level < eLogMinEnum) {
    	level = eLogMinEnum;
	} else if(level > eLogMaxEnum) {
		level = eLogMaxEnum;
	}
	return levelStr[level];
}

void logDebugLineNum(int enable) {
#ifdef FLUB_DEBUG
    mutexGrab(g_logCtx.mutex);
    g_logCtx.linenums = enable;
    mutexRelease(g_logCtx.mutex);
#endif // FLUB_DEBUG
}

#ifdef FLUB_DEBUG

static void _logDebugEntryCreate(const char *module, const char *detail) {
    _logDebugEntry_t *entry;

    entry = util_alloc(sizeof(_logDebugEntry_t), NULL);
    entry->module = util_strdup(module);
    entry->detail = util_strdup(detail);
    entry->next = g_logCtx.debugSettings;
    g_logCtx.debugSettings = entry;
}

static int _logDebugEntryFind(const char *module, const char *detail) {
    _logDebugEntry_t *walk;
    int all = 0;

    if((!strcasecmp(module, "all"))) {
        all = 1;
    }

    for(walk = g_logCtx.debugSettings; walk != NULL; walk = walk->next) {
        if((all) || (!strcasecmp("all", walk->module)) || (!strcasecmp(module, walk->module))) {
            if((!strcasecmp(detail, "all")) || (!strcasecmp("all", walk->detail)) || (!strcasecmp(detail, walk->detail))) {
                return 1;
            }
        }
    }

    return 0;
}

static logDebugModule_t *_logCreateModEntry(const char *module, int value) {
    logDebugModule_t *modEntry, *modlast = NULL;

    for(modEntry = g_logCtx.debugDetails; modEntry != NULL; modEntry = modEntry->next) {
        if(!strcasecmp(modEntry->str, module)) {
            break;
        }
        modlast = modEntry;
    }
    if(modEntry == NULL) {
        modEntry = util_calloc(sizeof(logDebugModule_t), 0, NULL);
        modEntry->str = util_strdup(module);
        modEntry->number = value;
        modEntry->next = NULL;
        modEntry->details = NULL;
        if(modlast == NULL) {
            g_logCtx.debugDetails = modEntry;
        } else {
            modlast->next = modEntry;
        }
        if(value == LOG_DBG_PLACEHOLDER) {
            debugf(DBG_CORE, DBG_LOG_DTL_LOGGER, "Added debug module \"%s\" placeholder value.", module);
        } else {
            debugf(DBG_CORE, DBG_LOG_DTL_LOGGER, "Added debug module \"%s\" value %d.", module, value);
        }
    } else if(modEntry->number == LOG_DBG_PLACEHOLDER) {
        debugf(DBG_CORE, DBG_LOG_DTL_LOGGER, "Setting debug module \"%s\" value to %d (from placeholder).", module, value);
        modEntry->number = value;
    }
    return modEntry;
}

static void _logCreateDetailEntry(const char *module, int modValue,
                                  const char *detail, int detailValue) {
    logDebugModule_t *modEntry;
    logDebugDetail_t *detailEntry, *detlast = NULL;
    
    modEntry = _logCreateModEntry(module, modValue);

    if(!strcasecmp(detail, "all")) {
        modEntry->setAll = 1;
        debugf(DBG_CORE, DBG_LOG_DTL_LOGGER, "Set debug module \"%s\" flags to on for ALL.", module, detail);
        return;
    }
    for(detailEntry = modEntry->details; detailEntry != NULL; detailEntry = detailEntry->next) {
        if(!strcasecmp(detailEntry->str, detail)) {
            break;
        }
        detlast = detailEntry;
    }
    if(detailEntry == NULL) {
        detailEntry = util_calloc(sizeof(logDebugDetail_t), 0, NULL);
        detailEntry->str = util_strdup(detail);
        detailEntry->number = detailValue;
        detailEntry->next = NULL;
        if(detlast == NULL) {
            modEntry->details = detailEntry;
        } else {
            detlast->next = detailEntry;
        }
        if(detailValue == LOG_DBG_PLACEHOLDER) {
            debugf(DBG_CORE, DBG_LOG_DTL_LOGGER, "Added debug module \"%s\" detail \"%s\" placeholder value.", module, detail);
        } else {
            debugf(DBG_CORE, DBG_LOG_DTL_LOGGER, "Added debug module \"%s\" detail \"%s\" value %d.", module, detail, detailValue);
        }
    } else if(detailEntry->number == LOG_DBG_PLACEHOLDER) {
        debugf(DBG_CORE, DBG_LOG_DTL_LOGGER, "Setting debug module \"%s\" detail \"%s\" value to %d (from placeholder).", module, detail, detailValue);
        detailEntry->number = detailValue;        
    }
    if((modEntry->setAll) &&
       (detailEntry->number != LOG_DBG_PLACEHOLDER) &&
       (modEntry->number != LOG_DBG_PLACEHOLDER)) {
        LOG_DBG_SET(modEntry->number, detailEntry->number);
    }
}

void logDebugRegister(const char *module, int modValue,
                      const char *detail, int detailValue) {
    logDebugModule_t *modEntry, *modlast = NULL;
    logDebugDetail_t *detailEntry, *detlast = NULL;
    
    if((modValue < 1) || (modValue > 32)) {
        errorf("Failed to register debug module name \"%s\", value out of range: %d", module, modValue);
        return;
    }
    if((detailValue < 1) || (detailValue > 64)) {
        errorf("Failed to register debug module \"%s\" detail level \"%s\", detail out of range: %d", module, detail, detailValue);
        return;
    }
    if(!strcasecmp(detail, "all")) {
        errorf("Failed to register debug module \"%s\", detail level \"%s\", detail name is reserved.", module, detail);
        return;
    }
    _logCreateDetailEntry(module, modValue, detail, detailValue);

    if(_logDebugEntryFind(module, detail)) {
        logDebugSet(modValue, detailValue);
    }
}

void logDebugRegisterList(const char *module, int modValue,
                          flubLogDetailEntry_t *detailList) {
    int k;

    for(k = 0; detailList[k].name != NULL; k++) {
        logDebugRegister(module, modValue,
                         detailList[k].name,
                         detailList[k].value);
    }
}

void logDebugSet(int module, int detail) {
    int k;
    
    if(detail == -1) {
        for(k = 1; k <= 64; k++) {
            LOG_DBG_SET(module, k);
        }
    } else if((detail >= 1) && (detail <= 64)) {
        LOG_DBG_SET(module, detail);
    }
}

static void _logDebugSetDetail(logDebugModule_t *modEntry, const char *module, const char *detail) {
    logDebugDetail_t *detailEntry;
    int k;

    if(modEntry == NULL) {
        errorf("Failed to set debug for detail \"%s\" on non-existant module \"%s\"", detail, module);
        return;
    }
    if(!strcasecmp(detail, "all")) {
        for(k = 1; k <= 64; k++) {
            LOG_DBG_SET(modEntry->number, k);
        }
        debugf(DBG_CORE, DBG_LOG_DTL_LOGGER, "Setting debug for module \"%s\", all details", module);
        return;
    }
    for(detailEntry = modEntry->details; detailEntry != NULL; detailEntry = detailEntry->next) {
        if(!strcasecmp(detailEntry->str, detail)) {
            break;
        }
    }
    if(detailEntry == NULL) {
        errorf("Failed to set debug on module \"%s\" for non-existant detail \"%s\"", module, detail);
        return;
    }
    if((modEntry->number != LOG_DBG_PLACEHOLDER) &&
       (detailEntry->number != LOG_DBG_PLACEHOLDER)) {
        LOG_DBG_SET(modEntry->number, detailEntry->number);
    }
    debugf(DBG_CORE, DBG_LOG_DTL_LOGGER, "Setting debug for module \"%s\", detail \"%s\"", module, detail);
}

void logDebugSetByName(const char *module, const char *detail) {
    logDebugModule_t *modEntry;
    logDebugDetail_t *detailEntry;
    int k;

    // If the entries don't exist, create placeholder entries for them.
    //_logCreateDetailEntry(module, LOG_DBG_PLACEHOLDER, detail, LOG_DBG_PLACEHOLDER);

    if(!strcasecmp(module, "all")) {
        for(modEntry = g_logCtx.debugDetails; modEntry != NULL; modEntry = modEntry->next) {
            _logDebugSetDetail(modEntry, modEntry->str, detail);
        }
        if(!_logDebugEntryFind(module, detail)) {
            _logDebugEntryCreate(module, detail);
        }
    } else {
        for(modEntry = g_logCtx.debugDetails; modEntry != NULL; modEntry = modEntry->next) {
            if(!strcasecmp(modEntry->str, module)) {
                break;
            }
        }
        if(modEntry != NULL) {
            _logDebugSetDetail(modEntry, module, detail);
        }
        if((modEntry == NULL) || (!strcasecmp(detail, "all"))) {
            if(!_logDebugEntryFind(module, detail)) {
                _logDebugEntryCreate(module, detail);
            }
        }
    }
}

void logDebugSetByCmdline(const char *str) {
    char buf[64];
    char *detail;
    int len;

    memset(buf, 0, sizeof(buf));
    if((detail = strrchr(str, '+')) == NULL) {
        errorf("No detail specified for debug module \"%s\" on command line.", str);
        return;
    }
    len = detail - str;
    detail++;
    strncpy(buf, str, len);
    buf[len] = '\0';
    
    logDebugSetByName(buf, detail);
}

void logDebugClear(int module, int detail) {
    int k;
    
    if(detail == -1) {
        for(k = 1; k <= 64; k++) {
            LOG_DBG_SET(module, k);
            LOG_DBG_XOR(module, k);
        }
    }
    else if((detail >= 1) && (detail <= 64)) {
        LOG_DBG_SET( module, detail );
        LOG_DBG_XOR( module, detail );
    }
}

void logDebugClearByName(const char *module, const char *detail) {
    logDebugModule_t *modEntry;
    logDebugDetail_t *detailEntry;
    int k;
    
    for(modEntry = g_logCtx.debugDetails; modEntry != NULL; modEntry = modEntry->next) {
        if(!strcasecmp(modEntry->str, module)) {
            break;
        }
    }
    if(modEntry == NULL) {
        errorf("Failed to clear debug for detail \"%s\" on non-existant module \"%s\"", detail, module);
        return;
    }
    if(!strcasecmp(detail, "all")) {
        for(k = 1; k <= 64; k++) {
            LOG_DBG_SET(modEntry->number, k);
            LOG_DBG_XOR(modEntry->number, k);
        }
        debugf(DBG_CORE, DBG_LOG_DTL_LOGGER, "Clearing debug for module \"%s\", all details", module);
        return;
    }
    for(detailEntry = modEntry->details; detailEntry != NULL; detailEntry = detailEntry->next) {
        if(!strcasecmp(detailEntry->str, detail)) {
            break;
        }
    }
    if(detailEntry == NULL) {
        errorf("Failed to clear debug on module \"%s\" for non-existant detail \"%s\"", module, detail);
        return;
    }
    LOG_DBG_SET(modEntry->number, detailEntry->number);
    LOG_DBG_XOR(modEntry->number, detailEntry->number);
    debugf(DBG_CORE, DBG_LOG_DTL_LOGGER, "Clearing debug for module \"%s\", detail \"%s\"",
          modEntry->number, detailEntry->number);
}

void logDebugClearAll(void) {
    int k;
    
    for(k = 0; k < 256; k++) {
        g_LogDebugMask[k] = 0;
    }
}

void logDebugEnum(int activeOnly,
                  int (*callback)(const char *module, const char *detail,
                                  void *extra),
                  void *extra) {
    logDebugModule_t *modEntry;
    logDebugDetail_t *detailEntry;
    
    mutexGrab(g_logCtx.mutex);
    for(modEntry = g_logCtx.debugDetails; modEntry != NULL; modEntry = modEntry->next) {
        for(detailEntry = modEntry->details; detailEntry != NULL; detailEntry = detailEntry->next) {
            if((!activeOnly) || LOG_DBG_CHECK(modEntry->number, detailEntry->number)) {
                if(!callback(modEntry->str, detailEntry->str, extra)) {
                    break;
                }
            }
        }
    }
    mutexRelease(g_logCtx.mutex);
}

#endif // FLUB_DEBUG

void logAddNotifiee(logCallback_t cb) {
    logNotifiee_t *walk, *last = NULL, *entry;
    
    mutexGrab(g_logCtx.mutex);
    for(walk = g_logCtx.notifiees; walk != NULL; walk = walk->next) {
        if(walk->callback == cb) {
            // This callback has already been registered.
            mutexRelease(g_logCtx.mutex);
            return;
        }
        last = walk;
    }
    
    entry = util_calloc(sizeof(logNotifiee_t), 0, NULL);
    entry->callback = cb;
    entry->next = NULL;
    if(last == NULL) {
        g_logCtx.notifiees = entry;
    } else {
        last->next = entry;
    }
    mutexRelease(g_logCtx.mutex);
}

void logRemoveNotifiee(logCallback_t cb) {
    logNotifiee_t *walk, *last = NULL;
    
    mutexGrab(g_logCtx.mutex);
    for(walk = g_logCtx.notifiees; walk != NULL; walk = walk->next) {
        if(walk->callback == cb) {
            if(last == NULL) {
                g_logCtx.notifiees = walk->next;
            } else {
                last->next = walk->next;
            }
            free(walk);
            break;
        }
        last = walk;
    }
    mutexRelease(g_logCtx.mutex);
}

#ifdef FLUB_DEBUG

void logDebugMode(int mode) {
    mutexGrab(g_logCtx.mutex);
    g_logCtx.debugMode = mode;
    mutexRelease(g_logCtx.mutex);
}

#endif // FLUB_DEBUG

static void logGenTimestamp(void) {
	time_t now;
	struct tm *nowtm;

	now = time(NULL);
	nowtm = localtime(&now);
	sprintf(g_logCtx.timestamp, "%2.2d:%2.2d:%2.2d %2.2d/%2.2d",
            nowtm->tm_hour, nowtm->tm_min, nowtm->tm_sec,
            nowtm->tm_mon + 1, nowtm->tm_mday);
}

static void logSendLogMsg(eLogLevel_t level) {
	int k;
    logNotifiee_t *walk;

	// Ensure that the message ends in a LF, and only 1 LF is present
	for(k = 0; g_logCtx.buffer[k] != '\0'; k++) {
		if((g_logCtx.buffer[k] == '\n') || (g_logCtx.buffer[k] == '\r')) {
        	g_logCtx.buffer[k] = ' ';
		}
	}
	if(k == 0) {
		return;
	}
	if(g_logCtx.buffer[k - 1] == ' ') {
		g_logCtx.buffer[k - 1] = '\n';
	} else {
		g_logCtx.buffer[k] = '\n';
		g_logCtx.buffer[k + 1] = '\0';
	}

	logGenTimestamp();
	g_logCtx.msgStruct.levelStr = logLevelStr(level);
    g_logCtx.msgStruct.level = level;
    for(walk = g_logCtx.notifiees; walk != NULL; walk = walk->next) {
    	walk->callback(&g_logCtx.msgStruct);
	}
}

#ifdef FLUB_DEBUG
void logMessage(eLogLevel_t level, const char *file, int line, const char *msg) {
#else // !FLUB_DEBUG
void logMessage(eLogLevel_t level, const char *msg) {
#endif // FLUB_DEBUG
#ifdef FLUB_DEBUG
	int len = 0;
#endif // FLUB_DEBUG
    
    mutexGrab(g_logCtx.mutex);
    
	if((level < g_logCtx.level) || (g_logCtx.inProgress)) {
        mutexRelease(g_logCtx.mutex);
		return;
	}
    
    g_logCtx.inProgress = 1;

#ifdef FLUB_DEBUG

    if(g_logCtx.linenums) {
        len = snprintf(g_logCtx.buffer, sizeof(g_logCtx.buffer) - 1,
                       "[%s:%d] ", file, line);
        if(len > sizeof(g_logCtx.buffer)) {
            sprintf(g_logCtx.buffer, "[<unknown>:%d]", line);
            len = strlen(g_logCtx.buffer);
        }
    }
	strncpy(g_logCtx.buffer + len, msg, sizeof(g_logCtx.buffer) - 1 - len);

#else // !FLUB_DEBUG

	strncpy(g_logCtx.buffer, msg, sizeof(g_logCtx.buffer) - 1);

#endif // FLUB_DEBUG

	g_logCtx.buffer[sizeof(g_logCtx.buffer) - 2] = '\0';
	logSendLogMsg(level);

    g_logCtx.inProgress = 0;
    
    mutexRelease(g_logCtx.mutex);
}

#ifdef FLUB_DEBUG
void logMessagef(eLogLevel_t level, const char *file, int line, const char *fmt, ...) {
#else // !FLUB_DEBUG
void logMessagef(eLogLevel_t level, const char *fmt, ...) {
#endif // FLUB_DEBUG
	va_list ap;
#ifdef FLUB_DEBUG
	int len = 0;
#endif // FLUB_DEBUG

    mutexGrab(g_logCtx.mutex);
    
	if((level < g_logCtx.level) || (g_logCtx.inProgress)) {
        mutexRelease(g_logCtx.mutex);
		return;
	}
    
    g_logCtx.inProgress = 1;

#ifdef FLUB_DEBUG

    if(g_logCtx.linenums) {
        len = snprintf(g_logCtx.buffer, sizeof(g_logCtx.buffer) - 1,
                       "[%s:%d] ", file, line);
        if(len > sizeof(g_logCtx.buffer)) {
            sprintf(g_logCtx.buffer, "[<unknown>:%d]", line);
            len = strlen(g_logCtx.buffer);
        }
    }
    va_start(ap, fmt);
	vsnprintf(g_logCtx.buffer + len, sizeof(g_logCtx.buffer) - 2 - len, fmt, ap);

#else // !FLUB_DEBUG

    va_start(ap, fmt);
	vsnprintf(g_logCtx.buffer, sizeof(g_logCtx.buffer) - 2, fmt, ap);

#endif // FLUB_DEBUG

	va_end(ap);
	g_logCtx.buffer[sizeof(g_logCtx.buffer) - 2] = '\0';
	logSendLogMsg(level);

    g_logCtx.inProgress = 0;
    
    mutexRelease(g_logCtx.mutex);
}

void _logFileCallback(const logMessage_t *msg) {
    fprintf(g_logCtx.fp, "%s %s %s", msg->timestamp, msg->levelStr,
            msg->message);
    fflush(g_logCtx.fp);
}

int logFileInit(const char *file) {
    logFileShutdown();
    mutexGrab(g_logCtx.mutex);
    if((g_logCtx.fp = fopen(file, "w+")) == NULL) {
        errorf("Failed to open log file \"%s\" for output.", file);
        mutexRelease(g_logCtx.mutex);
        return 0;
    }
    logAddNotifiee(_logFileCallback);
    mutexRelease(g_logCtx.mutex);
}

void logFileShutdown(void) {
    mutexGrab(g_logCtx.mutex);
    if(g_logCtx.fp) {
        logRemoveNotifiee(_logFileCallback);
        fclose(g_logCtx.fp);
        g_logCtx.fp = NULL;
    }
    mutexRelease(g_logCtx.mutex);
}

void _logStdoutCallback(const logMessage_t *msg) {
    printf("%s %s %s", msg->timestamp, msg->levelStr, msg->message);
    fflush(stdout);
}

void logStdoutInit(void) {
    mutexGrab(g_logCtx.mutex);
    if(!g_logCtx.toStdout) {
        logAddNotifiee(_logStdoutCallback);
        g_logCtx.toStdout = 1;
    }
    mutexRelease(g_logCtx.mutex);
}

void logStdoutShutdown(void) {
    mutexGrab(g_logCtx.mutex);
    if(g_logCtx.toStdout)
    {
        logRemoveNotifiee(_logStdoutCallback);
        g_logCtx.toStdout = 0;
    }
    mutexRelease(g_logCtx.mutex);
}

void _logSDLCallback(void *userdata, int category, SDL_LogPriority priority, const char *message) {
    switch(priority) {
        case SDL_LOG_PRIORITY_DEBUG:
            debug(DBG_SDL, DBG_SDL_DTL_GENERAL, message);
            break;
        case SDL_LOG_PRIORITY_VERBOSE:
        case SDL_LOG_PRIORITY_INFO:
            info(message);
            break;
        case SDL_LOG_PRIORITY_WARN:
            warning(message);
            break;
        case SDL_LOG_PRIORITY_ERROR:
            error(message);
            break;
        case SDL_LOG_PRIORITY_CRITICAL:
            fatal(message);
            break;
    }
}

void logRedirectSDLLogs(void) {
    logDebugRegister("sdl", DBG_SDL, "general", DBG_SDL_DTL_GENERAL);

    if(g_logCtx.redirectSDL != NULL) {
        SDL_LogGetOutputFunction(&(g_logCtx.redirectSDL), NULL);
    }
    SDL_LogSetOutputFunction(_logSDLCallback, NULL);
}

void logStopRedirectSDLLogs(void) {
    SDL_LogSetOutputFunction(g_logCtx.redirectSDL, NULL);
}

#ifdef FLUB_DEBUG
static eCmdLineStatus_t _logDebugListHandler(const char *name, const char *arg, void *context) {
    logDebugModule_t *module;
    logDebugDetail_t *detail;

    printf("Debug module details:\n");
    for(module = g_logCtx.debugDetails; module != NULL; module = module->next) {
        printf("  %s\n", module->str);
        for(detail = module->details; detail != NULL; detail = detail->next) {
            printf("    %s\n", detail->str);
        }
    }
}
#endif // FLUB_DEBUG

void logCmdlineOptRegister(void) {
#ifdef FLUB_DEBUG
    cmdlineOptAdd("debug-list", 0, 0, NULL, "List all debug modules and details", _logDebugListHandler);
#endif // FLUB_DEBUG
}
