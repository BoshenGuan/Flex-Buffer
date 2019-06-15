#include "stdafx.h"

#include "FLEX.h"

#define NSEC_PER_MSEC      1000000ULL
#define NSEC_PER_SEC    1000000000ULL

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
    pthread_mutex_t Mutex;
    pthread_cond_t  Cond [2];       /* [0] - Wr */
                                    /* [1] - Rd */
    FLEX_RANGE      Range[2][2];    /* The buffer may be divided into two parts */
    bool            Dequeued[2];

} FLEX_BUFFER;

FLEX_BUFFER *FLEX_CreateBuffer(size_t Size, size_t Alignment)
{
    size_t i, j;

    if (!Size)
    {
        return NULL;
    }

    FLEX_BUFFER *FlexBuffer = (FLEX_BUFFER *)malloc(sizeof(FLEX_BUFFER));

    if (!FlexBuffer)
    {
        return NULL;
    }

    int Ret = pthread_mutex_init(&FlexBuffer->Mutex, NULL);

    if (Ret)
    {
        FLEX_DeleteBuffer(FlexBuffer);
        return NULL;
    }

    for (i = 0; i < 2; i++)
    {
        Ret = pthread_cond_init(&FlexBuffer->Cond[i], NULL);
        if (Ret)
        {
            FLEX_DeleteBuffer(FlexBuffer);
            return NULL;
        }
    }
    
    FlexBuffer->Position = 0;
    FlexBuffer->Length = Size;
    FlexBuffer->Size = Size;
    FlexBuffer->Data = (uint8_t *)FLEX_Aligned_Malloc(Size, Alignment);

    if (!FlexBuffer->Data)
    {
        FLEX_DeleteBuffer(FlexBuffer);
        return NULL;
    }

    for (i = 0; i < 2; i++)
    {
        for (j = 0; j < 2; j++)
        {
            memset(&FlexBuffer->Range[i][j], 0, sizeof(FLEX_RANGE));
        }
    }

    FlexBuffer->Dequeued[0] = false;
    FlexBuffer->Dequeued[1] = false;

    return FlexBuffer;
}

void FLEX_DeleteBuffer(FLEX_BUFFER *FlexBuffer)
{
    size_t i;

    if (!FlexBuffer)
    {
        return;
    }

    pthread_mutex_destroy(&FlexBuffer->Mutex);

    for (i = 0; i < 2; i++)
        pthread_cond_destroy(&FlexBuffer->Cond[i]);

    if (FlexBuffer->Data)
    {
        FLEX_Aligned_Free(FlexBuffer->Data);

        FlexBuffer->Data = NULL;
    }

    free(FlexBuffer);
}

FLEX_RANGE *FLEX_GetWrBuffer(FLEX_BUFFER *FlexBuffer, size_t Length, bool Partial, uint32_t Milliseconds)
{
    if (!FlexBuffer || !Length)
    {
        return NULL;
    }

    int Ret = pthread_mutex_lock(&FlexBuffer->Mutex);

    if (Ret)
        return NULL;

    if (FlexBuffer->Dequeued[0])
    {
        pthread_mutex_unlock(&FlexBuffer->Mutex);
        return NULL;
    }

    struct timespec Ts;
    Ret = FLEX_Timespec_Get(&Ts);

    if (Ret)
    {
        pthread_mutex_unlock(&FlexBuffer->Mutex);
        return NULL;
    }

    uint64_t Nsec = Ts.tv_nsec + Milliseconds * NSEC_PER_MSEC;
    
    /* Handle carry on seconds */
#if 0
    while (Nsec >= NSEC_PER_SEC)
    {
        Ts.tv_sec++;
        Nsec -= NSEC_PER_SEC;
    }

    Ts.tv_nsec = (long)Nsec;
#else
    Ts.tv_sec += Nsec / NSEC_PER_SEC;
    Ts.tv_nsec = (long)(Nsec % NSEC_PER_SEC);
#endif

    FLEX_RANGE *Range = NULL;
    Ret = 0;

    while (FlexBuffer->Length < Length && Ret == 0)
    {
        if (Milliseconds != FLEX_INFINITE)
        {
            Ret = pthread_cond_timedwait(&FlexBuffer->Cond[0], &FlexBuffer->Mutex, &Ts);
        }
        else
            Ret = pthread_cond_wait(&FlexBuffer->Cond[0], &FlexBuffer->Mutex);
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

    pthread_mutex_unlock(&FlexBuffer->Mutex);

    return Range;
}

FLEX_RANGE *FLEX_GetRdBuffer(FLEX_BUFFER *FlexBuffer, size_t Length, bool Partial, uint32_t Milliseconds)
{
    if (!FlexBuffer || !Length)
    {
        return NULL;
    }

    int Ret = pthread_mutex_lock(&FlexBuffer->Mutex);

    if (Ret)
        return NULL;

    if (FlexBuffer->Dequeued[1])
    {
        pthread_mutex_unlock(&FlexBuffer->Mutex);
        return NULL;
    }

    struct timespec Ts;
    Ret = FLEX_Timespec_Get(&Ts);

    if (Ret)
    {
        pthread_mutex_unlock(&FlexBuffer->Mutex);
        return NULL;
    }

    uint64_t Nsec = Ts.tv_nsec + Milliseconds * NSEC_PER_MSEC;

    /* Handle carry on seconds */
#if 0
    while (Nsec >= NSEC_PER_SEC)
    {
        Ts.tv_sec++;
        Nsec -= NSEC_PER_SEC;
    }

    Ts.tv_nsec = (long)Nsec;
#else
    Ts.tv_sec += Nsec / NSEC_PER_SEC;
    Ts.tv_nsec = (long)(Nsec % NSEC_PER_SEC);
#endif

    FLEX_RANGE *Range = NULL;
    Ret = 0;

    while (FlexBuffer->Size - FlexBuffer->Length < Length && Ret == 0)
    {
        if (Milliseconds != FLEX_INFINITE)
        {
            Ret = pthread_cond_timedwait(&FlexBuffer->Cond[1], &FlexBuffer->Mutex, &Ts);
        }
        else
            Ret = pthread_cond_wait(&FlexBuffer->Cond[1], &FlexBuffer->Mutex);
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

    pthread_mutex_unlock(&FlexBuffer->Mutex);

    return Range;
}

size_t FLEX_PeekWrLength(FLEX_BUFFER *FlexBuffer)
{
    if (!FlexBuffer)
        return 0;

    int Ret = pthread_mutex_lock(&FlexBuffer->Mutex);

    if (Ret)
        return 0;

    size_t Length = FlexBuffer->Length;

    pthread_mutex_unlock(&FlexBuffer->Mutex);

    return Length;
}

size_t FLEX_PeekRdLength(FLEX_BUFFER *FlexBuffer)
{
    if (!FlexBuffer)
        return 0;

    int Ret = pthread_mutex_lock(&FlexBuffer->Mutex);

    if (Ret)
        return 0;

    size_t Length = FlexBuffer->Size - FlexBuffer->Length;

    pthread_mutex_unlock(&FlexBuffer->Mutex);

    return Length;
}

bool FLEX_PutWrBuffer(FLEX_BUFFER *FlexBuffer, FLEX_RANGE *Range)
{
    if (!FlexBuffer || !Range)
    {
        return false;
    }

    int Ret = pthread_mutex_lock(&FlexBuffer->Mutex);

    if (Ret)
        return false;

    if (!FlexBuffer->Dequeued[0])
    {
        pthread_mutex_unlock(&FlexBuffer->Mutex);
        return false;
    }

    size_t Length = Range->Size;

    if (Range->Next)
    {
        Length += Range->Next->Size;
    }

    if (Length > FlexBuffer->Length)
    {
        pthread_mutex_unlock(&FlexBuffer->Mutex);
        return false;
    }
    
    FlexBuffer->Position += Length;
    FlexBuffer->Length   -= Length;

    /* Wrap-around */
    if (FlexBuffer->Position >= FlexBuffer->Size)
        FlexBuffer->Position -= FlexBuffer->Size;

    FlexBuffer->Dequeued[0] = false;

    pthread_cond_signal(&FlexBuffer->Cond[1]);
    
    pthread_mutex_unlock(&FlexBuffer->Mutex);
    return true;
}

bool FLEX_PutRdBuffer(FLEX_BUFFER *FlexBuffer, FLEX_RANGE *Range)
{
    if (!FlexBuffer || !Range)
    {
        return false;
    }

    int Ret = pthread_mutex_lock(&FlexBuffer->Mutex);

    if (Ret)
        return false;

    if (!FlexBuffer->Dequeued[1])
    {
        pthread_mutex_unlock(&FlexBuffer->Mutex);
        return false;
    }

    size_t Length = Range->Size;

    if (Range->Next)
    {
        Length += Range->Next->Size;
    }

    if (Length > FlexBuffer->Size - FlexBuffer->Length)
    {
        pthread_mutex_unlock(&FlexBuffer->Mutex);
        return false;
    }

    FlexBuffer->Length += Length;

    FlexBuffer->Dequeued[1] = false;

    pthread_cond_signal(&FlexBuffer->Cond[0]);

    pthread_mutex_unlock(&FlexBuffer->Mutex);
    return true;
}

bool FLEX_ReleaseWrBuffer(FLEX_BUFFER *FlexBuffer)
{
    if (!FlexBuffer)
    {
        return false;
    }

    int Ret = pthread_mutex_lock(&FlexBuffer->Mutex);

    if (Ret)
        return false;

    if (!FlexBuffer->Dequeued[0])
    {
        pthread_mutex_unlock(&FlexBuffer->Mutex);
        return false;
    }

    FlexBuffer->Dequeued[0] = false;

    pthread_mutex_unlock(&FlexBuffer->Mutex);
    return true;
}

bool FLEX_ReleaseRdBuffer(FLEX_BUFFER *FlexBuffer)
{
    if (!FlexBuffer)
    {
        return false;
    }

    int Ret = pthread_mutex_lock(&FlexBuffer->Mutex);

    if (Ret)
        return false;

    if (!FlexBuffer->Dequeued[1])
    {
        pthread_mutex_unlock(&FlexBuffer->Mutex);
        return false;
    }

    FlexBuffer->Dequeued[1] = false;

    pthread_mutex_unlock(&FlexBuffer->Mutex);
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


