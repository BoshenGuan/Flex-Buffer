#include "stdafx.h"

#include "FLEX.h"
#include "FLEX_OS.h"

typedef struct FLEX_RANGE
{
    uint8_t    * Data;
    size_t       Size;
    FLEX_RANGE * Next;

} FLEX_RANGE;

typedef struct FLEX_BUFFER
{
    uint8_t *       Data;
    size_t          Size;
    size_t          Position;       /* Index of free buffer */
    size_t          Length;         /* Free buffer length, 0 if no buffer available */
    size_t          Alignment;

    FLEX_MUTEX      Mutex;
    FLEX_EVENT      Event[2];		/* [0] - WR / [1] - RD */

    FLEX_RANGE      Range[2][2];    /* The buffer may be divided into two parts */
    bool            Dequeued[2];

} FLEX_BUFFER;

FLEX_BUFFER *FLEX_CreateBuffer(size_t Size, size_t Alignment)
{
    size_t i;

    if (!Size)
    {
        return NULL;
    }

    FLEX_BUFFER *FlexBuffer = (FLEX_BUFFER *)calloc(1, sizeof(FLEX_BUFFER));

    if (!FlexBuffer)
    {
        return NULL;
    }

    int Ret = FLEX_CreateMutex(&FlexBuffer->Mutex);

    if (Ret)
    {
        FLEX_DeleteBuffer(FlexBuffer);
        return NULL;
    }

    for (i = 0; i < 2; i++)
    {
        Ret = FLEX_CreateEvent(&FlexBuffer->Event[i]);
        if (Ret)
        {
            FLEX_DeleteBuffer(FlexBuffer);
            return NULL;
        }
    }
    
    FlexBuffer->Length = Size;
    FlexBuffer->Size = Size;
    FlexBuffer->Alignment = Alignment;

    if (Alignment)
    {
        FlexBuffer->Data = (uint8_t *)FLEX_Aligned_Malloc(Size, Alignment);
    }
    else
        FlexBuffer->Data = (uint8_t *)malloc(Size);

    if (!FlexBuffer->Data)
    {
        FLEX_DeleteBuffer(FlexBuffer);
        return NULL;
    }

    return FlexBuffer;
}

void FLEX_DeleteBuffer(FLEX_BUFFER *FlexBuffer)
{
    size_t i;

    if (!FlexBuffer)
    {
        return;
    }

    FLEX_DeleteMutex(&FlexBuffer->Mutex);

    for (i = 0; i < 2; i++)
    {
        FLEX_DeleteEvent(&FlexBuffer->Event[i]);
    }

    if (FlexBuffer->Data)
    {
        if (FlexBuffer->Alignment)
        {
            FLEX_Aligned_Free(FlexBuffer->Data);
        }
        else
            free(FlexBuffer->Data);
    }

    free(FlexBuffer);
}

void FLEX_RestoreBuffer(FLEX_BUFFER *FlexBuffer)
{
    size_t i, j;

    if (!FlexBuffer)
    {
        return;
    }

    FlexBuffer->Position = 0;
    FlexBuffer->Length = FlexBuffer->Size;

    for (i = 0; i < 2; i++)
    {
        for (j = 0; j < 2; j++)
        {
            memset(&FlexBuffer->Range[i][j], 0, sizeof(FLEX_RANGE));
        }
    }

    FlexBuffer->Dequeued[0] = false;
    FlexBuffer->Dequeued[1] = false;
}

FLEX_RANGE *FLEX_GetWrBuffer(FLEX_BUFFER *FlexBuffer, size_t Length, bool Partial, uint32_t Milliseconds)
{
    if (!FlexBuffer || !Length)
    {
        return NULL;
    }

#ifdef _WIN32
    int Ret = FLEX_Mutex_Lock(&FlexBuffer->Mutex, FLEX_INFINITE);
#else
    int Ret = FLEX_Mutex_Lock(&FlexBuffer->Mutex, NULL);
#endif

    if (Ret)
        return NULL;

    if (FlexBuffer->Dequeued[0])
    {
        FLEX_Mutex_Unlock(&FlexBuffer->Mutex);
        return NULL;
    }
    
#ifdef _WIN32
    FILETIME FileTime;

    GetSystemTimeAsFileTime(&FileTime);
#else
    struct timespec Ts;

    Ret = clock_gettime(CLOCK_REALTIME, &Ts);

    if (Ret)
    {
        FLEX_Mutex_Unlock(&FlexBuffer->Mutex);
        return NULL;
    }
#endif

#ifdef _WIN32
    uint64_t Time = (uint64_t)FileTime.dwLowDateTime + (((uint64_t)FileTime.dwHighDateTime) << 32);

    /* FILETIME has a precision of 100-nano seconds */
    Time /= 10000ULL;

    if (Milliseconds != FLEX_INFINITE)
    {
        Time += Milliseconds; /* Wait will be terminated at Time */
    }
#else
    if (Milliseconds != FLEX_INFINITE)
    {
        uint64_t Nano = Ts.tv_nsec + Milliseconds * 1000000ULL;

        Ts.tv_sec += Nano / 1000000000ULL;
        Ts.tv_nsec = Nano % 1000000000ULL;
    }
#endif

    FLEX_RANGE *Range = NULL;

    int Result = 0;

    while (FlexBuffer->Length < Length && Result == 0)
    {
#ifdef _WIN32
        uint32_t Timeout = Milliseconds;

        if (Milliseconds != FLEX_INFINITE)
        {
            GetSystemTimeAsFileTime(&FileTime);

            uint64_t Now = (uint64_t)FileTime.dwLowDateTime + (((uint64_t)FileTime.dwHighDateTime) << 32);

            /* FILETIME has a precision of 100-nano seconds */
            Now /= 10000ULL;

            if (Now < Time)
            {
                /* Prevent from infinite wait if (Time - Now == INFINITE),
                 * which is probably possible on a multi-core CPU system. 
                 */
                Timeout = (uint32_t)min(Time - Now, FLEX_INFINITE - 1);
            }
            else
                Timeout = 0; /* Time in the past */
        }

        Ret = FLEX_Mutex_Unlock(&FlexBuffer->Mutex);

        if (Ret)
            break;

        /* There is no native CV implementation on Windows before 
         * Vista. CV is emulated on Windows with non-atomic mutex
         * unlock and wait. 
         *
         * Event state on Windows is preserved even if no wait is
         * on going, which is different from CV whose signal must
         * be sent when there goes a wait (or the signal would be
         * lost). 
         *
         * In this case, non-atomic operation should work with no
         * problem.
         */

        Result = FLEX_Event_Wait(&FlexBuffer->Event[0], Timeout);

        Ret = FLEX_Mutex_Lock(&FlexBuffer->Mutex, FLEX_INFINITE);

        if (Ret)
        {
            /* This should never happen in practice */
            return NULL;
        }
#else
        if (Milliseconds != FLEX_INFINITE)
        {
            Result = pthread_cond_timedwait(&FlexBuffer->Event[0], &FlexBuffer->Mutex, &Ts);
        }
        else
            Result = pthread_cond_wait(&FlexBuffer->Event[0], &FlexBuffer->Mutex);
#endif
    }

    size_t Actual = FlexBuffer->Length;

    if (Actual > Length)
    {
        Actual = Length;
    }

    if (Actual < Length && !Partial)
    {
        Actual = 0;
    }

    if (Actual)
    {
        /* Fill in range fields and return 
         * no allocation required 
         */
        Range = &FlexBuffer->Range[0][0];

        FlexBuffer->Range[0][0].Data = &FlexBuffer->Data[FlexBuffer->Position];

        if (FlexBuffer->Position + Actual <= FlexBuffer->Size)
        {
            FlexBuffer->Range[0][0].Size = Actual;
            FlexBuffer->Range[0][0].Next = NULL;
        }
        else
        {
            FlexBuffer->Range[0][0].Size = FlexBuffer->Size - FlexBuffer->Position;
            FlexBuffer->Range[0][0].Next = &FlexBuffer->Range[0][1];

            /* Wrap-around */
            FlexBuffer->Range[0][1].Data = &FlexBuffer->Data[0];
            FlexBuffer->Range[0][1].Size = FlexBuffer->Position + Actual - FlexBuffer->Size;
            FlexBuffer->Range[0][1].Next = NULL;
        }

        /* Dequeued */
        FlexBuffer->Dequeued[0] = true;
    }

    FLEX_Mutex_Unlock(&FlexBuffer->Mutex);

    return Range;
}

FLEX_RANGE *FLEX_GetRdBuffer(FLEX_BUFFER *FlexBuffer, size_t Length, bool Partial, uint32_t Milliseconds)
{
    if (!FlexBuffer || !Length)
    {
        return NULL;
    }

#ifdef _WIN32
    int Ret = FLEX_Mutex_Lock(&FlexBuffer->Mutex, FLEX_INFINITE);
#else
    int Ret = FLEX_Mutex_Lock(&FlexBuffer->Mutex, NULL);
#endif

    if (Ret)
        return NULL;

    if (FlexBuffer->Dequeued[1])
    {
        FLEX_Mutex_Unlock(&FlexBuffer->Mutex);
        return NULL;
    }

#ifdef _WIN32
    FILETIME FileTime;

    GetSystemTimeAsFileTime(&FileTime);
#else
    struct timespec Ts;

    Ret = clock_gettime(CLOCK_REALTIME, &Ts);

    if (Ret)
    {
        FLEX_Mutex_Unlock(&FlexBuffer->Mutex);
        return NULL;
    }
#endif

#ifdef _WIN32
    uint64_t Time = (uint64_t)FileTime.dwLowDateTime + (((uint64_t)FileTime.dwHighDateTime) << 32);

    /* FILETIME has a precision of 100-nano seconds */
    Time /= 10000ULL;

    if (Milliseconds != FLEX_INFINITE)
    {
        Time += Milliseconds;
    }
#else
    if (Milliseconds != FLEX_INFINITE)
    {
        uint64_t Nano = Ts.tv_nsec + Milliseconds * 1000000ULL;

        Ts.tv_sec += Nano / 1000000000ULL;
        Ts.tv_nsec = Nano % 1000000000ULL;
}
#endif

    FLEX_RANGE *Range = NULL;

    int Result = 0;

    while (FlexBuffer->Size - FlexBuffer->Length < Length && Result == 0)
    {
#ifdef _WIN32
        uint32_t Timeout = Milliseconds;

        if (Milliseconds != FLEX_INFINITE)
        {
            GetSystemTimeAsFileTime(&FileTime);

            uint64_t Now = (uint64_t)FileTime.dwLowDateTime + (((uint64_t)FileTime.dwHighDateTime) << 32);

            /* FILETIME has a precision of 100-nano seconds */
            Now /= 10000ULL;

            if (Now < Time)
            {
                /* Prevent from infinite wait if (Time - Now == INFINITE),
                 * which is probably possible on a multi-core CPU system. 
                 */
                Timeout = (uint32_t)min(Time - Now, FLEX_INFINITE - 1);
            }
            else
                Timeout = 0; /* Time in the past */
        }

        Ret = FLEX_Mutex_Unlock(&FlexBuffer->Mutex);

        if (Ret)
            break;

        Result = FLEX_Event_Wait(&FlexBuffer->Event[1], Timeout);

        Ret = FLEX_Mutex_Lock(&FlexBuffer->Mutex, FLEX_INFINITE);

        if (Ret)
        {
            /* This should never happen in practice */
            return NULL;
        }
#else
        if (Milliseconds != FLEX_INFINITE)
        {
            Result = pthread_cond_timedwait(&FlexBuffer->Event[1], &FlexBuffer->Mutex, &Ts);
        }
        else
            Result = pthread_cond_wait(&FlexBuffer->Event[1], &FlexBuffer->Mutex);
#endif
    }

    size_t Actual = FlexBuffer->Size - FlexBuffer->Length;

    if (Actual > Length)
    {
        Actual = Length;
    }

    if (Actual < Length && !Partial)
    {
        Actual = 0;
    }

    if (Actual)
    {
        /* Fill in range fields and return 
         * no allocation required 
         */
        Range = &FlexBuffer->Range[1][0];

        size_t Position = FlexBuffer->Position + FlexBuffer->Length;

        /* Wrap-around */
        if (Position >= FlexBuffer->Size)
            Position -= FlexBuffer->Size;

        FlexBuffer->Range[1][0].Data = &FlexBuffer->Data[Position];

        if (Position + Actual <= FlexBuffer->Size)
        {
            FlexBuffer->Range[1][0].Size = Actual;
            FlexBuffer->Range[1][0].Next = NULL;
        }
        else
        {
            FlexBuffer->Range[1][0].Size = FlexBuffer->Size - Position;
            FlexBuffer->Range[1][0].Next = &FlexBuffer->Range[1][1];

            /* Wrap-around */
            FlexBuffer->Range[1][1].Data = &FlexBuffer->Data[0];
            FlexBuffer->Range[1][1].Size = Position + Actual - FlexBuffer->Size;
            FlexBuffer->Range[1][1].Next = NULL;
        }

        /* Dequeued */
        FlexBuffer->Dequeued[1] = true;
    }

    FLEX_Mutex_Unlock(&FlexBuffer->Mutex);

    return Range;
}

size_t FLEX_PeekWrLength(FLEX_BUFFER *FlexBuffer)
{
    if (!FlexBuffer)
        return 0;

#ifdef _WIN32
    int Ret = FLEX_Mutex_Lock(&FlexBuffer->Mutex, FLEX_INFINITE);
#else
    int Ret = FLEX_Mutex_Lock(&FlexBuffer->Mutex, NULL);
#endif

    if (Ret)
        return 0;

    size_t Length = FlexBuffer->Length;

    FLEX_Mutex_Unlock(&FlexBuffer->Mutex);

    return Length;
}

size_t FLEX_PeekRdLength(FLEX_BUFFER *FlexBuffer)
{
    if (!FlexBuffer)
        return 0;

#ifdef _WIN32
    int Ret = FLEX_Mutex_Lock(&FlexBuffer->Mutex, FLEX_INFINITE);
#else
    int Ret = FLEX_Mutex_Lock(&FlexBuffer->Mutex, NULL);
#endif

    if (Ret)
        return 0;

    size_t Length = FlexBuffer->Size - FlexBuffer->Length;

    FLEX_Mutex_Unlock(&FlexBuffer->Mutex);

    return Length;
}

bool FLEX_PutWrBuffer(FLEX_BUFFER *FlexBuffer, FLEX_RANGE *Range)
{
    if (!FlexBuffer || !Range)
    {
        return false;
    }

#ifdef _WIN32
    int Ret = FLEX_Mutex_Lock(&FlexBuffer->Mutex, FLEX_INFINITE);
#else
    int Ret = FLEX_Mutex_Lock(&FlexBuffer->Mutex, NULL);
#endif

    if (Ret)
        return false;

    if (!FlexBuffer->Dequeued[0])
    {
        FLEX_Mutex_Unlock(&FlexBuffer->Mutex);
        return false;
    }

    size_t Length = Range->Size;

    if (Range->Next)
    {
        Length += Range->Next->Size;
    }

    if (Length > FlexBuffer->Length)
    {
        FLEX_Mutex_Unlock(&FlexBuffer->Mutex);
        return false;
    }
    
    FlexBuffer->Position += Length;
    FlexBuffer->Length   -= Length;

    /* Wrap-around */
    if (FlexBuffer->Position >= FlexBuffer->Size)
        FlexBuffer->Position -= FlexBuffer->Size;

    FlexBuffer->Dequeued[0] = false;

    FLEX_Event_Signal(&FlexBuffer->Event[1]);
    
    FLEX_Mutex_Unlock(&FlexBuffer->Mutex);
    return true;
}

bool FLEX_PutRdBuffer(FLEX_BUFFER *FlexBuffer, FLEX_RANGE *Range)
{
    if (!FlexBuffer || !Range)
    {
        return false;
    }

#ifdef _WIN32
    int Ret = FLEX_Mutex_Lock(&FlexBuffer->Mutex, FLEX_INFINITE);
#else
    int Ret = FLEX_Mutex_Lock(&FlexBuffer->Mutex, NULL);
#endif

    if (Ret)
        return false;

    if (!FlexBuffer->Dequeued[1])
    {
        FLEX_Mutex_Unlock(&FlexBuffer->Mutex);
        return false;
    }

    size_t Length = Range->Size;

    if (Range->Next)
    {
        Length += Range->Next->Size;
    }

    if (Length > FlexBuffer->Size - FlexBuffer->Length)
    {
        FLEX_Mutex_Unlock(&FlexBuffer->Mutex);
        return false;
    }

    FlexBuffer->Length += Length;

    FlexBuffer->Dequeued[1] = false;

    FLEX_Event_Signal(&FlexBuffer->Event[0]);

    FLEX_Mutex_Unlock(&FlexBuffer->Mutex);
    return true;
}

bool FLEX_ReleaseWrBuffer(FLEX_BUFFER *FlexBuffer)
{
    if (!FlexBuffer)
    {
        return false;
    }

#ifdef _WIN32
    int Ret = FLEX_Mutex_Lock(&FlexBuffer->Mutex, FLEX_INFINITE);
#else
    int Ret = FLEX_Mutex_Lock(&FlexBuffer->Mutex, NULL);
#endif

    if (Ret)
        return false;

    if (!FlexBuffer->Dequeued[0])
    {
        FLEX_Mutex_Unlock(&FlexBuffer->Mutex);
        return false;
    }

    FlexBuffer->Dequeued[0] = false;

    FLEX_Mutex_Unlock(&FlexBuffer->Mutex);
    return true;
}

bool FLEX_ReleaseRdBuffer(FLEX_BUFFER *FlexBuffer)
{
    if (!FlexBuffer)
    {
        return false;
    }

#ifdef _WIN32
    int Ret = FLEX_Mutex_Lock(&FlexBuffer->Mutex, FLEX_INFINITE);
#else
    int Ret = FLEX_Mutex_Lock(&FlexBuffer->Mutex, NULL);
#endif

    if (Ret)
        return false;

    if (!FlexBuffer->Dequeued[1])
    {
        FLEX_Mutex_Unlock(&FlexBuffer->Mutex);
        return false;
    }

    FlexBuffer->Dequeued[1] = false;

    FLEX_Mutex_Unlock(&FlexBuffer->Mutex);
    return true;
}

uint8_t * FLEX_GetRangeData(FLEX_RANGE *Range, size_t *Size)
{
    if (!Range || !Size)
        return NULL;
    
    *Size = Range->Size;

    return Range->Data;
}

uint8_t * FLEX_GetExtraData(FLEX_RANGE *Range, size_t *Size)
{
    if (!Range || !Range->Next)
        return NULL;

    if (!Size)
        return NULL;

    *Size = Range->Next->Size;

    return Range->Next->Data;
}

