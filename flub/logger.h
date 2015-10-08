#ifndef _FLUB_LOGGER_HEADER_
#define _FLUB_LOGGER_HEADER_


#define DBG_CORE        1
#define DBG_CMDLINE     2
#define DBG_CONFIG      3
#define DBG_VIDEO       4
#define DBG_TEXTURE     5
#define DBG_FONT        6
#define DBG_CONSOLE     7
#define DBG_GUI         8
#define DBG_OVERLAY     9
#define DBG_AUDIO       10
#define DBG_NETWORK     11
#define DBG_FILE        12
#define DBG_EVENT       13
#define DBG_SDL         14
#define DBG_MEMFILE     15
#define DBG_GFX         16
#define DBG_RESERVED    17

#define DBG_USER        0x00010000

#define DBG_LOG_DTL_LOGGER  1
#define DBG_LOG_DTL_PHYSFS  2
#define DBG_LOG_DTL_APP     3

#define DBG_SDL_DTL_GENERAL 1


#define LOG_MAX_MSG_SIZE            1024
#define LOG_MAX_TIMESTAMP_SIZE      64
#define LOG_MAX_INFO_SIZE           128


typedef enum {
    eLogDebug = 0,
    eLogMinEnum = eLogDebug,
    eLogInfo,
    eLogWarning,
    eLogError,
    eLogFatal,
    eLogMaxEnum = eLogFatal
} eLogLevel_t;


typedef struct flubLogDetailEntry_s flubLogDetailEntry_t;
struct flubLogDetailEntry_s {
    const char *name;
    int value;
};


#ifdef FLUB_DEBUG
// We support 32 debug groups, with 64 detail keys each
//     8 16 24 32 40 48 56 64  -  8 bytes per group

// unsigned char g_LogDebugMask[ 256 ];
extern unsigned char g_LogDebugMask[];

#define LOG_MASK_BYTE( g, d )   ( ( ( g ) * 8 ) + ( ( d ) / 8 ) )
#define LOG_MASK_BIT( d )       ( ( d ) % 8 )

#define LOG_DBG_CHECK( g, d )     ( g_LogDebugMask[ LOG_MASK_BYTE( ( g ), ( d ) ) ] & LOG_MASK_BIT( d ) )
#define LOG_DBG_SET( g, d )       ( g_LogDebugMask[ LOG_MASK_BYTE( ( g ), ( d ) ) ] |= LOG_MASK_BIT( d ) )
#define LOG_DBG_XOR( g, d )       ( g_LogDebugMask[ LOG_MASK_BYTE( ( g ), ( d ) ) ] ^= LOG_MASK_BIT( d ) )

#endif // FLUB_DEBUG


typedef struct logMessage_s {
    const char *levelStr;
    const char *message;
    const char *timestamp;
    eLogLevel_t level;
} logMessage_t;


typedef void (*logCallback_t)( const logMessage_t *msg );


int logInit(void);
int logValid(void);
void logShutdown(void);

void logLevelSet(eLogLevel_t level);
void logLevelSetStr(const char *str);
eLogLevel_t logLevelGet(void);
const char *logLevelStr(eLogLevel_t level);

void logDebugLineNum(int enable);

#ifdef FLUB_DEBUG

void logDebugRegister(const char *module, int modValue,
                      const char *detail, int detailValue);
void logDebugRegisterList(const char *module, int modValue,
                          flubLogDetailEntry_t *detailList);

void logDebugSet(int module, int detail);
void logDebugSetByName(const char *module, const char *detail);
void logDebugSetByCmdline(const char *str);
void logDebugClear(int module, int detail);
void logDebugClearByName(const char *module, const char *detail);
void logDebugClearAll(void);

void logDebugEnum(int activeOnly,
                  int (*callback)(const char *module, const char *detail,
                                  void *extra),
                  void *extra);

void logDebugMode(int mode);

void logMessage(eLogLevel_t level, const char *file, int line, const char *msg);
void logMessagef(eLogLevel_t level, const char *file, int line, const char *fmt, ...);

#   define debug(g,d,m)      do{ if(LOG_DBG_CHECK((g),(d))) { logMessage(eLogDebug, __FILE__, __LINE__, m); } }while(0)
#   define debugf(g,d,f,...) do{ if(LOG_DBG_CHECK((g),(d))) { logMessagef(eLogDebug, __FILE__, __LINE__, f, ##__VA_ARGS__); } }while(0)
#   define info(m)           logMessage(eLogInfo, __FILE__, __LINE__, m)
#   define infof(f,...)      logMessagef(eLogInfo, __FILE__, __LINE__, f, ##__VA_ARGS__)
#   define warning(m)        logMessage(eLogWarning, __FILE__, __LINE__, m)
#   define warningf(f,...)   logMessagef(eLogWarning, __FILE__, __LINE__, f, ##__VA_ARGS__)
#   define error(m)          logMessage(eLogError, __FILE__, __LINE__, m)
#   define errorf(f,...)     logMessagef(eLogError, __FILE__, __LINE__, f, ##__VA_ARGS__)
#   define fatal(m)          logMessage(eLogFatal, __FILE__, __LINE__, m)
#   define fatalf(f,...)     logMessagef(eLogFatal, __FILE__, __LINE__, f, ##__VA_ARGS__)

#else // NOT FLUB_DEBUG

#   define logDebugRegister(module, modValue, detail, detailValue)
#   define logDebugRegisterList(module, modValue, detailList)

#   define logDebugSet(module, detail)
#   define logDebugSetByName(module, detail)
#   define logDebugSetByCmdline(str)
#   define logDebugClear(module, detail)
#   define logDebugClearByName(module, detail)
#   define logDebugClearAll()

#   define logDebugEnum(activeOnly, callback, extra)

#   define logDebugMode(mode)

void logMessage(eLogLevel_t level, const char *msg);
void logMessagef(eLogLevel_t level, const char *fmt, ...);

#   define debug(g,d,m)
#   define debugf(g,d,f,...)
#   define info(m)         logMessage(eLogInfo, m)
#   define infof(f,...)    logMessagef(eLogInfo, f, ##__VA_ARGS__)
#   define warning(m)      logMessage(eLogWarning, m)
#   define warningf(f,...) logMessagef(eLogWarning, f, ##__VA_ARGS__)
#   define error(m)        logMessage(eLogError, m)
#   define errorf(f,...)   logMessagef(eLogError, f, ##__VA_ARGS__)
#   define fatal(m)        logMessage(eLogFatal, m)
#   define fatalf(f,...)   logMessagef(eLogFatal, f, ##__VA_ARGS__)

#endif // FLUB_DEBUG

void logAddNotifiee(logCallback_t cb);
void logRemoveNotifiee(logCallback_t cb);

int logFileInit(const char *file);
void logFileShutdown(void);

void logStdoutInit(void);
void logStdoutShutdown(void);

void logRedirectSDLLogs(void);
void logStopRedirectSDLLogs(void);

void logCmdlineOptRegister(void);


#endif // _FLUB_LOGGER_HEADER_
