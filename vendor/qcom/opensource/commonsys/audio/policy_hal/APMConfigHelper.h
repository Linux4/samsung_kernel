/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of The Linux Foundation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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

#ifndef _APM_CONFIG_HELPER_H_
#define _APM_CONFIG_HELPER_H_

#include <string>
#include <utils/String8.h>
#include <media/stagefright/foundation/ABase.h>
#include <utils/RefBase.h>

#ifdef AHAL_EXT_ENABLED
#include <vendor/qti/hardware/audiohalext/1.0/IAudioHalExt.h>
#include <vendor/qti/hardware/audiohalext/1.0/types.h>
#include <configstore/Utils.h>
#endif

namespace android {

using namespace std;

#ifdef AHAL_EXT_ENABLED
using namespace vendor::qti::hardware::audiohalext::V1_0;
using namespace vendor::qti::hardware::audiohalext;
#else
struct ApmValues {
    bool     audio_offload_video;
    bool     audio_offload_disable;
    bool     audio_deepbuffer_media;
    bool     audio_av_streaming_offload_enable;
    bool     audio_offload_track_enable;
    bool     audio_offload_multiple_enabled;
    bool     voice_dsd_playback_conc_disabled;
    bool     audio_sva_conc_enabled;
    bool     audio_va_concurrency_enabled;
    bool     audio_rec_playback_conc_disabled;
    bool     voice_path_for_pcm_voip;
    bool     voice_playback_conc_disabled;
    bool     voice_record_conc_disabled;
    bool     voice_voip_conc_disabled;
    uint32_t audio_offload_min_duration_secs;
    string   voice_conc_fallbackpath;
    bool     audio_extn_hdmi_spk_enabled;
    bool     audio_extn_formats_enabled;
    bool     audio_extn_afe_proxy_enabled;
    bool     compress_voip_enabled;
    bool     fm_power_opt;
    bool     voice_concurrency;
    bool     record_play_concurrency;
    bool     use_xml_audio_policy_conf;
};
#endif

class APMConfigHelper : public RefBase {
public:
    APMConfigHelper();

    virtual ~APMConfigHelper() {};

    /* member functions to query settigns */
    bool isAudioOffloadVideoEnabled();
    bool isAudioOffloadDisabled();
    bool isAudioDeepbufferMediaEnabled();
    bool isAVStreamingOffloadEnabled();
    bool isAudioTrackOffloadEnabled();
    bool isAudioMultipleOffloadEnable();
    bool isVoiceDSDConcDisabled();
    bool isSVAConcEnabled();
    bool isVAConcEnabled();
    bool isRecPlayConcDisabled();
    bool useVoicePathForPCMVOIP();
    bool isVoicePlayConcDisabled();
    bool isVoiceRecConcDisabled();
    bool isVoiceVOIPConcDisabled();

    uint32_t getAudioOffloadMinDuration();
    string getVoiceConcFallbackPath();

    bool isHDMISpkEnabled();
    bool isExtnFormatsEnabled();
    bool isAFEProxyEnabled();
    bool isCompressVOIPEnabled();
    bool isFMPowerOptEnabled();
    bool isVoiceConcEnabled();
    bool isRecPlayConcEnabled();
    bool useXMLAudioPolicyConf();
    void dump(String8 *dst) const;
private:
    inline void retrieveConfigs();

    ApmValues mConfigs;
    bool      isRemote; // configs specified from remote

    DISALLOW_EVIL_CONSTRUCTORS(APMConfigHelper);
};

}

#endif /* _APM_CONFIG_HELPER_H_ */
