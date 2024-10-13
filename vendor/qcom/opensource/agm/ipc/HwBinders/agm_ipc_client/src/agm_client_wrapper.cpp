/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
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
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the
 * disclaimer below) provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *
 *     * Neither the name of Qualcomm Innovation Center, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
 * GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
 * HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#define LOG_TAG "agm_client_wrapper"

#include <cutils/list.h>
#include <vendor/qti/hardware/AGMIPC/1.0/IAGMCallback.h>
#include <hidl/LegacySupport.h>
#include <log/log.h>
#include <unistd.h>
#include <vendor/qti/hardware/AGMIPC/1.0/IAGM.h>

#include <agm/agm_api.h>
#include "inc/AGMCallback.h"
#include <mutex>

using android::hardware::Return;
using android::hardware::hidl_vec;
using vendor::qti::hardware::AGMIPC::V1_0::IAGM;
using vendor::qti::hardware::AGMIPC::V1_0::IAGMCallback;
using vendor::qti::hardware::AGMIPC::V1_0::implementation::AGMCallback;
using vendor::qti::hardware::AGMIPC::V1_0::MmapBufInfo;
using vendor::qti::hardware::AGMIPC::V1_0::AgmDumpInfo;
using android::hardware::defaultPassthroughServiceImplementation;
using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;
using android::sp;

static bool agm_server_died = false;
static pthread_mutex_t agmclient_init_lock = PTHREAD_MUTEX_INITIALIZER;
static android::sp<IAGM> agm_client = NULL;
static sp<server_death_notifier> Server_death_notifier = NULL;
#ifdef AGM_HIDL_ENABLED
sp<IAGMCallback> ClbkBinder = NULL;
#else
static bool is_cb_registered = false;
#endif
static list_declare(client_clbk_data_list);
static pthread_mutex_t clbk_data_list_lock = PTHREAD_MUTEX_INITIALIZER;
static std::mutex agm_session_register_cb_mutex;

struct client_cb_data {
   struct listnode node;
   uint64_t data;
};

void server_death_notifier::serviceDied(uint64_t cookie,
                   const android::wp<::android::hidl::base::V1_0::IBase>& who __unused)
{
    ALOGE("%s : AGM Service died ,cookie : %llu",__func__, (unsigned long long)cookie);
    agm_server_died = true;
    if (cb_ != NULL)
        cb_(cookie_);
    // We exit the client process here, so that it also can restart
    // leading to a fresh start on both the sides.
}

android::sp<IAGM> get_agm_server() {
    pthread_mutex_lock(&agmclient_init_lock);
    if (agm_client == NULL) {
        agm_client = IAGM::getService();
        if (agm_client == nullptr) {
            ALOGE("AGM service not initialized\n");
            goto done;
        }
        if(Server_death_notifier == NULL)
        {
            Server_death_notifier = new server_death_notifier();
            agm_client->linkToDeath(Server_death_notifier,0);
            ALOGI("%s : server linked to death \n", __func__);
        }
    }
done:
    pthread_mutex_unlock(&agmclient_init_lock);
    return agm_client ;
}

int agm_register_service_crash_callback(agm_service_crash_cb cb, uint64_t cookie)
{
    int ret = 0;
    pthread_mutex_lock(&agmclient_init_lock);
    if (agm_client == NULL) {
        agm_client = IAGM::getService();
        if (agm_client == nullptr) {
            ALOGE("AGM service not initialized\n");
            ret = -ESRCH;
            goto done;
        }
    }
    if (Server_death_notifier == NULL) {
        Server_death_notifier = new server_death_notifier(cb, cookie);
        agm_client->linkToDeath(Server_death_notifier,0);
        ALOGI("%s : server linked to death \n", __func__);
    } else {
        Server_death_notifier->register_crash_cb(cb, cookie);
    }
done:
    pthread_mutex_unlock(&agmclient_init_lock);
    return 0;
}

int agm_aif_set_media_config(uint32_t audio_intf,
                                struct agm_media_config *media_config) {
    ALOGV("%s called audio_intf = %d \n", __func__, audio_intf);
    if (!agm_server_died) {
        android::sp<IAGM> agm_client = get_agm_server();
        hidl_vec<AgmMediaConfig> media_config_hidl(1);
        media_config_hidl.data()->rate = media_config->rate;
        media_config_hidl.data()->channels = media_config->channels;
        media_config_hidl.data()->format = (::vendor::qti::hardware::AGMIPC::V1_0::AgmMediaFormat) media_config->format;
        media_config_hidl.data()->data_format = media_config->data_format;
        return agm_client->ipc_agm_aif_set_media_config(audio_intf,
                                                        media_config_hidl);
    }
    return -EINVAL;
}

int agm_session_set_config(uint64_t handle,
                            struct agm_session_config *session_config,
                            struct agm_media_config *media_config,
                            struct agm_buffer_config *buffer_config) {
    ALOGV("%s called with handle = %llx \n", __func__, (unsigned long long)handle);
    if (!agm_server_died) {
        android::sp<IAGM> agm_client = get_agm_server();
        hidl_vec<AgmSessionConfig> session_config_hidl(1);
        memcpy(session_config_hidl.data(),
               session_config,
               sizeof(struct agm_session_config));

        hidl_vec<AgmMediaConfig> media_config_hidl(1);
        media_config_hidl.data()->rate = media_config->rate;
        media_config_hidl.data()->channels = media_config->channels;
        media_config_hidl.data()->format = (::vendor::qti::hardware::AGMIPC::V1_0::AgmMediaFormat) media_config->format;
        media_config_hidl.data()->data_format = media_config->data_format;

        hidl_vec<AgmBufferConfig> buffer_config_hidl(1);
        buffer_config_hidl.data()->count = buffer_config->count;
        buffer_config_hidl.data()->size = buffer_config->size;
        ALOGV("%s : Exit", __func__);
        return agm_client->ipc_agm_session_set_config(handle,
                                                      session_config_hidl,
                                                      media_config_hidl,
                                                      buffer_config_hidl);
    }
    return -EINVAL;
}


int agm_init(){
     /*agm_init in IPC happens in context of the server*/
      return 0;
}

int agm_deinit(){
     /*agm_deinit in IPC happens in context of the server*/
      return 0;
}

int agm_aif_set_metadata(uint32_t audio_intf, uint32_t size, uint8_t *metadata){
    ALOGV("%s called aif = %d, size =%d \n", __func__, audio_intf, size);
    if (!agm_server_died) {
        android::sp<IAGM> agm_client = get_agm_server();
        hidl_vec<uint8_t> metadata_hidl;
        metadata_hidl.resize(size);
        memcpy(metadata_hidl.data(), metadata, size);
        int32_t ret = agm_client->ipc_agm_aif_set_metadata(audio_intf,
                                                           size, metadata_hidl);
        return ret;
    }
    return -EINVAL;
}

int agm_session_set_metadata(uint32_t session_id,
                             uint32_t size,
                             uint8_t *metadata){
    ALOGV("%s called sess_id = %d, size = %d\n", __func__, session_id, size);
    if (!agm_server_died) {
        android::sp<IAGM> agm_client = get_agm_server();
        hidl_vec<uint8_t> metadata_hidl;
        metadata_hidl.resize(size);
        memcpy(metadata_hidl.data(), metadata, size);
        int32_t ret = agm_client->ipc_agm_session_set_metadata(session_id,
                                                               size,
                                                               metadata_hidl);
        return ret;
    }
    return -EINVAL;
}

int agm_session_aif_set_metadata(uint32_t session_id, uint32_t audio_intf,
                                 uint32_t size, uint8_t *metadata){
    ALOGV("%s called with sess_id = %d, aif = %d, size = %d\n", __func__,
                                                 session_id, audio_intf, size);
    if (!agm_server_died) {
        android::sp<IAGM> agm_client = get_agm_server();
        hidl_vec<uint8_t> metadata_hidl;
        metadata_hidl.resize(size);
        memcpy(metadata_hidl.data(), metadata, size);
        int32_t ret = agm_client->ipc_agm_session_aif_set_metadata(session_id,
                                                                audio_intf,
                                                                size,
                                                                metadata_hidl);
        return ret;
    }
    return -EINVAL;
}

int agm_session_close(uint64_t handle){
    ALOGV("%s called with handle = %llx \n", __func__, (unsigned long long) handle);
    if (!agm_server_died) {
        android::sp<IAGM> agm_client = get_agm_server();
        return agm_client->ipc_agm_session_close(handle);
    }
    return -EINVAL;
}

int agm_session_prepare(uint64_t handle){
    ALOGV("%s called with handle = %llx \n", __func__, (unsigned long long) handle);
    if (!agm_server_died) {
        android::sp<IAGM> agm_client = get_agm_server();
        return agm_client->ipc_agm_session_prepare(handle);
    }
    return -EINVAL;
}

int agm_session_start(uint64_t handle){
    ALOGV("%s called with handle = %llx \n", __func__, (unsigned long long) handle);
    if (!agm_server_died) {
        android::sp<IAGM> agm_client = get_agm_server();
        return agm_client->ipc_agm_session_start(handle);
    }
    return -EINVAL;
}

int agm_session_stop(uint64_t handle){
    ALOGV("%s called with handle = %llx \n", __func__, (unsigned long long) handle);
    if (!agm_server_died) {
        android::sp<IAGM> agm_client = get_agm_server();
        return agm_client->ipc_agm_session_stop(handle);
    }
    return -EINVAL;
}

int agm_session_pause(uint64_t handle){
    ALOGV("%s called with handle = %llx \n", __func__, (unsigned long long) handle);
    if (!agm_server_died) {
        android::sp<IAGM> agm_client = get_agm_server();
        return agm_client->ipc_agm_session_pause(handle);
    }
    return -EINVAL;
}

int agm_session_flush(uint64_t handle){
    ALOGV("%s called with handle = %llx \n", __func__, (unsigned long long) handle);
    if (!agm_server_died) {
        android::sp<IAGM> agm_client = get_agm_server();
        return agm_client->ipc_agm_session_flush(handle);
    }
    return -EINVAL;
}

int agm_sessionid_flush(uint32_t session_id)
{
    ALOGV("%s called with session id = %d", __func__, session_id);
    if (!agm_server_died) {
        android::sp<IAGM> agm_client = get_agm_server();
        return agm_client->ipc_agm_sessionid_flush(session_id);
    }
    return -EINVAL;
}

int agm_session_resume(uint64_t handle){
    ALOGV("%s called with handle = %llx \n", __func__, (unsigned long long) handle);
    if (!agm_server_died) {
        android::sp<IAGM> agm_client = get_agm_server();
        return agm_client->ipc_agm_session_resume(handle);
    }
    return -EINVAL;
}

int agm_session_suspend(uint64_t handle){
    ALOGV("%s called with handle = %llx \n", __func__, (unsigned long long) handle);
    if (!agm_server_died) {
        android::sp<IAGM> agm_client = get_agm_server();
        return agm_client->ipc_agm_session_suspend(handle);
    }
    return -EINVAL;
}

int agm_session_open(uint32_t session_id, enum agm_session_mode sess_mode ,
                     uint64_t *handle) {
    ALOGD("%s called with handle = %x , *handle = %x\n", __func__, handle, *handle);
    int ret = -EINVAL;
    if (!agm_server_died) {
        android::sp<IAGM> agm_client = get_agm_server();
        AgmSessionMode sess_mode_hidl = (AgmSessionMode) sess_mode;
        auto status = agm_client->ipc_agm_session_open(session_id, sess_mode_hidl,
                              [&](int32_t _ret, hidl_vec<uint64_t> handle_hidl)
                              {  ret = _ret;
                                 *handle = *handle_hidl.data();
                              });
        if (!status.isOk()) {
            ALOGE("%s: HIDL call failed. ret=%d\n", __func__, ret);
        }
    }
    ALOGD("%s Received handle = %p , *handle = %llx\n", __func__, handle, (unsigned long long) *handle);
    return ret;
}

int  agm_session_aif_connect(uint32_t session_id,
                             uint32_t audio_intf,
                             bool state) {
    ALOGV("%s called with sess_id = %d, aif = %d, state = %s\n", __func__,
           session_id, audio_intf, state ? "true" : "false" );
    if (!agm_server_died) {
        android::sp<IAGM> agm_client = get_agm_server();
        return agm_client->ipc_agm_session_aif_connect(session_id,
                                                       audio_intf,
                                                       state);
    }
    return -EINVAL;
}

int agm_session_read(uint64_t handle, void *buf, size_t *byte_count){
    ALOGV("%s called with handle = %llx \n", __func__, (unsigned long long) handle);
    if (!agm_server_died) {
        android::sp<IAGM> agm_client = get_agm_server();
        if (!handle)
            return -EINVAL;

        int ret = -EINVAL;

        auto status = agm_client->ipc_agm_session_read(handle, *byte_count,
                   [&](int32_t _ret, hidl_vec<uint8_t> buff_hidl, uint32_t cnt)
                   { ret = _ret;
                     if (ret != -ENOMEM) {
                         memcpy(buf, buff_hidl.data(), *byte_count);
                         *byte_count = (size_t) cnt;
                     }
                   });
        if (!status.isOk()) {
            ALOGE("%s: HIDL call failed. ret=%d\n", __func__, ret);
        }
        return ret;
    }
    return -EINVAL;
}

int agm_session_write(uint64_t handle, void *buf, size_t *byte_count) {
    ALOGV("%s called with handle = %llx \n", __func__, (unsigned long long) handle);
    if (!agm_server_died) {
        android::sp<IAGM> agm_client = get_agm_server();
        int ret = -EINVAL;

        if (!handle)
            return -EINVAL;

        hidl_vec<uint8_t> buf_hidl;
        buf_hidl.resize(*byte_count);
        memcpy(buf_hidl.data(), buf, *byte_count);

        auto status = agm_client->ipc_agm_session_write(handle, buf_hidl, *byte_count,
                                           [&](int32_t _ret, uint32_t cnt)
                                           { ret = _ret;
                                             if (ret != -ENOMEM)
                                                 *byte_count = (size_t) cnt;
                                           });
        if (!status.isOk()) {
            ALOGE("%s: HIDL call failed. ret=%d\n", __func__, ret);
        }
        return ret;
    }
    return -EINVAL;
}


int agm_session_set_loopback(uint32_t capture_session_id,
                             uint32_t playback_session_id,
                             bool state)
{
    ALOGV("%s called capture_session_id = %d, playback_session_id = %d\n", __func__,
           capture_session_id, playback_session_id);
    if (!agm_server_died) {
        android::sp<IAGM> agm_client = get_agm_server();
        return agm_client->ipc_agm_session_set_loopback(capture_session_id,
                                                        playback_session_id,
                                                        state);
    }
    return -EINVAL;
}

size_t agm_get_hw_processed_buff_cnt(uint64_t handle, enum direction dir) {
    ALOGV("%s called with handle = %llx \n", __func__, (unsigned long long) handle);
    if (!agm_server_died) {
        android::sp<IAGM> agm_client = get_agm_server();
        Direction dir_hidl = (Direction) dir;
        return agm_client->ipc_agm_get_hw_processed_buff_cnt(handle,
                                                             dir_hidl);
    }
    return -EINVAL;
}

int agm_get_aif_info_list(struct aif_info *aif_list, size_t *num_aif_info) {
    ALOGV("%s called \n", __func__);
    if (!agm_server_died) {
        uint32_t num = (uint32_t) *num_aif_info;
        int ret = -EINVAL;
        android::sp<IAGM> agm_client = get_agm_server();
        auto status = agm_client->ipc_agm_get_aif_info_list(num,[&](int32_t _ret,
                                            hidl_vec<AifInfo> aif_list_ret_hidl,
                                            uint32_t num_aif_info_hidl )
        { ret = _ret;
          if (ret != -ENOMEM) {
              if (aif_list != NULL) {
                  for (int i=0 ; i<num_aif_info_hidl ; i++) {
                      strlcpy(aif_list[i].aif_name,
                              aif_list_ret_hidl.data()[i].aif_name.c_str(),
                              AIF_NAME_MAX_LEN);
                      ALOGV("%s : The retrived %d aif_name = %s \n", __func__, i,
                                                              aif_list[i].aif_name);
                      aif_list[i].dir = (enum direction)
                                                    aif_list_ret_hidl.data()[i].dir;
                  }
              }
          *num_aif_info = (size_t) num_aif_info_hidl;
          }
        });
        if (!status.isOk()) {
            ALOGE("%s: HIDL call failed. ret=%d\n", __func__, ret);
        }
        return ret;
    }
    return -EINVAL;
}

int agm_session_aif_get_tag_module_info(uint32_t session_id, uint32_t aif_id,
                                        void *payload, size_t *size)
{
    ALOGV("%s : sess_id = %d, aif_id = %d\n", __func__, session_id, aif_id);
    if (!agm_server_died) {
        android::sp<IAGM> agm_client = get_agm_server();
        uint32_t size_hidl = (uint32_t) *size;
        int ret = 0;
        auto status = agm_client->ipc_agm_session_aif_get_tag_module_info(
                            session_id,
                            aif_id,
                            size_hidl,
                            [&](int32_t _ret,
                                hidl_vec<uint8_t> payload_ret,
                                uint32_t size_ret)
                            { ret = _ret;
                              if (ret != -ENOMEM) {
                                  if (payload != NULL)
                                      memcpy(payload, payload_ret.data(), size_ret);
                                  else if (size_ret == 0)
                                      ALOGE("%s : received NULL Payload",__func__);
                                  *size = (size_t) size_ret;
                              }
                            });
        if (!status.isOk()) {
            ALOGE("%s: HIDL call failed. ret=%d\n", __func__, ret);
        }
        return ret;
    }
    return -EINVAL;
}

int agm_session_get_params(uint32_t session_id, void *payload, size_t size)
{
    ALOGV("%s : sess_id = %d, size = %zu\n", __func__, session_id, size);
    if (!agm_server_died) {
        android::sp<IAGM> agm_client = get_agm_server();
        hidl_vec<uint8_t> buf_hidl;
        int ret = 0;

        buf_hidl.resize(size);
        memcpy(buf_hidl.data(), payload, size);
        auto status = agm_client->ipc_agm_session_get_params(session_id, size, buf_hidl,
                           [&](int32_t _ret, hidl_vec<uint8_t> payload_ret)
                           { ret = _ret;
                             if (!ret) {
                                 if (payload != NULL)
                                     memcpy(payload, payload_ret.data(), size);
                             }
                           });
        if (!status.isOk()) {
            ALOGE("%s: HIDL call failed. ret=%d\n", __func__, ret);
        }
        return ret;
    }
    return -EINVAL;
}

int agm_aif_set_params(uint32_t aif_id, void *payload, size_t size)
{
    ALOGV("%s : aif_id = %d\n", __func__, aif_id);
    if (!agm_server_died) {
        android::sp<IAGM> agm_client = get_agm_server();

        uint32_t size_hidl = (uint32_t) size;
        hidl_vec<uint8_t> payload_hidl;
        payload_hidl.resize(size_hidl);
        memcpy(payload_hidl.data(), payload, size_hidl);

        return agm_client->ipc_agm_aif_set_params(aif_id,
                                            payload_hidl, size_hidl);
    }
    return -EINVAL;
}

int agm_session_aif_set_params(uint32_t session_id, uint32_t aif_id,
                                                    void *payload, size_t size)
{
    ALOGV("%s : sess_id = %d, aif_id = %d\n", __func__, session_id, aif_id);
    if (!agm_server_died) {
        android::sp<IAGM> agm_client = get_agm_server();

        uint32_t size_hidl = (uint32_t) size;
        hidl_vec<uint8_t> payload_hidl;
        payload_hidl.resize(size_hidl);
        memcpy(payload_hidl.data(), payload, size_hidl);

        return agm_client->ipc_agm_session_aif_set_params(session_id,
                                                          aif_id,
                                                          payload_hidl,
                                                          size_hidl);
    }
    return -EINVAL;
}

int agm_session_set_params(uint32_t session_id, void *payload, size_t size)
{
    ALOGV("%s : sess_id = %d, size = %zu\n", __func__, session_id, size);
    if (!agm_server_died) {
        android::sp<IAGM> agm_client = get_agm_server();

        uint32_t size_hidl = (uint32_t) size;
        hidl_vec<uint8_t> payload_hidl;
        payload_hidl.resize(size_hidl);
        memcpy(payload_hidl.data(), payload, size_hidl);
        return agm_client->ipc_agm_session_set_params(session_id,
                                                      payload_hidl,
                                                      size_hidl);
    }
    return -EINVAL;
}

int agm_set_params_with_tag(uint32_t session_id, uint32_t aif_id,
                                             struct agm_tag_config *tag_config)
{
    ALOGV("%s : sess_id = %d, aif_id = %d\n", __func__, session_id, aif_id);
    if (!agm_server_died) {
        android::sp<IAGM> agm_client = get_agm_server();

        hidl_vec<AgmTagConfig> tag_cfg_hidl(1);
        tag_cfg_hidl.data()->tag = tag_config->tag;
        tag_cfg_hidl.data()->num_tkvs = tag_config->num_tkvs;
        tag_cfg_hidl.data()->kv.resize(tag_config->num_tkvs);
        for (int i = 0 ; i < tag_cfg_hidl.data()->num_tkvs ; i++ ) {
             tag_cfg_hidl.data()->kv[i].key = tag_config->kv[i].key;
             tag_cfg_hidl.data()->kv[i].value = tag_config->kv[i].value;
        }
        return agm_client->ipc_agm_set_params_with_tag(session_id,
                                                         aif_id, tag_cfg_hidl);
    }
    return -EINVAL;
}

int agm_set_params_with_tag_to_acdb(uint32_t session_id, uint32_t aif_id,
                                             void *payload, size_t size)
{
    ALOGV("%s : sess_id = %d, size = %zu\n", __func__, session_id, size);
    if (!agm_server_died) {
        android::sp<IAGM> agm_client = get_agm_server();

        uint32_t size_hidl = (uint32_t) size;
        hidl_vec<uint8_t> payload_hidl;
        payload_hidl.resize(size_hidl);
        memcpy(payload_hidl.data(), payload, size_hidl);
        return agm_client->ipc_agm_set_params_with_tag_to_acdb(session_id,
                                    aif_id, payload_hidl, size_hidl);
    }
    return -EINVAL;
}

int agm_set_params_to_acdb_tunnel(void *payload, size_t size)
{
    if (!agm_server_died) {
        android::sp<IAGM> agm_client = get_agm_server();

        uint32_t size_hidl = (uint32_t) size;
        hidl_vec<uint8_t> payload_hidl;
        payload_hidl.resize(size_hidl);
        memcpy(payload_hidl.data(), payload, size_hidl);
        return agm_client->ipc_agm_set_params_to_acdb_tunnel(payload_hidl, size_hidl);
    }
    return -EINVAL;
}

int agm_session_register_for_events(uint32_t session_id,
                                          struct agm_event_reg_cfg *evt_reg_cfg)
{
    ALOGV("%s : sess_id = %d\n", __func__, session_id);
    if (!agm_server_died) {
        android::sp<IAGM> agm_client = get_agm_server();

        hidl_vec<AgmEventRegCfg> evt_reg_cfg_hidl(1);

        evt_reg_cfg_hidl.data()->module_instance_id = evt_reg_cfg->module_instance_id;
        evt_reg_cfg_hidl.data()->event_id = evt_reg_cfg->event_id;
        evt_reg_cfg_hidl.data()->event_config_payload_size = evt_reg_cfg->event_config_payload_size;
        evt_reg_cfg_hidl.data()->is_register = evt_reg_cfg->is_register;
        evt_reg_cfg_hidl.data()->event_config_payload.resize(evt_reg_cfg->event_config_payload_size);
        for (int i = 0; i < evt_reg_cfg->event_config_payload_size; i++)
            evt_reg_cfg_hidl.data()->event_config_payload[i] = evt_reg_cfg->event_config_payload[i];

        return agm_client->ipc_agm_session_register_for_events(session_id,
                                                              evt_reg_cfg_hidl);
    }
    return -EINVAL;
}

int agm_session_aif_set_cal(uint32_t session_id ,uint32_t aif_id ,
                                              struct agm_cal_config *cal_config)
{
    ALOGV("%s : sess_id = %d, aif_id = %d\n", __func__, session_id, aif_id);
    if (!agm_server_died) {
        android::sp<IAGM> agm_client = get_agm_server();

        hidl_vec<AgmCalConfig> cal_cfg_hidl(1);
        cal_cfg_hidl.data()->num_ckvs = cal_config->num_ckvs;
        cal_cfg_hidl.data()->kv.resize(cal_config->num_ckvs);
        for (int i = 0; i < cal_cfg_hidl.data()->num_ckvs; i++) {
            cal_cfg_hidl.data()->kv[i].key = cal_config->kv[i].key;
            cal_cfg_hidl.data()->kv[i].value = cal_config->kv[i].value;
        }
        return agm_client->ipc_agm_session_aif_set_cal(session_id, aif_id,
                                                                  cal_cfg_hidl);
    }
    return -EINVAL;
}

int agm_session_set_ec_ref(uint32_t capture_session_id,
                           uint32_t aif_id, bool state)
{
    ALOGV("%s : cap_sess_id = %d, aif_id = %d\n", __func__,
                                  capture_session_id, aif_id);
    if (!agm_server_died) {
        android::sp<IAGM> agm_client = get_agm_server();
        return agm_client->ipc_agm_session_set_ec_ref(capture_session_id,
                                                       aif_id, state);
    }
    return -EINVAL;
}

int agm_session_register_cb(uint32_t session_id, agm_event_cb cb,
                             enum event_type evt_type, void *client_data)
{
    std::lock_guard<std::mutex> lck(agm_session_register_cb_mutex);
    ALOGV("%s : sess_id = %d, evt_type = %d, client_data = %p \n", __func__,
           session_id, evt_type, client_data);
    int32_t ret = 0;
    if (!agm_server_died) {
#ifndef AGM_HIDL_ENABLED
        sp<IAGMCallback> ClbkBinder = NULL;
#endif
        ClntClbk *cl_clbk_data = NULL;
        uint64_t cl_clbk_data_add = 0;
        android::sp<IAGM> agm_client = get_agm_server();
#ifndef AGM_HIDL_ENABLED
        if (!is_cb_registered) {
            ClbkBinder = new AGMCallback();
            ret = agm_client->ipc_agm_client_register_callback(ClbkBinder);
            if (ret) {
                ALOGE("Client callback registration failed");
                return ret;
            }
            is_cb_registered = true;
        }
#else //AGM_HIDL_ENABLED
        if (!ClbkBinder)
            ClbkBinder = new AGMCallback();
        ret = agm_client->ipc_agm_client_register_callback(ClbkBinder);
        if (ret) {
            ALOGE("Client callback registration failed");
            return ret;
        }
#endif
        if (cb != NULL) {
            cl_clbk_data = new ClntClbk(session_id, cb, evt_type, client_data);
            struct client_cb_data *cb_data = (struct client_cb_data *)calloc(1, sizeof(struct client_cb_data));
            if (!cb_data) {
                delete cl_clbk_data;
                return -ENOMEM;
            }
            cl_clbk_data_add = (uint64_t) cl_clbk_data;
            pthread_mutex_lock(&clbk_data_list_lock);
            cb_data->data = cl_clbk_data_add;
            list_add_tail(&client_clbk_data_list, &cb_data->node);
            pthread_mutex_unlock(&clbk_data_list_lock);
        } else {
            struct listnode *node = NULL, *tempnode = NULL;
            struct client_cb_data *cb_data = NULL;
            pthread_mutex_lock(&clbk_data_list_lock);
            list_for_each_safe(node, tempnode, &client_clbk_data_list) {
                cb_data = node_to_item(node, struct client_cb_data, node);
                cl_clbk_data = (ClntClbk *) cb_data->data;
                if ((cl_clbk_data->session_id == session_id) &&
                    (cl_clbk_data->event == evt_type) &&
                    (cl_clbk_data->clnt_data == client_data)) {
                    list_remove(node);
                    delete cl_clbk_data;
                    cl_clbk_data = NULL;
                    free(cb_data);
                    break;
                }
            }
            pthread_mutex_unlock(&clbk_data_list_lock);
        }
        int ret = agm_client->ipc_agm_session_register_callback(
                                                        session_id,
                                                        evt_type,
                                                        cl_clbk_data_add,
                                                        (uint64_t )client_data);
        return ret;
    }
    return -EINVAL;
}

int agm_session_eos(uint64_t handle)
{
    ALOGV("%s called with handle = %llx \n", __func__, (unsigned long long) handle);
    if (!agm_server_died) {
        android::sp<IAGM> agm_client = get_agm_server();
        return agm_client->ipc_agm_session_eos(handle);
    }
    return -EINVAL;
}

int agm_get_session_time(uint64_t handle, uint64_t *timestamp)
{
    ALOGV("%s called with handle = %llx \n", __func__, (unsigned long long) handle);
    if (!agm_server_died) {
        android::sp<IAGM> agm_client = get_agm_server();
        int ret = -EINVAL;
        auto status = agm_client->ipc_agm_get_session_time(handle,
                                             [&](int _ret, uint64_t ts)
                                             { ret = _ret;
                                               *timestamp = ts;
                                             });
        if (!status.isOk()) {
            ALOGE("%s: HIDL call failed. ret=%d\n", __func__, ret);
        }
    }
    return -EINVAL;
}

int agm_get_buffer_timestamp(uint32_t session_id, uint64_t *timestamp)
{
    ALOGV("%s: session_id = %x\n", __func__, session_id);
    int ret = -EINVAL;
    if (!agm_server_died) {
        android::sp<IAGM> agm_client = get_agm_server();
        auto status = agm_client->ipc_agm_get_buffer_timestamp(session_id,
                                             [&](int _ret, uint64_t ts)
                                             { ret = _ret;
                                               *timestamp = ts;
                                             });
        if (!status.isOk()) {
            ALOGE("%s: HIDL call failed. ret=%d\n", __func__, ret);
        }
    }
    return ret;
}

int agm_session_get_buf_info(uint32_t session_id, struct agm_buf_info *buf_info, uint32_t flag)
{
    ALOGV("%s : session_id = %d\n", __func__, session_id);
    int ret = -EINVAL;
    if (!agm_server_died) {
        android::sp<IAGM> agm_client = get_agm_server();
        const native_handle *datahandle = nullptr;
        const native_handle *poshandle = nullptr;

        auto status = agm_client->ipc_agm_session_get_buf_info(session_id, flag,
                [&](int32_t _ret, const MmapBufInfo& buf_info_ret_hidl)
                { ret = _ret;
                if (!ret) {
                if (flag & DATA_BUF) {
                datahandle = buf_info_ret_hidl.dataSharedMemory.handle();
                buf_info->data_buf_fd = dup(datahandle->data[0]);
                buf_info->data_buf_size = buf_info_ret_hidl.data_size;
                }
                if (flag & POS_BUF) {
                poshandle = buf_info_ret_hidl.posSharedMemory.handle();
                buf_info->pos_buf_fd = poshandle->data[0];
                buf_info->pos_buf_size = buf_info_ret_hidl.pos_size;
                }
                }
                });
        if (!status.isOk()) {
            ALOGE("%s: HIDL call failed. ret=%d\n", __func__, ret);
        }
    }
    return ret;
}

int agm_set_gapless_session_metadata(uint64_t handle, enum agm_gapless_silence_type type,
                                     uint32_t silence)
{
    ALOGV("%s called with handle = %x \n", __func__, handle);
    if (!agm_server_died) {
        android::sp<IAGM> agm_client = get_agm_server();
        AgmGaplessSilenceType type_hidl = (AgmGaplessSilenceType) type;
        return agm_client->ipc_agm_set_gapless_session_metadata(handle,
                                                 type_hidl,
                                                 silence);
    }
    return -EINVAL;
}

int agm_session_set_non_tunnel_mode_config(uint64_t handle,
                            struct agm_session_config *session_config,
                            struct agm_media_config *in_media_config,
                            struct agm_media_config *out_media_config,
                            struct agm_buffer_config *in_buffer_config,
                            struct agm_buffer_config *out_buffer_config) {

    ALOGV("%s called with handle = %llx \n", __func__, (unsigned long long)handle);

    if (!agm_server_died) {
        android::sp<IAGM> agm_client = get_agm_server();
        hidl_vec<AgmSessionConfig> session_config_hidl(1);
        memcpy(session_config_hidl.data(),
               session_config,
               sizeof(struct agm_session_config));

        hidl_vec<AgmMediaConfig> in_media_config_hidl(1), out_media_config_hidl(1);
        in_media_config_hidl.data()->rate = in_media_config->rate;
        in_media_config_hidl.data()->channels = in_media_config->channels;
        in_media_config_hidl.data()->format =
                       (::vendor::qti::hardware::AGMIPC::V1_0::AgmMediaFormat) in_media_config->format;
        in_media_config_hidl.data()->data_format = in_media_config->data_format;

        out_media_config_hidl.data()->rate = out_media_config->rate;
        out_media_config_hidl.data()->channels = out_media_config->channels;
        out_media_config_hidl.data()->format =
                       (::vendor::qti::hardware::AGMIPC::V1_0::AgmMediaFormat) out_media_config->format;
        out_media_config_hidl.data()->data_format = out_media_config->data_format;

        hidl_vec<AgmBufferConfig> in_buffer_config_hidl(1), out_buffer_config_hidl(1);
        in_buffer_config_hidl.data()->count = in_buffer_config->count;
        in_buffer_config_hidl.data()->size = in_buffer_config->size;
        in_buffer_config_hidl.data()->max_metadata_size = in_buffer_config->max_metadata_size;

        out_buffer_config_hidl.data()->count = out_buffer_config->count;
        out_buffer_config_hidl.data()->size = out_buffer_config->size;
        out_buffer_config_hidl.data()->max_metadata_size = out_buffer_config->max_metadata_size;

        ALOGV("%s : Exit", __func__);
        return agm_client->ipc_agm_session_set_non_tunnel_mode_config(handle,
                                                      session_config_hidl,
                                                      in_media_config_hidl,
                                                      out_media_config_hidl,
                                                      in_buffer_config_hidl,
                                                      out_buffer_config_hidl);
    }
    return -EINVAL;
}

int agm_session_write_with_metadata(uint64_t handle, struct agm_buff *buf, size_t *consumed_size)
{
    ALOGV("%s called with handle = %x \n", __func__, handle);
    int32_t ret = -EINVAL;

    if (!agm_server_died) {
        ALOGV("%s:%d hndl %p",__func__, __LINE__, handle);
        android::sp<IAGM> agm_client = get_agm_server();
        hidl_vec<AgmBuff> buf_hidl(1);
        native_handle_t *allocHidlHandle = nullptr;
        allocHidlHandle = native_handle_create(1, 1);
        if (!allocHidlHandle) {
            ALOGE("%s native_handle_create fails", __func__);
            ret = -ENOMEM;
            goto done;
        }
        allocHidlHandle->data[0] = buf->alloc_info.alloc_handle;
        allocHidlHandle->data[1] = buf->alloc_info.alloc_handle;
        AgmBuff *agmBuff = buf_hidl.data();
        agmBuff->size = buf->size;
        agmBuff->buffer.resize(buf->size);
        agmBuff->flags = buf->flags;
        agmBuff->timestamp = buf->timestamp;
        if (buf->size && buf->addr)
            memcpy(agmBuff->buffer.data(), buf->addr, buf->size);
        if ((buf->metadata_size > 0) && buf->metadata) {
            agmBuff->metadata_size = buf->metadata_size;
            agmBuff->metadata.resize(buf->metadata_size);
            memcpy(agmBuff->metadata.data(),
                   buf->metadata, buf->metadata_size);
         }
         agmBuff->alloc_info.alloc_handle = hidl_memory("ar_alloc_handle", hidl_handle(allocHidlHandle),
                    buf->alloc_info.alloc_size);

         ALOGV("%s:%d: fd [0] %d fd [1] %d", __func__,__LINE__, allocHidlHandle->data[0], allocHidlHandle->data[1]);
         agmBuff->alloc_info.alloc_size = buf->alloc_info.alloc_size;
         agmBuff->alloc_info.offset = buf->alloc_info.offset;
         auto status = agm_client->ipc_agm_session_write_with_metadata(
                                            handle, buf_hidl,
                                            *consumed_size,
                                      [&](int32_t _ret, uint32_t cnt)
                                           {
                                             ret = _ret;
                                             if (ret != -ENOMEM)
                                                 *consumed_size = (size_t) cnt;
                                           });
        if (!status.isOk()) {
            ALOGE("%s: HIDL call failed. ret=%d\n", __func__, ret);
        }
        if (allocHidlHandle)
            native_handle_delete(allocHidlHandle);
    }
done:
    return ret;
}

int agm_session_read_with_metadata(uint64_t handle, struct agm_buff  *buf, uint32_t *captured_size)
{
    int ret = -EINVAL;
    ALOGV("%s:%d size %d", __func__, __LINE__, buf->size);

    if (!handle)
        goto done;

    if (!agm_server_died) {
        android::sp<IAGM> agm_client = get_agm_server();
        native_handle_t *allocHidlHandle = nullptr;
        allocHidlHandle = native_handle_create(1, 1);
        if (!allocHidlHandle) {
            ALOGE("%s native_handle_create fails", __func__);
            ret = -ENOMEM;
            goto done;
        }
        allocHidlHandle->data[0] = buf->alloc_info.alloc_handle;
        allocHidlHandle->data[1] = buf->alloc_info.alloc_handle;

        hidl_vec<AgmBuff> buf_hidl(1);
        AgmBuff *agmBuff = buf_hidl.data();
        agmBuff->size = buf->size;
        agmBuff->metadata_size = buf->metadata_size;
        agmBuff->alloc_info.alloc_handle = hidl_memory("ar_alloc_handle", hidl_handle(allocHidlHandle),
                    buf->alloc_info.alloc_size);
        agmBuff->alloc_info.alloc_size = buf->alloc_info.alloc_size;
        agmBuff->alloc_info.offset = buf->alloc_info.offset;

        ALOGV("%s:%d size %d %d", __func__, __LINE__, buf_hidl.data()->size, buf->size);
        ALOGV("%s:%d: fd [0] %d fd [1] %d", __func__,__LINE__, allocHidlHandle->data[0], allocHidlHandle->data[1]);
        auto status = agm_client->ipc_agm_session_read_with_metadata(handle, buf_hidl, *captured_size,
               [&](int32_t ret_, hidl_vec<AgmBuff> ret_buf_hidl, uint32_t captured_size_ret)
                  {
                      if (ret_ > 0) {
                          if (ret_buf_hidl.data()->size > buf->size) {
                              ALOGE("ret buf sz %d bigger than request buf sz %d",
                                     ret_buf_hidl.data()->size, buf->size);
                              ret_ = -ENOMEM;
                           } else {
                                if ((buf->metadata_size > 0) && buf->metadata)
                                     memcpy(buf->metadata,
                                           ret_buf_hidl.data()->metadata.data(),
                                           ret_buf_hidl.data()->metadata_size);
                                     buf->timestamp = ret_buf_hidl.data()->timestamp;
                           }
                           buf->flags = ret_buf_hidl.data()->flags;
                           if (buf->addr)
                               memcpy(buf->addr,
                                      ret_buf_hidl.data()->buffer.data(),
                                       buf->size);
                           *captured_size = captured_size_ret;
                      }
                      ret = ret_;
                  });
        if (!status.isOk()) {
            ALOGE("%s: HIDL call failed. ret=%d\n", __func__, ret);
        }
        if (allocHidlHandle)
            native_handle_delete(allocHidlHandle);
    }
done:
    return ret;
}

int agm_aif_group_set_media_config(uint32_t group_id,
                                struct agm_group_media_config *media_config)
{
    ALOGV("%s called, group_id = %d \n", __func__, group_id);
    if (!agm_server_died) {
        android::sp<IAGM> agm_client = get_agm_server();
        hidl_vec<AgmGroupMediaConfig> media_config_hidl(1);
        media_config_hidl.data()->rate = media_config->config.rate;
        media_config_hidl.data()->channels = media_config->config.channels;
        media_config_hidl.data()->format = (::vendor::qti::hardware::AGMIPC::V1_0::AgmMediaFormat) media_config->config.format;
        media_config_hidl.data()->data_format = media_config->config.data_format;
        media_config_hidl.data()->slot_mask = media_config->slot_mask;
        return agm_client->ipc_agm_aif_group_set_media_config(group_id,
                                                        media_config_hidl);
    }
    return -EINVAL;
}

int agm_get_group_aif_info_list(struct aif_info *aif_list, size_t *num_groups)
{
    ALOGV("%s called \n", __func__);
    if (!agm_server_died) {
        uint32_t num = (uint32_t) *num_groups;
        int ret = -EINVAL;
        android::sp<IAGM> agm_client = get_agm_server();
        auto status = agm_client->ipc_agm_get_group_aif_info_list(num,[&](int32_t _ret,
                                            hidl_vec<AifInfo> aif_list_ret_hidl,
                                            uint32_t num_groups_hidl )
        { ret = _ret;
          if (ret != -ENOMEM) {
              if (aif_list != NULL) {
                  for (int i = 0 ; i < num_groups_hidl ; i++) {
                      strlcpy(aif_list[i].aif_name,
                              aif_list_ret_hidl.data()[i].aif_name.c_str(),
                              AIF_NAME_MAX_LEN);
                      ALOGV("%s : The retrived %d aif_name = %s \n", __func__, i,
                                                              aif_list[i].aif_name);
                      aif_list[i].dir = (enum direction)
                                                    aif_list_ret_hidl.data()[i].dir;
                  }
              }
          *num_groups = (size_t) num_groups_hidl;
          }
        });
        if (!status.isOk()) {
            ALOGE("%s: HIDL call failed. ret=%d\n", __func__, ret);
        }
        return ret;
    }
    return -EINVAL;
}

int agm_session_write_datapath_params(uint32_t session_id, struct agm_buff *buf)
{
    ALOGV("%s called with session id = %d \n", __func__, session_id);

    if (!agm_server_died) {
        android::sp<IAGM> agm_client = get_agm_server();
        hidl_vec<AgmBuff> buf_hidl(1);
        AgmBuff *agmBuff = buf_hidl.data();
        agmBuff->size = buf->size;
        agmBuff->buffer.resize(buf->size);
        agmBuff->flags = buf->flags;
        agmBuff->timestamp = buf->timestamp;
        if (buf->size && buf->addr)
            memcpy(agmBuff->buffer.data(), buf->addr, buf->size);
        else {
            ALOGE("%s: buf size or addr is null", __func__);
            return -EINVAL;
        }

        return agm_client->ipc_agm_session_write_datapath_params(
                                            session_id, buf_hidl);
    }
    return -EINVAL;
}

int agm_dump(struct agm_dump_info *dump_info) {
    if (agm_server_died) {
        ALOGE("%s: Cannot perform dump, AGM service has died", __func__);
        return -EINVAL;
    }
    android::sp<IAGM> agm_client = get_agm_server();
    hidl_vec<AgmDumpInfo> dump_info_hidl;
    dump_info_hidl.resize(1);
    memcpy(dump_info_hidl.data(),
            dump_info,
            sizeof(struct agm_dump_info));
    return agm_client->ipc_agm_dump(dump_info_hidl);
}
