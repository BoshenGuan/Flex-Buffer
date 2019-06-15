#ifndef __FLEX_H__
#define __FLEX_H__

#include "FLEX_OS.h"
    
#define FLEX_INFINITE   0xFFFFFFFF

/////////////////////////////////////////////////////////////////////////////
//       ________    _______  __    ____  __  ________________________     //
//      / ____/ /   / ____/ |/ /   / __ )/ / / / ____/ ____/ ____/ __ \    //
//     / /_  / /   / __/  |   /   / __  / / / / /_  / /_  / __/ / /_/ /    //
//    / __/ / /___/ /___ /   |   / /_/ / /_/ / __/ / __/ / /___/ __ _/     //
//   /_/   /_____/_____//_/|_|  /_____/\____/_/   /_/   /_____/_/ |_|      //
//                                                                         //
/////////////////////////////////////////////////////////////////////////////
//                                                                         //
// Introduction                                                            //
//                                                                         //
// The Flex Buffer is a simple buffer management utility that implements a //
// canonical producer-consumer form data buffering. The underlying fact is //
// a circular buffer with pre-allocated size.                              //
//                                                                         //
// Usage                                                                   //
//                                                                         //
// To make the utility easy to use, here is a brief introduction of usage. //
//                                                                         //
// 1. The buffer is always created with FLEX_CreateBuffer and destroyed by //
//    FLEX_DeleteBuffer.                                                   //
//                                                                         //
// 2. Use FLEX_GetWrBuffer to get buffer ranges to write data to.          //
//                                                                         //
// 3. After finishing writing the data, use FLEX_PutWrBuffer to put ranges //
//    back for read, or use FLEX_ReleaseWrBuffer to return ranges back for //
//    write again later. For example, errors may occur while preparing the //
//    data, and in this case, use FLEX_ReleaseWrBuffer to release the      //
//    ranges to prevent corrupted data from being read.                    //
//                                                                         //
// 4. Use FLEX_GetRdBuffer to get buffer ranges to read data from.         //
//                                                                         //
// 5. After finishing reading the data, use FLEX_PutRdBuffer to put ranges //
//    back for write (for re-use), or use FLEX_ReleaseWrBuffer to return   //
//    ranges back for read again later to prevent data from being dropped. //
//                                                                         //
// 6. At the end of circular buffer, the requested buffer may be separated //
//    into two parts. FLEX_GetRangeData returns the normal (or first) part //
//    and FLEX_GetExtraData returns the extra (or second) part if exists.  //
//                                                                         //
// 7. Be careful with FLEX_PeekWrLength and FLEX_PeekRdLength, because the //
//    buffer length may have changed since the function returns.           //
//                                                                         //
// Application Note                                                        //
//                                                                         //
// 1. Data streaming. Employ Flex Buffer as dynamic speed balancer between //
//    source and destination. Use larger buffer size to prevent protential //
//    data lost caused by speed jitter. By selecting read and write length //
//    elaborately, it is possible to achieve source and destination speeds //
//    adaptation.                                                          //
//                                                                         //
// 2. Frame composer / decomposer. Use Flex Buffer with streaming protocol //
//    (TCP) to provide easy frame compose and decompose ability. Streaming //
//    data is pushed into the buffer at any size, and pulled out blockwise //
//    from the buffer.                                                     //
//                                                                         //
/////////////////////////////////////////////////////////////////////////////

typedef struct FLEX_BUFFER FLEX_BUFFER;
typedef struct FLEX_RANGE  FLEX_RANGE;

/**
 * Create an instance for given size and alignment
 *
 * @param Size      Buffer size in bytes (> 0)
 * @param Alignment Memory alignment (power of 2)
 *
 * @return Instance pointer or NULL for error
 */
FLEX_BUFFER *FLEX_CreateBuffer(size_t Size, size_t Alignment);

/**
 * Delete an instance
 *
 * @param FlexBuffer Instance pointer (not NULL)
 *
 * @return None
 */
void FLEX_DeleteBuffer(FLEX_BUFFER *FlexBuffer);

/**
 * Get buffer ranges for write or read from the instance
 *
 * @param FlexBuffer   Instance pointer (not NULL)
 * @param Length       Requested length (> 0)
 * @param Milliseconds Wait timeout before return, or FLEX_INFINITE to wait infinitely
 * @param Partial      Partial buffer (< Length) allowed when return
 *
 * @return Ranges pointer or NULL if no buffer available
 *
 * @note The ranges should be put or released before the next call, otherwise NULL is returned
 */
FLEX_RANGE *FLEX_GetWrBuffer(FLEX_BUFFER *FlexBuffer, size_t Length, bool Partial, uint32_t Milliseconds);
FLEX_RANGE *FLEX_GetRdBuffer(FLEX_BUFFER *FlexBuffer, size_t Length, bool Partial, uint32_t Milliseconds);

/**
 * Put buffer ranges for read or write back to the instance
 *
 * @param FlexBuffer Instance pointer (not NULL)
 * @param Range      Ranges to put, returned by FLEX_GetWrBuffer or FLEX_GetRdBuffer (not NULL)
 *
 * @return true if succeed, otherwise false
 *
 * @note FLEX_PutWrBuffer puts ranges back for read and FLEX_PutRdBuffer puts ranges back for write
 */
bool FLEX_PutWrBuffer(FLEX_BUFFER *FlexBuffer, FLEX_RANGE *Range);
bool FLEX_PutRdBuffer(FLEX_BUFFER *FlexBuffer, FLEX_RANGE *Range);

/**
 * Release buffer ranges for write or read again later
 *
 * @param FlexBuffer Instance pointer (not NULL)
 *
 * @return true if succeed, otherwise false
 *
 * @note FLEX_ReleaseWrBuffer for write again later and FLEX_ReleaseRdBuffer for read again later
 */
bool FLEX_ReleaseWrBuffer(FLEX_BUFFER *FlexBuffer);
bool FLEX_ReleaseRdBuffer(FLEX_BUFFER *FlexBuffer);

/**
 * Peek write or read buffer length (snapshot only)
 *
 * @param FlexBuffer Instance pointer (not NULL)
 *
 * @return Snapshot of buffer length if succeed, otherwise 0
 *
 * @note The length may have changed after the function returns
 */
size_t FLEX_PeekWrLength(FLEX_BUFFER *FlexBuffer);
size_t FLEX_PeekRdLength(FLEX_BUFFER *FlexBuffer);

/**
 * Retrive data buffer held by the range. At the end of circular
 * buffer, the requested buffer may be separated into two parts. 
 * This function returns the first part.
 *
 * @param Range Range pointer (not NULL)
 * @param Size  [OUT] Return the range size in bytes
 *
 * @return Data buffer pointer, NULL for error
 *
 * @note Range pointer returned by FLEX_GetWrBuffer or FLEX_GetRdBuffer.
 * @remarks 
*/
uint8_t *FLEX_GetRangeData(FLEX_RANGE *Range, size_t *Size);

/**
 * Retrive extra data buffer held by the range. At the end of circular
 * buffer, the requested buffer may be separated into two parts. This
 * function returns the second part.
 *
 * @param Range Range pointer (not NULL)
 * @param Size  [OUT] Return the extra size in bytes
 *
 * @return Data buffer pointer, NULL if no extra data
 *
 * @note Range pointer returned by FLEX_GetWrBuffer or FLEX_GetRdBuffer.
*/
uint8_t *FLEX_GetExtraData(FLEX_RANGE *Range, size_t *Size);

#endif // __FLEX_H__