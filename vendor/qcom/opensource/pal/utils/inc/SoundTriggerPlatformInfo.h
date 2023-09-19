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

#ifndef SOUND_TRIGGER_PLATFORM_INFO_H
#define SOUND_TRIGGER_PLATFORM_INFO_H

#include <stdint.h>
#include <map>
#include <vector>
#include <memory>
#include <string>
#include "PalDefs.h"
#include "SoundTriggerUtils.h"
#include "SoundTriggerXmlParser.h"

#define IS_MODULE_TYPE_PDK(type) (type == ST_MODULE_TYPE_PDK5 || type == ST_MODULE_TYPE_PDK6)
#define MAX_MODULE_CHANNELS 4

using UUID = SoundTriggerUUID;

using st_cap_profile_map_t =
    std::map<std::string, std::shared_ptr<CaptureProfile>>;

class SecondStageConfig : public SoundTriggerXml {
 public:
    SecondStageConfig();

    void HandleStartTag(const char *tag, const char **attribs) override;

    st_sound_model_type_t GetDetectionType() const { return detection_type_; }
    uint32_t GetSoundModelID() const { return sm_id_; }
    std::string GetLibName() const { return module_lib_; }
    uint32_t GetSampleRate() const { return sample_rate_; }
    uint32_t GetBitWidth() const { return bit_width_; }
    uint32_t GetChannels() const { return channels_; }

 private:
    st_sound_model_type_t detection_type_;
    uint32_t sm_id_;
    std::string module_lib_;
    uint32_t sample_rate_;
    uint32_t bit_width_;
    uint32_t channels_;
};

class SoundTriggerModuleInfo : public SoundTriggerXml {
 public:
    SoundTriggerModuleInfo();

    void HandleStartTag(const char *tag, const char **attribs) override;

    st_module_type_t GetModuleType() const { return module_type_; }
    std::string GetModuleName() const { return module_name_; }
    uint32_t GetModuleTagId(st_param_id_type_t param_id) const {
        return module_tag_ids_[param_id];
    }
    uint32_t GetParamId(st_param_id_type_t param_id) const {
        return param_ids_[param_id];
    }
    std::pair<uint32_t, uint32_t> getStreamConfigKV() {
        return stream_config_;
    }

 private:
    st_module_type_t module_type_;
    std::string module_name_;
    uint32_t module_tag_ids_[MAX_PARAM_IDS];
    uint32_t param_ids_[MAX_PARAM_IDS];
    std::pair<uint32_t, uint32_t> stream_config_;
};

class SoundModelConfig : public SoundTriggerXml {
 public:
    /*
     * constructor takes a reference to map of capture profiles as it has to
     * look-up the capture profiles for this sound-model
     */
    SoundModelConfig(const st_cap_profile_map_t&);
    SoundModelConfig(SoundModelConfig &rhs) = delete;
    SoundModelConfig & operator=(SoundModelConfig &rhs) = delete;

    SoundTriggerUUID GetUUID() const { return vendor_uuid_; }
    bool GetModuleVersionSupported() const {
        return get_module_version_supported_; }
    bool GetMergeFirstStageSoundModels() const {
        return merge_first_stage_sound_models_;
    }
    bool isQCVAUUID() const { return is_qcva_uuid_; }
    bool isQCMDUUID() const { return is_qcmd_uuid_; }
    uint32_t GetSampleRate() const { return sample_rate_; }
    uint32_t GetBitWidth() const { return bit_width_; }
    uint32_t GetOutChannels() const { return out_channels_; }
    void SetOutChannels(uint32_t channels) {
        if (channels <= MAX_MODULE_CHANNELS)
            out_channels_ = channels;
    }
    uint32_t GetKwDuration() const { return capture_keyword_; }
    uint32_t GetCaptureReadDelay() const { return client_capture_read_delay_; }
    uint32_t GetKwStartTolerance() const { return kw_start_tolerance_; }
    uint32_t GetKwEndTolerance() const { return kw_end_tolerance_; }
    uint32_t GetDataBeforeKwStart() const { return data_before_kw_start_; }
    uint32_t GetDataAfterKwEnd() const { return data_after_kw_end_; }
    st_module_type_t GetModuleType();
    std::shared_ptr<CaptureProfile> GetCaptureProfile(
        std::pair<StOperatingModes, StInputModes> mode_pair) const {
        return op_modes_.at(mode_pair);
    }
    std::shared_ptr<SecondStageConfig> GetSecondStageConfig(
        const listen_model_indicator_enum& sm_type) const;
    std::shared_ptr<SoundTriggerModuleInfo> GetSoundTriggerModuleInfo(
        const uint32_t type) const;
    std::shared_ptr<SoundTriggerModuleInfo> GetSoundTriggerModuleInfo();
    std::string GetModuleName();
    std::string GetModuleName(st_module_type_t type);
    void HandleStartTag(const char *tag, const char **attribs)
        override;
    void HandleEndTag(struct xml_userdata *data, const char *tag) override;
 private:
    /* reads capture profile names into member variables */
    void ReadCapProfileNames(StOperatingModes mode, const char* * attribs);

    SoundTriggerUUID vendor_uuid_;
    bool is_qcva_uuid_;
    bool is_qcmd_uuid_;
    bool get_module_version_supported_;
    bool merge_first_stage_sound_models_;
    uint32_t sample_rate_;
    uint32_t bit_width_;
    uint32_t out_channels_;
    uint32_t capture_keyword_;
    uint32_t client_capture_read_delay_;
    uint32_t kw_start_tolerance_;
    uint32_t kw_end_tolerance_;
    uint32_t data_before_kw_start_;
    uint32_t data_after_kw_end_;
    const st_cap_profile_map_t& cap_profile_map_;
    std::map<std::pair<StOperatingModes, StInputModes>, std::shared_ptr<CaptureProfile>> op_modes_;
    std::shared_ptr<SoundTriggerXml> curr_child_;
    std::map<uint32_t, std::shared_ptr<SecondStageConfig>> ss_config_list_;
    std::map<uint32_t, std::shared_ptr<SoundTriggerModuleInfo>> st_module_info_list_;
    std::map<UUID, std::shared_ptr<SoundTriggerModuleInfo>> sm_list_uuid_mod_info_;
};

class SoundTriggerPlatformInfo : public SoundTriggerXml {
 public:
    SoundTriggerPlatformInfo(SoundModelConfig &rhs) = delete;
    SoundTriggerPlatformInfo & operator=(SoundTriggerPlatformInfo &rhs) =
         delete;

    static std::shared_ptr<SoundTriggerPlatformInfo> GetInstance();
    std::string GetSoundModelLib() const { return sound_model_lib_; }
    uint32_t GetVersion() const { return version_; }
    bool GetEnableFailureDetection() const {
        return enable_failure_detection_; }
    bool GetSupportDevSwitch() const { return support_device_switch_; }
    bool GetSupportNLPISwitch() const { return support_nlpi_switch_; }
    bool GetTransitToNonLpiOnCharging() const {
        return transit_to_non_lpi_on_charging_;
    }
    bool GetDedicatedSvaPath() const { return dedicated_sva_path_; }
    bool GetDedicatedHeadsetPath() const { return dedicated_headset_path_; }
    bool GetLpiEnable() const { return lpi_enable_; }
    bool GetEnableDebugDumps() const { return enable_debug_dumps_; }
    bool GetNonLpiWithoutEc() const { return non_lpi_without_ec_; }
    bool GetConcurrentCaptureEnable() const { return concurrent_capture_; }
    bool GetConcurrentVoiceCallEnable() const { return concurrent_voice_call_; }
    bool GetConcurrentVoipCallEnable() const { return concurrent_voip_call_; }
    bool GetLowLatencyBargeinEnable() const {
        return low_latency_bargein_enable_;
    }
    bool GetMmapEnable() const { return mmap_enable_; }
    bool GetNotifySecondStageFailure() { return notify_second_stage_failure_; }
    uint32_t GetMmapBufferDuration() const { return mmap_buffer_duration_; }
    uint32_t GetMmapFrameLength() const { return mmap_frame_length_; }
    std::shared_ptr<SoundModelConfig> GetSmConfig(const UUID& uuid) const;
    std::shared_ptr<CaptureProfile> GetCapProfile(const std::string& name) const;
    void GetSmConfigForVersionQuery(
        std::vector<std::shared_ptr<SoundModelConfig>> &sm_cfg_list) const;

    void HandleStartTag(const char *tag, const char **attribs)
        override;
    void HandleEndTag(struct xml_userdata *data, const char *tag) override;

 private:
    SoundTriggerPlatformInfo();
    static std::shared_ptr<SoundTriggerPlatformInfo> me_;
    uint32_t version_;
    bool enable_failure_detection_;
    bool support_device_switch_;
    bool support_nlpi_switch_;
    bool transit_to_non_lpi_on_charging_;
    bool dedicated_sva_path_;
    bool dedicated_headset_path_;
    bool lpi_enable_;
    bool enable_debug_dumps_;
    bool non_lpi_without_ec_;
    bool concurrent_capture_;
    bool concurrent_voice_call_;
    bool concurrent_voip_call_;
    bool low_latency_bargein_enable_;
    bool mmap_enable_;
    bool notify_second_stage_failure_;
    uint32_t mmap_buffer_duration_;
    uint32_t mmap_frame_length_;
    std::string sound_model_lib_;
    std::map<UUID, std::shared_ptr<SoundModelConfig>> sound_model_cfg_list_;
    st_cap_profile_map_t capture_profile_map_;
    std::shared_ptr<SoundTriggerXml> curr_child_;
};
#endif
