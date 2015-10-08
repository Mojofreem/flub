#ifndef _FLUB_ANIM_HEADER_
#define _FLUB_ANIM_HEADER_


#include <SDL2/SDL.h>

#define _FLUB_ANIM_4PT_STRUCT(sname,stype)  struct { stype pt1; stype pt2; stype pt3; stype pt4; } sname

typedef union {
    _FLUB_ANIM_4PT_STRUCT(_uint32,Uint32);
    _FLUB_ANIM_4PT_STRUCT(_sint32,Sint32);
    _FLUB_ANIM_4PT_STRUCT(_uint8,Uint8);
    _FLUB_ANIM_4PT_STRUCT(_float,float);
} flubAnimVals_t;

#define _FLUB_ANIM_4PT_PTR_STRUCT(sname,stype)  struct { stype pt1; stype pt2; stype pt3; stype pt4; } sname

typedef union {
    _FLUB_ANIM_4PT_PTR_STRUCT(_uint32,Uint32);
    _FLUB_ANIM_4PT_PTR_STRUCT(_sint32,Sint32);
    _FLUB_ANIM_4PT_PTR_STRUCT(_uint8,Uint8);
    _FLUB_ANIM_4PT_PTR_STRUCT(_float,float);
} flubAnimVars_t;

#define FLUB_ANIM_FLAG_1PT      0x01
#define FLUB_ANIM_FLAG_2PT      0x02
#define FLUB_ANIM_FLAG_3PT      0x04
#define FLUB_ANIM_FLAG_4PT      0x08
#define FLUB_ANIM_PT_MASK       (FLUB_ANIM_FLAG_1PT | FLUB_ANIM_FLAG_2PT | FLUB_ANIM_FLAG_3PT | FLUB_ANIM_FLAG_4PT)

#define FLUB_ANIM_FLAG_INT32    0x10
#define FLUB_ANIM_FLAG_UINT32   0x20
#define FLUB_ANIM_FLAG_UINT8    0x40
#define FLUB_ANIM_FLAG_FLOAT    0x80
#define FLUB_ANIM_SIZE_MASK     (FLUB_ANIM_FLAG_INT32 | FLUB_ANIM_FLAG_UINT32 | FLUB_ANIM_FLAG_UINT8 | FLUB_ANIM_FLAG_FLOAT)

#define FLUB_ANIM_LERP_SINE     0x100

typedef struct flubAnimState_s flubAnimState_t;
struct flubAnimState_s {
    flubAnimVals_t initial;
    flubAnimVals_t target;
    flubAnimVars_t vars;
    Uint32 duration;
    Uint32 elapsed;
    flubAnimState_t *trigger;
    Uint32 triggerTime;
    Uint32 flags;
    flubAnimState_t *next;
};

typedef struct flubAnimToken_s flubAnimToken_t;
struct flubAnimToken_s {
    flubAnimState_t *state;
    flubAnimState_t *home;
};


int flubAnimInit(void);
int flubAnimIsValid(void);
void flubAnimShutdown(void);

flubAnimState_t *flubAnimStateCreate1PtUint32(Uint32 initPt1, Uint32 targetPt1, Uint32 *varPt1, Uint32 duration);
flubAnimState_t *flubAnimStateCreate2PtUint32(Uint32 initPt1, Uint32 initPt2, Uint32 targetPt1, Uint32 targetPt2, Uint32 *varPt1, Uint32 *varPt2, Uint32 duration);
flubAnimState_t *flubAnimStateCreate3PtUint32(Uint32 initPt1, Uint32 initPt2, Uint32 initPt3, Uint32 targetPt1, Uint32 targetPt2, Uint32 targetPt3, Uint32 *varPt1, Uint32 *varPt2, Uint32 *varPt3, Uint32 duration);
flubAnimState_t *flubAnimStateCreate4PtUint32(Uint32 initPt1, Uint32 initPt2, Uint32 initPt3, Uint32 initPt4, Uint32 targetPt1, Uint32 targetPt2, Uint32 targetPt3, Uint32 targetPt4, Uint32 *varPt1, Uint32 *varPt2, Uint32 *varPt3, Uint32 *varPt4, Uint32 duration);

flubAnimState_t *flubAnimStateCreate1PtSint32(Sint32 initPt1, Sint32 targetPt1, Sint32 *varPt1, Uint32 duration);
flubAnimState_t *flubAnimStateCreate2PtSint32(Sint32 initPt1, Sint32 initPt2, Sint32 targetPt1, Sint32 targetPt2, Sint32 *varPt1, Sint32 *varPt2, Uint32 duration);
flubAnimState_t *flubAnimStateCreate3PtSint32(Sint32 initPt1, Sint32 initPt2, Sint32 initPt3, Sint32 targetPt1, Sint32 targetPt2, Sint32 targetPt3, Sint32 *varPt1, Sint32 *varPt2, Sint32 *varPt3, Uint32 duration);
flubAnimState_t *flubAnimStateCreate4PtSint32(Sint32 initPt1, Sint32 initPt2, Sint32 initPt3, Sint32 initPt4, Sint32 targetPt1, Sint32 targetPt2, Sint32 targetPt3, Sint32 targetPt4, Sint32 *varPt1, Sint32 *varPt2, Sint32 *varPt3, Sint32 *varPt4, Uint32 duration);

flubAnimState_t *flubAnimStateCreate1PtUint8(Uint8 initPt1, Uint8 targetPt1, Uint8 *varPt1, Uint32 duration);
flubAnimState_t *flubAnimStateCreate2PtUint8(Uint8 initPt1, Uint8 initPt2, Uint8 targetPt1, Uint8 targetPt2, Uint8 *varPt1, Uint8 *varPt2, Uint32 duration);
flubAnimState_t *flubAnimStateCreate3PtUint8(Uint8 initPt1, Uint8 initPt2, Uint8 initPt3, Uint8 targetPt1, Uint8 targetPt2, Uint8 targetPt3, Uint8 *varPt1, Uint8 *varPt2, Uint8 *varPt3, Uint32 duration);
flubAnimState_t *flubAnimStateCreate4PtUint8(Uint8 initPt1, Uint8 initPt2, Uint8 initPt3, Uint8 initPt4, Uint8 targetPt1, Uint8 targetPt2, Uint8 targetPt3, Uint8 targetPt4, Uint8 *varPt1, Uint8 *varPt2, Uint8 *varPt3, Uint8 *varPt4, Uint32 duration);

flubAnimState_t *flubAnimStateCreate1PtFloat(float initPt1, float targetPt1, float *varPt1, Uint32 duration);
flubAnimState_t *flubAnimStateCreate2PtFloat(float initPt1, float initPt2, float targetPt1, float targetPt2, float *varPt1, float *varPt2, Uint32 duration);
flubAnimState_t *flubAnimStateCreate3PtFloat(float initPt1, float initPt2, float initPt3, float targetPt1, float targetPt2, float targetPt3, float *varPt1, float *varPt2, float *varPt3, Uint32 duration);
flubAnimState_t *flubAnimStateCreate4PtFloat(float initPt1, float initPt2, float initPt3, float initPt4, float targetPt1, float targetPt2, float targetPt3, float targetPt4, float *varPt1, float *varPt2, float *varPt3, float *varPt4, Uint32 duration);

void flubAnimStateDestroy(flubAnimState_t *state);

int flubAnimStateAddPeer(flubAnimState_t *state, flubAnimState_t *peer);
int flubAnimStateAddTrigger(flubAnimState_t *state, flubAnimState_t *trigger, int ticks);

void flubAnimStateReset(flubAnimState_t *state);
int flubAnimStateIsDone(flubAnimState_t *state);

flubAnimToken_t *flubAnimTokenCreate(flubAnimState_t *state);
void flubAnimTokenDestroy(flubAnimToken_t *token);

void flubAnimUpdate(Uint32 ticks);

void flubAnimTokenRegister(flubAnimToken_t *token);
void flubAnimTokenUnregister(flubAnimToken_t *token);


typedef struct flubAnim1Pti_s {
	int ptBegin;
	int ptEnd;
	Uint32 duration;
	Uint32 current;
	float rate;
} flubAnim1Pti_t;


void flubAnimInit1Pti(flubAnim1Pti_t *anim, int begin, int end, Uint32 duration);
int flubAnimTick1Pti(flubAnim1Pti_t *anim, Uint32 elapsedTicks, int *pt1);
void flubAnimReverse1Pti(flubAnim1Pti_t *anim);

typedef struct flubAnim2Pti_s {
	int pt1Begin;
	int pt1End;
	int pt2Begin;
	int pt2End;
	Uint32 duration;
	Uint32 current;
	float rate;
} flubAnim2Pti_t;

void flubAnimInit2Pti(flubAnim2Pti_t *anim, int pt1begin, int pt1end,
				 int pt2begin, int pt2end, Uint32 duration);
void flubAnimTick2Pti(flubAnim2Pti_t *anim, Uint32 elapsedTicks, int *pt1, int *pt2);

typedef struct flubAnim1Ptf_s {
	float ptBegin;
	float ptEnd;
	Uint32 duration;
	Uint32 current;
	float rate;
} flubAnim1Ptf_t;

void flubAnimInit1Ptf(flubAnim1Ptf_t *anim, float begin, float end, Uint32 duration);
void flubAnimTick1Ptf(flubAnim1Ptf_t *anim, Uint32 elapsedTicks, float *pt1);

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

void flubAnimInit3Ptf(flubAnim3Ptf_t *anim, float pt1begin, float pt1end,
				 float pt2begin, float pt2end, float pt3begin, float pt3end,
				 Uint32 duration);
void flubAnimTick3Ptf(flubAnim3Ptf_t *anim, Uint32 elapsedTicks,
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

void flubAnimInit4Ptf(flubAnim4Ptf_t *anim, float pt1begin, float pt1end,
				 float pt2begin, float pt2end, float pt3begin, float pt3end,
				 float pt4begin, float pt4end, Uint32 duration);
void flubAnimTick4Ptf(flubAnim4Ptf_t *anim, Uint32 elapsedTicks,
				 float *pt1, float *pt2, float *pt3, float *pt4);


Uint32 flubAnimTicksElapsed(Uint32 *lastTick, Uint32 currentTick);


#endif // _FLUB_ANIM_HEADER_

