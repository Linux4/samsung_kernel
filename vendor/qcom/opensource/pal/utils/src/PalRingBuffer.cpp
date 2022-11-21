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


#include "PalRingBuffer.h"
#include "PalCommon.h"
#define LOG_TAG "PAL: PalRingBuffer"

int32_t PalRingBuffer::removeReader(PalRingBufferReader *reader)
{
    auto iter = std::find(readOffsets_.begin(), readOffsets_.end(), reader);
    if (iter != readOffsets_.end())
        readOffsets_.erase(iter);

    return 0;
}

size_t PalRingBuffer::read(std::shared_ptr<PalRingBufferReader>reader __unused,
                           void* readBuffer __unused, size_t readSize __unused)
{
    return 0;
}

size_t PalRingBuffer::getFreeSize()
{

    size_t freeSize = bufferEnd_;
    std::vector<PalRingBufferReader*>::iterator it;

    for (it = readOffsets_.begin(); it != readOffsets_.end(); it++) {
        if ((*(it))->state_ == READER_ENABLED)
            freeSize = std::min(freeSize, bufferEnd_ - (*(it))->unreadSize_);
    }
    return freeSize;
}

void PalRingBuffer::updateUnReadSize(size_t writtenSize)
{
    int32_t i = 0;
    std::vector<PalRingBufferReader*>::iterator it;

    for (it = readOffsets_.begin(); it != readOffsets_.end(); it++, i++) {
        (*(it))->unreadSize_ += writtenSize;
        PAL_VERBOSE(LOG_TAG, "Reader (%d), unreadSize(%zu)", i, (*(it))->unreadSize_);
    }
}

void PalRingBuffer::updateIndices(uint32_t startIndice, uint32_t endIndice)
{
    startIndex = startIndice;
    endIndex = endIndice;
    PAL_VERBOSE(LOG_TAG, "start index = %u, end index = %u", startIndex, endIndex);
}

size_t PalRingBuffer::write(void* writeBuffer, size_t writeSize)
{
    /* update the unread size for each reader*/
    mutex_.lock();
    size_t freeSize = getFreeSize();
    size_t writtenSize = 0;
    int32_t i = 0;
    size_t sizeToCopy = 0;

#ifndef SEC_AUDIO_ADD_FOR_DEBUG
    PAL_DBG(LOG_TAG, "Enter. freeSize(%zu), writeOffset(%zu)", freeSize, writeOffset_);
#endif

    if (writeSize <= freeSize)
        sizeToCopy = writeSize;
    else
        sizeToCopy = freeSize;

    if (sizeToCopy) {
        //buffer wrapped around
        if (writeOffset_ + sizeToCopy > bufferEnd_) {
            i = bufferEnd_ - writeOffset_;

            ar_mem_cpy(buffer_ + writeOffset_, i, writeBuffer, i);
            writtenSize += i;
            sizeToCopy -= writtenSize;
            ar_mem_cpy(buffer_, sizeToCopy, (char*)writeBuffer + writtenSize,
                             sizeToCopy);
            writtenSize += sizeToCopy;
            writeOffset_ = sizeToCopy;
        } else {
            ar_mem_cpy(buffer_ + writeOffset_, sizeToCopy, writeBuffer,
                             sizeToCopy);
            writeOffset_ += sizeToCopy;
            writtenSize = sizeToCopy;
        }
    }
    updateUnReadSize(writtenSize);
    writeOffset_ = writeOffset_ % bufferEnd_;
#ifndef SEC_AUDIO_ADD_FOR_DEBUG
    PAL_DBG(LOG_TAG, "Exit. writeOffset(%zu)", writeOffset_);
#endif
    mutex_.unlock();
    return writtenSize;
}

void PalRingBuffer::reset()
{
    std::vector<PalRingBufferReader*>::iterator it;

    mutex_.lock();
    startIndex = 0;
    endIndex = 0;
    writeOffset_ = 0;
    mutex_.unlock();

    /* Reset all the associated readers */
    for (it = readOffsets_.begin(); it != readOffsets_.end(); it++)
        (*(it))->reset();
}

void PalRingBuffer::resizeRingBuffer(size_t bufferSize)
{
    if (buffer_) {
        delete[] buffer_;
        buffer_ = nullptr;
    }
    buffer_ = (char *)new char[bufferSize];
    bufferEnd_ = bufferSize;
}

int32_t PalRingBufferReader::read(void* readBuffer, size_t bufferSize)
{
    int32_t readSize = 0;

    if (state_ == READER_DISABLED)
        return -EINVAL;

    // Return 0 when no data can be read for current reader
    if (unreadSize_ == 0)
        return 0;

    ringBuffer_->mutex_.lock();

    // when writeOffset leads readOffset
    if (ringBuffer_->writeOffset_ > readOffset_) {
        unreadSize_ = ringBuffer_->writeOffset_ - readOffset_;

        if (bufferSize >= unreadSize_) {
            ar_mem_cpy(readBuffer, unreadSize_, ringBuffer_->buffer_ +
                             readOffset_, unreadSize_);
            readOffset_ += unreadSize_;
            readSize = unreadSize_;
            unreadSize_ = 0;
        } else {
            ar_mem_cpy(readBuffer, unreadSize_, ringBuffer_->buffer_ +
                             readOffset_, bufferSize);
            readOffset_ += bufferSize;
            readSize = bufferSize;
            unreadSize_ = ringBuffer_->writeOffset_ - readOffset_;
        }
    } else {
        //When readOffset leads WriteOffset
        int32_t freeClientSize = bufferSize;
        int32_t i = ringBuffer_->bufferEnd_ - readOffset_;

        if (bufferSize >= i) {
            ar_mem_cpy(readBuffer, i, (char*)(ringBuffer_->buffer_ +
                             readOffset_), i);
            readSize = i;
            freeClientSize -= readSize;
            unreadSize_ = ringBuffer_->writeOffset_;
            readOffset_ = 0;
            //copy remaining unread buffer
            if (freeClientSize > unreadSize_) {
                ar_mem_cpy((char *)readBuffer + readSize, unreadSize_,
                                 ringBuffer_->buffer_, unreadSize_);
                readSize += unreadSize_;
                readOffset_ = unreadSize_;
                unreadSize_ = 0;
            } else {
                //copy whatever we can
                ar_mem_cpy((char *)readBuffer + readSize, freeClientSize,
                                 ringBuffer_->buffer_, freeClientSize);
                readSize += freeClientSize;
                readOffset_ = freeClientSize;
                unreadSize_ = ringBuffer_->writeOffset_ - readOffset_;
            }

        } else {
            ar_mem_cpy(readBuffer, bufferSize, ringBuffer_->buffer_ +
                             readOffset_, bufferSize);
            readSize = bufferSize;
            readOffset_ += bufferSize;
            unreadSize_ = ringBuffer_->bufferEnd_ - readOffset_ +
                          ringBuffer_->writeOffset_;
        }
    }
    ringBuffer_->mutex_.unlock();
    return readSize;
}

size_t PalRingBufferReader::advanceReadOffset(size_t advanceSize)
{
    size_t size_advanced = 0;

    std::lock_guard<std::mutex> lock(ringBuffer_->mutex_);

    /* add code to advance the offset here*/
    if (unreadSize_ < advanceSize) {
        PAL_ERR(LOG_TAG, "Cannot advance read offset %zu greater than unread size %zu",
            advanceSize, unreadSize_);
        return size_advanced;
    }

    unreadSize_ -= advanceSize;
    if (readOffset_ + advanceSize < ringBuffer_->bufferEnd_) {
        readOffset_ += advanceSize;
    } else {
        readOffset_ = readOffset_ + advanceSize - ringBuffer_->bufferEnd_;
    }
    size_advanced += advanceSize;

    return size_advanced;
}

void PalRingBufferReader::updateState(pal_ring_buffer_reader_state state)
{
    PAL_DBG(LOG_TAG, "update reader state to %d", state);
    std::lock_guard<std::mutex> lock(ringBuffer_->mutex_);

    if (state_ == READER_DISABLED && state == READER_ENABLED) {
        if (unreadSize_ > ringBuffer_->bufferEnd_)
           unreadSize_ = ringBuffer_->bufferEnd_;

        if (unreadSize_ <= ringBuffer_->writeOffset_) {
            readOffset_ = ringBuffer_->writeOffset_ - unreadSize_;
        } else {
            readOffset_ = ringBuffer_->writeOffset_ +
                ringBuffer_->bufferEnd_ - unreadSize_;
        }
    }
    state_ = state;
}

void PalRingBufferReader::getIndices(uint32_t *startIndice, uint32_t *endIndice)
{
    *startIndice = ringBuffer_->startIndex;
    *endIndice = ringBuffer_->endIndex;
    PAL_VERBOSE(LOG_TAG, "start index = %u, end index = %u",
                ringBuffer_->startIndex, ringBuffer_->endIndex);
}

size_t PalRingBufferReader::getUnreadSize()
{
    PAL_VERBOSE(LOG_TAG, "unread size %zu", unreadSize_);
    return unreadSize_;
}

void PalRingBufferReader::reset()
{
    ringBuffer_->mutex_.lock();
    readOffset_ = 0;
    unreadSize_ = 0;
    state_ = READER_DISABLED;
    ringBuffer_->mutex_.unlock();
}

PalRingBufferReader* PalRingBuffer::newReader()
{
    PalRingBufferReader* readOffset =
                  new PalRingBufferReader(this);
    readOffsets_.push_back(readOffset);
    return readOffset;
}
