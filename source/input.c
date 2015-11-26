#include <flub/input.h>
#include <flub/memory.h>
#include <flub/video.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_scancode.h>
#include <SDL2/SDL_keycode.h>
#include <ctype.h>
#include <stdio.h>
#include <flub/data/critbit.h>
#include <flub/cmdline.h>
#include <string.h>
#include <flub/util/string.h>
#include <stdlib.h>

#define MAX_INPUT_LINE 128


#include <flub/input_events.inc>
#include <flub/logger.h>
#include <flub/module.h>


///////////////////////////////////////////////////////////////////////////////
//
// Events action bindings are stored in a hash table. The table has 1024
// entries. SDL events are mapped to the table using the following design:
//
// 300 Key event mapping (SDL 2.0.3 currently only has 284 keys defined)
//  10 Mouse events:
//         * X axis
//         * Y axis
//         * Left, middle, right, x1, x2, x3
//         * Wheel up, wheel down
// 496 Joystick events:
//         * 8 joysticks (62 events per joystick):
//             - 6 axis
//             - 16 buttons
//             - 4 balls:
//                 = 2 axis
//             - 4 hats:
//                 = 8 positions (excluding center)
// 210 Gamepad events:
//         * 10 gamepads (21 events per gamepad):
//             - 6 axis:
//                 = left x, left y, right x, right y, trigger left, trigger right
//             - 15 buttons:
//                 = a, b, x, y, back, guide, start, left stick, right stick,
//                   left shoulder, right shoulder, dpad up/down/left/right
// 1016 total events (8 unused)
//
///////////////////////////////////////////////////////////////////////////////


typedef enum {
	eFlubInputCaptureTrap = 0,
	eFlubInputCaptureEvent,
	eFlubInputCaptureMouse,
} eFlubInputCaptureMode_t;

typedef struct flubInputCaptureEntry_s {
	eFlubInputCaptureMode_t mode;
	flubInputActionHandler_t captureHandler;
	flubInputEventTrap_t trapHandler;

	struct flubInputCaptureEntry_s *next;
} flubInputCaptureEntry_t;

typedef struct flubInputAction_s {
	char *bind;
	char *display;
	int mode;
	int event;
	flubInputActionHandler_t handler;

	struct flubInputAction_s *next;
} flubInputAction_t;

typedef struct flubInputGroup_s {
	char *name;

	flubInputAction_t *actions;

	struct flubInputGroup_s *next;
} flubInputGroup_t;

typedef struct flubInputMode_s {
	char *name;
	int id;

	flubInputAction_t *eventMap[1024];

	struct flubInputMode_s *next;
} flubInputMode_t;

struct {
    int init;
	critbit_t actionRegistry;
	critbit_t eventRegistry;
	int mouseX;
	int mouseY;
	flubInputCaptureEntry_t *capturePool;
	flubInputCaptureEntry_t *captureEventHandler;
	flubInputCaptureEntry_t *captureMouseHandler;
	flubInputCaptureEntry_t *trapHandler;
	flubInputGroup_t *groups;
	flubInputMode_t *modes, *active;
	int showCursor;
} _inputCtx = {
        .init = 0,
		.actionRegistry = CRITBIT_TREE_INIT,
		.eventRegistry = CRITBIT_TREE_INIT,
		.mouseX = 0,
		.mouseY = 0,
		.capturePool = NULL,
		.captureEventHandler = NULL,
		.captureMouseHandler = NULL,
		.trapHandler = NULL,
		.groups = NULL,
		.modes = NULL,
		.active = NULL,
		.showCursor = 0,
	};

#define DBG_INPUT_DTL_GENERAL	1
#define DBG_INPUT_DTL_EVENT		2
#define DBG_INPUT_DTL_ACTION	3

static flubLogDetailEntry_t _consoleDbg[] = {
		{"general", DBG_INPUT_DTL_GENERAL},
		{"event", DBG_INPUT_DTL_EVENT},
		{"action", DBG_INPUT_DTL_ACTION},
		{NULL, 0},
	};


static void _inputEventRegistryBuild(void) {
	int k;

	debugf(DBG_CONSOLE, DBG_INPUT_DTL_EVENT, "Building input event registry");
	for(k = 0; _flubInputEventTable[k].name != NULL; k++) {
		critbitInsert(&(_inputCtx.eventRegistry), _flubInputEventTable[k].bindname, &(_flubInputEventTable[k]), NULL);
        //infof("Event: %d %s", _flubInputEventTable[k].index, _flubInputEventTable[k].bindname);
        if(!critbitContains(&(_inputCtx.eventRegistry), _flubInputEventTable[k].bindname, NULL)) {
            errorf("Failed to register input event");
        }
	}
}

static int _inputCmdlinePrinter(const char *name, void *context) {
    printf("    %s\n", name);
    return 1;
}

static eCmdLineStatus_t _inputCmdlineEvents(const char *name, const char *arg, void *context) {
    printf("Input event bindings:\n");
    inputEnumBindings(_inputCmdlinePrinter, NULL);
    return eCMDLINE_EXIT_SUCCESS;
}

static eCmdLineStatus_t _inputCmdlineActions(const char *name, const char *arg, void *context) {
    printf("Input action bindings:\n");
    inputEnumActions(_inputCmdlinePrinter, NULL);
    return eCMDLINE_EXIT_SUCCESS;
}


int inputInit(appDefaults_t *defaults) {
	logDebugRegisterList("console", DBG_CONSOLE, _consoleDbg);

	_inputEventRegistryBuild();

    if((!inputModeAdd("General", 0)) ||
       (!inputGroupAdd("General"))) {
        return 0;
    }

    inputModeSet(0);

    cmdlineOptAdd("list-bindings", 0, 0, NULL,
                  "List the recognized input events", _inputCmdlineEvents);
    cmdlineOptAdd("list-actions", 0, 0, NULL,
                  "List the recognized input actions", _inputCmdlineActions);

    _inputCtx.init = 1;

	return 1;
}

void _inputShutdown(void) {
}

static char *_initDeps[] = {"sdl", NULL};
flubModuleCfg_t flubModuleInput = {
	.name = "input",
	.init = inputInit,
	.update = inputUpdate,
	.initDeps = _initDeps,
};

void inputMousePosSet(int x, int y){
	_inputCtx.mouseX = x;
	_inputCtx.mouseY = y;
}

void inputMousePosGet(int *x, int *y) {
	if(x != NULL) {
		*x = _inputCtx.mouseX;
	}
	if(y != NULL) {
		*y = _inputCtx.mouseY;
	}
}

static flubInputMode_t *_inputModeGet(const char *name, int mode, flubInputMode_t **previous) {
	flubInputMode_t *walk, *entry, *last = NULL;

	for(walk = _inputCtx.modes; walk != NULL; last = walk, walk = walk->next) {
		if(previous != NULL) {
			*previous = last;
		}
		if((name != NULL) && (!strcmp(walk->name, name))) {
			return walk;
		} else if(walk->id == mode) {
			return walk;
		}
	}
	if(previous != NULL) {
		*previous = last;
	}
	return NULL;
}

static void _inputCaptureAndTrapClear(void) {
	flubInputCaptureEntry_t  *walk;

	if(_inputCtx.captureEventHandler != NULL) {
		if(_inputCtx.capturePool == NULL) {
			_inputCtx.capturePool = _inputCtx.captureEventHandler;
		} else {
			for(walk = _inputCtx.capturePool; walk->next != NULL; walk = walk->next);
			walk->next = _inputCtx.captureEventHandler;
		}
		_inputCtx.captureEventHandler = NULL;
	}
	if(_inputCtx.captureMouseHandler != NULL) {
		if(_inputCtx.capturePool == NULL) {
			_inputCtx.capturePool = _inputCtx.captureMouseHandler;
		} else {
			for(walk = _inputCtx.capturePool; walk->next != NULL; walk = walk->next);
			walk->next = _inputCtx.captureMouseHandler;
		}
		_inputCtx.captureMouseHandler = NULL;
	}
	if(_inputCtx.trapHandler != NULL) {
		if(_inputCtx.capturePool == NULL) {
			_inputCtx.capturePool = _inputCtx.trapHandler;
		} else {
			for(walk = _inputCtx.capturePool; walk->next != NULL; walk = walk->next);
			walk->next = _inputCtx.trapHandler;
		}
		_inputCtx.trapHandler = NULL;
	}
}

int inputModeSet(int id) {
	flubInputMode_t *mode;

	if((mode = _inputModeGet(NULL, id, NULL)) == NULL) { 
		errorf("Could not set non existent input mode %d", id);
		return 0;
	}

	_inputCtx.active = mode;

	_inputCaptureAndTrapClear();

	return 1;
}

static flubInputCaptureEntry_t *_inputCaptureEntryGet(void) {
	flubInputCaptureEntry_t *entry;

	if(_inputCtx.capturePool != NULL) {
		entry = _inputCtx.capturePool;
		_inputCtx.capturePool = entry->next;
		entry->next = NULL;
	} else {
		entry = util_calloc(sizeof(flubInputCaptureEntry_t), 0, NULL);
	}
	memset(entry, 0, sizeof(flubInputCaptureEntry_t));

	return entry;
}

void inputEventTrapCapture(flubInputEventTrap_t handler) {
	flubInputCaptureEntry_t *entry;

	entry = _inputCaptureEntryGet();

	entry->mode = eFlubInputCaptureTrap;
	entry->trapHandler = handler;
	entry->next = _inputCtx.trapHandler;
	_inputCtx.trapHandler = entry;
}

void inputEventTrapRelease(flubInputEventTrap_t handler) {
    flubInputCaptureEntry_t *entry, *last = NULL;

    for(entry = _inputCtx.trapHandler; entry != NULL; last = entry, entry = entry->next) {
        if(entry->trapHandler == handler) {
            if(last == NULL) {
                _inputCtx.trapHandler = entry->next;
            } else {
                last->next = entry->next;
            }
            entry->next = _inputCtx.capturePool;
            _inputCtx.capturePool = entry;
            return;
        }
    }
}

void inputEventCapture(flubInputActionHandler_t handler) {
	flubInputCaptureEntry_t *entry;

	entry = _inputCaptureEntryGet();

	entry->mode = eFlubInputCaptureEvent;
	entry->captureHandler = handler;
	entry->next = _inputCtx.captureEventHandler;
	_inputCtx.captureEventHandler = entry;
}

void inputEventRelease(flubInputActionHandler_t handler) {
    flubInputCaptureEntry_t *entry, *last = NULL;

    for(entry = _inputCtx.captureEventHandler; entry != NULL; last = entry, entry = entry->next) {
        if(entry->captureHandler == handler) {
            if(last == NULL) {
                _inputCtx.captureEventHandler = entry->next;
            } else {
                last->next = entry->next;
            }
            entry->next = _inputCtx.capturePool;
            _inputCtx.capturePool = entry;
            return;
        }
    }
}

void inputMouseCapture(flubInputActionHandler_t handler){
	flubInputCaptureEntry_t *entry;

	entry = _inputCaptureEntryGet();

	entry->mode = eFlubInputCaptureMouse;
	entry->captureHandler = handler;
	entry->next = _inputCtx.captureMouseHandler;
	_inputCtx.captureMouseHandler = entry;
}

void inputMouseRelease(flubInputActionHandler_t handler) {
    flubInputCaptureEntry_t *entry, *last = NULL;

    for(entry = _inputCtx.captureMouseHandler; entry != NULL; last = entry, entry = entry->next) {
        if(entry->captureHandler == handler) {
            if(last == NULL) {
                _inputCtx.captureMouseHandler = entry->next;
            } else {
                last->next = entry->next;
            }
            entry->next = _inputCtx.capturePool;
            _inputCtx.capturePool = entry;
            return;
        }
    }
}

void inputMouseCursor(int show) {
	_inputCtx.showCursor = show;
}

///////////////////////////////////////////////////////////////////////////////
//
// Events action bindings are stored in a hash table. The table has 1024
// entries. SDL events are mapped to the table using the following design:
//
// 300 Key event mapping (SDL 2.0.3 currently only has 284 keys defined)
//  10 Mouse events:
//         * X axis
//         * Y axis
//         * Left, middle, right, x1, x2, x3
//         * Wheel up, wheel down
// 496 Joystick events:
//         * 8 joysticks (62 events per joystick):
//             - 6 axis
//             - 16 buttons
//             - 4 balls:
//                 = 2 axis
//             - 4 hats:
//                 = 8 positions (excluding center)
// 210 Gamepad events:
//         * 10 gamepads (21 events per gamepad):
//             - 6 axis:
//                 = left x, left y, right x, right y, trigger left, trigger right
//             - 15 buttons:
//                 = a, b, x, y, back, guide, start, left stick, right stick,
//                   left shoulder, right shoulder, dpad up/down/left/right
// 1016 total events (8 unused)
//
///////////////////////////////////////////////////////////////////////////////

/*
#define FLUB_INPUT_MOUSE_BASE(x)		(300+(x))
#define FLUB_INPUT_MOUSE_XAXIS()		FLUB_INPUT_MOUSE_BASE(0)
#define FLUB_INPUT_MOUSE_YAXIS()		FLUB_INPUT_MOUSE_BASE(1)
#define FLUB_INPUT_MOUSE_LEFT()			FLUB_INPUT_MOUSE_BASE(2)
#define FLUB_INPUT_MOUSE_MIDDLE()		FLUB_INPUT_MOUSE_BASE(3)
#define FLUB_INPUT_MOUSE_RIGHT()		FLUB_INPUT_MOUSE_BASE(4)
#define FLUB_INPUT_MOUSE_X1()			FLUB_INPUT_MOUSE_BASE(5)
#define FLUB_INPUT_MOUSE_X2()			FLUB_INPUT_MOUSE_BASE(6)
#define FLUB_INPUT_MOUSE_BTN(x)			FLUB_INPUT_MOUSE_BASE((x)+2)
#define FLUB_INPUT_MOUSE_WHEEL_UP()		FLUB_INPUT_MOUSE_BASE(8)
#define FLUB_INPUT_MOUSE_WHEEL_DOWN()	FLUB_INPUT_MOUSE_BASE(9)

#define FLUB_INPUT_JOY_BASE(x)			(310+(x))
#define FLUB_INPUT_JOY_DEV(j)			(FLUB_INPUT_JOY_BASE((j)*62))
#define FLUB_INPUT_JOY_AXIS(j,a)		(FLUB_INPUT_JOY_DEV(j)+(a))
#define FLUB_INPUT_JOY_BTN(j,b)			(FLUB_INPUT_JOY_DEV(j)+6+(b))
#define FLUB_INPUT_JOY_BALL(j,b,a)		(FLUB_INPUT_JOY_DEV(j)+22+((b)*2)+(a))
#define FLUB_INPUT_JOY_HAT(j,h,p)		(FLUB_INPUT_JOY_DEV(j)+30+((h)*8)+(p))

#define FLUB_INPUT_GPAD_BASE(x)			(806+(x))
#define FLUB_INPUT_GPAD_DEV(g)			(FLUB_INPUT_GPAD_DEV((g)*21))
#define FLUB_INPUT_GPAD_AXIS(g,a)		(FLUB_INPUT_GPAD_DEV(g)+(a))
#define FLUB_INPUT_GPAD_BTN(g,b)		(FLUB_INPUT_GPAD_DEV((g)+6+(b)))
*/

int inputEventProcess(SDL_Event *event) {
	int mouse = 0;
	int index = -1;
	flubInputAction_t *action;
    SDL_Scancode scancode;


	Uint8 code;
	int k;
	int motion = 0;
	int pressed = 0;
	int x = 0;
	int y = 0;
	Uint8 value;

	// TODO Process input events
	switch(event->type) {
		case SDL_KEYDOWN:
			pressed = 1;
			// Intentional fallthrough
		case SDL_KEYUP:
            scancode = SDL_GetScancodeFromKey(event->key.keysym.sym);
			if(scancode < 300) {
                index = scancode;
            }
			break;
		case SDL_JOYAXISMOTION:
			if((event->jaxis.which < 8) && (event->jaxis.axis < 6)) {
				index = FLUB_INPUT_JOY_AXIS(event->jaxis.which, event->jaxis.axis);
				motion = 1;
				x = event->jaxis.value;
			}
			break;
		case SDL_JOYBALLMOTION:
			if((event->jball.which < 8) && (event->jball.ball < 4)) {
				index = FLUB_INPUT_JOY_BALL(event->jball.which, event->jball.ball);
				motion = 1;
				x = event->jball.xrel;
				y = event->jball.yrel;
			}
			break;
		case SDL_JOYHATMOTION:
			if((event->jhat.which < 8) && (event->jhat.hat < 2)) {
				switch(event->jhat.value) {
					case SDL_HAT_LEFTUP:
						value = 0;
						break;
					case SDL_HAT_UP:
						value = 1;
						break;
					case SDL_HAT_RIGHTUP:
						value = 2;
						break;
					case SDL_HAT_RIGHT:
						value = 3;
						break;
					case SDL_HAT_RIGHTDOWN:
						value = 4;
						break;
					case SDL_HAT_DOWN:
						value = 5;
						break;
					case SDL_HAT_LEFTDOWN:
						value = 6;
						break;
					case SDL_HAT_LEFT:
						value = 7;
						break;
					default:
						value = 255;
						break;
				}
				if(value == 255) {
					break;
				}
				index = FLUB_INPUT_JOY_HAT(event->jhat.which, event->jhat.hat, value);
			}
			break;
		case SDL_JOYBUTTONDOWN:
			pressed = 1;
			// Intentional fallthrough
		case SDL_JOYBUTTONUP:
			if((event->jbutton.which < 8) && (event->jbutton.button < 16)) {
				index = FLUB_INPUT_JOY_BTN(event->jbutton.which, event->jbutton.button);
			}
			break;
		case SDL_CONTROLLERAXISMOTION:
			if((event->caxis.which < 8) && (event->caxis.axis < 4)) {
				index = FLUB_INPUT_GPAD_AXIS(event->caxis.which, event->caxis.axis);
				motion = 1;
				x = event->caxis.value;
			}
			break;
		case SDL_CONTROLLERBUTTONDOWN:
			pressed = 1;
			// Intentional fallthrough
		case SDL_CONTROLLERBUTTONUP:
			if((event->cbutton.which < 8) && (event->cbutton.button < 16)) {
				index = FLUB_INPUT_GPAD_BTN(event->cbutton.which, event->cbutton.button);
			}
			break;
		case SDL_MOUSEMOTION:
			mouse = 1;
			_inputCtx.mouseX += event->motion.xrel;
			_inputCtx.mouseY += event->motion.yrel;
			if(_inputCtx.mouseX < 0) {
				_inputCtx.mouseX = 0;
			} else if(_inputCtx.mouseX >= *videoWidth) {
				_inputCtx.mouseX = *videoWidth - 1;
			}
			if(_inputCtx.mouseY < 0) {
				_inputCtx.mouseY = 0;
			} else if(_inputCtx.mouseY >= *videoHeight) {
				_inputCtx.mouseY = *videoHeight - 1;
			}
			if(event->motion.xrel) {
				x = event->motion.xrel;
			}
			if(event->motion.yrel) {
				y = event->motion.yrel;
			}
			index = FLUB_INPUT_MOUSE_AXIS();
			break;
		case SDL_MOUSEBUTTONDOWN:
			pressed = 1;
			// Intentional fallthrough
		case SDL_MOUSEBUTTONUP:
			switch(event->button.button) {
				case SDL_BUTTON_LEFT:
					index = FLUB_INPUT_MOUSE_LEFT();
					break;
				case SDL_BUTTON_MIDDLE:
					index = FLUB_INPUT_MOUSE_MIDDLE();
					break;
				case SDL_BUTTON_RIGHT:
					index = FLUB_INPUT_MOUSE_RIGHT();
					break;
				case SDL_BUTTON_X1:
					index = FLUB_INPUT_MOUSE_X1();
					break;
				case SDL_BUTTON_X2:
					index = FLUB_INPUT_MOUSE_X2();
					break;
			}
			break;
		case SDL_MOUSEWHEEL:
			if(event->wheel.x > 0) {
				index = FLUB_INPUT_MOUSE_WHEEL_UP();
			}
			if(event->wheel.x < 0) {
				index = FLUB_INPUT_MOUSE_WHEEL_DOWN();
			}
			break;
		case SDL_JOYDEVICEADDED:
		case SDL_JOYDEVICEREMOVED:
		case SDL_CONTROLLERDEVICEADDED:
		case SDL_CONTROLLERDEVICEREMOVED:
		case SDL_CONTROLLERDEVICEREMAPPED:
			warning("A gamepad was added or removed.");
			break;
	}

	if(_inputCtx.trapHandler != NULL) {
		_inputCtx.trapHandler->trapHandler(event, index);
		return 1;
	}
	if(mouse) {
		if(_inputCtx.captureMouseHandler != NULL) {
			_inputCtx.captureMouseHandler->captureHandler(event, pressed, motion, x, y);
			return 1;
		}
	} else {
		if(_inputCtx.captureEventHandler != NULL) {
			_inputCtx.captureEventHandler->captureHandler(event, pressed, motion, x, y);
			return 1;
		}		
	}
	if(index > 0) {
		if(_inputCtx.active != NULL) {
			if((action = _inputCtx.active->eventMap[index]) != NULL) {
				action->handler(event, pressed, motion, x, y);
				return 1;
			} else {
                //info("No action specified");
            }
		}
		if(_inputCtx.modes != NULL) {
			if((action = _inputCtx.modes->eventMap[index]) != NULL) {
				action->handler(event, pressed, motion, x, y);
				return 1;
			}
		}
	}

	return 0;
}

int inputPollEvent(SDL_Event *event) {
	if(SDL_PollEvent(event)) {
		if(!inputEventProcess(event)) {
			return 1;
		}
		return 0;
	}
	return 0;
}

int inputWaitEventTimeout(SDL_Event *event, int timeout) {
	Uint32 lastTicks, elapsed = 0;

	lastTicks = SDL_GetTicks();
	while(SDL_WaitEventTimeout(event, timeout)) {
		if(!inputEventProcess(event)) {
			return 1;
		}
		elapsed = SDL_GetTicks() - lastTicks;
		timeout -= elapsed;
		if(timeout > 0) {
			return 0;
		}
	}
	return 0;
}

int inputUpdate(Uint32 ticks, Uint32 elapsed) {
	if(_inputCtx.showCursor) {
		// TODO Update and display the mouse, if mouse cursor is enabled		
	}
	return 1;
}

int inputModeAdd(const char *name, int id) {
	flubInputMode_t *mode, *walk, *last;

	if(_inputModeGet(name, id, NULL) != NULL) {
		errorf("Cannot add input mode \"%s\" multiple times.", name);
		return 1;
	}
	if(_inputModeGet(NULL, id, NULL) != NULL) {
		errorf("Cannot add multiple input modes with id %d", id);
		return 0;
	}

	mode = util_calloc(sizeof(flubInputMode_t), 0, NULL);
	mode->name = util_strdup(name);
	mode->id = id;

	if(_inputCtx.modes == NULL) {
		_inputCtx.modes = mode;
		return 1;
	}

	for(last = NULL, walk = _inputCtx.modes; walk != NULL; last = walk, walk = walk->next) {
		if(walk->id < id) {
			if(last == NULL) {
				mode->next = _inputCtx.modes;
				_inputCtx.modes = mode;
			} else {
				mode->next = last->next;
				last->next = mode;
			}
			return 1;
		}
	}
	last->next = mode;
	return 1;
}

int inputModeRemove(int id) {
	flubInputMode_t *mode, *previous = NULL;
	flubInputGroup_t *group;
	flubInputAction_t *action;

	if((mode = _inputModeGet(NULL, id, &previous)) == NULL) {
		if(_inputCtx.active == mode) {
			_inputCtx.active = NULL;
		}
		for(group = _inputCtx.groups; group != NULL; group = group->next) {
			for(action = group->actions; action != NULL; action = action->next) {
				if(action->mode == id) {
					errorf("Cannot remove input mode %d while actions are still present.", id);
					return 0;
				}
			}
		}
		if(previous == NULL) {
			_inputCtx.modes = mode->next;
		} else {
			previous->next = mode->next;
		}
		util_free(mode->name);
		util_free(mode);
		return 1;
	}
	return 0;
}

static flubInputGroup_t *_inputGroupGet(const char *name, flubInputGroup_t **previous) {
	flubInputGroup_t *group, *last = NULL;

	for(group = _inputCtx.groups; group != NULL; last = group, group = group->next) {
		if(previous != NULL) {
			*previous = last;
		}
		if(!strcmp(group->name, name)) {
			return group;
		}
	}
	if(previous != NULL) {
		*previous = last;
	}

	return NULL;
}

int inputGroupAdd(const char *group) {
	flubInputGroup_t *entry, *last;

	if((entry = _inputGroupGet(group, &last)) != NULL) {
		errorf("Cannout add input group \"%s\" multiple times.", group);
		return 0;
	}

	entry = util_calloc(sizeof(flubInputGroup_t), 0, NULL);
	entry->name = util_strdup(group);
    if(last == NULL) {
        _inputCtx.groups = entry;
    } else {
        last->next = entry;
    }

	return 1;
}

int inputGroupRemove(const char *group){
	flubInputGroup_t *gentry, *previous = NULL;
	const char *gname, *bind;

	if((gentry = _inputGroupGet(group, &previous)) == NULL) {
		while(gentry->actions != NULL) {
			inputActionUnregister(gentry->name, gentry->actions->bind);
		}
		if(previous == NULL) {
			_inputCtx.groups = gentry->next;
		} else {
			previous->next = gentry->next;
		}
		util_free(gentry->name);
		util_free(gentry);
		return 1;
	}
	return 0;	
}

static flubInputAction_t *_inputActionGet(const char *bindname,
										  flubInputGroup_t **group,
										  flubInputAction_t **previous) {
	flubInputGroup_t *gwalk;
	flubInputAction_t *awalk, *last = NULL;

	for(gwalk = _inputCtx.groups; gwalk != NULL; gwalk = gwalk->next) {
		for(awalk = gwalk->actions; awalk != NULL; last = awalk, awalk = awalk->next) {
			if(!strcmp(awalk->bind, bindname)) {
				if(previous != NULL) {
					*previous = last;
				}
				if(group != NULL) {
					*group = gwalk;
				}
				return awalk;
			}
		}
	}
	return NULL;
}

int inputActionRegister(const char *group, int mode, const char *bind,
						const char *display, flubInputActionHandler_t handler) {
	flubInputGroup_t *gentry;
	flubInputMode_t *mentry;
	flubInputAction_t *entry, *walk, *last;

    if(!_inputCtx.init) {
        error("Cannot register actions before input is initiated.");
        return 0;
    }

	if(_inputActionGet(bind, NULL, NULL) != NULL) {
		errorf("Cannot register input action \"%s\" multiple times.", bind);
		return 0;
	}

	if((gentry = _inputGroupGet(group, NULL)) == NULL) {
		errorf("Cannot register input action \"%s\" to non-existent group \"%s\".", bind, group);
		return 0;
	}

	if((mentry = _inputModeGet(NULL, mode, NULL)) == NULL) {
		errorf("Cannot register input action \"%s\" to non-existent mode %d.", bind, mode);
		return 0;
	}

	entry = util_calloc(sizeof(flubInputAction_t), 0, NULL);
	entry->bind = util_strdup(bind);
	entry->display = util_strdup(display);
	entry->handler = handler;

	for(last = NULL, walk = gentry->actions; walk != NULL; last = walk, walk = walk->next);

	if(last == NULL) {
		gentry->actions = entry;
	} else {
		last->next = entry;
	}

	last = NULL;
	if(!critbitInsert(&(_inputCtx.actionRegistry), bind, entry, ((void **)&last))) {
		errorf("Failed to add input action \"%s\" to the action registry.", bind);
		return 0;
	}
	if(last != NULL) {
		errorf("New input action \"%s\" already had an entry in the registry.", bind);
	}

	return 1;
}

int inputActionUnregister(const char *group, const char *bind) {
	flubInputGroup_t *gentry;
	flubInputAction_t *entry, *last;
	flubInputMode_t *mode;

	if((entry = _inputActionGet(bind, &gentry, &last)) == NULL) {
		errorf("Cannot unregister non-existent input action \"%s\".", bind);
		return 0;
	}

	if((mode = _inputModeGet(NULL, entry->mode, NULL)) == NULL) {
		errorf("Cannot register input action \"%s\" to non-existent mode %d.", bind, mode);
		return 0;
	}

	mode->eventMap[entry->event] = NULL;

	if(last == NULL) {
		gentry->actions = entry->next;
	} else {
		last->next = entry->next;
	}

	util_free(entry->bind);
	util_free(entry->display);
	util_free(entry);

	return 1;
}

int inputActionBindEvent(int index, const char *bind) {
	flubInputAction_t *entry;
	flubInputMode_t *mode;
    SDL_Scancode scancode;

    scancode = SDL_GetScancodeFromKey(index);

	if(critbitContains(&(_inputCtx.actionRegistry), bind, ((void **)&entry))) {
		if((mode = _inputModeGet(NULL, entry->mode, NULL)) == NULL) {
			errorf("Cannot bind input action \"%s\" with non-existent mode %d.", bind, mode);
			return 0;
		}

        // TODO Clear previous input binding
        if(entry->event != 0) {
        }
		mode->eventMap[scancode] = entry;
        entry->event = scancode;
        entry->mode = mode->id;

		return 1;
	}

	return 0;
}

int inputActionBind(const char *event, const char *bind) {
    int index;

    if((index = inputEventIndexFromBindName(event)) == 0) {
        infof("Failed to find event \"%s\"", event);
        return 0;
    }

    return inputActionBindEvent(index, bind);
}

int inputActionUnbind(const char *bind) {
	flubInputAction_t *entry;
	flubInputMode_t *mode;

	if(critbitContains(&(_inputCtx.actionRegistry), bind, ((void **)&entry))) {
		if((mode = _inputModeGet(NULL, entry->mode, NULL)) == NULL) {
			errorf("Cannot unbind input action \"%s\" with non-existent mode %d.", bind, mode);
			return 0;
		}
		mode->eventMap[entry->event] = NULL;
		return 1;
	}

	return 0;
}

const char *inputEventNameFromIndex(int index) {
	int k;

	for(k = 0; _flubInputEventTable[k].name != NULL; k++) {
		if(_flubInputEventTable[k].index == index) {
			return _flubInputEventTable[k].name;
		}
	}
	return NULL;
}

const char *inputEventBindNameFromIndex(int index) {
	int k;

	for(k = 0; _flubInputEventTable[k].name != NULL; k++) {
		if(_flubInputEventTable[k].index == index) {
			return _flubInputEventTable[k].bindname;
		}
	}
	return NULL;
}

const char *inputEventDisplayNameFromIndex(int index) {
	int k;

	for(k = 0; _flubInputEventTable[k].name != NULL; k++) {
		if(_flubInputEventTable[k].index == index) {
			return _flubInputEventTable[k].displayName;
		}
	}
	return NULL;
}

int inputEventIndexFromBindName(const char *name) {
	flubInputEventEntry_t *entry;

	if(critbitContains(&(_inputCtx.eventRegistry), name, ((void **)&entry))) {
		return entry->index;
	}
	return 0;
}

int inputEventFromAction(const char *bind) {
	flubInputAction_t *entry;

	if(!critbitContains(&(_inputCtx.actionRegistry), bind, ((void **)&entry))) {
		return entry->event;
	}
	return 0;
}

int inputBindingsExport(const char *fname) {
	flubInputGroup_t *group;
	flubInputAction_t *action;
	const char *event;
	FILE *fp;

	if((fp = fopen(fname, "wt")) == NULL) {
		return 0;
	}

	for(group = _inputCtx.groups; group != NULL; group = group->next) {
		fprintf(fp, "# %s\n", group->name);
		for(action = group->actions; action != NULL; action = action->next) {
			event = inputEventNameFromIndex(action->event);
			fprintf(fp, "%s %s\n", event, action->bind);
		}
	}

	fclose(fp);

	return 1;
}

// TODO Load bindings using physfs
int inputBindingsImport(const char *fname) {
	char buf[MAX_INPUT_LINE];
	char *ptr, *ename;
	FILE *fp;

	if((fp = fopen(fname, "rt")) == NULL) {
		return 0;
	}

	while(!feof(fp)) {
		fgets(buf, sizeof(buf) - 1, fp);
		ename = trimStr(buf);
		for(ptr = ename; ((*ptr != '\0') && (!isspace(*ptr))); ptr++);
		if(*ptr == '\0') {
			continue;
		}
		*ptr = '\0';
		ptr++;
		ptr = trimStr(ptr);
		inputActionBind(ename, ptr);
	}

	fclose(fp);

	return 1;
}

typedef struct _inputEnumCBBundle_s {
    inputEnumCallback_t callback;
    void *context;
} _inputEnumCBBundle_t;

static int _inputEnumHelper(const char *name, void *data, void *context) {
    _inputEnumCBBundle_t *bundle = (_inputEnumCBBundle_t *)context;

    return(bundle->callback(name, bundle->context));
}

void inputEnumBindings(inputEnumCallback_t callback, void *context) {
    _inputEnumCBBundle_t bundle;

    bundle.callback = callback;
    bundle.context = context;
    critbitAllPrefixed(&(_inputCtx.eventRegistry), "", _inputEnumHelper, ((void *)(&(bundle))));
}

void inputEnumActions(inputEnumCallback_t callback, void *context) {
    _inputEnumCBBundle_t bundle;

    bundle.callback = callback;
    bundle.context = context;
    critbitAllPrefixed(&(_inputCtx.actionRegistry), "", _inputEnumHelper, ((void *)(&(bundle))));
}
