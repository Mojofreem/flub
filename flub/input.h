#ifndef _FLUB_INPUT_HEADER_
#define _FLUB_INPUT_HEADER_


#include <SDL2/SDL.h>


typedef void (*flubInputActionHandler_t)(SDL_Event *event, int pressed, int motion, int x, int y);
typedef int (*flubInputEventTrap_t)(SDL_Event *event, int index);


void inputMousePosSet(int x, int y);
void inputMousePosGet(int *x, int *y);

int inputModeSet(int mode);

void inputEventTrapCapture(flubInputEventTrap_t handler);
void inputEventTrapRelease(flubInputEventTrap_t handler);

void inputEventCapture(flubInputActionHandler_t handler);
void inputEventRelease(flubInputActionHandler_t handler);

void inputMouseCapture(flubInputActionHandler_t handler);
void inputMouseRelease(flubInputActionHandler_t handler);

void inputMouseCursor(int show);

int inputPollInit(int timeout);
int inputPollEvent(SDL_Event *event);
int inputWaitEventTimeout(SDL_Event *event, int timeout);
int inputEventProcess(SDL_Event *event);
int inputUpdate(Uint32 ticks, Uint32 elapsed);

int inputModeAdd(const char *name, int id);
int inputModeRemove(int id);

int inputGroupAdd(const char *group);
int inputGroupRemove(const char *group);

int inputActionRegister(const char *group, int mode, const char *bind,
						const char *display, flubInputActionHandler_t handler);
int inputActionUnregister(const char *group, const char *bind);

int inputActionBindEvent(int index, const char *bind);
int inputActionBind(const char *event, const char *bind);
int inputActionUnbind(const char *bind);

const char *inputEventNameFromIndex(int index);
const char *inputEventBindNameFromIndex(int index);
const char *inputEventDisplayNameFromIndex(int index);
int inputEventIndexFromBindName(const char *name);
int inputEventFromAction(const char *bind);

int inputBindingsExport(const char *fname);
int inputBindingsImport(const char *fname);

typedef int (*inputEnumCallback_t)(const char *name, void *context);

void inputEnumBindings(inputEnumCallback_t callback, void *context);
void inputEnumActions(inputEnumCallback_t callback, void *context);


#endif // _FLUB_INPUT_HEADER_
