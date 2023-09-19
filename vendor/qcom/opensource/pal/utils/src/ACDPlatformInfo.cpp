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

#include "ACDPlatformInfo.h"

#include <errno.h>

#include <string>
#include <map>

#include "PalCommon.h"
#include "PalDefs.h"
#include "kvh2xml.h"

#define LOG_TAG "PAL: ACDPlatformInfo"

static const std::map<std::string, int32_t> acdContextTypeMap {
    {std::string{"ACD_SOUND_MODEL_ID_ENV"}, ACD_SOUND_MODEL_ID_ENV},
    {std::string{"ACD_SOUND_MODEL_ID_EVENT"}, ACD_SOUND_MODEL_ID_EVENT},
    {std::string{"ACD_SOUND_MODEL_ID_SPEECH"}, ACD_SOUND_MODEL_ID_SPEECH},
    {std::string{"ACD_SOUND_MODEL_ID_MUSIC"}, ACD_SOUND_MODEL_ID_MUSIC},
    {std::string{"ACD_SOUND_MODEL_AMBIENCE_NOISE_SILENCE"}, ACD_SOUND_MODEL_AMBIENCE_NOISE_SILENCE},
};

ACDContextInfo::ACDContextInfo(uint32_t context_id, uint32_t type) :
    context_id_(context_id),
    context_type_(type)
{
}

uint32_t ACDContextInfo::GetContextId() {
    return context_id_;
}

uint32_t ACDContextInfo::GetContextType() {
    return context_type_;
}

ACDSoundModelInfo::ACDSoundModelInfo(StreamConfig *sm_cfg) :
    model_bin_name_(""),
    is_parsing_contexts(false),
    sm_cfg_(sm_cfg)
{
}

void ACDSoundModelInfo::HandleStartTag(const char* tag, const char** attribs __unused) {

    if (is_parsing_contexts) {
        if (!strcmp(tag, "context")) {
            uint32_t i = 0;
            uint32_t id;
            while (attribs[i]) {
                if (!strcmp(attribs[i], "id")) {
                    std::string tagid(attribs[++i]);
                    id = ResourceManager::convertCharToHex(tagid);
                    std::shared_ptr<ACDContextInfo> context_info =
                        std::make_shared<ACDContextInfo>(id, model_id_);
                    acd_context_info_list_.push_back(context_info);
                    sm_cfg_->UpdateContextModelMap(id);
                }
                ++i; /* move to next attribute */
            }
        }
    }

    if (!strcmp(tag, "contexts"))
        is_parsing_contexts = true;
}

void ACDSoundModelInfo::HandleEndTag(struct xml_userdata *data, const char* tag_name) {
    PAL_DBG(LOG_TAG, "Got end tag %s", tag_name);

    if (!strcmp(tag_name, "contexts"))
        is_parsing_contexts = false;

    if (data->offs <= 0)
        return;

    data->data_buf[data->offs] = '\0';

    if (!strcmp(tag_name, "name")) {
        std::string type(data->data_buf);
        model_type_ = type;
        auto valItr = acdContextTypeMap.find(data->data_buf);
        if (valItr == acdContextTypeMap.end()) {
            PAL_ERR(LOG_TAG, "Error:%d could not find value %s in lookup table",
                        -EINVAL, data->data_buf);
        } else {
            model_id_ = valItr->second;
        }
    } else if (!strcmp(tag_name, "bin")) {
        std::string bin_name(data->data_buf);
        model_bin_name_ = bin_name;
    } else if (!strcmp(tag_name, "uuid")) {
        std::string uuid(data->data_buf);
        model_uuid_ = ResourceManager::convertCharToHex(uuid);
    }

    return;
}

std::string ACDSoundModelInfo::GetModelType() {
    return model_type_;
}

std::string ACDSoundModelInfo::GetModelBinName() {
    return model_bin_name_;
}

uint32_t ACDSoundModelInfo::GetModelUUID() {
    return model_uuid_;
}

uint32_t ACDSoundModelInfo::GetModelId() {
    return model_id_;
}

std::vector<std::shared_ptr<ACDContextInfo>> ACDSoundModelInfo::GetSupportedContextList() {
    return acd_context_info_list_;
}

size_t ACDSoundModelInfo::GetNumContexts() {
    return acd_context_info_list_.size();
}

StreamConfig::StreamConfig(const acd_cap_profile_map_t& cap_prof_map) :
    sample_rate_(16000),
    bit_width_(16),
    out_channels_(1),
    cap_profile_map_(cap_prof_map),
    curr_child_(nullptr)
{
}

std::pair<uint32_t, uint32_t> StreamConfig::GetStreamMetadata() {
    return stream_metadata_;
}

ACDUUID StreamConfig::GetUUID() {
    return vendor_uuid_;
}

uint32_t StreamConfig::GetSampleRate() {
    return sample_rate_;
}

uint32_t StreamConfig::GetBitWidth() {
    return bit_width_;
}

uint32_t StreamConfig::GetOutChannels() {
    return out_channels_;
}

std::string StreamConfig::GetStreamConfigName() {
    return name_;
}

std::shared_ptr<CaptureProfile> StreamConfig::GetCaptureProfile(
        std::pair<StOperatingModes, StInputModes> mode_pair) {
    return op_modes_.at(mode_pair);
}

std::vector<std::shared_ptr<ACDSoundModelInfo>> StreamConfig::GetSoundModelList() {
    return acd_soundmodel_info_list_;
}

void StreamConfig::ReadCapProfileNames(StOperatingModes mode,
    const char** attribs) {
    uint32_t i = 0;
    while (attribs[i]) {
        if (!strcmp(attribs[i], "capture_profile_handset")) {
            op_modes_[std::make_pair(mode, ST_INPUT_MODE_HANDSET)] =
                cap_profile_map_.at(std::string(attribs[++i]));
        } else if(!strcmp(attribs[i], "capture_profile_headset")) {
            op_modes_[std::make_pair(mode, ST_INPUT_MODE_HEADSET)] =
                cap_profile_map_.at(std::string(attribs[++i]));
        } else {
            PAL_ERR(LOG_TAG, "Error:%d got unexpected attribute: %s", -EINVAL, attribs[i]);
        }
        ++i; /* move to next attribute */
    }
}

void StreamConfig::UpdateContextModelMap(uint32_t context_id) {
     std::shared_ptr<ACDSoundModelInfo> sm_info(
            std::static_pointer_cast<ACDSoundModelInfo>(curr_child_));
     context_model_map_.insert(std::make_pair(context_id, sm_info));
}

std::shared_ptr<ACDSoundModelInfo> StreamConfig::GetSoundModelInfoByContextId(uint32_t context_id) {
    auto contextModel = context_model_map_.find(context_id);
    if (contextModel != context_model_map_.end())
        return contextModel->second;
    else
        return nullptr;
}

std::shared_ptr<ACDSoundModelInfo> StreamConfig::GetSoundModelInfoByModelId(uint32_t model_id) {
    auto modelInfo = acd_modelinfo_map_.find(model_id);
    if (modelInfo != acd_modelinfo_map_.end())
        return modelInfo->second;
    else
        return nullptr;
}

void StreamConfig::HandleStartTag(const char* tag, const char** attribs) {
    PAL_DBG(LOG_TAG, "Got tag %s", tag);

    /* Delegate to child element if currently active */
    if (curr_child_) {
        curr_child_->HandleStartTag(tag, attribs);
        return;
    }

    if (!strcmp(tag, "model")) {
        curr_child_ = std::static_pointer_cast<SoundTriggerXml>(
            std::make_shared<ACDSoundModelInfo>(this));
        return;
    }

    if (!strcmp(tag, "param")) {
        uint32_t i = 0;
        while (attribs[i]) {
            if (!strcmp(attribs[i], "vendor_uuid")) {
                SoundTriggerUUID::StringToUUID(attribs[++i],
                    vendor_uuid_);
            } else if (!strcmp(attribs[i], "sample_rate")) {
                sample_rate_ = std::stoi(attribs[++i]);
            } else if (!strcmp(attribs[i], "bit_width")) {
                bit_width_ = std::stoi(attribs[++i]);
            } else if (!strcmp(attribs[i], "out_channels")) {
                out_channels_ = std::stoi(attribs[++i]);
            } else {
                PAL_INFO(LOG_TAG, "Invalid attribute %s", attribs[i++]);
            }
            ++i; /* move to next attribute */
        }
    } else if (!strcmp(tag, "kvpair")) {
        uint32_t i = 0;
        uint32_t key = 0, value = 0;
        while (attribs[i]) {
            if (!strcmp(attribs[i], "key")) {
                std::string tagkey(attribs[++i]);
                key = ResourceManager::convertCharToHex(tagkey);
            } else if(!strcmp(attribs[i], "value")) {
                std::string tagvalue(attribs[++i]);
                value = ResourceManager::convertCharToHex(tagvalue);
                stream_metadata_ = std::make_pair(key, value);
                PAL_DBG(LOG_TAG, "stream_metadata_, key = %x, value = %x",
                     stream_metadata_.first, stream_metadata_.second);
            }
            ++i; /* move to next attribute */
        }
    }  else if (!strcmp(tag, "low_power")) {
        ReadCapProfileNames(ST_OPERATING_MODE_LOW_POWER, attribs);
    } else if (!strcmp(tag, "low_power_ns")) {
        ReadCapProfileNames(ST_OPERATING_MODE_LOW_POWER_NS, attribs);
    } else if (!strcmp(tag, "high_performance")) {
        ReadCapProfileNames(ST_OPERATING_MODE_HIGH_PERF, attribs);
    } else if (!strcmp(tag, "high_performance_ns")) {
        ReadCapProfileNames(ST_OPERATING_MODE_HIGH_PERF_NS, attribs);
    } else if (!strcmp(tag, "high_performance_and_charging")) {
        ReadCapProfileNames(ST_OPERATING_MODE_HIGH_PERF_AND_CHARGING, attribs);
    } else {
          PAL_INFO(LOG_TAG, "Invalid tag %s", (char *)tag);
    }
}

void StreamConfig::HandleEndTag(struct xml_userdata *data, const char* tag) {
    PAL_DBG(LOG_TAG, "Got end tag %s", tag);

    if (!strcmp(tag, "model")) {
       std::shared_ptr<ACDSoundModelInfo> acd_sm_info(
            std::static_pointer_cast<ACDSoundModelInfo>(curr_child_));
        acd_soundmodel_info_list_.push_back(acd_sm_info);
        acd_modelinfo_map_.insert(std::make_pair(acd_sm_info->GetModelId(), acd_sm_info));
        curr_child_ = nullptr;
    }

    if (curr_child_) {
        curr_child_->HandleEndTag(data, tag);
        return;
    }

    if (!strcmp(tag, "name")) {
        if (data->offs <= 0)
            return;
        data->data_buf[data->offs] = '\0';

        std::string name(data->data_buf);
        name_ = name;
    }
    return;
}

std::shared_ptr<ACDPlatformInfo> ACDPlatformInfo::me_ =
    nullptr;

ACDPlatformInfo::ACDPlatformInfo() :
    acd_enable_(true),
    support_device_switch_(false),
    support_nlpi_switch_(true),
    dedicated_sva_path_(true),
    dedicated_headset_path_(false),
    lpi_enable_(true),
    enable_debug_dumps_(false),
    concurrent_capture_(false),
    concurrent_voice_call_(false),
    concurrent_voip_call_(false),
    low_latency_bargein_enable_(false),
    curr_child_(nullptr)
{
}

bool ACDPlatformInfo::GetSupportDevSwitch() const {
    return support_device_switch_;
}

bool ACDPlatformInfo::GetSupportNLPISwitch() const {
    return support_nlpi_switch_;
}

bool ACDPlatformInfo::GetDedicatedSvaPath() const {
    return dedicated_sva_path_;
}

bool ACDPlatformInfo::GetDedicatedHeadsetPath() const {
    return dedicated_headset_path_;
}

bool ACDPlatformInfo::GetLpiEnable() const {
    return lpi_enable_;
}

bool ACDPlatformInfo::GetEnableDebugDumps() const {
    return enable_debug_dumps_;
}

bool ACDPlatformInfo::GetConcurrentCaptureEnable() const {
    return concurrent_capture_;
}

bool ACDPlatformInfo::GetConcurrentVoiceCallEnable() const {
    return concurrent_voice_call_;
}

bool ACDPlatformInfo::GetConcurrentVoipCallEnable() const {
    return concurrent_voip_call_;
}

bool ACDPlatformInfo::GetLowLatencyBargeinEnable() const {
    return low_latency_bargein_enable_;
}

bool ACDPlatformInfo::IsACDEnabled() const {
    return acd_enable_;
}

std::shared_ptr<StreamConfig> ACDPlatformInfo::GetStreamConfig(
    const ACDUUID& uuid) const {
    auto streamCfg = acd_cfg_list_.find(uuid);
    if (streamCfg != acd_cfg_list_.end())
        return streamCfg->second;
    else
        return nullptr;
}

std::shared_ptr<CaptureProfile> ACDPlatformInfo::GetCapProfile(
    const std::string& name) const {
    auto capProfile = capture_profile_map_.find(name);
    if (capProfile != capture_profile_map_.end())
        return capProfile->second;
    else
        return nullptr;
}

std::shared_ptr<ACDPlatformInfo>
ACDPlatformInfo::GetInstance() {

    if (!me_)
        me_ = std::shared_ptr<ACDPlatformInfo>
            (new ACDPlatformInfo);

    return me_;
}

void ACDPlatformInfo::HandleStartTag(const char* tag,
                                              const char** attribs) {
    /* Delegate to child element if currently active */
    if (curr_child_) {
        curr_child_->HandleStartTag(tag, attribs);
        return;
    }

    PAL_DBG(LOG_TAG, "Got start tag %s", tag);
    if (!strcmp(tag, "stream_config")) {
        curr_child_ = std::static_pointer_cast<SoundTriggerXml>(
            std::make_shared<StreamConfig>(capture_profile_map_));
        return;
    }

    if (!strcmp(tag, "capture_profile")) {
        if (attribs[0] && !strcmp(attribs[0], "name")) {
            curr_child_ = std::static_pointer_cast<SoundTriggerXml>(
                    std::make_shared<CaptureProfile>(attribs[1]));
            return;
        } else {
            PAL_ERR(LOG_TAG, "Error:%d missing name attrib for tag %s", -EINVAL, tag);
            return;
        }
    }

    if (!strcmp(tag, "common_config") || !strcmp(tag, "capture_profile_list")) {
        PAL_INFO(LOG_TAG, "tag:%s appeared, nothing to do", tag);
        return;
    }

    if (!strcmp(tag, "param")) {
        uint32_t i = 0;
        while (attribs[i]) {
            if (!attribs[i]) {
                PAL_ERR(LOG_TAG, "Error:%d missing attrib value for tag %s", -EINVAL, tag);
            } else if (!strcmp(attribs[i], "acd_enable")) {
                acd_enable_ =
                    !strncasecmp(attribs[++i], "true", 4) ? true : false;
            } else if (!strcmp(attribs[i], "support_device_switch")) {
                support_device_switch_ =
                    !strncasecmp(attribs[++i], "true", 4) ? true : false;
            } else if (!strcmp(attribs[i], "support_nlpi_switch")) {
                support_nlpi_switch_ =
                    !strncasecmp(attribs[++i], "true", 4) ? true : false;
            } else if (!strcmp(attribs[i], "lpi_enable")) {
                lpi_enable_ =
                    !strncasecmp(attribs[++i], "true", 4) ? true : false;
            } else if (!strcmp(attribs[i], "enable_debug_dumps")) {
                enable_debug_dumps_ =
                    !strncasecmp(attribs[++i], "true", 4) ? true : false;
            } else if (!strcmp(attribs[i], "concurrent_capture")) {
                concurrent_capture_ =
                    !strncasecmp(attribs[++i], "true", 4) ? true : false;
            } else if (!strcmp(attribs[i], "concurrent_voice_call") &&
                       concurrent_capture_) {
                concurrent_voice_call_ =
                    !strncasecmp(attribs[++i], "true", 4) ? true : false;
            } else if (!strcmp(attribs[i], "concurrent_voip_call") &&
                       concurrent_capture_) {
                concurrent_voip_call_ =
                    !strncasecmp(attribs[++i], "true", 4) ? true : false;
            } else if (!strcmp(attribs[i], "low_latency_bargein_enable") &&
                       concurrent_capture_) {
                low_latency_bargein_enable_ =
                    !strncasecmp(attribs[++i], "true", 4) ? true : false;
            } else {
                PAL_INFO(LOG_TAG, "Invalid attribute %s", attribs[i++]);
            }
            ++i; /* move to next attribute */
        }
    } else {
        PAL_INFO(LOG_TAG, "Invalid tag %s", tag);
    }
}

void ACDPlatformInfo::HandleEndTag(struct xml_userdata *data, const char* tag) {
    PAL_DBG(LOG_TAG, "Got end tag %s", tag);

    if (!strcmp(tag, "stream_config")) {
        std::shared_ptr<StreamConfig> acd_cfg(
            std::static_pointer_cast<StreamConfig>(curr_child_));
        const auto res = acd_cfg_list_.insert(
            std::make_pair(acd_cfg->GetUUID(), acd_cfg));
        if (!res.second)
            PAL_ERR(LOG_TAG, "Error:%d Failed to insert to map", -EINVAL);
        curr_child_ = nullptr;
    } else if (!strcmp(tag, "capture_profile")) {
        std::shared_ptr<CaptureProfile> cap_prof(
            std::static_pointer_cast<CaptureProfile>(curr_child_));
        const auto res = capture_profile_map_.insert(
            std::make_pair(cap_prof->GetName(), cap_prof));
        if (!res.second)
            PAL_ERR(LOG_TAG, "Error:%d Failed to insert to map", -EINVAL);
        curr_child_ = nullptr;
    }


    if (curr_child_)
        curr_child_->HandleEndTag(data, tag);

    return;
}
