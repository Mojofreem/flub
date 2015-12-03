#include <flub/flub.h>


struct {
	Uint32 pollTick;
	Uint32 lastTick;
	Uint32 elapsed;
	Uint32 fpsTick;
	int frames;
	int fps;
} _coreCtx = {
		.pollTick = 0,
	};


int flubPollEvent(SDL_Event *event, Uint32 *wait) {
	Uint32 currentTick;
	Uint32 elapsed;
	Uint32 lastTick;
	Uint32 timeout = 0;

	if(_coreCtx.pollTick == 0) {
		_coreCtx.pollTick = SDL_GetTicks();
	}
	currentTick = SDL_GetTicks();
	if(wait == NULL) {
		wait = &timeout;
	}
	elapsed = (currentTick - _coreCtx.pollTick);
	if(elapsed > *wait) {
		*wait = 0;
	} else {
		*wait -= elapsed;
	}

	if(!SDL_WaitEventTimeout(event, *wait)) {
		*wait = 0;
		return 0;
	}

	// TODO Pre-filter event, before passing on to caller
	return 1;
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

	_coreCtx.fpsTick += elapsedTicks;
	if(_coreCtx.fpsTick > 1000) {
		_coreCtx.fpsTick -= 1000;
		_coreCtx.fps = _coreCtx.frames;
		_coreCtx.frames = 0;
	}
	_coreCtx.frames++;

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

int flubFpsGet(void) {
	return _coreCtx.fps;
}
