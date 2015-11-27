#include <flub/logger.h>
#include <flub/audio.h>
#include <flub/physfsrwops.h>
//#include <flub/util.h>
#include <stdlib.h>
#include <string.h>
#include <flub/memory.h>
#include <PHYSFS_memfile.h>
#include <flub/module.h>


/////////////////////////////////////////////////////////////////////////////
// audio module registration
/////////////////////////////////////////////////////////////////////////////

int flubAudioInit(appDefaults_t *defaults);

static char *_initDeps[] = {"sdl", NULL};
flubModuleCfg_t flubModuleAudio = {
	.name = "audio",
	.init = flubAudioInit,
	.initDeps = _initDeps,
};

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

#define DBG_AUD_DTL_GENERAL 1


typedef struct flubAudioRegEntry_s flubAudioRegEntry_t;
struct flubAudioRegEntry_s {
	const char *name;
	sound_t *sound;
	int count;

	flubAudioRegEntry_t *next;
};


struct {
	int init;
	flubAudioRegEntry_t *registry;
} _audioCtx = {
		.init = 0,
		.registry = NULL,
	};


static void _audioShutdown(void) {
	Mix_CloseAudio();
}

int flubAudioInit(appDefaults_t *defaults) {
	if(_audioCtx.init) {
		return 1;
	}

	if(Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT, 2, 1024) == -1) {
		fatalf("Failed to initialize SDL_Mixer: %s", Mix_GetError());
		return 0;
	}

	// By default, we allocate 16 channels for mixing. The program may
	// reallocate at any time.
	Mix_AllocateChannels(16);

	// One channel is reserved for the system UI. The program may reserve
	// additional channels, but should always leave channel 0 for Flub.
	Mix_ReserveChannels(1);

	atexit(_audioShutdown);

	logDebugRegister("audio", DBG_AUDIO, "general", DBG_AUD_DTL_GENERAL);

	_audioCtx.init = 1;

	return 1;
}

static void _audioCacheSound(const char *name, sound_t *sound) {
	flubAudioRegEntry_t *walk, *last = NULL, *entry;

	entry = util_calloc(sizeof(flubAudioRegEntry_t), 0, NULL);
	entry->name = util_strdup(name);
	entry->sound = sound;
	entry->count = 1;

	for(walk = _audioCtx.registry; walk != NULL; last = walk, walk = walk->next) {
		if(strcmp(name, walk->name) > 0) {
			if(last == NULL) {
				entry->next = _audioCtx.registry;
				_audioCtx.registry = entry;
			} else {
				last->next = entry;
			}
			return;
		}
	}
	if(last == NULL) {
		entry->next = _audioCtx.registry;
		_audioCtx.registry = entry;
	} else {
		last->next = entry;
	}
}

sound_t *audioSoundGet(const char *fname) {
	flubAudioRegEntry_t *walk;
	PHYSFS_memfile *memfile;
	SDL_RWops *rwops;
	sound_t *sound = NULL;

	debugf(DBG_AUDIO, DBG_AUD_DTL_GENERAL, "Loading sound \"%s\"", fname);

	for(walk = _audioCtx.registry; walk != NULL; walk = walk->next) {
		if(!strcmp(walk->name, fname)) {
			debugf(DBG_AUDIO, DBG_AUD_DTL_GENERAL, "Sound \"%s\" found in audio cache", fname);
			walk->count++;
			return walk->sound;
		}
	}

    if((rwops = PHYSFSRWOPS_openRead(fname)) == NULL) {
		errorf("Unable to locate sound file \"%s\"", fname);
		return NULL;
	}

	debugf(DBG_AUDIO, DBG_AUD_DTL_GENERAL, "Reading sound \"%s\"", fname);
	sound = Mix_LoadWAV_RW(rwops, 1);
	if(sound == NULL) {
		errorf("Failed to load the sound \"%s\": %s", fname, Mix_GetError());
	}

	debugf(DBG_AUDIO, DBG_AUD_DTL_GENERAL, "Sound \"%s\" loaded", fname);

	_audioCacheSound(fname, sound);

	return sound;
}

void audioSoundRelease(sound_t *sound) {
	flubAudioRegEntry_t *walk;

	for(walk = _audioCtx.registry; walk != NULL; walk = walk->next) {
		if(walk->sound == sound) {
			walk->count--;
			if(walk->count == 0) {
				// TODO Delete cached audio sample
			}
			return;
		}
	}	
}

