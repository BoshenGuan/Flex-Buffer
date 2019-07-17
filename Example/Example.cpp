// Example.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <stdio.h>

#ifdef _WIN32
#pragma warning(disable: 4996)
#endif

#include "FLEX.h"

/* This example shows a simple producer-consumer model to
 * demostrate the use of Flex Buffer.
 *
 * The producer generates random data and passes the data
 * to the Flex Buffer instance. The data is also written
 * to a file for comparison.
 *
 * The consumer gets data from the Flex Buffer instance
 * and writes what it gets in the buffer to another file
 * for comparison.
 *
 * Comparison between two files checks if all data are
 * correctly buffered without lost or corruption.
 */

#define NAME_SRC "SRC.bin"
#define NAME_DST "DST.bin"

/* Total transfer data size in bytes */
#define TOTAL_TRANSFER  (1024 * 1024)

/* Producer routine */
void *ProducerProc(void *Param)
{
    size_t i;

    srand((unsigned int)time((time_t *)NULL));

    /* Retrieve instance pointer */
    FLEX_BUFFER *BufferPtr = (FLEX_BUFFER *)Param;
    
    /* Store the data for comparsion later */
    FILE *File = fopen(NAME_SRC, "wb+");

    if (!File)
        return 0;

    const size_t Block = 256; /* Write 256 bytes each time */

    size_t Transfer = 0;

    while (Transfer < TOTAL_TRANSFER)
    {
        /* Wait at most 1000 ms. Do not return partial buffer 
         * if the requested length can not be fulfiled. */

        FLEX_RANGE *RangePtr = FLEX_GetWrBuffer(BufferPtr, Block, false, 1000);

        if (RangePtr)
        {
            /* Requested buffer is successfully dequeued */

            size_t Size;
            uint8_t *Data = FLEX_GetRangeData(RangePtr, &Size);

            if (Data)
            {
                Transfer += Size;

                for (i = 0; i < Size; i++)
                    Data[i] = rand() % 256;

                fwrite(Data, 1, Size, File);
            }
            
            /* At the end of circular buffer, the requested 
             * buffer may be divided into two parts. */

            Data = FLEX_GetExtraData(RangePtr, &Size);

            /* The second part presents if Data != NULL */
            if (Data)
            {
                Transfer += Size;

                for (i = 0; i < Size; i++)
                    Data[i] = rand() % 256;

                fwrite(Data, 1, Size, File);
            }

            fflush(File);

            /* Finally return the filled buffer back to instance
             * and make it ready to read. */

            FLEX_PutWrBuffer(BufferPtr, RangePtr);
        }
    }

    fclose(File);
    return 0;
}

/* Consumer routine */
void *ConsumerProc(void *Param)
{
    /* Retrieve instance pointer */
    FLEX_BUFFER *BufferPtr = (FLEX_BUFFER *)Param;

    FILE *File = fopen(NAME_DST, "wb+");

    if (!File)
        return 0;

    const size_t Block = 1024; /* Read 1024 bytes each time */

    size_t Transfer = 0;

    while (Transfer < TOTAL_TRANSFER)
    {
        /* Wait at most 1000 ms. Do not return partial buffer 
         * if the requested length can not be fulfiled. */

        FLEX_RANGE* RangePtr = FLEX_GetRdBuffer(BufferPtr, Block, false, 1000);

        if (RangePtr)
        {
            /* Requested buffer is successfully dequeued */

            size_t Size;
            uint8_t *Data = FLEX_GetRangeData(RangePtr, &Size);

            if (Data)
                fwrite(Data, 1, Size, File);

            Data = FLEX_GetExtraData(RangePtr, &Size);

            if (Data)
                fwrite(Data, 1, Size, File);

            /* Because no partial buffer dequeued, instead
             * of accumulating the size of each range, add 
             * total requested length. */

            Transfer += Block;

            fflush(File);

            /* Return the read buffer back to instance
             * and make it ready to write again. */

            FLEX_PutRdBuffer(BufferPtr, RangePtr);
        }
    }

    fclose(File);
    return 0;
}

bool VerifyData()
{
    size_t i;
    FILE *File[2];

    File[0] = fopen(NAME_SRC, "rb");
    File[1] = fopen(NAME_DST, "rb");

    if (!File[0] || !File[1])
    {
        for (i = 0; i < 2; i++)
            if (File[i])
                fclose(File[i]);

        return false;
    }

    char Buf1[1024];
    char Buf2[1024];

    bool Match = true;

    do {
        size_t N1 = fread(Buf1, 1, 1024, File[0]);
        size_t N2 = fread(Buf2, 1, 1024, File[1]);

        if (N1 != N2 || memcmp(Buf1, Buf2, N1))
        {
            Match = false;
            break;
        }

    } while (!feof(File[0]) && !feof(File[1]));

    if (Match)
        Match = feof(File[0]) && feof(File[1]);

    for (i = 0; i < 2; i++)
        if (File[i])
            fclose(File[i]);

    return Match;
}

int main(void)
{
    /* In this exmaple a Flex Buffer instance is created 
     * with a given buffer size and alignment. 
     *
     * Note:
     * Alignment has some restrictions that vary among OS 
     * and platforms. 
     */
    FLEX_BUFFER *BufferPtr = FLEX_CreateBuffer(1024, 16);

    if (!BufferPtr)
    {
        return -1;
    }

#ifdef _WIN32

    HANDLE hProducer = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ProducerProc, BufferPtr, 0, NULL);
    HANDLE hConsumer = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ConsumerProc, BufferPtr, 0, NULL);

    WaitForSingleObject(hProducer, INFINITE);
    WaitForSingleObject(hConsumer, INFINITE);

#else
    pthread_t TID_Producer;
    pthread_t TID_Consumer;

    /*
     * Create two threads. One thread produces some data and
     * puts the data into the buffer. The other thread gets 
     * data from the buffer and processes it.
     *
     * Pass the instance pointer in as threads argument.
     */
    pthread_create(&TID_Producer, NULL, ProducerProc, BufferPtr);
    pthread_create(&TID_Consumer, NULL, ConsumerProc, BufferPtr);

    void *Ret;

    /* Wait until threads stopped */
    pthread_join(TID_Producer, &Ret);
    pthread_join(TID_Consumer, &Ret);
#endif

    /* Delete Flex Buffer instance and free resources. */
    FLEX_DeleteBuffer(BufferPtr);

    /* Check if all data are correctly buffered */
    printf("VERIFY ... %s\n", VerifyData() ? "OK" : "ERROR" );

    return 0;
}

