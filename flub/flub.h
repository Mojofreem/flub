#ifndef _FLUB_COMMON_HEADER_
#define _FLUB_COMMON_HEADER_


#ifdef WIN32
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#	define PLATFORM_PATH_SEP	'\\'
#else // WIN32
#	define PLATFORM_PATH_SEP	'/'
#endif // WIN32

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#ifdef WIN32
#	define GLEW_STATIC
#	include <flub/3rdparty/glew/glew.h>
#endif

#include <SDL2/SDL.h>
#ifdef MACOSX
#   include <OpenGL/gl.h>
#	include <OpenGL/glext.h>
#else // MACOSX
#   include <gl/gl.h>
#	include <GL/glext.h>
#endif // MACOSX

#include <SDL2/SDL_opengl.h>

#include <physfs.h>

#include <flub/core.h>
#include <flub/app.h>
#include <flub/logger.h>
#include <flub/cmdline.h>
#include <flub/config.h>
#include <flub/thread.h>
#include <flub/license.h>
//#include <flub/util.h>
#include <flub/data/critbit.h>
#include <flub/video.h>
#include <flub/texture.h>
#include <flub/gfx.h>
#include <flub/font.h>
#include <flub/audio.h>
#include <flub/physfsrwops.h>
#include <flub/input.h>
#include <flub/console.h>
#include <flub/anim.h>


#endif // _FLUB_COMMON_HEADER_

#if 0
/*
//////////////////////////////////////////////////////////////////////////////

Design and implement:
---------------------------------------------------
revise layout engine
text panel
overlay
* overview
* layout
* effects
* text scroller
* counter
* notifications
* help prompts
chat input
* chat text entry hooks and callback
text editor
* scoped window
* editing
* autocompletion
* prompting
* history
* multiline
* multicolor
* embedable content

animation
* design basic 2D workflow
* implement 2D API
random
* randomness functions
gui
* base framework
* base widget
* specialized widgets
* common dialogs
    - file dialog
    - simple message box
    - font picker
    - color picker
* gui widgets
* gui layout components
    - horizontal
    - vertical
    - grid
* gui descriptive templates
* menu component
* gui interface for event binding
gui theme
* design theming/layout engine
* build dirt simple tools for finding coords
* integrate basic simple theme
* write more robust theming app
sys menu
* base framework
* common actions/features
* customizable
* themeable


* write test app for blitters, and tweak blit response to achieve pixel perfect blits


Misc:
---------------------------------------------------
Documentation

Utilities:
---------------------------------------------------
* Demo app
* slice builder
* gui themer
* animation builder
* gui builder

Long term:
---------------------------------------------------
QT integration
network
* third party auth
* client server
* local server
* http wrapper
* resource download
* resource synchronization
* central registry
* chat
* voice chat
multithread
* offload tasks
* asynchronous loading
* thread safety

Module startup naming convention:
---------------------------------------------------


Bugs
---------------------------------------------------
Parse color is broken, won't parse 6 hex digit numbers (#FF00FF)

//////////////////////////////////////////////////////////////////////////////
*/

#endif
