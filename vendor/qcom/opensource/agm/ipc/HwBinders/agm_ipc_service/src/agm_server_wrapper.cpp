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

#define LOG_TAG "agm_server_wrapper"
#include "inc/agm_server_wrapper.h"
#include <log/log.h>
#include <cutils/list.h>
#include <pthread.h>
#include "gsl_intf.h"
#include <hwbinder/IPCThreadState.h>

#define MAX_CACHE_SIZE 64
using AgmCallbackData = ::vendor::qti::hardware::AGMIPC::V1_0::implementation::clbk_data;
using AgmServerCallback = ::vendor::qti::hardware::AGMIPC::V1_0::implementation::SrvrClbk;

static list_declare(client_list);
static pthread_mutex_t client_list_lock = PTHREAD_MUTEX_INITIALIZER;

static list_declare(clbk_data_list);
static pthread_mutex_t clbk_data_list_lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
   struct listnode list;
   uint32_t session_id;
   uint64_t handle;
   std::vector<std::pair<int, int>> shared_mem_fd_list;
} agm_client_session_handle;

typedef struct {
    struct listnode list;
    uint32_t pid;
    android::sp<IAGMCallback> clbk_binder;
    struct listnode agm_client_hndl_list;
} client_info;

void client_death_notifier::serviceDied(uint64_t cookie,
                   const android::wp<::android::hidl::base::V1_0::IBase>& who __unused)
{
    ALOGI("Client died (pid): %llu",(unsigned long long) cookie);
    struct listnode *node = NULL;
    struct listnode *tempnode = NULL;
    agm_client_session_handle *hndl = NULL;
    client_info *handle = NULL;
    struct listnode *sess_node = NULL;
    struct listnode *sess_tempnode = NULL;

    AgmCallbackData* clbk_data_hndl = NULL;
    pthread_mutex_lock(&clbk_data_list_lock);
    list_for_each_safe(node, tempnode, &clbk_data_list) {
        clbk_data_hndl = node_to_item(node, AgmCallbackData, list);
        if (clbk_data_hndl->srv_clt_data->pid == cookie) {
            ALOGV("%s pid matched %d ", __func__, clbk_data_hndl->srv_clt_data->pid);
            AgmServerCallback* tmp_sr_clbk_data = clbk_data_hndl->srv_clt_data;
            /*Unregister this callback from session_obj*/
            agm_session_register_cb(tmp_sr_clbk_data->session_id,
                                    NULL,
                                    (enum event_type)tmp_sr_clbk_data->event,
                                    tmp_sr_clbk_data),
            list_remove(node);
            delete tmp_sr_clbk_data;
            free(clbk_data_hndl);
        }
    }
    pthread_mutex_unlock(&clbk_data_list_lock);

    pthread_mutex_lock(&client_list_lock);
    list_for_each_safe(node, tempnode, &client_list) {
        handle = node_to_item(node, client_info, list);
        if (handle->pid == cookie) {
            ALOGV("%s: MATCHED pid = %llu\n", __func__, (unsigned long long) cookie);
            list_for_each_safe(sess_node, sess_tempnode,
                                      &handle->agm_client_hndl_list) {
                hndl = node_to_item(sess_node, agm_client_session_handle, list);
                if (hndl->handle) {
                    agm_session_close(hndl->handle);
                    list_remove(sess_node);
                    hndl->shared_mem_fd_list.clear();
                    free(hndl);
                }
            }
            list_remove(node);
            free(handle);
        }
    }
    pthread_mutex_unlock(&client_list_lock);
    ALOGV("%s: exit\n", __func__);
}

void add_fd_to_list(uint64_t sess_handle, int input_fd, int dup_fd)
{
    struct listnode *node = NULL;
    struct listnode *tempnode = NULL;
    agm_client_session_handle *hndle = NULL;
    client_info *handle = NULL;
    struct listnode *sess_node = NULL;
    struct listnode *sess_tempnode = NULL;
    std::vector<std::pair<int, int>>::iterator it;

    pthread_mutex_lock(&client_list_lock);
    list_for_each_safe(node, tempnode, &client_list) {
        handle = node_to_item(node, client_info, list);
        list_for_each_safe(sess_node, sess_tempnode,
                                     &handle->agm_client_hndl_list) {
            hndle = node_to_item(sess_node,
                                     agm_client_session_handle,
                                     list);
            if (hndle->handle == sess_handle) {
                if (hndle->shared_mem_fd_list.size() > MAX_CACHE_SIZE) {
                    close(hndle->shared_mem_fd_list.front().second);
                    it = hndle->shared_mem_fd_list.begin();
                    hndle->shared_mem_fd_list.erase(it);
                }
                hndle->shared_mem_fd_list.push_back(std::make_pair(input_fd, dup_fd));
                ALOGV("sess_handle %x, session_id:%d input_fd %d, dup fd %d", hndle->handle,
                       hndle->session_id, input_fd, dup_fd);
            }
         }
    }
    pthread_mutex_unlock(&client_list_lock);
}

static void add_handle_to_list(uint32_t session_id, uint64_t handle)
{
    struct listnode *node = NULL;
    client_info *client_handle = NULL;
    client_info *client_handle_temp = NULL;
    agm_client_session_handle *hndl = NULL;
    int pid = ::android::hardware::IPCThreadState::self()->getCallingPid();
    int flag = 0;

    pthread_mutex_lock(&client_list_lock);
    list_for_each(node, &client_list) {
        client_handle_temp = node_to_item(node, client_info, list);
        if (client_handle_temp->pid == pid) {
            hndl = (agm_client_session_handle *)
                               calloc(1, sizeof(agm_client_session_handle));  
            if (hndl == NULL) {
                ALOGE("%s: Cannot allocate memory for agm handle\n", __func__);
                goto exit;
            }
            hndl->handle = handle;
            hndl->session_id = session_id;
            ALOGV("%s: Adding session id %d and handle %x to client handle list \n", __func__, session_id, handle);
            list_add_tail(&client_handle_temp->agm_client_hndl_list, &hndl->list);
            flag = 1;
            break;
        }
    }
    if (flag == 0) {
        client_handle = (client_info *)calloc(1, sizeof(client_info));
        if (client_handle == NULL) {
            ALOGE("%s: Cannot allocate memory for client handle\n", __func__);
            goto exit;
        }
        client_handle->pid = pid;
        list_add_tail(&client_list, &client_handle->list);
        list_init(&client_handle->agm_client_hndl_list);
        hndl = (agm_client_session_handle *)calloc(1, sizeof(agm_client_session_handle));
        if (hndl == NULL) {
            ALOGE("%s: Cannot allocate memory to store agm session handle\n", __func__);
            goto exit;
        }
        hndl->handle = handle;
        hndl->session_id = session_id;
        ALOGV("%s: Adding session id %d and handle %x to client handle list \n", __func__, session_id, handle);
        list_add_tail(&client_handle->agm_client_hndl_list, &hndl->list);
    }
exit :
        pthread_mutex_unlock(&client_list_lock);
}

namespace vendor {
namespace qti {
namespace hardware {
namespace AGMIPC {
namespace V1_0 {
namespace implementation {

void ipc_callback (uint32_t session_id,
                   struct agm_event_cb_params *evt_param,
                   void *client_data)
{
    ALOGV("%s called with sess_id = %d, client_data = %p \n", __func__,
          session_id, client_data);
    SrvrClbk *sr_clbk_dat;
    sr_clbk_dat = (SrvrClbk *)client_data;
    sp<IAGMCallback> clbk_bdr = NULL;
    struct listnode *node = NULL;
    struct listnode *tempnode = NULL;
    hidl_vec<AgmEventCbParams> evt_param_l;
    hidl_vec<AgmReadWriteEventCbParams> rw_evt_param_hidl;
    AgmReadWriteEventCbParams *rw_evt_param = NULL;
    AgmEventReadWriteDonePayload *rw_payload = NULL;
    struct gsl_event_read_write_done_payload *rw_done_payload;
    uint32_t eventId = evt_param->event_id;
    client_info *client_obj = NULL;

    pthread_mutex_lock(&client_list_lock);
    list_for_each_safe(node, tempnode, &client_list) {
        client_obj = node_to_item(node, client_info, list);
        if (client_obj->pid == sr_clbk_dat->pid) {
            clbk_bdr = client_obj->clbk_binder;
            break;
        }
    }
    pthread_mutex_unlock(&client_list_lock);
    if (!client_obj || !clbk_bdr) {
        ALOGE("Failed to get client data for pid %d", sr_clbk_dat->pid);
        return;
    }
    /*
     *In case of EXTERN_MEM and SHMEM mode only we have a
     *valid payload for READ/WRITE_DONE event
     */
    if ((evt_param->event_payload_size > 0) && ((eventId == AGM_EVENT_READ_DONE) ||
                               (eventId == AGM_EVENT_WRITE_DONE))) {

        /*
         *iterate over list of sessions to find the original fd that was input
         *during write/read
        */
        struct listnode *node = NULL;
        struct listnode *tempnode = NULL;
        agm_client_session_handle *hndle = NULL;
        struct listnode *sess_node = NULL;
        struct listnode *sess_tempnode = NULL;
        int input_fd = -1;

        rw_done_payload = (struct gsl_event_read_write_done_payload *)evt_param->event_payload;

        pthread_mutex_lock(&client_list_lock);
        list_for_each_safe(sess_node, sess_tempnode,
                                     &client_obj->agm_client_hndl_list) {
            hndle = node_to_item(sess_node,
                                     agm_client_session_handle,
                                     list);
            if (hndle->session_id == session_id) {
                std::vector<std::pair<int, int>>::iterator it;
                for (int i = 0; i < hndle->shared_mem_fd_list.size(); i++) {
                     ALOGV("fd_list [input - dup ] [%d %d] list size %d \n",
                           hndle->shared_mem_fd_list[i].first, hndle->shared_mem_fd_list[i].second,
                           hndle->shared_mem_fd_list.size());
                     if (hndle->shared_mem_fd_list[i].second == rw_done_payload->buff.alloc_info.alloc_handle) {
                         input_fd = hndle->shared_mem_fd_list[i].first;
                         it = (hndle->shared_mem_fd_list.begin() + i);
                         if (it != hndle->shared_mem_fd_list.end())
                             hndle->shared_mem_fd_list.erase(it);
                         ALOGV("input fd %d  payload fd %d\n", input_fd,
                               rw_done_payload->buff.alloc_info.alloc_handle);
                         break;
                     }
                }
            }
        }
        pthread_mutex_unlock(&client_list_lock);
        /*
         *In case of EXTERN_MEM and SHMEM mode only we have a
         *valid payload for READ/WRITE_DONE event, as of now we dont
         *support SHMEM mode on LX
         */
        native_handle_t *allocHidlHandle = nullptr;
        allocHidlHandle = native_handle_create(1, 1);
        if (!allocHidlHandle) {
            ALOGE("%s native_handle_create fails", __func__);
            return;
        }
        rw_evt_param_hidl.resize(sizeof(struct AgmReadWriteEventCbParams));
        rw_evt_param = rw_evt_param_hidl.data();
        rw_evt_param->source_module_id = evt_param->source_module_id;
        rw_evt_param->event_payload_size = sizeof(struct agm_event_read_write_done_payload);
        rw_evt_param->event_id = evt_param->event_id;
        rw_evt_param->rw_payload.resize(sizeof(struct agm_event_read_write_done_payload));
        rw_payload = rw_evt_param->rw_payload.data();
        rw_payload->tag = rw_done_payload->tag;
        rw_payload->md_status = rw_done_payload->md_status;
        rw_payload->status = rw_done_payload->status;
        rw_payload->buff.timestamp = rw_done_payload->buff.timestamp;
        rw_payload->buff.flags = rw_done_payload->buff.flags;
        rw_payload->buff.size = rw_done_payload->buff.size;

        allocHidlHandle->data[0] = rw_done_payload->buff.alloc_info.alloc_handle;
        allocHidlHandle->data[1] = input_fd;
        rw_payload->buff.alloc_info.alloc_handle = hidl_memory("ar_alloc_handle", hidl_handle(allocHidlHandle),
                                           rw_done_payload->buff.alloc_info.alloc_size);
        rw_payload->buff.alloc_info.alloc_size = rw_done_payload->buff.alloc_info.alloc_size;
        rw_payload->buff.alloc_info.offset = rw_done_payload->buff.alloc_info.offset;
        if (rw_done_payload->buff.metadata_size > 0) {
            rw_payload->buff.metadata_size = rw_done_payload->buff.metadata_size;
            rw_payload->buff.metadata.resize(rw_done_payload->buff.metadata_size);
            memcpy(rw_payload->buff.metadata.data(), rw_done_payload->buff.metadata,
                   rw_done_payload->buff.metadata_size);
        }
        auto status = clbk_bdr->event_callback_rw_done(session_id, rw_evt_param_hidl,
                                  sr_clbk_dat->get_clnt_data());
        if (!status.isOk()) {
            ALOGE("%s: HIDL call failed.\n", __func__);
        }

        // allocated during read_with_metadata()
        if (rw_done_payload->buff.metadata)
            free(rw_done_payload->buff.metadata);
        close(rw_done_payload->buff.alloc_info.alloc_handle);
    } else {
        evt_param_l.resize(sizeof(struct agm_event_cb_params) +
                                evt_param->event_payload_size);
        evt_param_l.data()->source_module_id = evt_param->source_module_id;
        evt_param_l.data()->event_payload_size = evt_param->event_payload_size;
        evt_param_l.data()->event_id = evt_param->event_id;
        evt_param_l.data()->event_payload.resize(evt_param->event_payload_size);
        int8_t *dst = (int8_t *)evt_param_l.data()->event_payload.data();
        int8_t *src = (int8_t *)evt_param->event_payload;
        memcpy(dst, src, evt_param->event_payload_size);
        auto status = clbk_bdr->event_callback(session_id, evt_param_l,
                                  sr_clbk_dat->get_clnt_data());
        if (!status.isOk()) {
            ALOGE("%s: HIDL call failed.\n", __func__);
        }
    }
}
// Methods from ::vendor::qti::hardware::AGMIPC::V1_0::IAGM follow.
Return<int32_t> AGM::ipc_agm_init() {
    ALOGV("%s called \n", __func__);
    return 0;
}

Return<int32_t> AGM::ipc_agm_deinit() {
    ALOGV("%s called \n", __func__);
    return 0;
}

Return<int32_t> AGM::ipc_agm_aif_set_media_config(uint32_t aif_id,
                                 const hidl_vec<AgmMediaConfig>& media_config) {
    ALOGV("%s called with aif_id = %d \n", __func__, aif_id);
    struct agm_media_config *med_config_l = NULL;
    med_config_l =
          (struct agm_media_config*)calloc(1, sizeof(struct agm_media_config));
    if (med_config_l == NULL) {
        ALOGE("%s: Cannot allocate memory for med_config_l\n", __func__);
        return -ENOMEM;
    }
    med_config_l->rate = media_config.data()->rate;
    med_config_l->channels = media_config.data()->channels;
    med_config_l->format = (enum agm_media_format) media_config.data()->format;
    med_config_l->data_format = media_config.data()->data_format;
    int32_t ret = agm_aif_set_media_config (aif_id, med_config_l);
    free(med_config_l);
    return ret;
}

Return<int32_t> AGM::ipc_agm_aif_set_metadata(uint32_t aif_id,
                                            uint32_t size,
                                            const hidl_vec<uint8_t>& metadata) {
    ALOGV("%s called with aif_id = %d, size = %d\n", __func__, aif_id, size);
    uint8_t * metadata_l = NULL;
    int32_t ret = 0;
    metadata_l = (uint8_t *) calloc(1,size);
    if (metadata_l == NULL) {
        ALOGE("%s: Cannot allocate memory for metadata_l\n", __func__);
        return -ENOMEM;
    }
    memcpy(metadata_l, metadata.data(), size);
    ret = agm_aif_set_metadata(aif_id, size, metadata_l);
    free(metadata_l);
    return ret;
}

Return<int32_t> AGM::ipc_agm_session_set_metadata(uint32_t session_id,
                                            uint32_t size,
                                            const hidl_vec<uint8_t>& metadata) {
    ALOGV("%s : session_id = %d, size = %d\n", __func__, session_id, size);
    uint8_t * metadata_l = NULL;
    int32_t ret = 0;
    metadata_l = (uint8_t *) calloc(1,size);
    if (metadata_l == NULL) {
        ALOGE("%s: Cannot allocate memory for metadata_l\n", __func__);
        return -ENOMEM;
    }
    memcpy(metadata_l, metadata.data(), size);
    ret = agm_session_set_metadata(session_id, size, metadata_l);
    free(metadata_l);
    return ret;
}

Return<int32_t> AGM::ipc_agm_session_aif_set_metadata(uint32_t session_id,
                                            uint32_t aif_id,
                                            uint32_t size,
                                            const hidl_vec<uint8_t>& metadata) {
    ALOGV("%s : session_id = %d, aif_id =%d, size = %d\n", __func__,
                                                      session_id, aif_id, size);
    uint8_t * metadata_l = NULL;
    int32_t ret = 0;
    metadata_l = (uint8_t *) calloc(1,size);
    if (metadata_l == NULL) {
        ALOGE("%s: Cannot allocate memory for metadata_l\n", __func__);
        return -ENOMEM;
    }
    memcpy(metadata_l, metadata.data(), size);
    ret = agm_session_aif_set_metadata(session_id, aif_id, size, metadata_l);
    free(metadata_l);
    return ret;
}

Return<int32_t> AGM::ipc_agm_session_aif_connect(uint32_t session_id,
                                                 uint32_t aif_id,
                                                 bool state) {
    ALOGV("%s : session_id = %d, aif_id =%d, state = %s\n", __func__,
                          session_id, aif_id, state ? "true" : "false");
    return agm_session_aif_connect(session_id, aif_id, state);
}

Return<void> AGM::ipc_agm_session_aif_get_tag_module_info(uint32_t session_id,
                          uint32_t aif_id,
                          uint32_t size,
                          ipc_agm_session_aif_get_tag_module_info_cb _hidl_cb) {
    ALOGV("%s : session_id = %d, aif_id =%d, size = %d\n", __func__,
                                                      session_id, aif_id, size);
    uint8_t * payload_local = NULL;
    size_t size_local;
    size_local = (size_t) size;
    hidl_vec<uint8_t> payload_hidl;
    if (size_local) {
        payload_local = (uint8_t *) calloc (1, size_local);
        if (payload_local == NULL) {
            ALOGE("%s: Cannot allocate memory for payload_local\n", __func__);
            _hidl_cb(-ENOMEM, payload_hidl, size);
            return Void();
        }
    }
    int32_t ret = agm_session_aif_get_tag_module_info(session_id,
                                                      aif_id,
                                                      payload_local,
                                                      &size_local);
    payload_hidl.resize(size_local);
    if (payload_local)
        memcpy(payload_hidl.data(), payload_local, size_local);
    uint32_t size_hidl = (uint32_t) size_local;
    _hidl_cb(ret, payload_hidl, size_hidl);
    free(payload_local);
    return Void();
}

Return<void> AGM::ipc_agm_session_get_params(uint32_t session_id,
                                     uint32_t size,
                                     const hidl_vec<uint8_t>& buff,
                                     ipc_agm_session_get_params_cb _hidl_cb) {
    ALOGV("%s : session_id = %d, size = %d\n", __func__, session_id, size);
    uint8_t * payload_local = NULL;
    int32_t ret = 0;
    hidl_vec<uint8_t> payload_hidl;

    payload_local = (uint8_t *) calloc (1, size);
    if (payload_local == NULL) {
        ALOGE("%s: Cannot allocate memory for payload_local\n", __func__);
        _hidl_cb(-ENOMEM, payload_hidl);
        return Void();
    }

    memcpy(payload_local, buff.data(), size);
    payload_hidl.resize((size_t)size);
    ret = agm_session_get_params(session_id, payload_local, (size_t)size);
    if (!ret)
       memcpy(payload_hidl.data(), payload_local, (size_t)size);

     _hidl_cb(ret, payload_hidl);
    free(payload_local);
    return Void();
}

Return<int32_t> AGM::ipc_agm_aif_set_params(uint32_t aif_id,
                                            const hidl_vec<uint8_t>& payload,
                                            uint32_t size)
{
    size_t size_local = (size_t) size;
    void * payload_local = NULL;
    int32_t ret = 0;

    ALOGV("%s : aif_id =%d, size = %d\n", __func__, aif_id, size);
    payload_local = (void*) calloc (1,size);
    if (payload_local == NULL) {
        ALOGE("%s: calloc failed for payload_local\n", __func__);
        return -ENOMEM;
    }
    memcpy(payload_local, payload.data(), size);
    ret = agm_aif_set_params(aif_id, payload_local, size_local);
    free(payload_local);
    return ret;
}

Return<int32_t> AGM::ipc_agm_session_aif_set_params(uint32_t session_id,
                                               uint32_t aif_id,
                                               const hidl_vec<uint8_t>& payload,
                                               uint32_t size) {
    ALOGV("%s : session_id = %d, aif_id =%d, size = %d\n", __func__,
                                                      session_id, aif_id, size);
    size_t size_local = (size_t) size;
    void * payload_local = NULL;
    int32_t ret = 0;
    payload_local = (void*) calloc (1,size);
    if (payload_local == NULL) {
        ALOGE("%s: Cannot allocate memory for payload_local\n", __func__);
        return -ENOMEM;
    }
    memcpy(payload_local, payload.data(), size);
    ret = agm_session_aif_set_params(session_id,
                                      aif_id,
                                      payload_local,
                                      size_local);
    free(payload_local);
    return ret;
}

Return<int32_t> AGM::ipc_agm_session_aif_set_cal(uint32_t session_id,
                                    uint32_t aif_id,
                                    const hidl_vec<AgmCalConfig>& cal_config) {
    ALOGV("%s : session_id = %d, aif_id = %d\n", __func__, session_id, aif_id);
    struct agm_cal_config *cal_config_local = NULL;
    int32_t ret = 0;

    cal_config_local =
              (struct agm_cal_config*) calloc(1, sizeof(struct agm_cal_config) +
               cal_config.data()->num_ckvs * sizeof(struct agm_key_value));

    if (cal_config_local == NULL) {
            ALOGE("%s: Cannot allocate memory for cal_config_local\n", __func__);
            return -ENOMEM;
    }
    cal_config_local->num_ckvs = cal_config.data()->num_ckvs;
    AgmKeyValue * ptr = NULL;
    for (int i=0 ; i < cal_config.data()->num_ckvs ; i++ ) {
        ptr = (AgmKeyValue *) (cal_config.data() +
                                             sizeof(struct agm_cal_config) +
                                             (sizeof(AgmKeyValue)*i));
        cal_config_local->kv[i].key = ptr->key;
        cal_config_local->kv[i].value = ptr->value;
    }
    ret = agm_session_aif_set_cal(session_id, aif_id, cal_config_local);
    free(cal_config_local);
    return ret;
}

Return<int32_t> AGM::ipc_agm_session_set_params(uint32_t session_id,
                                               const hidl_vec<uint8_t>& payload,
                                               uint32_t size) {
    ALOGV("%s : session_id = %d, size = %d\n", __func__, session_id, size);
    size_t size_local = (size_t) size;
    void * payload_local = NULL;
    int32_t ret = 0;
    payload_local = (void*) calloc (1,size);
    if (payload_local == NULL) {
        ALOGE("%s: Cannot allocate memory for payload_local\n", __func__);
        return -ENOMEM;
    }
    memcpy(payload_local, payload.data(), size);
    ret = agm_session_set_params(session_id, payload_local, size_local);
    free(payload_local);
    return ret;
}

Return<int32_t> AGM::ipc_agm_set_params_with_tag(uint32_t session_id,
                                     uint32_t aif_id,
                                     const hidl_vec<AgmTagConfig>& tag_config) {
    ALOGV("%s : session_id = %d, aif_id = %d\n", __func__, session_id, aif_id);
    struct agm_tag_config *tag_config_local;
    size_t size_local = (sizeof(struct agm_tag_config) +
                        (tag_config.data()->num_tkvs) * sizeof(agm_key_value));
    int32_t ret = 0;
    tag_config_local = (struct agm_tag_config *) calloc(1,size_local);
    if (tag_config_local == NULL) {
        ALOGE("%s: Cannot allocate memory for tag_config_local\n", __func__);
        return -ENOMEM;
    }
    tag_config_local->num_tkvs = tag_config.data()->num_tkvs;
    tag_config_local->tag = tag_config.data()->tag;
    AgmKeyValue * ptr = NULL;
    for (int i=0 ; i < tag_config.data()->num_tkvs ; i++ ) {
        ptr = (AgmKeyValue *) (tag_config.data() +
                                             sizeof(struct agm_tag_config) +
                                             (sizeof(AgmKeyValue)*i));
        tag_config_local->kv[i].key = ptr->key;
        tag_config_local->kv[i].value = ptr->value;
    }
    ret = agm_set_params_with_tag(session_id, aif_id, tag_config_local);
    free(tag_config_local);
    return ret;
}

Return<int32_t> AGM::ipc_agm_set_params_with_tag_to_acdb(uint32_t session_id,
                                     uint32_t aif_id,
                                     const hidl_vec<uint8_t>& payload,
                                     uint32_t size) {
    ALOGV("%s : session_id = %d, aif_id = %d\n", __func__, session_id, aif_id);
    size_t size_local = (size_t) size;
    void * payload_local = NULL;
    int32_t ret = 0;
    payload_local = (void*) calloc(1,size);
    if (payload_local == NULL) {
        ALOGE("%s: Cannot allocate memory for payload_local\n", __func__);
        return -ENOMEM;
    }
    memcpy(payload_local, payload.data(), size);

    ret = agm_set_params_with_tag_to_acdb(session_id, aif_id, payload_local, size_local);
    free(payload_local);
    return ret;
}

Return<int32_t> AGM::ipc_agm_session_register_for_events(uint32_t session_id,
                                  const hidl_vec<AgmEventRegCfg>& evt_reg_cfg) {
    ALOGV("%s : session_id = %d\n", __func__, session_id);
    struct agm_event_reg_cfg *evt_reg_cfg_local;
    int32_t ret = 0;
    evt_reg_cfg_local = (struct agm_event_reg_cfg*)
              calloc(1,(sizeof(struct agm_event_reg_cfg) +
              (evt_reg_cfg.data()->event_config_payload_size)*sizeof(uint8_t)));
    if (evt_reg_cfg_local == NULL) {
        ALOGE("%s: Cannot allocate memory for evt_reg_cfg_local\n", __func__);
        return -ENOMEM;
    }
    evt_reg_cfg_local->module_instance_id = evt_reg_cfg.data()->module_instance_id;
    evt_reg_cfg_local->event_id = evt_reg_cfg.data()->event_id;
    evt_reg_cfg_local->event_config_payload_size = evt_reg_cfg.data()->event_config_payload_size;
    evt_reg_cfg_local->is_register = evt_reg_cfg.data()->is_register;
    for (int i = 0; i < evt_reg_cfg.data()->event_config_payload_size; i++)
        evt_reg_cfg_local->event_config_payload[i] = evt_reg_cfg.data()->event_config_payload[i];

    ret = agm_session_register_for_events(session_id, evt_reg_cfg_local);
    free(evt_reg_cfg_local);
    return ret;
}

Return<void> AGM::ipc_agm_session_open(uint32_t session_id,
                                       AgmSessionMode sess_mode,
                                       ipc_agm_session_open_cb _hidl_cb) {
    uint64_t handle;
    enum agm_session_mode session_mode = (enum agm_session_mode) sess_mode;
    ALOGV("%s: session_id=%d session_mode=%d\n", __func__, session_id,
              session_mode);
    int32_t ret = agm_session_open(session_id, session_mode, &handle);
    hidl_vec<uint64_t> handle_ret;
    handle_ret.resize(sizeof(uint64_t));
    *handle_ret.data() = handle;
    _hidl_cb(ret, handle_ret);
    if (!ret)
        add_handle_to_list(session_id, handle);
    ALOGV("%s : handle received is : %llx",__func__, (unsigned long long) handle);
    return Void();
}

Return<int32_t> AGM::ipc_agm_session_set_config(uint64_t hndl,
                const hidl_vec<AgmSessionConfig>& session_config,
                const hidl_vec<AgmMediaConfig>& media_config,
                const hidl_vec<AgmBufferConfig>& buffer_config) {
    ALOGV("%s called with handle = %llx \n", __func__, (unsigned long long) hndl);

    struct agm_media_config *media_config_local = NULL;
    int32_t ret = 0;
    media_config_local = (struct agm_media_config*)
                                    calloc(1, sizeof(struct agm_media_config));
    if (media_config_local == NULL) {
        ALOGE("%s: Cannot allocate memory for media_config_local\n", __func__);
        return -ENOMEM;
    }
    media_config_local->rate = media_config.data()->rate;
    media_config_local->channels = media_config.data()->channels;
    media_config_local->format = (enum agm_media_format) media_config.data()->format;
    media_config_local->data_format = (enum agm_media_format) media_config.data()->data_format;
    struct agm_session_config *session_config_local = NULL;
    session_config_local = (struct agm_session_config*)
                                  calloc(1, sizeof(struct agm_session_config));
    if (session_config_local == NULL) {
        ALOGE("%s: Cannot allocate memory for session_config_local\n", __func__);
        return -ENOMEM;
    }
    memcpy(session_config_local,
           session_config.data(),
           sizeof(struct agm_session_config));

    struct agm_buffer_config *buffer_config_local = NULL;
    buffer_config_local = (struct agm_buffer_config*)
                                   calloc(1, sizeof(struct agm_buffer_config));
    if (buffer_config_local == NULL) {
        ALOGE("%s: Cannot allocate memory for buffer_config_local\n", __func__);
        return -ENOMEM;
    }
    buffer_config_local->count = buffer_config.data()->count;
    buffer_config_local->size = buffer_config.data()->size;
    ret = agm_session_set_config(hndl,
                                  session_config_local,
                                  media_config_local,
                                  buffer_config_local);
    free(session_config_local);
    free(media_config_local);
    free(buffer_config_local);
    return ret;
}

Return<int32_t> AGM::ipc_agm_session_close(uint64_t hndl) {
    ALOGV("%s called with handle = %llx \n", __func__, (unsigned long long) hndl);
    struct listnode *node = NULL;
    struct listnode *tempnode = NULL;
    agm_client_session_handle *hndle = NULL;
    client_info *handle = NULL;
    struct listnode *sess_node = NULL;
    struct listnode *sess_tempnode = NULL;
    int pid = ::android::hardware::IPCThreadState::self()->getCallingPid();

    pthread_mutex_lock(&client_list_lock);
    list_for_each_safe(node, tempnode, &client_list) {
        handle = node_to_item(node, client_info, list);
        if (handle->pid != pid)
            continue;

        list_for_each_safe(sess_node, sess_tempnode,
                                  &handle->agm_client_hndl_list) {
            hndle = node_to_item(sess_node,
                                 agm_client_session_handle,
                                 list);
           if (hndle->handle == hndl) {
               list_remove(sess_node);
               free(hndle);
           }
       }
    }
    pthread_mutex_unlock(&client_list_lock);
    return agm_session_close(hndl);
}

Return<int32_t> AGM::ipc_agm_session_prepare(uint64_t hndl) {
    ALOGV("%s called with handle = %llx \n", __func__, (unsigned long long) hndl);

    return agm_session_prepare(hndl);
}

Return<int32_t> AGM::ipc_agm_session_start(uint64_t hndl) {
    ALOGV("%s called with handle = %llx \n", __func__, (unsigned long long) hndl);

    return agm_session_start(hndl);
}

Return<int32_t> AGM::ipc_agm_session_stop(uint64_t hndl) {
    ALOGV("%s called with handle = %llx \n", __func__, (unsigned long long) hndl);

    return agm_session_stop(hndl);
}

Return<int32_t> AGM::ipc_agm_session_pause(uint64_t hndl) {
    ALOGV("%s called with handle = %llx \n", __func__, (unsigned long long) hndl);

    return agm_session_pause(hndl);
}

Return<int32_t> AGM::ipc_agm_session_flush(uint64_t hndl) {
    ALOGV("%s called with handle = %llx \n", __func__, (unsigned long long) hndl);

    return agm_session_flush(hndl);
}

Return<int32_t> AGM::ipc_agm_session_resume(uint64_t hndl) {
    ALOGV("%s called with handle = %llx \n", __func__, (unsigned long long) hndl);

    return agm_session_resume(hndl);
}

Return<int32_t> AGM::ipc_agm_session_suspend(uint64_t hndl) {
    ALOGV("%s called with handle = %llx \n", __func__, (unsigned long long) hndl);

    return agm_session_suspend(hndl);
}

Return<void> AGM::ipc_agm_session_read(uint64_t hndl, uint32_t count,
                                             ipc_agm_session_read_cb _hidl_cb) {
    ALOGV("%s called with handle = %llx \n", __func__, (unsigned long long) hndl);
    hidl_vec <uint8_t> buff_ret;
    void *buffer = NULL;
    buffer = (void*) calloc(1,count);
    if (buffer == NULL) {
        ALOGE("%s: Cannot allocate memory for buffer\n", __func__);
        _hidl_cb (-ENOMEM, buff_ret, count);
        return Void();
    }
    size_t cnt = (size_t) count;
    int ret = agm_session_read(hndl, buffer, &cnt);
    buff_ret.resize(count);
    memcpy(buff_ret.data(), buffer, count);
    _hidl_cb (ret, buff_ret, cnt);
    free(buffer);
    return Void();
}

Return<void> AGM::ipc_agm_session_write(uint64_t hndl,
                                        const hidl_vec<uint8_t>& buff,
                                        uint32_t count,
                                        ipc_agm_session_write_cb _hidl_cb) {
    ALOGV("%s called with handle = %llx \n", __func__, (unsigned long long) hndl);
    void* buffer = NULL;
    buffer = (void*) calloc(1,count);
    if (buffer == NULL) {
        ALOGE("%s: Cannot allocate memory for buffer\n", __func__);
        _hidl_cb (-ENOMEM, count);
        return Void();
    }
    memcpy(buffer, buff.data(), count);
    size_t cnt = (size_t) count;
    int ret = agm_session_write(hndl, buffer, &cnt);
    _hidl_cb (ret, cnt);
    free(buffer);
    return Void();
}

Return<int32_t> AGM::ipc_agm_get_hw_processed_buff_cnt(uint64_t hndl,
                                                        Direction dir) {
    ALOGV("%s called with handle = %llx \n", __func__, (unsigned long long) hndl);
    enum direction dir_local = (enum direction) dir;
    return agm_get_hw_processed_buff_cnt(hndl, dir_local);
}

Return<void> AGM::ipc_agm_get_aif_info_list(uint32_t num_aif_info,
                                       ipc_agm_get_aif_info_list_cb _hidl_cb) {
    ALOGV("%s called with num_aif_info = %d\n", __func__, num_aif_info);
    int32_t ret;
    hidl_vec<AifInfo> aif_list_ret;
    struct aif_info * aif_list = NULL;
    if (num_aif_info != 0) {
        aif_list = (struct aif_info*)
                            calloc(1,(sizeof(struct aif_info) * num_aif_info));
        if (aif_list == NULL) {
            ALOGE("%s: Cannot allocate memory for aif_list\n", __func__);
            _hidl_cb(-ENOMEM, aif_list_ret, num_aif_info);
            return Void();
        }
    }
    size_t num_aif_info_ret = (size_t) num_aif_info;
    ret = agm_get_aif_info_list(aif_list, &num_aif_info_ret);
    aif_list_ret.resize(sizeof(struct aif_info) * num_aif_info);
    if ( aif_list != NULL) {
        for (int i=0 ; i<num_aif_info ; i++) {
            aif_list_ret.data()[i].aif_name = aif_list[i].aif_name;
            aif_list_ret.data()[i].dir = (Direction) aif_list[i].dir;
        }
    }
    num_aif_info = (uint32_t) num_aif_info_ret;
    ret = 0;
    _hidl_cb(ret, aif_list_ret, num_aif_info);
    free(aif_list);
    return Void();
}
Return<int32_t> AGM::ipc_agm_session_set_loopback(uint32_t capture_session_id,
                                                  uint32_t playback_session_id,
                                                  bool state) {
    ALOGV("%s called capture_session_id = %d, playback_session_id = %d\n", __func__,
           capture_session_id, playback_session_id);
    return agm_session_set_loopback(capture_session_id,
                                    playback_session_id,
                                    state);
}


Return<int32_t> AGM::ipc_agm_session_set_ec_ref(uint32_t capture_session_id,
                                                uint32_t aif_id, bool state) {
    ALOGV("%s : cap_sess_id = %d, aif_id = %d\n", __func__,
                                  capture_session_id, aif_id);
    return agm_session_set_ec_ref(capture_session_id, aif_id, state);
}

Return<int32_t> AGM::ipc_agm_client_register_callback(const sp<IAGMCallback>& cb)
{
    int pid = ::android::hardware::IPCThreadState::self()->getCallingPid();
    client_info *client_handle = NULL;
    struct listnode* node = NULL;

    pthread_mutex_lock(&client_list_lock);
    list_for_each(node, &client_list) {
        client_handle = node_to_item(node, client_info, list);
        if (client_handle->pid == pid)
                goto register_cb;
    }

    client_handle = (client_info *)calloc(1, sizeof(client_info));
    if (client_handle == NULL) {
        ALOGE("%s: Cannot allocate memory for client handle\n", __func__);
        pthread_mutex_unlock(&client_list_lock);
        return -ENOMEM;
    }
    client_handle->pid = pid;
    list_init(&client_handle->agm_client_hndl_list);
    list_add_tail(&client_list, &client_handle->list);
register_cb:
    if (mDeathNotifier == NULL)
        mDeathNotifier = new client_death_notifier();

    cb->linkToDeath(mDeathNotifier, pid);
    client_handle->clbk_binder = cb;
    pthread_mutex_unlock(&client_list_lock);
    return 0;
}

Return<int32_t> AGM::ipc_agm_session_register_callback(uint32_t session_id,
                                                     uint32_t evt_type,
                                                     uint64_t ipc_client_data,
                                                     uint64_t clnt_data) {
    agm_event_cb ipc_cb;
    SrvrClbk  *sr_clbk_data = NULL, *tmp_sr_clbk_data = NULL;
    clbk_data *clbk_data_obj = NULL;
    struct listnode* node = NULL;
    int pid = ::android::hardware::IPCThreadState::self()->getCallingPid();
    int32_t ret = 0;

    if (ipc_client_data) {
        sr_clbk_data = new SrvrClbk (session_id, evt_type, ipc_client_data, pid);
        ALOGV("%s new SrvrClbk= %p, clntdata= %llx, sess id= %d, evt_type= %d \n",
                __func__, (void *) sr_clbk_data, (unsigned long long) ipc_client_data, session_id,
                (uint32_t)evt_type);

        clbk_data_obj = (clbk_data *)calloc(1, sizeof(clbk_data));
        if (clbk_data_obj == NULL) {
            delete sr_clbk_data;
            ALOGE("%s: Cannot allocate memory for cb data object\n", __func__);
            return -ENOMEM;
        }
        pthread_mutex_lock(&clbk_data_list_lock);
        clbk_data_obj->clbk_clt_data = clnt_data;
        clbk_data_obj->srv_clt_data = sr_clbk_data;
        list_add_tail(&clbk_data_list, &clbk_data_obj->list);
        pthread_mutex_unlock(&clbk_data_list_lock);
        ipc_cb = &ipc_callback;
    } else {
        /*
         *This condition indicates that the client wants to deregister the
         *callback. Hence we pass the callback as NULL to AGM and also the
         *client data should match with the one that was used to register
         *with AGM.
         */
        pthread_mutex_lock(&clbk_data_list_lock);
        list_for_each(node, &clbk_data_list) {
            clbk_data_obj = node_to_item(node, clbk_data, list);
            tmp_sr_clbk_data = clbk_data_obj->srv_clt_data;
            if ((tmp_sr_clbk_data->session_id == session_id) &&
                (tmp_sr_clbk_data->event == evt_type) &&
                (clbk_data_obj->clbk_clt_data == clnt_data)) {
                sr_clbk_data = tmp_sr_clbk_data;
                break;
            }
        }
        pthread_mutex_unlock(&clbk_data_list_lock);
        ipc_cb = NULL;
    }
    if (sr_clbk_data == NULL) {
        ALOGE("server callback data is NULL");
        return -ENOMEM;
    }
    ALOGV("%s : Exit ", __func__);
    ret =  agm_session_register_cb(session_id,
                                   ipc_cb,
                                   (enum event_type) evt_type,
                                   (void *) sr_clbk_data);
    /*
     * Free client data after deregister
     */
    if (!ipc_client_data) {
        list_remove(node);
        free(clbk_data_obj);
        delete sr_clbk_data;
        clbk_data_obj = NULL;
        sr_clbk_data = nullptr;
    }
    return ret;
}

Return<int32_t> AGM::ipc_agm_session_eos(uint64_t hndl){
    ALOGV("%s : handle = %llx \n", __func__, (unsigned long long) hndl);
    return agm_session_eos(hndl);
}

Return<void> AGM::ipc_agm_get_session_time(uint64_t hndl,
                                          ipc_agm_get_session_time_cb _hidl_cb){
    ALOGV("%s : handle = %llx \n", __func__, (unsigned long long) hndl);
    uint64_t ts;
    int ret = agm_get_session_time(hndl, &ts);
    _hidl_cb(ret,ts);
    return Void();
}

Return<void> AGM::ipc_agm_get_buffer_timestamp(uint32_t session_id,
                                          ipc_agm_get_buffer_timestamp_cb _hidl_cb){
    ALOGV("%s : session_id = %u\n", __func__, session_id);
    uint64_t ts = 0;
    int ret = agm_get_buffer_timestamp(session_id, &ts);

    _hidl_cb(ret, ts);
    return Void();
}

Return<void> AGM::ipc_agm_session_get_buf_info(uint32_t session_id, uint32_t flag,
                                             ipc_agm_session_get_buf_info_cb _hidl_cb) {
    struct agm_buf_info buf_info;
    int32_t ret = -EINVAL;
    MmapBufInfo info;
    native_handle_t *dataHidlHandle = nullptr;
    native_handle_t *posHidlHandle = nullptr;

    ALOGV("%s : session_id = %d\n", __func__, session_id);

    ret = agm_session_get_buf_info(session_id, &buf_info, flag);
    if (!ret) {
        if (flag & DATA_BUF) {
            dataHidlHandle = native_handle_create(1, 0);
            if (!dataHidlHandle) {
                ALOGE("%s native_handle_create fails", __func__);
                goto exit;
            }
            dataHidlHandle->data[0] = buf_info.data_buf_fd;
            info.dataSharedMemory = hidl_memory("ar_data_buf", hidl_handle(dataHidlHandle),
                    buf_info.data_buf_size);
            info.data_size = buf_info.data_buf_size;
        }
        if (flag & POS_BUF) {
            posHidlHandle = native_handle_create(1, 0);
            if (!posHidlHandle) {
                ALOGE("%s native_handle_create fails", __func__);
                goto exit;
            }
            posHidlHandle->data[0] = buf_info.pos_buf_fd;
            info.posSharedMemory = hidl_memory("ar_pos_buf", posHidlHandle,
                    buf_info.pos_buf_size);
            info.pos_size = buf_info.pos_buf_size;
        }
    }

    _hidl_cb(ret, info);

exit:
    if (dataHidlHandle != nullptr)
        native_handle_delete(dataHidlHandle);

    if (posHidlHandle != nullptr)
        native_handle_delete(posHidlHandle);

    return Void();
}

Return<int32_t> AGM::ipc_agm_set_gapless_session_metadata(uint64_t hndl,
                     AgmGaplessSilenceType type, uint32_t silence) {
    enum agm_gapless_silence_type type_local = (enum agm_gapless_silence_type)type;
    ALOGV("%s : handle = %lu \n", __func__, hndl);
    return agm_set_gapless_session_metadata(hndl, type_local, silence);
}

Return<int32_t> AGM::ipc_agm_session_set_non_tunnel_mode_config(uint64_t hndl,
                const hidl_vec<AgmSessionConfig>& session_config,
                const hidl_vec<AgmMediaConfig>& in_media_config,
                const hidl_vec<AgmMediaConfig>& out_media_config,
                const hidl_vec<AgmBufferConfig>& in_buffer_config,
                const hidl_vec<AgmBufferConfig>& out_buffer_config) {
    ALOGV("%s called with handle = %llx \n", __func__, (unsigned long long) hndl);

    struct agm_media_config *in_media_config_local, *out_media_config_local = NULL;
    int32_t ret = 0;
    in_media_config_local = (struct agm_media_config*)
                                    calloc(1, sizeof(struct agm_media_config));
    if (in_media_config_local == NULL) {
        ALOGE("%s: Cannot allocate memory for media_config_local\n", __func__);
        return -ENOMEM;
    }

    out_media_config_local = (struct agm_media_config*)
                                    calloc(1, sizeof(struct agm_media_config));
    if (out_media_config_local == NULL) {
        ALOGE("%s: Cannot allocate memory for media_config_local\n", __func__);
        return -ENOMEM;
    }

    in_media_config_local->rate = in_media_config.data()->rate;
    in_media_config_local->channels = in_media_config.data()->channels;
    in_media_config_local->format = (enum agm_media_format) in_media_config.data()->format;
    in_media_config_local->data_format = (enum agm_media_format) in_media_config.data()->data_format;

    out_media_config_local->rate = out_media_config.data()->rate;
    out_media_config_local->channels = out_media_config.data()->channels;
    out_media_config_local->format = (enum agm_media_format) out_media_config.data()->format;
    out_media_config_local->data_format = (enum agm_media_format) out_media_config.data()->data_format;


    struct agm_session_config *session_config_local = NULL;
    session_config_local = (struct agm_session_config*)
                                  calloc(1, sizeof(struct agm_session_config));
    if (session_config_local == NULL) {
        ALOGE("%s: Cannot allocate memory for session_config_local\n", __func__);
        return -ENOMEM;
    }
    memcpy(session_config_local,
           session_config.data(),
           sizeof(struct agm_session_config));

    struct agm_buffer_config *in_buffer_config_local, *out_buffer_config_local = NULL;
    in_buffer_config_local = (struct agm_buffer_config*)
                                   calloc(1, sizeof(struct agm_buffer_config));
    if (in_buffer_config_local == NULL) {
        ALOGE("%s: Cannot allocate memory for buffer_config_local\n", __func__);
        return -ENOMEM;
    }

    out_buffer_config_local = (struct agm_buffer_config*)
                                   calloc(1, sizeof(struct agm_buffer_config));
    if (out_buffer_config_local == NULL) {
        ALOGE("%s: Cannot allocate memory for buffer_config_local\n", __func__);
        return -ENOMEM;
    }

    in_buffer_config_local->count = in_buffer_config.data()->count;
    in_buffer_config_local->size = in_buffer_config.data()->size;
    in_buffer_config_local->max_metadata_size = in_buffer_config.data()->max_metadata_size;

    out_buffer_config_local->count = out_buffer_config.data()->count;
    out_buffer_config_local->size = out_buffer_config.data()->size;
    out_buffer_config_local->max_metadata_size = out_buffer_config.data()->max_metadata_size;

    ret = agm_session_set_non_tunnel_mode_config(hndl,
                                  session_config_local,
                                  in_media_config_local,
                                  out_media_config_local,
                                  in_buffer_config_local,
                                  out_buffer_config_local);
    free(session_config_local);
    free(in_media_config_local);
    free(out_media_config_local);
    free(in_buffer_config_local);
    free(out_buffer_config_local);
    return ret;
}

Return<void> AGM::ipc_agm_session_write_with_metadata(uint64_t hndl, const hidl_vec<AgmBuff>& buff_hidl,
                                               uint64_t consumed_sz,
                                               ipc_agm_session_write_with_metadata_cb _hidl_cb)
{
    int32_t ret = -EINVAL;
    struct agm_buff buf;
    uint32_t bufSize;
    size_t consumed_size = consumed_sz;
    const native_handle *allochandle = nullptr;
    buf.addr = nullptr;
    buf.metadata = nullptr;

    bufSize = buff_hidl.data()->size;
    buf.addr = (uint8_t *)calloc(1, bufSize);
    if (!buf.addr) {
        ALOGE("%s: failed to calloc", __func__);
        goto exit;
    }
    buf.size = (size_t)bufSize;
    buf.timestamp = buff_hidl.data()->timestamp;
    buf.flags = buff_hidl.data()->flags;
    if (buff_hidl.data()->metadata_size) {
        buf.metadata_size = buff_hidl.data()->metadata_size;
        buf.metadata = (uint8_t *)calloc(1, buf.metadata_size);
        if (!buf.metadata) {
            ALOGE("%s: failed to calloc", __func__);
            goto exit;
        }
        memcpy(buf.metadata, buff_hidl.data()->metadata.data(),
               buf.metadata_size);
    }

    allochandle = buff_hidl.data()->alloc_info.alloc_handle.handle();
    buf.alloc_info.alloc_handle = dup(allochandle->data[0]);
    add_fd_to_list(hndl, allochandle->data[1], buf.alloc_info.alloc_handle);
    buf.alloc_info.alloc_size = buff_hidl.data()->alloc_info.alloc_size;
    buf.alloc_info.offset = buff_hidl.data()->alloc_info.offset;
    if (bufSize)
        memcpy(buf.addr, buff_hidl.data()->buffer.data(), bufSize);
    ALOGV("%s:%d sz %d", __func__,__LINE__,bufSize);
    ret = agm_session_write_with_metadata(hndl, &buf, &consumed_size);
    _hidl_cb(ret, consumed_size);

exit:
    if (buf.metadata != nullptr)
        free(buf.metadata);
    if (buf.addr != nullptr)
        free(buf.addr);
    return Void();
}

Return<void> AGM::ipc_agm_session_read_with_metadata(uint64_t hndl, const hidl_vec<AgmBuff>& inBuff_hidl,
                                               uint32_t captured_sz,
                                               ipc_agm_session_read_with_metadata_cb _hidl_cb)
{
    struct agm_buff buf;
    int32_t ret = 0;
    hidl_vec<AgmBuff> outBuff_hidl;
    uint32_t bufSize;
    uint32_t captured_size = captured_sz;
    const native_handle *allochandle = nullptr;

    bufSize = inBuff_hidl.data()->size;
    buf.addr = (uint8_t *)calloc(1, bufSize);
    buf.size = (size_t)bufSize;
    buf.metadata_size = inBuff_hidl.data()->metadata_size;
    buf.metadata = (uint8_t *)calloc(1, buf.metadata_size);
    if (!buf.addr || !buf.metadata) {
        ALOGE("%s: failed to calloc", __func__);
        goto exit;
    }
    buf.timestamp = 0;
    buf.flags = 0;

    allochandle = inBuff_hidl.data()->alloc_info.alloc_handle.handle();

    buf.alloc_info.alloc_handle = dup(allochandle->data[0]);
    add_fd_to_list(hndl, allochandle->data[1], buf.alloc_info.alloc_handle);

    buf.alloc_info.alloc_size = inBuff_hidl.data()->alloc_info.alloc_size;
    buf.alloc_info.offset = inBuff_hidl.data()->alloc_info.offset;
    ALOGV("%s:%d sz %d", __func__,__LINE__,bufSize);
    ret = agm_session_read_with_metadata(hndl, &buf, &captured_size);
    if (ret > 0) {
        outBuff_hidl.resize(sizeof(struct agm_buff));
        outBuff_hidl.data()->size = (uint32_t)buf.size;
        outBuff_hidl.data()->buffer.resize(buf.size);
        memcpy(outBuff_hidl.data()->buffer.data(), buf.addr,
               buf.size);
        outBuff_hidl.data()->timestamp = buf.timestamp;
        if (buf.metadata_size) {
           outBuff_hidl.data()->metadata.resize(buf.metadata_size);
           outBuff_hidl.data()->metadata_size = buf.metadata_size;
           memcpy(outBuff_hidl.data()->metadata.data(),
                  buf.metadata, buf.metadata_size);
        }
    }
    _hidl_cb(ret, outBuff_hidl, captured_size);

exit:
    if (buf.addr)
        free(buf.addr);

    return Void();
}

Return<int32_t> AGM::ipc_agm_aif_group_set_media_config(uint32_t group_id,
                                 const hidl_vec<AgmGroupMediaConfig>& media_config) {
    ALOGV("%s called with aif_id = %d \n", __func__, group_id);
    struct agm_group_media_config *med_config_l = NULL;
    med_config_l =
          (struct agm_group_media_config*)calloc(1, sizeof(struct agm_group_media_config));
    if (med_config_l == NULL) {
        ALOGE("%s: Cannot allocate memory for med_config_l\n", __func__);
        return -ENOMEM;
    }
    med_config_l->config.rate = media_config.data()->rate;
    med_config_l->config.channels = media_config.data()->channels;
    med_config_l->config.format = (enum agm_media_format) media_config.data()->format;
    med_config_l->config.data_format = media_config.data()->data_format;
    med_config_l->slot_mask = media_config.data()->slot_mask;
    int32_t ret = agm_aif_group_set_media_config (group_id, med_config_l);
    free(med_config_l);
    return ret;
}

Return<void> AGM::ipc_agm_get_group_aif_info_list(uint32_t num_groups,
                                       ipc_agm_get_aif_info_list_cb _hidl_cb) {
    ALOGV("%s called with num_aif_groups = %d\n", __func__, num_groups);
    int32_t ret;
    hidl_vec<AifInfo> aif_list_ret;
    struct aif_info * aif_list = NULL;
    if (num_groups != 0) {
        aif_list = (struct aif_info*)
                            calloc(1,(sizeof(struct aif_info) * num_groups));
        if (aif_list == NULL) {
            ALOGE("%s: Cannot allocate memory for aif_list\n", __func__);
            _hidl_cb(-ENOMEM, aif_list_ret, num_groups);
            return Void();
        }
    }
    size_t num_aif_groups_ret = (size_t) num_groups;
    ret = agm_get_group_aif_info_list(aif_list, &num_aif_groups_ret);
    aif_list_ret.resize(sizeof(struct aif_info) * num_groups);
    if (aif_list != NULL) {
        for (int i = 0; i < num_groups ; i++) {
            aif_list_ret.data()[i].aif_name = aif_list[i].aif_name;
            aif_list_ret.data()[i].dir = (Direction) aif_list[i].dir;
        }
    }
    num_groups = (uint32_t) num_aif_groups_ret;
    ret = 0;
    _hidl_cb(ret, aif_list_ret, num_groups);
    free(aif_list);
    return Void();
}

Return<int32_t> AGM::ipc_agm_session_write_datapath_params(uint32_t session_id,
                                                const hidl_vec<AgmBuff>& buff_hidl)
{
    int32_t ret = -EINVAL;
    struct agm_buff buf;
    uint32_t bufSize;
    buf.addr = nullptr;
    buf.metadata = nullptr;

    bufSize = buff_hidl.data()->size;
    buf.addr = (uint8_t *)calloc(1, bufSize);
    if (!buf.addr) {
        ALOGE("%s: failed to calloc", __func__);
        goto exit;
    }
    buf.size = (size_t)bufSize;
    buf.timestamp = buff_hidl.data()->timestamp;
    buf.flags = buff_hidl.data()->flags;

    if (bufSize)
        memcpy(buf.addr, buff_hidl.data()->buffer.data(), bufSize);
    else {
        ALOGE("%s: buf size is null", __func__);
        goto exit;
    }
    ALOGV("%s: sz %d", __func__, bufSize);
    ret = agm_session_write_datapath_params(session_id, &buf);

exit:
    if (buf.addr != nullptr)
        free(buf.addr);
    return ret;
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace AGMIPC
}  // namespace hardware
}  // namespace qti
}  // namespace vendor
