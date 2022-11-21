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


#ifndef SOUNDTRIGGERENGINECAPI_H
#define SOUNDTRIGGERENGINECAPI_H

#include "capi_v2.h"
#include "capi_v2_extn.h"

#include "SoundTriggerEngine.h"
#include "PalRingBuffer.h"

class Stream;
class SecondStageConfig;

class SoundTriggerEngineCapi : public SoundTriggerEngine {
 public:
    SoundTriggerEngineCapi(Stream *s,
        listen_model_indicator_enum type,
        std::shared_ptr<SoundModelConfig> sm_cfg);
    ~SoundTriggerEngineCapi();
    int32_t LoadSoundModel(Stream *s, uint8_t *data,
                           uint32_t data_size) override;
    int32_t UnloadSoundModel(Stream *s) override;
    int32_t StartRecognition(Stream *s) override;
    int32_t RestartRecognition(Stream *s __unused) override;
    int32_t StopRecognition(Stream *s) override;
    int32_t UpdateConfLevels(
        Stream *s,
        struct pal_st_recognition_config *config,
        uint8_t *conf_levels,
        uint32_t num_conf_levels) override;
    void SetDetected(bool detected) override;

    int32_t GetParameters(uint32_t param_id __unused, void **payload __unused) {
        return 0;
    }
    int32_t ConnectSessionDevice(
        Stream* stream_handle __unused,
        pal_stream_type_t stream_type __unused,
        std::shared_ptr<Device> device_to_connect __unused) { return 0; }
    int32_t DisconnectSessionDevice(
        Stream* stream_handle __unused,
        pal_stream_type_t stream_type __unused,
        std::shared_ptr<Device> device_to_disconnect __unused) { return 0; }
    int32_t SetupSessionDevice(
       Stream* stream_handle __unused,
       pal_stream_type_t stream_type __unused,
       std::shared_ptr<Device> device_to_disconnect __unused) { return 0; }
    int32_t UpdateBufConfig(Stream *s __unused,
                            uint32_t hist_buffer_duration __unused,
                            uint32_t pre_roll_duration __unused) {
        return 0;
    }
    void GetUpdatedBufConfig(uint32_t *hist_buffer_duration __unused,
                            uint32_t *pre_roll_duration __unused) {}
    void SetCaptureRequested(bool is_requested __unused) {}
    void* GetDetectionEventInfo() { return nullptr; }
    int32_t setECRef(
        Stream *s __unused,
        std::shared_ptr<Device> dev __unused,
        bool is_enable __unused) { return 0; }
    int32_t GetDetectedConfScore() { return det_conf_score_; }
    int32_t GetDetectionState() { return detection_state_; }
    ChronoSteadyClock_t GetDetectedTime() {
        return std::chrono::steady_clock::time_point::min();
    }

 private:
    int32_t StartSoundEngine();
    int32_t StopSoundEngine();
    int32_t StartKeywordDetection();
    int32_t StartUserVerification();
    static void BufferThreadLoop(SoundTriggerEngineCapi *capi_engine);

    std::string lib_name_;
    capi_v2_t *capi_handle_;
    void* capi_lib_handle_;
    capi_v2_init_f capi_init_;

    std::mutex event_mutex_;
    st_sound_model_type_t detection_type_;
    bool processing_started_;
    bool keyword_detected_;
    int32_t confidence_threshold_;
    uint32_t buffer_size_;
    std::shared_ptr<SecondStageConfig> ss_cfg_;
    /*
     * externally to allow engine to know where
     * it can stop and start processing
     */
    uint32_t buffer_start_;
    uint32_t buffer_end_;
    uint64_t kw_start_timestamp_;  // input from 1st stage
    uint64_t kw_end_timestamp_;
    uint32_t bytes_processed_;
    uint32_t confidence_score_;  // used by Vop engine
    uint32_t kw_start_tolerance_;
    uint32_t kw_end_tolerance_;
    uint32_t data_before_kw_start_;
    uint32_t data_after_kw_end_;
    int32_t det_conf_score_;
    int32_t detection_state_;
    stage2_uv_wrapper_scratch_param_t in_model_buffer_param_;
    stage2_uv_wrapper_scratch_param_t scratch_param_;
};
#endif  // SOUNDTRIGGERENGINECAPI_H

