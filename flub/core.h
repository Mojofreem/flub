#ifndef _FLUB_CORE_HEADER_
#define _FLUB_CORE_HEADER_


#include <SDL2/SDL.h>


int flubPollEvent(SDL_Event *event, Uint32 *ticks, Uint32 *elapsed, Uint32 *wait);
Uint32 flubTicksElapsed(void);
Uint32 flubTicksGet(void);


#endif // _FLUB_CORE_HEADER_
