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
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "PAL: HotwordInterface"

#include "HotwordInterface.h"

HotwordInterface::HotwordInterface(std::shared_ptr<VUIStreamConfig> sm_cfg) {
    sm_cfg_ = sm_cfg;
    hist_duration_ = 0;
    preroll_duration_ = 0;
    custom_event_ = nullptr;
    custom_event_size_ = 0;
}

HotwordInterface::~HotwordInterface() {
    if (custom_event_)
        free(custom_event_);
}

int32_t HotwordInterface::ParseSoundModel(std::shared_ptr<VUIStreamConfig> sm_cfg,
                                          struct pal_st_sound_model *sound_model,
                                          st_module_type_t &first_stage_type,
                                          std::vector<sm_pair_t> &model_list) {

    int32_t status = 0;
    struct pal_st_phrase_sound_model *phrase_sm = nullptr;
    struct pal_st_sound_model *common_sm = nullptr;
    uint8_t *ptr = nullptr;
    uint8_t *sm_data = nullptr;
    int32_t sm_size = 0;

    PAL_DBG(LOG_TAG, "Enter");

    if (sound_model->type == PAL_SOUND_MODEL_TYPE_KEYPHRASE) {
        // handle for phrase sound model
        phrase_sm = (struct pal_st_phrase_sound_model *)sound_model;
        first_stage_type = sm_cfg->GetVUIModuleType();
        sm_size = phrase_sm->common.data_size;
        sm_data = (uint8_t *)calloc(1, sm_size);
        if (!sm_data) {
            PAL_ERR(LOG_TAG, "Failed to allocate memory for sm_data");
            status = -ENOMEM;
            goto error_exit;
        }
        ptr = (uint8_t*)phrase_sm + phrase_sm->common.data_offset;
        ar_mem_cpy(sm_data, sm_size, ptr, sm_size);
        model_list.push_back(std::make_pair(ST_SM_ID_SVA_F_STAGE_GMM,
                                            std::make_pair(sm_data, sm_size)));
    } else if (sound_model->type == PAL_SOUND_MODEL_TYPE_GENERIC) {
        // handle for generic sound model
        first_stage_type = sm_cfg->GetVUIModuleType();
        common_sm = sound_model;
        sm_size = common_sm->data_size;
        sm_data = (uint8_t *)calloc(1, sm_size);
        if (!sm_data) {
            PAL_ERR(LOG_TAG, "Failed to allocate memory for sm_data");
            status = -ENOMEM;
            goto error_exit;
        }
        ptr = (uint8_t*)common_sm + common_sm->data_offset;
        ar_mem_cpy(sm_data, sm_size, ptr, sm_size);
        model_list.push_back(std::make_pair(ST_SM_ID_SVA_F_STAGE_GMM,
                                            std::make_pair(sm_data, sm_size)));
    }
    PAL_DBG(LOG_TAG, "Exit, status %d", status);
    return status;

error_exit:
    // clean up memory added to model_list in failure case
    for (auto iter = model_list.begin(); iter != model_list.end(); iter++) {
        if ((*iter).second.first)
            free((*iter).second.first);
    }
    model_list.clear();

    PAL_DBG(LOG_TAG, "Exit, status %d", status);
    return status;
}

int32_t HotwordInterface::ParseRecognitionConfig(Stream *s,
    struct pal_st_recognition_config *config) {

    int32_t status = 0;
    struct sound_model_info *sm_info = nullptr;
    uint32_t hist_buffer_duration = 0;
    uint32_t pre_roll_duration = 0;

    if (sm_info_map_.find(s) != sm_info_map_.end() && sm_info_map_[s]) {
        sm_info = sm_info_map_[s];
        sm_info->rec_config = config;
    } else {
        PAL_ERR(LOG_TAG, "Stream not registered to interface");
        return -EINVAL;
    }

    PAL_DBG(LOG_TAG, "Enter");
    if (!config) {
        PAL_ERR(LOG_TAG, "Invalid config");
        status = -EINVAL;
        goto exit;
    }

    // get history buffer duration from sound trigger platform xml
    hist_buffer_duration = sm_cfg_->GetKwDuration();
    pre_roll_duration = 0;

    sm_info_map_[s]->hist_buffer_duration = hist_buffer_duration;
    sm_info_map_[s]->pre_roll_duration = pre_roll_duration;

exit:
    PAL_DBG(LOG_TAG, "Exit, status %d", status);
    return status;
}

void HotwordInterface::GetBufferingConfigs(Stream *s,
                                           uint32_t *hist_duration,
                                           uint32_t *preroll_duration) {

    if (sm_info_map_.find(s) != sm_info_map_.end() && sm_info_map_[s]) {
        *hist_duration = sm_info_map_[s]->hist_buffer_duration;
        *preroll_duration = sm_info_map_[s]->pre_roll_duration;
    } else {
        PAL_ERR(LOG_TAG, "Stream not registered to interface");
    }
}

int32_t HotwordInterface::ParseDetectionPayload(void *event, uint32_t size) {
    int32_t status = 0;

    if (!event || size == 0) {
        PAL_ERR(LOG_TAG, "Invalid detection payload");
        return -EINVAL;
    }

    custom_event_ = (uint8_t *)realloc(custom_event_, size);
    if (!custom_event_) {
        PAL_ERR(LOG_TAG, "Failed to allocate memory for detection payload");
        return -ENOMEM;
    }

    ar_mem_cpy(custom_event_, size, event, size);
    custom_event_size_ = size;

    return status;
}

Stream* HotwordInterface::GetDetectedStream() {
    PAL_DBG(LOG_TAG, "Enter");
    if (sm_info_map_.empty()) {
        PAL_ERR(LOG_TAG, "Unexpected, No streams attached to engine!");
        return nullptr;
    } else if (sm_info_map_.size() == 1) {
        return sm_info_map_.begin()->first;
    } else {
        PAL_ERR(LOG_TAG, "Only single hotword usecase is supported");
        return nullptr;
    }
}

int32_t HotwordInterface::GenerateCallbackEvent(Stream *s,
                                                struct pal_st_recognition_event **event,
                                                uint32_t *size, bool detection) {

    struct sound_model_info *sm_info = nullptr;
    struct pal_st_phrase_recognition_event *phrase_event = nullptr;
    struct pal_st_generic_recognition_event *generic_event = nullptr;
    struct pal_stream_attributes strAttr;
    size_t event_size = 0;
    uint8_t *opaque_data = nullptr;
    int32_t status = 0;

    if (sm_info_map_.find(s) != sm_info_map_.end() && sm_info_map_[s]) {
        sm_info = sm_info_map_[s];
    } else {
        PAL_ERR(LOG_TAG, "Stream not registered to interface");
        return -EINVAL;
    }

    status = s->getStreamAttributes(&strAttr);

    PAL_DBG(LOG_TAG, "Enter");
    *event = nullptr;
    if (sm_info->type == PAL_SOUND_MODEL_TYPE_KEYPHRASE) {
        event_size = sizeof(struct pal_st_phrase_recognition_event) +
                     custom_event_size_;
        // event allocated will be used/deallocated in stream side
        phrase_event = (struct pal_st_phrase_recognition_event *)
                       calloc(1, event_size);
        if (!phrase_event) {
            PAL_ERR(LOG_TAG, "Failed to alloc memory for recognition event");
            status =  -ENOMEM;
            goto exit;
        }

        phrase_event->num_phrases = sm_info->rec_config->num_phrases;
        memcpy(phrase_event->phrase_extras, sm_info->rec_config->phrases,
               phrase_event->num_phrases *
               sizeof(struct pal_st_phrase_recognition_extra));
        *event = &(phrase_event->common);
        (*event)->status = detection ? PAL_RECOGNITION_STATUS_SUCCESS :
                           PAL_RECOGNITION_STATUS_FAILURE;
        (*event)->type = sm_info->type;
        (*event)->st_handle = (pal_st_handle_t *)this;
        (*event)->capture_available = sm_info->rec_config->capture_requested;
        // TODO: generate capture session
        (*event)->capture_session = 0;
        (*event)->capture_delay_ms = 0;
        (*event)->capture_preamble_ms = 0;
        (*event)->trigger_in_data = true;
        (*event)->data_size = custom_event_size_;
        (*event)->data_offset = sizeof(struct pal_st_phrase_recognition_event);
        (*event)->media_config.sample_rate =
            strAttr.in_media_config.sample_rate;
        (*event)->media_config.bit_width =
            strAttr.in_media_config.bit_width;
        (*event)->media_config.ch_info.channels =
            strAttr.in_media_config.ch_info.channels;
        (*event)->media_config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE;

        // Filling Opaque data
        opaque_data = (uint8_t *)phrase_event +
                       phrase_event->common.data_offset;
        ar_mem_cpy(opaque_data, custom_event_size_, custom_event_, custom_event_size_);
    } else if (sm_info->type == PAL_SOUND_MODEL_TYPE_GENERIC) {
        event_size = sizeof(struct pal_st_generic_recognition_event) +
                     custom_event_size_;
        // event allocated will be used/deallocated in stream side
        generic_event = (struct pal_st_generic_recognition_event *)
                       calloc(1, event_size);
        if (!generic_event) {
            PAL_ERR(LOG_TAG, "Failed to alloc memory for recognition event");
            status =  -ENOMEM;
            goto exit;

        }

        *event = &(generic_event->common);
        (*event)->status = PAL_RECOGNITION_STATUS_SUCCESS;
        (*event)->type = sm_info->type;
        (*event)->st_handle = (pal_st_handle_t *)this;
        (*event)->capture_available = sm_info->rec_config->capture_requested;
        // TODO: generate capture session
        (*event)->capture_session = 0;
        (*event)->capture_delay_ms = 0;
        (*event)->capture_preamble_ms = 0;
        (*event)->trigger_in_data = true;
        (*event)->data_size = custom_event_size_;
        (*event)->data_offset = sizeof(struct pal_st_generic_recognition_event);
        (*event)->media_config.sample_rate =
            strAttr.in_media_config.sample_rate;
        (*event)->media_config.bit_width =
            strAttr.in_media_config.bit_width;
        (*event)->media_config.ch_info.channels =
            strAttr.in_media_config.ch_info.channels;
        (*event)->media_config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE;

        // Filling Opaque data
        opaque_data = (uint8_t *)generic_event +
                       generic_event->common.data_offset;
        ar_mem_cpy(opaque_data, custom_event_size_, custom_event_, custom_event_size_);
    }
    *size = event_size;
exit:
    PAL_DBG(LOG_TAG, "Exit");
    return status;
}

