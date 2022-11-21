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

#define LOG_TAG "PAL: SoundTriggerEngine"

#include "SoundTriggerEngine.h"

#include "SoundTriggerEngineGsl.h"
#include "SoundTriggerEngineCapi.h"
#include "Stream.h"
#include "SoundTriggerPlatformInfo.h"

std::shared_ptr<SoundTriggerEngine> SoundTriggerEngine::Create(
    Stream *s,
    listen_model_indicator_enum type,
    st_module_type_t module_type,
    std::shared_ptr<SoundModelConfig> sm_cfg)
{
    PAL_VERBOSE(LOG_TAG, "Enter, type %d", type);

    if (!s) {
        PAL_ERR(LOG_TAG, "Invalid stream handle");
        return nullptr;
    }

    std::shared_ptr<SoundTriggerEngine> st_engine(nullptr);

    switch (type) {
    case ST_SM_ID_SVA_F_STAGE_GMM:
        if (!sm_cfg->GetMergeFirstStageSoundModels() &&
            !IS_MODULE_TYPE_PDK(module_type))
            st_engine = std::make_shared<SoundTriggerEngineGsl>(s, type,
                                          module_type, sm_cfg);
        else
            st_engine = SoundTriggerEngineGsl::GetInstance(s, type,
                                                   module_type, sm_cfg);

        if (!st_engine)
            PAL_ERR(LOG_TAG, "SoundTriggerEngine GSL creation failed");
        break;

    case ST_SM_ID_SVA_S_STAGE_PDK:
    case ST_SM_ID_SVA_S_STAGE_RNN:
    case ST_SM_ID_SVA_S_STAGE_USER:
        st_engine = std::make_shared<SoundTriggerEngineCapi>(s, type,
                                                              sm_cfg);
        if (!st_engine)
            PAL_ERR(LOG_TAG, "SoundTriggerEngine capi creation failed");
        break;

    default:
        PAL_ERR(LOG_TAG, "Invalid model type: %u", type);
        break;
    }

    PAL_VERBOSE(LOG_TAG, "Exit, engine %p", st_engine.get());

    return st_engine;
}

int32_t SoundTriggerEngine::CreateBuffer(uint32_t buffer_size,
    uint32_t engine_size, std::vector<PalRingBufferReader *> &reader_list)
{
    int32_t status = 0;
    int32_t i = 0;
    PalRingBufferReader *reader = nullptr;

    if (!buffer_size || !engine_size) {
        PAL_ERR(LOG_TAG, "Invalid buffer size or engine number");
        status = -EINVAL;
        goto exit;
    }

    if (engine_type_ != ST_SM_ID_SVA_F_STAGE_GMM) {
        PAL_ERR(LOG_TAG, "Cannot create buffer in non-GMM engine");
        status = -EINVAL;
        goto exit;
    }

    PAL_DBG(LOG_TAG, "Enter");
    if (!buffer_) {
        buffer_ = new PalRingBuffer(buffer_size);
        if (!buffer_) {
            PAL_ERR(LOG_TAG, "Failed to allocate memory for ring buffer");
            status = -ENOMEM;
            goto exit;
        }
        PAL_VERBOSE(LOG_TAG, "Created a new buffer: %pK with size: %d",
            buffer_, buffer_size);
    } else {
        buffer_->reset();
        /* Resize the ringbuffer if it is changed */
        if (buffer_->getBufferSize() != buffer_size) {
            PAL_VERBOSE(LOG_TAG, "Resize the buffer %pK from old size: %zu to new size: %d",
                    buffer_, buffer_->getBufferSize(), buffer_size);
            buffer_->resizeRingBuffer(buffer_size);
        }
        /* Reset the readers from existing list*/
        for (int32_t i = 0; i < reader_list.size(); i++)
            reader_list[i]->reset();
    }

    if (engine_size != reader_list.size()) {
        reader_list.clear();
        for (i = 0; i < engine_size; i++) {
            reader = buffer_->newReader();
            if (!reader) {
                PAL_ERR(LOG_TAG, "Failed to create new ring buffer reader");
                status = -ENOMEM;
                goto exit;
            }
            reader_list.push_back(reader);
        }
    }

exit:
    PAL_DBG(LOG_TAG, "Exit, status %d", status);

    return status;
}

int32_t SoundTriggerEngine::SetBufferReader(PalRingBufferReader *reader)
{
    int32_t status = 0;

    if (engine_type_ == ST_SM_ID_SVA_F_STAGE_GMM) {
        PAL_DBG(LOG_TAG, "No need to set reader for GMM engine");
        return status;
    }

    reader_ = reader;

    return status;
}

int32_t SoundTriggerEngine::ResetBufferReaders(
    std::vector<PalRingBufferReader *> &reader_list)
{
    if (engine_type_ != ST_SM_ID_SVA_F_STAGE_GMM) {
        PAL_ERR(LOG_TAG, "Cannot reset buffer readers in non-GMM engine");
        return -EINVAL;
    }

    for (int32_t i = 0; i < reader_list.size(); i++)
        buffer_->removeReader(reader_list[i]);

    return 0;
}

uint32_t SoundTriggerEngine::UsToBytes(uint64_t input_us) {
    uint32_t bytes = 0;

    bytes = sample_rate_ * bit_width_ * channels_ * input_us /
        (BITS_PER_BYTE * US_PER_SEC);

    return bytes;
}

uint32_t SoundTriggerEngine::FrameToBytes(uint32_t frames) {
    return frames * bit_width_ * channels_ / BITS_PER_BYTE;
}

uint32_t SoundTriggerEngine::BytesToFrames(uint32_t bytes) {
    return (bytes * BITS_PER_BYTE) / (bit_width_ * channels_);
}
