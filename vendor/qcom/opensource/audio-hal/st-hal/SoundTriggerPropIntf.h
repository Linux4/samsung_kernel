/* SoundTriggerPropIntf.h
 *
 * Interface for sound trigger related communication
 * across modules.
 *
 * Copyright (c) 2014, 2016-2020, The Linux Foundation. All rights reserved.
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
#ifndef SOUND_TRIGGER_PROP_INTF_H
#define SOUND_TRIGGER_PROP_INTF_H

#include <hardware/sound_trigger.h>
#include "audio_extn.h"

#define MAKE_HAL_VERSION(maj, min) ((((maj) & 0xff) << 8) | ((min) & 0xff))
#define MAJOR_VERSION(ver) (((ver) & 0xff00) >> 8)
#define MINOR_VERSION(ver) ((ver) & 0x00ff)

/* Interface version used for compatibility with AHAL */
#define STHAL_PROP_API_VERSION_1_0 MAKE_HAL_VERSION(1, 0)
#define STHAL_PROP_API_VERSION_1_1 MAKE_HAL_VERSION(1, 1)
#define STHAL_PROP_API_CURRENT_VERSION STHAL_PROP_API_VERSION_1_1

#define ST_EVENT_CONFIG_MAX_STR_VALUE 32

enum audio_event_type {
    AUDIO_EVENT_CAPTURE_DEVICE_INACTIVE,
    AUDIO_EVENT_CAPTURE_DEVICE_ACTIVE,
    AUDIO_EVENT_PLAYBACK_STREAM_INACTIVE,
    AUDIO_EVENT_PLAYBACK_STREAM_ACTIVE,
    AUDIO_EVENT_STOP_LAB,
    AUDIO_EVENT_SSR,
    AUDIO_EVENT_NUM_ST_SESSIONS,
    AUDIO_EVENT_READ_SAMPLES,
    AUDIO_EVENT_DEVICE_CONNECT,
    AUDIO_EVENT_DEVICE_DISCONNECT,
    AUDIO_EVENT_SVA_EXEC_MODE,
    AUDIO_EVENT_SVA_EXEC_MODE_STATUS,
    AUDIO_EVENT_CAPTURE_STREAM_INACTIVE,
    AUDIO_EVENT_CAPTURE_STREAM_ACTIVE,
    AUDIO_EVENT_BATTERY_STATUS_CHANGED,
    AUDIO_EVENT_GET_PARAM,
    AUDIO_EVENT_UPDATE_ECHO_REF,
    AUDIO_EVENT_SCREEN_STATUS_CHANGED
};
typedef enum audio_event_type audio_event_type_t;

typedef enum {
    USECASE_TYPE_PCM_PLAYBACK,
    USECASE_TYPE_PCM_CAPTURE,
    USECASE_TYPE_VOICE_CALL,
    USECASE_TYPE_VOIP_CALL,
} audio_stream_usecase_type_t;

enum ssr_event_status {
    SND_CARD_STATUS_OFFLINE,
    SND_CARD_STATUS_ONLINE,
    CPE_STATUS_OFFLINE,
    CPE_STATUS_ONLINE,
    SLPI_STATUS_OFFLINE,
    SLPI_STATUS_ONLINE
};

struct audio_read_samples_info {
    struct sound_trigger_session_info *ses_info;
    void *buf;
    size_t num_bytes;
};

struct audio_hal_usecase {
    audio_stream_usecase_type_t type;
};

struct sound_trigger_device_info {
    int device;
};

struct sound_trigger_get_param_data {
    char *param;
    int sm_handle;
    struct str_parms *reply;
};

struct audio_event_info {
    union {
        enum ssr_event_status status;
        int value;
        struct sound_trigger_session_info ses_info;
        struct audio_read_samples_info aud_info;
        char str_value[ST_EVENT_CONFIG_MAX_STR_VALUE];
        struct audio_hal_usecase usecase;
        bool audio_ec_ref_enabled;
        struct sound_trigger_get_param_data st_get_param_data;
    } u;
    struct sound_trigger_device_info device_info;
};
typedef struct audio_event_info audio_event_info_t;

struct sthw_extn_fptrs {
    int (*set_parameters)(const struct sound_trigger_hw_device *dev,
                          sound_model_handle_t sound_model_handle,
                          const char *kv_pairs);
    size_t (*get_buffer_size)(const struct sound_trigger_hw_device *dev,
                              sound_model_handle_t sound_model_handle);
    int (*read_buffer)(const struct sound_trigger_hw_device *dev,
                       sound_model_handle_t sound_model_handle,
                       unsigned char *buf,
                       size_t bytes);
    int (*stop_buffering)(const struct sound_trigger_hw_device *dev,
                          sound_model_handle_t sound_model_handle);
    int (*get_param_data)(const struct sound_trigger_hw_device *dev,
                          sound_model_handle_t sound_model_handle,
                          const char *param,
                          void *payload,
                          size_t payload_size,
                          size_t *param_data_size);
};
typedef struct sthw_extn_fptrs sthw_extn_fptrs_t;

/* STHAL callback which is called by AHAL */
typedef int (*sound_trigger_hw_call_back_t)(enum audio_event_type,
                                  struct audio_event_info*);

/* STHAL version queried by AHAL */
typedef int (*sound_trigger_hw_get_version_t)();

/* STHAL extn interface functionality called by qti wrapper */
typedef void (*sthw_extn_get_fptrs_t)(sthw_extn_fptrs_t *fptrs);

/* AHAL callback which is called by STHAL */
typedef void (*audio_hw_call_back_t)(sound_trigger_event_type_t event,
                          sound_trigger_event_info_t* config);

/* AHAL function which is called by STHAL */
typedef int (*audio_hw_acdb_init_t)(int snd_card_num);

/* AHAL function which is called by STHAL */
typedef int (*audio_hw_acdb_init_v2_t)(struct mixer *mixer);

/* AHAL extn util function which is called by STHAL */
typedef int (*audio_hw_get_snd_card_num_t)();

/* AHAL extn util function which is called by STHAL */
typedef int (*audio_hw_open_snd_mixer_t)(struct mixer **mixer);

/* AHAL extn util function which is called by STHAL */
typedef void (*audio_hw_close_snd_mixer_t)(struct mixer *mixer);
#endif /* SOUND_TRIGGER_PROP_INTF_H */
