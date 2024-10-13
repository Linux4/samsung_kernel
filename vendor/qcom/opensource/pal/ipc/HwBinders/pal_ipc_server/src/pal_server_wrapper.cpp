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
 *
 * Changes from Qualcomm Innovation Center are provided under the following license:
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "pal_server_wrapper"
#include "inc/pal_server_wrapper.h"
#include "MetadataParser.h"
#include <hwbinder/IPCThreadState.h>

#define MAX_CACHE_SIZE 64

using vendor::qti::hardware::pal::V1_0::IPAL;
using android::hardware::hidl_handle;
using android::hardware::hidl_memory;

// map<FD, map<offset, input_frame_id>>
std::map<int, std::map<uint32_t, uint64_t>> gInputsPendingAck;
std::mutex gInputsPendingAckLock;

void addToPendingInputs(int fd, uint32_t offset, uint32_t ip_frame_id) {
    ALOGV("%s: fd %d, offset %u", __func__, fd, offset);
    std::lock_guard<std::mutex> pendingAcksLock(gInputsPendingAckLock);
    auto itFd = gInputsPendingAck.find(fd);
    if (itFd != gInputsPendingAck.end()) {
        gInputsPendingAck[fd][offset] = ip_frame_id;
        ALOGV("%s: added offset %u and frame id %u", __func__, offset,
               (uint32_t)gInputsPendingAck[fd][offset]);
    } else {
        //create new map<offset,input_buffer_index> and add to FD map
        ALOGV("%s: added map for fd %lu", __func__, (unsigned long)fd);
        gInputsPendingAck.insert(std::make_pair(fd, std::map<uint32_t, uint64_t>()));
        ALOGV("%s: added frame id %lu for fd %d offset %u", __func__,
                    (unsigned long)ip_frame_id,  fd, (unsigned int)offset);
        gInputsPendingAck[fd].insert(std::make_pair(offset, ip_frame_id));
    }
}

int getInputBufferIndex(int fd, uint32_t offset, uint64_t &buf_index) {
    int status = 0;

    std::lock_guard<std::mutex> pendingAcksLock(gInputsPendingAckLock);
    ALOGV("%s: fd %d, offset %u", __func__, fd, offset);
    std::map<int, std::map<uint32_t, uint64_t>>::iterator itFd = gInputsPendingAck.find(fd);
    if (itFd != gInputsPendingAck.end()) {
        std::map<uint32_t, uint64_t> offsetToFrameIdxMap = itFd->second;
        auto itOffsetFrameIdxPair = offsetToFrameIdxMap.find(offset);
        if (itOffsetFrameIdxPair != offsetToFrameIdxMap.end()){
            buf_index = itOffsetFrameIdxPair->second;
            ALOGV("%s ip_frame_id=%lu", __func__, (unsigned long)buf_index);
        } else {
            status = -EINVAL;
            ALOGE("%s: Entry doesn't exist for FD 0x%x and offset 0x%x",
                    __func__, fd, offset);
        }
        gInputsPendingAck.erase(itFd);
    }
    return status;
}

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
    std::lock_guard<std::mutex> lock(mPalInstance->mClientLock);
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
                   sItr->callback_binder->sharedMemFdList.clear();
                   sItr->callback_binder.clear();
                }
                client->mActiveSessions.clear();
            }
            itr = clients.erase(itr);
            break;
        }
    }
}

void PAL::add_input_and_dup_fd(const uint64_t streamHandle, int input_fd, int dup_fd)
{
    std::vector<std::pair<int, int>>::iterator it;
    std::lock_guard<std::mutex> guard(mClientLock);
    for (auto& s: mPalClients) {
        std::lock_guard<std::mutex> lock(s->mActiveSessionsLock);
        for (int i = 0; i < s->mActiveSessions.size(); i++) {
            session_info session = s->mActiveSessions[i];
            if (session.session_handle == streamHandle) {
                /*If number of FDs increase than the MAX Cache size we delete the oldest one
                  NOTE: We still create a new fd for every input fd*/
                if (session.callback_binder->sharedMemFdList.size() > MAX_CACHE_SIZE) {
                    ALOGE("%s cache limit exceeded handle %p fd [input %d - dup %d]",
                            __func__ , streamHandle, input_fd, dup_fd );
                }
                session.callback_binder->sharedMemFdList.push_back(
                                            std::make_pair(input_fd, dup_fd));
            }
        }
    }
}


static void printFdList(const std::vector<std::pair<int, int>> &list, const char * caller) {
    if (list.size() > 0 ) {
        std::string s;
        for (auto & x : list) {
            s.append( "{ ");
            s.append(std::to_string(x.first));
            s.append( " : ");
            s.append(std::to_string(x.second));
            s.append("} ");
        }
        ALOGV("%s size %d, list %s", caller, list.size(), s.c_str());
    }
}

int32_t SrvrClbk::callReadWriteTransferThread(
        PalReadWriteDoneCommand cmd,
        const uint8_t* data, size_t dataSize) {
    if (!mCommandMQ->write(&cmd)) {
        ALOGE("command message queue write failed for %d", cmd);
        return -EAGAIN;
    }
    if (data != nullptr) {
        size_t availableToWrite = mDataMQ->availableToWrite();
        if (dataSize > availableToWrite) {
            ALOGW("truncating write data from %lld to %lld due to insufficient data queue space",
                    (long long)dataSize, (long long)availableToWrite);
            dataSize = availableToWrite;
        }
        if (!mDataMQ->write(data, dataSize)) {
            ALOGE("data message queue write failed");
        }
    }
    mEfGroup->wake(static_cast<uint32_t>(PalMessageQueueFlagBits::NOT_EMPTY));

    uint32_t efState = 0;
retry:
    status_t ret = mEfGroup->wait(static_cast<uint32_t>(PalMessageQueueFlagBits::NOT_FULL), &efState);
    if (ret == -EAGAIN || ret == -EINTR) {
        // Spurious wakeup. This normally retries no more than once.
        ALOGE("%s: Wait returned error", __func__);
        goto retry;
    }
    return 0;
}

int32_t SrvrClbk::prepare_mq_for_transfer(uint64_t streamHandle, uint64_t cookie) {
    std::unique_ptr<DataMQ> tempDataMQ;
    std::unique_ptr<CommandMQ> tempCommandMQ;
    PalReadWriteDoneResult retval;
    auto ret = clbk_binder->prepare_mq_for_transfer(streamHandle, cookie,
            [&](PalReadWriteDoneResult r,
                    const DataMQ::Descriptor& dataMQ,
                    const CommandMQ::Descriptor& commandMQ) {
                retval = r;
                if (retval == PalReadWriteDoneResult::OK) {
                    tempDataMQ.reset(new DataMQ(dataMQ));
                    tempCommandMQ.reset(new CommandMQ(commandMQ));
                    if (tempDataMQ->isValid() && tempDataMQ->getEventFlagWord()) {
                        EventFlag::createEventFlag(tempDataMQ->getEventFlagWord(), &mEfGroup);
                    }
                }
            });
    if (retval != PalReadWriteDoneResult::OK) {
        return NO_INIT;
    }
    if (!tempDataMQ || !tempDataMQ->isValid() ||
            !tempCommandMQ || !tempCommandMQ->isValid() ||
            !mEfGroup) {
        ALOGE_IF(!tempDataMQ, "Failed to obtain data message queue for transfer");
        ALOGE_IF(tempDataMQ && !tempDataMQ->isValid(),
                 "Data message queue for transfer is invalid");
        ALOGE_IF(!tempCommandMQ, "Failed to obtain command message queue for transfer");
        ALOGE_IF(tempCommandMQ && !tempCommandMQ->isValid(),
                 "Command message queue for transfer is invalid");
        ALOGE_IF(!mEfGroup, "Event flag creation for transfer failed");
        return NO_INIT;
    }

    mDataMQ = std::move(tempDataMQ);
    mCommandMQ = std::move(tempCommandMQ);
    return 0;
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
        std::lock_guard<std::mutex> guard(PAL::getInstance()->mClientLock);
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
        hidl_vec<PalCallbackBuffer> rwDonePayloadHidl;
        PalCallbackBuffer *rwDonePayload;
        struct pal_event_read_write_done_payload *rw_done_payload;
        int input_fd = -1;
        int fdToBeClosed = -1;

        rw_done_payload = (struct pal_event_read_write_done_payload *)event_data;
        /*
         * Find the original fd that was passed by client based on what
         * input and dup fd list and send that back.
         */
        PAL::getInstance()->mClientLock.lock();
        for (auto& s: PAL::getInstance()->mPalClients) {
            std::lock_guard<std::mutex> lock(s->mActiveSessionsLock);
            for (int idx = 0; idx < s->mActiveSessions.size(); idx++) {
                session_info session = s->mActiveSessions[idx];
                if (session.session_handle != (uint64_t)stream_handle) {
                    continue;
                }
                std::vector<std::pair<int, int>>::iterator it;
                for (int i = 0; i < sr_clbk_dat->sharedMemFdList.size(); i++) {
                    if (sr_clbk_dat->sharedMemFdList[i].second ==
                            rw_done_payload->buff.alloc_info.alloc_handle) {
                        input_fd = sr_clbk_dat->sharedMemFdList[i].first;
                        it = (sr_clbk_dat->sharedMemFdList.begin() + i);
                        if (it != sr_clbk_dat->sharedMemFdList.end()) {
                            fdToBeClosed = sr_clbk_dat->sharedMemFdList[i].second;
                            sr_clbk_dat->sharedMemFdList.erase(it);
                            ALOGV("Removing fd [input %d - dup %d]", input_fd, fdToBeClosed);
                        }
                        break;
                    }
                }
            }
        }
        PAL::getInstance()->mClientLock.unlock();

        rwDonePayloadHidl.resize(sizeof(pal_callback_buffer));
        rwDonePayload = (PalCallbackBuffer *)rwDonePayloadHidl.data();
        rwDonePayload->status = rw_done_payload->status;
        switch (rw_done_payload->md_status) {
            case ENOTRECOVERABLE: {
                ALOGE("%s: Error, md cannot be parsed in buffer", __func__);
                rwDonePayload->status = rw_done_payload->md_status;
                break;
            }
            case EOPNOTSUPP: {
                ALOGE("%s: Error, md id not recognized in buffer", __func__);
                rwDonePayload->status = rw_done_payload->md_status;
                break;
            }
            case ENOMEM: {
                ALOGE("%s: Error, md buffer size received is small", __func__);
                rwDonePayload->status = rw_done_payload->md_status;
                break;
            }
            default: {
                if (rw_done_payload->md_status) {
                    ALOGE("%s: Error received during callback, md status = 0x%x",
                           __func__, rw_done_payload->md_status);
                    rwDonePayload->status = rw_done_payload->md_status;
                }
                break;
            }
        }

        if (!rwDonePayload->status) {
            auto metadataParser = std::make_unique<MetadataParser>();
            if (event_id == PAL_STREAM_CBK_EVENT_READ_DONE) {
                auto cb_buf_info = std::make_unique<pal_clbk_buffer_info>();
                rwDonePayload->status = metadataParser->parseMetadata(
                            rw_done_payload->buff.metadata,
                            rw_done_payload->buff.metadata_size,
                            cb_buf_info.get());
                rwDonePayload->cbBufInfo.frame_index = cb_buf_info->frame_index;
                rwDonePayload->cbBufInfo.sample_rate = cb_buf_info->sample_rate;
                rwDonePayload->cbBufInfo.channel_count = cb_buf_info->channel_count;
                rwDonePayload->cbBufInfo.bit_width = cb_buf_info->bit_width;
            } else if (event_id == PAL_STREAM_CBK_EVENT_WRITE_READY) {
                rwDonePayload->status = getInputBufferIndex(
                            rw_done_payload->buff.alloc_info.alloc_handle,
                            rw_done_payload->buff.alloc_info.offset,
                            rwDonePayload->cbBufInfo.frame_index);
            }
            ALOGV("%s: frame_index=%u", __func__, rwDonePayload->cbBufInfo.frame_index);
        }

        rwDonePayload->size = rw_done_payload->buff.size;
        if (rw_done_payload->buff.ts != NULL) {
            rwDonePayload->timeStamp.tvSec =  rw_done_payload->buff.ts->tv_sec;
            rwDonePayload->timeStamp.tvNSec = rw_done_payload->buff.ts->tv_nsec;
        }
        if ((rw_done_payload->buff.buffer != NULL) &&
             !(sr_clbk_dat->session_attr.flags & PAL_STREAM_FLAG_EXTERN_MEM)) {
            rwDonePayload->buffer.resize(rwDonePayload->size);
            memcpy(rwDonePayload->buffer.data(), rw_done_payload->buff.buffer,
                   rwDonePayload->size);
        }

        ALOGV("fd [input %d - dup %d]", input_fd, rw_done_payload->buff.alloc_info.alloc_handle);
        if (!sr_clbk_dat->client_died) {
            if (!sr_clbk_dat->mDataMQ && !sr_clbk_dat->mCommandMQ) {
                if (sr_clbk_dat->prepare_mq_for_transfer(
                        (uint64_t)stream_handle, sr_clbk_dat->client_data_)) {
                    ALOGE("MQ prepare failed for stream %p", stream_handle);
                }
            }
            sr_clbk_dat->callReadWriteTransferThread((PalReadWriteDoneCommand) event_id,
                                        (uint8_t *)rwDonePayload,
                                        sizeof(PalCallbackBuffer));
        } else
            ALOGE("Client died dropping this event %d", event_id);

        if (fdToBeClosed != -1) {
            ALOGV("closing dup fd %d ", fdToBeClosed);
            close(fdToBeClosed);
        } else {
            ALOGE("Error finding fd %d", rw_done_payload->buff.alloc_info.alloc_handle);
        }
    } else {
        hidl_vec<uint8_t> PayloadHidl;
        PayloadHidl.resize(event_data_size);
        memcpy(PayloadHidl.data(), event_data, event_data_size);
        if (!sr_clbk_dat->client_died) {
            auto status = clbk_bdr->event_callback((uint64_t)stream_handle, event_id,
                              event_data_size, PayloadHidl,
                              sr_clbk_dat->client_data_);
            if (!status.isOk()) {
                 ALOGE("%s: HIDL call failed during event_callback", __func__);
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
   ALOGV("type [%d] flags [%0x] dir[%d] ", attr->type, attr->flags, attr->direction);
   print_stream_info(&attr->info);
   print_media_config(&attr->in_media_config);
   print_media_config(&attr->out_media_config);
}

bool PAL::isValidstreamHandle(const uint64_t streamHandle) {
    int pid = ::android::hardware::IPCThreadState::self()->getCallingPid();

    std::lock_guard<std::mutex> guard(mClientLock);
    for (auto itr = mPalClients.begin(); itr != mPalClients.end(); ) {
        auto client = *itr;
        if (client->pid == pid) {
            std::lock_guard<std::mutex> lock(client->mActiveSessionsLock);
            auto sItr = client->mActiveSessions.begin();
            for (; sItr != client->mActiveSessions.end(); sItr++) {
                if (sItr->session_handle == streamHandle) {
                    return true;
                }
            }
            ALOGE("%s: streamHandle: %pK for pid %d not found",
                    __func__, streamHandle, pid);
            return false;
        }
        itr++;
    }

    ALOGE("%s: client info for pid %d not found",
            __func__, pid);
    return false;
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
        ALOGE("Invalid hidl attributes ");
        return Void();
    }
    in_ch = attr_hidl.data()->in_media_config.ch_info.channels;
    out_ch = attr_hidl.data()->out_media_config.ch_info.channels;
    attr = (struct pal_stream_attributes *)calloc(1,
                                          sizeof(struct pal_stream_attributes));
    if (!attr) {
        ALOGE("Not enough memory for attributes");
        goto exit;
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

    if (noOfDevices > devs_hidl.size()) {
        ALOGE("Invalid number of devices.");
        goto exit;
    }

    if (devs_hidl.size()) {
        PalDevice *dev_hidl = NULL;
        devices = (struct pal_device *)calloc (1,
                                      sizeof(struct pal_device) * noOfDevices);
        if (!devices) {
            ALOGE("Not enough memory for devices");
            goto exit;
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

    if (modskv_hidl.size()) {
        modifiers = (struct modifier_kv *)calloc(1,
                                   sizeof(struct modifier_kv) * noOfModifiers);
        if (!modifiers) {
            ALOGE("Not enough memory for modifiers");
            goto exit;
        }
        memcpy(modifiers, modskv_hidl.data(),
               sizeof(struct modifier_kv) * noOfModifiers);
    }

    sr_clbk_data->setSessionAttr(attr);

    ret = pal_stream_open(attr, noOfDevices, devices, noOfModifiers, modifiers,
                          callback, (uint64_t)sr_clbk_data.get(), &stream_handle);

    if (!ret) {
        std::lock_guard<std::mutex> guard(mClientLock);
        for(auto& client: mPalClients) {
            if (client->pid == pid) {
                /*Another session from the same client*/
                ALOGI("Add session for existing client %d session %pK total sessions %d", pid,
                        (uint64_t)stream_handle, client->mActiveSessions.size());
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
            ALOGI("Add session from new client %d session %pK", pid, (uint64_t)stream_handle);
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
exit:
    if (modifiers)
        free(modifiers);
    if (devices)
        free(devices);
    if (attr)
        free(attr);
    return Void();
}

Return<int32_t> PAL::ipc_pal_stream_close(const uint64_t streamHandle)
{
    int pid = ::android::hardware::IPCThreadState::self()->getCallingPid();

    if (!isValidstreamHandle(streamHandle)) {
        ALOGE("%s: Invalid streamHandle: %pK", __func__, streamHandle);
        return -EINVAL;
    }

    mClientLock.lock();
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
    mClientLock.unlock();

    Return<int32_t> status = pal_stream_close((pal_stream_handle_t *)streamHandle);

    return status;
}

Return<int32_t> PAL::ipc_pal_stream_start(const uint64_t streamHandle) {
    if (!isValidstreamHandle(streamHandle)) {
        ALOGE("%s: Invalid streamHandle: %pK", __func__, streamHandle);
        return -EINVAL;
    }

    return pal_stream_start((pal_stream_handle_t *)streamHandle);
}

Return<int32_t> PAL::ipc_pal_stream_stop(const uint64_t streamHandle) {
    if (!isValidstreamHandle(streamHandle)) {
        ALOGE("%s: Invalid streamHandle: %pK", __func__, streamHandle);
        return -EINVAL;
    }

    return pal_stream_stop((pal_stream_handle_t *)streamHandle);
}

Return<int32_t> PAL::ipc_pal_stream_pause(const uint64_t streamHandle) {
    if (!isValidstreamHandle(streamHandle)) {
        ALOGE("%s: Invalid streamHandle: %pK", __func__, streamHandle);
        return -EINVAL;
    }

    return pal_stream_pause((pal_stream_handle_t *)streamHandle);
}

Return<int32_t> PAL::ipc_pal_stream_drain(uint64_t streamHandle, PalDrainType type)
{
    pal_drain_type_t drain_type = (pal_drain_type_t) type;

    if (!isValidstreamHandle(streamHandle)) {
        ALOGE("%s: Invalid streamHandle: %pK", __func__, streamHandle);
        return -EINVAL;
    }

    return pal_stream_drain((pal_stream_handle_t *)streamHandle,
                             drain_type);
}

Return<int32_t> PAL::ipc_pal_stream_flush(const uint64_t streamHandle) {
    if (!isValidstreamHandle(streamHandle)) {
        ALOGE("%s: Invalid streamHandle: %pK", __func__, streamHandle);
        return -EINVAL;
    }

    return pal_stream_flush((pal_stream_handle_t *)streamHandle);
}

Return<int32_t> PAL::ipc_pal_stream_suspend(const uint64_t streamHandle) {
    if (!isValidstreamHandle(streamHandle)) {
        ALOGE("%s: Invalid streamHandle: %pK", __func__, streamHandle);
        return -EINVAL;
    }

    return pal_stream_suspend((pal_stream_handle_t *)streamHandle);
}

Return<int32_t> PAL::ipc_pal_stream_resume(const uint64_t streamHandle) {
    if (!isValidstreamHandle(streamHandle)) {
        ALOGE("%s: Invalid streamHandle: %pK", __func__, streamHandle);
        return -EINVAL;
    }

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

    if (!isValidstreamHandle(streamHandle)) {
        ALOGE("%s: Invalid streamHandle: %pK", __func__, streamHandle);
        return Void();
    }

    in_buf_cfg.buf_count = in_buff_config.buf_count;
    in_buf_cfg.buf_size = in_buff_config.buf_size;
    if (in_buff_config.max_metadata_size) {
        in_buf_cfg.max_metadata_size = in_buff_config.max_metadata_size;
    } else {
        in_buf_cfg.max_metadata_size = MetadataParser::WRITE_METADATA_MAX_SIZE();
    }

    out_buf_cfg.buf_count = out_buff_config.buf_count;
    out_buf_cfg.buf_size = out_buff_config.buf_size;
    if (out_buff_config.max_metadata_size) {
        out_buf_cfg.max_metadata_size = out_buff_config.max_metadata_size;
    } else {
        out_buf_cfg.max_metadata_size = MetadataParser::READ_METADATA_MAX_SIZE();
    }

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
    struct pal_buffer buf = {0};

    if (!isValidstreamHandle(streamHandle)) {
        ALOGE("%s: Invalid streamHandle: %pK", __func__, streamHandle);
        return -EINVAL;
    }

    buf.size = buff_hidl.data()->size;
    std::vector<uint8_t> dataBuffer;
    if (buff_hidl.data()->buffer.size() == buf.size) {
        dataBuffer.resize(buf.size);
        buf.buffer = dataBuffer.data();
    }
    buf.offset = (size_t)buff_hidl.data()->offset;
    auto timeStamp = std::make_unique<timespec>();
    timeStamp->tv_sec =  buff_hidl.data()->timeStamp.tvSec;
    timeStamp->tv_nsec = buff_hidl.data()->timeStamp.tvNSec;
    buf.ts = timeStamp.get();
    buf.flags = buff_hidl.data()->flags;
    buf.frame_index = buff_hidl.data()->frame_index;

    buf.metadata_size = MetadataParser::WRITE_METADATA_MAX_SIZE();
    std::vector<uint8_t> bufMetadata(buf.metadata_size, 0);
    buf.metadata = bufMetadata.data();
    auto stream_media_config = std::make_shared<pal_media_config>();
    mClientLock.lock();
    for (auto& s: PAL::getInstance()->mPalClients) {
        std::lock_guard<std::mutex> lock(s->mActiveSessionsLock);
        for (auto session : s->mActiveSessions) {
            if (session.session_handle == streamHandle) {
                memcpy((uint8_t *)stream_media_config.get(),
                       (uint8_t *)&session.callback_binder->session_attr.out_media_config,
                       sizeof(pal_media_config));
            }
        }
    }
    mClientLock.unlock();
    auto metadataParser = std::make_unique<MetadataParser>();
    metadataParser->fillMetaData(buf.metadata, buf.frame_index, buf.size,
                                 stream_media_config.get());
    const native_handle *allochandle = buff_hidl.data()->alloc_info.alloc_handle.handle();

    buf.alloc_info.alloc_handle = dup(allochandle->data[0]);
    add_input_and_dup_fd(streamHandle, allochandle->data[1], buf.alloc_info.alloc_handle);

    ALOGV("%s: fd[input%d - dup%d]", __func__, allochandle->data[1], buf.alloc_info.alloc_handle);
    buf.alloc_info.alloc_size = buff_hidl.data()->alloc_info.alloc_size;
    buf.alloc_info.offset = buff_hidl.data()->alloc_info.offset;

    if (buf.buffer)
        memcpy(buf.buffer, buff_hidl.data()->buffer.data(), buf.size);
    ALOGV("%s:%d sz %d, frame_index %u", __func__,__LINE__, buf.size, buf.frame_index);

    addToPendingInputs(buf.alloc_info.alloc_handle,
                       buf.alloc_info.offset, buf.frame_index);

    return pal_stream_write((pal_stream_handle_t *)streamHandle, &buf);
}

Return<void> PAL::ipc_pal_stream_read(const uint64_t streamHandle,
                                      const hidl_vec<PalBuffer>& inBuff_hidl,
                                      ipc_pal_stream_read_cb _hidl_cb) {

    struct pal_buffer buf = {0};
    hidl_vec<PalBuffer> outBuff_hidl;

    if (!isValidstreamHandle(streamHandle)) {
        ALOGE("%s: Invalid streamHandle: %pK", __func__, streamHandle);
        return Void();
    }

    buf.size = inBuff_hidl.data()->size;
    std::vector<uint8_t> dataBuffer(buf.size, 0);
    buf.buffer = dataBuffer.data();
    buf.metadata_size = MetadataParser::READ_METADATA_MAX_SIZE();

    const native_handle *allochandle = inBuff_hidl.data()->alloc_info.alloc_handle.handle();

    buf.alloc_info.alloc_handle = dup(allochandle->data[0]);
    add_input_and_dup_fd(streamHandle, allochandle->data[1], buf.alloc_info.alloc_handle);
    ALOGV("%s: fd[input%d - dup%d]", __func__, allochandle->data[1], buf.alloc_info.alloc_handle);

    buf.alloc_info.alloc_size = inBuff_hidl.data()->alloc_info.alloc_size;
    buf.alloc_info.offset = inBuff_hidl.data()->alloc_info.offset;

    int32_t ret = pal_stream_read((pal_stream_handle_t *)streamHandle, &buf);
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
    }
    _hidl_cb(ret, outBuff_hidl);
    return Void();
}

Return<int32_t> PAL::ipc_pal_stream_set_param(const uint64_t streamHandle, uint32_t paramId,
                                        const hidl_vec<PalParamPayload>& paramPayload)
{
    int32_t ret = 0;
    pal_param_payload *param_payload;

    if (!isValidstreamHandle(streamHandle)) {
        ALOGE("%s: Invalid streamHandle: %pK", __func__, streamHandle);
        return -EINVAL;
    }

    param_payload = (pal_param_payload *)calloc (1,
                                    sizeof(pal_param_payload) + paramPayload.data()->size);
    if (!param_payload) {
        ALOGE("Not enough memory for param_payload");
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

    if (!isValidstreamHandle(streamHandle)) {
        ALOGE("%s: Invalid streamHandle: %pK", __func__, streamHandle);
        return Void();
    }

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
    int32_t ret = -ENOMEM;

    if (!isValidstreamHandle(streamHandle)) {
        ALOGE("%s: Invalid streamHandle: %pK", __func__, streamHandle);
        return -EINVAL;
    }

    if (devs_hidl.size()) {
        PalDevice *dev_hidl = NULL;
        devices = (struct pal_device *)calloc (1,
                                    sizeof(struct pal_device) * noOfDevices);
        if (!devices) {
            ALOGE("Not enough memory for devices");
            goto exit;
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

    ret = pal_stream_set_device((pal_stream_handle_t *)streamHandle,
                                  noOfDevices, devices);
exit:
    if(devices)
        free(devices);
    return ret;
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
    int32_t ret = -ENOMEM;

    if (!isValidstreamHandle(streamHandle)) {
        ALOGE("%s: Invalid streamHandle: %pK", __func__, streamHandle);
        return -EINVAL;
    }

    volume = (struct pal_volume_data *) calloc(1,
                                        sizeof(struct pal_volume_data) +
                                        noOfVolPairs * sizeof(pal_channel_vol_kv));
    if (!volume) {
        ALOGE("Not enough memory for volume");
        goto exit;
    }
    memcpy(volume->volume_pair, vol.data()->volPair.data(),
            noOfVolPairs * sizeof(pal_channel_vol_kv));
    volume->no_of_volpair = noOfVolPairs;
    ret = pal_stream_set_volume((pal_stream_handle_t *)streamHandle, volume);
exit:
    if(volume)
        free(volume);
    return ret;
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
    if (!isValidstreamHandle(streamHandle)) {
        ALOGE("%s: Invalid streamHandle: %pK", __func__, streamHandle);
        return -EINVAL;
    }

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

    if (!isValidstreamHandle(streamHandle)) {
        ALOGE("%s: Invalid streamHandle: %pK", __func__, streamHandle);
        return Void();
    }

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
    if (!isValidstreamHandle(streamHandle)) {
        ALOGE("%s: Invalid streamHandle: %pK", __func__, streamHandle);
        return -EINVAL;
    }

    return pal_add_remove_effect((pal_stream_handle_t *)streamHandle,
                                   (pal_audio_effect_t) effect, enable);
}

Return<int32_t> PAL::ipc_pal_set_param(uint32_t paramId,
                                       const hidl_vec<uint8_t>& payload_hidl,
                                       uint32_t size)
{   uint32_t ret = -EINVAL;
    uint8_t *payLoad;
    uint32_t size_p;
    if (payload_hidl == NULL) {
        ALOGE("vector payload_hidl is null");
        return ret;
    }
    size_p = payload_hidl.size() * sizeof(uint8_t);
    if (size_p < size) {
        ALOGE("%s: , size of hidl data %d is less than the size of payload data %d",
                __func__, size_p, size);
        return ret;
    }
    payLoad = (uint8_t*) calloc (1, size);
    if (!payLoad) {
        ALOGE("Not enough memory for payLoad");
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
        ALOGE("Not enough memory for payLoad");
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

    if (!isValidstreamHandle(streamHandle)) {
        ALOGE("%s: Invalid streamHandle: %pK", __func__, streamHandle);
        return Void();
    }

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

    if (!isValidstreamHandle(streamHandle)) {
        ALOGE("%s: Invalid streamHandle: %pK", __func__, streamHandle);
        return Void();
    }

    mmap_position_hidl.resize(sizeof(struct pal_mmap_position));
    ret = pal_stream_get_mmap_position((pal_stream_handle_t *)streamHandle, &mmap_position);
    memcpy(mmap_position_hidl.data(), &mmap_position, sizeof(struct pal_mmap_position));
    _hidl_cb(ret, mmap_position_hidl);
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

    if (!isValidstreamHandle(streamHandle)) {
        ALOGE("%s: Invalid streamHandle: %pK", __func__, streamHandle);
        return Void();
    }

    if (size > 0) {
        payload = (uint8_t *)calloc(1, size);
        if (!payload) {
            ALOGE("Not enough memory for payload");
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
