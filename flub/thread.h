#ifndef _FLUB_THREADS_HEADER_
#define _FLUB_THREADS_HEADER_


#include <SDL2/SDL.h>


typedef SDL_sem semaphore_t;
typedef SDL_mutex mutex_t;
typedef SDL_Thread thread_t;


#define semaphoreCreate(value)       SDL_CreateSemaphore(value)
#define semaphoreDestroy(semaphore)  SDL_DestroySemaphore(semaphore)
#define semaphoreWait(semaphore)     SDL_SemWait(semaphore)
#define semaphoreTry(semaphore)      SDL_SemTryWait(semaphore)
#define semaphoreRelease(semaphore)  SDL_SemPost(semaphore)

#define mutexCreate()                SDL_CreateMutex()
#define mutexDestroy(mutex)          SDL_DestroyMutex(mutex)
#define mutexGrab(mutex)             SDL_LockMutex(mutex)
#define mutexRelease(mutex)          SDL_UnlockMutex(mutex)

#define threadCreate(function,name,extra) SDL_CreateThread(function,name,extra)
#define threadWait(thread,status)         SDL_WaitThread(thread,status)
#define threadDetach(thread)              SDL_DetachThread(thread)


#endif // _FLUB_THREADS_HEADER_
