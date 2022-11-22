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

#define LOG_TAG "sthal_SoundTriggerSession"
#define ATRACE_TAG (ATRACE_TAG_AUDIO | ATRACE_TAG_HAL)
#define LOG_NDEBUG 0
/*#define VERY_VERY_VERBOSE_LOGGING*/
#ifdef VERY_VERY_VERBOSE_LOGGING
#define ALOGVV ALOGV
#else
#define ALOGVV(a...) do { } while(0)
#endif

#include "SoundTriggerSession.h"

#include <log/log.h>
#include <utils/Trace.h>
#include <cstring>
#include <chrono>
#include <thread>

#include "PalApi.h"

#ifdef SEC_AUDIO_SOUND_TRIGGER_TYPE
#include <cutils/str_parms.h>
#endif

SoundTriggerSession::SoundTriggerSession(sound_model_handle_t handle,
                                         audio_hw_call_back_t callback)
{
    state_ = IDLE;
    sm_handle_ = handle;
    rec_config_payload_ = nullptr;
    rec_config_ = nullptr;
    hal_callback_ = callback;
}

int SoundTriggerSession::pal_callback(
    pal_stream_handle_t *stream_handle,
    uint32_t event_id,
    uint32_t *event_data,
    uint32_t event_size,
    uint64_t cookie)
{
    int32_t status = 0;
    bool lock_status = false;
    unsigned int size = 0;
    int i = 0;
    int j = 0;
    recognition_callback_t callback;
    SoundTriggerSession *session = nullptr;
    struct pal_st_recognition_event *event = nullptr;
    struct sound_trigger_recognition_event *st_event = nullptr;
    struct sound_trigger_phrase_recognition_event *phrase_event = nullptr;
    struct pal_st_phrase_recognition_event *pal_phrase_event = nullptr;

    if (!stream_handle || !event_data) {
        status = -EINVAL;
        ALOGE("%s: error, invalid stream handle or event data", __func__);
        goto exit;
    }

    ALOGD("%s: stream_handle (%p), event_id (%x), event_size (%d),"
        "cookie (%" PRIu64 ")", __func__, stream_handle, event_id,
        event_size, cookie);

    session = (SoundTriggerSession *)cookie;
    /*
     * Sometimes Client may call unload directly, which may get blocked
     * in PAL when releasing second stage engine thread, as it is waiting
     * for this callback to finish. Check if session state changes to non
     * ACTIVE state.
     */
    do {
        lock_status = session->ses_mutex_.try_lock();
    } while(!lock_status && (session->state_ == ACTIVE));

    if (session->state_ != ACTIVE) {
        ALOGW("%s: skip notification as client has stopped", __func__);
        goto exit;
    }

    event = (struct pal_st_recognition_event *)event_data;

    if (event->type == PAL_SOUND_MODEL_TYPE_GENERIC) {
        // parse event and check if keyword detected
        size = sizeof(struct sound_trigger_recognition_event) +
               event->data_size;
        st_event = (struct sound_trigger_recognition_event *)calloc(1, size);
        if (!st_event) {
            status = -ENOMEM;
            ALOGE("%s: error, failed to allocate recognition event", __func__);
            goto exit;
        }

        st_event->data_offset = sizeof(struct sound_trigger_recognition_event);
    } else if (event->type == PAL_SOUND_MODEL_TYPE_KEYPHRASE) {
        size = sizeof(struct sound_trigger_phrase_recognition_event) +
               event->data_size;
        phrase_event =
            (struct sound_trigger_phrase_recognition_event *)calloc(1, size);
        if (!phrase_event) {
            status = -ENOMEM;
            ALOGE("%s: error, failed to allocate recognition event", __func__);
            goto exit;
        }

        st_event = (struct sound_trigger_recognition_event *)phrase_event;
        st_event->data_offset =
            sizeof(struct sound_trigger_phrase_recognition_event);

        // copy data only related to phrase event
        pal_phrase_event = (struct pal_st_phrase_recognition_event *)event;
        phrase_event->num_phrases = pal_phrase_event->num_phrases;
        for (i = 0; i < phrase_event->num_phrases; i++) {
            phrase_event->phrase_extras[i].id =
                pal_phrase_event->phrase_extras[i].id;
            phrase_event->phrase_extras[i].recognition_modes =
                pal_phrase_event->phrase_extras[i].recognition_modes;
            phrase_event->phrase_extras[i].confidence_level =
                pal_phrase_event->phrase_extras[i].confidence_level;
            phrase_event->phrase_extras[i].num_levels =
                pal_phrase_event->phrase_extras[i].num_levels;

            for (j = 0; j < phrase_event->phrase_extras[i].num_levels; j++) {
                phrase_event->phrase_extras[i].levels[j].user_id =
                    pal_phrase_event->phrase_extras[i].levels[j].user_id;
                phrase_event->phrase_extras[i].levels[j].level =
                    pal_phrase_event->phrase_extras[i].levels[j].level;
            }
        }
    } else {
        ALOGE("%s: Invalid event type :%d", __func__, event->type);
        status = -EINVAL;
        goto exit;
    }

    // copy members inside structrue
    st_event->status = event->status;
    st_event->type = (sound_trigger_sound_model_type_t)event->type;
    st_event->model = session->GetSoundModelHandle();
    st_event->capture_available = event->capture_available;
    st_event->capture_session = session->GetCaptureHandle();
    st_event->capture_delay_ms = event->capture_delay_ms;
    st_event->capture_preamble_ms = event->capture_preamble_ms;
    st_event->trigger_in_data = event->trigger_in_data;

    st_event->audio_config.sample_rate = event->media_config.sample_rate;
    if (event->media_config.ch_info.channels == 1)
        st_event->audio_config.channel_mask = AUDIO_CHANNEL_IN_MONO;
    else if (event->media_config.ch_info.channels == 2)
        st_event->audio_config.channel_mask = AUDIO_CHANNEL_IN_STEREO;
    st_event->audio_config.format = AUDIO_FORMAT_PCM_16_BIT;

    st_event->data_size = event->data_size;

    // copy opaque data
    memcpy((uint8_t *)st_event + st_event->data_offset,
           (uint8_t *)event + event->data_offset,
           st_event->data_size);

    // callback to SoundTriggerService
    session->GetRecognitionCallback(&callback);
    session->ses_mutex_.unlock();
    lock_status = false;
    ATRACE_BEGIN("sthal: client detection callback");
    callback(st_event, session->GetCookie());
    ATRACE_END();

exit:
    // release resources allocated
    if (phrase_event)
        free(phrase_event);
    else if (st_event)
        free(st_event);
    if (lock_status)
        session->ses_mutex_.unlock();
    ALOGV("%s: Exit, status %d", __func__, status);

    return status;
}

bool SoundTriggerSession::IsACDSoundModel(struct sound_trigger_sound_model *sound_model)
{
    //todo: get this from PAL instead of hardcoding.
    const sound_trigger_uuid_t qc_acd_uuid = { 0x4e93281b, 0x296e, 0x4d73, 0x9833,
                                              { 0x27, 0x10, 0xc3, 0xc7, 0xc1, 0xdb } };

    if (sound_model &&
        !std::memcmp(&sound_model->vendor_uuid, &qc_acd_uuid,
                     sizeof(sound_trigger_uuid_t)))
        return true;
    else
        return false;
}

int SoundTriggerSession::OpenPALStream(pal_stream_type_t stream_type)
{
    int status = 0;
    struct pal_stream_attributes stream_attributes;
    struct pal_device device;

    ALOGV("%s: Enter", __func__);

    device.id = PAL_DEVICE_IN_HANDSET_VA_MIC; // To-Do: convert into PAL Device
    device.config.sample_rate = 48000;
    device.config.bit_width = 16;
    device.config.ch_info.channels = 2;
    device.config.ch_info.ch_map[0] = PAL_CHMAP_CHANNEL_FL;
    device.config.ch_info.ch_map[1] = PAL_CHMAP_CHANNEL_FR;
    device.config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE;

    stream_attributes.type = stream_type;
    stream_attributes.flags = (pal_stream_flags_t)0;
    stream_attributes.direction = PAL_AUDIO_INPUT;
    stream_attributes.in_media_config.sample_rate = 16000;
    stream_attributes.in_media_config.bit_width = 16;
    stream_attributes.in_media_config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE;
    stream_attributes.in_media_config.ch_info.channels = 1;
    stream_attributes.in_media_config.ch_info.ch_map[0] = PAL_CHMAP_CHANNEL_FL;

    ALOGD("%s:(%x:status)%d", __func__, status, __LINE__);
    status = pal_stream_open(&stream_attributes,
                             1,
                             &device,
                             0,
                             nullptr,
                             &pal_callback,
                             (uint64_t)this,
                             &pal_handle_);

    ALOGD("%s:(%x:status)%d", __func__, status, __LINE__);

    if (status) {
        ALOGE("%s: Pal Stream Open Error (%x)", __func__, status);
        status = -EINVAL;
        goto exit;
    }

exit:
    ALOGV("%s: Exit, status = %d", __func__, status);

    return status;
}

int SoundTriggerSession::StopRecognition_l()
{
    int status = 0;

    ALOGV("%s: Enter", __func__);

    // deregister from audio hal
    RegisterHalEvent(false);

    // stop pal stream
    status = pal_stream_stop(pal_handle_);
    if (status) {
        ALOGE("%s: error, failed to stop pal stream, status = %d",
              __func__, status);
    }

    if (rec_config_payload_) {
        free(rec_config_payload_);
        rec_config_payload_ = nullptr;
    }
    rec_config_ = nullptr;

    state_ = STOPPED;
    ALOGV("%s: Exit, status = %d", __func__, status);

    return status;
}

int SoundTriggerSession::LoadSoundModel(
    struct sound_trigger_sound_model *sound_model)
{
    int status = 0;
    struct pal_st_sound_model *pal_common_sm = nullptr;
    struct pal_st_phrase_sound_model *pal_phrase_sm = nullptr;
    struct sound_trigger_sound_model *common_sm = nullptr;
    struct sound_trigger_phrase_sound_model *phrase_sm = nullptr;
    pal_param_payload *param_payload = nullptr;
    unsigned int size = 0;
    pal_stream_type_t stream_type = PAL_STREAM_VOICE_UI;

    ALOGV("%s: Enter", __func__);
    std::lock_guard<std::mutex> lck(ses_mutex_);

    if (IsACDSoundModel(sound_model))
        stream_type = PAL_STREAM_ACD;

    // open pal stream
    status = OpenPALStream(stream_type);
    if (status) {
        ALOGE("%s: error, failed to open PAL stream", __func__);
        goto exit;
    }

    // parse sound model into pal_sound_model
    if (sound_model->type == SOUND_MODEL_TYPE_GENERIC) {
        common_sm = sound_model;
        if ((stream_type != PAL_STREAM_ACD) &&
            (!common_sm->data_size ||
            (common_sm->data_offset < sizeof(*common_sm)))) {
            ALOGE("%s: Invalid Generic sound model params "
                  "data size=%d, data offset=%d", __func__,
                  common_sm->data_size, common_sm->data_offset);
            status = -EINVAL;
            goto exit;
        }

        size = sizeof(struct pal_st_sound_model) +
               common_sm->data_size;
        param_payload = (pal_param_payload *)calloc(1,
            sizeof(pal_param_payload) + size);
        if (!param_payload) {
            ALOGE("%s: error, failed to allocate pal sound model", __func__);
            status = -ENOMEM;
            goto exit;
        }
        param_payload->payload_size = size;
        pal_common_sm = (struct pal_st_sound_model *)param_payload->payload;

        pal_common_sm->type = (pal_st_sound_model_type_t)common_sm->type;
        memcpy(&pal_common_sm->uuid, &common_sm->uuid,
               sizeof(struct st_uuid));
        memcpy(&pal_common_sm->vendor_uuid, &common_sm->vendor_uuid,
               sizeof(struct st_uuid));
        pal_common_sm->data_size = common_sm->data_size;
        pal_common_sm->data_offset = sizeof(struct pal_st_sound_model);

        // data_size is zero for ACD streams and non-zero for other streams
        if (pal_common_sm->data_size)
            memcpy((uint8_t *)pal_common_sm + pal_common_sm->data_offset,
                   (uint8_t *)common_sm + common_sm->data_offset,
                   pal_common_sm->data_size);

    } else if (sound_model->type == SOUND_MODEL_TYPE_KEYPHRASE) {
        phrase_sm = (struct sound_trigger_phrase_sound_model *)sound_model;
        if ((phrase_sm->common.data_size == 0) ||
            (phrase_sm->common.data_offset < sizeof(*phrase_sm)) ||
            (phrase_sm->common.type != SOUND_MODEL_TYPE_KEYPHRASE) ||
            (phrase_sm->num_phrases == 0)) {
            ALOGE("%s: Invalid phrase sound model params "
                  "data size=%d, data offset=%d, type=%d phrases=%d",
                  __func__, phrase_sm->common.data_size,
                  phrase_sm->common.data_offset, phrase_sm->common.type,
                  phrase_sm->num_phrases);
            status = -EINVAL;
            goto exit;
        }

        size = sizeof(struct pal_st_phrase_sound_model) +
               phrase_sm->common.data_size;
        param_payload = (pal_param_payload *)calloc(1,
            sizeof(pal_param_payload) + size);
        if (!param_payload) {
            ALOGE("%s: error, failed to allocate pal phrase sound model",
                __func__);
            status = -ENOMEM;
            goto exit;
        }
        param_payload->payload_size = size;
        pal_phrase_sm =
            (struct pal_st_phrase_sound_model *)param_payload->payload;

        pal_phrase_sm->common.type =
            (pal_st_sound_model_type_t)phrase_sm->common.type;
        memcpy(&pal_phrase_sm->common.uuid, &phrase_sm->common.uuid,
               sizeof(struct st_uuid));
        memcpy(&pal_phrase_sm->common.vendor_uuid,
               &phrase_sm->common.vendor_uuid,
               sizeof(struct st_uuid));
        pal_phrase_sm->common.data_size = phrase_sm->common.data_size;
        pal_phrase_sm->common.data_offset =
            sizeof(struct pal_st_phrase_sound_model);
        pal_phrase_sm->num_phrases = phrase_sm->num_phrases;

        for (int i = 0; i < pal_phrase_sm->num_phrases; i++) {
            pal_phrase_sm->phrases[i].id = phrase_sm->phrases[i].id;
            pal_phrase_sm->phrases[i].recognition_mode =
                phrase_sm->phrases[i].recognition_mode;
            pal_phrase_sm->phrases[i].num_users =
                phrase_sm->phrases[i].num_users;
            for (int j = 0; j < pal_phrase_sm->phrases[i].num_users; j++)
                pal_phrase_sm->phrases[i].users[j] =
                    phrase_sm->phrases[i].users[j];

            memcpy(pal_phrase_sm->phrases[i].locale,
                   phrase_sm->phrases[i].locale,
                   SOUND_TRIGGER_MAX_LOCALE_LEN);
            memcpy(pal_phrase_sm->phrases[i].text,
                   phrase_sm->phrases[i].text,
                   SOUND_TRIGGER_MAX_STRING_LEN);
        }

        memcpy((uint8_t *)pal_phrase_sm + pal_phrase_sm->common.data_offset,
               (uint8_t *)phrase_sm + phrase_sm->common.data_offset,
               pal_phrase_sm->common.data_size);
        pal_common_sm = (struct pal_st_sound_model *)pal_phrase_sm;
    } else {
        ALOGE("%s: error, unknown sound model type %d",
              __func__, sound_model->type);
        status = -EINVAL;
        goto exit;
    }

    // load sound model with pal api
    status = pal_stream_set_param(pal_handle_,
                                  PAL_PARAM_ID_LOAD_SOUND_MODEL,
                                  param_payload);
    if (status) {
        ALOGE("%s: error, failed to load sound model into PAL, status = %d",
              __func__, status);
        goto exit;
    }

    state_ = LOADED;

exit:
    if (param_payload)
        free(param_payload);
    ALOGV("%s: Exit, status = %d", __func__, status);

    return status;
}

int SoundTriggerSession::UnloadSoundModel()
{
    int status = 0;

    ALOGV("%s: Enter", __func__);
    std::lock_guard<std::mutex> lck(ses_mutex_);
    if (state_ == ACTIVE) {
        status = StopRecognition_l();
        if (status) {
            ALOGE("%s: error, failed to stop recognition, status = %d",
                __func__, status);
        }
    }

    status = pal_stream_close(pal_handle_);
    if (status) {
        ALOGE("%s: error, failed to close pal stream, status = %d",
              __func__, status);
        goto exit;
    }
    if (rec_config_payload_) {
        free(rec_config_payload_);
        rec_config_payload_ = nullptr;
    }
    rec_config_ = nullptr;

    state_ = IDLE;

exit:
    ALOGV("%s: Exit, status = %d", __func__, status);

    return status;
}

#ifdef SEC_AUDIO_SOUND_TRIGGER_TYPE
struct sound_trigger_ss_va_config
{
    int32_t ctrl_SVT_MODE;      // 0 : SVT OFF, 1 : Voice Trigger w/ LPSD , 2 : Sound Detect, 3 : Voice Trigger w/o LPSD
    int32_t ctrl_LPSD;          // 0 : LPSD OFF, 1: LPSD ON
    int32_t ctrl_VT;            // 0 : Voice Trigger OFF, 1 : Voice Trigger ON
    int32_t ctrl_hist_buf_time; // 0 ~ 2000ms
};

static void ParseUserData(const char *keyValuePairs,
    int *amodel_mode, int *backlog_size)
{
    ALOGD("%s : %s", __func__, keyValuePairs);
    struct str_parms *parms = str_parms_create_str(keyValuePairs);
    int value;
    int ret;

    if (!parms) {
        ALOGE("%s: str_params NULL", __func__);
        return;
    }

    ret = str_parms_get_int(parms, "voice_trig", &value);
    if (ret >= 0) {
        ALOGV("voice_trigger = (%d)", value);
        *backlog_size = value;
        str_parms_del(parms, "voice_trig");
    }
    ret = str_parms_get_int(parms, "wakeup_mode", &value);
    if (ret >= 0) {
        ALOGV("wakeup_mode = (%d)", value);
        *amodel_mode = value;
        str_parms_del(parms, "wakeup_mode");
    }
    str_parms_destroy(parms);
}
#endif

int SoundTriggerSession::StartRecognition(
    const struct sound_trigger_recognition_config *config,
    recognition_callback_t callback,
    void *cookie,
    uint32_t version)
{
    int status = 0;
    unsigned int size = 0;

    ALOGV("%s: Enter, state = %d", __func__, state_);
    std::lock_guard<std::mutex> lck(ses_mutex_);

    if (rec_config_payload_) {
        free(rec_config_payload_); // valid due to subsequent start after a detection
        rec_config_payload_ = nullptr;
    }
#ifdef SEC_AUDIO_SOUND_TRIGGER_TYPE
    sound_trigger_ss_va_config *ss_param = nullptr;
    int backlog_size = 0;
    int amodel_mode = 0;
    if (config->data_size > 0) {
        char *params = (char*)config + config->data_offset;
        if (version == SOUND_TRIGGER_DEVICE_API_VERSION_1_3) {
            params = (char*)params - sizeof(struct sound_trigger_recognition_config_header);
        }
        if (params) {
            char *paramsEndWithNull = (char*)malloc(config->data_size + 1);
            if (paramsEndWithNull) {
                memset(paramsEndWithNull, 0, config->data_size + 1);
                memcpy(paramsEndWithNull, params, config->data_size);
                ALOGI("%s params(%d) : %s", __func__, config->data_size, paramsEndWithNull);
                ParseUserData(paramsEndWithNull, &amodel_mode, &backlog_size);
                free (paramsEndWithNull);
            } else {
                ALOGI("%s params(%d) : %s", __func__, config->data_size, params);
                ParseUserData(params, &amodel_mode, &backlog_size);
            }
        }
    }

    if (amodel_mode > 0) {
        size = sizeof(struct pal_st_recognition_config) +
            sizeof(struct sound_trigger_ss_va_config);
    } else
#endif
    size = sizeof(struct pal_st_recognition_config) +
           config->data_size;
    rec_config_payload_ = (pal_param_payload *)calloc(1,
        sizeof(pal_param_payload) + size);
    if (!rec_config_payload_) {
        ALOGE("%s: error, failed to allocate pal recognition config", __func__);
        status = -ENOMEM;
        goto exit;
    }
    rec_config_payload_->payload_size = size;
    rec_config_ = (struct pal_st_recognition_config *)rec_config_payload_->payload;

    rec_config_->capture_handle = (int32_t)config->capture_handle;
    rec_config_->capture_device = (uint32_t)config->capture_device;
    rec_config_->capture_requested = config->capture_requested;
    rec_config_->num_phrases = config->num_phrases;
    for (int i = 0; i < rec_config_->num_phrases; i++) {
        rec_config_->phrases[i].id = config->phrases[i].id;
        rec_config_->phrases[i].recognition_modes =
            config->phrases[i].recognition_modes;
        rec_config_->phrases[i].confidence_level =
            config->phrases[i].confidence_level;
        rec_config_->phrases[i].num_levels = config->phrases[i].num_levels;

        for (int j = 0; j < rec_config_->phrases[i].num_levels; j++) {
            rec_config_->phrases[i].levels[j].user_id =
                config->phrases[i].levels[j].user_id;
            rec_config_->phrases[i].levels[j].level =
                config->phrases[i].levels[j].level;
        }
    }
#ifdef SEC_AUDIO_SOUND_TRIGGER_TYPE
    if (amodel_mode > 0)
        rec_config_->data_size = sizeof(struct sound_trigger_ss_va_config);
    else
#endif
    rec_config_->data_size = config->data_size;
    rec_config_->data_offset = sizeof(struct pal_st_recognition_config);
    rec_config_->cookie = (uint8_t *)this;
#ifdef SEC_AUDIO_SOUND_TRIGGER_TYPE
    if (amodel_mode > 0) {
        ss_param = (sound_trigger_ss_va_config *)((uint8_t *)rec_config_ +
            rec_config_->data_offset);
        ss_param->ctrl_SVT_MODE = amodel_mode;
        ss_param->ctrl_LPSD = 0;
        ss_param->ctrl_VT = 0;
        ss_param->ctrl_hist_buf_time = backlog_size;
        ALOGD("%s: ss_param->ctrl_SVT_MODE %d, ss_param->ctrl_hist_buf_time %d",
            __func__, ss_param->ctrl_SVT_MODE, ss_param->ctrl_hist_buf_time);
    } else {
#endif
    if (version == SOUND_TRIGGER_DEVICE_API_VERSION_1_3) {
        memcpy((uint8_t *)rec_config_ + rec_config_->data_offset,
            (uint8_t *)config + config->data_offset -
            sizeof(struct sound_trigger_recognition_config_header),
            rec_config_->data_size);
    } else {
        memcpy((uint8_t *)rec_config_ + rec_config_->data_offset,
            (uint8_t *)config + config->data_offset,
            rec_config_->data_size);
    }
#ifdef SEC_AUDIO_SOUND_TRIGGER_TYPE
    }
#endif

    // set recognition config
    status = pal_stream_set_param(pal_handle_,
                                  PAL_PARAM_ID_RECOGNITION_CONFIG,
                                  rec_config_payload_);
    if (status) {
        ALOGE("%s: error, failed to set recognition config, status = %d",
              __func__, status);
        goto exit;
    }

    rec_callback_ = callback;
    cookie_ = cookie;

    // start pal stream
    status = pal_stream_start(pal_handle_);
    if (status) {
        ALOGE("%s: error, failed to start pal stream, status = %d",
              __func__, status);
        goto exit;
    }
    state_ = ACTIVE;

    // register to audio hal
    RegisterHalEvent(true);

exit:
    if (status && rec_config_payload_) {
        free(rec_config_payload_);
        rec_config_payload_ = nullptr;
        rec_config_ = nullptr;
    }

    ALOGV("%s: Exit, status = %d", __func__, status);

    return status;
}

int SoundTriggerSession::StopRecognition()
{
    int status = 0;

    ALOGV("%s: Enter", __func__);
    std::lock_guard<std::mutex> lck(ses_mutex_);

    // deregister from audio hal
    RegisterHalEvent(false);

    // stop pal stream
    status = pal_stream_stop(pal_handle_);
    if (status) {
        ALOGE("%s: error, failed to stop pal stream, status = %d",
              __func__, status);
    }

    if (rec_config_payload_) {
        free(rec_config_payload_);
        rec_config_payload_ = nullptr;
    }
    rec_config_ = nullptr;

    state_ = STOPPED;
    ALOGV("%s: Exit, status = %d", __func__, status);

    return status;
}

void SoundTriggerSession::RegisterHalEvent(bool is_register)
{
    struct sound_trigger_event_info event_info;

    if ((rec_config_ && rec_config_->capture_requested) && hal_callback_) {
        if (is_register) {
            ALOGD("%s:[c%d] ST_EVENT_SESSION_REGISTER capture_handle %d",
                  __func__, sm_handle_, rec_config_->capture_handle);
            event_info.st_ses.p_ses = (void *)pal_handle_;
            event_info.st_ses.capture_handle = rec_config_->capture_handle;
            hal_callback_(ST_EVENT_SESSION_REGISTER, &event_info);
        } else {
            ALOGD("%s:[c%d] ST_EVENT_SESSION_DEREGISTER capture_handle %d",
                  __func__, sm_handle_, rec_config_->capture_handle);
            event_info.st_ses.p_ses = (void *)pal_handle_;
            event_info.st_ses.capture_handle = rec_config_->capture_handle;
            hal_callback_(ST_EVENT_SESSION_DEREGISTER, &event_info);
        }
    }
}

sound_model_handle_t SoundTriggerSession::GetSoundModelHandle()
{
    return sm_handle_;
}

int SoundTriggerSession::GetCaptureHandle()
{
    int handle = 0;

    if (rec_config_)
        handle = rec_config_->capture_handle;

    return handle;
}

int SoundTriggerSession::GetModuleVersion(char version[])
{
    int status = 0;
    pal_param_payload *param_payload = nullptr;
    struct version_arch_payload *version_payload = nullptr;

    ALOGV("%s: Enter", __func__);
    std::lock_guard<std::mutex> lck(ses_mutex_);
    status = OpenPALStream(PAL_STREAM_VOICE_UI);
    if (status) {
        ALOGE("%s: Failed to open pal stream, status = %d", __func__, status);
        goto exit;
    }

    status = pal_stream_get_param(pal_handle_,
        PAL_PARAM_ID_WAKEUP_MODULE_VERSION, &param_payload);
    if (status) {
        ALOGE("%s: Failed to get version, status = %d", __func__, status);
        goto exit;
    }

    version_payload = (struct version_arch_payload *)param_payload;
#ifdef SEC_AUDIO_SOUND_TRIGGER_TYPE
    if (version_payload == nullptr){
        goto exit;
    }
#endif
    snprintf(version, SOUND_TRIGGER_MAX_STRING_LEN, "%d, %s",
        version_payload->version, version_payload->arch);

exit:
    if (pal_handle_) {
        status = pal_stream_close(pal_handle_);
        if (status) {
            ALOGE("%s: error, failed to close pal stream, status = %d",
                __func__, status);
        }
    }
    ALOGV("%s: Exit", __func__);
    return 0;
}

void *SoundTriggerSession::GetCookie()
{
    return cookie_;
}

void SoundTriggerSession::GetRecognitionCallback(
    recognition_callback_t *callback)
{
    *callback = rec_callback_;
}

