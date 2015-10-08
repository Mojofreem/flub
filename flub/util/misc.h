#ifndef _FLUB_UTIL_MISC_HEADER_
#define _FLUB_UTIL_MISC_HEADER_


#define UNUSED __attribute__((unused))
#define NORETURN __attribute__((noreturn))
#define STRINGIFY2( x )     #x
#define STRINGIFY( x )      STRINGIFY2( x )

#ifdef WIN32
#	define PLATFORM_PATH_SEP	'\\'
#else // WIN32
#	define PLATFORM_PATH_SEP	'/'
#endif // WIN32

#ifdef WIN32
void MsgBox(const char *title, const char *fmt, ...);
#else // WIN32
#define MsgBox(t,f,...)
#endif // WIN32

const char *utilBasePath(const char *path);
#define SOURCE_FILE() basePath(__FILE__);

int utilNextPowerOfTwo( int x );
int utilNumBitsSet(unsigned int value);

void utilHexDump(void *ptr, int len);


#endif // _FLUB_UTIL_MISC_HEADER_
