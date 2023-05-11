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
#include <audio_extn/AudioExtn.h>

#ifndef AUDIO_MODE_CALL_SCREEN
#define AUDIO_MODE_CALL_SCREEN 4
#endif


int AudioVoice::SetMode(const audio_mode_t mode) {
    int ret = 0;
#ifdef SEC_AUDIO_COMMON
    std::shared_ptr<AudioDevice> adevice = AudioDevice::GetInstance();
#endif

    AHAL_DBG("Enter: mode: %d", mode);
    if (mode_ != mode) {
#ifdef SEC_AUDIO_CALL
        if (mode == AUDIO_MODE_IN_CALL) {
            voice_session_t *session = NULL;
            for (int i = 0; i < max_voice_sessions_; i++) {
                if (sec_voice_->cur_vsid == voice_.session[i].vsid) {
                    session = &voice_.session[i];
                    break;
                }
            }
            if (session) {
                session->state.new_ = CALL_ACTIVE;
                AHAL_DBG("new state is ACTIVE for vsid:%x", session->vsid);
            }
#ifdef SEC_AUDIO_RECOVERY
            if (adevice->factory_ &&
                    (adevice->factory_->factory.state != FACTORY_TEST_INACTIVE)) {
                adevice->factory_->InitState();
                AHAL_DBG("initialize factory state before starting call");
            }
#endif
#ifdef SEC_AUDIO_FMRADIO
            if (adevice->sec_device_->fm.on) {
                AHAL_DBG("%s: FM force stop for call", __func__);
                adevice->sec_device_->fm.on = false;
                struct str_parms *fm_parms = str_parms_create();
                str_parms_add_int(fm_parms, AUDIO_PARAMETER_KEY_HANDLE_FM,
                                      (int)(AUDIO_DEVICE_NONE|AUDIO_DEVICE_OUT_SPEAKER));
                AudioExtn::audio_extn_fm_set_parameters(adevice, fm_parms);
            }
#endif
        }
#endif
        /*start a new session for full voice call*/
        if ((mode ==  AUDIO_MODE_CALL_SCREEN && mode_ == AUDIO_MODE_IN_CALL)||
           (mode == AUDIO_MODE_IN_CALL && mode_ == AUDIO_MODE_CALL_SCREEN)){
            mode_ = mode;
            AHAL_DBG("call screen device switch called: %d", mode);
            VoiceSetDevice(voice_.session);
        } else {
#ifdef SEC_AUDIO_CALL_VOIP
            if (mode_ == AUDIO_MODE_IN_COMMUNICATION) {
                std::shared_ptr<StreamOutPrimary> astream_out = adevice->OutGetStream(PAL_STREAM_VOIP_RX);
                if (astream_out && astream_out->HasPalStreamHandle() && sec_voice_->cng_enable)
                    sec_voice_->SetCNGForEchoRefMute(false);
#ifdef SEC_AUDIO_RECOVERY
                if (mode == AUDIO_MODE_NORMAL) {
                    sec_voice_->InitState(__func__);
                }
#endif
            }
#endif
            mode_ = mode;

#ifdef SEC_AUDIO_CALL_VOIP
            if (mode == AUDIO_MODE_IN_COMMUNICATION) {
                // P220608-06439, cp call mute top of the whatsapp call on dual sim model
                if (voice_.in_call && voice_.session->pal_voice_handle == NULL) {
                    ret = StopCall();
                }
                std::shared_ptr<StreamOutPrimary> astream_out = adevice->OutGetStream(PAL_STREAM_VOIP_RX);
                if (astream_out) {
                    if (astream_out->HasPalStreamHandle() && sec_voice_->cng_enable)
                        sec_voice_->SetCNGForEchoRefMute(true);
                    /* duo mt call : voicestream > mode 3, hac custom key setting error */
                    astream_out->ForceRouteStream({AUDIO_DEVICE_NONE});
                }
            }
#endif

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
#ifndef SEC_AUDIO_CALL_HAC
    bool hac;
#endif
#ifdef SEC_AUDIO_CALL
    std::shared_ptr<AudioDevice> adevice = AudioDevice::GetInstance();
#endif

    parms = str_parms_create_str(kvpairs);
    if (!parms)
       return  -EINVAL;

    AHAL_DBG("Enter params: %s", kvpairs);
#ifdef SEC_AUDIO_CALL
    ret = sec_voice_->VoiceSetParameters(adevice, parms);
#endif

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
    }
    err = str_parms_get_str(parms, AUDIO_PARAMETER_KEY_TTY_MODE, c_value, sizeof(c_value));
    if (err >= 0) {
#ifdef SEC_AUDIO_CALL_TTY
        uint32_t vsid = VOICEMMODE1_VSID;
        err = str_parms_get_str(parms, AUDIO_PARAMETER_SUBKEY_TTY_MODE_SIM2, c_value, sizeof(c_value));
        if (err >= 0) {
            vsid = VOICEMMODE2_VSID;
            str_parms_del(parms, AUDIO_PARAMETER_SUBKEY_TTY_MODE_SIM2);
        }
        str_parms_del(parms, AUDIO_PARAMETER_KEY_TTY_MODE);
#endif
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
#ifdef SEC_AUDIO_CALL_TTY
            if (voice_.session[i].vsid == vsid) {
                voice_.session[i].tty_mode = tty_mode;
            }
#else
            voice_.session[i].tty_mode = tty_mode;
#endif
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

#ifndef SEC_AUDIO_CALL_HAC
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
#endif
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
#ifdef SEC_AUDIO_CALL
    std::shared_ptr<AudioDevice> adevice = AudioDevice::GetInstance();
    pal_effect_custom_payload_t *custom_payload = NULL;
#endif
#ifndef SEC_AUDIO_CALL_TTY
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
#endif
#ifdef SEC_AUDIO_CALL
    ret = str_parms_get_str(query, AUDIO_PARAMETER_SEC_GLOBAL_FACTORY_ECHOREF_MUTE_DETECT, value, sizeof(value));
    if (ret >= 0) {
        uint32_t echoref_mute_status = 0;
        uint32_t custom_data_sz = DEFAULT_PARAM_LEN * sizeof(uint32_t);
        custom_payload = (pal_effect_custom_payload_t *) calloc (1,
                                           sizeof(pal_effect_custom_payload_t) +
                                           custom_data_sz);
        if (!custom_payload) {
             AHAL_ERR("calloc failed for size %d", custom_data_sz);
             ret = -ENOMEM;
             return;
        }

        custom_payload->paramId = PARAM_ID_ECHOREF_MUTE_DETECT_PARAM;
        custom_payload->data[0] = echoref_mute_status;

        adevice->effect_->get_custom_payload_effect(TAG_ECHOREF_NO_KEY, custom_payload, custom_data_sz);
        str_parms_add_int(reply, AUDIO_PARAMETER_SEC_GLOBAL_FACTORY_ECHOREF_MUTE_VALUE, custom_payload->data[0]);
        AHAL_INFO("echoref mute detect %d", custom_payload->data[0]);

        free(custom_payload);
        custom_payload = NULL;
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
#ifdef SEC_AUDIO_BLE_OFFLOAD
            case AUDIO_DEVICE_OUT_BLE_HEADSET:
                tx_devices.insert(AUDIO_DEVICE_IN_BLE_HEADSET);
                break;
#endif
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
#ifdef SEC_AUDIO_COMMON
    std::shared_ptr<AudioDevice> adevice = AudioDevice::GetInstance();
#endif

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

#ifdef SEC_AUDIO_SPK_AMP_MUTE
    if (IsAnyCallActive() &&
        (pal_voice_rx_device_id_ == PAL_DEVICE_OUT_SPEAKER) &&
        (pal_voice_rx_device_id_ != pal_rx_device)) {
        adevice->sec_device_->SetSpeakerMute(true);
    }
#endif

    pal_voice_rx_device_id_ = pal_rx_device;
    pal_voice_tx_device_id_ = pal_tx_device;

#ifdef SEC_AUDIO_SUPPORT_REMOTE_MIC
    if (adevice->sec_device_->aas_on) {
        if (adevice->sec_device_->isAASActive() &&
                (adevice->sec_device_->pal_aas_out_device == pal_rx_device)) {
            AHAL_DBG("skip to SetAASMode due to same pal device id %d", pal_rx_device);
        } else {
            adevice->sec_device_->SetAASMode(adevice->sec_device_->aas_on);
        }
    }
#endif

    voice_mutex_.lock();
    if (!IsAnyCallActive()) {
        if (mode_ == AUDIO_MODE_IN_CALL || mode_ == AUDIO_MODE_CALL_SCREEN) {
            voice_.in_call = true;
            ret = UpdateCalls(voice_.session);
#ifdef SEC_AUDIO_CALL_FORWARDING /* || CscFeature_Audio_SupportAutoAnswer */
            if (!sec_voice_->is_shutter_playing) {
                if (sec_voice_->IsCallForwarding()) {
                    // if fwd or call_memo param sent before call setup, enable here
                    sec_voice_->SetCallForwarding(true);
                } else if (sec_voice_->call_memo == CALLMEMO_REC) {
                    // if call_memo rec param sent before call setup, enable here
                    adevice->voice_->SetMicMute(true);
                    sec_voice_->SetVoiceRxMute(true);
                }
#ifdef SEC_AUDIO_VOICE_TX_FOR_INCALL_MUSIC
                else if(sec_voice_->screen_call){
                    sec_voice_->SetVoiceRxMute(true);
                }
#endif
            }
#endif
        }
    } else {
        // do device switch here
        for (int i = 0; i < max_voice_sessions_; i++) {
#ifdef SEC_AUDIO_BLE_OFFLOAD
            {
                /* already in call, and now if BLE is connected send metadata
                 * so that BLE can be configured for call and then switch to
                 * BLE device
                 */
                updateVoiceMetadataForBT(true);
                // dont start the call, until we get suspend from BLE
                std::unique_lock<std::mutex> guard(reconfig_wait_mutex_);
            }
#endif
            ret = VoiceSetDevice(&voice_.session[i]);
            if (ret) {
                AHAL_ERR("Device switch failed for session[%d]", i);
            }
#ifdef SEC_AUDIO_CALL
            else {
                voice_session_t *session = &voice_.session[i];
                if (session && session->pal_voice_handle &&
                        session->pal_vol_data && sec_voice_->volume != -1.0f) {
                    session->pal_vol_data->volume_pair[0].vol = sec_voice_->volume;
#ifdef SEC_AUDIO_SUPPORT_BT_RVC
                    if (adevice->effect_->SetScoVolume(session->pal_vol_data->volume_pair[0].vol) == 0) {
                        AHAL_DBG("sco volume applied on voice session %d", i);
                    } else
#endif
                    ret = pal_stream_set_volume(session->pal_voice_handle,
                            session->pal_vol_data);
                    if (ret)
                        AHAL_ERR("Failed to apply volume on voice session %d, status %x", i, ret);
                }
            }
#endif
        }
    }

#ifdef SEC_AUDIO_SPK_AMP_MUTE
    adevice->sec_device_->SetSpeakerMute(false);
#endif

    voice_mutex_.unlock();
    free(pal_device_ids);
exit:
    AHAL_DBG("Exit ret: %d", ret);
    return ret;
}

#ifdef SEC_AUDIO_BLE_OFFLOAD
bool AudioVoice::get_voice_call_state(audio_mode_t *mode) {
    int i;
    *mode = mode_;
    for (i = 0; i < max_voice_sessions_; i++) {
        if (voice_.session[i].state.current_ == CALL_ACTIVE) {
            return true;
        }
    }
    return false;
}
#endif

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
#ifdef SEC_AUDIO_BLE_OFFLOAD
                {
                    updateVoiceMetadataForBT(true);
                    //dont start the call, until we get suspend from BLE
                    std::unique_lock<std::mutex> guard(reconfig_wait_mutex_);
                }
#endif

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

#ifdef SEC_AUDIO_BLE_OFFLOAD
                AHAL_DBG("ACTIVE -> INACTIVE update meta data as MUSIC");
                updateVoiceMetadataForBT(false);
#endif
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
#ifdef SEC_AUDIO_RECOVERY
    sec_voice_->InitState(__func__);
#endif
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
#ifdef SEC_AUDIO_EARLYDROP_PATCH
        /**  device pairs for HCO usecase
          *  <handset, headset-mic>
          *  <handset, usb-headset-mic>
          *  <speaker, headset-mic>
          *  <speaker, usb-headset-mic>
          *  override devices accordingly.
          */
        if (pal_voice_rx_device_id_ == PAL_DEVICE_OUT_WIRED_HEADSET ||
            (pal_voice_rx_device_id_ == PAL_DEVICE_OUT_USB_HEADSET &&
                        adevice->usb_input_dev_enabled))
            palDevices[1].id = PAL_DEVICE_OUT_HANDSET;
        else if (pal_voice_rx_device_id_ == PAL_DEVICE_OUT_SPEAKER) {
         /*does not handle 3-pole wired or USB headset*/
#ifdef SEC_AUDIO_CALL_TTY
            if (adevice->usb_input_dev_enabled) {
                palDevices[0].id = PAL_DEVICE_IN_USB_HEADSET;
            } else if (!adevice->USBConnected()) {
                palDevices[0].id = PAL_DEVICE_IN_WIRED_HEADSET;
            }
#else
            palDevices[0].id = PAL_DEVICE_IN_WIRED_HEADSET;
            if (adevice->usb_input_dev_enabled)
               palDevices[0].id = PAL_DEVICE_IN_USB_HEADSET;
#endif
        }
#else
        /**  device pairs for HCO usecase
          *  <handset, headset-mic>
          *  <speaker, headset-mic>
          *  override devices accordingly.
          */
        if (pal_voice_rx_device_id_ == PAL_DEVICE_OUT_WIRED_HEADSET)
            palDevices[1].id = PAL_DEVICE_OUT_HANDSET;
        else if (pal_voice_rx_device_id_ == PAL_DEVICE_OUT_SPEAKER)
            palDevices[0].id = PAL_DEVICE_IN_WIRED_HEADSET;
#endif
        else
            AHAL_ERR("Invalid device pair for the usecase");
    }
    if (streamAttributes.info.voice_call_info.tty_mode == PAL_TTY_VCO) {
#ifdef SEC_AUDIO_EARLYDROP_PATCH
        /**  device pairs for VCO usecase
          *  <headphones, handset-mic>
          *  <usb-headset, handset-mic>
          *  <headphones, speaker-mic>
          *  <usb-headset, speaker-mic>
          *  override devices accordingly.
          */
        if (pal_voice_rx_device_id_ == PAL_DEVICE_OUT_WIRED_HEADSET ||
            pal_voice_rx_device_id_ == PAL_DEVICE_OUT_WIRED_HEADPHONE ||
            pal_voice_rx_device_id_ == PAL_DEVICE_OUT_USB_HEADSET)
            palDevices[0].id = PAL_DEVICE_IN_HANDSET_MIC;
        else if (pal_voice_rx_device_id_ == PAL_DEVICE_OUT_SPEAKER) {
#ifdef SEC_AUDIO_CALL_TTY
            if (adevice->usb_input_dev_enabled) {
                palDevices[1].id = PAL_DEVICE_OUT_USB_HEADSET;
            } else if (!adevice->USBConnected()) {
                palDevices[1].id = PAL_DEVICE_OUT_WIRED_HEADSET;
            }
#else
            palDevices[1].id = PAL_DEVICE_OUT_WIRED_HEADSET;
          /*does not handle 3-pole wired or USB headset*/
            if (adevice->usb_input_dev_enabled)
               palDevices[1].id = PAL_DEVICE_OUT_USB_HEADSET;
#endif
        }
#else
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
#endif
        else
            AHAL_ERR("Invalid device pair for the usecase");
    }

#ifdef SEC_AUDIO_CALL
    sec_voice_->SetCustomKey(adevice, palDevices);
#endif
#ifdef SEC_AUDIO_LOOPBACK_TEST
    adevice->factory_->GetPalDeviceId(palDevices, IO_TYPE_BOTH);
#ifdef SEC_AUDIO_DUAL_SPEAKER
    if (adevice->sec_device_->speaker_left_amp_off &&
        adevice->factory_->factory.state & (FACTORY_LOOPBACK_ACTIVE | FACTORY_ROUTE_ACTIVE)) {
        adevice->sec_device_->speaker_status_change = true;
        pal_param_speaker_status_t param_speaker_status;
        param_speaker_status.mute_status = PAL_DEVICE_UPPER_SPEAKER_UNMUTE;
        pal_set_param(PAL_PARAM_ID_SPEAKER_STATUS,
                (void*)&param_speaker_status,
                sizeof(pal_param_speaker_status_t));
    }
#endif
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

#ifndef SEC_AUDIO_CALL_HAC
    /*set custom key for hac mode*/
    if (session && session->hac && palDevices[1].id ==
        PAL_DEVICE_OUT_HANDSET) {
        strlcpy(palDevices[0].custom_config.custom_key, "HAC",
                    sizeof(palDevices[0].custom_config.custom_key));
        strlcpy(palDevices[1].custom_config.custom_key, "HAC",
                    sizeof(palDevices[1].custom_config.custom_key));
        AHAL_INFO("Setting custom key as %s", palDevices[0].custom_config.custom_key);
    }
#endif
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
#ifdef SEC_AUDIO_CALL
        if ((sec_voice_->volume != -1.0f) &&
                (session->pal_vol_data->volume_pair[0].vol != sec_voice_->volume)) {
            AHAL_DBG("apply sec_voice_->volume(%f) instead of cached volume(%f)",
                        sec_voice_->volume, session->pal_vol_data->volume_pair[0].vol);
            session->pal_vol_data->volume_pair[0].vol = sec_voice_->volume;
        }
#endif
#ifdef SEC_AUDIO_SUPPORT_BT_RVC
        if (adevice->effect_->SetScoVolume(session->pal_vol_data->volume_pair[0].vol) == 0) {
            AHAL_DBG("sco volume applied on voice session");
        } else
#endif
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

#ifdef SEC_AUDIO_ENFORCED_AUDIBLE
    if (!sec_voice_->mute_voice && session->device_mute.mute && !sec_voice_->is_shutter_playing)
        session->device_mute.mute = false;
#endif

    /*Apply device mute if needed*/
    if (session->device_mute.mute) {
        ret = SetDeviceMute(session);
    }

    /*apply cached mic mute*/
    if (adevice->mute_) {
        pal_stream_set_mute(session->pal_voice_handle, adevice->mute_);
    }

#ifdef SEC_AUDIO_DYNAMIC_NREC
    // reset dsp aec mixer when entering cp call
    sec_voice_->SetECNS(true);
#endif

#ifdef SEC_AUDIO_CALL
    if (adevice->factory_->factory.loopback_type == LOOPBACK_OFF) {
        sec_voice_->SetNBQuality(adevice->voice_->sec_voice_->nb_quality);
#ifdef SEC_AUDIO_ADAPT_SOUND
        sec_voice_->SetDHAData(adevice, NULL, DHA_SET);
#endif
        if (sec_voice_->cng_enable) {
            sec_voice_->SetCNGForEchoRefMute(true);
        }
        sec_voice_->SetDeviceInfo(palDevices[1].id);
    }
#endif

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
#ifdef SEC_AUDIO_ADAPT_SOUND
        std::shared_ptr<AudioDevice> adevice = AudioDevice::GetInstance();
        if (adevice->factory_->factory.loopback_type == LOOPBACK_OFF)
            sec_voice_->SetDHAData(adevice, NULL, DHA_RESET);
#endif
#ifdef SEC_AUDIO_CALL
        if (sec_voice_->cng_enable) {
            sec_voice_->SetCNGForEchoRefMute(false);
        }
#endif
        ret = pal_stream_stop(session->pal_voice_handle);
        if (ret)
            AHAL_ERR("Pal Stream stop failed %x", ret);
        ret = pal_stream_close(session->pal_voice_handle);
        if (ret)
            AHAL_ERR("Pal Stream close failed %x", ret);
        session->pal_voice_handle = NULL;
    }

#ifdef SEC_AUDIO_CALL
    sec_voice_->pre_device_type = VOICE_DEVICE_INVALID;
#endif

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
#ifdef SEC_AUDIO_EARLYDROP_PATCH
        /**  device pairs for HCO usecase
          *  <handset, headset-mic>
          *  <handset, usb-headset-mic>
          *  <speaker, headset-mic>
          *  <speaker, usb-headset-mic>
          *  override devices accordingly.
          */
        if (pal_voice_rx_device_id_ == PAL_DEVICE_OUT_WIRED_HEADSET ||
            (pal_voice_rx_device_id_ == PAL_DEVICE_OUT_USB_HEADSET &&
                                     adevice->usb_input_dev_enabled))
            palDevices[1].id = PAL_DEVICE_OUT_HANDSET;
        else if (pal_voice_rx_device_id_ == PAL_DEVICE_OUT_SPEAKER) {
        /* does not handle 3-pole wired or USB headset */
#ifdef SEC_AUDIO_CALL_TTY
            if (adevice->usb_input_dev_enabled) {
                palDevices[0].id = PAL_DEVICE_IN_USB_HEADSET;
            } else if (!adevice->USBConnected()) {
                palDevices[0].id = PAL_DEVICE_IN_WIRED_HEADSET;
            }
#else
            palDevices[0].id = PAL_DEVICE_IN_WIRED_HEADSET;
            if (adevice->usb_input_dev_enabled)
               palDevices[0].id = PAL_DEVICE_IN_USB_HEADSET;
#endif
        }
#else
        /**  device pairs for HCO usecase
          *  <handset, headset-mic>
          *  <speaker, headset-mic>
          *  override devices accordingly.
          */
        if (pal_voice_rx_device_id_ == PAL_DEVICE_OUT_WIRED_HEADSET)
            palDevices[1].id = PAL_DEVICE_OUT_HANDSET;
        else if (pal_voice_rx_device_id_ == PAL_DEVICE_OUT_SPEAKER)
            palDevices[0].id = PAL_DEVICE_IN_WIRED_HEADSET;
#endif
        else
            AHAL_ERR("Invalid device pair for the usecase");
    }
    if (session && session->tty_mode == PAL_TTY_VCO) {
#ifdef SEC_AUDIO_EARLYDROP_PATCH
        /**  device pairs for VCO usecase
          *  <headphones, handset-mic>
          *  <usb-headset, handset-mic>
          *  <headphones, speaker-mic>
          *  <usb-headset, speaker-mic>
          *  override devices accordingly.
          */
        if (pal_voice_rx_device_id_ == PAL_DEVICE_OUT_WIRED_HEADSET ||
            pal_voice_rx_device_id_ == PAL_DEVICE_OUT_WIRED_HEADPHONE ||
            pal_voice_rx_device_id_ == PAL_DEVICE_OUT_USB_HEADSET)
            palDevices[0].id = PAL_DEVICE_IN_HANDSET_MIC;
        else if (pal_voice_rx_device_id_ == PAL_DEVICE_OUT_SPEAKER) {
#ifdef SEC_AUDIO_CALL_TTY
            if (adevice->usb_input_dev_enabled) {
                palDevices[1].id = PAL_DEVICE_OUT_USB_HEADSET;
            } else if (!adevice->USBConnected()) {
                palDevices[1].id = PAL_DEVICE_OUT_WIRED_HEADSET;
            }
#else
            palDevices[1].id = PAL_DEVICE_OUT_WIRED_HEADSET;
        /* does not handle 3-pole wired or USB headset */
            if (adevice->usb_input_dev_enabled)
               palDevices[1].id = PAL_DEVICE_OUT_USB_HEADSET;
#endif
        }
#else
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
#endif
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
#ifndef SEC_AUDIO_CALL_HAC
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
#endif
#ifdef SEC_AUDIO_CALL
    sec_voice_->SetCustomKey(adevice, palDevices);
#endif

    if (session && session->pal_voice_handle) {
        ret = pal_stream_set_device(session->pal_voice_handle, 2, palDevices);
        if (ret) {
            AHAL_ERR("Pal Stream Set Device failed %x", ret);
            ret = -EINVAL;
            goto exit;
        }
#ifdef SEC_AUDIO_CALL
        sec_voice_->SetDeviceInfo(palDevices[1].id);
#endif
    } else {
        AHAL_ERR("Voice handle not found");
    }

    // (SEC_TEMP) check SEC_AUDIO_ENFORCED_AUDIBLE case
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

#ifdef SEC_AUDIO_CALL_FORWARDING
    std::shared_ptr<AudioDevice> adevice = AudioDevice::GetInstance();
    if (!mute &&
        (adevice->mute_ ||
        (sec_voice_->IsCallForwarding() || sec_voice_->call_memo == CALLMEMO_REC))) {
        AHAL_DBG("skip voice tx unmute on %s",
                            (adevice->mute_? "tx mute":"force mute"));
        return ret;
    }
#endif
#ifdef SEC_AUDIO_CALL
    sec_voice_->tx_mute = mute;
#endif
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
#ifdef SEC_AUDIO_SUPPORT_BT_RVC
                std::shared_ptr<AudioDevice> adevice = AudioDevice::GetInstance();
                if (adevice->effect_->SetScoVolume(volume) == 0) {
                    AHAL_DBG("sco volume applied on voice session %d", i);
                    return 0;
                }
#endif
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

#ifdef SEC_AUDIO_CALL_VOIP
    if (mode_ == AUDIO_MODE_IN_COMMUNICATION) {
        std::shared_ptr<AudioDevice> adevice = AudioDevice::GetInstance();
        std::shared_ptr<StreamOutPrimary> astream_out = adevice->OutGetStream(PAL_STREAM_VOIP_RX);
        if (astream_out && astream_out->HasPalStreamHandle()) {
            astream_out->SetVolume(volume, volume);
        }
    }
#endif

    AHAL_DBG("Exit ret: %d", ret);
    return ret;
}

#ifdef SEC_AUDIO_BLE_OFFLOAD
void AudioVoice::updateVoiceMetadataForBT(bool call_active)
{
    ssize_t track_count = 1;
    std::vector<playback_track_metadata_t> tracks;
    tracks.resize(track_count);

    source_metadata_t btSourceMetadata;

    if (pal_voice_rx_device_id_ == PAL_DEVICE_OUT_BLUETOOTH_BLE) {
        if (call_active) {
            btSourceMetadata.track_count = track_count;
            btSourceMetadata.tracks = tracks.data();

            btSourceMetadata.tracks->usage = AUDIO_USAGE_VOICE_COMMUNICATION;
            btSourceMetadata.tracks->content_type = AUDIO_CONTENT_TYPE_SPEECH;
            //pass the metadata to PAL
            pal_set_param(PAL_PARAM_ID_SET_SOURCE_METADATA,(void*)&btSourceMetadata,0);
        } else {
            btSourceMetadata.track_count = track_count;
            btSourceMetadata.tracks = tracks.data();

            btSourceMetadata.tracks->usage = AUDIO_USAGE_MEDIA;
            btSourceMetadata.tracks->content_type = AUDIO_CONTENT_TYPE_MUSIC;
            //pass the metadata to PAL
            pal_set_param(PAL_PARAM_ID_SET_SOURCE_METADATA,(void*)&btSourceMetadata,0);
        }
#ifdef SEC_AUDIO_ADD_FOR_DEBUG
        AHAL_INFO("Set PAL_PARAM_ID_SET_SOURCE_METADATA usage=%d, content_type=%d",
                   btSourceMetadata.tracks->usage, btSourceMetadata.tracks->content_type);
#endif
    }
}
#endif

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
#ifndef SEC_AUDIO_CALL_HAC
        voice_.session[i].hac = false;
#endif
    }

    voice_.session[MMODE1_SESS_IDX].vsid = VOICEMMODE1_VSID;
    voice_.session[MMODE2_SESS_IDX].vsid = VOICEMMODE2_VSID;

    stream_out_primary_ = NULL;
#ifdef SEC_AUDIO_CALL
    sec_voice_ = SecVoiceInit();
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
#ifndef SEC_AUDIO_CALL_HAC
        voice_.session[i].hac = false;
#endif
    }

    voice_.session[MMODE1_SESS_IDX].vsid = VOICEMMODE1_VSID;
    voice_.session[MMODE2_SESS_IDX].vsid = VOICEMMODE2_VSID;

    stream_out_primary_ = NULL;
    max_voice_sessions_ = 0;
}

#ifdef SEC_AUDIO_CALL
std::shared_ptr<SecAudioVoice> AudioVoice::SecVoiceInit() {
    std::shared_ptr<SecAudioVoice> sec_voice (new SecAudioVoice());
    return sec_voice;
}
#endif

