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


#ifndef SOUNDTRIGGERENGINEGSL_H
#define SOUNDTRIGGERENGINEGSL_H

#include <map>

#include "SoundTriggerEngine.h"
#include "SoundTriggerUtils.h"
#include "StreamSoundTrigger.h"
#include "PalRingBuffer.h"
#include "PayloadBuilder.h"
#include "detection_cmn_api.h"

#define MAX_MODEL_ID_VALUE 0xFFFFFFFE
#define MIN_MODEL_ID_VALUE 1
#define CONFIDENCE_LEVEL_INFO    0x1
#define KEYWORD_INDICES_INFO     0x2
#define TIME_STAMP_INFO          0x4
#define FTRT_INFO                0x8
#define MULTI_MODEL_RESULT       0x20

typedef enum {
    ENG_IDLE,
    ENG_LOADED,
    ENG_ACTIVE,
    ENG_BUFFERING,
    ENG_DETECTED,
} eng_state_t;

class Session;
class Stream;

class SoundTriggerEngineGsl : public SoundTriggerEngine {
 public:
    SoundTriggerEngineGsl(Stream *s,
               listen_model_indicator_enum type,
               st_module_type_t module_type,
               std::shared_ptr<SoundModelConfig> sm_cfg);
    ~SoundTriggerEngineGsl();
    static std::shared_ptr<SoundTriggerEngineGsl> GetInstance(Stream *s,
                          listen_model_indicator_enum type,
                          st_module_type_t module_type,
                          std::shared_ptr<SoundModelConfig> sm_cfg);
    void DetachStream(Stream *s, bool erase_engine) override;
    int32_t LoadSoundModel(Stream *s, uint8_t *data,
                           uint32_t data_size) override;
    int32_t UnloadSoundModel(Stream *s) override;
    int32_t StartRecognition(Stream *s) override;
    int32_t RestartRecognition(Stream *s) override;
    int32_t StopRecognition(Stream *s) override;
    int32_t UpdateConfLevels(
        Stream *s __unused,
        struct pal_st_recognition_config *config,
        uint8_t *conf_levels,
        uint32_t num_conf_levels) override;
    int32_t UpdateBufConfig(Stream *s, uint32_t hist_buffer_duration,
                            uint32_t pre_roll_duration) override;
    void GetUpdatedBufConfig(uint32_t *hist_buffer_duration,
                            uint32_t *pre_roll_duration) override;
     void SetDetected(bool detected __unused) {};
    int32_t GetParameters(uint32_t param_id, void **payload) override;
    int32_t ConnectSessionDevice(
        Stream* stream_handle,
        pal_stream_type_t stream_type,
        std::shared_ptr<Device> device_to_connect) override;
    int32_t DisconnectSessionDevice(
        Stream* stream_handle,
        pal_stream_type_t stream_type,
        std::shared_ptr<Device> device_to_disconnect) override;
    int32_t SetupSessionDevice(
        Stream* stream_handle,
        pal_stream_type_t stream_type,
        std::shared_ptr<Device> device_to_disconnect) override;
    void SetCaptureRequested(bool is_requested) override;
    int32_t ReconfigureDetectionGraph(Stream *s) override;
    void* GetDetectionEventInfo() override;
    int32_t setECRef(
        Stream *s,
        std::shared_ptr<Device> dev,
        bool is_enable) override;
    int32_t GetCustomDetectionEvent(uint8_t **event, size_t *size) override;
    int32_t GetDetectedConfScore() { return 0; }
    int32_t GetDetectionState() { return 0; }
    ChronoSteadyClock_t GetDetectedTime() {
        return detection_time_;
    }
    void UpdateState(eng_state_t state);

 private:
    int32_t StartBuffering(Stream *s);
    int32_t UpdateSessionPayload(st_param_id_type_t param);
    int32_t ParseDetectionPayloadPDK(void *event_data);
    int32_t ParseDetectionPayload(void *event_data);
    void UpdateKeywordIndex(uint64_t kwd_start_timestamp,
        uint64_t kwd_end_timestamp, uint64_t ftrt_start_timestamp);
    void HandleSessionEvent(uint32_t event_id __unused, void *data, uint32_t size);
    void ResetEngine();

    static void EventProcessingThread(SoundTriggerEngineGsl *gsl_engine);
    static void HandleSessionCallBack(uint64_t hdl, uint32_t event_id, void *data,
                                      uint32_t event_size);
    bool CheckIfOtherStreamsAttached(Stream *s);
    bool CheckIfOtherStreamsActive(Stream *s);
    int32_t HandleMultiStreamLoad(Stream *s, uint8_t *data, uint32_t data_size);
    int32_t HandleMultiStreamUnloadPDK(Stream *s);
    int32_t HandleMultiStreamUnload(Stream *s);
    int32_t UpdateEngineModel(Stream *s, uint8_t *data,
                              uint32_t data_size, bool add);
    int32_t AddSoundModel(Stream *s, uint8_t *data, uint32_t data_size);
    int32_t DeleteSoundModel(Stream *s);
    int32_t QuerySoundModel(SoundModelInfo *sm_info,
                            uint8_t *data, uint32_t data_size);
    int32_t MergeSoundModels(uint32_t num_models, listen_model_type *in_models[],
             listen_model_type *out_model);
    int32_t DeleteFromMergedModel(char **keyphrases, uint32_t num_keyphrases,
             listen_model_type *in_model, listen_model_type *out_model);
    int32_t ConstructAPMPayload(uint32_t param_id, uint8_t** payload,
                                uint8_t* data, uint32_t data_size);
    int32_t ProcessStartRecognition(Stream *s);
    int32_t ProcessStopRecognition(Stream *s);
    int32_t UpdateMergeConfLevelsWithActiveStreams();
    int32_t UpdateMergeConfLevelsPayload(SoundModelInfo* src_sm_info,
                               bool set);
    int32_t UpdateEngineConfigOnStop(Stream *s);
    int32_t UpdateEngineConfigOnRestart(Stream *s);
    int32_t UpdateConfigPDK(uint32_t model_id);
    int32_t UpdateConfigs();
    Stream* GetDetectedStream(uint32_t model_id = 0);
    void CheckAndSetDetectionConfLevels(Stream *s);
    bool IsEngineActive();
    Session *session_;
    PayloadBuilder *builder_;
    std::map<uint32_t, Stream*> mid_stream_map_;
    std::map<uint32_t, std::pair<uint32_t, uint32_t>> mid_buff_cfg_;
    st_module_type_t module_type_;
    static std::map<st_module_type_t,std::shared_ptr<SoundTriggerEngineGsl>>
                                                                      eng_;
    std::map<uint32_t, struct detection_engine_config_stage1_pdk> mid_wakeup_cfg_;
    std::vector<Stream *> eng_streams_;
    std::vector<uint32_t> updated_cfg_;
    SoundModelInfo *eng_sm_info_;
    bool sm_merged_;
    int32_t dev_disconnect_count_;
    eng_state_t eng_state_;
    struct detection_engine_config_voice_wakeup wakeup_config_;
    struct detection_engine_config_stage1_pdk pdk_wakeup_config_;
    struct detection_engine_multi_model_buffering_config buffer_config_;
    struct detection_event_info detection_event_info_;
    struct detection_event_info_pdk detection_event_info_multi_model_ ;
    struct param_id_detection_engine_deregister_multi_sound_model_t
                                                     deregister_config_;

    bool is_qcva_uuid_;
    bool is_qcmd_uuid_;
    uint32_t lpi_miid_;
    uint32_t nlpi_miid_;
    bool use_lpi_;
    uint32_t module_tag_ids_[MAX_PARAM_IDS];
    uint32_t param_ids_[MAX_PARAM_IDS];
    uint8_t *custom_data;
    size_t custom_data_size;
    uint8_t *custom_detection_event;
    size_t custom_detection_event_size;
    struct pal_mmap_buffer mmap_buffer_;
    size_t mmap_buffer_size_;
    uint32_t mmap_write_position_;
    uint64_t kw_transfer_latency_;
    ChronoSteadyClock_t detection_time_;
    std::mutex state_mutex_;
};
#endif  // SOUNDTRIGGERENGINEGSL_H
