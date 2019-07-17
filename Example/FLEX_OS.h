#ifndef __FLEX_OS_H__
#define __FLEX_OS_H__

#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <string.h>
#include <pthread.h>
#endif

#include <time.h>

#ifdef _WIN32
#define FLEX_INFINITE INFINITE
#else
#define FLEX_INFINITE 0xFFFFFFFFUL
#endif

#ifdef _WIN32
typedef HANDLE FLEX_MUTEX;
typedef HANDLE FLEX_EVENT; /* Use event instead of CV on Windows */
#else
typedef pthread_mutex_t FLEX_MUTEX;
typedef pthread_cond_t  FLEX_EVENT; /* Use CV on Others */
#endif

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
 * Create Mutex Synchronization primitives
 *
 * @param Mutex Pointer to FLEX_MUTEX
 *
 * @return 0 if successful, an error code on failure
 */
int FLEX_CreateMutex(FLEX_MUTEX *Mutex);

/**
 * Delete Mutex primitives and release resources
 *
 * @param Mutex Pointer to FLEX_MUTEX
 *
 * @return 0 if successful, an error code on failure
 */
int FLEX_DeleteMutex(FLEX_MUTEX *Mutex);

/**
 * Create Event Synchronization primitives
 *
 * @param Event Pointer to FLEX_EVENT
 *
 * @return 0 if successful, an error code on failure
 *
 * @remark Event object on Windows and pthread_cond_t on others
 */
int FLEX_CreateEvent(FLEX_EVENT *Event);

/**
 * Delete Event primitives and release resources
 *
 * @param Event Pointer to FLEX_EVENT
 *
 * @return 0 if successful, an error code on failure
 */
int FLEX_DeleteEvent(FLEX_EVENT *Event);

/**
 * Lock a Mutex primitives
 *
 * @param Mutex        Pointer to FLEX_MUTEX
 * @param Milliseconds Timeout in milliseconds, or FLEX_INFINITE to wait until signaled
 * @param Tp           Absolute time to wait, or NULL to wait until signaled
 *
 * @return 0 if successful, an error code on failure
 */
#ifdef _WIN32
int FLEX_Mutex_Lock(FLEX_MUTEX *Mutex, uint32_t Milliseconds);
#else
int FLEX_Mutex_Lock(FLEX_MUTEX *Mutex, struct timespec *Tp);
#endif

/**
 * Unlock an Mutex primitives
 *
 * @param Mutex Pointer to FLEX_MUTEX
 *
 * @return 0 if successful, an error code on failure
 */
int FLEX_Mutex_Unlock(FLEX_MUTEX *Mutex);

/**
 * Wait an Event primitives
 *
 * @param Event        Pointer to FLEX_EVENT
 * @param Mutex        Pointer to FLEX_MUTEX
 * @param Milliseconds Timeout in milliseconds, or FLEX_INFINITE to wait until signaled
 * @param Tp           Absolute time to wait, or NULL to wait until signaled
 *
 * @return 0 if successful, an error code on failure
 */
#ifdef _WIN32
int FLEX_Event_Wait(FLEX_EVENT *Event, uint32_t Milliseconds);
#else
int FLEX_Event_Wait(FLEX_EVENT *Event, FLEX_MUTEX *Mutex, struct timespec *Tp);
#endif

/**
 * Signal an Event primitives
 *
 * @param Event Pointer to FLEX_EVENT
 *
 * @return 0 if successful, an error code on failure
 */
int FLEX_Event_Signal(FLEX_EVENT *Event);

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