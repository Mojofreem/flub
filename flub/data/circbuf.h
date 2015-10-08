#ifndef _FLUB_CIRCBUF
#define _FLUB_CIRCBUF


typedef void circularBuffer_t;


typedef int (*circBufEntryEnumCB_t)(int entry, void *ptr, int length);

circularBuffer_t *circBufInit(int size, int maxCount, int useIndex);
int circBufOverrun(circularBuffer_t *buf);
void circBufClear(circularBuffer_t *buf);
void *circBufAllocNextEntryPtr(circularBuffer_t *buf, int size);
int circBufEnumEntries(circularBuffer_t *buf, circBufEntryEnumCB_t callback,
					   int begin, int end);
int circBufGetCount(circularBuffer_t *buf);
int circBufGetEntry(circularBuffer_t *buf, int entry, void **bufptr, int *size);


#endif // _FLUB_CIRCBUF
