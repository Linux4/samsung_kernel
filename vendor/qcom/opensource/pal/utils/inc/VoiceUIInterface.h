/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef VOICEUI_INTERFACE_H
#define VOICEUI_INTERFACE_H

#include "PalDefs.h"
#include "SoundTriggerUtils.h"
#include "VoiceUIPlatformInfo.h"
#include "SoundTriggerPlatformInfo.h"
#include "Stream.h"
#include "SoundTriggerEngine.h"

typedef std::pair<listen_model_indicator_enum, std::pair<void *, uint32_t>> sm_pair_t;
typedef std::map<Stream *, struct sound_model_info *> sound_model_info_map_t;
typedef std::vector<std::pair<listen_model_indicator_enum, uint32_t>> sec_stage_level_t;

struct sound_model_info {
    pal_st_sound_model_type_t type;
    uint32_t recognition_mode;
    uint32_t model_id;
    void *sm_data;
    uint32_t sm_size;
    struct pal_st_sound_model *model;
    struct pal_st_recognition_config *rec_config;
    void *wakeup_config;
    uint32_t wakeup_config_size;
    uint32_t hist_buffer_duration;
    uint32_t pre_roll_duration;
    sec_stage_level_t sec_threshold;
    sec_stage_level_t sec_det_level;
    SoundModelInfo *info;
    bool is_hist_buffer_duration_set;
};

class VoiceUIInterface {
  public:

    virtual ~VoiceUIInterface() {}

    /*
     * @brief create VoiceUI Interface object
     * @caller StreamSoundTrigger
     *
     * @param[in] sm_cfg Sound Model config for current model
     *
     * @return shared pointer of VoiceUI Interface object
     */
    static std::shared_ptr<VoiceUIInterface> Create(std::shared_ptr<VUIStreamConfig> sm_cfg);

    /*
     * @brief parse sound model from client
     * @caller StreamSoundTrigger
     *
     * @param[in]   sm_cfg            sound model config
     * @param[in]   sound_model       sound model payload sent by client
     * @param[out]  first_stage_type  first stage module type
     * @param[out]  model_list        list of blobs for each stage
     *
     * @return 0 Sound model parsed successfuly
     * @return -EINVAL sm_data provided is inproper
     * @return -ENOMEM no memory available to create blobs of each stage
     */
    static int32_t ParseSoundModel(std::shared_ptr<VUIStreamConfig> sm_cfg,
                                   struct pal_st_sound_model *sound_model,
                                   st_module_type_t &first_stage_type,
                                   std::vector<sm_pair_t> &model_list);

    /*
     * @brief parse recognition config from client
     * @caller StreamSoundTrigger
     *
     * @param[in]  s       stream pointer of caller
     * @param[in]  config  recognition config sent by client
     *
     * @return 0 Recognition config parsed successfully
     * @return -EINVAL Recognition config is invalid
     */
    virtual int32_t ParseRecognitionConfig(Stream *s,
                                           struct pal_st_recognition_config *config) = 0;

    /*
     * @brief Acquire wakeup configs parsed by ParseRecognitionConfig
     * @caller StreamSoundTrigger
     *
     * @param[in]   s       stream pointer of caller
     * @param[out]  config  payload for wake up configs
     * @param[out]  size    payload size
     */
    virtual void GetWakeupConfigs(Stream *s, void **config, uint32_t *size) = 0;

    /*
     * @brief Acquire buffering duration parsed by ParseRecognitionConfig
     *
     * @param[in]   s                 stream pointer of caller
     * @param[out]  hist_duration     history buffer duration
     * @param[out]  preroll_duration  preroll duration
     */
    virtual void GetBufferingConfigs(Stream *s,
                                     uint32_t *hist_duration,
                                     uint32_t *preroll_duration) = 0;

    /*
     * @brief Get second stage threshold parsed by ParseRecognitionConfig
     * @caller StreamSoundTrigger
     *
     * @param[in]   s      stream pointer of caller
     * @param[in]   type   second stage type(KWD/UV)
     * @param[out]  level  threshold
     */
    virtual void GetSecondStageConfLevels(Stream *s,
                                          listen_model_indicator_enum type,
                                          uint32_t *level) = 0;

    /*
     * @brief Set second stage detection level
     * @caller SoundTriggerEngineCapi
     *
     * @param[in]  s      stream pointer of caller
     * @param[in]  type   second stage type(KWD/UV)
     * @param[in]  level  threshold
     */
    virtual void SetSecondStageDetLevels(Stream *s,
                                         listen_model_indicator_enum type,
                                         uint32_t level) = 0;

    /*
     * @brief Parse detection event sent by ADSP
     * @caller SoundTriggerEngineGsl
     *
     * @param[in]  event  detection event payload
     * @param[in]  size   detection event payload size
     *
     * @return 0 detection event parsed successfully
     * @return -EINVAL invalid detection event
     */
    virtual int32_t ParseDetectionPayload(void *event, uint32_t size) = 0;

    /*
     * @brief Get stream handle for current detection
     * @caller SoundTriggerEngineGsl
     */
    virtual Stream* GetDetectedStream() = 0;

    virtual void* GetDetectionEventInfo() = 0;

    /*
     * @brief Generate call back event after detection
     * @caller StreamSoundTrigger
     *
     * @param[out]  event       generated callback event
     * @param[out]  event_size  callback event size
     * @param[out]  detection   flag indicating detection success or failure
     *
     * @return 0 callback event generated successfully
     * @return -ENOMEM no memory available for callback event
     */
    virtual int32_t GenerateCallbackEvent(Stream *s,
                                          struct pal_st_recognition_event **event,
                                          uint32_t *event_size, bool detection) = 0;

    /*
     * @brief Post processing for lab data
     * @caller SoundTriggerEngineGsl
     * @note used for customization, e.g. gain adjust
     *
     * @param[in]  data  pcm data acquried from ADSP
     * @param[in]  size  pcm data size for post-processing
     */
    virtual void ProcessLab(void *data, uint32_t size) = 0;

    /*
     * @brief Cache FTRT data for further use
     * @caller SoundTriggerEngineGsl
     * @note used for customization
     *
     * @param[in]  data  ftrt data
     * @param[in]  size  ftrt data size
     */
    virtual void UpdateFTRTData(void *data, uint32_t size) = 0;

    /*
     * @brief check if qc wakeup config is used
     * @caller SoundTriggerEngineGsl
     * @note used for 3rd party path with QC wakeup config
     */
    virtual bool IsQCWakeUpConfigUsed() = 0;

    /*
     * @brief get history/preroll duration with max value
     * @note for multiple stream case
     */
    void GetBufDuration(uint32_t *hist_duration,
                        uint32_t *preroll_duration) {
        *hist_duration = hist_duration_;
        *preroll_duration = preroll_duration_;
    }

    /*
     * @brief get model id
     *
     * @param[in]  s  stream pointer
     */
    uint32_t GetModelId(Stream *s);

    /*
     * @brief get model type
     *
     * @param[in]  s  stream pointer
     */
    st_module_type_t GetModuleType(Stream *s) { return module_type_; }

    /*
     * @brief get sound model info
     * @note used for SVA4 only
     * @param[in]  s  stream pointer
     */
    SoundModelInfo* GetSoundModelInfo(Stream *s);

    /*
     * @brief set sound model info
     * @note used for SVA4 only
     * @param[in]  info  sound model info
     */
    void SetSoundModelInfo(SoundModelInfo *info) { sound_model_info_ = info; }

    /*
     * @brief set sound model id
     *
     * @param[in]  s         stream pointer
     * @param[in]  model_id  model id to be updated
     */
    void SetModelId(Stream *s, uint32_t model_id);

    void SetSTModuleType(st_module_type_t model_type) {
        module_type_ = model_type;
    }

    void SetRecognitionMode(Stream *s, uint32_t mode);

    uint32_t GetRecognitionMode(Stream *s);

    /*
     * @brief register stream/model to interface
     *
     * @param[in]  s        stream pointer
     * @param[in]  model    cached sound model for stream s
     * @param[in]  sm_data  first stage model acquired from ParseSoundModel
     * @param[in]  sm_size  first stage model size
     *
     * @return  0        stream/model registered successfully
     * @return  -ENOMEM  no memory for allocation
     */
    int32_t RegisterModel(Stream *s,
                          struct pal_st_sound_model *model,
                          void *sm_data, uint32_t sm_size);

    /*
     * @brief deregister stream/model to interface
     *
     * @param[in]  s        stream pointer
     */
    void DeregisterModel(Stream *s);

    /*
     * @brief Get keyword start/end index for current detection
     * @caller SoundTriggerEngineGsl
     *
     * @param[out]  start_index  keyword start index
     * @param[out]  end_index    keyword end index
     */
    void GetKeywordIndex(uint32_t *start_index, uint32_t *end_index);

    /*
     * @brief Get size for ftrt data
     * @caller SoundTriggerEngineGsl
     */
    uint32_t GetFTRTDataSize() { return ftrt_size_; }

    /*
     * @brief Get lab read offset
     * @caller StreamSoundTrigger
     */
    uint32_t GetReadOffset() { return read_offset_; }

    /*
     * @brief Set lab read offset
     * @caller StreamSoundTrigger
     */
    void SetReadOffset(uint32_t offset) { read_offset_ = offset; }

    /*
     * @brief transfer duration(us) to bytes based on sm config
     *
     * @param[in]  input_us  duration in us
     *
     * @return  bytes corresponding to the duration
     */
    uint32_t UsToBytes(uint64_t input_us);

  protected:
    uint32_t hist_duration_;
    uint32_t preroll_duration_;
    st_module_type_t module_type_;
    sound_model_info_map_t sm_info_map_;
    std::shared_ptr<VUIStreamConfig> sm_cfg_;

    SoundModelInfo *sound_model_info_;

    uint32_t start_index_ = 0;
    uint32_t end_index_ = 0;
    uint32_t ftrt_size_ = 0;
    uint32_t read_offset_ = 0;
};

#endif

