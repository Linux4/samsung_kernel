/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
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
/*
 * Changes from Qualcomm Innovation Center are provided under the following license:
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef ANDROID_HARDWARE_AHAL_ADEVICE_H_
#define ANDROID_HARDWARE_AHAL_ADEVICE_H_

#include <stdlib.h>
#include <unistd.h>
#include <mutex>
#include <vector>
#include <set>
#include <string>

#include <cutils/properties.h>
#include <hardware/audio.h>
#include <system/audio.h>

#include <expat.h>

#include "AudioStream.h"
#include "AudioVoice.h"
#include "PalDefs.h"

#define MAX_PERF_LOCK_OPTS 20

/* HDR Audio use case parameters */
#define AUDIO_PARAMETER_KEY_HDR "hdr_record_on"
#define AUDIO_PARAMETER_KEY_WNR "wnr_on"
#define AUDIO_PARAMETER_KEY_ANS "ans_on"
#define AUDIO_PARAMETER_KEY_ORIENTATION "orientation"
#define AUDIO_PARAMETER_KEY_INVERTED "inverted"
#define AUDIO_PARAMETER_KEY_FACING "facing"
#define AUDIO_PARAMETER_KEY_HDR_CHANNELS "hdr_audio_channel_count"
#define AUDIO_PARAMETER_KEY_HDR_SAMPLERATE "hdr_audio_sampling_rate"

#define AUDIO_MAKE_STRING_FROM_ENUM(X)   { #X, X }
#define PAL_MAX_INPUT_DEVICES (PAL_DEVICE_IN_MAX - (PAL_DEVICE_IN_MIN + 1))
#define MIC_INFO_MAP_INDEX(X) (X - (PAL_DEVICE_IN_MIN + 1))
#define XML_READ_BUFFER_SIZE 1024
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#define COMPRESS_CAPTURE_AAC_MAX_OUTPUT_BUFFER_SIZE 2048
#define COMPRESS_CAPTURE_AAC_PCM_SAMPLES_IN_FRAME 1024

#ifdef SEC_AUDIO_CALL
// pal device direction for voice
enum {
    PAL_TX = 0,
    PAL_RX,
    PAL_TX_RX,
};

// custom key tables
enum {
    CUSTOM_KEY_INVALID = -1,
    /* voice usecases */
    CUSTOM_KEY_VOLTE_VOICE_CALL,
    CUSTOM_KEY_VOLTE_VOICE_CALL_WITH_VF,
    CUSTOM_KEY_VOLTE_VIDEO_CALL,
    CUSTOM_KEY_VOLTE_VIDEO_CALL_WITH_VF,
    CUSTOM_KEY_GSM3G_VOICE_CALL,
    CUSTOM_KEY_GSM3G_VOICE_CALL_WITH_VF,
    CUSTOM_KEY_VOWIFI_CALL,
    CUSTOM_KEY_VOIP_SMART_VIEW_CALL,
    CUSTOM_KEY_MAX
};

const char * const ck_table[CUSTOM_KEY_MAX] = {
    [CUSTOM_KEY_VOWIFI_CALL]              = "vowifi",
    [CUSTOM_KEY_VOLTE_VOICE_CALL]         = "volte-voice",
    [CUSTOM_KEY_VOLTE_VOICE_CALL_WITH_VF] = "volte-voice-vf",
    [CUSTOM_KEY_VOLTE_VIDEO_CALL]         = "volte-video",
    [CUSTOM_KEY_VOLTE_VIDEO_CALL_WITH_VF] = "volte-video-vf",
    [CUSTOM_KEY_GSM3G_VOICE_CALL]         = "gsm3g-voice",
    [CUSTOM_KEY_GSM3G_VOICE_CALL_WITH_VF] = "gsm3g-voice-vf",
    [CUSTOM_KEY_VOIP_SMART_VIEW_CALL]     = "voip-smartview",
};
#endif

#ifdef SEC_AUDIO_COMMON
#define AUDIO_PARAMETER_SEC_LOCAL_FMRADIO_ROUTING "l_fmradio_routing"
#endif

/*M55 code for P240105-05058 by hujincan at 2023/01/16 start*/
enum {
    BT_SCO_SWITCH_DEFAULT = 0,
    BT_SCO_SWITCH_HANDSET,
    BT_SCO_SWITCH_BT,
};
/*M55 code for P240105-05058 by hujincan at 2023/01/16 end*/

typedef enum {
    TAG_MICROPHONE_CHARACTERISTIC,
    TAG_SND_DEVICES,
    TAG_INPUT_SND_DEVICE,
    TAG_INPUT_SND_DEVICE_TO_MIC_MAPPING,
    TAG_SND_DEV,
    TAG_MIC_INFO
} mic_xml_tags_t;

/*M55 code for SR-QN6887A-01-565&SR-QN6887A-01-572 by yingboyang at 2023/08/23 start*/
enum speaker_test_specific_type {
    SPK_TEST_DEFAULT,
    SPK_TEST_SPK1_ONLY,
    SPK_TEST_SPK2_ONLY,
    SPK_TEST_SPK3_ONLY,
};

enum mic_test_specific_type {
    MIC_TEST_DEFAULT,
    MIC_TEST_MIC1_ONLY,
    MIC_TEST_MIC2_ONLY,
    MIC_TEST_MIC3_ONLY,
};
/*M55 code for SR-QN6887A-01-565&SR-QN6887A-01-572 by yingboyang at 2023/08/23 end*/

typedef struct xml_userdata_t {
    char data_buf[XML_READ_BUFFER_SIZE];
    size_t offs;
    mic_xml_tags_t tag;
} xml_userdata_t;

typedef struct microphone_characteristics_t
{
    uint32_t declared_mic_count;
    struct audio_microphone_characteristic_t microphone[AUDIO_MICROPHONE_MAX_COUNT];
} microphone_characteristics_t;

typedef struct mic_info_t {
    char device_id[AUDIO_MICROPHONE_ID_MAX_LEN];
    uint32_t channel_count;
    audio_microphone_channel_mapping_t channel_mapping[AUDIO_CHANNEL_COUNT_MAX];
} mic_info_t;

typedef struct snd_device_to_mic_map_t {
    mic_info_t microphones[AUDIO_MICROPHONE_MAX_COUNT];
    uint32_t mic_count;
} snd_device_to_mic_map_t;

/*M55 code for SR-QN6887A-01-563 by yingboyang at 20230919 start*/
typedef struct pal_param_rotation {
    int    angle;
} pal_param_rotation_t;

extern pal_param_rotation_t param_angle;
/*M55 code for SR-QN6887A-01-563 by yingboyang at 20230919 end*/

class AudioPatch{
    public:
        enum PatchType{
            PATCH_NONE = -1,
            PATCH_PLAYBACK,
            PATCH_CAPTURE,
            PATCH_DEVICE_LOOPBACK
        };
        AudioPatch() = default;
        AudioPatch(PatchType patch_type,
                   const std::vector<struct audio_port_config>& sources,
                   const std::vector<struct audio_port_config>& sinks);
   protected:
        enum PatchType type = PATCH_NONE;
        audio_patch_handle_t handle = AUDIO_PATCH_HANDLE_NONE;
        std::vector<struct audio_port_config> sources, sinks;
        static audio_patch_handle_t generate_patch_handle_l();
        friend class AudioDevice;
};

class AudioDevice {
public:
    ~AudioDevice();
    static std::shared_ptr<AudioDevice> GetInstance();
    static std::shared_ptr<AudioDevice> GetInstance(audio_hw_device_t* device);
    int Init(hw_device_t **device, const hw_module_t *module);
    std::shared_ptr<StreamOutPrimary> CreateStreamOut(
            audio_io_handle_t handle,
            const std::set<audio_devices_t> &devices,
            audio_output_flags_t flags,
            struct audio_config *config,
            audio_stream_out **stream_out,
            const char *address);
    void CloseStreamOut(std::shared_ptr<StreamOutPrimary> stream);
    int CreateAudioPatch(audio_patch_handle_t* handle,
                         const std::vector<struct audio_port_config>& sources,
                         const std::vector<struct audio_port_config>& sinks);
    int ReleaseAudioPatch(audio_patch_handle_t handle);
    int SetGEFParam(void *data, int length);
    int GetGEFParam(void *data, int *length);
    std::shared_ptr<StreamOutPrimary> OutGetStream(audio_io_handle_t handle);
    std::shared_ptr<StreamOutPrimary> OutGetStream(audio_stream_t* audio_stream);
    std::shared_ptr<StreamInPrimary> CreateStreamIn(
            audio_io_handle_t handle,
            const std::set<audio_devices_t> &devices,
            audio_input_flags_t flags,
            struct audio_config *config,
            const char *address,
            audio_stream_in **stream_in,
            audio_source_t source);
    void CloseStreamIn(std::shared_ptr<StreamInPrimary> stream);
    std::shared_ptr<StreamInPrimary> InGetStream(audio_io_handle_t handle);
    std::shared_ptr<StreamInPrimary> InGetStream(audio_stream_t* stream_in);
    std::shared_ptr<AudioVoice> voice_;
    int SetMicMute(bool state);
    bool mute_;
    int GetMicMute(bool *state);
    int SetParameters(const char *kvpairs);
    char* GetParameters(const char *keys);
    int SetMode(const audio_mode_t mode);
    int SetVoiceVolume(float volume);
    void SetChargingMode(bool is_charging);
    void FillAndroidDeviceMap();
    int GetPalDeviceIds(
            const std::set<audio_devices_t>& hal_device_id,
            pal_device_id_t* pal_device_id);
    int usb_card_id_;
    int usb_dev_num_;
    int dp_controller;
    int dp_stream;
    int num_va_sessions_ = 0;
#ifdef SEC_AUDIO_COMMON
    uint32_t vsid;
    bool call_type_realcalling;
    bool call_type_wificalling;
    bool call_type_volte_video;
    bool call_type_gsm3g_voice;
    /*M55 code for QN6887A-1679 by hujincan at 2023/12/06 start*/
    uint32_t call_band_vowifi;
    /*M55 code for QN6887A-1679 by hujincan at 2023/12/06 end*/
#endif
    /*M55 code for P240105-05058 by hujincan at 2024/01/26 start*/
    int bt_sco_switch = BT_SCO_SWITCH_DEFAULT;
    bool rx_device_switch = false;
    /*M55 code for P240105-05058 by hujincan at 2024/01/26 end*/
    /*M55 code for P240126-04225 by hujincan at 2024/02/22 start */
    bool ringbacktone_state = false;
    /*M55 code for P240126-04225 by hujincan at 2024/02/22 end */
    pal_speaker_rotation_type current_rotation;
    static card_status_t sndCardState;
    std::mutex adev_init_mutex;
    std::mutex adev_perf_mutex;
    uint32_t adev_init_ref_count = 0;
    int32_t perf_lock_acquire_cnt = 0;
    hw_device_t *GetAudioDeviceCommon();
    int perf_lock_handle;
    int perf_lock_opts[MAX_PERF_LOCK_OPTS];
    int perf_lock_opts_size;
    bool hdr_record_enabled = false;
    bool wnr_enabled = false;
    bool ans_enabled = false;
    bool orientation_landscape = true;
    bool inverted = false;
    /*M55 code for SR-QN6887A-01-565&SR-QN6887A-01-572 by yingboyang at 2023/08/23 start*/
    bool speaker_test = false;
    bool mic_test = false;
    int speaker_choose = 0;
    int mic_choose = 0;
    /*M55 code for SR-QN6887A-01-565&SR-QN6887A-01-572 by yingboyang at 2023/08/23 end*/
    /*M55 code for QN6887A-1485 by zhouchenghua at 2023/11/29 start*/
    bool bt_nrec_flag = false;
    /*M55 code for QN6887A-1485 by zhouchenghua at 2023/11/29 end*/
    int  facing = 0; /*0 - none, 1 - back, 2 - front/selfie*/
    int  hdr_channel_count = 0;
    int  hdr_sample_rate = 0;
    int cameraOrientation = CAMERA_DEFAULT;
    bool hac_voip = false;
    bool usb_input_dev_enabled = false;
    bool usb_out_headset = false;
    static bool mic_characteristics_available;
    static microphone_characteristics_t microphones;
    static snd_device_to_mic_map_t microphone_maps[PAL_MAX_INPUT_DEVICES];
    static bool find_enum_by_string(const struct audio_string_to_enum * table, const char * name,
                                    int32_t len, unsigned int *value);
    static bool set_microphone_characteristic(struct audio_microphone_characteristic_t *mic);
    static int32_t get_microphones(struct audio_microphone_characteristic_t *mic_array, size_t *mic_count);
    static void process_microphone_characteristics(const XML_Char **attr);
    static bool is_input_pal_dev_id(int deviceId);
    static void process_snd_dev(const XML_Char **attr);
    static bool set_microphone_map(pal_device_id_t in_snd_device, const mic_info_t *info);
    static bool is_built_in_input_dev(pal_device_id_t deviceId);
    static void process_mic_info(const XML_Char **attr);
    int32_t get_active_microphones(uint32_t channels, pal_device_id_t id,
                                          struct audio_microphone_characteristic_t *mic_array,
                                          uint32_t *mic_count);
    static void xml_start_tag(void *userdata, const XML_Char *tag_name,
                             const XML_Char **attr);
    static void xml_end_tag(void *userdata, const XML_Char *tag_name);
    static void xml_char_data_handler(void *userdata, const XML_Char *s, int len);
    static int parse_xml();
#ifdef SEC_AUDIO_CALL
    audio_io_handle_t primary_out_io_handle = AUDIO_IO_HANDLE_NONE;
#endif
#ifdef SEC_AUDIO_COMMON
    std::shared_ptr<StreamInPrimary> GetActiveInStreamByUseCase(int UseCase);
    std::shared_ptr<StreamOutPrimary> OutGetStream(pal_stream_type_t pal_stream_type);
#endif
#ifdef SEC_AUDIO_CALL_VOIP
    bool RerouteForVoip();
#endif
#ifdef SEC_AUDIO_SCREEN_MIRRORING
    bool voip_via_smart_view = false;
#endif
#ifdef SEC_AUDIO_SOUND_TRIGGER_TYPE
    int register_voice_keyword = 0;
    bool seamless_enabled = false;
#endif
#ifdef SEC_AUDIO_DUAL_SPEAKER
    bool receiver_speaker_off = false;
#endif
protected:
    AudioDevice() {}
    std::shared_ptr<AudioVoice> VoiceInit();
    static std::shared_ptr<AudioDevice> adev_;
    static std::shared_ptr<audio_hw_device_t> device_;
    std::vector<std::shared_ptr<StreamOutPrimary>> stream_out_list_;
    std::vector<std::shared_ptr<StreamInPrimary>> stream_in_list_;
    std::mutex out_list_mutex;
    std::mutex in_list_mutex;
    std::mutex patch_map_mutex;
    btsco_lc3_cfg_t btsco_lc3_cfg;
    bool bt_lc3_speech_enabled;
    void *offload_effects_lib_;
    offload_effects_start_output fnp_offload_effect_start_output_ = nullptr;
    offload_effects_stop_output fnp_offload_effect_stop_output_ = nullptr;
    bool is_charging_;
    void *visualizer_lib_;
    visualizer_hal_start_output fnp_visualizer_start_output_ = nullptr;
    visualizer_hal_stop_output fnp_visualizer_stop_output_ = nullptr;
    std::map<audio_devices_t, pal_device_id_t> android_device_map_;
    std::map<audio_patch_handle_t, AudioPatch*> patch_map_;
    int add_input_headset_if_usb_out_headset(int *device_count,  pal_device_id_t** pal_device_ids, bool conn_state);
#ifdef SEC_AUDIO_SCREEN_MIRRORING
private:
    void UpdateSmartViewState(bool new_state);
#endif
};

static inline uint32_t lcm(uint32_t num1, uint32_t num2)
{
    uint32_t high = num1, low = num2, temp = 0;

    if (!num1 || !num2)
        return 0;

    if (num1 < num2) {
         high = num2;
         low = num1;
    }

    while (low != 0) {
        temp = low;
        low = high % low;
        high = temp;
    }
    return (num1 * num2)/high;
}

static inline uint32_t nearest_multiple(uint32_t num, uint32_t multiplier)
{
    uint32_t remainder = 0;

    if (!multiplier)
        return num;

    remainder = num % multiplier;
    if (remainder)
        num += (multiplier - remainder);

    return num;
}

#endif //ANDROID_HARDWARE_AHAL_ADEVICE_H_
