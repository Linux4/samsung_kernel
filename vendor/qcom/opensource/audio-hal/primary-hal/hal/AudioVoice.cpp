/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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

#define LOG_TAG "AHAL: AudioVoice"
#define ATRACE_TAG (ATRACE_TAG_AUDIO|ATRACE_TAG_HAL)
#define LOG_NDEBUG 0

#include <stdio.h>
#include <cutils/str_parms.h>
#include "audio_extn.h"
#include "AudioVoice.h"
#include "PalApi.h"
#include "AudioCommon.h"
#ifdef SEC_AUDIO_COMMON
#include "AudioDevice.h"
#endif
/*M55 code for P240105-05058 by hujincan at 2023/01/16 start*/
#include <signal.h>
#include <sys/time.h>
/*M55 code for P240105-05058 by hujincan at 2023/01/16 end*/

#ifndef AUDIO_MODE_CALL_SCREEN
#define AUDIO_MODE_CALL_SCREEN 4
#endif


int AudioVoice::SetMode(const audio_mode_t mode) {
    int ret = 0;

    AHAL_DBG("Enter: mode: %d", mode);
    if (mode_ != mode) {
#ifdef SEC_AUDIO_CALL
        if (mode == AUDIO_MODE_IN_CALL) {
            voice_session_t *session = NULL;
            std::shared_ptr<AudioDevice> adevice =
                AudioDevice::GetInstance();

            for (int i = 0; i < max_voice_sessions_; i++) {
                if (adevice->vsid == voice_.session[i].vsid) {
                    session = &voice_.session[i];
                    break;
                }
            }
            if (session) {
                session->state.new_ = CALL_ACTIVE;
                AHAL_DBG("new state is ACTIVE for vsid:%x", session->vsid);
            }
        }
#endif
        /*start a new session for full voice call*/
        if ((mode ==  AUDIO_MODE_CALL_SCREEN && mode_ == AUDIO_MODE_IN_CALL)||
           (mode == AUDIO_MODE_IN_CALL && mode_ == AUDIO_MODE_CALL_SCREEN)){
            mode_ = mode;
            AHAL_DBG("call screen device switch called: %d", mode);
            VoiceSetDevice(voice_.session);
        } else {
            mode_ = mode;
            if (voice_.in_call && mode == AUDIO_MODE_NORMAL)
                ret = StopCall();
            else if (mode ==  AUDIO_MODE_CALL_SCREEN)
                UpdateCalls(voice_.session);
        }
    }
    AHAL_DBG("Exit ret: %d", ret);
    return ret;
}

int AudioVoice::VoiceSetParameters(const char *kvpairs) {
    int value, i;
    char c_value[32];
    int ret = 0, err;
    struct str_parms *parms;
    pal_param_payload *params = nullptr;
    uint32_t tty_mode;
    bool volume_boost;
    bool slow_talk;
    bool hd_voice;
    bool hac;

    parms = str_parms_create_str(kvpairs);
    if (!parms)
       return  -EINVAL;

    AHAL_DBG("Enter params: %s", kvpairs);

#ifdef SEC_AUDIO_COMMON
#define AUDIO_PARAMETER_KEY_SAMSUNG_SIM_SLOT "g_call_sim_slot"
    std::shared_ptr<AudioDevice> adevice = AudioDevice::GetInstance();

    err = str_parms_get_int(parms, AUDIO_PARAMETER_KEY_SAMSUNG_SIM_SLOT, &value);
    if (err >= 0) {
        str_parms_del(parms, AUDIO_PARAMETER_KEY_SAMSUNG_SIM_SLOT);

#define SIM_SLOT_REAL_CALL_SIM_1 0x01
#define SIM_SLOT_REAL_CALL_SIM_2 0x02
#define SIM_SLOT_EMERGENCY_SIM_X 0x10

        switch(value) {
            case SIM_SLOT_REAL_CALL_SIM_1:
                adevice->vsid = VOICEMMODE1_VSID;
                break;

            case SIM_SLOT_REAL_CALL_SIM_2:
#ifdef SEC_AUDIO_USE_SINGLE_VSID
                adevice->vsid = VOICEMMODE1_VSID;
#else
                adevice->vsid = VOICEMMODE2_VSID;
#endif
                break;

            case SIM_SLOT_EMERGENCY_SIM_X:
                if(adevice->call_type_realcalling) {
                    UpdateCallState(adevice->vsid, CALL_INACTIVE);
                }

                switch(adevice->vsid) {
                    case VOICEMMODE1_VSID:
#ifdef SEC_AUDIO_USE_SINGLE_VSID
                        adevice->vsid = VOICEMMODE1_VSID;
#else
                        adevice->vsid = VOICEMMODE2_VSID;
#endif
                        break;

                    case VOICEMMODE2_VSID:
                        adevice->vsid = VOICEMMODE1_VSID;
                        break;

                    default:
                        adevice->vsid = -1;
                        ret = -EINVAL;
                        goto done;
                }

                if(adevice->call_type_realcalling) {
                    ret = UpdateCallState(adevice->vsid, CALL_ACTIVE);
                }
                break;

            default:
                adevice->vsid = -1;
                ret = -EINVAL;
                goto done;
        }
    }

#define AUDIO_PARAMETER_KEY_SAMSUNG_CALL_STATE "g_call_state"

    err = str_parms_get_int(parms, AUDIO_PARAMETER_KEY_SAMSUNG_CALL_STATE, &value);
    if (err >= 0) {
        str_parms_del(parms, AUDIO_PARAMETER_KEY_SAMSUNG_CALL_STATE);

#ifdef SEC_AUDIO_CALL
        bool prev_state;
        
        if (value & CALL_STATUS_REAL_CALL) {
            if (value & CALL_STATUS_REAL_CALL_ON) {
                adevice->call_type_gsm3g_voice = true;
            } else {
                adevice->call_type_gsm3g_voice = false;
            }
        }

        if (value & CALL_STATUS_VOLTE) {
            prev_state = adevice->call_type_volte_video;

            adevice->call_type_gsm3g_voice = false;

            if (value & CALL_STATUS_VOLTE_VIDEO) {
                adevice->call_type_volte_video = true;
            } else {
                adevice->call_type_volte_video = false;
            }

            // volte vt <-> voice/off, rerouting for spk path
            if (IsAnyCallActive()
                  && (prev_state != adevice->call_type_volte_video)
                  && (pal_voice_rx_device_id_ == PAL_DEVICE_OUT_SPEAKER)) {
                for (int i = 0; i < MAX_VOICE_SESSIONS; i++) {
                    if (IsCallActive(&voice_.session[i])) {
                        VoiceSetDevice(&voice_.session[i]);
                    }
                }
            }
        }

        AHAL_INFO("volte video call %s", adevice->call_type_volte_video ? "ON" : "OFF");
        AHAL_INFO("gsm3g voice call %s", adevice->call_type_gsm3g_voice ? "ON" : "OFF");

        prev_state = adevice->call_type_wificalling;
#endif

#define CALL_STATE_REAL_CALL_CLR CALL_STATUS_REAL_CALL_OFF
#define CALL_STATE_REAL_CALL_SET CALL_STATUS_REAL_CALL_ON
#define CALL_STATE_WIFI_CALL_CLR CALL_STATUS_VOIPWIFICALLING_OFF
#define CALL_STATE_WIFI_CALL_SET CALL_STATUS_VOIPWIFICALLING_ON
#define CALL_STATE_VOIP_CALL_SET CALL_STATUS_VOIP_AOSP_ON

        if(value & CALL_STATE_REAL_CALL_SET) {
            adevice->call_type_realcalling = true;
        } else if(value & CALL_STATE_REAL_CALL_CLR) {
            adevice->call_type_realcalling = false;
        } else if(value & CALL_STATE_WIFI_CALL_SET) {
            adevice->call_type_wificalling = true;
        } else if(value & CALL_STATE_WIFI_CALL_CLR) {
            adevice->call_type_wificalling = false;
        } else if(value & CALL_STATE_VOIP_CALL_SET) {
            adevice->call_type_wificalling = false;
        } else {
            ret = -EINVAL;
            goto done;
        }

#ifdef SEC_AUDIO_CALL
        // if other AP call path is already actived before, set path again here
        if ((adevice->call_type_wificalling != prev_state) && (mode_ == AUDIO_MODE_IN_COMMUNICATION)) {
            AHAL_INFO("reroute voip for wifi call %s", adevice->call_type_wificalling ? "ON" : "OFF");
            adevice->RerouteForVoip();
        }
#else
        if(value & (CALL_STATE_REAL_CALL_SET |
                    CALL_STATE_REAL_CALL_CLR)) {
            int call_state = -1;

            if(value & CALL_STATE_REAL_CALL_SET) {
                call_state = CALL_ACTIVE;
            } else if(value & CALL_STATE_REAL_CALL_CLR) {
                call_state = CALL_INACTIVE;
            }

            if (is_valid_vsid(adevice->vsid)) {
                ret = UpdateCallState(adevice->vsid, call_state);
            } else {
                ALOGE("%s: invalid vsid:%x [call_state:%d (is_valid_call_state:%d)]",
                      __func__, adevice->vsid, call_state, is_valid_call_state(call_state));
                ret = -EINVAL;
                goto done;
            }
        }
#endif
#else
    err = str_parms_get_int(parms, AUDIO_PARAMETER_KEY_VSID, &value);
    if (err >= 0) {
        uint32_t vsid = value;
        int call_state = -1;
        err = str_parms_get_int(parms, AUDIO_PARAMETER_KEY_CALL_STATE, &value);
        if (err >= 0) {
            call_state = value;
        } else {
            AHAL_ERR("error call_state key not found");
            ret = -EINVAL;
            goto done;
        }

        if (is_valid_vsid(vsid) && is_valid_call_state(call_state)) {
            ret = UpdateCallState(vsid, call_state);
        } else {
            AHAL_ERR("invalid vsid:%x or call_state:%d",
                     vsid, call_state);
            ret = -EINVAL;
            goto done;
        }
#endif
    }
#ifdef SEC_AUDIO_CALL_VOIP
    err = str_parms_get_int(parms, AUDIO_PARAMETER_SEC_LOCAL_MIC_INPUT_CONTROL_MODE, &value);
    if (err >= 0) {
        str_parms_del(parms, AUDIO_PARAMETER_SEC_LOCAL_MIC_INPUT_CONTROL_MODE);
        if (aosp_voip_call_isolation_mode != value) {
            aosp_voip_call_isolation_mode = value;
            AHAL_INFO("set aosp_voip_call_isolation_mode as %d \n", value);
            SetVoipMicModeEffect();
        }
    }

    err = str_parms_get_int(parms, AUDIO_PARAMETER_SEC_LOCAL_MIC_INPUT_CONTROL_MODE_CALL, &value);
    if (err >= 0) {
        str_parms_del(parms, AUDIO_PARAMETER_SEC_LOCAL_MIC_INPUT_CONTROL_MODE_CALL);
        if (wifi_real_call_isolation_mode != value) {
            wifi_real_call_isolation_mode = value;
            AHAL_INFO("set wifi_real_call_isolation_mode as %d \n", value);
            for (i = 0; i < max_voice_sessions_; i++) {
                if (IsCallActive(&voice_.session[i])) {
                    VoiceSetDevice(&voice_.session[i]);
                }
            }
            SetVoipMicModeEffect();
        }
    }

    /*M55 code for QN6887A-1679 by hujincan at 2023/12/06 start*/
    err = str_parms_get_str(parms, AUDIO_PARAMETER_SEC_GLOBAL_CALL_BAND, c_value, sizeof(c_value));
    if (err >= 0) {
        str_parms_del(parms, AUDIO_PARAMETER_SEC_GLOBAL_CALL_BAND);
        if (strcmp(c_value, AUDIO_PARAMETER_VALUE_NB) == 0){
            adevice->call_band_vowifi = NB;
        }
        if (strcmp(c_value, AUDIO_PARAMETER_VALUE_WB) == 0){
            adevice->call_band_vowifi = WB;
        }
        if (strcmp(c_value, AUDIO_PARAMETER_VALUE_SWB) == 0){
            adevice->call_band_vowifi = SWB;
        }
        if (strcmp(c_value, AUDIO_PARAMETER_VALUE_FB) == 0){
            adevice->call_band_vowifi = FB;
        }
    }
    /*M55 code for QN6887A-1679 by hujincan at 2023/12/06 end*/
#endif
    err = str_parms_get_str(parms, AUDIO_PARAMETER_KEY_TTY_MODE, c_value, sizeof(c_value));
    if (err >= 0) {
        if (strcmp(c_value, AUDIO_PARAMETER_VALUE_TTY_OFF) == 0)
            tty_mode = PAL_TTY_OFF;
        else if (strcmp(c_value, AUDIO_PARAMETER_VALUE_TTY_VCO) == 0)
            tty_mode = PAL_TTY_VCO;
        else if (strcmp(c_value, AUDIO_PARAMETER_VALUE_TTY_HCO) == 0)
            tty_mode = PAL_TTY_HCO;
        else if (strcmp(c_value, AUDIO_PARAMETER_VALUE_TTY_FULL) == 0)
            tty_mode = PAL_TTY_FULL;
        else {
            ret = -EINVAL;
            goto done;
        }

        for ( i = 0; i < max_voice_sessions_; i++) {
            voice_.session[i].tty_mode = tty_mode;
            if (IsCallActive(&voice_.session[i])) {
                params = (pal_param_payload *)calloc(1,
                                   sizeof(pal_param_payload) + sizeof(tty_mode));
                if (!params) {
                    AHAL_ERR("calloc failed for size %zu",
                            sizeof(pal_param_payload) + sizeof(tty_mode));
                    continue;
                }
                params->payload_size = sizeof(tty_mode);
                memcpy(params->payload, &tty_mode, params->payload_size);
                pal_stream_set_param(voice_.session[i].pal_voice_handle,
                                     PAL_PARAM_ID_TTY_MODE, params);
                free(params);
                params = nullptr;

                /*need to device switch for hco and vco*/
                if (tty_mode == PAL_TTY_VCO || tty_mode == PAL_TTY_HCO) {
                    VoiceSetDevice(&voice_.session[i]);
                }
            }
        }
    }
    err = str_parms_get_str(parms, AUDIO_PARAMETER_KEY_VOLUME_BOOST, c_value, sizeof(c_value));
    if (err >= 0) {
        if (strcmp(c_value, "on") == 0)
            volume_boost = true;
        else if (strcmp(c_value, "off") == 0) {
            volume_boost = false;
        }
        else {
            ret = -EINVAL;
            goto done;
        }
        params = (pal_param_payload *)calloc(1, sizeof(pal_param_payload) +
                                                sizeof(volume_boost));
        if (!params) {
            AHAL_ERR("calloc failed for size %zu",
                   sizeof(pal_param_payload) + sizeof(volume_boost));
        } else {
            params->payload_size = sizeof(volume_boost);
            params->payload[0] = volume_boost;

            for ( i = 0; i < max_voice_sessions_; i++) {
                voice_.session[i].volume_boost = volume_boost;
                if (IsCallActive(&voice_.session[i])) {
                    pal_stream_set_param(voice_.session[i].pal_voice_handle,
                                        PAL_PARAM_ID_VOLUME_BOOST, params);
                }
            }
            free(params);
            params = nullptr;
        }
    }

    err = str_parms_get_str(parms, AUDIO_PARAMETER_KEY_SLOWTALK, c_value, sizeof(c_value));
    if (err >= 0) {
        if (strcmp(c_value, "true") == 0)
            slow_talk = true;
        else if (strcmp(c_value, "false") == 0) {
            slow_talk = false;
        }
        else {
            ret = -EINVAL;
            goto done;
        }
        params = (pal_param_payload *)calloc(1, sizeof(pal_param_payload) +
                                                sizeof(slow_talk));
        if (!params) {
            AHAL_ERR("calloc failed for size %zu",
                   sizeof(pal_param_payload) + sizeof(slow_talk));
        } else {
            params->payload_size = sizeof(slow_talk);
            params->payload[0] = slow_talk;

            for ( i = 0; i < max_voice_sessions_; i++) {
                voice_.session[i].slow_talk = slow_talk;
                if (IsCallActive(&voice_.session[i])) {
                    pal_stream_set_param(voice_.session[i].pal_voice_handle,
                                         PAL_PARAM_ID_SLOW_TALK, params);
                }
            }
            free(params);
            params = nullptr;
        }
    }
    err = str_parms_get_str(parms, AUDIO_PARAMETER_KEY_HD_VOICE, c_value, sizeof(c_value));
    if (err >= 0) {
        if (strcmp(c_value, "true") == 0)
            hd_voice = true;
        else if (strcmp(c_value, "false") == 0) {
            hd_voice = false;
        }
        else {
            ret = -EINVAL;
            goto done;
        }
        params = (pal_param_payload *)calloc(1, sizeof(pal_param_payload) +
                                                sizeof(hd_voice));
        if (!params) {
            AHAL_ERR("calloc failed for size %zu",
                     sizeof(pal_param_payload) + sizeof(hd_voice));
        } else {
            params->payload_size = sizeof(hd_voice);
            params->payload[0] = hd_voice;

            for ( i = 0; i < max_voice_sessions_; i++) {
                voice_.session[i].hd_voice = hd_voice;
                if (IsCallActive(&voice_.session[i])) {
                    pal_stream_set_param(voice_.session[i].pal_voice_handle,
                                         PAL_PARAM_ID_HD_VOICE, params);
                }
            }
            free(params);
            params = nullptr;
        }
    }
    err = str_parms_get_str(parms, AUDIO_PARAMETER_KEY_DEVICE_MUTE, c_value,
                            sizeof(c_value));
    if (err >= 0) {
        bool mute = false;
        pal_stream_direction_t dir = PAL_AUDIO_INPUT;
        str_parms_del(parms, AUDIO_PARAMETER_KEY_DEVICE_MUTE);

        if (strcmp(c_value, "true") == 0) {
            mute = true;
        }

        err = str_parms_get_str(parms, AUDIO_PARAMETER_KEY_DIRECTION, c_value,
                                sizeof(c_value));
        if (err >= 0) {
            str_parms_del(parms, AUDIO_PARAMETER_KEY_DIRECTION);

            if (strcmp(c_value, "rx") == 0){
                dir = PAL_AUDIO_OUTPUT;
            }
        } else {
            AHAL_ERR("error direction key not found");
            ret = -EINVAL;
            goto done;
        }
        params = (pal_param_payload *)calloc(1, sizeof(pal_param_payload) +
                                                sizeof(pal_device_mute_t));
        if (!params) {
            AHAL_ERR("calloc failed for size %zu",
                     sizeof(pal_param_payload) + sizeof(pal_device_mute_t));
        } else {
            params->payload_size = sizeof(pal_device_mute_t);

            for ( i = 0; i < max_voice_sessions_; i++) {
                voice_.session[i].device_mute.mute = mute;
                voice_.session[i].device_mute.dir = dir;
                memcpy(params->payload, &(voice_.session[i].device_mute), params->payload_size);
                if (IsCallActive(&voice_.session[i])) {
                    ret= pal_stream_set_param(voice_.session[i].pal_voice_handle,
                                         PAL_PARAM_ID_DEVICE_MUTE, params);
                }
                if (ret != 0) {
                    AHAL_ERR("Failed to set mute err:%d", ret);
                    ret = -EINVAL;
                    goto done;
                }
            }
        }
    }
    err = str_parms_get_str(parms, AUDIO_PARAMETER_KEY_HAC, c_value, sizeof(c_value));
    if (err >= 0) {
        hac = false;
        if (strcmp(c_value, AUDIO_PARAMETER_VALUE_HAC_ON) == 0)
            hac = true;
        for ( i = 0; i < max_voice_sessions_; i++) {
            if (voice_.session[i].hac != hac) {
                voice_.session[i].hac = hac;
                if (IsCallActive(&voice_.session[i])) {
                    ret = VoiceSetDevice(&voice_.session[i]);
                }
            }
        }
    }
    /*M55 code for QN6887A-1485 by zhouchenghua at 2023/12/14 start*/
    err = str_parms_get_str(parms,"bt_headset_nrec", c_value, sizeof(c_value));
    if (err >= 0) {
        if (strcmp(c_value, "on") == 0) {
            AHAL_DBG("VoiceSetParameters %s bt_nrec_voice_on success\n", __func__ );
            for ( i = 0; i < max_voice_sessions_; i++) {
                if (voice_.session[i].bt_nrec_voice_on != true) {
                    voice_.session[i].bt_nrec_voice_on = true;
                    AHAL_DBG("VoiceSetParameters %s VoiceSetDevice bt_nrec_voice_on\n", __func__ );
                    if (IsCallActive(&voice_.session[i])) {
                        AHAL_DBG("VoiceSetParameters %s VoiceSetDevice bt_nrec_voice_on\n", __func__ );
                        ret = VoiceSetDevice(&voice_.session[i]);
                    }
                }
            }
        } else{
            AHAL_DBG("VoiceSetParameters %s bt_nrec_voice_off success\n", __func__ );
            for ( i = 0; i < max_voice_sessions_; i++) {
                if (voice_.session[i].bt_nrec_voice_on != false) {
                    voice_.session[i].bt_nrec_voice_on = false;
                    AHAL_DBG("VoiceSetParameters %s VoiceSetDevice bt_nrec_voice_off\n", __func__ );
                }
            }
        }
    }
    /*M55 code for QN6887A-1485 by zhouchenghua at 2023/12/14 end*/
    /*M55 code for P240105-05058 by hujincan at 2023/01/16 start*/
    err = str_parms_get_str(parms, "BleSuspended", c_value, sizeof(c_value));
    if (err >= 0) {
        std::shared_ptr<AudioDevice> adevice = AudioDevice::GetInstance();
        if (strcmp(c_value, "false") == 0) {
            adevice->bt_sco_switch = BT_SCO_SWITCH_HANDSET;
        } else if (strcmp(c_value, "true") == 0) {
            adevice->bt_sco_switch = BT_SCO_SWITCH_BT;
        } else {
            adevice->bt_sco_switch = BT_SCO_SWITCH_DEFAULT;
        }
        AHAL_INFO("bt_sco_switch: %d, mode: %d", adevice->bt_sco_switch, mode_);
    }
    /*M55 code for P240105-05058 by hujincan at 2023/01/16 end*/
    /*M55 code for P240126-04225 by hujincan at 2024/02/22 start */
    err = str_parms_get_str(parms, "g_call_ringbacktone_state", c_value, sizeof(c_value));
    if (err >= 0) {
        std::shared_ptr<AudioDevice> adevice = AudioDevice::GetInstance();
        if (strcmp(c_value, "on") == 0) {
            adevice->ringbacktone_state = true;
        } else if (strcmp(c_value, "off") == 0) {
            adevice->ringbacktone_state = false;
        }
        AHAL_INFO("ringbacktone_state: %d", adevice->ringbacktone_state);
    }
    /*M55 code for P240126-04225 by hujincan at 2024/02/22 end */
done:
    str_parms_destroy(parms);
    AHAL_DBG("Exit ret: %d", ret);
    return ret;
}

void AudioVoice::VoiceGetParameters(struct str_parms *query, struct str_parms *reply)
{
    uint32_t tty_mode = 0;
    int ret = 0;
    char value[256]={0};

    ret = str_parms_get_str(query, AUDIO_PARAMETER_KEY_TTY_MODE,
                            value, sizeof(value));
    if (ret >= 0) {
        for (int voiceSession_ind = 0; voiceSession_ind < max_voice_sessions_; voiceSession_ind++) {
            tty_mode = voice_.session[voiceSession_ind].tty_mode;
        }
        if (tty_mode >= PAL_TTY_OFF || tty_mode <= PAL_TTY_FULL) {
            switch(tty_mode) {
                case PAL_TTY_OFF:
                    str_parms_add_str(reply, AUDIO_PARAMETER_KEY_TTY_MODE, AUDIO_PARAMETER_VALUE_TTY_OFF);
                break;
               case PAL_TTY_VCO:
                    str_parms_add_str(reply, AUDIO_PARAMETER_KEY_TTY_MODE, AUDIO_PARAMETER_VALUE_TTY_VCO);
                break;
                case PAL_TTY_HCO:
                    str_parms_add_str(reply, AUDIO_PARAMETER_KEY_TTY_MODE, AUDIO_PARAMETER_VALUE_TTY_HCO);
                break;
                case PAL_TTY_FULL:
                    str_parms_add_str(reply, AUDIO_PARAMETER_KEY_TTY_MODE, AUDIO_PARAMETER_VALUE_TTY_FULL);
                break;
            }
        } else {
            AHAL_ERR("Error happened for getting TTY mode");
        }
    }
#ifdef SEC_AUDIO_CALL_VOIP
    ret = str_parms_get_str(query, AUDIO_PARAMETER_SEC_LOCAL_MIC_INPUT_CONTROL_MODE, value, sizeof(value));
    if (ret >= 0) {
        str_parms_add_int(reply, AUDIO_PARAMETER_SEC_LOCAL_MIC_INPUT_CONTROL_MODE, aosp_voip_call_isolation_mode);
    }

    ret = str_parms_get_str(query, AUDIO_PARAMETER_SEC_LOCAL_MIC_INPUT_CONTROL_MODE_CALL, value, sizeof(value));
    if (ret >= 0) {
        str_parms_add_int(reply, AUDIO_PARAMETER_SEC_LOCAL_MIC_INPUT_CONTROL_MODE_CALL, wifi_real_call_isolation_mode);
    }
#endif
    return;
}

bool AudioVoice::is_valid_vsid(uint32_t vsid)
{
    if (vsid == VOICEMMODE1_VSID ||
        vsid == VOICEMMODE2_VSID)
        return true;
    else
        return false;
}

bool AudioVoice::is_valid_call_state(int call_state)
{
    if (call_state < CALL_INACTIVE || call_state > CALL_ACTIVE)
        return false;
    else
        return true;
}

int AudioVoice::GetMatchingTxDevices(const std::set<audio_devices_t>& rx_devices,
                                     std::set<audio_devices_t>& tx_devices){
    std::shared_ptr<AudioDevice> adevice = AudioDevice::GetInstance();
    for(auto rx_dev : rx_devices)
        switch(rx_dev) {
            case AUDIO_DEVICE_OUT_EARPIECE:
                tx_devices.insert(AUDIO_DEVICE_IN_BUILTIN_MIC);
                break;
            case AUDIO_DEVICE_OUT_SPEAKER:
                tx_devices.insert(AUDIO_DEVICE_IN_BACK_MIC);
                break;
            case AUDIO_DEVICE_OUT_WIRED_HEADSET:
                tx_devices.insert(AUDIO_DEVICE_IN_WIRED_HEADSET);
                break;
            case AUDIO_DEVICE_OUT_LINE:
            case AUDIO_DEVICE_OUT_WIRED_HEADPHONE:
                tx_devices.insert(AUDIO_DEVICE_IN_BUILTIN_MIC);
                break;
            case AUDIO_DEVICE_OUT_USB_HEADSET:
            case AUDIO_DEVICE_OUT_USB_DEVICE:
                if (adevice->usb_input_dev_enabled)
                    tx_devices.insert(AUDIO_DEVICE_IN_USB_HEADSET);
                else
                    tx_devices.insert(AUDIO_DEVICE_IN_BUILTIN_MIC);
                break;
            case AUDIO_DEVICE_OUT_BLUETOOTH_SCO:
            case AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET:
            case AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT:
                tx_devices.insert(AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET);
                break;
            case AUDIO_DEVICE_OUT_HEARING_AID:
                tx_devices.insert(AUDIO_DEVICE_IN_BUILTIN_MIC);
                break;
            default:
                tx_devices.insert(AUDIO_DEVICE_NONE);
                AHAL_ERR("unsupported Device Id of %d", rx_dev);
                break;
        }

    return tx_devices.size();
}

int AudioVoice::RouteStream(const std::set<audio_devices_t>& rx_devices) {
    int ret = 0;
    std::set<audio_devices_t> tx_devices;
    pal_device_id_t pal_rx_device = (pal_device_id_t) NULL;
    pal_device_id_t pal_tx_device = (pal_device_id_t) NULL;
    pal_device_id_t* pal_device_ids = NULL;
    uint16_t device_count = 0;

    AHAL_DBG("Enter");

    if (AudioExtn::audio_devices_empty(rx_devices)){
        AHAL_ERR("invalid routing device %d", AudioExtn::get_device_types(rx_devices));
        goto exit;
    }

    GetMatchingTxDevices(rx_devices, tx_devices);

    /**
     * if device_none is in Tx/Rx devices,
     * which is invalid, teardown the usecase.
     */
    if (tx_devices.find(AUDIO_DEVICE_NONE) != tx_devices.end() ||
        rx_devices.find(AUDIO_DEVICE_NONE) != rx_devices.end()) {
        AHAL_ERR("Invalid Tx/Rx device");
        ret = 0;
        goto exit;
    }

    device_count = tx_devices.size() > rx_devices.size() ? tx_devices.size() : rx_devices.size();

    pal_device_ids = (pal_device_id_t *)calloc(1, device_count * sizeof(pal_device_id_t));
    if (!pal_device_ids) {
        AHAL_ERR("fail to allocate memory for pal device array");
        ret = -ENOMEM;
        goto exit;
    }

    AHAL_DBG("Routing is %d", AudioExtn::get_device_types(rx_devices));

    if (stream_out_primary_) {
        stream_out_primary_->getPalDeviceIds(rx_devices, pal_device_ids);
        pal_rx_device = pal_device_ids[0];
        memset(pal_device_ids, 0, device_count * sizeof(pal_device_id_t));
        stream_out_primary_->getPalDeviceIds(tx_devices, pal_device_ids);
        pal_tx_device = pal_device_ids[0];
    }

    pal_voice_rx_device_id_ = pal_rx_device;
    pal_voice_tx_device_id_ = pal_tx_device;

    voice_mutex_.lock();
    if (!IsAnyCallActive()) {
        if (mode_ == AUDIO_MODE_IN_CALL || mode_ == AUDIO_MODE_CALL_SCREEN) {
            voice_.in_call = true;
            ret = UpdateCalls(voice_.session);
        }
    } else {
        //do device switch here
        for (int i = 0; i < max_voice_sessions_; i++) {
             ret = VoiceSetDevice(&voice_.session[i]);
             if (ret)
                 AHAL_ERR("Device switch failed for session[%d]", i);
        }
    }
    voice_mutex_.unlock();

    free(pal_device_ids);
exit:
    AHAL_DBG("Exit ret: %d", ret);
    return ret;
}

int AudioVoice::UpdateCallState(uint32_t vsid, int call_state) {
    voice_session_t *session = NULL;
    int i, ret = 0;
    bool is_call_active;


    for (i = 0; i < max_voice_sessions_; i++) {
        if (vsid == voice_.session[i].vsid) {
            session = &voice_.session[i];
            break;
        }
    }

    voice_mutex_.lock();
    if (session) {
        session->state.new_ = call_state;
        is_call_active = IsCallActive(session);
        AHAL_DBG("is_call_active:%d in_call:%d, mode:%d",
                 is_call_active, voice_.in_call, mode_);
        if (is_call_active ||
                (voice_.in_call && (mode_ == AUDIO_MODE_IN_CALL || mode_ == AUDIO_MODE_CALL_SCREEN))) {
            ret = UpdateCalls(voice_.session);
        }
    } else {
        ret = -EINVAL;
    }
    voice_mutex_.unlock();

    return ret;
}

int AudioVoice::UpdateCalls(voice_session_t *pSession) {
    int i, ret = 0;
    voice_session_t *session = NULL;

    for (i = 0; i < max_voice_sessions_; i++) {
        session = &pSession[i];
        AHAL_DBG("cur_state=%d new_state=%d vsid=%x",
                 session->state.current_, session->state.new_, session->vsid);

        switch(session->state.new_)
        {
        case CALL_ACTIVE:
            switch(session->state.current_)
            {
            case CALL_INACTIVE:
                AHAL_DBG("INACTIVE -> ACTIVE vsid:%x", session->vsid);
                ret = VoiceStart(session);
                if (ret < 0) {
                    AHAL_ERR("VoiceStart() failed");
                } else {
                    session->state.current_ = session->state.new_;
                }
                break;
            default:
                AHAL_ERR("CALL_ACTIVE cannot be handled in state=%d vsid:%x",
                          session->state.current_, session->vsid);
                break;
            }
            break;

        case CALL_INACTIVE:
            switch(session->state.current_)
            {
            case CALL_ACTIVE:
                AHAL_DBG("ACTIVE -> INACTIVE vsid:%x", session->vsid);
                ret = VoiceStop(session);
                if (ret < 0) {
                    AHAL_ERR("VoiceStop() failed");
                } else {
                    session->state.current_ = session->state.new_;
                }
                break;

            default:
                AHAL_ERR("CALL_INACTIVE cannot be handled in state=%d vsid:%x",
                         session->state.current_, session->vsid);
                break;
            }
            break;
        default:
            break;
        } //end out switch loop
    } //end for loop

    return ret;
}

int AudioVoice::StopCall() {
    int i, ret = 0;

    AHAL_DBG("Enter");
    voice_.in_call = false;
    for (i = 0; i < max_voice_sessions_; i++)
        voice_.session[i].state.new_ = CALL_INACTIVE;
    ret = UpdateCalls(voice_.session);
    AHAL_DBG("Exit ret: %d", ret);
    return ret;
}

bool AudioVoice::IsCallActive(AudioVoice::voice_session_t *pSession) {

    return (pSession->state.current_ != CALL_INACTIVE) ? true : false;
}

bool AudioVoice::IsAnyCallActive()
{
    int i;

    for (i = 0; i < max_voice_sessions_; i++) {
        if (IsCallActive(&voice_.session[i]))
            return true;
    }

    return false;
}

/*M55 code for P240105-05058 by hujincan at 2023/01/16 start*/
void AudioVoice::SetPAMute(bool mute) {
    pal_param_speaker_status_t param_speaker_status;
    AHAL_INFO("mute %d", mute);
    if (mute) {
        param_speaker_status.mute_status = PAL_DEVICE_EACH_SPEAKER_MUTE;
        pal_set_param(PAL_PARAM_ID_SPEAKER_STATUS,
            (void*)&param_speaker_status, sizeof(pal_param_speaker_status_t));
    } else {
        param_speaker_status.mute_status = PAL_DEVICE_EACH_SPEAKER_UNMUTE;
        pal_set_param(PAL_PARAM_ID_SPEAKER_STATUS,
            (void*)&param_speaker_status, sizeof(pal_param_speaker_status_t));
    }
}

void SetPaUnMute() {
    pal_param_speaker_status_t param_speaker_status;
    AHAL_INFO("Set PA Unmute");
    param_speaker_status.mute_status = PAL_DEVICE_EACH_SPEAKER_UNMUTE;
    pal_set_param(PAL_PARAM_ID_SPEAKER_STATUS,
        (void*)&param_speaker_status, sizeof(pal_param_speaker_status_t));
}
/*M55 code for P240105-05058 by hujincan at 2023/01/16 end*/

int AudioVoice::VoiceStart(voice_session_t *session) {
    int ret;
    struct pal_stream_attributes streamAttributes;
    std::shared_ptr<AudioDevice> adevice = AudioDevice::GetInstance();
    struct pal_device palDevices[2];
    struct pal_channel_info out_ch_info = {0, {0}}, in_ch_info = {0, {0}};
    pal_param_payload *param_payload = nullptr;

    if (!session) {
        AHAL_ERR("Invalid session");
        return -EINVAL;
    }
    /*M55 code for P240126-04225 by tangjie at 2023/02/23 start*/
    if (pal_voice_rx_device_id_ == PAL_DEVICE_OUT_HANDSET) {
        int PaMuteDelayMs = 150000;
        SetPAMute(true);
        signal(SIGALRM, (__sighandler_t)SetPaUnMute);
        struct itimerval next_alarm_usec = {{0, 0}, {0, PaMuteDelayMs}};
        struct itimerval *old = NULL;
        setitimer(ITIMER_REAL, &next_alarm_usec, old);
    }
    /*M55 code for P240126-04225 by tangjie at 2023/02/23 end*/
    AHAL_DBG("Enter");

    in_ch_info.channels = 1;
    in_ch_info.ch_map[0] = PAL_CHMAP_CHANNEL_FL;

    out_ch_info.channels = 2;
    out_ch_info.ch_map[0] = PAL_CHMAP_CHANNEL_FL;
    out_ch_info.ch_map[1] = PAL_CHMAP_CHANNEL_FR;

    palDevices[0].id = pal_voice_tx_device_id_;
    palDevices[0].config.ch_info = in_ch_info;
    palDevices[0].config.sample_rate = 48000;
    palDevices[0].config.bit_width = CODEC_BACKEND_DEFAULT_BIT_WIDTH;
    palDevices[0].config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE; // TODO: need to convert this from output format
    palDevices[0].address.card_id = adevice->usb_card_id_;
    palDevices[0].address.device_num =adevice->usb_dev_num_;

    palDevices[1].id = pal_voice_rx_device_id_;
    palDevices[1].config.ch_info = out_ch_info;
    palDevices[1].config.sample_rate = 48000;
    palDevices[1].config.bit_width = CODEC_BACKEND_DEFAULT_BIT_WIDTH;
    palDevices[1].config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE; // TODO: need to convert this from output format
    palDevices[1].address.card_id = adevice->usb_card_id_;
    palDevices[1].address.device_num = adevice->usb_dev_num_;

    memset(&streamAttributes, 0, sizeof(streamAttributes));
    streamAttributes.type = PAL_STREAM_VOICE_CALL;
    streamAttributes.info.voice_call_info.VSID = session->vsid;
    streamAttributes.info.voice_call_info.tty_mode = session->tty_mode;
    /*device overrides for specific use cases*/
    if (mode_ == AUDIO_MODE_CALL_SCREEN) {
        AHAL_DBG("in call screen mode");
        palDevices[0].id = PAL_DEVICE_IN_PROXY;  //overwrite the device with proxy dev
        palDevices[1].id = PAL_DEVICE_OUT_PROXY;  //overwrite the device with proxy dev
    }
    if (streamAttributes.info.voice_call_info.tty_mode == PAL_TTY_HCO) {
        /**  device pairs for HCO usecase
          *  <handset, headset-mic>
          *  <speaker, headset-mic>
          *  override devices accordingly.
          */
        if (pal_voice_rx_device_id_ == PAL_DEVICE_OUT_WIRED_HEADSET)
            palDevices[1].id = PAL_DEVICE_OUT_HANDSET;
        else if (pal_voice_rx_device_id_ == PAL_DEVICE_OUT_SPEAKER)
            palDevices[0].id = PAL_DEVICE_IN_WIRED_HEADSET;
        else
            AHAL_ERR("Invalid device pair for the usecase");
    }
    if (streamAttributes.info.voice_call_info.tty_mode == PAL_TTY_VCO) {
        /**  device pairs for VCO usecase
          *  <headphones, handset-mic>
          *  <headphones, speaker-mic>
          *  override devices accordingly.
          */
        if (pal_voice_rx_device_id_ == PAL_DEVICE_OUT_WIRED_HEADSET ||
            pal_voice_rx_device_id_ == PAL_DEVICE_OUT_WIRED_HEADPHONE)
            palDevices[0].id = PAL_DEVICE_IN_HANDSET_MIC;
        else if (pal_voice_rx_device_id_ == PAL_DEVICE_OUT_SPEAKER)
            palDevices[1].id = PAL_DEVICE_OUT_WIRED_HEADSET;
        else
            AHAL_ERR("Invalid device pair for the usecase");
    }
#ifdef SEC_AUDIO_CALL
    SetCustomKey(adevice, palDevices);
#endif
    streamAttributes.direction = PAL_AUDIO_INPUT_OUTPUT;
    streamAttributes.in_media_config.sample_rate = 48000;
    streamAttributes.in_media_config.ch_info = in_ch_info;
    streamAttributes.in_media_config.bit_width = CODEC_BACKEND_DEFAULT_BIT_WIDTH;
    streamAttributes.in_media_config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE; // TODO: need to convert this from output format
    streamAttributes.out_media_config.sample_rate = 48000;
    streamAttributes.out_media_config.ch_info = out_ch_info;
    streamAttributes.out_media_config.bit_width = CODEC_BACKEND_DEFAULT_BIT_WIDTH;
    streamAttributes.out_media_config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE; // TODO: need to convert this from output format

    /*set custom key for hac mode*/
    if (session && session->hac && palDevices[1].id ==
        PAL_DEVICE_OUT_HANDSET) {
        strlcpy(palDevices[0].custom_config.custom_key, "HAC",
                    sizeof(palDevices[0].custom_config.custom_key));
        strlcpy(palDevices[1].custom_config.custom_key, "HAC",
                    sizeof(palDevices[1].custom_config.custom_key));
        AHAL_INFO("Setting custom key as %s", palDevices[0].custom_config.custom_key);
    }
    /*M55 code for QN6887A-1485 by zhouchenghua at 2023/11/30 start*/
    if (session && session->bt_nrec_voice_on && (palDevices[1].id == PAL_DEVICE_OUT_BLUETOOTH_SCO)) {
        strlcpy(palDevices[0].custom_config.custom_key, "bt_nrec_on",
                    sizeof(palDevices[0].custom_config.custom_key));
        strlcpy(palDevices[1].custom_config.custom_key, "bt_nrec_on",
                    sizeof(palDevices[1].custom_config.custom_key));
        AHAL_DBG("VoiceStart Setting custom key as %s", palDevices[0].custom_config.custom_key);
    }
    /*M55 code for QN6887A-1485 by zhouchenghua at 2023/11/30 end*/
    /*M55 code for QN6887A-5008 by hujincan at 2024/01/18 start*/
    if (session && adevice->usb_out_headset && !adevice->usb_input_dev_enabled &&
        (palDevices[1].id == PAL_DEVICE_OUT_USB_HEADSET)) {
        strlcpy(palDevices[0].custom_config.custom_key, "3pole-usb-voice",
                    sizeof(palDevices[0].custom_config.custom_key));
        strlcpy(palDevices[1].custom_config.custom_key, "3pole-usb-voice",
                    sizeof(palDevices[1].custom_config.custom_key));
        AHAL_INFO("VoiceStart Setting custom key as %s", palDevices[0].custom_config.custom_key);
    }
    /*M55 code for QN6887A-5008 by hujincan at 2024/01/18 end*/
    //streamAttributes.in_media_config.ch_info = ch_info;
    ret = pal_stream_open(&streamAttributes,
                          2,
                          palDevices,
                          0,
                          NULL,
                          NULL,//callback
                          (uint64_t)this,
                          &session->pal_voice_handle);// Need to add this to the audio stream structure.

    AHAL_DBG("pal_stream_open() ret:%d", ret);
    if (ret) {
        AHAL_ERR("Pal Stream Open Error (%x)", ret);
        ret = -EINVAL;
        goto error_open;
    }

    /*apply cached voice effects features*/
    if (session->slow_talk) {
        param_payload = (pal_param_payload *)calloc(1, sizeof(pal_param_payload) +
                                             sizeof(session->slow_talk));
        if (!param_payload) {
            AHAL_ERR("calloc for size %zu failed",
                   sizeof(pal_param_payload) + sizeof(session->slow_talk));
        } else {
            param_payload->payload_size = sizeof(session->slow_talk);
            param_payload->payload[0] = session->slow_talk;
            ret = pal_stream_set_param(session->pal_voice_handle,
                                       PAL_PARAM_ID_SLOW_TALK,
                                       param_payload);
            if (ret)
                AHAL_ERR("Slow Talk enable failed %x", ret);
            free(param_payload);
            param_payload = nullptr;
        }
    }

    if (session->volume_boost) {
        param_payload = (pal_param_payload *)calloc(1, sizeof(pal_param_payload) +
                                             sizeof(session->volume_boost));
        if (!param_payload) {
            AHAL_ERR("calloc for size %zu failed",
                  sizeof(pal_param_payload) + sizeof(session->volume_boost));
        } else {
            param_payload->payload_size = sizeof(session->volume_boost);
            param_payload->payload[0] = session->volume_boost;
            ret = pal_stream_set_param(session->pal_voice_handle, PAL_PARAM_ID_VOLUME_BOOST,
                                   param_payload);
            if (ret)
                AHAL_ERR("Volume Boost enable failed %x", ret);
            free(param_payload);
            param_payload = nullptr;
        }
    }

    if (session->hd_voice) {
        param_payload = (pal_param_payload *)calloc(1, sizeof(pal_param_payload) +
                                             sizeof(session->hd_voice));
        if (!param_payload) {
            AHAL_ERR("calloc for size %zu failed",
                     sizeof(pal_param_payload) + sizeof(session->hd_voice));
        } else {
            param_payload->payload_size = sizeof(session->hd_voice);
            param_payload->payload[0] = session->hd_voice;
            ret = pal_stream_set_param(session->pal_voice_handle, PAL_PARAM_ID_HD_VOICE,
                                   param_payload);
            if (ret)
                AHAL_ERR("HD Voice enable failed %x",ret);
            free(param_payload);
            param_payload = nullptr;
        }
    }

    /* apply cached volume set by APM */
    if (session->pal_voice_handle && session->pal_vol_data &&
        session->pal_vol_data->volume_pair[0].vol != -1.0) {
        ret = pal_stream_set_volume(session->pal_voice_handle, session->pal_vol_data);
        if (ret)
            AHAL_ERR("Failed to apply volume on voice session %x", ret);
    } else {
        if (!session->pal_voice_handle || !session->pal_vol_data)
            AHAL_ERR("Invalid voice handle or volume data");
        if (session->pal_vol_data && session->pal_vol_data->volume_pair[0].vol == -1.0)
            AHAL_DBG("session volume is not set");
    }

   ret = pal_stream_start(session->pal_voice_handle);
   if (ret) {
       AHAL_ERR("Pal Stream Start Error (%x)", ret);
       ret = pal_stream_close(session->pal_voice_handle);
       if (ret)
           AHAL_ERR("Pal Stream close failed %x", ret);
           session->pal_voice_handle = NULL;
           ret = -EINVAL;
   } else {
      AHAL_DBG("Pal Stream Start Success");
   }

   /*Apply device mute if needed*/
   if (session->device_mute.mute) {
        ret = SetDeviceMute(session);
   }

   /*apply cached mic mute*/
   if (adevice->mute_) {
       pal_stream_set_mute(session->pal_voice_handle, adevice->mute_);
   }


error_open:
    AHAL_DBG("Exit ret: %d", ret);
    return ret;
}

int AudioVoice::SetDeviceMute(voice_session_t *session) {
    int ret = 0;
    pal_param_payload *param_payload = nullptr;

    if (!session) {
        AHAL_ERR("Invalid Session");
        return -EINVAL;
    }

    param_payload = (pal_param_payload *)calloc(1, sizeof(pal_param_payload) +
                        sizeof(session->device_mute));
    if (!param_payload) {
        AHAL_ERR("calloc failed for size %zu",
                sizeof(pal_param_payload) + sizeof(session->device_mute));
        ret = -EINVAL;
    } else {
        param_payload->payload_size = sizeof(session->device_mute);
        memcpy(param_payload->payload, &(session->device_mute), param_payload->payload_size);
        ret = pal_stream_set_param(session->pal_voice_handle, PAL_PARAM_ID_DEVICE_MUTE,
                                param_payload);
        if (ret)
            AHAL_ERR("Voice Device mute failed %x", ret);
        free(param_payload);
        param_payload = nullptr;
    }

   AHAL_DBG("Exit ret: %d", ret);
   return ret;
}


int AudioVoice::VoiceStop(voice_session_t *session) {
    int ret = 0;

    AHAL_DBG("Enter");
    if (session && session->pal_voice_handle) {
        ret = pal_stream_stop(session->pal_voice_handle);
        if (ret)
            AHAL_ERR("Pal Stream stop failed %x", ret);
        ret = pal_stream_close(session->pal_voice_handle);
        if (ret)
            AHAL_ERR("Pal Stream close failed %x", ret);
        session->pal_voice_handle = NULL;
    }

    /*M55 code for P240105-05058 by hujincan at 2023/01/16 start*/
    SetPAMute(false);
    /*M55 code for P240105-05058 by hujincan at 2023/01/16 end*/

    if (ret)
        ret = -EINVAL;
    AHAL_DBG("Exit ret: %d", ret);
    return ret;
}

int AudioVoice::VoiceSetDevice(voice_session_t *session) {
    int ret = 0;
    struct pal_device palDevices[2];
    struct pal_channel_info out_ch_info = {0, {0}}, in_ch_info = {0, {0}};
    std::shared_ptr<AudioDevice> adevice = AudioDevice::GetInstance();
    pal_param_payload *param_payload = nullptr;

    if (!session) {
        AHAL_ERR("Invalid session");
        return -EINVAL;
    }

    AHAL_DBG("Enter");
    in_ch_info.channels = 1;
    in_ch_info.ch_map[0] = PAL_CHMAP_CHANNEL_FL;

    out_ch_info.channels = 2;
    out_ch_info.ch_map[0] = PAL_CHMAP_CHANNEL_FL;
    out_ch_info.ch_map[1] = PAL_CHMAP_CHANNEL_FR;

    palDevices[0].id = pal_voice_tx_device_id_;
    palDevices[0].config.ch_info = in_ch_info;
    palDevices[0].config.sample_rate = 48000;
    palDevices[0].config.bit_width = CODEC_BACKEND_DEFAULT_BIT_WIDTH;
    palDevices[0].config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE; // TODO: need to convert this from output format
    palDevices[0].address.card_id = adevice->usb_card_id_;
    palDevices[0].address.device_num =adevice->usb_dev_num_;

    palDevices[1].id = pal_voice_rx_device_id_;
    palDevices[1].config.ch_info = out_ch_info;
    palDevices[1].config.sample_rate = 48000;
    palDevices[1].config.bit_width = CODEC_BACKEND_DEFAULT_BIT_WIDTH;
    palDevices[1].config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE; // TODO: need to convert this from output format
    palDevices[1].address.card_id = adevice->usb_card_id_;
    palDevices[1].address.device_num =adevice->usb_dev_num_;
    /*device overwrites for usecases*/
    if (mode_ == AUDIO_MODE_CALL_SCREEN) {
        AHAL_DBG("in call screen mode");
        palDevices[0].id = PAL_DEVICE_IN_PROXY;  //overwrite the device with proxy dev
        palDevices[1].id = PAL_DEVICE_OUT_PROXY;  //overwrite the device with proxy dev
    }

    if (session && session->tty_mode == PAL_TTY_HCO) {
        /**  device pairs for HCO usecase
          *  <handset, headset-mic>
          *  <speaker, headset-mic>
          *  override devices accordingly.
          */
        if (pal_voice_rx_device_id_ == PAL_DEVICE_OUT_WIRED_HEADSET)
            palDevices[1].id = PAL_DEVICE_OUT_HANDSET;
        else if (pal_voice_rx_device_id_ == PAL_DEVICE_OUT_SPEAKER)
            palDevices[0].id = PAL_DEVICE_IN_WIRED_HEADSET;
        else
            AHAL_ERR("Invalid device pair for the usecase");
    }
    if (session && session->tty_mode == PAL_TTY_VCO) {
        /**  device pairs for VCO usecase
          *  <headphones, handset-mic>
          *  <headphones, speaker-mic>
          *  override devices accordingly.
          */
        if (pal_voice_rx_device_id_ == PAL_DEVICE_OUT_WIRED_HEADSET ||
            pal_voice_rx_device_id_ == PAL_DEVICE_OUT_WIRED_HEADPHONE)
            palDevices[0].id = PAL_DEVICE_IN_HANDSET_MIC;
        else if (pal_voice_rx_device_id_ == PAL_DEVICE_OUT_SPEAKER)
            palDevices[1].id = PAL_DEVICE_OUT_WIRED_HEADSET;
        else
            AHAL_ERR("Invalid device pair for the usecase");
    }

    if (session && session->volume_boost) {
            /* volume boost if device is not supported */
            param_payload = (pal_param_payload *)calloc(1, sizeof(pal_param_payload) +
                                               sizeof(session->volume_boost));
            if (!param_payload) {
                AHAL_ERR("calloc for size %zu failed",
                     sizeof(pal_param_payload) + sizeof(session->volume_boost));
            } else {
                param_payload->payload_size = sizeof(session->volume_boost);
                if (palDevices[1].id != PAL_DEVICE_OUT_HANDSET &&
                    palDevices[1].id != PAL_DEVICE_OUT_SPEAKER)
                    param_payload->payload[0] = false;
                else
                    param_payload->payload[0] = true;
                ret = pal_stream_set_param(session->pal_voice_handle, PAL_PARAM_ID_VOLUME_BOOST,
                                           param_payload);
                if (ret)
                    AHAL_ERR("Volume Boost enable/disable failed %x", ret);
                free(param_payload);
                param_payload = nullptr;
            }
    }
    /*set or remove custom key for hac mode*/
    if (session && session->hac && palDevices[1].id ==
        PAL_DEVICE_OUT_HANDSET) {
        strlcpy(palDevices[0].custom_config.custom_key, "HAC",
                    sizeof(palDevices[0].custom_config.custom_key));
        strlcpy(palDevices[1].custom_config.custom_key, "HAC",
                    sizeof(palDevices[1].custom_config.custom_key));
            AHAL_INFO("Setting custom key as %s", palDevices[0].custom_config.custom_key);
    } else {
        strlcpy(palDevices[0].custom_config.custom_key, "",
                sizeof(palDevices[0].custom_config.custom_key));
    }
#ifdef SEC_AUDIO_CALL
    SetCustomKey(adevice, palDevices);
#endif
    /*M55 code for QN6887A-1485 by zhouchenghua at 2023/11/30 start*/
    if (session && session->bt_nrec_voice_on && (palDevices[1].id == PAL_DEVICE_OUT_BLUETOOTH_SCO)) {
        strlcpy(palDevices[0].custom_config.custom_key, "bt_nrec_on",
                    sizeof(palDevices[0].custom_config.custom_key));
        strlcpy(palDevices[1].custom_config.custom_key, "bt_nrec_on",
                    sizeof(palDevices[1].custom_config.custom_key));
        AHAL_DBG("VoiceSetDevice Setting custom key as %s  bt_nrec_voice_on=%d   palDevices[1].id=%d \n", palDevices[0].custom_config.custom_key,session->bt_nrec_voice_on,palDevices[1].id);
    }
    /*M55 code for QN6887A-1485 by zhouchenghua at 2023/11/30 end*/
    /*M55 code for QN6887A-5008 by hujincan at 2024/01/18 start*/
    if (session && adevice->usb_out_headset && !adevice->usb_input_dev_enabled &&
        (palDevices[1].id == PAL_DEVICE_OUT_USB_HEADSET)) {
        strlcpy(palDevices[0].custom_config.custom_key, "3pole-usb-voice",
                    sizeof(palDevices[0].custom_config.custom_key));
        strlcpy(palDevices[1].custom_config.custom_key, "3pole-usb-voice",
                    sizeof(palDevices[1].custom_config.custom_key));
        AHAL_INFO("VoiceSetDevice Setting custom key as %s", palDevices[0].custom_config.custom_key);
    }
    /*M55 code for QN6887A-5008 by hujincan at 2024/01/18 end*/
    /*M55 code for P240105-05058 by hujincan at 2024/01/26 start*/
    if (palDevices[1].id == PAL_DEVICE_OUT_HANDSET && adevice->rx_device_switch) {
        int PaMuteDelayMs = 150000;
        SetPAMute(true);
        if (adevice->bt_sco_switch == BT_SCO_SWITCH_HANDSET) {
            PaMuteDelayMs = 600000;
        } else if (adevice->bt_sco_switch == BT_SCO_SWITCH_BT) {
            PaMuteDelayMs = 990000;
        }
        signal(SIGALRM, (__sighandler_t)SetPaUnMute);
        struct itimerval next_alarm_usec = {{0, 0}, {0, PaMuteDelayMs}};
        struct itimerval *old = NULL;
        setitimer(ITIMER_REAL, &next_alarm_usec, old);
        adevice->rx_device_switch = false;
    }
    /*M55 code for P240105-05058 by hujincan at 2024/01/26 end*/

    if (session && session->pal_voice_handle) {
        ret = pal_stream_set_device(session->pal_voice_handle, 2, palDevices);
        if (ret) {
            AHAL_ERR("Pal Stream Set Device failed %x", ret);
            ret = -EINVAL;
            goto exit;
        }
    } else {
        AHAL_ERR("Voice handle not found");
    }

    /* apply device mute if needed*/
    if (session->device_mute.mute) {
        ret = SetDeviceMute(session);
   }

exit:
    AHAL_DBG("Exit ret: %d", ret);
    return ret;
}

int AudioVoice::SetMicMute(bool mute) {
    int ret = 0;
    voice_session_t *session = voice_.session;

    AHAL_DBG("Enter mute: %d", mute);
    if (session) {
        for (int i = 0; i < max_voice_sessions_; i++) {
            if (session[i].pal_voice_handle) {
                ret = pal_stream_set_mute(session[i].pal_voice_handle, mute);
                if (ret)
                    AHAL_ERR("Error applying mute %d for voice session %d", mute, i);
            }
        }
    }
    AHAL_DBG("Exit ret: %d", ret);
    return ret;

}

int AudioVoice::SetVoiceVolume(float volume) {
    int ret = 0;
    voice_session_t *session = voice_.session;

    AHAL_DBG("Enter vol: %f", volume);
    if (session) {

        for (int i = 0; i < max_voice_sessions_; i++) {
            /* APM volume is cached when voice call is not active
             * cached volume is applied in voicestart before pal_stream_start
             */
            if (session[i].pal_vol_data) {
                session[i].pal_vol_data->volume_pair[0].vol = volume;
                if (session[i].pal_voice_handle) {
                    ret = pal_stream_set_volume(session[i].pal_voice_handle,
                            session[i].pal_vol_data);
                    AHAL_DBG("volume applied on voice session %d status %x", i, ret);
                } else {
                    AHAL_DBG("volume is cached on voice session %d", i);
                }
            } else {
                AHAL_ERR("unable to apply/cache volume on voice session %d", i);
            }
        }
    }
    AHAL_DBG("Exit ret: %d", ret);
    return ret;
}

AudioVoice::AudioVoice() {

    voice_.in_call = false;
    max_voice_sessions_ = MAX_VOICE_SESSIONS;
    pal_vol_ = NULL;
    pal_vol_ = (struct pal_volume_data*)malloc(sizeof(uint32_t)
        + sizeof(struct pal_channel_vol_kv));
    if (pal_vol_) {
        pal_vol_->no_of_volpair = 1;
        pal_vol_->volume_pair[0].channel_mask = 0x01;
        pal_vol_->volume_pair[0].vol = -1.0;
    } else {
        AHAL_ERR("volume malloc failed %s", strerror(errno));
    }

    for (int i = 0; i < max_voice_sessions_; i++) {
        voice_.session[i].state.current_ = CALL_INACTIVE;
        voice_.session[i].state.new_ = CALL_INACTIVE;
        voice_.session[i].vsid = VOICEMMODE1_VSID;
        voice_.session[i].pal_voice_handle = NULL;
        voice_.session[i].tty_mode = PAL_TTY_OFF;
        voice_.session[i].volume_boost = false;
        voice_.session[i].slow_talk = false;
        voice_.session[i].pal_voice_handle = NULL;
        voice_.session[i].hd_voice = false;
        voice_.session[i].pal_vol_data = pal_vol_;
        voice_.session[i].device_mute.dir = PAL_AUDIO_OUTPUT;
        voice_.session[i].device_mute.mute = false;
        voice_.session[i].hac = false;
        /*M55 code for QN6887A-1485 by zhouchenghua at 2023/11/30 start*/
        voice_.session[i].bt_nrec_voice_on = false;
        /*M55 code for QN6887A-1485 by zhouchenghua at 2023/11/30 end*/
    }

    voice_.session[MMODE1_SESS_IDX].vsid = VOICEMMODE1_VSID;
    voice_.session[MMODE2_SESS_IDX].vsid = VOICEMMODE2_VSID;

    stream_out_primary_ = NULL;

#ifdef SEC_AUDIO_CALL_VOIP
#ifdef SEC_AUDIO_SUPPORT_VOIP_MICMODE_DEFAULT
    aosp_voip_call_isolation_mode = EFFECTS_MICMODE_DEFAULT;
#else
    aosp_voip_call_isolation_mode = EFFECTS_MICMODE_STANDARD;
#endif
    wifi_real_call_isolation_mode = EFFECTS_MICMODE_STANDARD;
#endif
}

AudioVoice::~AudioVoice() {

    voice_.in_call = false;
    if (pal_vol_)
        free(pal_vol_);

    for (int i = 0; i < max_voice_sessions_; i++) {
        voice_.session[i].state.current_ = CALL_INACTIVE;
        voice_.session[i].state.new_ = CALL_INACTIVE;
        voice_.session[i].vsid = VOICEMMODE1_VSID;
        voice_.session[i].tty_mode = PAL_TTY_OFF;
        voice_.session[i].volume_boost = false;
        voice_.session[i].slow_talk = false;
        voice_.session[i].pal_voice_handle = NULL;
        voice_.session[i].hd_voice = false;
        voice_.session[i].pal_vol_data = NULL;
        voice_.session[i].hac = false;
        /*M55 code for QN6887A-1485 by zhouchenghua at 2023/11/30 start*/
        voice_.session[i].bt_nrec_voice_on = false;
        /*M55 code for QN6887A-1485 by zhouchenghua at 2023/11/30 end*/
    }

    voice_.session[MMODE1_SESS_IDX].vsid = VOICEMMODE1_VSID;
    voice_.session[MMODE2_SESS_IDX].vsid = VOICEMMODE2_VSID;

    stream_out_primary_ = NULL;
    max_voice_sessions_ = 0;
}

#ifdef SEC_AUDIO_CALL
void AudioVoice::SetCustomKey(std::shared_ptr<AudioDevice> adevice, struct pal_device *palDevices) {
    int ck_id = CUSTOM_KEY_INVALID;
    int key_dir = PAL_TX;

    bool is_focus_mode = 
#ifdef SEC_AUDIO_SUPPORT_CALL_MICMODE
        (wifi_real_call_isolation_mode == EFFECTS_MICMODE_VOICE_FOCUS);
#else
        false;
#endif

    for (int i = 0; i < 2; i++) {
        memset(palDevices[i].custom_config.custom_key, 0, PAL_MAX_CUSTOM_KEY_SIZE);

        /* Case 1: call tx
        */
        if (palDevices[i].id > PAL_DEVICE_IN_MIN) {
            key_dir = PAL_TX;
            if(adevice->call_type_volte_video) {
                if (palDevices[i].id == PAL_DEVICE_IN_SPEAKER_MIC) {
                    if (is_focus_mode) {
                        ck_id = CUSTOM_KEY_VOLTE_VIDEO_CALL_WITH_VF;
                    } else {
                        ck_id = CUSTOM_KEY_VOLTE_VIDEO_CALL;
                    }
                }
            } else if(adevice->call_type_gsm3g_voice) {
                if (palDevices[i].id == PAL_DEVICE_IN_HANDSET_MIC ||
                    palDevices[i].id == PAL_DEVICE_IN_SPEAKER_MIC) {
                    if (is_focus_mode) {
                        ck_id = CUSTOM_KEY_GSM3G_VOICE_CALL_WITH_VF;
                    } else {
                        ck_id = CUSTOM_KEY_GSM3G_VOICE_CALL;
                    }
                }
            } else {
                if (palDevices[i].id == PAL_DEVICE_IN_HANDSET_MIC ||
                    palDevices[i].id == PAL_DEVICE_IN_SPEAKER_MIC) {
                    if (is_focus_mode) {
                        ck_id = CUSTOM_KEY_VOLTE_VOICE_CALL_WITH_VF;
                    } else {
                        /*M55 code for P240103-06199 by hujincan at 2023/01/14 start*/
                        ck_id = CUSTOM_KEY_VOLTE_VOICE_CALL;
                        /*M55 code for P240103-06199 by hujincan at 2023/01/14 end*/
                    }
                }
            }
        }
        /* Case 2: call rx
        */
        else {
            key_dir = PAL_RX;
            if(adevice->call_type_volte_video) {
                if (palDevices[i].id == PAL_DEVICE_OUT_SPEAKER) {
                    if (is_focus_mode) {
                        ck_id = CUSTOM_KEY_VOLTE_VIDEO_CALL_WITH_VF;
                    } else {
                        ck_id = CUSTOM_KEY_VOLTE_VIDEO_CALL;
                    }
                }
            } else if(adevice->call_type_gsm3g_voice) {
                if (palDevices[i].id == PAL_DEVICE_OUT_HANDSET ||
                    palDevices[i].id == PAL_DEVICE_OUT_SPEAKER) {
                    if (is_focus_mode) {
                        ck_id = CUSTOM_KEY_GSM3G_VOICE_CALL_WITH_VF;
                    } else {
                        ck_id = CUSTOM_KEY_GSM3G_VOICE_CALL;
                    }
                }
            } else {
                if (palDevices[i].id == PAL_DEVICE_OUT_HANDSET ||
                    palDevices[i].id == PAL_DEVICE_OUT_SPEAKER) {
                    if (is_focus_mode) {
                        ck_id = CUSTOM_KEY_VOLTE_VOICE_CALL_WITH_VF;
                    } else {
                        /*M55 code for P240103-06199 by hujincan at 2023/01/14 start*/
                        ck_id = CUSTOM_KEY_VOLTE_VOICE_CALL;
                        /*M55 code for P240103-06199 by hujincan at 2021/01/14 end*/
                    }
                }
            }
        }

        if (ck_id != CUSTOM_KEY_INVALID) {
            strcpy(palDevices[key_dir].custom_config.custom_key, ck_table[ck_id]);
            ck_id = CUSTOM_KEY_INVALID;
            AHAL_INFO("Setting custom key voice_tx: %s voice_rx: %s",
            palDevices[PAL_TX].custom_config.custom_key, palDevices[PAL_RX].custom_config.custom_key);
        }
    }
}
#endif
#ifdef SEC_AUDIO_CALL_VOIP
int AudioVoice::SetVoipMicModeEffect() {
    std::shared_ptr<AudioDevice> adevice = AudioDevice::GetInstance();
    std::shared_ptr<StreamOutPrimary> astream_out = adevice->OutGetStream(PAL_STREAM_VOIP_RX);
    std::shared_ptr<StreamInPrimary> astream_in = adevice->GetActiveInStreamByUseCase(USECASE_AUDIO_RECORD_VOIP);

    int mode = (adevice->call_type_wificalling ? wifi_real_call_isolation_mode : aosp_voip_call_isolation_mode);
    bool isSuppportMicModeEffectDevice = false;

    if (adevice->voice_->mode_ != AUDIO_MODE_IN_COMMUNICATION) {
        return 0;
    }

#ifdef SEC_AUDIO_SUPPORT_CALL_MICMODE
    if (astream_out && astream_out->isDeviceAvailable(PAL_DEVICE_OUT_HANDSET)) {
        isSuppportMicModeEffectDevice = true;
    } else
#endif
    if (astream_out && astream_out->isDeviceAvailable(PAL_DEVICE_OUT_SPEAKER)) {
        isSuppportMicModeEffectDevice = true;
#ifdef SEC_AUDIO_SCREEN_MIRRORING
        isSuppportMicModeEffectDevice = !adevice->voip_via_smart_view;
#endif
    }

    if (isSuppportMicModeEffectDevice
            && astream_out && astream_in) {
        auto it = getVoipMicMode.find(mode);
        if (it == getVoipMicMode.end()) {
#ifdef SEC_AUDIO_SUPPORT_CALL_MICMODE
           mode = EFFECTS_MICMODE_STANDARD;
#else
           mode = EFFECTS_MICMODE_DEFAULT;
#endif
        }
        mode = getVoipMicMode.at(mode);
        astream_out->SetVoipMicModeEffectKvParams(mode);
        astream_in->SetVoipMicModeEffectKvParams(mode);
        AHAL_INFO("set SetVoipMicModeEffect as %d", mode);
    /*M55 code for QN6887A-1679 by hujincan at 2023/12/06 start*/
    } else if (adevice->call_type_wificalling && astream_out && astream_in) {
        mode = EFFECTS_MICMODE_STANDARD;
        mode = getVoipMicMode.at(mode);
        astream_out->SetVoipMicModeEffectKvParams(mode);
        astream_in->SetVoipMicModeEffectKvParams(mode);
        AHAL_INFO("set vowifi nb/wb/swb for headset/bt/usb");
    }
    /*M55 code for QN6887A-1679 by hujincan at 2023/12/06 end*/

    return 0;
}
#endif
