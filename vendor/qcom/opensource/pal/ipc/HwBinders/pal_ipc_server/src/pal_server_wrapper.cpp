/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
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

#define LOG_TAG "pal_server_wrapper"
#include "inc/pal_server_wrapper.h"
#include <hwbinder/IPCThreadState.h>

#define MAX_CACHE_SIZE 64

using vendor::qti::hardware::pal::V1_0::IPAL;
using android::hardware::hidl_handle;
using android::hardware::hidl_memory;


namespace vendor {
namespace qti {
namespace hardware {
namespace pal {
namespace V1_0 {
namespace implementation {

PAL* PAL::sInstance;

void PalClientDeathRecipient::serviceDied(uint64_t cookie,
                   const android::wp<::android::hidl::base::V1_0::IBase>& who)
{
    std::lock_guard<std::mutex> guard(mLock);
    ALOGD("%s : client died pid : %d", __func__, cookie);
    int pid = (int) cookie;
    auto &clients = mPalInstance->mPalClients;
    for (auto itr = clients.begin(); itr != clients.end(); itr++) {
        auto client = *itr;
        if (client->pid == pid) {
            {
                std::lock_guard<std::mutex> lock(client->mActiveSessionsLock);
                for (auto sItr = client->mActiveSessions.begin();
                          sItr != client->mActiveSessions.end(); sItr++) {
                   ALOGD("Closing the session %pK", sItr->session_handle);
                   ALOGV("hdle %x binder %p", sItr->session_handle, sItr->callback_binder.get());
                   sItr->callback_binder->client_died = true;
                   pal_stream_stop((pal_stream_handle_t *)sItr->session_handle);
                   pal_stream_close((pal_stream_handle_t *)sItr->session_handle);
                   /*close the dupped fds in PAL server context*/
                   for (int i = 0; i < sItr->callback_binder->sharedMemFdList.size(); i++) {
                       close(sItr->callback_binder->sharedMemFdList[i].second);
                   }
                   sItr->callback_binder.clear();
                }
                client->mActiveSessions.clear();
            }
            itr = clients.erase(itr);
            break;
        }
    }
}

int PAL::find_dup_fd_from_input_fd(const uint64_t streamHandle, int input_fd, int *dup_fd)
{
    for (auto& s: mPalClients) {
        std::lock_guard<std::mutex> lock(s->mActiveSessionsLock);
        for (int i = 0; i < s->mActiveSessions.size(); i++) {
            session_info session = s->mActiveSessions[i];
            if (session.session_handle == streamHandle) {
                for (int j = 0; j < session.callback_binder->sharedMemFdList.size(); j++) {
                    ALOGV("fd list, input %d dup %d",
                           session.callback_binder->sharedMemFdList[j].first,
                           session.callback_binder->sharedMemFdList[j].second);
                    if (session.callback_binder->sharedMemFdList[j].first == input_fd) {
                        *dup_fd = session.callback_binder->sharedMemFdList[j].second;
                        ALOGV("matching input fd found, return dup fd %d", *dup_fd);
                        return 0;
                    }
                }
            }
        }
    }
    return -1;
}

void PAL::add_input_and_dup_fd(const uint64_t streamHandle, int input_fd, int dup_fd)
{
    std::vector<std::pair<int, int>>::iterator it;
    for (auto& s: mPalClients) {
        std::lock_guard<std::mutex> lock(s->mActiveSessionsLock);
        for (int i = 0; i < s->mActiveSessions.size(); i++) {
            session_info session = s->mActiveSessions[i];
            if (session.session_handle == streamHandle) {
                /*If number of FDs increase than the MAX Cache size we delete the oldest one
                  NOTE: We still create a new fd for every input fd*/
                if (session.callback_binder->sharedMemFdList.size() > MAX_CACHE_SIZE) {
                    close(session.callback_binder->sharedMemFdList.front().second);
                    it = session.callback_binder->sharedMemFdList.begin();
                    session.callback_binder->sharedMemFdList.erase(it);
                }
                session.callback_binder->sharedMemFdList.push_back(
                                            std::make_pair(input_fd, dup_fd));
            }
        }
    }
}

static int32_t pal_callback(pal_stream_handle_t *stream_handle,
                            uint32_t event_id, uint32_t *event_data,
                            uint32_t event_data_size,
                            uint64_t cookie)
{
    auto isPalSessionActive = [&](const uint64_t stream_handle) {
        if (!PAL::getInstance()) {
           ALOGE("%s: No PAL instance running", __func__);
           return false;
        }
        for (auto& s: PAL::getInstance()->mPalClients) {
            std::lock_guard<std::mutex> lock(s->mActiveSessionsLock);
            for (int idx = 0; idx < s->mActiveSessions.size(); idx++) {
                session_info session = s->mActiveSessions[idx];
                if (session.session_handle == stream_handle)
                    return true;
            }
        }
        return false;
    };

    if (!isPalSessionActive((uint64_t)stream_handle)) {
        ALOGE("%s: PAL session %pK is no longer active", __func__, stream_handle);
        return -EINVAL;
    }

    sp<SrvrClbk> sr_clbk_dat = (SrvrClbk *) cookie;
    sp<IPALCallback> clbk_bdr = sr_clbk_dat->clbk_binder;

    if ((sr_clbk_dat->session_attr.type == PAL_STREAM_NON_TUNNEL) &&
          ((event_id == PAL_STREAM_CBK_EVENT_READ_DONE) ||
           (event_id == PAL_STREAM_CBK_EVENT_WRITE_READY))) {
        hidl_vec<PalEventReadWriteDonePayload> rwDonePayloadHidl;
        PalEventReadWriteDonePayload *rwDonePayload;
        struct pal_event_read_write_done_payload *rw_done_payload;
        int input_fd = -1;
        native_handle_t *allocHidlHandle = nullptr;

        allocHidlHandle = native_handle_create(1, 1);
        if (!allocHidlHandle) {
            ALOGE("handle allocHidlHandle is NULL");
            return -EINVAL;
        }

        rw_done_payload = (struct pal_event_read_write_done_payload *)event_data;
        /*
         * Find the original fd that was passed by client based on what
         * input and dup fd list and send that back.
         */
        std::vector<std::pair<int, int>>::iterator it;
        for (int i = 0; i < sr_clbk_dat->sharedMemFdList.size(); i++) {
             if (sr_clbk_dat->sharedMemFdList[i].second == rw_done_payload->buff.alloc_info.alloc_handle) {
                 input_fd = sr_clbk_dat->sharedMemFdList[i].first;
               /*  it = (sr_clbk_dat->sharedMemFdList.begin() + i);
                 if (it != sr_clbk_dat->sharedMemFdList.end())
                     sr_clbk_dat->sharedMemFdList.erase(it);*/
             }
        }

        rwDonePayloadHidl.resize(sizeof(struct pal_event_read_write_done_payload));
        rwDonePayload =(PalEventReadWriteDonePayload *)rwDonePayloadHidl.data();
        rwDonePayload->tag = rw_done_payload->tag;
        rwDonePayload->status = rw_done_payload->status;
        rwDonePayload->md_status = rw_done_payload->md_status;

        rwDonePayload->buff.offset = rw_done_payload->buff.offset;
        rwDonePayload->buff.flags = rw_done_payload->buff.flags;
        rwDonePayload->buff.size = rw_done_payload->buff.size;
        if (rw_done_payload->buff.ts != NULL) {
            rwDonePayload->buff.timeStamp.tvSec = rw_done_payload->buff.ts->tv_sec;
            rwDonePayload->buff.timeStamp.tvNSec = rw_done_payload->buff.ts->tv_nsec;
        }
        if ((rw_done_payload->buff.buffer != NULL) &&
             !(sr_clbk_dat->session_attr.flags & PAL_STREAM_FLAG_EXTERN_MEM)) {
            rwDonePayload->buff.buffer.resize(rwDonePayload->buff.size);
            memcpy(rwDonePayload->buff.buffer.data(), rw_done_payload->buff.buffer,
                   rwDonePayload->buff.size);
        }
        if ((rw_done_payload->buff.metadata_size > 0) &&
             rw_done_payload->buff.metadata) {
            ALOGV("metadatasize %d ", rw_done_payload->buff.metadata_size);
            rwDonePayload->buff.metadataSz = rw_done_payload->buff.metadata_size;
            rwDonePayload->buff.metadata.resize(rwDonePayload->buff.metadataSz);
            memcpy(rwDonePayload->buff.metadata.data(), rw_done_payload->buff.metadata,
                    rwDonePayload->buff.metadataSz);
        }

        allocHidlHandle->data[0] = rw_done_payload->buff.alloc_info.alloc_handle;
        allocHidlHandle->data[1] = input_fd;
        ALOGV("returning fd %d for dup fd %d\n", allocHidlHandle->data[1], rw_done_payload->buff.alloc_info.alloc_handle);
        rwDonePayload->buff.alloc_info.alloc_handle = hidl_memory("arpal_alloc_handle",
                                                       hidl_handle(allocHidlHandle),
                                                       rw_done_payload->buff.alloc_info.alloc_size);

        rwDonePayload->buff.alloc_info.alloc_size = rw_done_payload->buff.alloc_info.alloc_size;
        rwDonePayload->buff.alloc_info.offset = rw_done_payload->buff.alloc_info.offset;
        if (!sr_clbk_dat->client_died) {
            auto status = clbk_bdr->event_callback_rw_done((uint64_t)stream_handle, event_id,
                              sizeof(struct pal_event_read_write_done_payload),
                              rwDonePayloadHidl,
                              sr_clbk_dat->client_data_);
            if (!status.isOk()) {
                 ALOGE("%s: HIDL call failed during event_callback_rw_done\n", __func__);
            }
           // close(rw_done_payload->buff.alloc_info.alloc_handle);
        } else
            ALOGE("Client died dropping this event %d", event_id);
    } else {
        hidl_vec<uint8_t> PayloadHidl;
        PayloadHidl.resize(event_data_size);
        memcpy(PayloadHidl.data(), event_data, event_data_size);
        if (!sr_clbk_dat->client_died) {
            auto status = clbk_bdr->event_callback((uint64_t)stream_handle, event_id,
                              event_data_size, PayloadHidl,
                              sr_clbk_dat->client_data_);
            if (!status.isOk()) {
                 ALOGE("%s: HIDL call failed during event_callback \n", __func__);
            }
        } else
            ALOGE("Client died dropping this event %d", event_id);
    }
    return 0;
}

static void print_stream_info(pal_stream_info_t *info_)
{
   ALOGV("%s",__func__);
   pal_stream_info *info = &info_->opt_stream_info;
   ALOGV("ver [%lld] sz [%lld] dur[%lld] has_video [%d] is_streaming [%d] lpbk_type [%d]",
           info->version, info->size, info->duration_us, info->has_video, info->is_streaming,
           info->loopback_type);
}

static void print_media_config(struct pal_media_config *cfg)
{
   ALOGV("%s",__func__);
   ALOGV("sr[%d] bw[%d] aud_fmt_id[%d]", cfg->sample_rate, cfg->bit_width, cfg->aud_fmt_id);
   ALOGV("channel [%d]", cfg->ch_info.channels);
   for (int i = 0; i < 8; i++)
        ALOGV("ch[%d] map%d", i, cfg->ch_info.ch_map[i]);
}

static void print_devices(uint32_t noDevices, struct pal_device *devices)
{
   ALOGV("%s",__func__);
   for (int i = 0 ; i < noDevices; i++) {
     ALOGV("id [%d]", devices[i].id);
     print_media_config(&(devices[i].config));
   }
}

static void print_kv_modifier(uint32_t noOfMods, struct modifier_kv *kv)
{
   ALOGV("%s",__func__);
   for (int i = 0 ; i < noOfMods; i++) {
     ALOGV("key [%d] value [%d]", kv[i].key, kv[i].value);
   }
}


static void print_attr(struct pal_stream_attributes *attr)
{
   ALOGV("%s",__func__);
   ALOGV("type [%d] flags [%0x] dir[%d]\n", attr->type, attr->flags, attr->direction);
   print_stream_info(&attr->info);
   print_media_config(&attr->in_media_config);
   print_media_config(&attr->out_media_config);
}

Return<void> PAL::ipc_pal_stream_open(const hidl_vec<PalStreamAttributes>& attr_hidl,
                            uint32_t noOfDevices,
                            const hidl_vec<PalDevice>& devs_hidl,
                            uint32_t noOfModifiers,
                            const hidl_vec<ModifierKV>& modskv_hidl,
                            const sp<IPALCallback>& cb, uint64_t ipc_clt_data,
                            ipc_pal_stream_open_cb _hidl_cb)
{
    struct pal_stream_attributes *attr = NULL;
    struct pal_device *devices = NULL;
    struct modifier_kv *modifiers = NULL;
    pal_stream_handle_t *stream_handle = NULL;
    uint16_t in_ch = 0;
    uint16_t out_ch = 0;
    int cnt = 0;
    int32_t ret = -EINVAL;
    uint8_t *temp = NULL;
    int pid = ::android::hardware::IPCThreadState::self()->getCallingPid();
    sp<SrvrClbk> sr_clbk_data = (cb == nullptr) ? nullptr : new SrvrClbk (cb, ipc_clt_data, pid);
    pal_stream_callback callback = (cb == nullptr) ? nullptr : pal_callback;
    bool new_client = true;

    if (attr_hidl == NULL) {
        ALOGE("Invalid hidl attributes\n");
        return Void();
    }
    in_ch = attr_hidl.data()->in_media_config.ch_info.channels;
    out_ch = attr_hidl.data()->out_media_config.ch_info.channels;
    attr = (struct pal_stream_attributes *)calloc(1,
                                          sizeof(struct pal_stream_attributes));
    if (!attr) {
        ALOGE("Not enough memory for attr\n");
        return Void();
    }
    attr->type = (pal_stream_type_t)attr_hidl.data()->type;
    attr->info.opt_stream_info.version = attr_hidl.data()->info.version;
    attr->info.opt_stream_info.size = attr_hidl.data()->info.size;
    attr->info.opt_stream_info.duration_us = attr_hidl.data()->info.duration_us;
    attr->info.opt_stream_info.has_video = attr_hidl.data()->info.has_video;
    attr->info.opt_stream_info.is_streaming = attr_hidl.data()->info.is_streaming;
    attr->flags = (pal_stream_flags_t)attr_hidl.data()->flags;
    attr->direction = (pal_stream_direction_t)attr_hidl.data()->direction;
    attr->in_media_config.sample_rate =
                     attr_hidl.data()->in_media_config.sample_rate;
    attr->in_media_config.bit_width = attr_hidl.data()->in_media_config.bit_width;
    attr->in_media_config.aud_fmt_id =
                     (pal_audio_fmt_t)attr_hidl.data()->in_media_config.aud_fmt_id;
    attr->in_media_config.ch_info.channels = attr_hidl.data()->in_media_config.ch_info.channels;
    memcpy(&attr->in_media_config.ch_info.ch_map, &attr_hidl.data()->in_media_config.ch_info.ch_map,
           sizeof(uint8_t [64]));
    attr->out_media_config.sample_rate = attr_hidl.data()->out_media_config.sample_rate;
    attr->out_media_config.bit_width =
                        attr_hidl.data()->out_media_config.bit_width;
    attr->out_media_config.aud_fmt_id =
                        (pal_audio_fmt_t)attr_hidl.data()->out_media_config.aud_fmt_id;
    attr->out_media_config.ch_info.channels = attr_hidl.data()->out_media_config.ch_info.channels;
    memcpy(&attr->out_media_config.ch_info.ch_map, &attr_hidl.data()->out_media_config.ch_info.ch_map,
           sizeof(uint8_t [64]));

    if (devs_hidl.size()) {
        PalDevice *dev_hidl = NULL;
        devices = (struct pal_device *)calloc (1,
                                      sizeof(struct pal_device) * noOfDevices);
        if (!devices) {
            ALOGE("Not enough memory for devices\n");
            return Void();
        }
        dev_hidl = (PalDevice *)devs_hidl.data();
        for ( cnt = 0; cnt < noOfDevices; cnt++) {
             devices[cnt].id = (pal_device_id_t)dev_hidl->id;
             devices[cnt].config.sample_rate = dev_hidl->config.sample_rate;
             devices[cnt].config.bit_width = dev_hidl->config.bit_width;
             devices[cnt].config.ch_info.channels = dev_hidl->config.ch_info.channels;
             memcpy(&devices[cnt].config.ch_info.ch_map, &dev_hidl->config.ch_info.ch_map,
                    sizeof(uint8_t [64]));
             devices[cnt].config.aud_fmt_id =
                                  (pal_audio_fmt_t)dev_hidl->config.aud_fmt_id;
             dev_hidl =  (PalDevice *)(dev_hidl + sizeof(PalDevice));
        }
    }
   // print_attr(attr); print_devices(noOfDevices, devices);
    if (modskv_hidl.size()) {
        modifiers = (struct modifier_kv *)calloc(1,
                                   sizeof(struct modifier_kv) * noOfModifiers);
        if (!modifiers) {
            ALOGE("Not enough memory for modifiers\n");
#ifdef SEC_AUDIO_EARLYDROP_PATCH
            free(devices);
#endif
            return Void();
        }
        memcpy(modifiers, modskv_hidl.data(),
               sizeof(struct modifier_kv) * noOfModifiers);
    }
    //print_kv_modifier(noOfModifiers, modifiers);
    sr_clbk_data->setSessionAttr(attr);

    ret = pal_stream_open(attr, noOfDevices, devices, noOfModifiers, modifiers,
                          callback, (uint64_t)sr_clbk_data.get(), &stream_handle);

    if (!ret) {
        for(auto& client: mPalClients) {
            if (client->pid == pid) {
                /*Another session from the same client*/
                ALOGI("Add session for same pid %d session %pK", pid, (uint64_t)stream_handle);
                struct session_info session;
                session.session_handle = (uint64_t)stream_handle;
                session.callback_binder = sr_clbk_data;
                ALOGV("hdle %x binder %p", session.session_handle, session.callback_binder.get());
                {
                    std::lock_guard<std::mutex> lock(client->mActiveSessionsLock);
                    client->mActiveSessions.push_back(session);
                }
                new_client = false;
                break;
            }
        }
        if (new_client) {
            auto client = std::make_shared<client_info>();
            struct session_info session;
            ALOGI("session from new pid %d session %pK", pid, (uint64_t)stream_handle);
            client->pid = pid;
            session.session_handle = (uint64_t)stream_handle;
            session.callback_binder = sr_clbk_data;
            ALOGV("hdle %x binder %p", session.session_handle, session.callback_binder.get());
            {
                std::lock_guard<std::mutex> lock(client->mActiveSessionsLock);
                client->mActiveSessions.push_back(session);
            }
            mPalClients.push_back(client);
            if (cb != NULL) {
                if (this->mDeathRecipient.get() == nullptr) {
                    this->mDeathRecipient = new PalClientDeathRecipient(this);
                }
                cb->linkToDeath(this->mDeathRecipient, pid);
            }
        }
    } else {
        /*stream_open failed, free the callback binder object*/
        sr_clbk_data.clear();
    }
    _hidl_cb(ret, (uint64_t)stream_handle);

#ifdef SEC_AUDIO_EARLYDROP_PATCH
    if (devices)
        free(devices);
    if (modifiers)
        free(modifiers);
    free(attr);
#endif
    return Void();
}

Return<int32_t> PAL::ipc_pal_stream_close(const uint64_t streamHandle)
{
    int pid = ::android::hardware::IPCThreadState::self()->getCallingPid();
    Return<int32_t> status = pal_stream_close((pal_stream_handle_t *)streamHandle);

    for (auto itr = mPalClients.begin(); itr != mPalClients.end(); ) {
        auto client = *itr;
        if (client->pid == pid) {
            {
                std::lock_guard<std::mutex> lock(client->mActiveSessionsLock);
                auto sItr = client->mActiveSessions.begin();
                for (; sItr != client->mActiveSessions.end(); sItr++) {
                    if (sItr->session_handle == streamHandle) {
                        /*close the shared mem fds dupped in PAL server context*/
                        for (int i=0; i < sItr->callback_binder->sharedMemFdList.size(); i++) {
                             close(sItr->callback_binder->sharedMemFdList[i].second);
                        }
                        ALOGV("Closing the session %pK", streamHandle);
                        sItr->callback_binder->sharedMemFdList.clear();
                        sItr->callback_binder.clear();
                        break;
                    }
                }
                if (sItr != client->mActiveSessions.end()) {
                    ALOGV("Delete session info %pK", sItr->session_handle);
                    client->mActiveSessions.erase(sItr);
                }
            }
            if (client->mActiveSessions.empty()) {
                ALOGV("Delete client info");
                itr = mPalClients.erase(itr);
            } else {
                itr++;
            }
            break;
        }
    }
    return status;
}

Return<int32_t> PAL::ipc_pal_stream_start(const uint64_t streamHandle) {

    return pal_stream_start((pal_stream_handle_t *)streamHandle);
}

Return<int32_t> PAL::ipc_pal_stream_stop(const uint64_t streamHandle) {
    return pal_stream_stop((pal_stream_handle_t *)streamHandle);
}

Return<int32_t> PAL::ipc_pal_stream_pause(const uint64_t streamHandle) {
    return pal_stream_pause((pal_stream_handle_t *)streamHandle);
}

Return<int32_t> PAL::ipc_pal_stream_drain(uint64_t streamHandle, PalDrainType type)
{
    pal_drain_type_t drain_type = (pal_drain_type_t) type;
    return pal_stream_drain((pal_stream_handle_t *)streamHandle,
                             drain_type);
}

Return<int32_t> PAL::ipc_pal_stream_flush(const uint64_t streamHandle) {
    return pal_stream_flush((pal_stream_handle_t *)streamHandle);
}

Return<int32_t> PAL::ipc_pal_stream_suspend(const uint64_t streamHandle) {
    return pal_stream_suspend((pal_stream_handle_t *)streamHandle);
}

Return<int32_t> PAL::ipc_pal_stream_resume(const uint64_t streamHandle) {
    return pal_stream_resume((pal_stream_handle_t *)streamHandle);
}

Return<void> PAL::ipc_pal_stream_set_buffer_size(const uint64_t streamHandle,
                                             const PalBufferConfig& in_buff_config,
                                             const PalBufferConfig& out_buff_config,
                                             ipc_pal_stream_set_buffer_size_cb _hidl_cb)
{
    int32_t ret = 0;
    pal_buffer_config_t out_buf_cfg, in_buf_cfg;
    PalBufferConfig in_buff_config_ret, out_buff_config_ret;

    in_buf_cfg.buf_count = in_buff_config.buf_count;
    in_buf_cfg.buf_size = in_buff_config.buf_size;
    in_buf_cfg.max_metadata_size =  in_buff_config.max_metadata_size;

    out_buf_cfg.buf_count = out_buff_config.buf_count;
    out_buf_cfg.buf_size = out_buff_config.buf_size;
    out_buf_cfg.max_metadata_size =  out_buff_config.max_metadata_size;

    ret = pal_stream_set_buffer_size((pal_stream_handle_t *)streamHandle,
                                    &in_buf_cfg, &out_buf_cfg);

    in_buff_config_ret.buf_count = in_buf_cfg.buf_count;
    in_buff_config_ret.buf_size = in_buf_cfg.buf_size;
    in_buff_config_ret.max_metadata_size = in_buf_cfg.max_metadata_size;

    out_buff_config_ret.buf_count = out_buf_cfg.buf_count;
    out_buff_config_ret.buf_size = out_buf_cfg.buf_size;
    out_buff_config_ret.max_metadata_size = out_buf_cfg.max_metadata_size;

    _hidl_cb(ret, in_buff_config_ret, out_buff_config_ret);
    return Void();
}

Return<void> PAL::ipc_pal_stream_get_buffer_size(const uint64_t streamHandle,
                                                 uint32_t inBufSize,
                                                 uint32_t outBufSize,
                                                 ipc_pal_stream_get_buffer_size_cb _hidl_cb)
{
#if 0 // This api is not yet implemented in PAL
    int32_t ret = 0;
    size_t inBufSz_l = (size_t)inBufSize;
    size_t outBufSz_l = (size_t)outBufSize;
    ret = pal_stream_get_buffer_size((pal_stream_handle_t *)streamHandle,
                                    &inBufSz_l, &outBufSz_l);
    _hidl_cb(ret, (uint32_t)inBufSz_l, (uint32_t)outBufSz_l);
#endif
    return Void();
}


Return<int32_t> PAL::ipc_pal_stream_write(const uint64_t streamHandle,
                                          const hidl_vec<PalBuffer>& buff_hidl) {
    int32_t ret = -EINVAL;
    struct pal_buffer buf = {0};
#ifdef SEC_AUDIO_EARLYDROP_PATCH
    buf.buffer = nullptr;
    buf.ts = nullptr;
    buf.metadata = nullptr;
#endif

    uint32_t bufSize;
    bufSize = buff_hidl.data()->size;
    if (buff_hidl.data()->buffer.size() == bufSize)
        buf.buffer = (uint8_t *)calloc(1, bufSize);
    buf.size = (size_t)bufSize;
    buf.offset = (size_t)buff_hidl.data()->offset;
    buf.ts = (struct timespec *) calloc(1, sizeof(struct timespec));
    if (!buf.ts) {
#ifdef SEC_AUDIO_EARLYDROP_PATCH
        free(buf.buffer);
#endif
        ALOGE("Not enough memory for buf.ts\n");
        return -ENOMEM;
    }
    buf.ts->tv_sec =  buff_hidl.data()->timeStamp.tvSec;
    buf.ts->tv_nsec = buff_hidl.data()->timeStamp.tvNSec;
    buf.flags = buff_hidl.data()->flags;
    if (buff_hidl.data()->metadataSz) {
        buf.metadata_size = buff_hidl.data()->metadataSz;
        buf.metadata = (uint8_t *)calloc(1, buf.metadata_size);
        if (!buf.metadata) {
            ALOGE("Not enough memory for buf.metadata\n");
#ifdef SEC_AUDIO_EARLYDROP_PATCH
            ret = -ENOMEM;
            goto exit;
#else
            return -ENOMEM;
#endif
        }
        memcpy(buf.metadata, buff_hidl.data()->metadata.data(),
               buf.metadata_size);
    }
    const native_handle *allochandle = nullptr;
    int dupfd = -1;

    allochandle = buff_hidl.data()->alloc_info.alloc_handle.handle();

    buf.alloc_info.alloc_handle = dup(allochandle->data[0]);
    add_input_and_dup_fd(streamHandle, allochandle->data[1], buf.alloc_info.alloc_handle);

    ALOGV("%s:%d input fd %d dup fd %d \n", __func__,__LINE__, allochandle->data[1], buf.alloc_info.alloc_handle);
    buf.alloc_info.alloc_size = buff_hidl.data()->alloc_info.alloc_size;
    buf.alloc_info.offset = buff_hidl.data()->alloc_info.offset;

    if (buf.buffer != NULL)
        memcpy(buf.buffer, buff_hidl.data()->buffer.data(), bufSize);
    ALOGV("%s:%d sz %d", __func__,__LINE__,bufSize);
    ret = pal_stream_write((pal_stream_handle_t *)streamHandle, &buf);

#ifdef SEC_AUDIO_EARLYDROP_PATCH
exit:
    if (buf.ts)
        free(buf.ts);
    if (buf.metadata)
        free(buf.metadata);
    if (buf.buffer)
        free(buf.buffer);
#else
    free(buf.buffer);
#endif
    return ret;
}

Return<void> PAL::ipc_pal_stream_read(const uint64_t streamHandle,
                                      const hidl_vec<PalBuffer>& inBuff_hidl,
                                      ipc_pal_stream_read_cb _hidl_cb) {
    struct pal_buffer buf;
    int32_t ret = 0;
    hidl_vec<PalBuffer> outBuff_hidl;
    uint32_t bufSize;

#ifdef SEC_AUDIO_EARLYDROP_PATCH
    buf.buffer = nullptr;
    buf.metadata = nullptr;
#endif

    bufSize = inBuff_hidl.data()->size;
    buf.buffer = (uint8_t *)calloc(1, bufSize);
    buf.size = (size_t)bufSize;
    buf.metadata_size = inBuff_hidl.data()->metadataSz;
    buf.metadata = (uint8_t *)calloc(1, buf.metadata_size);
    if (!buf.metadata) {
        ALOGE("Not enough memory for buf.metadata\n");
#ifdef SEC_AUDIO_EARLYDROP_PATCH
        if (buf.buffer)
           free(buf.buffer);
#endif
        return Void();
    }
    const native_handle *allochandle = nullptr;
    int dupfd = -1;

    allochandle = inBuff_hidl.data()->alloc_info.alloc_handle.handle();

    buf.alloc_info.alloc_handle = dup(allochandle->data[0]);
    add_input_and_dup_fd(streamHandle, allochandle->data[1], buf.alloc_info.alloc_handle);
    ALOGV("%s:%d input fd %d dup fd %d \n", __func__,__LINE__, allochandle->data[1], buf.alloc_info.alloc_handle);

    buf.alloc_info.alloc_size = inBuff_hidl.data()->alloc_info.alloc_size;
    buf.alloc_info.offset = inBuff_hidl.data()->alloc_info.offset;

    ret = pal_stream_read((pal_stream_handle_t *)streamHandle, &buf);
    if (ret > 0) {
        outBuff_hidl.resize(sizeof(struct pal_buffer));
        outBuff_hidl.data()->size = (uint32_t)buf.size;
        outBuff_hidl.data()->offset = (uint32_t)buf.offset;
        outBuff_hidl.data()->buffer.resize(buf.size);
        memcpy(outBuff_hidl.data()->buffer.data(), buf.buffer,
               buf.size);
        if (buf.ts) {
          outBuff_hidl.data()->timeStamp.tvSec = buf.ts->tv_sec;
          outBuff_hidl.data()->timeStamp.tvNSec = buf.ts->tv_nsec;
        }
        if (buf.metadata_size) {
           outBuff_hidl.data()->metadata.resize(buf.metadata_size);
           memcpy(outBuff_hidl.data()->metadata.data(),
                  buf.metadata, buf.metadata_size);
        }
    }
    _hidl_cb(ret, outBuff_hidl);
    if (buf.buffer)
       free(buf.buffer);
#ifdef SEC_AUDIO_EARLYDROP_PATCH
    if (buf.metadata)
       free(buf.metadata);
#endif

    return Void();
}

Return<int32_t> PAL::ipc_pal_stream_set_param(const uint64_t streamHandle, uint32_t paramId,
                                        const hidl_vec<PalParamPayload>& paramPayload)
{
    int32_t ret = 0;
    pal_param_payload *param_payload;
    param_payload = (pal_param_payload *)calloc (1,
                                    sizeof(pal_param_payload) + paramPayload.data()->size);
    if (!param_payload) {
        ALOGE("Not enough memory for param_payload\n");
        return -ENOMEM;
    }
    param_payload->payload_size = paramPayload.data()->size;
    memcpy(param_payload->payload, paramPayload.data()->payload.data(),
           param_payload->payload_size);
    ret = pal_stream_set_param((pal_stream_handle_t *)streamHandle, paramId, param_payload);
    free(param_payload);
    return ret;
}

Return<void> PAL::ipc_pal_stream_get_param(const uint64_t streamHandle,
                                         uint32_t paramId,
                                         ipc_pal_stream_get_param_cb _hidl_cb)
{
    int32_t ret = 0;
    pal_param_payload *param_payload;
    hidl_vec<PalParamPayload> paramPayload;
    ret = pal_stream_get_param((pal_stream_handle_t *)streamHandle, paramId, &param_payload);
    if (ret == 0) {
        paramPayload.resize(sizeof(PalParamPayload));
        paramPayload.data()->payload.resize(param_payload->payload_size);
        paramPayload.data()->size = param_payload->payload_size;
        memcpy(paramPayload.data()->payload.data(), param_payload->payload,
               param_payload->payload_size);
    }
    _hidl_cb(ret, paramPayload);
    return Void();
}

Return<void> PAL::ipc_pal_stream_get_device(const uint64_t streamHandle,
                                    ipc_pal_stream_get_device_cb _hidl_cb)
{

    return Void();
}

Return<int32_t> PAL::ipc_pal_stream_set_device(const uint64_t streamHandle,
                                    uint32_t noOfDevices,
                                    const hidl_vec<PalDevice> &devs_hidl)
{
    uint32_t dev_size = 0;
    struct pal_device *devices = NULL;
    int cnt = 0;

    if (devs_hidl.size()) {
        PalDevice *dev_hidl = NULL;
        devices = (struct pal_device *)calloc (1,
                                    sizeof(struct pal_device) * noOfDevices);
        if (!devices) {
            ALOGE("Not enough memory for devices\n");
            return -ENOMEM;
        }
        dev_hidl = (PalDevice *)devs_hidl.data();
        for (cnt = 0; cnt < noOfDevices; cnt++) {
            devices[cnt].id = (pal_device_id_t)dev_hidl->id;
            devices[cnt].config.sample_rate = dev_hidl->config.sample_rate;
            devices[cnt].config.bit_width = dev_hidl->config.bit_width;
            devices[cnt].config.ch_info.channels = dev_hidl->config.ch_info.channels;
            memcpy(&devices[cnt].config.ch_info.ch_map, &dev_hidl->config.ch_info.ch_map,
                   sizeof(uint8_t [64]));
            devices[cnt].config.aud_fmt_id =
                                (pal_audio_fmt_t)dev_hidl->config.aud_fmt_id;
            dev_hidl = (PalDevice *)(dev_hidl + sizeof(PalDevice));
        }
    }
#ifdef SEC_AUDIO_EARLYDROP_PATCH
    Return<int32_t> rtn = pal_stream_set_device((pal_stream_handle_t *)streamHandle,
                                  noOfDevices, devices);
    free(devices);
    return rtn;
#else
    return pal_stream_set_device((pal_stream_handle_t *)streamHandle,
                                  noOfDevices, devices);
#endif
}

Return<void> PAL::ipc_pal_stream_get_volume(const uint64_t streamHandle,
                                    ipc_pal_stream_get_volume_cb _hidl_cb)
{
    return Void();
}

Return<int32_t> PAL::ipc_pal_stream_set_volume(const uint64_t streamHandle,
                                    const hidl_vec<PalVolumeData> &vol)
{
    struct pal_volume_data *volume = nullptr;
    uint32_t noOfVolPairs = vol.data()->noOfVolPairs;
    volume = (struct pal_volume_data *) calloc(1,
                                        sizeof(struct pal_volume_data) +
                                        noOfVolPairs * sizeof(pal_channel_vol_kv));
    if (!volume) {
        ALOGE("Not enough memory for volume\n");
        return -ENOMEM;
    }
    memcpy(volume->volume_pair, vol.data()->volPair.data(),
            noOfVolPairs * sizeof(pal_channel_vol_kv));
    volume->no_of_volpair = noOfVolPairs;
#ifdef SEC_AUDIO_EARLYDROP_PATCH
    Return<int32_t> rtn = pal_stream_set_volume((pal_stream_handle_t *)streamHandle, volume);
    free(volume)
    return rtn;
#else
    return pal_stream_set_volume((pal_stream_handle_t *)streamHandle, volume);
#endif
}

Return<void> PAL::ipc_pal_stream_get_mute(const uint64_t streamHandle,
                                    ipc_pal_stream_get_mute_cb _hidl_cb)
{
#if 0
    int32_t ret = 0;
    bool state;
    ret = pal_stream_get_mute((pal_stream_handle_t *)streamHandle, &state);
    _hidl_cb(ret, state);
#endif
    return Void();
}

Return<int32_t> PAL::ipc_pal_stream_set_mute(const uint64_t streamHandle,
                                    bool state)
{
    return pal_stream_set_mute((pal_stream_handle_t *)streamHandle, state);
}

Return<void> PAL::ipc_pal_get_mic_mute(ipc_pal_get_mic_mute_cb _hidl_cb)
{
    return Void();
}

Return<int32_t> PAL::ipc_pal_set_mic_mute(bool state)
{
    return 0;
}

Return<void> PAL::ipc_pal_get_timestamp(const uint64_t streamHandle,
                                   ipc_pal_get_timestamp_cb _hidl_cb)
{
    struct pal_session_time stime;
    int32_t ret = 0;
    hidl_vec<PalSessionTime> sessTime_hidl;
    sessTime_hidl.resize(sizeof(struct pal_session_time));
    ret = pal_get_timestamp((pal_stream_handle_t *)streamHandle, &stime);
    memcpy(sessTime_hidl.data(), &stime, sizeof(struct pal_session_time));
    _hidl_cb(ret, sessTime_hidl);
    return Void();
}

Return<int32_t> PAL::ipc_pal_add_remove_effect(const uint64_t streamHandle,
                                          const PalAudioEffect effect,
                                          bool enable)
{
    return pal_add_remove_effect((pal_stream_handle_t *)streamHandle,
                                   (pal_audio_effect_t) effect, enable);
}

Return<int32_t> PAL::ipc_pal_set_param(uint32_t paramId,
                                       const hidl_vec<uint8_t>& payload_hidl,
                                       uint32_t size)
{   uint32_t ret = -EINVAL;
    uint8_t *payLoad;
    payLoad = (uint8_t*) calloc (1, size);
    if (!payLoad) {
        ALOGE("Not enough memory for payLoad\n");
        return -ENOMEM;
    }
    memcpy(payLoad, payload_hidl.data(), size);

    ret = pal_set_param(paramId, (void *)payLoad, size);
    free(payLoad);
    return ret;
}

Return<void> PAL::ipc_pal_get_param(uint32_t paramId,
                                    ipc_pal_get_param_cb _hidl_cb)
{
    int32_t ret = 0;
    void *payLoad;
    hidl_vec<uint8_t> payload_hidl;
    size_t sz;
    ret = pal_get_param(paramId, &payLoad, &sz, NULL);
    if (!payLoad) {
        ALOGE("Not enough memory for payLoad\n");
        return Void();
    }
    payload_hidl.resize(sz);
    memcpy(payload_hidl.data(), payLoad, sz);
    _hidl_cb(ret, payload_hidl, sz);
    return Void();
}

Return<void>PAL::ipc_pal_stream_create_mmap_buffer(PalStreamHandle streamHandle,
                              int32_t min_size_frames,
                              ipc_pal_stream_create_mmap_buffer_cb _hidl_cb)
{

    int32_t ret = 0;
    struct pal_mmap_buffer info;
    hidl_vec<PalMmapBuffer> mMapBuffer_hidl;
    mMapBuffer_hidl.resize(sizeof(struct pal_mmap_buffer));
    ret = pal_stream_create_mmap_buffer((pal_stream_handle_t *)streamHandle, min_size_frames, &info);
    mMapBuffer_hidl.data()->buffer = (uint64_t)info.buffer;
    mMapBuffer_hidl.data()->fd = info.fd;
    mMapBuffer_hidl.data()->buffer_size_frames = info.buffer_size_frames;
    mMapBuffer_hidl.data()->burst_size_frames = info.burst_size_frames;
    mMapBuffer_hidl.data()->flags = (PalMmapBufferFlags)info.flags;
    _hidl_cb(ret, mMapBuffer_hidl);
    return Void();
}

Return<void>PAL::ipc_pal_stream_get_mmap_position(PalStreamHandle streamHandle,
                               ipc_pal_stream_get_mmap_position_cb _hidl_cb)
{
    int32_t ret = 0;
    struct pal_mmap_position mmap_position;
    hidl_vec<PalMmapPosition> mmap_position_hidl;
    mmap_position_hidl.resize(sizeof(struct pal_mmap_position));
    ret = pal_stream_get_mmap_position((pal_stream_handle_t *)streamHandle, &mmap_position);
    memcpy(mmap_position_hidl.data(), &mmap_position, sizeof(struct pal_mmap_position));
    return Void();
}

Return<int32_t>PAL::ipc_pal_register_global_callback(const sp<IPALCallback>& cb, uint64_t cookie)
{
    return 0;
}

Return<void>PAL::ipc_pal_gef_rw_param(uint32_t paramId, const hidl_vec<uint8_t> &param_payload,
                              int32_t payload_size, PalDeviceId dev_id,
                              PalStreamType strm_type, uint8_t dir,
                              ipc_pal_gef_rw_param_cb _hidl_cb)
{
    return Void();
}

Return<void>PAL::ipc_pal_stream_get_tags_with_module_info(PalStreamHandle streamHandle,
                               uint32_t size,
                               ipc_pal_stream_get_tags_with_module_info_cb _hidl_cb)
{
    int32_t ret = -EINVAL;
    uint8_t *payload = NULL;
    size_t sz = size;
    hidl_vec<uint8_t> payloadRet;

    if (size > 0) {
        payload = (uint8_t *)calloc(1, size);
        if (!payload) {
            ALOGE("Not enough memory for payload\n");
            return Void();
        }
    }

    ret = pal_stream_get_tags_with_module_info((pal_stream_handle_t *)streamHandle, &sz, payload);
    if (!ret && (sz <= size)) {
        payloadRet.resize(sz);
        memcpy(payloadRet.data(), payload, sz);
    }
    _hidl_cb(ret, sz, payloadRet);
    free(payload);
    return Void();
}



IPAL* HIDL_FETCH_IPAL(const char* /* name */) {
    ALOGV("%s");
    return new PAL();
}

}
}
}
}
}
}
