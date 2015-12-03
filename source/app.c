#include <flub/app.h>
#include <flub/module.h>
#include <flub/logger.h>
#include <flub/memory.h>

#include <flub/cmdline.h>
#include <flub/sdl.h>
#include <flub/physfsutil.h>
#include <flub/config.h>
#include <flub/util/misc.h>
#include <flub/logger.h>
#include <flub/video.h>
#include <flub/texture.h>
#include <flub/font.h>
#include <flub/audio.h>
#include <flub/input.h>
#include <flub/console.h>
#include <flub/theme.h>
#include <flub/memory.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <flub/module.h>

#ifdef WIN32
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#include <flub/memory.h>
#endif // WIN32


#define DBG_CORE_DTL_GENERAL    1


extern flubModuleCfg_t flubModuleLogger;
extern flubModuleCfg_t flubModuleMemory;
extern flubModuleCfg_t flubModuleCmdline;
extern flubModuleCfg_t flubModuleSDL;
extern flubModuleCfg_t flubModulePhysfs;
extern flubModuleCfg_t flubModuleConfig;
extern flubModuleCfg_t flubModuleVideo;
extern flubModuleCfg_t flubModuleTexture;
extern flubModuleCfg_t flubModuleFont;
extern flubModuleCfg_t flubModuleGfx;
extern flubModuleCfg_t flubModuleAudio;
extern flubModuleCfg_t flubModuleInput;
extern flubModuleCfg_t flubModuleConsole;
extern flubModuleCfg_t flubModuleTheme;

flubModuleCfg_t *_flubModules[] = {
    &flubModuleLogger,
    &flubModuleMemory,
    &flubModuleCmdline,
    &flubModuleSDL,
    &flubModulePhysfs,
    &flubModuleConfig,
    &flubModuleVideo,
    &flubModuleTexture,
    &flubModuleFont,
    &flubModuleGfx,
    &flubModuleAudio,
    &flubModuleInput,
    &flubModuleConsole,
    &flubModuleTheme,
    NULL,
};


static struct {
    int launchedFromConsole;
    const char *progName;
    flubModuleUpdateCB_t *update;
    int done;
} _flubAppCtx = {
    .launchedFromConsole = 1,
};


int appUpdate(uint32_t ticks, uint32_t elapsed) {
    int k;

    if(_flubAppCtx.done) {
        return 0;
    }
    
    for(k = 0; _flubAppCtx.update[k] != NULL; k++) {
        if(!_flubAppCtx.update[k](ticks, elapsed)) {
            return 0;
        }
    }

    memFrameStackReset();

	return 1;
}

const char *appName(void) {
	return _flubAppCtx.progName;
}

#ifdef WIN32
// http://stackoverflow.com/questions/510805/can-a-win32-console-application-detect-if-it-has-been-run-from-the-explorer-or-n
static int _appLaunchedFromConsole(void) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;

    if (!GetConsoleScreenBufferInfo( GetStdHandle( STD_OUTPUT_HANDLE), &csbi))
    {
        return 1;
    }

    // if cursor position is (0,0) then we were launched in a separate console
    if((!csbi.dwCursorPosition.X) && (!csbi.dwCursorPosition.Y)) {
        return 0;
    }
    return 1;
}
#else // WIN32
static int _appLaunchedFromConsole(void) {
    return 1;
}
#endif // WIN32

int appInit(int argc, char **argv) {
    const char *opt;
    int k;

    appDefaults.argc = argc;
    appDefaults.argv = argv;

    _flubAppCtx.launchedFromConsole = _appLaunchedFromConsole();

    if((opt = strrchr(argv[0], PLATFORM_PATH_SEP)) != NULL) {
    	_flubAppCtx.progName = opt;
    } else {
	    _flubAppCtx.progName = argv[0];
    }

    for(k = 0; _flubModules[k] != NULL; k++) {
        flubModuleRegister(_flubModules[k]);
    }

    if(!flubModulesInit(&appDefaults)) {
        return 0;
    }

    logDebugRegister("core", DBG_CORE, "general", DBG_CORE_DTL_GENERAL);

    _flubAppCtx.update = flubModulesUpdateList();

	return 1;
}

int appLaunchedFromConsole(void) {
    return _flubAppCtx.launchedFromConsole;
}

eCmdLineStatus_t appStart(void *cmdlineContext) {
    appDefaults.cmdlineContext = cmdlineContext;

    if(!flubModulesStart()) {
        return eCMDLINE_EXIT_FAILURE;
    }

    if(appDefaults.bindingFile != NULL) {
        inputBindingsImport(appDefaults.bindingFile);
    }

    debug(DBG_CORE, DBG_LOG_DTL_APP, "Begin application...");

	return eCMDLINE_OK;
}

void appQuit(void) {
    _flubAppCtx.done = 1;
}

void appShutdown(void) {
    flubModulesShutdown();
}
