#include <flub/flub.h>


struct {
	Uint32 lastTick;
	Uint32 elapsed;
} _coreCtx = {
		.lastTick = 0,
		.elapsed = 0,
	};


int flubPollEvent(SDL_Event *event, Uint32 *wait) {
	Uint32 currentTicks;
	int timeout;

	currentTicks = SDL_GetTicks();

	if(wait != NULL) {
		*wait -= currentTicks;
		if(*wait <= 0) {
			*wait = 0;
		} else {
            timeout = *wait;
        }
	} else {
		timeout = 0;
	}

	while(1) {
		if(!SDL_WaitEventTimeout(event, timeout)) {
			return 0;
		}

		// TODO Pre-filter event, before passing on to caller
		return 1;
	}
}

Uint32 flubTicksRefresh(Uint32 *elapsed) {
	Uint32 currentTicks;
	Uint32 elapsedTicks;
	int result;
	int timeout;

	currentTicks = SDL_GetTicks();

	if(_coreCtx.lastTick == 0) {
		elapsedTicks = 0;
	} else {
		elapsedTicks = currentTicks - _coreCtx.lastTick;
	}

	_coreCtx.lastTick = currentTicks;
	_coreCtx.elapsed = elapsedTicks;

	if(elapsed != NULL) {
		*elapsed = elapsedTicks;
	}

	return currentTicks;
}

Uint32 flubTicksElapsed(void) {
	return _coreCtx.elapsed;
}

Uint32 flubTicksGet(void) {
	return _coreCtx.lastTick;
}
