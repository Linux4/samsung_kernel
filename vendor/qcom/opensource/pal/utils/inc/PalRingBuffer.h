/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <stdlib.h>
#include <memory>
#include <mutex>
#include <vector>
#include <string>
#include <iostream>
#include <string.h>

#ifndef PALRINGBUFFER_H_
#define PALRINGBUFFER_H_

#define DEFAULT_PAL_RING_BUFFER_SIZE 4096 * 10

typedef enum {
    READER_DISABLED = 0,
    READER_ENABLED = 1,
} pal_ring_buffer_reader_state;

class PalRingBuffer;

class PalRingBufferReader {
 public:
     PalRingBufferReader(PalRingBuffer *buffer)
         : ringBuffer_(buffer),
           unreadSize_(0),
           readOffset_(0),
           state_(READER_DISABLED) {}

    ~PalRingBufferReader() {};

    size_t advanceReadOffset(size_t advanceSize);
    int32_t read(void* readBuffer, size_t readSize);
    void updateState(pal_ring_buffer_reader_state state);
    void getIndices(uint32_t *startIndice, uint32_t *endIndice);
    size_t getUnreadSize();
    void reset();
    bool isEnabled() { return state_ == READER_ENABLED; }

    friend class PalRingBuffer;
    friend class StreamSoundTrigger;

 protected:
    PalRingBuffer *ringBuffer_;
    size_t unreadSize_;
    size_t readOffset_;
    pal_ring_buffer_reader_state state_;
};

class PalRingBuffer {
 public:
    explicit PalRingBuffer(size_t bufferSize)
        : buffer_((char*)(new char[bufferSize])),
          startIndex(0),
          endIndex(0),
          writeOffset_(0),
          bufferEnd_(bufferSize) {}

    ~PalRingBuffer() {}

    PalRingBufferReader* newReader();

    int32_t removeReader(PalRingBufferReader *reader);
    size_t read(std::shared_ptr<PalRingBufferReader>reader, void* readBuffer,
                size_t readSize);
    size_t write(void* writeBuffer, size_t writeSize);
    size_t getFreeSize();
    void updateIndices(uint32_t startIndice, uint32_t endIndice);
    void reset();
    size_t getBufferSize() { return bufferEnd_; };
    void resizeRingBuffer(size_t bufferSize);

 protected:
    std::mutex mutex_;
    char* buffer_;
    uint32_t startIndex;
    uint32_t endIndex;
    size_t writeOffset_;
    size_t bufferEnd_;
    std::vector<PalRingBufferReader*> readOffsets_;
    void updateUnReadSize(size_t writtenSize);
    friend class PalRingBufferReader;
};
#endif
