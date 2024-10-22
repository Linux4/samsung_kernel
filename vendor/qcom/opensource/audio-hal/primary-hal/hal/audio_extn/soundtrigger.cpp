/* Copyright (c) 2013-2014, 2016-2021 The Linux Foundation. All rights reserved.
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
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "AHAL: soundtrigger"
/* #define LOG_NDEBUG 0 */
#define LOG_NDDEBUG 0

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>
#include <cutils/list.h>

#include "AudioCommon.h"
#include "AudioDevice.h"
#include "audio_extn.h"
#include "AudioExtn.h"
#include "PalApi.h"

/*-------------------- Begin: AHAL-STHAL Interface ---------------------------*/
/*
 * Maintain the proprietary interface between AHAL and STHAL locally to avoid
 * the compilation dependency of interface header file from STHAL.
 */

#define MAKE_HAL_VERSION(maj, min) ((((maj) & 0xff) << 8) | ((min) & 0xff))
#define MAJOR_VERSION(ver) (((ver) & 0xff00) >> 8)
#define MINOR_VERSION(ver) ((ver) & 0x00ff)

/* Proprietary interface version used for compatibility with STHAL */
#define STHAL_PROP_API_VERSION_1_0 MAKE_HAL_VERSION(1, 0)
#define STHAL_PROP_API_VERSION_1_1 MAKE_HAL_VERSION(1, 1)
#define STHAL_PROP_API_CURRENT_VERSION STHAL_PROP_API_VERSION_1_1

#define ST_EVENT_CONFIG_MAX_STR_VALUE 32
#define ST_DEVICE_HANDSET_MIC 1

typedef enum {
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
    AUDIO_EVENT_UPDATE_ECHO_REF
} audio_event_type_t;

typedef enum {
    USECASE_TYPE_PCM_PLAYBACK,
    USECASE_TYPE_PCM_CAPTURE,
    USECASE_TYPE_VOICE_CALL,
    USECASE_TYPE_VOIP_CALL,
} audio_stream_usecase_type_t;

typedef enum {
    SND_CARD_STATUS_OFFLINE,
    SND_CARD_STATUS_ONLINE,
    CPE_STATUS_OFFLINE,
    CPE_STATUS_ONLINE,
    SLPI_STATUS_OFFLINE,
    SLPI_STATUS_ONLINE
} ssr_event_status_t;

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
        ssr_event_status_t status;
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
/* STHAL callback which is called by AHAL */
typedef int (*sound_trigger_hw_call_back_t)(audio_event_type_t,
                                  struct audio_event_info*);

/*---------------- End: AHAL-STHAL Interface ---------------------------------*/

#ifdef DYNAMIC_LOG_ENABLED
#include <log_xml_parser.h>
#define LOG_MASK HAL_MOD_FILE_SND_TRIGGER
#include <log_utils.h>
#endif

#define XSTR(x) STR(x)
#define STR(x) #x
#define MAX_LIBRARY_PATH 100

#define DLSYM(handle, ptr, symbol, err) \
do {\
    ptr = dlsym(handle, #symbol); \
    if (ptr == NULL) {\
        AHAL_INFO("%s not found. %s", #symbol, dlerror());\
        err = -ENODEV;\
    }\
} while (0)

#ifdef __LP64__
#define SOUND_TRIGGER_LIBRARY_PATH "/vendor/lib64/hw/sound_trigger.primary.%s.so"
#else
#define SOUND_TRIGGER_LIBRARY_PATH "/vendor/lib/hw/sound_trigger.primary.%s.so"
#endif

#define SVA_PARAM_DIRECTION_OF_ARRIVAL "st_direction_of_arrival"
#define SVA_PARAM_CHANNEL_INDEX "st_channel_index"
#define MAX_STR_LENGTH_FFV_PARAMS 30
#define MAX_FFV_SESSION_ID 100

#define VUI_PARAMETER_SET_META_DATA "vui_set_metadata"
#define VUI_PARAMETER_GET_META_DATA "vui_get_metadata"
#define VUI_PARAMETER_CAPTURE_META_DATA "vui_cap_metadata"

pal_stream_handle_t *pal_vui_handle = nullptr;

/*
 * Current proprietary API version used by AHAL. Queried by STHAL
 * for compatibility check with AHAL
 */
extern "C" const unsigned int sthal_prop_api_version =
    STHAL_PROP_API_CURRENT_VERSION;

struct sound_trigger_info  {
    struct sound_trigger_session_info st_ses;
    bool lab_stopped;
    struct listnode list;
};

struct sound_trigger_audio_device {
    void *lib_handle;
    std::shared_ptr<AudioDevice> adev;
    sound_trigger_hw_call_back_t st_callback;
    struct listnode st_ses_list;
    pthread_mutex_t lock;
    unsigned int sthal_prop_api_version;
    bool st_ec_ref_enabled;
};

static struct sound_trigger_audio_device *st_dev;

#if LINUX_ENABLED
static void get_library_path(char *lib_path)
{
    snprintf(lib_path, MAX_LIBRARY_PATH,
             "sound_trigger.primary.default.so");
}
#else
#ifdef __LP64__
static void get_library_path(char *lib_path)
{
    snprintf(lib_path, MAX_LIBRARY_PATH,
             "/vendor/lib64/hw/sound_trigger.primary.%s.so",
             XSTR(SOUND_TRIGGER_PLATFORM_NAME));
}
#else
static void get_library_path(char *lib_path)
{
    snprintf(lib_path, MAX_LIBRARY_PATH,
             "/vendor/lib/hw/sound_trigger.primary.%s.so",
             XSTR(SOUND_TRIGGER_PLATFORM_NAME));
}
#endif
#endif

static struct sound_trigger_info *
get_sound_trigger_info(int capture_handle)
{
    struct sound_trigger_info  *st_ses_info = NULL;
    struct listnode *node;
    AHAL_VERBOSE("list empty %d capture_handle %d",
           list_empty(&st_dev->st_ses_list), capture_handle);
    list_for_each(node, &st_dev->st_ses_list) {
        st_ses_info = node_to_item(node, struct sound_trigger_info , list);
        if (st_ses_info->st_ses.capture_handle == capture_handle)
            return st_ses_info;
    }
    return NULL;
}

extern "C" void check_init_audio_extension()
{
    AudioExtn::audio_extn_hidl_init();
}

extern "C" void audio_hw_call_back(sound_trigger_event_type_t event,
                                  sound_trigger_event_info_t* config)
{
    int status = 0;
    struct sound_trigger_info  *st_ses_info;

    if (!st_dev)
       return;

    pthread_mutex_lock(&st_dev->lock);
    switch (event) {
    case ST_EVENT_SESSION_REGISTER:
        if (!config) {
            AHAL_ERR("NULL config");
            status = -EINVAL;
            break;
        }
        st_ses_info = get_sound_trigger_info(config->st_ses.capture_handle);
        if (st_ses_info) {
            memcpy(&st_ses_info->st_ses, &config->st_ses,
                sizeof(struct sound_trigger_session_info));
        } else {
            st_ses_info = (struct sound_trigger_info *)calloc(1,
                sizeof(struct sound_trigger_info ));
            if (!st_ses_info) {
                AHAL_ERR("st_ses_info alloc failed");
                status = -ENOMEM;
                break;
            }
            memcpy(&st_ses_info->st_ses, &config->st_ses,
                sizeof(struct sound_trigger_session_info));
            AHAL_VERBOSE("add capture_handle %d st session opaque ptr %p",
                st_ses_info->st_ses.capture_handle, st_ses_info->st_ses.p_ses);
            list_add_tail(&st_dev->st_ses_list, &st_ses_info->list);
        }
        break;

    case ST_EVENT_SESSION_DEREGISTER:
        if (!config) {
            AHAL_ERR("NULL config");
            status = -EINVAL;
            break;
        }
        st_ses_info = get_sound_trigger_info(config->st_ses.capture_handle);
        if (!st_ses_info) {
            AHAL_ERR("st session opaque ptr %p not in the list!",
                  config->st_ses.p_ses);
            status = -EINVAL;
            break;
        }
        AHAL_VERBOSE("remove capture_handle %d st session opaque ptr %p",
              st_ses_info->st_ses.capture_handle, st_ses_info->st_ses.p_ses);
        list_remove(&st_ses_info->list);
        free(st_ses_info);
        break;

    default:
        AHAL_INFO("Unknown event %d", event);
        break;
    }
    pthread_mutex_unlock(&st_dev->lock);

}

void* audio_extn_sound_trigger_check_and_get_session(StreamInPrimary *in_stream)
{
    struct sound_trigger_info *st_ses_info = nullptr;
    struct listnode *node = nullptr;
    void *handle = nullptr;

    AHAL_VERBOSE("Enter");
    if (!st_dev || !in_stream) {
        AHAL_ERR("st_dev %d, in_stream %d", !st_dev, !in_stream);
        goto exit;
    }

    pthread_mutex_lock(&st_dev->lock);
    in_stream->is_st_session = false;
    AHAL_VERBOSE("list %d capture_handle %d",
          list_empty(&st_dev->st_ses_list), in_stream->GetHandle());
    list_for_each(node, &st_dev->st_ses_list) {
        st_ses_info = node_to_item(node, struct sound_trigger_info , list);
#ifdef SEC_AUDIO_SOUND_TRIGGER_TYPE
        if (st_ses_info->st_ses.capture_handle == in_stream->GetHandle()
            || (in_stream->IsSeamlessEnabled() && st_ses_info->st_ses.capture_handle == AUDIO_IO_HANDLE_NONE))
#else
        if (st_ses_info->st_ses.capture_handle == in_stream->GetHandle())
#endif
        {
            handle = st_ses_info->st_ses.p_ses;
            in_stream->is_st_session = true;
#ifdef SEC_AUDIO_SOUND_TRIGGER_TYPE
            AHAL_DBG("capture_handle %d (%d) is sound trigger",
                  in_stream->GetHandle(), st_ses_info->st_ses.capture_handle);
#else
            AHAL_DBG("capture_handle %d is sound trigger",
                  in_stream->GetHandle());
#endif
            break;
        }
    }
    if (!in_stream->is_st_session) {
        // Since the API is called twice, manage restoring vui handle.
        if (pal_vui_handle) {
            handle = (void *)pal_vui_handle;
            in_stream->pal_vui_handle_ = pal_vui_handle;
            pal_vui_handle = nullptr;
            in_stream->is_st_session = true;
        } else if (in_stream->pal_vui_handle_) {
            handle = (void *)in_stream->pal_vui_handle_;
            in_stream->is_st_session = true;
        }
    }
    pthread_mutex_unlock(&st_dev->lock);

exit:
    AHAL_VERBOSE("Exit");

    return handle;
}

bool audio_extn_sound_trigger_check_session_activity(StreamInPrimary *in_stream)
{
    struct sound_trigger_info *st_ses_info = nullptr;
    struct listnode *node = nullptr;
    bool st_session_available = false;

    AHAL_VERBOSE("Enter");
    if (!st_dev || !in_stream) {
        AHAL_ERR("st_dev %d, in_stream %d", !st_dev, !in_stream);
        goto exit;
    }

    pthread_mutex_lock(&st_dev->lock);
    AHAL_VERBOSE("list %d capture_handle %d",
          list_empty(&st_dev->st_ses_list), in_stream->GetHandle());
    list_for_each(node, &st_dev->st_ses_list) {
        st_ses_info = node_to_item(node, struct sound_trigger_info, list);
#ifdef SEC_AUDIO_SOUND_TRIGGER_TYPE
        if (st_ses_info->st_ses.capture_handle == in_stream->GetHandle()
            || st_ses_info->st_ses.capture_handle == AUDIO_IO_HANDLE_NONE)
#else
        if (st_ses_info->st_ses.capture_handle == in_stream->GetHandle())
#endif
        {
#ifndef SEC_AUDIO_COMMON
            AHAL_VERBOSE("sound trigger session available for capture_handle %d",
                  in_stream->GetHandle());
#endif
            st_session_available = true;
            break;
        }
    }
    pthread_mutex_unlock(&st_dev->lock);

    if (in_stream->pal_vui_handle_) {
        st_session_available = true;
    }

exit:
    AHAL_VERBOSE("Exit");

    return st_session_available;
}

void audio_extn_sound_trigger_set_parameters(std::shared_ptr<AudioDevice> adev __unused,
    struct str_parms *params)
{
    int status = 0;
    char value[1024] = {0};
    char *str = value;
    size_t payload_sz = 0;
    pal_stream_handle_t *handle = nullptr;

    if (!params) {
        AHAL_ERR("null params");
        return;
    }

    status = str_parms_get_str(params, VUI_PARAMETER_SET_META_DATA, value, sizeof(value));
    if (status >= 0) {
        pal_set_param(PAL_PARAM_ID_VUI_SET_META_DATA, (void *)value, sizeof(value));
    }

    status = str_parms_get_str(params, VUI_PARAMETER_CAPTURE_META_DATA , value, sizeof(value));
    if (status >= 0) {
        pal_vui_handle = nullptr;
        payload_sz = sizeof(value);
        status = pal_get_param(PAL_PARAM_ID_VUI_CAPTURE_META_DATA, (void **)&str, &payload_sz, nullptr);
        if (!status && payload_sz) {
            pal_vui_handle = (pal_stream_handle_t *)*((uint64_t *)value);
        }
    }
}

void audio_extn_sound_trigger_get_parameters(std::shared_ptr<AudioDevice> adev __unused,
    struct str_parms *query, struct str_parms *reply)
{
    int status = 0;
    char value[1024] = {0};
    char *str = value;
    size_t payload_sz = 0;
    size_t offset = strlen(VUI_PARAMETER_GET_META_DATA) + 1;
    char *query_str = nullptr;

    if (!query || !reply) {
        AHAL_ERR("null query/reply");
        return;
    }

    query_str = str_parms_to_str(query);
    if (query_str && strstr(query_str, VUI_PARAMETER_GET_META_DATA)) {
        strlcpy(value, query_str + offset, strlen(query_str) - offset + 1);
        status = pal_get_param(PAL_PARAM_ID_VUI_GET_META_DATA, (void **)&str, &payload_sz, nullptr);
        if (!status) {
            str_parms_add_str(reply, VUI_PARAMETER_GET_META_DATA, value);
        }
    }

    if (query_str)
        free(query_str);
}

int audio_extn_sound_trigger_init(std::shared_ptr<AudioDevice> adev)
{
    int status = 0;
    char sound_trigger_lib[100];
    void *sthalPropApiVersion;

    AHAL_INFO("Enter");

    st_dev = (struct sound_trigger_audio_device*)
        calloc(1, sizeof(struct sound_trigger_audio_device));
    if (!st_dev) {
        AHAL_ERR("ERROR. sound trigger alloc failed");
        return -ENOMEM;
    }

    get_library_path(sound_trigger_lib);
    st_dev->lib_handle = dlopen(sound_trigger_lib, RTLD_NOW);

    if (st_dev->lib_handle == NULL) {
        AHAL_ERR("error %s", dlerror());
        status = -ENODEV;
        goto cleanup;
    }
    AHAL_INFO("DLOPEN successful for %s", sound_trigger_lib);

    DLSYM(st_dev->lib_handle, sthalPropApiVersion,
          sthal_prop_api_version, status);
    if (status) {
        st_dev->sthal_prop_api_version = 0;
        status  = 0; /* passthru for backward compability */
    } else {
        if (sthalPropApiVersion != NULL) {
            st_dev->sthal_prop_api_version = *(int*)sthalPropApiVersion;
        }
        if (MAJOR_VERSION(st_dev->sthal_prop_api_version) !=
            MAJOR_VERSION(sthal_prop_api_version)) {
            AHAL_ERR("Incompatible API versions ahal:0x%x != sthal:0x%x",
                     STHAL_PROP_API_CURRENT_VERSION,
                     st_dev->sthal_prop_api_version);
            goto cleanup;
        }
        AHAL_DBG("sthal is using proprietary API version 0x%04x",
              st_dev->sthal_prop_api_version);
    }

    st_dev->adev = adev;
    st_dev->st_ec_ref_enabled = false;
    list_init(&st_dev->st_ses_list);

    return 0;

cleanup:
    if (st_dev->lib_handle)
        dlclose(st_dev->lib_handle);
    free(st_dev);
    st_dev = NULL;

    return status;
}

void audio_extn_sound_trigger_deinit(std::shared_ptr<AudioDevice> adev)
{
    struct sound_trigger_info *st_ses_info = NULL;
    struct listnode *node, *temp;

    AHAL_INFO("Enter");
    if (st_dev) {
        list_for_each_safe(node, temp, &st_dev->st_ses_list) {
            st_ses_info = node_to_item(node, struct sound_trigger_info, list);
            if (st_ses_info) {
                list_remove(&st_ses_info->list);
                free(st_ses_info);
            }
        }
    }

    if (st_dev && (st_dev->adev == adev) && st_dev->lib_handle) {
        dlclose(st_dev->lib_handle);
        free(st_dev);
        st_dev = NULL;
    }
}
