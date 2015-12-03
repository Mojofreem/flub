#ifndef _FLUB_CORE_HEADER_
#define _FLUB_CORE_HEADER_


#include <SDL2/SDL.h>


int flubPollEvent(SDL_Event *event, Uint32 *wait);
Uint32 flubTicksRefresh(Uint32 *elapsed);
Uint32 flubTicksGet(void);
Uint32 flubTicksElapsed(void);
int flubFpsGet(void);


#endif // _FLUB_CORE_HEADER_
