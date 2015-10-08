#ifndef _FLUB_PHYSFS_HEADER_
#define _FLUB_PHYSFS_HEADER_


#include <physfs.h>


#define DBG_FILE_DTL_GENERAL    1


int flubPhysfsInit(const char *appPath);
int flubPhysfsValid(void);
void flubPhysfsShutdown(void);

char *PHYSFS_gets(char *str, int maxlen, PHYSFS_File *handle);
int PHYSFS_fixedEof(PHYSFS_File *handle);


#endif // _FLUB_PHYSFS_HEADER_
