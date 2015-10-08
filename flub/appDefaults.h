#ifndef _FLUB_APP_DEFAULTS_HEADER_
#define _FLUB_APP_DEFAULTS_HEADER_

#include <flub/app.h>
#include <stdlib.h>


appDefaults_t appDefaults = {
        .major = 1,
        .minor = 0,
        .title = "FlubApp",
        .configFile = NULL,
        .bindingFile = NULL,
        .archiveFile = NULL,
        .videoMode = "640x480",
        .fullscreen = "false",
        .allowVideoModeChange = 0,
        .allowFullscreenChange = 0,
        .cmdlineHandler = NULL,
        .cmdlineParamStr = NULL,
        .resources = NULL,
};


#endif // _FLUB_APP_DEFAULTS_HEADER_
