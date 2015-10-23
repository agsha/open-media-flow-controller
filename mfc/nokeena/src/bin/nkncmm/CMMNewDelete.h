
/*
 * CMMNewDelete.h - Internal memory allocation interfaces
 */

#ifndef _CMM_NEWDELETE_H
#define _CMM_NEWDELETE_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void CMMAllocInit(int useFastAlloc);
void* CMMnew(size_t sz);
void CMMdelete(void* p);

#ifdef __cplusplus
}
#endif

#endif /* _CMM_NEWDELETE_H */

/*
 * End of CMMNewDelete.h
 */
