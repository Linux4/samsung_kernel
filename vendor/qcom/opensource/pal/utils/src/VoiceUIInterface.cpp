/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "PAL: VoiceUIInterface"

#include "VoiceUIInterface.h"
#include "SVAInterface.h"
#include "HotwordInterface.h"
#include "CustomVAInterface.h"

std::shared_ptr<VoiceUIInterface> VoiceUIInterface::Create(
    std::shared_ptr<VUIStreamConfig> sm_cfg) {

    std::shared_ptr<VoiceUIInterface> interface = nullptr;

    if (!sm_cfg) {
        PAL_ERR(LOG_TAG, "Invalid VUI stream config");
        return nullptr;
    }

    PAL_DBG(LOG_TAG, "Create interface with VUI module type %d",
        sm_cfg->GetVUIModuleType());

    switch (sm_cfg->GetVUIModuleType()) {
        case ST_MODULE_TYPE_GMM:
        case ST_MODULE_TYPE_PDK:
            interface = std::make_shared<SVAInterface>(sm_cfg);
            break;
        case ST_MODULE_TYPE_HW:
            interface = std::make_shared<HotwordInterface>(sm_cfg);
            break;
        case ST_MODULE_TYPE_CUSTOM_1:
        case ST_MODULE_TYPE_CUSTOM_2:
            interface = std::make_shared<CustomVAInterface>(sm_cfg);
            break;
        default:
            PAL_ERR(LOG_TAG, "Invalid VUI module type");
            break;
    }

    return interface;
}

int32_t VoiceUIInterface::ParseSoundModel(
    std::shared_ptr<VUIStreamConfig> sm_cfg,
    struct pal_st_sound_model *sound_model,
    st_module_type_t &first_stage_type,
    std::vector<sm_pair_t> &model_list) {

    uint32_t status = 0;

    switch (sm_cfg->GetVUIModuleType()) {
        case ST_MODULE_TYPE_GMM:
        case ST_MODULE_TYPE_PDK:
            status = SVAInterface::ParseSoundModel(sm_cfg,
                                                   sound_model,
                                                   first_stage_type,
                                                   model_list);
            break;
        case ST_MODULE_TYPE_HW:
            status = HotwordInterface::ParseSoundModel(sm_cfg,
                                                       sound_model,
                                                       first_stage_type,
                                                       model_list);
            break;
        case ST_MODULE_TYPE_CUSTOM_1:
        case ST_MODULE_TYPE_CUSTOM_2:
            status = CustomVAInterface::ParseSoundModel(sm_cfg,
                                                        sound_model,
                                                        first_stage_type,
                                                        model_list);
            break;
        default:
            PAL_ERR(LOG_TAG, "Invalid VUI module type %d",
                sm_cfg->GetVUIModuleType());
            status = -EINVAL;
            break;
    }

    return status;
}

uint32_t VoiceUIInterface::GetModelId(Stream *s) {
    if (sm_info_map_.find(s) != sm_info_map_.end() && sm_info_map_[s]) {
        return sm_info_map_[s]->model_id;
    } else {
        return 0;
    }
}

SoundModelInfo* VoiceUIInterface::GetSoundModelInfo(Stream *s) {
    if (sm_info_map_.find(s) != sm_info_map_.end() && sm_info_map_[s]) {
        return sm_info_map_[s]->info;
    } else {
        return nullptr;
    }
}

void VoiceUIInterface::SetModelId(Stream *s, uint32_t model_id) {
    if (sm_info_map_.find(s) != sm_info_map_.end() && sm_info_map_[s]) {
        sm_info_map_[s]->model_id = model_id;
    }
}

void VoiceUIInterface::SetRecognitionMode(Stream *s, uint32_t mode) {
    if (sm_info_map_.find(s) != sm_info_map_.end() && sm_info_map_[s]) {
        sm_info_map_[s]->recognition_mode = mode;
    }
}

uint32_t VoiceUIInterface::GetRecognitionMode(Stream *s) {
    if (sm_info_map_.find(s) != sm_info_map_.end() && sm_info_map_[s]) {
        return sm_info_map_[s]->recognition_mode;
    } else {
        return 0;
    }
}

int32_t VoiceUIInterface::RegisterModel(Stream *s,
                                        struct pal_st_sound_model *model,
                                        void *sm_data,
                                        uint32_t sm_size) {

    int32_t status = 0;

    if (sm_info_map_.find(s) == sm_info_map_.end()) {
        sm_info_map_[s] = (struct sound_model_info *)calloc(1,
            sizeof(struct sound_model_info));
        if (!sm_info_map_[s]) {
            PAL_ERR(LOG_TAG, "Failed to allocate memory for sm data");
            status = -ENOMEM;
            goto exit;
        }
    }

    sm_info_map_[s]->info = new SoundModelInfo();
    if (!sm_info_map_[s]->info) {
        PAL_ERR(LOG_TAG, "Failed to allocate memory for SoundModelInfo");
        status = -ENOMEM;
        free(sm_info_map_[s]);
        goto exit;
    }

    sm_info_map_[s]->sm_data = sm_data;
    sm_info_map_[s]->sm_size = sm_size;
    sm_info_map_[s]->model = model;
    sm_info_map_[s]->type = model->type;

exit:
    return status;
}

void VoiceUIInterface::DeregisterModel(Stream *s) {
    auto iter = sm_info_map_.find(s);
    if (iter != sm_info_map_.end() && sm_info_map_[s]) {
        if (sm_info_map_[s]->wakeup_config)
            free(sm_info_map_[s]->wakeup_config);
        if (sm_info_map_[s]->info)
            delete(sm_info_map_[s]->info);
        sm_info_map_[s]->sec_threshold.clear();
        sm_info_map_[s]->sec_det_level.clear();
        free(sm_info_map_[s]);
        sm_info_map_.erase(iter);
    } else {
        PAL_DBG(LOG_TAG, "Cannot deregister unregistered model")
    }
}

void VoiceUIInterface::GetKeywordIndex(uint32_t *start_index,
                                   uint32_t *end_index) {

    *start_index = start_index_;
    *end_index = end_index_;
}

uint32_t VoiceUIInterface::UsToBytes(uint64_t input_us) {
    uint32_t bytes = 0;

    bytes = sm_cfg_->GetSampleRate() *
            sm_cfg_->GetBitWidth() *
            sm_cfg_->GetOutChannels() * input_us /
            (BITS_PER_BYTE * US_PER_SEC);

    return bytes;
}
