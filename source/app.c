#include <flub/app.h>
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


/*

anim
audio
cmdline
config
console
core
font
gfx
gui
input
layout
logger
memory
physfs
texture
theme
thread
video
widget

*/

flubModuleCfg_t *_flubModules[] = {
    NULL,
};


static struct {
    int launchedFromConsole;
    const char *progName;
} _flubAppCtx = {
    .launchedFromConsole = 1,
    .progName = NULL,
};


int appUpdate(uint32_t ticks) {
    gfxUpdate(ticks);
    inputUpdate(ticks);
    consoleUpdate(ticks);

    videoUpdate();

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

    _flubAppCtx.launchedFromConsole = _appLaunchedFromConsole();

    if((opt = strrchr(argv[0], PLATFORM_PATH_SEP)) != NULL) {
    	_flubAppCtx.progName = opt;
    } else {
	    _flubAppCtx.progName = argv[0];
    }

    if(!logInit()) {
        return 0;
    }

    logDebugRegister("core", DBG_CORE, "general", DBG_CORE_DTL_GENERAL);

    for(k = 0; _flubModules[k] != NULL; k++) {
        flubModuleRegister(_flubModules[k]);
    }

    if((!memInit()) ||
       (!memFrameStackInit(appDefaults.frameStackSize)) ||
       (!cmdlineInit(argc, argv)) ||
       (!flubSDLInit()) ||
       (!flubPhysfsInit(argv[0])) ||
       (!flubCfgInit()) ||
       (!videoInit()) ||
       (!texmgrInit()) ||
       (!flubFontInit()) ||
       (!gfxInit()) ||
       (!audioInit()) ||
       (!inputInit()) ||
       (!consoleInit()) ||
       (!flubGuiThemeInit())
      ) {
        return 0;
    }

	return 1;
}

int appLaunchedFromConsole(void) {
    return _flubAppCtx.launchedFromConsole;
}

eCmdLineStatus_t appStart(void *cmdlineContext) {
	const char *opt;
    eCmdLineStatus_t status;

    opt = cmdlineGetAndRemoveOption("config", 'c', 1, "FILE",
                                    "specify the configuration option file", NULL,
                                    appDefaults.configFile);
	flubCfgLoad(opt);
	debug(DBG_CORE, DBG_LOG_DTL_APP, "Configuration system started.");

	// Initialize the command line processing subsystem
	status = cmdlineProcess(appDefaults.cmdlineHandler, cmdlineContext);
	if(status != eCMDLINE_OK) {
		return status;
	}
    debug(DBG_CORE, DBG_LOG_DTL_APP, "Command line parser completed.");

	// Video bringup
    if((!videoStart()) ||
       (!flubFontStart()) ||
       (!gfxStart()) ||
       (!consoleStart())) {
        return eCMDLINE_EXIT_FAILURE;
    }

    if(appDefaults.bindingFile != NULL) {
        inputBindingsImport(appDefaults.bindingFile);
    }

    debug(DBG_CORE, DBG_LOG_DTL_APP, "Begin application...");

	return eCMDLINE_OK;
}

void appShutdown(void) {
    // console
    // input
    // audio
    // gfx
    // font
    // texture
    videoShutdown();
    flubCfgShutdown();
    flubPhysfsShutdown();
    flubSDLShutdown();
    cmdlineShutdown();
    memShutdown();
    logShutdown();
}