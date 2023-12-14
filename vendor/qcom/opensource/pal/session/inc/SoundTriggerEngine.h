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


#ifndef SOUNDTRIGGERENGINE_H
#define SOUNDTRIGGERENGINE_H

#include <condition_variable>
#include <thread>
#include <mutex>
#include <vector>
#include <chrono>

#include "PalDefs.h"
#include "PalCommon.h"
#include "PalRingBuffer.h"
#include "Device.h"
#include "SoundTriggerUtils.h"

using ChronoSteadyClock_t = std::chrono::time_point<std::chrono::steady_clock>;

#define BITS_PER_BYTE 8
#define US_PER_SEC 1000000
#define MS_PER_SEC 1000
#define CNN_BUFFER_LENGTH 10000
#define CNN_FRAME_SIZE 320
#define CUSTOM_CONFIG_OPAQUE_DATA_SIZE 12
#define CONF_LEVELS_INTF_VERSION_0002 0x02
#define MAX_MODELS_SUPPORTED 8

class Stream;

struct model_stats
{
    uint32_t detected_model_id;
    uint32_t detected_keyword_id;
    uint32_t best_channel_idx;
    uint32_t best_confidence_level;
    uint32_t kw_start_timestamp_lsw;
    uint32_t kw_start_timestamp_msw;
    uint32_t kw_end_timestamp_lsw;
    uint32_t kw_end_timestamp_msw;
    uint32_t detection_timestamp_lsw;
    uint32_t detection_timestamp_msw;
};

struct detection_event_info_pdk
{
    uint32_t num_detected_models;
    struct model_stats detected_model_stats[MAX_MODELS_SUPPORTED];
    uint32_t ftrt_data_length_in_us;
};

struct detection_event_info
{
    uint16_t status;
    uint16_t num_confidence_levels;
    uint8_t confidence_levels[20];
    uint32_t kw_start_timestamp_lsw;
    uint32_t kw_start_timestamp_msw;
    uint32_t kw_end_timestamp_lsw;
    uint32_t kw_end_timestamp_msw;
    uint32_t detection_timestamp_lsw;
    uint32_t detection_timestamp_msw;
    uint32_t ftrt_data_length_in_us;
};

class SoundModelConfig;
class SoundTriggerPlatformInfo;

class SoundTriggerEngine {
public:
    static std::shared_ptr<SoundTriggerEngine> Create(Stream *s,
        listen_model_indicator_enum type, st_module_type_t module_type,
        std::shared_ptr<SoundModelConfig> sm_cfg);

    virtual ~SoundTriggerEngine() {}

    virtual int32_t LoadSoundModel(Stream *s, uint8_t *data,
                                   uint32_t data_size) = 0;
    virtual int32_t UnloadSoundModel(Stream *s) = 0;
    virtual int32_t StartRecognition(Stream *s) = 0;
    virtual int32_t RestartRecognition(Stream *s) = 0;
    virtual int32_t StopRecognition(Stream *s) = 0;
    virtual int32_t UpdateConfLevels(
        Stream *s,
        struct pal_st_recognition_config *config,
        uint8_t *conf_levels,
        uint32_t num_conf_levels) = 0;
    virtual int32_t UpdateBufConfig(Stream *s, uint32_t hist_buffer_duration,
                          uint32_t pre_roll_duration) = 0;
    virtual void GetUpdatedBufConfig(uint32_t *hist_buffer_duration,
                                    uint32_t *pre_roll_duration) = 0;
    virtual void SetDetected(bool detected) = 0;
    virtual int32_t GetParameters(uint32_t param_id, void **payload) = 0;
    virtual int32_t ConnectSessionDevice(
        Stream* stream_handle,
        pal_stream_type_t stream_type,
        std::shared_ptr<Device> device_to_connect) = 0;
    virtual int32_t DisconnectSessionDevice(
        Stream* stream_handle,
        pal_stream_type_t stream_type,
        std::shared_ptr<Device> device_to_disconnect) = 0;
    virtual int32_t SetupSessionDevice(
        Stream* streamHandle,
        pal_stream_type_t streamType,
        std::shared_ptr<Device> deviceToConnect) = 0;
    virtual void DetachStream(Stream *s, bool erase_engine) {}
    virtual void SetCaptureRequested(bool is_requested) = 0;
    virtual void* GetDetectionEventInfo() = 0;
    virtual int32_t ReconfigureDetectionGraph(Stream *s) { return 0; }
    virtual int32_t setECRef(
        Stream *s,
        std::shared_ptr<Device> dev,
        bool is_enable) = 0;
    virtual int32_t GetCustomDetectionEvent(uint8_t **event __unused,
        size_t *size __unused) { return 0; }
    virtual int32_t GetDetectedConfScore() = 0;
    virtual int32_t GetDetectionState() = 0;
    virtual ChronoSteadyClock_t GetDetectedTime() = 0;

    int32_t CreateBuffer(uint32_t buffer_size, uint32_t engine_size,
        std::vector<PalRingBufferReader *> &reader_list);
    int32_t SetBufferReader(PalRingBufferReader *reader);
    int32_t ResetBufferReaders(std::vector<PalRingBufferReader *> &reader_list);
    uint32_t UsToBytes(uint64_t input_us);
    uint32_t FrameToBytes(uint32_t frames);
    uint32_t BytesToFrames(uint32_t bytes);
    listen_model_indicator_enum GetEngineType() { return engine_type_; }

protected:
    listen_model_indicator_enum engine_type_;
    std::shared_ptr<SoundTriggerPlatformInfo> st_info_;
    std::shared_ptr<SoundModelConfig> sm_cfg_;
    uint8_t *sm_data_;
    uint32_t sm_data_size_;
    bool capture_requested_;
    Stream *stream_handle_;
    PalRingBuffer *buffer_;
    PalRingBufferReader *reader_;
    uint32_t sample_rate_;
    uint32_t bit_width_;
    uint32_t channels_;

    std::thread buffer_thread_handler_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool exit_thread_;
    bool exit_buffering_;
};

#endif  // SOUNDTRIGGERENGINE_H
