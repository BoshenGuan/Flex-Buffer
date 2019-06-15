#include "stdafx.h"

#include "FLEX_OS.h"

#ifdef _WIN32
#pragma comment(lib, "pthreadVC2.lib")
#endif

int FLEX_Timespec_Get(struct timespec *Tp)
{
#ifdef _WIN32

#define EPOCHFILETIME   116444736000000000LL    // 1 Jan 1601 to 1 Jan 1970
#define SECONDS                   10000000LL    // From 100 nano seconds to 1 second
    //
    // Get timespec on Windows
    //
    // The solution use GetSystemTimeAsFileTime to retrive system time
    // and convert it to timespec based from 1 Jan 1970.
    //
    // See: https://stackoverflow.com/questions/5404277/porting-clock-gettime-to-windows
    //

    FILETIME FileTime;
    GetSystemTimeAsFileTime(&FileTime);

    ULARGE_INTEGER Time;
    Time.HighPart = FileTime.dwHighDateTime;
    Time.LowPart  = FileTime.dwLowDateTime;

    Time.QuadPart -= EPOCHFILETIME;

    Tp->tv_sec  = Time.QuadPart / SECONDS;
    Tp->tv_nsec = Time.QuadPart % SECONDS * 100;

    return 0;
#endif

#ifdef __linux__
    return clock_gettime(CLOCK_REALTIME, Tp);
#endif

    return -1;
}

void * FLEX_Aligned_Malloc(size_t Size, size_t Alignment)
{
#ifdef _WIN32
    return _aligned_malloc(Size, Alignment);
#endif

#ifdef __linux__
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
#endif

#ifdef __linux__
    free(Memory);
#endif
}
