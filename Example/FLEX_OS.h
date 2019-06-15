#ifndef __FLEX_OS_H__
#define __FLEX_OS_H__

#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __linux__
#include <time.h>
#include <string.h>
#endif

#ifdef _WIN32
#include <windows.h>
#endif

#include <pthread.h>

/////////////////////////////////////////////////////////////////////////////
//                                                                         //
// This file defines some OS dependent functions to support cross-platform //
// features. x86 and x64 platforms are supported.                          //
//                                                                         //
// Windows and Linux are currently supported. Porting to other OS could be //
// achieved by re-implementing these functions in this file.               //
//                                                                         //
/////////////////////////////////////////////////////////////////////////////

/**
 * Get timespec cross-platform
 *
 * @param Tp [OUT] Pointer to timespec (not NULL)
 *
 * @return zero on success, -1 on failure
 */
int FLEX_Timespec_Get(struct timespec *Tp);

/**
 * Malloc address aligned memory
 *
 * @param Size      Size bytes to allocate (> 0)
 * @param Alignment Memory alignment (power of 2)
 *
 * @return Memory pointer on success, NULL on failure
 */
void *FLEX_Aligned_Malloc(size_t Size, size_t Alignment);

/**
 * Free memory allocated by FLEX_Aligned_Malloc
 *
 * @param Memory Pointer to memory
 *
 * @return void
 */
void FLEX_Aligned_Free(void *Memory);

#endif // __FLEX_OS_H__