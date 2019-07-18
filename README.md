# Flex-Buffer
## Introduction
Flex Buffer is a cross-platform buffer management utility that implements a canonical producer-consumer form data buffering. The underlying fact is a circular buffer with pre-allocated memory. <br/>

When it comes to data buffering, obtaining and releasing of buffers are functions that usually need to be implemented. These functions contain lots of buffering details, such as data position and length management, memory wrap-around issue, etc., which are time-costly to implement and need to be very carefully coded. <br/>

On the other hand, in practical use, buffer may become full or empty. Once the buffer gets full, it is important to know when the buffer has free space again. The same situation happens when the buffer gets empty. Instead of busy-waiting or periodic polling, a signal-oriented approach with timed-out waiting seems to be a better solution. <br/>

## Background
The basic idea of creating this repository is to hide coding details of data buffering and provide simple but robust APIs to manipulate buffers. This repository is originally initiated from a data streaming project, in which data buffering plays an important role in both acquisition and playback. <br/>

## Usage
Flex Buffer interfaces are designed to be simple but efficient. To make the utility easy to use, here is a brief introduction of basic workflow and usage. <br/>

* The buffer instance is always created with `FLEX_CreateBuffer` and destroyed by `FLEX_DeleteBuffer`.

* Use `FLEX_GetWrBuffer` to get buffer ranges to write data to.

* After finishing writing the data, use `FLEX_PutWrBuffer` to put ranges back for read, or use `FLEX_ReleaseWrBuffer` to return ranges back for write again later. For example, errors may occur while preparing the data, and in this case, use `FLEX_ReleaseWrBuffer` to release the ranges to prevent corrupted data from being read.

* Use `FLEX_GetRdBuffer` to get buffer ranges to read data from.

* After finishing reading the data, use `FLEX_PutRdBuffer` to put ranges back for write (for re-use), or use `FLEX_ReleaseWrBuffer` to return ranges back for read again later to prevent data from being dropped.

* At the end of circular buffer, the requested buffer may be separated into two parts. `FLEX_GetRangeData` returns the normal (or first) part and `FLEX_GetExtraData` returns the extra (or second) part if exists. 

* Be careful with `FLEX_PeekWrLength` and `FLEX_PeekRdLength`, because the buffer length may have changed since the function returns.

* Use `FLEX_RestoreBuffer` to restore the buffer to initial empty state.

## How to compile
Flex Buffer is designed to be a cross-platform utility with supports to both x86/x64 Windows (including Windows XP) and Linux. On Linux, `cd` to the repository directory containing `Makefile` and `make`. After building, executable `Example` is generated. Run it with `./Example` command. <br/>

On Windows, Visual Studio is required to build the code. Flex Buffer is developed using Visual Studio 2013 and it is tested to build on Visual Studio 2010. When building with version other than 2013, `Platform Toolset` in project's property page should be selected properly according to the Visual Studio version being used. For example, `v100` usually stands for Visual Studio 2010, `v120` for Visual Studio 2013 and `v141` for Visual Studio 2017. <br/>

## Application Note
1. Data streaming. Employ Flex Buffer as dynamic speed balancer between source and destination. Use larger buffer size to prevent from potential data lost caused by speed jitter. By selecting read and write length elaborately, it is possible to achieve source and destination speeds adaptation.

2. Frame composer/decomposer. Use Flex Buffer with streaming-oriented protocol (such as TCP) to provide easy frame composition and decomposition ability. Streaming data is pushed into the buffer at any size, and pulled out block-wisely (usually a complete frame) from the buffer.

## Limitation
To keep memory continuity, Flex Buffer only supports one producer and one consumer. That is, once a buffer (read or write) is obtained, it has to be put or released before obtaining the next buffer. <br/>

Flex Buffer is currently implemented using native synchronization primitives on Windows and `pthreads` on others (for example, Linux). Although `pthreads` is a cross-platform library, it is not supported by all platforms, which makes it difficult to port Flex Buffer to OS that does not support `pthreads`. <br/>
