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
 *
 * Changes from Qualcomm Innovation Center are provided under the following license:
 *
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define ATRACE_TAG (ATRACE_TAG_AUDIO | ATRACE_TAG_HAL)
#define LOG_TAG "PAL: SoundTriggerEngineGsl"

#include "SoundTriggerEngineGsl.h"

#include <cutils/trace.h>

#include "Session.h"
#include "Stream.h"
#include "StreamSoundTrigger.h"
#include "ResourceManager.h"
#include "SoundTriggerPlatformInfo.h"
#include "VoiceUIInterface.h"
#include "sh_mem_pull_push_mode_api.h"

// TODO: find another way to print debug logs by default
#define ST_DBG_LOGS
#ifdef ST_DBG_LOGS
#define PAL_DBG(LOG_TAG,...)  PAL_INFO(LOG_TAG,__VA_ARGS__)
#endif

#define TIMEOUT_FOR_EOS 100000
#define MAX_MMAP_POSITION_QUERY_RETRY_CNT 5

ST_DBG_DECLARE(static int dsp_output_cnt = 0);

std::map<st_module_type_t,std::vector<std::shared_ptr<SoundTriggerEngineGsl>>>
                 SoundTriggerEngineGsl::eng_;
std::map<Stream*, std::shared_ptr<SoundTriggerEngineGsl>>
                 SoundTriggerEngineGsl::str_eng_map_;
std::mutex SoundTriggerEngineGsl::eng_create_mutex_;
int32_t SoundTriggerEngineGsl::engine_count_ = 0;
std::condition_variable cvEOS;

void SoundTriggerEngineGsl::EventProcessingThread(
    SoundTriggerEngineGsl *gsl_engine) {

    int32_t status = 0;
    StreamSoundTrigger *det_str = nullptr;
    std::shared_ptr<ResourceManager> rm = ResourceManager::getInstance();

    PAL_INFO(LOG_TAG, "Enter. start thread loop");
    if (!gsl_engine) {
        PAL_ERR(LOG_TAG, "Invalid sound trigger engine");
        return;
    }
    std::unique_lock<std::mutex> lck(gsl_engine->mutex_);
    while (!gsl_engine->exit_thread_) {
        PAL_VERBOSE(LOG_TAG, "waiting on cond");
        gsl_engine->cv_.wait(lck);
        PAL_DBG(LOG_TAG, "done waiting on cond");

        if (gsl_engine->exit_thread_) {
            PAL_VERBOSE(LOG_TAG, "Exit thread");
            rm->releaseWakeLock();
            break;
        }

        // skip detection handling if it is stopped/restarted
        gsl_engine->state_mutex_.lock();
        if (gsl_engine->eng_state_ != ENG_DETECTED) {
            gsl_engine->state_mutex_.unlock();
            PAL_DBG(LOG_TAG, "Engine stopped/restarted after notification");
            rm->releaseWakeLock();
            continue;
        }
        gsl_engine->state_mutex_.unlock();

        det_str = dynamic_cast<StreamSoundTrigger *>(
            gsl_engine->vui_intf_->GetDetectedStream());
        if (det_str) {
            if (gsl_engine->capture_requested_) {
                status = gsl_engine->StartBuffering(det_str);
                if (status < 0) {
                    gsl_engine->RestartRecognition_l(det_str);
                }
            } else {
                lck.unlock();
                status = det_str->SetEngineDetectionState(GMM_DETECTED);
                lck.lock();
                if (status < 0)
                    gsl_engine->RestartRecognition_l(det_str);
                else
                    gsl_engine->UpdateSessionPayload(ENGINE_RESET);
            }
        }
        /*
         * After detection is handled, update the state to Active
         * if other streams are attached to engine and active
         */
        if (det_str && gsl_engine->CheckIfOtherStreamsActive(det_str))
            gsl_engine->UpdateState(ENG_ACTIVE);

        rm->releaseWakeLock();
    }
    PAL_DBG(LOG_TAG, "Exit");
}

int32_t SoundTriggerEngineGsl::StartBuffering(Stream *s) {
    int32_t status = 0;
    int32_t size = 0;
    struct pal_buffer buf;
    size_t input_buf_size = 0;
    size_t input_buf_num = 0;
    uint32_t bytes_to_drop = 0;
    uint64_t drop_duration = 0;
    size_t total_read_size = 0;
    size_t ftrt_size = 0;
    size_t size_to_read = 0;
    size_t read_offset = 0;
    size_t bytes_written = 0;
    uint32_t sleep_ms = 0;
    bool event_notified = false;
    StreamSoundTrigger *st = (StreamSoundTrigger *)s;
    struct pal_mmap_position mmap_pos;
    FILE *dsp_output_fd = nullptr;
    ChronoSteadyClock_t kw_transfer_begin;
    ChronoSteadyClock_t kw_transfer_end;
    size_t retry_cnt = 0;

    PAL_DBG(LOG_TAG, "Enter");
    UpdateState(ENG_BUFFERING);
    s->getBufInfo(&input_buf_size, &input_buf_num, nullptr, nullptr);
    sleep_ms = (input_buf_size * input_buf_num) *
        BITS_PER_BYTE * MS_PER_SEC /
        (sm_cfg_->GetSampleRate() * sm_cfg_->GetBitWidth() *
        sm_cfg_->GetOutChannels());

    std::memset(&buf, 0, sizeof(struct pal_buffer));
    buf.size = input_buf_size * input_buf_num;
    buf.buffer = (uint8_t *)calloc(1, buf.size);
    if (!buf.buffer) {
        PAL_ERR(LOG_TAG, "buf.buffer allocation failed");
        status = -ENOMEM;
        goto exit;
    }

    ftrt_size = vui_intf_->GetFTRTDataSize();
    if (IS_MODULE_TYPE_PDK(module_type_)) {
        drop_duration = (uint64_t)(buffer_config_.pre_roll_duration_in_ms -
            mid_buff_cfg_[st->GetModelId()].first);
        bytes_to_drop = UsToBytes(drop_duration * 1000);
    }

    if (vui_ptfm_info_->GetEnableDebugDumps()) {
        ST_DBG_FILE_OPEN_WR(dsp_output_fd, ST_DEBUG_DUMP_LOCATION,
            "dsp_output", "bin", dsp_output_cnt);
        PAL_DBG(LOG_TAG, "DSP output data stored in: dsp_output_%d.bin",
            dsp_output_cnt);
        dsp_output_cnt++;
    }

    if (mmap_buffer_size_ != 0) {
        read_offset = FrameToBytes(mmap_write_position_);
        PAL_DBG(LOG_TAG, "Start lab reading from offset %zu", read_offset);
    }

    ATRACE_ASYNC_BEGIN("stEngine: read FTRT data", (int32_t)module_type_);
    kw_transfer_begin = std::chrono::steady_clock::now();
    while (!exit_buffering_) {
        /*
         * When RestartRecognition is called during buffering thread
         * unlocking mutex, buffering loop may not exit properly as
         * exit_buffering_ is still false after RestartRecognition
         * finished. Add additional check here to avoid this corner
         * case.
         */
        if (eng_state_ != ENG_BUFFERING) {
            PAL_DBG(LOG_TAG, "engine is stopped/restarted, exit data reading");
            break;
        }

        PAL_VERBOSE(LOG_TAG, "request read %zu from gsl", buf.size);
        // read data from session
        ATRACE_ASYNC_BEGIN("stEngine: lab read", (int32_t)module_type_);
        if (mmap_buffer_size_ != 0) {
            /*
             * GetMmapPosition returns total frames written for this session
             * which will be accumulated during back to back detections, so
             * we get mmap position in SVA start and compute the difference
             * after detection, in this way we can get info for bytes written
             * and read after each detection.
             */
            status = session_->GetMmapPosition(s, &mmap_pos);
            if (!status) {
                if (mmap_pos.position_frames >= mmap_write_position_) {
                    bytes_written = FrameToBytes(mmap_pos.position_frames -
                        mmap_write_position_);
                    if (bytes_written == UINT32_MAX) {
                        PAL_ERR(LOG_TAG, "invalid frame value");
                        status = -EINVAL;
                        goto exit;
                    }
                } else {
                    PAL_ERR(LOG_TAG, "invalid mmap position value");
                    PAL_ERR(LOG_TAG, "position frames : %d, mmap write position : %d",
                         mmap_pos.position_frames, mmap_write_position_);
                    status = -EINVAL;
                    goto exit;
                }
                if (bytes_written > total_read_size) {
                    size_to_read = bytes_written - total_read_size;
                    retry_cnt = 0;
                } else {
                    retry_cnt++;
                    if (retry_cnt > MAX_MMAP_POSITION_QUERY_RETRY_CNT) {
                        status = -EIO;
                        goto exit;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
                    continue;
                }
                if (size_to_read > (2 * mmap_buffer_size_) - read_offset) {
                    PAL_ERR(LOG_TAG, "Bytes written is exceeding mmap buffer size");
                    status = -EINVAL;
                    goto exit;
                }
                PAL_VERBOSE(LOG_TAG, "Mmap write offset %zu, available bytes %zu",
                    bytes_written, size_to_read);
            } else {
                PAL_ERR(LOG_TAG, "Failed to get read position");
                status = -ENOMEM;
                goto exit;
            }

            if (size_to_read != buf.size) {
                buf.buffer = (uint8_t *)realloc(buf.buffer, size_to_read);
                if (!buf.buffer) {
                    PAL_ERR(LOG_TAG, "buf.buffer allocation failed");
                    status = -ENOMEM;
                    goto exit;
                }
                buf.size = size_to_read;
            }

            // TODO: directly write to PalRingBuffer with shared buffer pointer
            if (read_offset + size_to_read <= mmap_buffer_size_) {
                ar_mem_cpy(buf.buffer, size_to_read,
                    (uint8_t *)mmap_buffer_.buffer + read_offset,
                    size_to_read);
                read_offset += size_to_read;
            } else {
                ar_mem_cpy(buf.buffer, mmap_buffer_size_ - read_offset,
                    (uint8_t *)mmap_buffer_.buffer + read_offset,
                    mmap_buffer_size_ - read_offset);
                ar_mem_cpy(buf.buffer + mmap_buffer_size_ - read_offset,
                    size_to_read + read_offset - mmap_buffer_size_,
                    (uint8_t *)mmap_buffer_.buffer,
                    size_to_read + read_offset - mmap_buffer_size_);
                read_offset = size_to_read + read_offset - mmap_buffer_size_;
            }
            size = size_to_read;
            PAL_VERBOSE(LOG_TAG, "read %d bytes from shared buffer", size);
            total_read_size += size;
        } else if (buffer_->getFreeSize() >= buf.size) {
            if (total_read_size < ftrt_size &&
                ftrt_size - total_read_size < buf.size) {
                buf.size = ftrt_size - total_read_size;
                status = session_->read(s, SHMEM_ENDPOINT, &buf, &size);
                buf.size = input_buf_size * input_buf_num;
            } else {
                status = session_->read(s, SHMEM_ENDPOINT, &buf, &size);
            }
            if (status) {
                break;
            }
            PAL_VERBOSE(LOG_TAG, "requested %zu, read %d", buf.size, size);
            total_read_size += size;
        }
        ATRACE_ASYNC_END("stEngine: lab read", (int32_t)module_type_);
        // write data to ring buffer
        if (size) {
            if (total_read_size < ftrt_size)
                vui_intf_->UpdateFTRTData(buf.buffer, size);
            size_t ret = 0;
            if (bytes_to_drop) {
                if (size < bytes_to_drop) {
                    bytes_to_drop -= size;
                } else {
                    ret = buffer_->write((void*)(buf.buffer + bytes_to_drop),
                        size - bytes_to_drop);
                    bytes_to_drop = 0;
                    if (vui_ptfm_info_->GetEnableDebugDumps()) {
                        ST_DBG_FILE_WRITE(dsp_output_fd,
                            buf.buffer + bytes_to_drop, size - bytes_to_drop);
                    }
                }
            } else {
                ret = buffer_->write(buf.buffer, size);
                if (vui_ptfm_info_->GetEnableDebugDumps()) {
                    ST_DBG_FILE_WRITE(dsp_output_fd, buf.buffer, size);
                }
            }
            PAL_VERBOSE(LOG_TAG, "%zu written to ring buffer", ret);
        }

        // notify client until ftrt data read
        if (total_read_size >= ftrt_size) {
            if (!event_notified) {
                kw_transfer_end = std::chrono::steady_clock::now();
                ATRACE_ASYNC_END("stEngine: read FTRT data", (int32_t)module_type_);
                kw_transfer_latency_ = std::chrono::duration_cast<std::chrono::milliseconds>(
                    kw_transfer_end - kw_transfer_begin).count();
                PAL_INFO(LOG_TAG, "FTRT data read done! total_read_size %zu, ftrt_size %zu, read latency %llums",
                        total_read_size, ftrt_size, (long long)kw_transfer_latency_);

                StreamSoundTrigger *s = dynamic_cast<StreamSoundTrigger *>(vui_intf_->GetDetectedStream());
                if (s) {
                    mutex_.unlock();
                    status = s->SetEngineDetectionState(GMM_DETECTED);
                    mutex_.lock();
                    if (status < 0)
                        RestartRecognition_l(s);
                }

                if (status) {
                    PAL_ERR(LOG_TAG,
                        "Failed to set engine detection state to stream, status %d",
                        status);
                    break;
                }
                event_notified = true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
        }
    }

exit:
    if (buf.buffer) {
        free(buf.buffer);
    }
    if (buf.ts) {
        free(buf.ts);
    }
    if (vui_ptfm_info_->GetEnableDebugDumps()) {
        ST_DBG_FILE_CLOSE(dsp_output_fd);
    }
    PAL_DBG(LOG_TAG, "Exit, status %d", status);
    return status;
}

SoundTriggerEngineGsl::SoundTriggerEngineGsl(
    Stream *s,
    listen_model_indicator_enum type,
    st_module_type_t module_type,
    std::shared_ptr<VUIStreamConfig> sm_cfg) {

    struct pal_stream_attributes sAttr;
    std::shared_ptr<ResourceManager> rm = nullptr;
    engine_type_ = type;
    module_type_ = module_type;
    sm_cfg_ = sm_cfg;
    vui_intf_ = nullptr;
    exit_thread_ = false;
    exit_buffering_ = false;
    capture_requested_ = false;
    stream_handle_ = s;
    sm_data_ = nullptr;
    reader_ = nullptr;
    buffer_ = nullptr;
    rx_ec_dev_ = nullptr;
    is_qcva_uuid_ = false;
    custom_data = nullptr;
    custom_data_size = 0;
    custom_detection_event = nullptr;
    custom_detection_event_size = 0;
    mmap_write_position_ = 0;
    kw_transfer_latency_ = 0;
    std::shared_ptr<VUIFirstStageConfig> sm_module_info = nullptr;
    builder_ = new PayloadBuilder();
    eng_sm_info_ = new SoundModelInfo();
    dev_disconnect_count_ = 0;
    lpi_miid_ = 0;
    nlpi_miid_ = 0;
    ec_ref_count_ = 0;
    is_crr_dev_using_ext_ec_ = false;

    UpdateState(ENG_IDLE);

    use_lpi_ = dynamic_cast<StreamSoundTrigger *>(s)->GetLPIEnabled();

    std::memset(&detection_event_info_, 0, sizeof(struct detection_event_info));
    std::memset(&pdk_wakeup_config_, 0, sizeof(pdk_wakeup_config_));
    std::memset(&buffer_config_, 0, sizeof(buffer_config_));
    std::memset(&mmap_buffer_, 0, sizeof(mmap_buffer_));
    mmap_buffer_.fd = -1;

    PAL_DBG(LOG_TAG, "Enter");

    vui_ptfm_info_ = VoiceUIPlatformInfo::GetInstance();
    if (!vui_ptfm_info_) {
        PAL_ERR(LOG_TAG, "No voice UI platform info present");
        throw std::runtime_error("No voice UI platform info present");
    }

    if (sm_cfg_) {
        sample_rate_ = sm_cfg_->GetSampleRate();
        bit_width_ = sm_cfg_->GetBitWidth();
        channels_ = sm_cfg_->GetOutChannels();

        sm_module_info = sm_cfg_->GetVUIFirstStageConfig(module_type_);
        if (!sm_module_info) {
            PAL_ERR(LOG_TAG, "Failed to get module info");
            throw std::runtime_error("Failed to get module info");
        }
        for (int i = LOAD_SOUND_MODEL; i < MAX_PARAM_IDS; i++) {
            module_tag_ids_[i] = sm_module_info->
                                 GetModuleTagId((st_param_id_type_t)i);
            param_ids_[i] = sm_module_info->GetParamId((st_param_id_type_t)i);
        }

        if (vui_ptfm_info_->GetMmapEnable()) {
            mmap_buffer_size_ = (vui_ptfm_info_->GetMmapBufferDuration() / MS_PER_SEC) *
                                 sm_cfg_->GetSampleRate() * sm_cfg_->GetBitWidth() *
                                 sm_cfg_->GetOutChannels() / BITS_PER_BYTE;
            if (mmap_buffer_size_ == 0) {
                PAL_ERR(LOG_TAG, "Mmap buffer duration not set");
                throw std::runtime_error("Mmap buffer duration not set");
            }
        } else {
            mmap_buffer_size_ = 0;
        }

        is_qcva_uuid_ = sm_cfg->isQCVAUUID();
    } else {
        PAL_ERR(LOG_TAG, "No sound model config present");
        throw std::runtime_error("No sound model config present");
    }

    // Create session
    rm = ResourceManager::getInstance();
    if (!rm) {
        PAL_ERR(LOG_TAG, "Failed to get ResourceManager instance");
        throw std::runtime_error("Failed to get ResourceManager instance");
    }
    stream_handle_->getStreamAttributes(&sAttr);
    session_ = Session::makeSession(rm, &sAttr);
    if (!session_) {
        PAL_ERR(LOG_TAG, "Failed to create session");
        throw std::runtime_error("Failed to create session");
    }

    session_->registerCallBack(HandleSessionCallBack, (uint64_t)this);

    buffer_thread_handler_ =
        std::thread(SoundTriggerEngineGsl::EventProcessingThread, this);
    if (!buffer_thread_handler_.joinable()) {
        PAL_ERR(LOG_TAG, "failed to create even processing thread");
        throw std::runtime_error("failed to create even processing thread");
    }

    PAL_DBG(LOG_TAG, "Exit");
}

SoundTriggerEngineGsl::~SoundTriggerEngineGsl() {
    PAL_INFO(LOG_TAG, "Enter");
    {
        exit_buffering_ = true;
        std::unique_lock<std::mutex> lck(mutex_);
        exit_thread_ = true;
        cv_.notify_one();
    }
    if (buffer_thread_handler_.joinable()) {
        buffer_thread_handler_.join();
        PAL_INFO(LOG_TAG, "Thread joined");
    }

    if (mmap_buffer_.fd != -1) {
        close(mmap_buffer_.fd);
    }

    if (buffer_) {
        delete buffer_;
    }
    if (reader_) {
        delete reader_;
    }
    if (eng_sm_info_) {
        delete eng_sm_info_;
    }
    if (builder_) {
        delete builder_;
    }
    if (session_) {
        delete session_;
    }
    PAL_INFO(LOG_TAG, "Exit");
}

int32_t SoundTriggerEngineGsl::QuerySoundModel(SoundModelInfo *sm_info,
                                               uint8_t *data,
                                               uint32_t data_size) {

    listen_sound_model_header sml_header = {};
    listen_model_type model = {};
    listen_status_enum sml_ret = kSucess;
    uint32_t status = 0;
    std::shared_ptr<SoundModelLib>sml = SoundModelLib::GetInstance();

    PAL_VERBOSE(LOG_TAG, "Enter: sound model size %d", data_size);

    if (!sml || !sm_info) {
        PAL_ERR(LOG_TAG, "soundmodel lib handle or model info NULL");
        return -ENOSYS;
    }

    model.data = data;
    model.size = data_size;

    sml_ret = sml->GetSoundModelHeader_(&model, &sml_header);
    if (sml_ret != kSucess) {
        PAL_ERR(LOG_TAG, "GetSoundModelHeader_ failed, err %d ", sml_ret);
        return -EINVAL;
    }
    if (sml_header.numKeywords == 0) {
        PAL_ERR(LOG_TAG, "num keywords zero!");
        return -EINVAL;
    }

    if (sml_header.numActiveUserKeywordPairs < sml_header.numUsers) {
        PAL_ERR(LOG_TAG, "smlib activeUserKwPairs(%d) < total users (%d)",
                sml_header.numActiveUserKeywordPairs, sml_header.numUsers);
        goto cleanup;
    }
    if (sml_header.numUsers && !sml_header.userKeywordPairFlags) {
        PAL_ERR(LOG_TAG, "userKeywordPairFlags is NULL, numUsers (%d)",
                sml_header.numUsers);
        goto cleanup;
    }

    PAL_VERBOSE(LOG_TAG, "SML model.data %pK, model.size %d", model.data,
            model.size);
    status = sm_info->SetKeyPhrases(&model, sml_header.numKeywords);
    if (status)
        goto cleanup;

    status = sm_info->SetUsers(&model, sml_header.numUsers);
    if (status)
        goto cleanup;

    status = sm_info->SetConfLevels(sml_header.numActiveUserKeywordPairs,
                                    sml_header.numUsersSetPerKw,
                                    sml_header.userKeywordPairFlags);
    if (status)
        goto cleanup;

    sml_ret = sml->ReleaseSoundModelHeader_(&sml_header);
    if (sml_ret != kSucess) {
        PAL_ERR(LOG_TAG, "ReleaseSoundModelHeader failed, err %d ", sml_ret);
        status = -EINVAL;
        goto cleanup_1;
    }
    PAL_VERBOSE(LOG_TAG, "exit");
    return 0;

cleanup:
    sml_ret = sml->ReleaseSoundModelHeader_(&sml_header);
    if (sml_ret != kSucess)
        PAL_ERR(LOG_TAG, "ReleaseSoundModelHeader_ failed, err %d ", sml_ret);

cleanup_1:
    return status;
}

int32_t SoundTriggerEngineGsl::MergeSoundModels(uint32_t num_models,
             listen_model_type *in_models[],
             listen_model_type *out_model) {

    listen_status_enum sm_ret = kSucess;
    int32_t status = 0;
    std::shared_ptr<SoundModelLib>sml = SoundModelLib::GetInstance();

    if (!sml) {
        PAL_ERR(LOG_TAG, "soundmodel lib handle NULL");
        return -ENOSYS;
    }

    PAL_VERBOSE(LOG_TAG, "num_models to merge %d", num_models);
    sm_ret = sml->GetMergedModelSize_(num_models, in_models,
        &out_model->size);
    if ((sm_ret != kSucess) || !out_model->size) {
        PAL_ERR(LOG_TAG, "GetMergedModelSize failed, err %d, size %d",
            sm_ret, out_model->size);
        return -EINVAL;
    }
    PAL_INFO(LOG_TAG, "merged sound model size %d", out_model->size);

    out_model->data = (uint8_t *)calloc(1, out_model->size * sizeof(char));
    if (!out_model->data) {
        PAL_ERR(LOG_TAG, "Merged sound model allocation failed");
        return -ENOMEM;
    }

    sm_ret = sml->MergeModels_(num_models, in_models, out_model);
    if (sm_ret != kSucess) {
        PAL_ERR(LOG_TAG, "MergeModels failed, err %d", sm_ret);
        status = -EINVAL;
        goto cleanup;
    }
    if (!out_model->data || !out_model->size) {
        PAL_ERR(LOG_TAG, "MergeModels returned NULL data or size %d",
              out_model->size);
        status = -EINVAL;
        goto cleanup;
    }

    if (vui_ptfm_info_->GetEnableDebugDumps()) {
        ST_DBG_DECLARE(FILE *sm_fd = NULL;
            static int sm_cnt = 0);
        ST_DBG_FILE_OPEN_WR(sm_fd, ST_DEBUG_DUMP_LOCATION,
            "st_smlib_output_merged_sm", "bin", sm_cnt);
        ST_DBG_FILE_WRITE(sm_fd, out_model->data, out_model->size);
        ST_DBG_FILE_CLOSE(sm_fd);
        PAL_DBG(LOG_TAG, "SM returned from SML merge stored in: st_smlib_output_merged_sm_%d.bin",
            sm_cnt);
        sm_cnt++;
    }
    PAL_DBG(LOG_TAG, "Exit, status: %d", status);
    return 0;

cleanup:
    if (out_model->data) {
        free(out_model->data);
        out_model->data = nullptr;
        out_model->size = 0;
    }
    return status;
}

int32_t SoundTriggerEngineGsl::AddSoundModel(Stream *s, uint8_t *data,
                                              uint32_t data_size){

    int32_t status = 0;
    uint32_t num_models = 0;
    StreamSoundTrigger *st = dynamic_cast<StreamSoundTrigger *>(s);
    listen_model_type **in_models = nullptr;
    listen_model_type out_model = {};
    SoundModelInfo *sm_info;

    PAL_VERBOSE(LOG_TAG, "Enter");
    if (vui_intf_->GetSoundModelInfo(st)->GetModelData()) {
        PAL_DBG(LOG_TAG, "Stream model already added");
        return 0;
    }

    if (!is_qcva_uuid_ && !vui_intf_->IsQCWakeUpConfigUsed()) {
        vui_intf_->GetSoundModelInfo(st)->SetModelData(data, data_size);
        *eng_sm_info_ = *(vui_intf_->GetSoundModelInfo(st));
        vui_intf_->SetSoundModelInfo(eng_sm_info_);
        sm_merged_ = false;
        return 0;
    }

    /* Populate sound model info for the incoming stream model */
    status = QuerySoundModel(vui_intf_->GetSoundModelInfo(st), data, data_size);
    if (status) {
        PAL_ERR(LOG_TAG, "QuerySoundModel failed status: %d", status);
        return status;
    }

    vui_intf_->GetSoundModelInfo(st)->SetModelData(data, data_size);

    /* Check for remaining stream sound models to merge */
    for (int i = 0; i < eng_streams_.size(); i++) {
        StreamSoundTrigger *sst = dynamic_cast<StreamSoundTrigger *>(eng_streams_[i]);
        if (s != eng_streams_[i] && sst && vui_intf_->GetSoundModelInfo(sst)->GetModelData())
             num_models++;
    }

    if (!num_models) {
        PAL_DBG(LOG_TAG, "Copy model info from incoming stream to engine");
        *eng_sm_info_ = *(vui_intf_->GetSoundModelInfo(st));
        vui_intf_->SetSoundModelInfo(eng_sm_info_);
        sm_merged_ = false;
        return 0;
    }

    PAL_VERBOSE(LOG_TAG, "number of existing models: %d", num_models);
    /*
     * Merge this stream model with already existing merged model due to other
     * streams models.
     */
    if (!eng_sm_info_) {
        PAL_ERR(LOG_TAG, "eng_sm_info is NULL");
        status = -EINVAL;
        goto cleanup;
    }

    if (!eng_sm_info_->GetModelData()) {
        if (num_models == 1) {
            /*
             * Its not a merged model yet, but engine sm_data is valid
             * and must be pointing to single stream sm_data
             */
            PAL_ERR(LOG_TAG, "Model data is NULL, num_models: %d", num_models);
            status = -EINVAL;
            goto cleanup;
        } else if (!sm_merged_) {
            PAL_ERR(LOG_TAG, "Unexpected, no pre-existing merged model,"
                  "num current models %d", num_models);
            status = -EINVAL;
            goto cleanup;
        }
    }

    /* Merge this stream model with remaining streams models */
    num_models = 2;
    SoundModelInfo::AllocArrayPtrs((char***)&in_models, num_models,
                                   sizeof(listen_model_type));
    if (!in_models) {
        PAL_ERR(LOG_TAG, "in_models allocation failed");
        status = -ENOMEM;
        goto cleanup;
    }
    /* Add existing model */
    in_models[0]->data = eng_sm_info_->GetModelData();
    in_models[0]->size = eng_sm_info_->GetModelSize();
    /* Add incoming stream model */
    in_models[1]->data = data;
    in_models[1]->size = data_size;

    status = MergeSoundModels(num_models, in_models, &out_model);
    if (status) {
        PAL_ERR(LOG_TAG, "merge models failed");
        goto cleanup;
    }
    sm_info = new SoundModelInfo();
    sm_info->SetModelData(out_model.data, out_model.size);

    /* Populate sound model info for the merged stream models */
    status = QuerySoundModel(sm_info, out_model.data, out_model.size);
    if (status) {
        delete sm_info;
        goto cleanup;
    }

    if (out_model.size < eng_sm_info_->GetModelSize()) {
        PAL_ERR(LOG_TAG, "Unexpected, merged model sz %d < current sz %d",
            out_model.size, eng_sm_info_->GetModelSize());
        delete sm_info;
        status = -EINVAL;
        goto cleanup;
    }
    SoundModelInfo::FreeArrayPtrs((char **)in_models, num_models);
    in_models = nullptr;

    /* Update the new merged model */
    PAL_INFO(LOG_TAG, "Updated sound model: current size %d, new size %d",
        eng_sm_info_->GetModelSize(), out_model.size);
    *eng_sm_info_ = *sm_info;
    vui_intf_->SetSoundModelInfo(eng_sm_info_);
    sm_merged_ = true;

    delete sm_info;
    PAL_DBG(LOG_TAG, "Exit: status %d", status);
    return 0;
cleanup:
    if (out_model.data)
        free(out_model.data);

    if (in_models)
        SoundModelInfo::FreeArrayPtrs((char **)in_models, num_models);

    return status;
}

int32_t SoundTriggerEngineGsl::DeleteFromMergedModel(char **keyphrases,
    uint32_t num_keyphrases, listen_model_type *in_model,
    listen_model_type *out_model) {

    listen_model_type merge_model = {};
    listen_status_enum sm_ret = kSucess;
    uint32_t out_model_sz = 0;
    int32_t status = 0;
    std::shared_ptr<SoundModelLib>sml = SoundModelLib::GetInstance();

    out_model->data = nullptr;
    out_model->size = 0;
    merge_model.data = in_model->data;
    merge_model.size = in_model->size;

    for (uint32_t i = 0; i < num_keyphrases; i++) {
        sm_ret = sml->GetSizeAfterDeleting_(&merge_model, keyphrases[i],
                                                   nullptr, &out_model_sz);
        if (sm_ret != kSucess) {
            PAL_ERR(LOG_TAG, "GetSizeAfterDeleting failed %d", sm_ret);
            status = -EINVAL;
            goto cleanup;
        }
        if (out_model_sz >= in_model->size) {
            PAL_ERR(LOG_TAG, "unexpected, GetSizeAfterDeleting returned size %d"
                  "not less than merged model size %d",
                  out_model_sz, in_model->size);
            status = -EINVAL;
            goto cleanup;
        }
        PAL_VERBOSE(LOG_TAG, "Size after deleting kw[%d] = %d", i, out_model_sz);
        if (!out_model->data) {
            /* Valid if deleting multiple keyphrases one after other */
            free(out_model->data);
            out_model->size = 0;
        }
        out_model->data = (uint8_t *)calloc(1, out_model_sz * sizeof(char));
        if (!out_model->data) {
            PAL_ERR(LOG_TAG, "Merge sound model allocation failed, size %d ",
                  out_model_sz);
            status = -ENOMEM;
            goto cleanup;
        }
        out_model->size = out_model_sz;

        sm_ret = sml->DeleteFromModel_(&merge_model, keyphrases[i],
                                              nullptr, out_model);
        if (sm_ret != kSucess) {
            PAL_ERR(LOG_TAG, "DeleteFromModel failed %d", sm_ret);
            status = -EINVAL;
            goto cleanup;
        }
        if (out_model->size != out_model_sz) {
            PAL_ERR(LOG_TAG, "unexpected, out_model size %d != expected size %d",
                  out_model->size, out_model_sz);
            status = -EINVAL;
            goto cleanup;
        }
        /* Used if deleting multiple keyphrases one after other */
        merge_model.data = out_model->data;
        merge_model.size = out_model->size;
    }

    if (vui_ptfm_info_->GetEnableDebugDumps()) {
        ST_DBG_DECLARE(FILE *sm_fd = NULL; static int sm_cnt = 0);
        ST_DBG_FILE_OPEN_WR(sm_fd, ST_DEBUG_DUMP_LOCATION,
            "st_smlib_output_deleted_sm", "bin", sm_cnt);
        ST_DBG_FILE_WRITE(sm_fd, merge_model.data, merge_model.size);
        ST_DBG_FILE_CLOSE(sm_fd);
        PAL_DBG(LOG_TAG, "SM returned from SML delete stored in: st_smlib_output_deleted_sm_%d.bin",
            sm_cnt);
        sm_cnt++;
    }
    return 0;

cleanup:
    if (out_model->data) {
        free(out_model->data);
        out_model->data = nullptr;
    }
    return status;
}

int32_t SoundTriggerEngineGsl::DeleteSoundModel(Stream *s) {

    int32_t status = 0;
    uint32_t num_models = 0;
    StreamSoundTrigger *st = dynamic_cast<StreamSoundTrigger *>(s);
    StreamSoundTrigger *rem_st = nullptr;
    listen_model_type in_model = {};
    listen_model_type out_model = {};
    SoundModelInfo *sm_info = nullptr;

    PAL_VERBOSE(LOG_TAG, "Enter");
    if (!vui_intf_->GetSoundModelInfo(st)->GetModelData()) {
        PAL_DBG(LOG_TAG, "Stream model data already deleted");
        return 0;
    }

    PAL_VERBOSE(LOG_TAG, "sm_data %pK, sm_size %d",
          vui_intf_->GetSoundModelInfo(st)->GetModelData(),
          vui_intf_->GetSoundModelInfo(st)->GetModelSize());

    /* Check for remaining streams sound models to merge */
    for (int i = 0; i < eng_streams_.size(); i++) {
        StreamSoundTrigger *sst = dynamic_cast<StreamSoundTrigger *>(eng_streams_[i]);
        if (s != eng_streams_[i] && sst) {
             if (vui_intf_->GetSoundModelInfo(sst) &&
                 vui_intf_->GetSoundModelInfo(sst)->GetModelData()) {
                 rem_st = sst;
                 num_models++;
                 PAL_DBG(LOG_TAG, "num_models: %d", num_models);
             }
        }
    }

    if (num_models == 0) {
        PAL_DBG(LOG_TAG, "No remaining models");
        return 0;
    }
    if (num_models == 1) {
        PAL_DBG(LOG_TAG, "reuse only remaining stream model, size %d",
            vui_intf_->GetSoundModelInfo(rem_st)->GetModelSize());
        /* If only one remaining stream model exists, re-use it */
        *eng_sm_info_ = *(vui_intf_->GetSoundModelInfo(rem_st));
        vui_intf_->SetSoundModelInfo(eng_sm_info_);
        wakeup_config_.num_active_models = eng_sm_info_->GetConfLevelsSize();
        for (int i = 0; i < eng_sm_info_->GetConfLevelsSize(); i++) {
            if (eng_sm_info_->GetConfLevels()) {
                wakeup_config_.confidence_levels[i] = eng_sm_info_->GetConfLevels()[i];
                wakeup_config_.keyword_user_enables[i] =
                    (wakeup_config_.confidence_levels[i] == 100) ? 0 : 1;
                PAL_DBG(LOG_TAG, "cf levels[%d] = %d", i, wakeup_config_.confidence_levels[i]);
            }
        }
        sm_merged_ = false;
        return 0;
    }

    /*
     * Delete this stream model with already existing merged model due to other
     * streams models.
     */
    if (!sm_merged_ || !(eng_sm_info_->GetModelData())) {
        PAL_ERR(LOG_TAG, "Unexpected, no pre-existing merged model to delete from,"
              "num current models %d", num_models);
        goto cleanup;
    }

    /* Existing merged model from which the current stream model to be deleted */
    in_model.data = eng_sm_info_->GetModelData();
    in_model.size = eng_sm_info_->GetModelSize();

    status = DeleteFromMergedModel(vui_intf_->GetSoundModelInfo(st)->GetKeyPhrases(),
        vui_intf_->GetSoundModelInfo(st)->GetNumKeyPhrases(),
        &in_model, &out_model);

    if (status)
        goto cleanup;
    sm_info = new SoundModelInfo();
    sm_info->SetModelData(out_model.data, out_model.size);

    /* Update existing merged model info with new merged model */
    status = QuerySoundModel(sm_info, out_model.data,
                               out_model.size);
    if (status)
        goto cleanup;

    if (out_model.size > eng_sm_info_->GetModelSize()) {
        PAL_ERR(LOG_TAG, "Unexpected, merged model sz %d > current sz %d",
            out_model.size, eng_sm_info_->GetModelSize());
        delete sm_info;
        status = -EINVAL;
        goto cleanup;
    }

    PAL_INFO(LOG_TAG, "Updated sound model: current size %d, new size %d",
        eng_sm_info_->GetModelSize(), out_model.size);

    *eng_sm_info_ = *sm_info;
    vui_intf_->SetSoundModelInfo(eng_sm_info_);
    sm_merged_ = true;

    return 0;

cleanup:
    if (out_model.data)
        free(out_model.data);

    return status;
}

int32_t SoundTriggerEngineGsl::UpdateEngineModel(Stream *s, uint8_t *data,
                                                 uint32_t data_size, bool add) {

    int32_t status = 0;

    if (add)
        status = AddSoundModel(s, data, data_size);
    else
        status = DeleteSoundModel(s);

    PAL_DBG(LOG_TAG, "Exit, status: %d", status);
    return status;
}

int32_t SoundTriggerEngineGsl::UpdateMergeConfLevelsPayload(
                               SoundModelInfo *src_sm_info,
                               bool set) {

    if (!src_sm_info) {
        PAL_ERR(LOG_TAG, "src sm info NULL");
        return -EINVAL;
    }

    if (!sm_merged_) {
        PAL_DBG(LOG_TAG, "Soundmodel is not merged, use source sm info");
        *eng_sm_info_ = *src_sm_info;
        if (!eng_sm_info_->GetConfLevels()) {
            PAL_ERR(LOG_TAG, "cf_levels_ allocation failed in assignment operator");
            return -ENOMEM;
        }
        for (uint32_t i = 0; i < eng_sm_info_->GetConfLevelsSize(); i++) {
            if (!set) {
                eng_sm_info_->UpdateConfLevel(i, MAX_CONF_LEVEL_VALUE);
                PAL_DBG(LOG_TAG, "reset: cf_levels[%d]=%d",
                    i, eng_sm_info_->GetConfLevels()[i]);
            }
        }
        return 0;
    }

    if (src_sm_info->GetConfLevelsSize() > eng_sm_info_->GetConfLevelsSize()) {
        PAL_ERR(LOG_TAG, "Unexpected, stream conf levels sz > eng conf levels sz");
        return -EINVAL;
    }

    for (uint32_t i = 0; i < src_sm_info->GetConfLevelsSize(); i++)
        PAL_VERBOSE(LOG_TAG, "source cf levels[%d] = %d for %s", i,
            src_sm_info->GetConfLevels()[i], src_sm_info->GetConfLevelsKwUsers()[i]);

    /* Populate DSP merged sound model conf levels */
    for (uint32_t i = 0; i < src_sm_info->GetConfLevelsSize(); i++) {
        for (uint32_t j = 0; j < eng_sm_info_->GetConfLevelsSize(); j++) {
            if (!strcmp(eng_sm_info_->GetConfLevelsKwUsers()[j],
                        src_sm_info->GetConfLevelsKwUsers()[i])) {
                if (set) {
                    eng_sm_info_->UpdateConfLevel(j, src_sm_info->GetConfLevels()[i]);
                    PAL_DBG(LOG_TAG, "set: cf_levels[%d]=%d",
                          j, eng_sm_info_->GetConfLevels()[j]);
                } else {
                    eng_sm_info_->UpdateConfLevel(j, MAX_CONF_LEVEL_VALUE);
                    PAL_DBG(LOG_TAG, "reset: cf_levels[%d]=%d",
                          j, eng_sm_info_->GetConfLevels()[j]);
                }
            }
        }
    }

    for (uint32_t i = 0; i < eng_sm_info_->GetConfLevelsSize(); i++)
        PAL_ERR(LOG_TAG, "engine cf_levels[%d] = %d",
            i, eng_sm_info_->GetConfLevels()[i]);

    return 0;
}

int32_t SoundTriggerEngineGsl::UpdateMergeConfLevelsWithActiveStreams() {

    int32_t status = 0;
    StreamSoundTrigger *st_str = nullptr;

    for (int i = 0; i < eng_streams_.size(); i++) {
        st_str = dynamic_cast<StreamSoundTrigger *>(eng_streams_[i]);
        if (st_str && st_str->GetCurrentStateId() == ST_STATE_ACTIVE) {
            PAL_VERBOSE(LOG_TAG, "update merge conf levels with other active streams");
            status = UpdateMergeConfLevelsPayload(vui_intf_->GetSoundModelInfo(st_str),
                        true);
            if (status)
                return status;
        }
    }
    return status;
}

void SoundTriggerEngineGsl::UpdateState(eng_state_t state) {

    state_mutex_.lock();
    PAL_INFO(LOG_TAG, "Engine state transitioned from %d to %d", eng_state_, state);
    eng_state_ = state;
    state_mutex_.unlock();

}

bool SoundTriggerEngineGsl::IsEngineActive() {

    state_mutex_.lock();
    if (eng_state_ == ENG_ACTIVE || eng_state_ == ENG_BUFFERING ||
        eng_state_ == ENG_DETECTED) {
        state_mutex_.unlock();
        return true;
    }
    state_mutex_.unlock();
    return false;
}

int32_t SoundTriggerEngineGsl::HandleMultiStreamLoad(Stream *s, uint8_t *data,
                                                     uint32_t data_size) {

    int32_t status = 0;
    bool restore_eng_state = false;

    PAL_DBG(LOG_TAG, "Enter");
    std::unique_lock<std::mutex> lck(mutex_);

    if (IsEngineActive()) {
        this->ProcessStopRecognition(eng_streams_[0]);
        restore_eng_state = true;
    }

    if (!IS_MODULE_TYPE_PDK(module_type_)) {
        status = session_->close(eng_streams_[0]);
        if (status)
            PAL_ERR(LOG_TAG, "Failed to close session, status = %d", status);
        if (mmap_buffer_.buffer) {
            close(mmap_buffer_.fd);
            mmap_buffer_.fd = -1;
            mmap_buffer_.buffer = nullptr;
        }
        UpdateState(ENG_IDLE);

        /* Update the engine with merged sound model */
        status = UpdateEngineModel(s, data, data_size, true);
        if (status) {
            PAL_ERR(LOG_TAG, "Failed to update engine model, status = %d", status);
            goto exit;
        }

        /*
         * Sound model merge would have changed the order of merge conf levels,
         * which need to be re-updated for all current active streams, if any.
        */
        status = UpdateMergeConfLevelsWithActiveStreams();
        if (status) {
            PAL_ERR(LOG_TAG, "Failed to update merge conf levels, status = %d",
                                                                      status);
            goto exit;
        }

        /* Load the updated/merged sound model */
        status = session_->open(eng_streams_[0]);
        if (0 != status) {
            PAL_ERR(LOG_TAG, "Failed to open session, status = %d", status);
            goto exit;
        }

        status = UpdateSessionPayload(LOAD_SOUND_MODEL);
        if (0 != status) {
            PAL_ERR(LOG_TAG, "Failed to update session payload, status = %d",
                                                                    status);
            session_->close(eng_streams_[0]);
            goto exit;
        }
    } else {
        status = UpdateSessionPayload(LOAD_SOUND_MODEL);

       if (0 != status) {
            PAL_ERR(LOG_TAG, "Failed to update session payload, status = %d",
                                                                     status);
            session_->close(s);
            goto exit;
        }
    }

    UpdateState(ENG_LOADED);

    if (restore_eng_state)
        status = ProcessStartRecognition(eng_streams_[0]);
exit:
    PAL_DBG(LOG_TAG, "Exit, status = %d", status);
    return status;
}

int32_t SoundTriggerEngineGsl::HandleMultiStreamUnloadPDK(Stream *s) {

    int32_t status = 0;
    uint32_t model_id = 0;
    uint32_t deleted_entries = 0;

    StreamSoundTrigger *st = dynamic_cast<StreamSoundTrigger *>(s);

    model_id = st->GetModelId();

    if (model_id == 0) {
        PAL_ERR(LOG_TAG, "Sound model not found");
        return -EINVAL;
    }

    deregister_config_.model_id = model_id;
    status = UpdateSessionPayload(UNLOAD_SOUND_MODEL);
    if (status != 0) {
        PAL_ERR(LOG_TAG,
        "Failed to update session payload for deregister multi sound model");
        return -EINVAL;
    }

    deleted_entries = mid_stream_map_.erase(model_id);
    if (deleted_entries == 0) {
        PAL_ERR(LOG_TAG, "Sound model not deleted");
        return -EINVAL;
    }

    deleted_entries = 0;
    deleted_entries = mid_buff_cfg_.erase(model_id);
    if (deleted_entries == 0) {
        PAL_ERR(LOG_TAG, "Buffer config map not updated");
        return -EINVAL;
    }

    deleted_entries = 0;
    deleted_entries = mid_wakeup_cfg_.erase(model_id);
    if (deleted_entries == 0) {
        PAL_ERR(LOG_TAG, "Wakeup config map not updated");
        return -EINVAL;
    }

    return status;
}

int32_t SoundTriggerEngineGsl::HandleMultiStreamUnload(Stream *s) {

    int32_t status = 0;

    bool restore_eng_state = false;

    PAL_DBG(LOG_TAG, "Enter");
    std::unique_lock<std::mutex> lck(mutex_);

    if (IsEngineActive()) {
        ProcessStopRecognition(eng_streams_[0]);
        restore_eng_state = true;
    }

    if (IS_MODULE_TYPE_PDK(module_type_)) {
        status = HandleMultiStreamUnloadPDK(s);
    } else {
        status = session_->close(eng_streams_[0]);
        if (status)
            PAL_ERR(LOG_TAG, "Failed to close session, status = %d", status);
        if (mmap_buffer_.buffer) {
            close(mmap_buffer_.fd);
            mmap_buffer_.fd = -1;
            mmap_buffer_.buffer = nullptr;
        }
        UpdateState(ENG_IDLE);
        /* Update the engine with modified sound model after deletion */
        status = UpdateEngineModel(s, nullptr, 0, false);
        if (status) {
            PAL_ERR(LOG_TAG, "Failed to open session, status = %d", status);
            goto exit;
        }
        /*
         * Sound model merge would have changed the order of merge conf levels,
         * which need to be re-updated for all current active streams, if any.
         */
        status = UpdateMergeConfLevelsWithActiveStreams();
        if (status) {
            PAL_ERR(LOG_TAG, "Failed to update merge conf levels, status = %d",
                                                                          status);
        goto exit;
        }

        /* Load the updated/merged sound model */
        status = session_->open(eng_streams_[0]);
        if (0 != status) {
            PAL_ERR(LOG_TAG, "Failed to open session, status = %d", status);
            goto exit;
        }

        status = UpdateSessionPayload(LOAD_SOUND_MODEL);
        if (0 != status) {
            PAL_ERR(LOG_TAG, "Failed to update session payload, status = %d",
                                                                     status);
            session_->close(eng_streams_[0]);
            goto exit;
        }
        UpdateState(ENG_LOADED);
    }

    if (restore_eng_state && CheckIfOtherStreamsActive(s)) {
        if (IS_MODULE_TYPE_PDK(module_type_)) {
            std::map<uint32_t, struct detection_engine_config_stage1_pdk>::
                                     iterator itr = mid_wakeup_cfg_.begin();
            status = ProcessStartRecognition(mid_stream_map_[itr->first]);
        } else {
            status = ProcessStartRecognition(eng_streams_[0]);
        }
    }
exit:
    PAL_DBG(LOG_TAG, "Exit, status = %d", status);
    return status;
}

int32_t SoundTriggerEngineGsl::UpdateConfigPDK(uint32_t model_id) {

    pdk_wakeup_config_.mode = mid_wakeup_cfg_[model_id].mode;
    pdk_wakeup_config_.num_keywords = mid_wakeup_cfg_[model_id].num_keywords;
    pdk_wakeup_config_.model_id = model_id;
    pdk_wakeup_config_.custom_payload_size = mid_wakeup_cfg_[model_id]
                                              .custom_payload_size;
    for(int i = 0; i < mid_wakeup_cfg_[model_id].num_keywords; ++i) {
        pdk_wakeup_config_.confidence_levels[i] = mid_wakeup_cfg_[model_id]
                                                  .confidence_levels[i];
    }

    buffer_config_.model_id = model_id;
    buffer_config_.hist_buffer_duration_in_ms = mid_buff_cfg_[model_id]
                                                               .second;
    buffer_config_.pre_roll_duration_in_ms = mid_buff_cfg_[model_id]
                                                               .first;
    return 0;
}

int32_t SoundTriggerEngineGsl::LoadSoundModel(Stream *s, uint8_t *data,
                                              uint32_t data_size) {
    int32_t status = 0;
    uint32_t model_id = 0;
    StreamSoundTrigger *st = dynamic_cast<StreamSoundTrigger *>(s);
    struct param_id_detection_engine_register_multi_sound_model_t *pdk_data =
           nullptr;
    std::shared_ptr<ResourceManager> rm = ResourceManager::getInstance();

    PAL_DBG(LOG_TAG, "Enter");
    if (!data) {
        PAL_ERR(LOG_TAG, "Invalid sound model data status %d", status);
        status = -EINVAL;
        return status;
    }

    if (IS_MODULE_TYPE_PDK(module_type_)) {
        model_id = st->GetModelId();
        mid_stream_map_[model_id] = s;

        pdk_data = (struct param_id_detection_engine_register_multi_sound_model_t *)
             malloc(sizeof(
             struct param_id_detection_engine_register_multi_sound_model_t) +
             data_size);

        if (pdk_data == nullptr) {
            PAL_ERR(LOG_TAG, "Failed to allocate memory for pdk_data");
            return -EINVAL;
        }

        pdk_data->model_id = model_id;
        pdk_data->model_size = data_size;
        ar_mem_cpy(pdk_data->model, data_size, data, data_size);
        sm_data_ = (uint8_t *)pdk_data;
        data_size += (sizeof(param_id_detection_engine_register_multi_sound_model_t));
        sm_data_size_ = data_size;
        PAL_DBG(LOG_TAG, "model id : %x, model size : %u", pdk_data->model_id,
                pdk_data->model_size);
    }

    exit_buffering_ = true;
    std::unique_lock<std::mutex> lck(mutex_);
    /* Check whether any stream is already attached to this engine */
    if (CheckIfOtherStreamsAttached(s)) {
        lck.unlock();
        status = HandleMultiStreamLoad(s, data, data_size);
        lck.lock();
        goto exit;
    }

    status = session_->open(s);
    if (0 != status) {
        PAL_ERR(LOG_TAG, "Failed to open session, status = %d", status);
        goto exit;
    }

    if (!IS_MODULE_TYPE_PDK(module_type_)) {
        /* Update the engine with sound model */
        status = UpdateEngineModel(s, data, data_size, true);
        if (status) {
            PAL_ERR(LOG_TAG, "Failed to update engine model, status = %d", status);
            session_->close(s);
            goto exit;
        }

    }
    status = UpdateSessionPayload(LOAD_SOUND_MODEL);

    if (0 != status) {
        PAL_ERR(LOG_TAG, "Failed to update session payload, status = %d", status);
        session_->close(s);
        goto exit;
    }

    UpdateState(ENG_LOADED);
exit:
    if (!status)
        eng_streams_.push_back(s);

    if (status == -ENETRESET || rm->cardState != CARD_STATUS_ONLINE) {
        PAL_INFO(LOG_TAG, "Update the status in case of SSR");
        status = 0;
    }

    if (pdk_data)
        free(pdk_data);

    PAL_DBG(LOG_TAG, "Exit, status = %d", status);
    return status;
}

int32_t SoundTriggerEngineGsl::UnloadSoundModel(Stream *s) {
    int32_t status = 0;
    uint32_t model_id = 0;
    StreamSoundTrigger *st = dynamic_cast<StreamSoundTrigger *>(s);
    std::shared_ptr<ResourceManager> rm = ResourceManager::getInstance();

    PAL_DBG(LOG_TAG, "Enter");

    exit_buffering_ = true;
    std::unique_lock<std::mutex> lck(mutex_);

    /* Check whether any stream is already attached to this engine */
    if (CheckIfOtherStreamsAttached(s)) {
        lck.unlock();
        status = HandleMultiStreamUnload(s);
        lck.lock();
        goto exit;
    }

    status = session_->close(s);
    if (status)
        PAL_ERR(LOG_TAG, "Failed to close session, status = %d", status);

    /* Delete the sound model in engine */
    status = UpdateEngineModel(s, nullptr, 0, false);
    if (status)
        PAL_ERR(LOG_TAG, "Failed to update engine model, status = %d", status);

    UpdateState(ENG_IDLE);
exit:
    if (status == -ENETRESET || rm->cardState != CARD_STATUS_ONLINE) {
        PAL_INFO(LOG_TAG, "Update the status in case of SSR");
        status = 0;
    }

    model_id = st->GetModelId();
    auto iter = std::find(updated_cfg_.begin(), updated_cfg_.end(), model_id);
    if (iter != updated_cfg_.end()) {
        updated_cfg_.erase(iter);
    }

    PAL_DBG(LOG_TAG, "Exit, status = %d", status);
    return status;
}

int32_t SoundTriggerEngineGsl::UpdateConfigs() {
    int32_t status = 0;

    if (is_qcva_uuid_) { //TEMP_FOR_SETUP_T, there is no is_qcmd_uuid_ unlike SM8450.
        status = UpdateSessionPayload(WAKEUP_CONFIG);
        if (0 != status) {
            PAL_ERR(LOG_TAG, "Failed to set wake up config, status = %d",
                status);
            goto exit;
        }
    } else if (module_tag_ids_[CUSTOM_CONFIG] && param_ids_[CUSTOM_CONFIG]) {
        status = UpdateSessionPayload(CUSTOM_CONFIG);
        if (0 != status) {
            PAL_ERR(LOG_TAG, "Failed to set custom config, status = %d",
                status);
            goto exit;
        }
    }

    status = UpdateSessionPayload(BUFFERING_CONFIG);

    if (0 != status) {
        PAL_ERR(LOG_TAG, "Failed to set wake-up buffer config, status = %d",
                status);
        goto exit;
    }
exit:
    PAL_DBG(LOG_TAG, "Exit, status %d", status);
    return status;
}

int32_t SoundTriggerEngineGsl::ProcessStartRecognition(Stream *s) {

    int32_t status = 0;
    struct pal_mmap_position mmap_pos;

    PAL_DBG(LOG_TAG, "Enter");
    std::shared_ptr<ResourceManager> rm = ResourceManager::getInstance();
    rm->acquireWakeLock();
    // release custom detection event before start
    if (custom_detection_event) {
        free(custom_detection_event);
        custom_detection_event = nullptr;
        custom_detection_event_size = 0;
    }

    if (updated_cfg_.size() > 0) {
        for (int i = 0; i < updated_cfg_.size(); ++i) {
            UpdateConfigPDK(updated_cfg_[i]);
            status = UpdateConfigs();
            if (status != 0) {
                PAL_ERR(LOG_TAG, "Failed to Update configs");
                goto exit;
            }
            s = mid_stream_map_[updated_cfg_[i]];
        }
        updated_cfg_.clear();
    } else {
        if (IS_MODULE_TYPE_PDK(module_type_)) {
            StreamSoundTrigger *st = dynamic_cast<StreamSoundTrigger *>(s);
            if (pdk_wakeup_config_.model_id != st->GetModelId())
                UpdateConfigPDK(st->GetModelId());
        }
        status = UpdateConfigs();
        if (status != 0) {
            PAL_ERR(LOG_TAG, "Failed to Update configs");
            goto exit;
        }
    }

    status = session_->prepare(s);
    if (0 != status) {
        PAL_ERR(LOG_TAG, "Failed to prepare session, status = %d", status);
        goto exit;
    }

    if (mmap_buffer_size_ != 0 && !mmap_buffer_.buffer) {
        status = session_->createMmapBuffer(s, BytesToFrames(mmap_buffer_size_),
            &mmap_buffer_);
        if (0 != status) {
            PAL_ERR(LOG_TAG, "Failed to create mmap buffer, status = %d",
                status);
            goto exit;
        }
        mmap_buffer_size_ = FrameToBytes(mmap_buffer_.buffer_size_frames);
        PAL_DBG(LOG_TAG, "Resize mmap buffer size to %u",
            (uint32_t)mmap_buffer_size_);
    }

    status = session_->start(s);
    if (0 != status) {
        PAL_ERR(LOG_TAG, "Failed to start session, status = %d", status);
        goto exit;
    }

    // Update mmap write position after start
    if (mmap_buffer_size_) {
        mmap_write_position_ = 0;
        // reset wall clk in agm pcm plugin
        status = session_->ResetMmapBuffer(s);
        if (status)
            PAL_ERR(LOG_TAG, "Failed to reset mmap buffer, status %d", status);
    }
    exit_buffering_ = false;
    UpdateState(ENG_ACTIVE);
exit:
    rm->releaseWakeLock();
    PAL_DBG(LOG_TAG, "Exit, status %d", status);
    return status;
}

int32_t SoundTriggerEngineGsl::StartRecognition(Stream *s) {
    int32_t status = 0;
    std::shared_ptr<ResourceManager> rm = ResourceManager::getInstance();

    PAL_DBG(LOG_TAG, "Enter");

    exit_buffering_ = true;

    std::unique_lock<std::mutex> lck(mutex_);

    if (IsEngineActive())
        ProcessStopRecognition(eng_streams_[0]);

    status = ProcessStartRecognition(s);
    if (0 != status) {
        PAL_ERR(LOG_TAG, "Failed to start recognition, status = %d", status);
        if (status == -ENETRESET || rm->cardState != CARD_STATUS_ONLINE) {
            PAL_INFO(LOG_TAG, "Update the status in case of SSR");
            status = 0;
        }
    }

    PAL_DBG(LOG_TAG, "Exit, status = %d", status);

    return status;
}

int32_t SoundTriggerEngineGsl::RestartRecognition(Stream *s) {
    int32_t status = 0;

    PAL_VERBOSE(LOG_TAG, "Enter");
    exit_buffering_ = true;
    std::lock_guard<std::mutex> lck(mutex_);

    status = RestartRecognition_l(s);

    PAL_VERBOSE(LOG_TAG, "Exit, status = %d", status);
    return status;
}

int32_t SoundTriggerEngineGsl::RestartRecognition_l(Stream *s) {
    int32_t status = 0;
    struct pal_mmap_position mmap_pos;
    std::shared_ptr<ResourceManager> rm = ResourceManager::getInstance();

    PAL_DBG(LOG_TAG, "Enter");
    std::unique_lock<std::mutex> lck_eos(eos_mutex_);

    /* If engine is not active, do not restart recognition again */
    if (!IsEngineActive()) {
        PAL_INFO(LOG_TAG, "Engine is not active, return");
        return 0;
    }

    UpdateEngineConfigOnRestart(s);
    if (buffer_) {
        buffer_->reset();
    }
    // release custom detection event before start
    if (custom_detection_event) {
        free(custom_detection_event);
        custom_detection_event = nullptr;
        custom_detection_event_size = 0;
    }

    status = UpdateSessionPayload(ENGINE_RESET);
    if (status)
        PAL_ERR(LOG_TAG, "Failed to reset engine, status = %d", status);

    PAL_DBG(LOG_TAG, "Waiting for EOS event");
    cvEOS.wait_for(lck_eos, std::chrono::microseconds(TIMEOUT_FOR_EOS));
    PAL_DBG(LOG_TAG, "Waiting done for EOS event");

    // Update mmap write position after engine reset
    if (mmap_buffer_size_) {
        status = session_->GetMmapPosition(s, &mmap_pos);
        if (!status)
            mmap_write_position_ = mmap_pos.position_frames;
        else
            PAL_ERR(LOG_TAG, "Failed to get mmap position, status %d", status);

        // reset wall clk in agm pcm plugin
        status = session_->ResetMmapBuffer(s);
        if (status)
            PAL_ERR(LOG_TAG, "Failed to reset mmap buffer, status %d", status);

        mmap_write_position_ =
            mmap_write_position_ % BytesToFrames(mmap_buffer_size_);
        PAL_DBG(LOG_TAG, "Reset mmap write position to %zu", mmap_write_position_);
    }

    exit_buffering_ = false;
    UpdateState(ENG_ACTIVE);

    if (status == -ENETRESET || rm->cardState != CARD_STATUS_ONLINE) {
        PAL_INFO(LOG_TAG, "Update the status in case of SSR");
        status = 0;
    }
    PAL_DBG(LOG_TAG, "Exit, status = %d", status);
    return status;
}

int32_t SoundTriggerEngineGsl::ReconfigureDetectionGraph(Stream *s) {
    int32_t status = 0;
    StreamSoundTrigger *st = dynamic_cast<StreamSoundTrigger *>(s);
    std::shared_ptr<ResourceManager> rm = ResourceManager::getInstance();

    PAL_DBG(LOG_TAG, "Enter");

    exit_buffering_ = true;
    DetachStream(s, false);
    std::unique_lock<std::mutex> lck(mutex_);

    /*
     * For PDK or sound model merging usecase, multi streams will
     * be attached to same gsl engine, so we only need to close
     * session when all attached streams are detached.
     */
    if (eng_streams_.size() == 0) {

        status = session_->close(s);
        if (status)
            PAL_ERR(LOG_TAG, "Failed to close session, status = %d", status);

        UpdateState(ENG_IDLE);
        if (mmap_buffer_.buffer) {
            close(mmap_buffer_.fd);
            mmap_buffer_.fd = -1;
            mmap_buffer_.buffer = nullptr;
        }
        use_lpi_ = st->GetLPIEnabled();
    }

    /* Delete sound model of stream s from merged sound model */
    status = UpdateEngineModel(s, nullptr, 0, false);
    if (status)
        PAL_ERR(LOG_TAG, "Failed to update engine model, status = %d", status);
    vui_intf_->GetSoundModelInfo(st)->SetModelData(nullptr, 0);

    if (status == -ENETRESET || rm->cardState != CARD_STATUS_ONLINE) {
        PAL_INFO(LOG_TAG, "Update the status in case of SSR");
        status = 0;
    }

    PAL_DBG(LOG_TAG, "Exit, status = %d", status);
    return status;
}

int32_t SoundTriggerEngineGsl::ProcessStopRecognition(Stream *s) {
    int32_t status = 0;

    PAL_DBG(LOG_TAG, "Enter");
    std::shared_ptr<ResourceManager> rm = ResourceManager::getInstance();
    rm->acquireWakeLock();
    if (buffer_) {
        buffer_->reset();
    }

    /*
     * TODO: Currently spf requires ENGINE_RESET to close the DAM gate as stop
     * will not close the gate, rather just flushes the buffers, resulting in no
     * further detections.
     */
    status = UpdateSessionPayload(ENGINE_RESET);
    if (0 != status) {
        PAL_ERR(LOG_TAG, "Failed to reset detection engine, status = %d",
                status);
    }

    status = session_->stop(s);
    if (status) {
        PAL_ERR(LOG_TAG, "Failed to stop session, status = %d", status);
    }
    UpdateState(ENG_LOADED);
    rm->releaseWakeLock();
    PAL_DBG(LOG_TAG, "Exit, status = %d", status);
    return status;
}

int32_t SoundTriggerEngineGsl::StopRecognition(Stream *s) {
    int32_t status = 0;
    bool restore_eng_state = false;
    uint32_t old_conf = 0;
    uint32_t model_id = 0;
    std::shared_ptr<ResourceManager> rm = ResourceManager::getInstance();
    PAL_DBG(LOG_TAG, "Enter");

    exit_buffering_ = true;

    std::lock_guard<std::mutex> lck(mutex_);

    if (IsEngineActive()) {
        restore_eng_state = true;
        status = ProcessStopRecognition(s);
        if (0 != status) {
            PAL_ERR(LOG_TAG, "Failed to stop recognition, status = %d", status);
            goto exit;
        }

        if (CheckIfOtherStreamsActive(s)) {
            PAL_INFO(LOG_TAG, "Other streams are attached to current engine");
            if (restore_eng_state) {
                PAL_DBG(LOG_TAG, "Other streams are active, restart recognition");
                UpdateEngineConfigOnStop(s);
                if (IS_MODULE_TYPE_PDK(module_type_)) {
                    StreamSoundTrigger *st = dynamic_cast<StreamSoundTrigger *>(s);
                    model_id = st->GetModelId();
                    PAL_DBG(LOG_TAG, "Update conf level for model id : %x",
                            model_id);
                    for (int i = 0; i < mid_wakeup_cfg_[model_id].num_keywords; ++i) {
                        old_conf = mid_wakeup_cfg_[model_id].confidence_levels[i];
                        mid_wakeup_cfg_[model_id].confidence_levels[i] = 100;
                        PAL_DBG(LOG_TAG,
                             "Older conf level : %d Updated conf level : %d",
                        old_conf, mid_wakeup_cfg_[model_id].confidence_levels[i]);
                    }
                    updated_cfg_.push_back(model_id);
                    PAL_DBG(LOG_TAG, "Model id : %x added in updated_cfg_",
                         model_id);
                }
                status = ProcessStartRecognition(eng_streams_[0]);
                if (0 != status) {
                    PAL_ERR(LOG_TAG, "Failed to start recognition, status = %d", status);
                    goto exit;
                }
            }
        }
    } else {
        PAL_DBG(LOG_TAG, "Engine is not active hence no need to stop engine");
    }
exit:
    if (status == -ENETRESET || rm->cardState != CARD_STATUS_ONLINE) {
        PAL_INFO(LOG_TAG, "Update the status in case of SSR");
        status = 0;
    }
    PAL_DBG(LOG_TAG, "Exit, status = %d", status);
    return status;
}

bool SoundTriggerEngineGsl::CheckIfOtherStreamsAttached(Stream *s) {
    for (uint32_t i = 0; i < eng_streams_.size(); i++)
        if (s != eng_streams_[i])
            return true;

    return false;
}

bool SoundTriggerEngineGsl::CheckIfOtherStreamsActive(Stream *s) {
    StreamSoundTrigger *st = nullptr;

    for (uint32_t i = 0; i < eng_streams_.size(); i++) {
        st = dynamic_cast<StreamSoundTrigger *>(eng_streams_[i]);
        if (s != eng_streams_[i] && st &&
            (st->GetCurrentStateId() == ST_STATE_ACTIVE ||
             st->GetCurrentStateId() == ST_STATE_BUFFERING ||
             st->GetCurrentStateId() == ST_STATE_DETECTED))
            return true;
    }

    return false;
}

int32_t SoundTriggerEngineGsl::UpdateConfLevels(
    Stream *s,
    struct pal_st_recognition_config *config,
    uint8_t *conf_levels,
    uint32_t num_conf_levels) {

    int32_t status = 0;
    StreamSoundTrigger *st = dynamic_cast<StreamSoundTrigger *>(s);
    uint32_t recognition_mode = st->GetRecognitionMode();

    exit_buffering_ = true;
    std::lock_guard<std::mutex> lck(mutex_);

    if (!config) {
        status = -EINVAL;
        PAL_ERR(LOG_TAG, "Invalid config, status %d", status);
        goto exit;
    }

    if (!is_qcva_uuid_) {
        custom_data_size = num_conf_levels;
        custom_data = (uint8_t *)calloc(1, custom_data_size);
        if (!custom_data) {
            PAL_ERR(LOG_TAG, "Failed to allocate memory for custom data");
            status = -ENOMEM;
            goto exit;
        }
        ar_mem_cpy(custom_data, custom_data_size,
            conf_levels, custom_data_size);
        goto exit;
    }

    if (num_conf_levels != 0 && !conf_levels) {
        status = -EINVAL;
        PAL_ERR(LOG_TAG, "Invalid conf_levels, status %d", status);
        goto exit;
    }

    PAL_VERBOSE(LOG_TAG, "Enter, config: %pK", config);

    if (!IS_MODULE_TYPE_PDK(module_type_)
        && vui_intf_->GetSoundModelInfo(st)->GetConfLevelsSize() != num_conf_levels) {
        PAL_ERR(LOG_TAG, "Unexpected, stream cf levels %d != sm_info cf levels %d",
                num_conf_levels, vui_intf_->GetSoundModelInfo(st)->GetConfLevelsSize());
        status = -EINVAL;
        goto exit;
    }
    /*
     * Cache it to use when stream restarts without config update or
     * during only one remaining stream model as there won't be a
     * merged model yet.
     */
    if (!IS_MODULE_TYPE_PDK(module_type_)) {
        vui_intf_->GetSoundModelInfo(st)->UpdateConfLevelArray(conf_levels,
            num_conf_levels);

        status = UpdateMergeConfLevelsPayload(vui_intf_->GetSoundModelInfo(st), true);
        if (status) {
            PAL_ERR(LOG_TAG, "Update merge conf levels failed %d", status);
            goto exit;
        }
    }

    if (IS_MODULE_TYPE_PDK(module_type_)){
        pdk_wakeup_config_.mode = recognition_mode;
        pdk_wakeup_config_.num_keywords = num_conf_levels;
        pdk_wakeup_config_.model_id = st->GetModelId();
        pdk_wakeup_config_.custom_payload_size = 0;

        if (mid_wakeup_cfg_.find(st->GetModelId()) != mid_wakeup_cfg_.end() &&
            std::find(updated_cfg_.begin(), updated_cfg_.end(), st->GetModelId())
            == updated_cfg_.end() && IsEngineActive()) {
            updated_cfg_.push_back(st->GetModelId());
            PAL_DBG(LOG_TAG, "Model id : %x added to updated_cfg_ list", st->GetModelId());
        }

        mid_wakeup_cfg_[st->GetModelId()].mode = pdk_wakeup_config_.mode;
        PAL_DBG(LOG_TAG, "Updating mid_wakeup_cfg_ for model id %x", st->GetModelId());
        mid_wakeup_cfg_[st->GetModelId()].num_keywords =
                                         pdk_wakeup_config_.num_keywords;
        mid_wakeup_cfg_[st->GetModelId()].custom_payload_size =
                                  pdk_wakeup_config_.custom_payload_size;
        mid_wakeup_cfg_[st->GetModelId()].model_id = st->GetModelId();

        PAL_DBG(LOG_TAG,
            "pdk_wakeup_config_ mode : %u, custom_payload_size : %u, num_keywords : %u, model_id : %u",
        pdk_wakeup_config_.mode,
        pdk_wakeup_config_.custom_payload_size,
        pdk_wakeup_config_.num_keywords, pdk_wakeup_config_.model_id);
        for (int i = 0; i < pdk_wakeup_config_.num_keywords; ++i) {
             pdk_wakeup_config_.confidence_levels[i] = conf_levels[i];
            mid_wakeup_cfg_[st->GetModelId()].confidence_levels[i] =
                                                        conf_levels[i];
            PAL_DBG(LOG_TAG, "%dth keyword confidence level : %u", i,
                    pdk_wakeup_config_.confidence_levels[i]);
        }
    } else if (!CheckIfOtherStreamsAttached(s)) {
        wakeup_config_.mode = recognition_mode;
        wakeup_config_.custom_payload_size = 0;
        wakeup_config_.num_active_models = num_conf_levels;
        wakeup_config_.reserved = 0;
        for (int i = 0; i < wakeup_config_.num_active_models; i++) {
            wakeup_config_.confidence_levels[i] = conf_levels[i];
            wakeup_config_.keyword_user_enables[i] =
                (wakeup_config_.confidence_levels[i] == 100) ? 0 : 1;
            PAL_DBG(LOG_TAG, "cf levels[%d] = %d", i,
                    wakeup_config_.confidence_levels[i]);
        }
    } else {
        /* Update recognition mode considering all streams */
        if (wakeup_config_.mode != recognition_mode)
            wakeup_config_.mode |= recognition_mode;
            wakeup_config_.custom_payload_size = 0;
            wakeup_config_.num_active_models = eng_sm_info_->GetConfLevelsSize();
            wakeup_config_.reserved = 0;
            for (int i = 0; i < wakeup_config_.num_active_models; i++) {
            wakeup_config_.confidence_levels[i] = eng_sm_info_->
                                                        GetConfLevels()[i];
            wakeup_config_.keyword_user_enables[i] =
                (wakeup_config_.confidence_levels[i] == 100) ? 0 : 1;
            PAL_DBG(LOG_TAG, "cf levels[%d] = %d", i,
                    wakeup_config_.confidence_levels[i]);
            }
        }

exit:
    PAL_DBG(LOG_TAG, "Exit, status %d", status);

    return status;
}

void SoundTriggerEngineGsl::GetUpdatedBufConfig(uint32_t *hist_buffer_duration,
                                                uint32_t *pre_roll_duration){

    *hist_buffer_duration = buffer_config_.hist_buffer_duration_in_ms;
    *pre_roll_duration = buffer_config_.pre_roll_duration_in_ms;
}

int32_t SoundTriggerEngineGsl::UpdateBufConfig(Stream *s,
    uint32_t hist_buffer_duration,
    uint32_t pre_roll_duration) {

    int32_t status = 0;
    StreamSoundTrigger *st = dynamic_cast<StreamSoundTrigger *>(s);
    buffer_config_.model_id = st->GetModelId();

    if (!CheckIfOtherStreamsAttached(s)) {
        buffer_config_.hist_buffer_duration_in_ms = hist_buffer_duration;
        buffer_config_.pre_roll_duration_in_ms = pre_roll_duration;
    } else {
        if (hist_buffer_duration > buffer_config_.hist_buffer_duration_in_ms)
            buffer_config_.hist_buffer_duration_in_ms = hist_buffer_duration;
        if (pre_roll_duration > buffer_config_.pre_roll_duration_in_ms)
            buffer_config_.pre_roll_duration_in_ms = pre_roll_duration;
    }

    mid_buff_cfg_[buffer_config_.model_id] = std::make_pair(
                                    pre_roll_duration, hist_buffer_duration);
    PAL_DBG(LOG_TAG, "updated hist buf:%d msec, pre roll:%d msec",
       buffer_config_.hist_buffer_duration_in_ms,
       buffer_config_.pre_roll_duration_in_ms);

   return status;
}

int32_t SoundTriggerEngineGsl::UpdateEngineConfigOnStop(Stream *s) {

    int32_t status = 0;
    StreamSoundTrigger *st = nullptr;
    bool is_any_stream_active = false, enable_lab = false;
    uint32_t hb_duration = 0, pr_duration = 0;

    /* If there is only single stream, do nothing */
    if (!CheckIfOtherStreamsAttached(s))
        return 0;

    /*
     * Adjust history buffer and preroll durations to highest of
     * remaining streams.
     */
    for (uint32_t i = 0; i < eng_streams_.size(); i++) {
        st = dynamic_cast<StreamSoundTrigger *>(eng_streams_[i]);
        if (s != eng_streams_[i] && st && st->GetCurrentStateId() == ST_STATE_ACTIVE) {
            is_any_stream_active = true;
            if (hb_duration < st->GetHistBufDuration())
                hb_duration = st->GetHistBufDuration();
            if (pr_duration < st->GetPreRollDuration())
                pr_duration = st->GetPreRollDuration();
            if (!enable_lab)
                enable_lab = st->IsCaptureRequested();
        }
    }

    if (!is_any_stream_active) {
        PAL_DBG(LOG_TAG, "No stream is active, reset engine config");
        buffer_config_.hist_buffer_duration_in_ms = 0;
        buffer_config_.pre_roll_duration_in_ms = 0;
        capture_requested_ = false;
        return 0;
    }

    buffer_config_.hist_buffer_duration_in_ms = hb_duration;
    buffer_config_.pre_roll_duration_in_ms = pr_duration;
    capture_requested_ = enable_lab;

    if (!IS_MODULE_TYPE_PDK(module_type_)) {
        /* Update the merged conf levels considering this stream stop */
        StreamSoundTrigger *stopped_st = dynamic_cast<StreamSoundTrigger *>(s);
        status = UpdateMergeConfLevelsPayload(vui_intf_->GetSoundModelInfo(stopped_st), false);
        for (int i = 0; i < eng_sm_info_->GetConfLevelsSize(); i++) {
            wakeup_config_.confidence_levels[i] = eng_sm_info_->GetConfLevels()[i];
            wakeup_config_.keyword_user_enables[i] =
                (wakeup_config_.confidence_levels[i] == 100) ? 0 : 1;
            PAL_DBG(LOG_TAG, "cf levels[%d] = %d", i, wakeup_config_.confidence_levels[i]);
        }
    }

    return status;
}

int32_t SoundTriggerEngineGsl::UpdateEngineConfigOnRestart(Stream *s) {

    int32_t status = 0;
    StreamSoundTrigger *st = nullptr;
    uint32_t hb_duration = 0, pr_duration = 0;
    bool enable_lab = false;

    /*
     * Adjust history buffer and preroll durations to highest of
     * all streams, including current restarting stream.
     */
    for (uint32_t i = 0; i < eng_streams_.size(); i++) {
        st = dynamic_cast<StreamSoundTrigger *>(eng_streams_[i]);
        if (s == eng_streams_[i] || st->GetCurrentStateId() == ST_STATE_ACTIVE) {
            if (hb_duration < st->GetHistBufDuration())
                hb_duration = st->GetHistBufDuration();
            if (pr_duration < st->GetPreRollDuration())
                pr_duration = st->GetPreRollDuration();
            if (!enable_lab)
                enable_lab = st->IsCaptureRequested();
        }
    }

    buffer_config_.hist_buffer_duration_in_ms = hb_duration;
    buffer_config_.pre_roll_duration_in_ms = pr_duration;
    capture_requested_ = enable_lab;

    if (!IS_MODULE_TYPE_PDK(module_type_)) {
        /* Update the merged conf levels considering this stream restarted as well */
        StreamSoundTrigger *restarted_st = dynamic_cast<StreamSoundTrigger *>(s);
        status = UpdateMergeConfLevelsPayload(vui_intf_->GetSoundModelInfo(restarted_st), true);
        for (int i = 0; i < eng_sm_info_->GetConfLevelsSize(); i++) {
            wakeup_config_.confidence_levels[i] = eng_sm_info_->GetConfLevels()[i];
            wakeup_config_.keyword_user_enables[i] =
                (wakeup_config_.confidence_levels[i] == 100) ? 0 : 1;
            PAL_DBG(LOG_TAG, "cf levels[%d] = %d", i, wakeup_config_.confidence_levels[i]);
        }
    }

    return status;
}

void SoundTriggerEngineGsl::HandleSessionEvent(uint32_t event_id __unused,
                                               void *data, uint32_t size) {
    int32_t status = 0;
    uint32_t start_index = 0;
    uint32_t end_index = 0;
    std::shared_ptr<ResourceManager> rm = ResourceManager::getInstance();

    /*
     * reset ring buffer before parsing detection payload as
     * keyword index will be updated in parsing.
     */
    buffer_->reset();
    status = vui_intf_->ParseDetectionPayload(data, size);
    if (status) {
        PAL_ERR(LOG_TAG, "Failed to parse detection payload, status %d",
            status);
        rm->releaseWakeLock();
        return;
    }

    // update keyword index to ring buffer
    vui_intf_->GetKeywordIndex(&start_index, &end_index);
    buffer_->updateIndices(start_index, end_index);

    if (vui_ptfm_info_->GetEnableDebugDumps()) {
        ST_DBG_DECLARE(FILE *det_event_fd = NULL;
            static int det_event_cnt = 0);
        ST_DBG_FILE_OPEN_WR(det_event_fd, ST_DEBUG_DUMP_LOCATION,
            "det_event", "bin", det_event_cnt);
        ST_DBG_FILE_WRITE(det_event_fd, data, size);
        ST_DBG_FILE_CLOSE(det_event_fd);
        PAL_DBG(LOG_TAG, "detection event stored in: det_event_%d.bin",
            det_event_cnt);
        det_event_cnt++;
    }
    PAL_INFO(LOG_TAG, "signal event processing thread");
    ATRACE_BEGIN("stEngine: keyword detected");
    ATRACE_END();
    UpdateState(ENG_DETECTED);
    cv_.notify_one();
}

void SoundTriggerEngineGsl::HandleSessionCallBack(uint64_t hdl, uint32_t event_id,
                                                  void *data, uint32_t event_size, uint32_t miid __unused) {
    SoundTriggerEngineGsl *engine = nullptr;
    std::shared_ptr<ResourceManager> rm = ResourceManager::getInstance();

    PAL_DBG(LOG_TAG, "Enter, event detected on SPF, event id = 0x%x", event_id);
    if ((hdl == 0) || !data || !event_size) {
        PAL_ERR(LOG_TAG, "Invalid engine handle or event data or event size");
        return;
    }

    // Possible that AGM_EVENT_EOS_RENDERED could be sent during spf stop.
    // Check and handle only required detection event.
    if (event_id != EVENT_ID_DETECTION_ENGINE_GENERIC_INFO) {
        if (event_id == EVENT_ID_SH_MEM_PUSH_MODE_EOS_MARKER) {
            PAL_DBG(LOG_TAG,
            "Received event for EVENT_ID_SH_MEM_PUSH_MODE_EOS_MARKER");
            cvEOS.notify_all();
        }
        return;
    }

    engine = (SoundTriggerEngineGsl *)hdl;
    std::unique_lock<std::mutex> lck(engine->mutex_);
    /*
     * In multi sound model/merged sound model case, SPF might still give detections
     * for one model when the engine is in buffering state due to detection
     * event received for the other sound model.
     * Avoid handling the detection events for such detections.
     */
    engine->state_mutex_.lock();
    if (engine->eng_state_ == ENG_ACTIVE) {
        engine->state_mutex_.unlock();
        engine->detection_time_ = std::chrono::steady_clock::now();
        /* Acquire the wake lock and handle session event to avoid apps suspend */
        rm->acquireWakeLock();
        engine->HandleSessionEvent(event_id, data, event_size);
    } else if (engine->eng_state_ == ENG_LOADED) {
        engine->state_mutex_.unlock();
        PAL_DBG(LOG_TAG, "Detection comes during engine stop, ignore and reset");
        engine->UpdateSessionPayload(ENGINE_RESET);
    } else {
        engine->state_mutex_.unlock();
    }

    PAL_DBG(LOG_TAG, "Exit");
    return;
}

int32_t SoundTriggerEngineGsl::GetParameters(uint32_t param_id,
                                             void **payload) {
    int32_t status = 0;
    size_t size = 0;
    uint32_t miid = 0;

    PAL_DBG(LOG_TAG, "Enter");
    switch (param_id) {
        case PAL_PARAM_ID_DIRECTION_OF_ARRIVAL:
            status = session_->getParameters(stream_handle_, TAG_ECNS,
                                            param_id, payload);
            break;
        case PAL_PARAM_ID_WAKEUP_MODULE_VERSION:
            status = session_->openGraph(stream_handle_);
            if (status != 0) {
                PAL_ERR(LOG_TAG, "Failed to open graph, status = %d", status);
                return status;
            }
            status = session_->getMIID(nullptr,
                module_tag_ids_[MODULE_VERSION], &miid);
            if (status != 0) {
                PAL_ERR(LOG_TAG, "Failed to get instance id for tag %x, status = %d",
                    module_tag_ids_[MODULE_VERSION], status);
                return status;
            }
            // TODO: update query size here
            builder_->payloadQuery((uint8_t **)payload, &size, miid,
                param_ids_[MODULE_VERSION],
                sizeof(struct version_arch_payload));
            status = session_->getParameters(stream_handle_,
                module_tag_ids_[MODULE_VERSION], param_id, payload);
            status = session_->close(stream_handle_);
            if (status != 0) {
                PAL_ERR(LOG_TAG, "Failed to close session, status = %d", status);
                return status;
            }
            break;
        case PAL_PARAM_ID_KW_TRANSFER_LATENCY:
            *(uint64_t **)payload = &kw_transfer_latency_;
            break;
        default:
            status = -EINVAL;
            PAL_ERR(LOG_TAG, "Unsupported param id %u status %d",
                    param_id, status);
            goto exit;
    }

    if (status) {
        PAL_ERR(LOG_TAG, "Failed to get parameters, param id %d, status %d",
                param_id, status);
    }

exit:
    PAL_DBG(LOG_TAG, "Exit, status %d", status);

    return status;
}

int32_t SoundTriggerEngineGsl::ConnectSessionDevice(
    Stream* stream_handle,
    pal_stream_type_t stream_type,
    std::shared_ptr<Device> device_to_connect) {

    int32_t status = 0;

    if (dev_disconnect_count_ == 0)
        status = session_->connectSessionDevice(stream_handle, stream_type,
                                            device_to_connect);
    if (status != 0)
        dev_disconnect_count_++;

    PAL_DBG(LOG_TAG, "dev_disconnect_count_: %d", dev_disconnect_count_);
    return status;
}

int32_t SoundTriggerEngineGsl::DisconnectSessionDevice(
    Stream* stream_handle,
    pal_stream_type_t stream_type,
    std::shared_ptr<Device> device_to_disconnect) {

    int32_t status = 0;

    dev_disconnect_count_++;
    if (dev_disconnect_count_ == eng_streams_.size())
        status = session_->disconnectSessionDevice(stream_handle, stream_type,
                                               device_to_disconnect);
    if (status != 0)
        dev_disconnect_count_--;
    PAL_DBG(LOG_TAG, "dev_disconnect_count_: %d", dev_disconnect_count_);
    return status;
}

int32_t SoundTriggerEngineGsl::SetupSessionDevice(
    Stream* stream_handle,
    pal_stream_type_t stream_type,
    std::shared_ptr<Device> device_to_disconnect) {

    int32_t status = 0;

    dev_disconnect_count_--;
    if (dev_disconnect_count_ < 0)
        dev_disconnect_count_ = 0;

    if (dev_disconnect_count_ == 0)
        status = session_->setupSessionDevice(stream_handle, stream_type,
                                          device_to_disconnect);
    if (status != 0)
        dev_disconnect_count_++;

    PAL_DBG(LOG_TAG, "dev_disconnect_count_: %d", dev_disconnect_count_);
    return status;
}

void SoundTriggerEngineGsl::SetCaptureRequested(bool is_requested) {
    capture_requested_ |= is_requested;
    PAL_DBG(LOG_TAG, "capture requested %d, set to engine %d",
        is_requested, capture_requested_);
}

int32_t SoundTriggerEngineGsl::setECRef(Stream *s, std::shared_ptr<Device> dev, bool is_enable,
                                        bool setECForFirstTime) {
    int32_t status = 0;
    bool force_enable = false;
    bool is_dev_enabled_ext_ec = false;

    if (!session_) {
        PAL_ERR(LOG_TAG, "Invalid session");
        return -EINVAL;
    }
    PAL_DBG(LOG_TAG, "Enter, EC ref count : %d, enable : %d", ec_ref_count_, is_enable);
    PAL_DBG(LOG_TAG, "Rx device : %s, stream is setting EC for first time : %d",
            dev ? dev->getPALDeviceName().c_str() :  "Null", setECForFirstTime);

    std::shared_ptr<ResourceManager> rm = ResourceManager::getInstance();
    if (!rm) {
        PAL_ERR(LOG_TAG, "Failed to get resource manager instance");
        return -EINVAL;
    }

    if (dev)
        is_dev_enabled_ext_ec = rm->isExternalECRefEnabled(dev->getSndDeviceId());
    std::unique_lock<std::mutex> lck(ec_ref_mutex_);
    if (is_enable) {
        if (is_crr_dev_using_ext_ec_ && !is_dev_enabled_ext_ec) {
            PAL_ERR(LOG_TAG, "Internal EC connot be set, when external EC is active");
            return -EINVAL;
        }
        if (setECForFirstTime) {
            ec_ref_count_++;
        } else if (rx_ec_dev_!=dev ){
            force_enable = true;
        } else {
            return status;
        }
        if (force_enable || ec_ref_count_ == 1) {
            status = session_->setECRef(s, dev, is_enable);
            if (status) {
                PAL_ERR(LOG_TAG, "Failed to set EC Ref for rx device %s",
                        dev ?  dev->getPALDeviceName().c_str() : "Null");
                if (setECForFirstTime) {
                    ec_ref_count_--;
                }
                if (force_enable || ec_ref_count_ == 0) {
                    rx_ec_dev_ = nullptr;
                }
            } else {
                is_crr_dev_using_ext_ec_ = is_dev_enabled_ext_ec;
                rx_ec_dev_ = dev;
            }
        }
    } else {
        if (!dev || dev == rx_ec_dev_) {
            if (ec_ref_count_ > 0) {
                ec_ref_count_--;
                if (ec_ref_count_ == 0) {
                    status = session_->setECRef(s, dev, is_enable);
                    if (status) {
                        PAL_ERR(LOG_TAG, "Failed to reset EC Ref");
                    } else {
                        rx_ec_dev_ = nullptr;
                        is_crr_dev_using_ext_ec_ = false;
                    }
                }
            } else {
                PAL_DBG(LOG_TAG, "Skipping EC disable, as ref count is 0");
            }
        } else {
            PAL_DBG(LOG_TAG, "Skipping EC disable, as EC disable is not for correct device");
        }
    }
    PAL_DBG(LOG_TAG, "Exit, EC ref count : %d", ec_ref_count_);

    return status;
}

int32_t SoundTriggerEngineGsl::UpdateSessionPayload(st_param_id_type_t param) {
    int32_t status = 0;
    uint32_t tag_id = 0;
    uint32_t param_id = 0;
    uint8_t *payload = nullptr;
    size_t payload_size = 0;
    uint32_t ses_param_id = 0;
    uint32_t detection_miid = 0;

    PAL_DBG(LOG_TAG, "Enter, param : %u", param);

    if (param < LOAD_SOUND_MODEL || param >= MAX_PARAM_IDS) {
        PAL_ERR(LOG_TAG, "Invalid param id %u", param);
        return -EINVAL;
    }

    tag_id = module_tag_ids_[param];
    param_id = param_ids_[param];
    if (!tag_id || !param_id) {
        PAL_ERR(LOG_TAG, "Invalid tag/param id %u", param);
        return -EINVAL;
    }

    if (use_lpi_) {
        if (lpi_miid_ == 0)
            status = session_->getMIID(nullptr, tag_id, &lpi_miid_);
        detection_miid = lpi_miid_;
    } else {
        if (nlpi_miid_ == 0)
            status = session_->getMIID(nullptr, tag_id, &nlpi_miid_);
        detection_miid = nlpi_miid_;
    }

    if (status != 0) {
        PAL_ERR(LOG_TAG, "Failed to get instance id for tag %x, status = %d",
            tag_id, status);
        return status;
    }

    switch(param) {
        case LOAD_SOUND_MODEL:
        {
            ses_param_id = PAL_PARAM_ID_LOAD_SOUND_MODEL;
            if (!IS_MODULE_TYPE_PDK(module_type_)) {
                /* Set payload data and size from engine's sound model info */
                status = builder_->payloadSVAConfig(&payload, &payload_size,
                    eng_sm_info_->GetModelData(), eng_sm_info_->GetModelSize(),
                    detection_miid, param_id);

            } else {
                status = builder_->payloadSVAConfig(&payload, &payload_size,
                         sm_data_, sm_data_size_, detection_miid, param_id);
            }
            break;
        }
        case UNLOAD_SOUND_MODEL:
        {
            ses_param_id = PAL_PARAM_ID_UNLOAD_SOUND_MODEL;
            if (!IS_MODULE_TYPE_PDK(module_type_)) {
                status = builder_->payloadSVAConfig(&payload, &payload_size,
                nullptr, 0, detection_miid, param_id);
            } else {
                status = builder_->payloadSVAConfig(&payload, &payload_size,
                  (uint8_t *) &deregister_config_, sizeof(
                   struct param_id_detection_engine_deregister_multi_sound_model_t),
                   detection_miid, param_id);
            }
            break;
        }
        case WAKEUP_CONFIG:
        {
            ses_param_id = PAL_PARAM_ID_WAKEUP_ENGINE_CONFIG;
            if (!IS_MODULE_TYPE_PDK(module_type_)) {
                size_t fixed_wakeup_payload_size =
                    sizeof(struct detection_engine_config_voice_wakeup) -
                    PAL_SOUND_TRIGGER_MAX_USERS * 2;
                size_t wakeup_payload_size = fixed_wakeup_payload_size +
                    wakeup_config_.num_active_models * 2;
                uint8_t *wakeup_payload = new uint8_t[wakeup_payload_size];
                ar_mem_cpy(wakeup_payload, fixed_wakeup_payload_size,
                    &wakeup_config_, fixed_wakeup_payload_size);
                uint8_t *confidence_level = wakeup_payload +
                    fixed_wakeup_payload_size;
                uint8_t *kw_user_enable = wakeup_payload +
                    fixed_wakeup_payload_size +
                    wakeup_config_.num_active_models;
                for (int i = 0; i < wakeup_config_.num_active_models; i++) {
                    confidence_level[i] = wakeup_config_.confidence_levels[i];
                    kw_user_enable[i] = wakeup_config_.keyword_user_enables[i];
                    PAL_VERBOSE(LOG_TAG,
                        "confidence_level[%d] = %d KW_User_enable[%d] = %d",
                        i, confidence_level[i], i, kw_user_enable[i]);
                }
                status = builder_->payloadSVAConfig(&payload, &payload_size,
                    (uint8_t *)wakeup_payload, wakeup_payload_size,
                    detection_miid, param_id);
                delete[] wakeup_payload;
            } else {
                size_t fixedConfigVoiceWakeupSize = sizeof(
                                     struct detection_engine_config_stage1_pdk)
                                     - (MAX_KEYWORD_SUPPORTED * sizeof(uint32_t));
                size_t payloadSize = fixedConfigVoiceWakeupSize +
                                     (pdk_wakeup_config_.num_keywords *
                                      sizeof(uint32_t));
                uint8_t *wakeup_payload = new uint8_t[payloadSize];
                if (!wakeup_payload){
                    PAL_ERR(LOG_TAG, "payload malloc failed %s", strerror(errno));
                    return -EINVAL;
                }
                ar_mem_cpy(wakeup_payload, fixedConfigVoiceWakeupSize,
                           &pdk_wakeup_config_, fixedConfigVoiceWakeupSize);
                uint32_t *confidence_level = (uint32_t *)(wakeup_payload +
                                              fixedConfigVoiceWakeupSize);

                for (int i = 0; i < pdk_wakeup_config_.num_keywords; ++i){
                    confidence_level[i] = pdk_wakeup_config_.confidence_levels[i];
                }
                status = builder_->payloadSVAConfig(&payload, &payload_size,
                          (uint8_t *)wakeup_payload, payloadSize, detection_miid, param_id);
            }
            break;
        }
        case BUFFERING_CONFIG :
        {
            ses_param_id = PAL_PARAM_ID_WAKEUP_BUFFERING_CONFIG;
            if (!IS_MODULE_TYPE_PDK(module_type_)) {
                status = builder_->payloadSVAConfig(&payload, &payload_size,
                       (uint8_t *)(&buffer_config_.hist_buffer_duration_in_ms),
                       sizeof(struct detection_engine_multi_model_buffering_config)
                       - sizeof(uint32_t), detection_miid, param_id);
            } else {
                status = builder_->payloadSVAConfig(&payload, &payload_size,
                        (uint8_t *)&buffer_config_,
                        sizeof(struct detection_engine_multi_model_buffering_config),
                        detection_miid, param_id);

            }
            break;
        }
        case ENGINE_RESET:
            ses_param_id = PAL_PARAM_ID_WAKEUP_ENGINE_RESET;
            status = builder_->payloadSVAConfig(&payload, &payload_size,
                nullptr, 0, detection_miid, param_id);
            break;
        case CUSTOM_CONFIG:
            ses_param_id = PAL_PARAM_ID_WAKEUP_CUSTOM_CONFIG;
            status = builder_->payloadSVAConfig(&payload, &payload_size,
                custom_data, custom_data_size, detection_miid, param_id);
            // release local custom data
            if (custom_data) {
                free(custom_data);
                custom_data = nullptr;
                custom_data_size = 0;
            }
            break;
        default:
            PAL_ERR(LOG_TAG, "Invalid param id %u", param);
            return -EINVAL;
    }

    if (status || !payload) {
        PAL_ERR(LOG_TAG, "Failed to construct SVA payload, status = %d",
            status);
        return -ENOMEM;
    }

    status = session_->setParameters(stream_handle_, tag_id, ses_param_id, payload);
    if (status != 0) {
        PAL_ERR(LOG_TAG, "Failed to set payload for param id %x, status = %d",
            ses_param_id, status);
    }

    return status;
}

void SoundTriggerEngineGsl::UpdateStateToActive() {
    UpdateState(ENG_ACTIVE);
}

std::shared_ptr<SoundTriggerEngineGsl> SoundTriggerEngineGsl::GetInstance(
    Stream *s,
    listen_model_indicator_enum type,
    st_module_type_t module_type,
    std::shared_ptr<VUIStreamConfig> sm_cfg) {

    std::shared_ptr<SoundTriggerEngineGsl> st_eng;
    st_module_type_t key = module_type;
    if (IS_MODULE_TYPE_PDK(module_type)) {
        key = ST_MODULE_TYPE_PDK;
    }
    eng_create_mutex_.lock();
    if (eng_.find(key) == eng_.end() ||
        (key != ST_MODULE_TYPE_GMM &&
         engine_count_ < sm_cfg->GetSupportedEngineCount())) {
        st_eng = std::make_shared<SoundTriggerEngineGsl>
                                    (s, type, module_type, sm_cfg);
        eng_[key].push_back(st_eng);
        engine_count_++;
    } else {
        st_eng = eng_[key][eng_[key].size() - 1];
    }
    str_eng_map_[s] = st_eng;
    eng_create_mutex_.unlock();
    return st_eng;
}

void SoundTriggerEngineGsl::DetachStream(Stream *s, bool erase_engine) {
    st_module_type_t key;

    std::unique_lock<std::mutex> lck(mutex_);

    if (s) {
        auto iter = std::find(eng_streams_.begin(), eng_streams_.end(), s);
        if (iter != eng_streams_.end())
            eng_streams_.erase(iter);
    }
    if (!eng_streams_.size() && erase_engine) {
        key = this->module_type_;
        if (IS_MODULE_TYPE_PDK(this->module_type_)) {
            key = ST_MODULE_TYPE_PDK;
        }

        eng_create_mutex_.lock();
        auto to_erase = std::find(eng_[key].begin(), eng_[key].end(),
                                  str_eng_map_[s]);
        if (to_erase != eng_[key].end()) {
            eng_[key].erase(to_erase);
            if (key == ST_MODULE_TYPE_PDK)
                engine_count_--;
        }

        if (!eng_[key].size()) {
            eng_.erase(key);
        }
        str_eng_map_.erase(s);
        eng_create_mutex_.unlock();
    }
}
