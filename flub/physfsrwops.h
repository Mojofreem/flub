/*
 * This code provides a glue layer between PhysicsFS and Simple Directmedia
 *  Layer's (SDL) RWops i/o abstraction.
 *
 * License: this code is public domain. I make no warranty that it is useful,
 *  correct, harmless, or environmentally safe.
 *
 * This particular file may be used however you like, including copying it
 *  verbatim into a closed-source project, exploiting it commercially, and
 *  removing any trace of my name from the source (although I hope you won't
 *  do that). I welcome enhancements and corrections to this file, but I do
 *  not require you to send me patches if you make changes.
 *
 * Unless otherwise stated, the rest of PhysicsFS falls under the GNU Lesser
 *  General Public License: http://www.gnu.org/licenses/lgpl.txt
 *
 * SDL falls under the LGPL, too. You can get SDL at http://www.libsdl.org/
 *
 *  This file was written by Ryan C. Gordon. (icculus@clutteredmind.org).
 */

#ifndef _PHYSFSRWOPS_HEADER_
#define _PHYSFSRWOPS_HEADER_


#include <SDL2/SDL.h>
#include <physfs.h>


SDL_RWops *PHYSFSRWOPS_makeRWops( PHYSFS_file *handle );
SDL_RWops *PHYSFSRWOPS_openRead( const char *fname );
SDL_RWops *PHYSFSRWOPS_openWrite( const char *fname );
SDL_RWops *PHYSFSRWOPS_openAppend( const char *fname );


#endif // _PHYSFSRWOPS_HEADER_


