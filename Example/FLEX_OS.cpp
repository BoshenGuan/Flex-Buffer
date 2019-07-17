#include "stdafx.h"

#include "FLEX_OS.h"

int FLEX_CreateMutex(FLEX_MUTEX *Mutex)
{
#ifdef _WIN32
    HANDLE hMutex = CreateMutex(NULL, FALSE, NULL);

    if (hMutex == NULL)
    {
        return (int)GetLastError(); /* Not zero */
    }

    *Mutex = hMutex;

    return 0;
#else
    return pthread_mutex_init(Mutex, NULL);
#endif
}

int FLEX_DeleteMutex(FLEX_MUTEX *Mutex)
{
#ifdef _WIN32
    HANDLE hMutex = *Mutex;

    if (hMutex)
    {
        CloseHandle(hMutex);
    }

    return 0;
#else
    return pthread_mutex_destroy(Mutex);
#endif
}

int FLEX_CreateEvent(FLEX_EVENT *Event)
{
#ifdef _WIN32
    HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    if (hEvent == NULL)
    {
        return (int)GetLastError();  /* Not zero */
    }

    *Event = hEvent;

    return 0;
#else
    return pthread_cond_init(Event, NULL);
#endif
}

int FLEX_DeleteEvent(FLEX_EVENT *Event)
{
#ifdef _WIN32
    HANDLE hEvent = *Event;

    if (hEvent)
    {
        CloseHandle(hEvent);
    }

    return 0;
#else
    return pthread_cond_destroy(Event);
#endif
}

#ifdef _WIN32
int FLEX_Mutex_Lock(FLEX_MUTEX *Mutex, uint32_t Milliseconds)
#else
int FLEX_Mutex_Lock(FLEX_MUTEX *Mutex, struct timespec *Tp)
#endif
{
#ifdef _WIN32
    HANDLE hMutex = *Mutex;

    DWORD Ret = WaitForSingleObject(hMutex, Milliseconds);

    if (Ret != WAIT_OBJECT_0 && Ret != WAIT_ABANDONED)
    {
        return Ret;
    }

    return 0;
#else
    if (Tp)
    {
        return pthread_mutex_timedlock(Mutex, Tp);
    }
    else
        return pthread_mutex_lock(Mutex);
#endif
}

int FLEX_Mutex_Unlock(FLEX_MUTEX *Mutex)
{
#ifdef _WIN32
    HANDLE hMutex = *Mutex;

    BOOL Ret = ReleaseMutex(hMutex);

    if (Ret)
    {
        return 0;
    }
    else
        return (int)GetLastError();
#else
    return pthread_mutex_unlock(Mutex);
#endif
}

#ifdef _WIN32
int FLEX_Event_Wait(FLEX_EVENT *Event, uint32_t Milliseconds)
#else
int FLEX_Event_Wait(FLEX_EVENT *Event, FLEX_MUTEX *Mutex, struct timespec *Tp)
#endif
{
#ifdef _WIN32
    HANDLE hEvent = *Event;

    DWORD Ret = WaitForSingleObject(hEvent, Milliseconds);

    if (Ret != WAIT_OBJECT_0 && Ret != WAIT_ABANDONED)
    {
        return Ret;
    }

    return 0;
#else
    if (Tp)
    {
        return pthread_cond_timedwait(Event, Mutex, Tp);
    }
    else
        return pthread_cond_wait(Event, Mutex);
#endif
}

int FLEX_Event_Signal(FLEX_EVENT *Event)
{
#ifdef _WIN32
    HANDLE hEvent = *Event;

    BOOL Ret = SetEvent(hEvent);

    if (Ret)
    {
        return 0;
    }
    else
        return (int)GetLastError();
#else
    return pthread_cond_signal(Event);
#endif
}

void * FLEX_Aligned_Malloc(size_t Size, size_t Alignment)
{
#ifdef _WIN32
    return _aligned_malloc(Size, Alignment);
#else
    void *Ptr;

    int Ret = posix_memalign(&Ptr, Alignment, Size);

    if (Ret)
        return NULL;

    return Ptr;
#endif

    return NULL;
}

void FLEX_Aligned_Free(void *Memory)
{
#ifdef _WIN32
    _aligned_free(Memory);
#else
    free(Memory);
#endif
}