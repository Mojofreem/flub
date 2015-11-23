#ifndef _INCL_PHYSFS_MEMFILE
#define _INCL_PHYSFS_MEMFILE


#include <stddef.h>


typedef struct __PHYSFS_memfile {
	const char *path;
	const unsigned char *data;
	const size_t length;

	struct __PHYSFS_memfile *next;
} PHYSFS_memfile;

extern void (*PHYSFS_log_callback)(const char *message);


#endif // _INCL_PHYSFS_MEMFILE
