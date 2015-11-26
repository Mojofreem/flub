#ifndef _FLUB_AUDIO_HEADER_
#define _FLUB_AUDIO_HEADER_

#ifdef MACOSX
#include <SDL2/SDL.h>
#include <SDL_mixer.h>
#else // MACOSX
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#endif // MACOSX
#include <flub/app.h>


typedef Mix_Chunk sound_t;


int audioInit(appDefaults_t *defaults);

sound_t *audioSoundGet(const char *fname);
void audioSoundRelease(sound_t *sound);


#endif // _FLUB_AUDIO_HEADER_
