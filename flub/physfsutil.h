#ifndef _FLUB_PHYSFS_HEADER_
#define _FLUB_PHYSFS_HEADER_


#include <physfs.h>
#include <flub/app.h>


#define DBG_FILE_DTL_GENERAL    1
#define DBG_FILE_DTL_PHYSFS		2


int flubPhysfsInit(appDefaults_t *defaults);
int flubPhysfsValid(void);
void flubPhysfsShutdown(void);

char *PHYSFS_gets(char *str, int maxlen, PHYSFS_File *handle);
int PHYSFS_fixedEof(PHYSFS_File *handle);


#endif // _FLUB_PHYSFS_HEADER_
