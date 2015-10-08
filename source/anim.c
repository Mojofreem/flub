#include <flub/anim.h>
#include <flub/logger.h>
#include <stdlib.h>


/*
	rate = range / duration
	adjust = rate * elapsed
	ticks = rate / adjust
	remainder = elapsed - ticks
*/

/*
typedef struct flubAnim1Pti_s {
	int ptBegin;
	int ptEnd;
	Uint32 duration;
	Uint32 current;
	float rate;
} flubAnim1Pti_t;
*/

void flubAnimInit1Pti(flubAnim1Pti_t *anim, int begin, int end, Uint32 duration) {
	int diff = abs(end - begin);

	anim->ptBegin = begin;
	anim->ptEnd = end;
	anim->duration = duration;
	anim->current = 0;
	anim->rate = (((float)diff) / ((float)duration));
}

int flubAnimTick1Pti(flubAnim1Pti_t *anim, Uint32 elapsedTicks, int *pt1) {
	anim->current += elapsedTicks;
	if(anim->current >= anim->duration) {
		anim->current = anim->duration;
		*pt1 = anim->ptEnd;
		return 1;
	} else {
		if(anim->ptBegin > anim->ptEnd) {
			*pt1 = anim->ptBegin - (anim->rate * ((float)anim->current));
		} else {
			*pt1 = anim->ptBegin + (anim->rate * ((float)anim->current));			
		}
	}
	return 0;
}

void flubAnimReverse1Pti(flubAnim1Pti_t *anim) {
	int swap;
	swap = anim->ptEnd;
	anim->ptEnd = anim->ptBegin;
	anim->ptBegin = swap;
	anim->current = anim->duration - anim->current;
}

#if 0
typedef struct flubAnim2Pti_s {
	int pt1Begin;
	int pt1End;
	int pt2Begin;
	int pt2End;
	Uint32 duration;
	Uint32 current;
	float rate;
} flubAnim2Pti_t;

flubAnimInit2Pti(flubAnim2Pti_t *anim, int pt1begin, int pt1end,
				 int pt2begin, int pt2end, Uint32 duration);
flubAnimTick2Pti(flubAnim2Pti_t *anim, Uint32 elapsedTicks, int *pt1, int *pt2);

typedef struct flubAnim1Ptf_s {
	float ptBegin;
	float ptEnd;
	Uint32 duration;
	Uint32 current;
	float rate;
} flubAnim1Ptf_t;

flubAnimInit1Ptf(flubAnim1Ptf_t *anim, float begin, float end, Uint32 duration);
flubAnimTick1Ptf(flubAnim1Ptf_t *anim, Uint32 elapsedTicks, float *pt1);

typedef struct flubAnim3Ptf_s {
	float pt1Begin;
	float pt1End;
	float pt2Begin;
	float pt2End;
	float pt3Begin;
	float pt3End;
	Uint32 duration;
	Uint32 current;
	float rate;
} flubAnim3Ptf_t;

flubAnimInit3Ptf(flubAnim3Ptf_t *anim, float pt1begin, float pt1end,
				 float pt2begin, float pt2end, float pt3begin, float pt3end,
				 Uint32 duration);
flubAnimTick3Ptf(flubAnim3Ptf_t *anim, Uint32 elapsedTicks,
				 float *pt1, float *pt2, float *pt3);

typedef struct flubAnim4Ptf_s {
	float pt1Begin;
	float pt1End;
	float pt2Begin;
	float pt2End;
	float pt3Begin;
	float pt3End;
	float pt4Begin;
	float pt4End;
	Uint32 duration;
	Uint32 current;
	float rate;
} flubAnim4Ptf_t;

flubAnimInit4Ptf(flubAnim4Ptf_t *anim, float pt1begin, float pt1end,
				 float pt2begin, float pt2end, float pt3begin, float pt3end,
				 float pt4begin, float pt4end, Uint32 duration);
flubAnimTick4Ptf(flubAnim4Ptf_t *anim, Uint32 elapsedTicks,
				 float *pt1, float *pt2, float *pt3, float *pt4);

#endif

Uint32 flubAnimTicksElapsed(Uint32 *lastTick, Uint32 currentTick) {
	if(*lastTick == 0) {
		*lastTick = currentTick;
		return 0;
	}
	Uint32 elapsed = currentTick - *lastTick;
	
	*lastTick = currentTick;
	return elapsed;
}

