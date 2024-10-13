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
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "pal_client_wrapper"
#include <vendor/qti/hardware/pal/1.0/IPAL.h>
#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>
#include <log/log.h>
#include "PalApi.h"
#include "inc/PalCallback.h"

using android::hardware::Return;
using android::hardware::hidl_vec;
using vendor::qti::hardware::pal::V1_0::IPAL;

using vendor::qti::hardware::pal::V1_0::implementation::PalCallback;
using android::sp;
using android::hardware::MessageQueue;
using android::hardware::EventFlag;
using android::Thread;
using android::status_t;

bool pal_server_died = false;
android::sp<IPAL> pal_client = NULL;
sp<server_death_notifier> Server_death_notifier = NULL;


std::mutex gLock;

class DataTransferThread : public Thread {
   public:
    DataTransferThread(std::atomic<bool>* stop, PalStreamHandle streamHandle,
                pal_stream_callback clbkObject,
                PalCallback::DataMQ* dataMQ,
                PalCallback::CommandMQ* commandMQ,
                EventFlag* efGroup,
                uint64_t cookie)
        : Thread(false),
          mStop(stop),
          mStreamHandle(streamHandle),
          mStreamCallback(clbkObject),
          mDataMQ(dataMQ),
          mCommandMQ(commandMQ),
          mEfGroup(efGroup),
          mBuffer(nullptr),
          mStreamCookie(cookie) {}
    bool init() {
        mBuffer.reset(new (std::nothrow) uint8_t[mDataMQ->getQuantumCount()]);
        return mBuffer != nullptr;
    }
    virtual ~DataTransferThread() {}

   private:
    std::atomic<bool>* mStop;
    PalStreamHandle mStreamHandle;
    pal_stream_callback mStreamCallback;
    PalCallback::DataMQ* mDataMQ;
    PalCallback::CommandMQ* mCommandMQ;
    EventFlag* mEfGroup;
    std::unique_ptr<uint8_t[]> mBuffer;
    uint64_t mStreamCookie;

    bool threadLoop() override;

    void startTransfer(int eventId);
};

void DataTransferThread::startTransfer(int eventId) {
    size_t availToRead = mDataMQ->availableToRead();

    if (mDataMQ->read(&mBuffer[0], availToRead)) {
        ALOGV("%s: calling client callback, data size %zu", __func__, availToRead);

        const ::vendor::qti::hardware::pal::V1_0::PalCallbackBuffer *rwDonePayload =
                (::vendor::qti::hardware::pal::V1_0::PalCallbackBuffer *)&mBuffer[0];
        auto cbBuffer = std::make_unique<pal_callback_buffer>();
        if (!cbBuffer) {
            ALOGE("%s: Failed to allocate memory for callback payload", __func__);
            return;
        }

        cbBuffer->size = rwDonePayload->size;
        std::vector<uint8_t> buffData = {};
        if (rwDonePayload->buffer.size() == cbBuffer->size) {
            buffData.resize(cbBuffer->size);
            memcpy(buffData.data(), rwDonePayload->buffer.data(), cbBuffer->size);
            cbBuffer->buffer = buffData.data();
        }

        auto bufTimeSpec = std::make_unique<timespec>();
        if (!bufTimeSpec) {
            ALOGE("%s: Failed to allocate memory for timespec", __func__);
            return;
        }
        bufTimeSpec->tv_sec = rwDonePayload->timeStamp.tvSec;
        bufTimeSpec->tv_nsec = rwDonePayload->timeStamp.tvNSec;
        cbBuffer->ts = (timespec *) bufTimeSpec.get();
        cbBuffer->status = rwDonePayload->status;
        cbBuffer->cb_buf_info.frame_index = rwDonePayload->cbBufInfo.frame_index;
        cbBuffer->cb_buf_info.sample_rate = rwDonePayload->cbBufInfo.sample_rate;
        cbBuffer->cb_buf_info.bit_width = rwDonePayload->cbBufInfo.bit_width;
        cbBuffer->cb_buf_info.channel_count = rwDonePayload->cbBufInfo.channel_count;
        mStreamCallback((pal_stream_handle_t *)mStreamHandle, eventId,
                        (uint32_t *)cbBuffer.get(), (uint32_t)availToRead,
                        mStreamCookie);
    }
}

bool DataTransferThread::threadLoop() {
    while (!std::atomic_load_explicit(mStop, std::memory_order_acquire)) {
        uint32_t efState = 0;
        mEfGroup->wait(static_cast<uint32_t>(PalMessageQueueFlagBits::NOT_EMPTY), &efState);
        if (!(efState & static_cast<uint32_t>(PalMessageQueueFlagBits::NOT_EMPTY))) {
            continue;  // Nothing to do.
        }
        PalReadWriteDoneCommand eventId;
        if (!mCommandMQ->read(&eventId)) {
            continue;
        }
        startTransfer((int)eventId);
        mEfGroup->wake(static_cast<uint32_t>(PalMessageQueueFlagBits::NOT_FULL));
    }

    return false;
}

void server_death_notifier::serviceDied(uint64_t cookie,
                   const android::wp<::android::hidl::base::V1_0::IBase>& who)
{
    ALOGE("%s : PAL Service died ,cookie : %lu",__func__,cookie);
    pal_server_died = true;
    // We exit the client process here, so that it also can restart
    // leading to a fresh start on both the sides.
    _exit(1);
}

android::sp<IPAL> get_pal_server() {
    std::lock_guard<std::mutex> guard(gLock);
    if (pal_client == NULL) {
        pal_client = IPAL::getService();
        if (pal_client == nullptr) {
            ALOGE("PAL service not initialized\n");
            goto exit;
        }
        if(Server_death_notifier == NULL) {
            Server_death_notifier = new server_death_notifier();
            pal_client->linkToDeath(Server_death_notifier, 0);
            ALOGE("palclient linked to death server death \n", __func__);
        }
    }
exit:
    return pal_client ;
}

int32_t pal_init(void)
{
   if (pal_client == NULL)
       pal_client = get_pal_server();
   return pal_client == NULL ?-1:0;
}

void pal_deinit(void)
{
   return;
}

PalCallback::~PalCallback() {
    mStopDataTransferThread.store(true, std::memory_order_release);
    if (mEfGroup) {
        mEfGroup->wake(static_cast<uint32_t>(PalMessageQueueFlagBits::NOT_EMPTY));
    }
    if (mDataTransferThread.get()) {
        status_t status = mDataTransferThread->join();
        ALOGE_IF(status, "write thread exit error: %s", strerror(-status));
    }
    if (mEfGroup) {
        status_t status = EventFlag::deleteEventFlag(&mEfGroup);
        ALOGE_IF(status, "write MQ event flag deletion error: %s", strerror(-status));
    }
}

Return<int32_t> PalCallback::event_callback(uint64_t strm_handle,
                                 uint32_t event_id,
                                 uint32_t event_data_size,
                                 const hidl_vec<uint8_t>& event_data,
                                 uint64_t cookie) {
    uint32_t *ev_data = NULL;
    int8_t *src = NULL;
    ev_data = (uint32_t *)calloc(1, event_data_size);
    if (!ev_data) {
        ALOGE("%s:%d Failed to allocate memory to event data", __func__, __LINE__);
        goto exit;
    }
    src = (int8_t *)event_data.data();
    memcpy(ev_data, src, event_data_size);

    ALOGV("event_payload_size %d", event_data_size);
    this->cb((pal_stream_handle_t *)strm_handle, event_id, ev_data, event_data_size,
                                    cookie);

exit:
    if(ev_data)
        free(ev_data);
    return int32_t {};
}

Return<void> PalCallback::event_callback_rw_done(uint64_t strm_handle,
                                 uint32_t event_id,
                                 uint32_t event_data_size,
                                 const hidl_vec<PalCallbackBuffer>& event_data,
                                 uint64_t cookie) {
    const PalCallbackBuffer *rwDonePayloadHidl = event_data.data();
    ALOGV("%s called \n", __func__);

    auto cbBuffer = std::make_unique<pal_callback_buffer>();
    if (!cbBuffer) {
        ALOGE("%s:%d Failed to allocate memory for callback payload", __func__, __LINE__);
        return Void();
    }

    cbBuffer->size = rwDonePayloadHidl->size;
    std::vector<uint8_t> buffData = {};
    if (rwDonePayloadHidl->buffer.size() == cbBuffer->size) {
        buffData.resize(cbBuffer->size);
        memcpy(buffData.data(), rwDonePayloadHidl->buffer.data(), cbBuffer->size);
        cbBuffer->buffer = buffData.data();
    }

    auto bufTimeSpec = std::make_unique<timespec>();
    if (!bufTimeSpec) {
        ALOGE("%s:%d Failed to allocate memory for timespec", __func__, __LINE__);
        return Void();
    }
    bufTimeSpec->tv_sec = rwDonePayloadHidl->timeStamp.tvSec;
    bufTimeSpec->tv_nsec = rwDonePayloadHidl->timeStamp.tvNSec;
    cbBuffer->ts = (timespec *) bufTimeSpec.get();
    cbBuffer->status = rwDonePayloadHidl->status;
    cbBuffer->cb_buf_info.frame_index = rwDonePayloadHidl->cbBufInfo.frame_index;
    cbBuffer->cb_buf_info.sample_rate = rwDonePayloadHidl->cbBufInfo.sample_rate;
    cbBuffer->cb_buf_info.bit_width = rwDonePayloadHidl->cbBufInfo.bit_width;
    cbBuffer->cb_buf_info.channel_count = rwDonePayloadHidl->cbBufInfo.channel_count;
    ALOGV("%s:%d Bufsize %d  ret bufSize %d", __func__, __LINE__,
                rwDonePayloadHidl->size, cbBuffer->size);
    ALOGV("event_payload_size %d", event_data_size);
    this->cb((pal_stream_handle_t *)strm_handle, event_id, (uint32_t *)cbBuffer.get(),
                                    event_data_size, cookie);

    return Void();
}

Return<void> PalCallback::prepare_mq_for_transfer(PalStreamHandle streamHandle,
                            uint64_t cookie, prepare_mq_for_transfer_cb _hidl_cb) {
    status_t status;

    // Wrap the _hidl_cb to return an error
    auto sendError = [&_hidl_cb](PalReadWriteDoneResult result) {
        _hidl_cb(result, DataMQ::Descriptor(), CommandMQ::Descriptor());
    };

    // Create message queues.
    if (mDataMQ) {
        ALOGE("the client attempts to call prepareForWriting twice");
        sendError(PalReadWriteDoneResult::INVALID_STATE);
        return Void();
    }

    std::unique_ptr<DataMQ> tempDataMQ(
            new DataMQ(sizeof(PalCallbackBuffer) * 2, true /* EventFlag */));

    std::unique_ptr<CommandMQ> tempCommandMQ(new CommandMQ(1));
    if (!tempDataMQ->isValid() || !tempCommandMQ->isValid()) {
        ALOGE_IF(!tempDataMQ->isValid(), "data MQ is invalid");
        ALOGE_IF(!tempCommandMQ->isValid(), "command MQ is invalid");
        sendError(PalReadWriteDoneResult::INVALID_ARGUMENTS);
        return Void();
    }
    EventFlag* tempRawEfGroup{};
    status = EventFlag::createEventFlag(tempDataMQ->getEventFlagWord(), &tempRawEfGroup);
    std::unique_ptr<EventFlag, void (*)(EventFlag*)> tempElfGroup(
        tempRawEfGroup, [](auto* ef) { EventFlag::deleteEventFlag(&ef); });
    if (status != android::OK || !tempElfGroup) {
        ALOGE("failed creating event flag for data MQ: %s", strerror(-status));
        sendError(PalReadWriteDoneResult::INVALID_ARGUMENTS);
        return Void();
    }

    // Create and launch the thread.
    auto tempDataTransferThread =
        sp<DataTransferThread>::make(&mStopDataTransferThread, streamHandle, cb, tempDataMQ.get(),
                                  tempCommandMQ.get(), tempElfGroup.get(), cookie);
    if (!tempDataTransferThread->init()) {
        ALOGW("failed to start writer thread: %s", strerror(-status));
        sendError(PalReadWriteDoneResult::INVALID_ARGUMENTS);
        return Void();
    }
    status = tempDataTransferThread->run("read_write_cb", android::PRIORITY_URGENT_AUDIO);
    if (status != android::OK) {
        ALOGW("failed to start read_write_cb thread: %s", strerror(-status));
        sendError(PalReadWriteDoneResult::INVALID_ARGUMENTS);
        return Void();
    }

    mDataMQ = std::move(tempDataMQ);
    mCommandMQ = std::move(tempCommandMQ);
    mDataTransferThread = tempDataTransferThread;
    mEfGroup = tempElfGroup.release();
    _hidl_cb(PalReadWriteDoneResult::OK, *mDataMQ->getDesc(), *mCommandMQ->getDesc());
    return Void();
}

int32_t pal_stream_open(struct pal_stream_attributes *attr,
                        uint32_t no_of_devices, struct pal_device *devices,
                        uint32_t no_of_modifiers, struct modifier_kv *modifiers,
                        pal_stream_callback cb, uint64_t cookie,
                        pal_stream_handle_t **stream_handle)
{
    int32_t ret = -EINVAL;
    if (!pal_server_died) {
        sp<IPALCallback> ClbkBinder = new PalCallback(cb);
        android::sp<IPAL> pal_client = get_pal_server();
        if (pal_client == nullptr)
            return ret;

        hidl_vec<PalStreamAttributes> attr_hidl;
        hidl_vec<PalDevice> devs_hidl;
        hidl_vec<ModifierKV> modskv_hidl;
        uint16_t in_channels = 0;
        uint16_t out_channels = 0;
        uint32_t dev_size = 0;
        int cnt = 0;
        uint8_t *temp = NULL;
        struct pal_stream_info info = attr->info.opt_stream_info;
        in_channels = attr->in_media_config.ch_info.channels;
        out_channels = attr->out_media_config.ch_info.channels;

        ALOGV("ver [%ld] sz [%ld] dur[%ld] has_video [%d] is_streaming [%d] lpbk_type [%d]",
            info.version, info.size, info.duration_us, info.has_video, info.is_streaming,
            info.loopback_type);

        attr_hidl.resize(sizeof(::vendor::qti::hardware::pal::V1_0::PalStreamAttributes) + in_channels +
                         out_channels);
        attr_hidl.data()->type = (PalStreamType)attr->type;
        attr_hidl.data()->info.version = info.version;
        attr_hidl.data()->info.size = info.size;
        attr_hidl.data()->info.duration_us = info.duration_us;
        attr_hidl.data()->info.has_video = info.has_video;
        attr_hidl.data()->info.is_streaming = info.is_streaming;
        attr_hidl.data()->flags = (PalStreamFlag)attr->flags;
        attr_hidl.data()->direction = (PalStreamDirection)attr->direction;
        attr_hidl.data()->in_media_config.sample_rate = attr->in_media_config.sample_rate;
        attr_hidl.data()->in_media_config.bit_width = attr->in_media_config.bit_width;

        if (in_channels) {
            attr_hidl.data()->in_media_config.ch_info.channels = attr->in_media_config.ch_info.channels;
            attr_hidl.data()->in_media_config.ch_info.ch_map = attr->in_media_config.ch_info.ch_map;
        }
        attr_hidl.data()->in_media_config.aud_fmt_id = (PalAudioFmt)attr->in_media_config.aud_fmt_id;

        ALOGD("%s:%d channels[in %d : out %d] format[in %d : out %d] flags %d",__func__,__LINE__,
             in_channels, out_channels, attr->in_media_config.aud_fmt_id,
             attr->out_media_config.aud_fmt_id, attr->flags);

        attr_hidl.data()->out_media_config.sample_rate = attr->out_media_config.sample_rate;
        attr_hidl.data()->out_media_config.bit_width = attr->out_media_config.bit_width;
        if (out_channels) {
            attr_hidl.data()->out_media_config.ch_info.channels = attr->out_media_config.ch_info.channels;
            attr_hidl.data()->out_media_config.ch_info.ch_map = attr->out_media_config.ch_info.ch_map;
        }
        attr_hidl.data()->out_media_config.aud_fmt_id = (PalAudioFmt)attr->out_media_config.aud_fmt_id;
        if (devices) {
            dev_size = no_of_devices * sizeof(struct pal_device);
            devs_hidl.resize(dev_size);
            PalDevice *dev_hidl = devs_hidl.data();
            for ( cnt = 0; cnt < no_of_devices; cnt++) {
                 dev_hidl->id =(PalDeviceId)devices[cnt].id;
                 dev_hidl->config.sample_rate = devices[cnt].config.sample_rate;
                 dev_hidl->config.bit_width = devices[cnt].config.bit_width;
                 dev_hidl->config.ch_info.channels = devices[cnt].config.ch_info.channels;
                 dev_hidl->config.ch_info.ch_map = devices[cnt].config.ch_info.ch_map;
                 dev_hidl->config.aud_fmt_id = (PalAudioFmt)devices[cnt].config.aud_fmt_id;
                 dev_hidl =  (PalDevice *)(devs_hidl.data() + sizeof(PalDevice));
            }
        }
        if (modifiers) {
            modskv_hidl.resize(sizeof(struct modifier_kv) * no_of_modifiers);
            memcpy(modskv_hidl.data(), modifiers, sizeof(struct modifier_kv) * no_of_modifiers);
        }
        pal_client->ipc_pal_stream_open(attr_hidl, no_of_devices, devs_hidl, no_of_modifiers,
                                        modskv_hidl, ClbkBinder, cookie,
                                        [&](int32_t ret_, PalStreamHandle streamHandleRet)
                                          {
                                               ret = ret_;
                                               *stream_handle = (uint64_t *)streamHandleRet;
                                          }
                                         );
    }
    return ret;
}



int32_t pal_stream_close(pal_stream_handle_t *stream_handle)
{
    if (!pal_server_died) {
        ALOGD("%s %d handle %pK", __func__, __LINE__, stream_handle);
        android::sp<IPAL> pal_client = get_pal_server();
        if (pal_client == nullptr)
            return -EINVAL;

        return pal_client->ipc_pal_stream_close((PalStreamHandle)stream_handle);
    }
    return -EINVAL;
}

int32_t pal_stream_start(pal_stream_handle_t *stream_handle)
{
    if (!pal_server_died) {
        ALOGD("%s %d handle %pK", __func__, __LINE__, stream_handle);
        android::sp<IPAL> pal_client = get_pal_server();
        if (pal_client == nullptr)
            return -EINVAL;

        return pal_client->ipc_pal_stream_start((PalStreamHandle)stream_handle);
    }
    return -EINVAL;
}

int32_t pal_stream_stop(pal_stream_handle_t *stream_handle)
{
    if (!pal_server_died) {
        android::sp<IPAL> pal_client = get_pal_server();
        if (pal_client == nullptr)
            return -EINVAL;

        ALOGD("%s %d handle %pK", __func__, __LINE__, stream_handle);
        return pal_client->ipc_pal_stream_stop((PalStreamHandle)stream_handle);
    }
    return -EINVAL;

}

int32_t pal_stream_pause(pal_stream_handle_t *stream_handle)
{
    if (!pal_server_died) {
        android::sp<IPAL> pal_client = get_pal_server();
        if (pal_client == nullptr)
            return -EINVAL;

        ALOGD("%s %d handle %pK", __func__, __LINE__, stream_handle);
        return pal_client->ipc_pal_stream_pause((PalStreamHandle)stream_handle);
    }
    return -EINVAL;

}

int32_t pal_stream_resume(pal_stream_handle_t *stream_handle)
{
    if (!pal_server_died) {
        ALOGD("%s %d handle %pK", __func__, __LINE__, stream_handle);
        android::sp<IPAL> pal_client = get_pal_server();
        if (pal_client == nullptr)
            return -EINVAL;

        return pal_client->ipc_pal_stream_resume((PalStreamHandle)stream_handle);
    }
    return -EINVAL;

}

int32_t pal_stream_flush(pal_stream_handle_t *stream_handle)
{
    if (!pal_server_died) {
        ALOGD("%s %d handle %pK", __func__, __LINE__, stream_handle);
        android::sp<IPAL> pal_client = get_pal_server();
        if (pal_client == nullptr)
            return -EINVAL;

        return pal_client->ipc_pal_stream_flush((PalStreamHandle)stream_handle);
    }
    return -EINVAL;
}

int32_t pal_stream_drain(pal_stream_handle_t *stream_handle, pal_drain_type_t drain)
{
    if (!pal_server_died) {
        ALOGD("%s %d handle %pK", __func__, __LINE__, stream_handle);
        android::sp<IPAL> pal_client = get_pal_server();
        if (pal_client == nullptr)
            return -EINVAL;

        return pal_client->ipc_pal_stream_drain((PalStreamHandle)stream_handle, (PalDrainType) drain);
    }
    return -EINVAL;
}

int32_t pal_stream_suspend(pal_stream_handle_t *stream_handle)
{
    if (!pal_server_died) {
        ALOGD("%s %d handle %pK", __func__, __LINE__, stream_handle);
        android::sp<IPAL> pal_client = get_pal_server();
        if (pal_client == nullptr)
            return -EINVAL;

        return pal_client->ipc_pal_stream_suspend((PalStreamHandle)stream_handle);
    }
    return -EINVAL;
}

int32_t pal_stream_set_buffer_size(pal_stream_handle_t *stream_handle,
                                    pal_buffer_config_t *in_buff_cfg,
                                    pal_buffer_config_t *out_buff_cfg)
{
    int32_t ret = -EINVAL;
    if (!pal_server_died) {
        android::sp<IPAL> pal_client = get_pal_server();
        if (pal_client == nullptr)
            return ret;

        PalBufferConfig out_buffer_cfg, in_buffer_cfg;

        if (in_buff_cfg) {
         ALOGV("%s:%d input incnt %d buf_sz %d max_metadata_size %d", __func__,__LINE__,
               in_buff_cfg->buf_count, in_buff_cfg->buf_size, in_buff_cfg->max_metadata_size);
               in_buffer_cfg.buf_count = in_buff_cfg->buf_count;
               in_buffer_cfg.buf_size = in_buff_cfg->buf_size;
               in_buffer_cfg.max_metadata_size = in_buff_cfg->max_metadata_size;
        }
        if (out_buff_cfg) {
         ALOGV("%s:%d output incnt %d buf_sz %d max_metadata_size %d", __func__,__LINE__,
               out_buff_cfg->buf_count, out_buff_cfg->buf_size, out_buff_cfg->max_metadata_size);
               out_buffer_cfg.buf_count = out_buff_cfg->buf_count;
               out_buffer_cfg.buf_size = out_buff_cfg->buf_size;
               out_buffer_cfg.max_metadata_size = out_buff_cfg->max_metadata_size;
        }

        pal_client->ipc_pal_stream_set_buffer_size((PalStreamHandle)stream_handle, in_buffer_cfg, out_buffer_cfg,
                       [&](int32_t ret_, PalBufferConfig in_buff_cfg_ret, PalBufferConfig out_buff_cfg_ret)
                           {
                               if (!ret_) {
                                   if (in_buff_cfg) {
                                       in_buff_cfg->buf_count = in_buff_cfg_ret.buf_count;
                                       in_buff_cfg->buf_size = in_buff_cfg_ret.buf_size;
                                       in_buff_cfg->max_metadata_size =  in_buff_cfg_ret.max_metadata_size;
                                   }
                                   if (out_buff_cfg) {
                                       out_buff_cfg->buf_count = out_buff_cfg_ret.buf_count;
                                       out_buff_cfg->buf_size = out_buff_cfg_ret.buf_size;
                                       out_buff_cfg->max_metadata_size = out_buff_cfg_ret.max_metadata_size;
                                   }
                                }
                                ret = ret_;
                           });
    }
    return ret;
}

ssize_t pal_stream_write(pal_stream_handle_t *stream_handle, struct pal_buffer *buf)
{
    int ret = -EINVAL;

    if (!stream_handle)
        return ret;

    if (!pal_server_died) {
        ALOGV("%s:%d hndl %p",__func__, __LINE__, stream_handle );
        android::sp<IPAL> pal_client = get_pal_server();
        if (!pal_client)
            return ret;

        hidl_vec<PalBuffer> buf_hidl;
        buf_hidl.resize(sizeof(struct pal_buffer));
        PalBuffer *palBuff = buf_hidl.data();
        native_handle_t *allocHidlHandle = nullptr;
        allocHidlHandle = native_handle_create(1, 1);
        if (!allocHidlHandle) {
            ALOGE("%s:%d Failed to create allocHidlHandle", __func__, __LINE__);
            return ret;
        }
        allocHidlHandle->data[0] = buf->alloc_info.alloc_handle;
        allocHidlHandle->data[1] = buf->alloc_info.alloc_handle;

        palBuff->size = buf->size;
        palBuff->offset = buf->offset;
        palBuff->buffer.resize(buf->size);
        palBuff->flags = buf->flags;
        palBuff->frame_index = buf->frame_index;
        if (buf->ts) {
             palBuff->timeStamp.tvSec = buf->ts->tv_sec;
             palBuff->timeStamp.tvNSec = buf->ts->tv_nsec;
        }
        if (buf->size && buf->buffer)
            memcpy(palBuff->buffer.data(), buf->buffer, buf->size);
         palBuff->alloc_info.alloc_handle =
                 hidl_memory("arpal_alloc_handle", hidl_handle(allocHidlHandle),
                              buf->alloc_info.alloc_size);
         ALOGV("%s:%d alloc handle %d sending %d",__func__,__LINE__,
                     buf->alloc_info.alloc_handle, allocHidlHandle->data[0]);
         palBuff->alloc_info.alloc_size = buf->alloc_info.alloc_size;
         palBuff->alloc_info.offset = buf->alloc_info.offset;
         ret = pal_client->ipc_pal_stream_write((PalStreamHandle)stream_handle, buf_hidl);
         if (allocHidlHandle)
             native_handle_delete(allocHidlHandle);
    }
    return ret;
}

ssize_t pal_stream_read(pal_stream_handle_t *stream_handle, struct pal_buffer *buf)
{
    int ret = -EINVAL;

    if (!stream_handle)
        return ret;

    if (!pal_server_died) {
        android::sp<IPAL> pal_client = get_pal_server();
        if (!pal_client)
            return ret;

        hidl_vec<PalBuffer> buf_hidl;
        buf_hidl.resize(sizeof(struct pal_buffer));
        PalBuffer *palBuff = buf_hidl.data();
        native_handle_t *allocHidlHandle = nullptr;
        allocHidlHandle = native_handle_create(1, 1);
        if (!allocHidlHandle) {
            ALOGE("%s:%d Failed to create allocHidlHandle", __func__, __LINE__);
            return ret;
        }
        allocHidlHandle->data[0] = buf->alloc_info.alloc_handle;
        allocHidlHandle->data[1] = buf->alloc_info.alloc_handle;

        palBuff->size = buf->size;
        palBuff->offset = buf->offset;
        palBuff->alloc_info.alloc_handle = hidl_memory("arpal_alloc_handle", hidl_handle(allocHidlHandle),
                                                         buf->alloc_info.alloc_size);
        palBuff->alloc_info.alloc_size = buf->alloc_info.alloc_size;
        palBuff->alloc_info.offset = buf->alloc_info.offset;

        ALOGV("%s:%d size %d %d",__func__,__LINE__,buf_hidl.data()->size, buf->size);
        ALOGV("%s:%d alloc handle %d sending %d",__func__,__LINE__,
                   buf->alloc_info.alloc_handle, allocHidlHandle->data[0]);
        pal_client->ipc_pal_stream_read((PalStreamHandle)stream_handle, buf_hidl,
               [&](int32_t ret_, hidl_vec<PalBuffer> ret_buf_hidl)
                  {
                      if (ret_ > 0) {
                          if (ret_buf_hidl.data()->size > buf->size) {
                              ALOGE("ret buf sz %d bigger than request buf sz %d",
                                     ret_buf_hidl.data()->size, buf->size);
                              ret_ = -ENOMEM;
                           } else {
                              if (buf->ts) {
                                  buf->ts->tv_sec =
                                       ret_buf_hidl.data()->timeStamp.tvSec;
                                  buf->ts->tv_nsec =
                                       ret_buf_hidl.data()->timeStamp.tvNSec;
                              }
                              buf->flags = ret_buf_hidl.data()->flags;

                              if (buf->buffer)
                                   memcpy(buf->buffer,
                                          ret_buf_hidl.data()->buffer.data(),
                                          buf->size);
                           }
                      }
                      ret = ret_;
                  });

        if (allocHidlHandle)
            native_handle_delete(allocHidlHandle);
    }
    return ret;
}

int32_t pal_stream_set_param(pal_stream_handle_t *stream_handle,
                             uint32_t param_id,
                             pal_param_payload *param_payload)
{   int32_t ret = -EINVAL;

    ALOGV("%s:%d:", __func__, __LINE__);
    if (stream_handle == NULL || !param_payload)
       goto done;
    if (!pal_server_died) {
        android::sp<IPAL> pal_client = get_pal_server();
        if (pal_client == nullptr)
            return ret;

        hidl_vec<PalParamPayload> paramPayload;
        paramPayload.resize(sizeof(PalParamPayload));
        paramPayload.data()->payload.resize(param_payload->payload_size);
        paramPayload.data()->size = param_payload->payload_size;
        memcpy(paramPayload.data()->payload.data(), param_payload->payload,
                param_payload->payload_size);
        ret = pal_client->ipc_pal_stream_set_param((PalStreamHandle)stream_handle, param_id,
                                        paramPayload);
    }
done:
    return ret;
}

int32_t pal_stream_get_param(pal_stream_handle_t *stream_handle,
                             uint32_t param_id,
                             pal_param_payload **param_payload)
{
    int32_t ret = -EINVAL;
    ALOGV("%s:%d:", __func__, __LINE__);
    if (stream_handle == NULL || !(*param_payload))
       goto done;

    if (!pal_server_died) {
        android::sp<IPAL> pal_client = get_pal_server();
        if (pal_client == nullptr)
            return ret;


        pal_client->ipc_pal_stream_get_param((PalStreamHandle)stream_handle, param_id,
                  [&](int32_t ret_, hidl_vec<PalParamPayload> paramPayload) 
                  {
                     if (!ret_) {
                         *param_payload = (pal_param_payload *)
                                 calloc (1, sizeof(pal_param_payload) + paramPayload.data()->size);
                         if (!(*param_payload)) {
                             ALOGE("%s:%d Failed to allocate memory for (*param_payload)",
                                     __func__, __LINE__);
                             ret_ = -ENOMEM;
                         } else {
                             (*param_payload)->payload_size = paramPayload.data()->size;
                             memcpy((*param_payload)->payload, paramPayload.data()->payload.data(),
                                     (*param_payload)->payload_size);
                         }
                     }
                     ret = ret_;
                  });
     }
done:
    return ret;
}

int32_t pal_stream_get_device(pal_stream_handle_t *stream_handle,
                              uint32_t no_of_devices,
                              struct pal_device *devices)
{
    ALOGD("%s:%d:", __func__, __LINE__);
    return -EINVAL;
}

int32_t pal_stream_set_device(pal_stream_handle_t *stream_handle,
                              uint32_t no_of_devices,
                              struct pal_device *devices)
{
    hidl_vec<PalDevice> devs_hidl;
    int32_t cnt = 0;
    uint32_t dev_size = 0;
    int32_t ret = -EINVAL;

    if (!pal_server_died) {
        ALOGD("%s:%d:", __func__, __LINE__);
        android::sp<IPAL> pal_client = get_pal_server();
        if (pal_client == nullptr)
            return ret;


        if (devices) {
           dev_size = no_of_devices * sizeof(struct pal_device);
           ALOGD("dev_size %d", dev_size);
           devs_hidl.resize(dev_size);
           PalDevice *dev_hidl = devs_hidl.data();
           for (cnt = 0; cnt < no_of_devices; cnt++) {
                dev_hidl->id =(PalDeviceId)devices[cnt].id;
                dev_hidl->config.sample_rate = devices[cnt].config.sample_rate;
                dev_hidl->config.bit_width = devices[cnt].config.bit_width;
                dev_hidl->config.ch_info.channels = devices[cnt].config.ch_info.channels;
                dev_hidl->config.ch_info.ch_map = devices[cnt].config.ch_info.ch_map;
                dev_hidl->config.aud_fmt_id = (PalAudioFmt)devices[cnt].config.aud_fmt_id;
                dev_hidl =  (PalDevice *)(devs_hidl.data() + sizeof(PalDevice));
           }
           ret = pal_client->ipc_pal_stream_set_device((PalStreamHandle)stream_handle,
                                                       no_of_devices, devs_hidl);
       }
    }
    return ret;
}

int32_t pal_stream_get_volume(pal_stream_handle_t *stream_handle,
                              struct pal_volume_data *volume __unused)
{

    ALOGD("%s:%d:", __func__, __LINE__);
    return -EINVAL;
}

int32_t pal_stream_set_volume(pal_stream_handle_t *stream_handle,
                              struct pal_volume_data *volume)
{
    hidl_vec<PalVolumeData> vol;
    int32_t ret = -EINVAL;
    if (volume == NULL) {
       ALOGE("Invalid volume");
       goto done;
    }
    if (!pal_server_died) {
        ALOGD("%s:%d:", __func__, __LINE__);
        android::sp<IPAL> pal_client = get_pal_server();
        if (pal_client == nullptr)
            return ret;

        uint32_t noOfVolPair = volume->no_of_volpair;
        uint32_t volSize = sizeof(PalVolumeData);
        vol.resize(volSize);
        vol.data()->volPair.resize(sizeof(PalChannelVolKv) * noOfVolPair);
        vol.data()->noOfVolPairs = noOfVolPair;
        memcpy(vol.data()->volPair.data(), volume->volume_pair,
               sizeof(PalChannelVolKv) * noOfVolPair);

        ret = pal_client->ipc_pal_stream_set_volume((PalStreamHandle)stream_handle,
                                                     vol);
    }
done:
    return ret;
}

int32_t pal_stream_get_mute(pal_stream_handle_t *stream_handle, bool *state)
{
    ALOGD("%s:%d:", __func__, __LINE__);
    return -EINVAL;
}

int32_t pal_stream_set_mute(pal_stream_handle_t *stream_handle, bool state)
{
    int32_t ret = -EINVAL;

    if (!pal_server_died) {
        android::sp<IPAL> pal_client = get_pal_server();
        if (pal_client == nullptr)
            return ret;


        ALOGD("%s:%d:", __func__, __LINE__);
        ret = pal_client->ipc_pal_stream_set_mute((PalStreamHandle)stream_handle,
                                                   state);
    }
   return ret;
}

int32_t pal_get_mic_mute(bool *state)
{
    ALOGD("%s:%d:", __func__, __LINE__);
    return -EINVAL;
}

int32_t pal_set_mic_mute(bool state)
{
    ALOGD("%s:%d:", __func__, __LINE__);
    return -EINVAL;
}

int32_t pal_get_timestamp(pal_stream_handle_t *stream_handle,
                          struct pal_session_time *stime)
{
    int32_t ret = -EINVAL;
    if (!pal_server_died) {
        android::sp<IPAL> pal_client = get_pal_server();
        if (pal_client == nullptr)
            return ret;

        ALOGV("%s:%d:", __func__, __LINE__);
        pal_client->ipc_pal_get_timestamp((PalStreamHandle)stream_handle,
                    [&](int32_t ret_, hidl_vec<PalSessionTime> sessTime_hidl)
                       {
                           if(!ret_) {
                              memcpy(stime,
                                     sessTime_hidl.data(),
                                   sizeof(struct pal_session_time));
                           }
                           ret = ret_;
                       });
    }
    return ret;
}

int32_t pal_add_remove_effect(pal_stream_handle_t *stream_handle,
                             pal_audio_effect_t effect, bool enable)
{
    int32_t ret = -EINVAL;
    if (!pal_server_died) {
        android::sp<IPAL> pal_client = get_pal_server();
        if (pal_client == nullptr)
            return ret;

        ALOGD("%s:%d:", __func__, __LINE__);
        ret = pal_client->ipc_pal_add_remove_effect((PalStreamHandle)stream_handle,
                                                    (PalAudioEffect) effect,
                                                     enable);
    }
    return ret;
}

int32_t pal_set_param(uint32_t param_id, void *param_payload,
                      size_t payload_size)
{
    int32_t ret = -EINVAL;
    if (!pal_server_died) {
        android::sp<IPAL> pal_client = get_pal_server();
        if (pal_client == nullptr)
            return ret;

        ALOGD("%s:%d:", __func__, __LINE__);
        hidl_vec<uint8_t> paramPayload;
        paramPayload.resize(payload_size);
        memcpy(paramPayload.data(), param_payload, payload_size);
        ret = pal_client->ipc_pal_set_param(param_id,
                                            paramPayload,
                                            payload_size);
    }
    return ret;
}

int32_t pal_get_param(uint32_t param_id, void **param_payload,
                      size_t *payload_size, void *query)
{
    int32_t ret = -EINVAL;

    if (!param_payload) {
        ALOGE("Invalid param_payload pointer");
        return ret;
    }
    if (!pal_server_died) {
        android::sp<IPAL> pal_client = get_pal_server();
        if (pal_client == nullptr)
            return ret;

        ALOGV("%s:%d:", __func__, __LINE__);
        pal_client->ipc_pal_get_param(param_id,
                 [&](int32_t ret_, hidl_vec<uint8_t>paramPayload,
                     uint32_t size)
                     {
                         if(!ret_) {
                             // TODO: fix the client expectation, memory should always be allocated by client.
                             if (*param_payload == NULL)
                                 *param_payload = calloc(1, size);
                             if (!(*param_payload)) {
                                 ALOGE("No valid memory for param_payload");
                                 ret_ = -ENOMEM;
                             } else {
                                 memcpy(*param_payload,
                                        paramPayload.data(),
                                        size);
                                 *payload_size = size;
                             }
                          }
                          ret = ret_;
                     });
    }
    return ret;
}

int32_t pal_stream_create_mmap_buffer(pal_stream_handle_t *stream_handle,
                              int32_t min_size_frames,
                              struct pal_mmap_buffer *info)
{
   int32_t ret = -EINVAL;
   if (!pal_server_died) {
       ALOGD("%s:%d:", __func__, __LINE__);
       android::sp<IPAL> pal_client = get_pal_server();
       if (pal_client == nullptr)
           return ret;
       pal_client->ipc_pal_stream_create_mmap_buffer((PalStreamHandle)stream_handle,
                         min_size_frames,
                          [&](int32_t ret_, hidl_vec<PalMmapBuffer> mMapBuffer_hidl)
                           {
                                 if (!ret_) {
                                     info->buffer = (void *)mMapBuffer_hidl.data()->buffer;
                                     info->fd = mMapBuffer_hidl.data()->fd;
                                     info->buffer_size_frames =
                                               mMapBuffer_hidl.data()->buffer_size_frames;
                                     info->burst_size_frames =
                                               mMapBuffer_hidl.data()->burst_size_frames;
                                     info->flags =
                                         (pal_mmap_buffer_flags_t)mMapBuffer_hidl.data()->flags;
                                 }
                                 ret = ret_;
                           });

   }
   return ret;
}

int32_t pal_stream_get_mmap_position(pal_stream_handle_t *stream_handle,
                              struct pal_mmap_position *position)
{
    int32_t ret = -EINVAL;
    if (!pal_server_died) {
        ALOGV("%s:%d:", __func__, __LINE__);
        android::sp<IPAL> pal_client = get_pal_server();
        if (pal_client == nullptr)
            return ret;
        pal_client->ipc_pal_stream_get_mmap_position((PalStreamHandle)stream_handle,
                             [&](int32_t ret_, hidl_vec<PalMmapPosition> position_hidl)
                              {
                                    if (!ret) {
                                        memcpy(position, position_hidl.data(),
                                               sizeof(struct pal_mmap_position));
                                     }
                                     ret = ret_;
                              });
    }
    return ret;
}

int32_t pal_register_global_callback(pal_global_callback cb, void *cookie)
{
    return 0;
}

int32_t pal_gef_rw_param(uint32_t param_id, void *param_payload,
                      size_t payload_size, pal_device_id_t pal_device_id,
                      pal_stream_type_t pal_stream_type, unsigned int dir)
{
    return 0;
}

int32_t pal_gef_rw_param_acdb(uint32_t param_id __unused, void *param_payload,
                              size_t payload_size __unused, pal_device_id_t pal_device_id,
                              pal_stream_type_t pal_stream_type, uint32_t sample_rate,
                              uint32_t instance_id, uint32_t dir, bool is_play)
{
    return 0;
}

int32_t pal_stream_get_tags_with_module_info(pal_stream_handle_t *stream_handle,
                                             size_t *size ,uint8_t *payload)
{
    int32_t ret = -EINVAL;
    if (!pal_server_died) {
        ALOGV("%s:%d: size %d", __func__, __LINE__, *size);
        android::sp<IPAL> pal_client = get_pal_server();
        if (pal_client == nullptr)
            return ret;
        pal_client->ipc_pal_stream_get_tags_with_module_info((PalStreamHandle)stream_handle,(uint32_t)*size,
                             [&](int32_t ret_, uint32_t size_ret, hidl_vec<uint8_t> payload_ret)
                              {
                                    if (!ret_) {
                                        if (payload && (*size > 0) && (*size <= size_ret)) {
                                            memcpy(payload, payload_ret.data(), *size);
                                        } else if (payload && (*size > size_ret)) {
                                            memcpy(payload, payload_ret.data(), size_ret);
                                        }
                                        *size = size_ret;
                                    }
                                    ALOGV("ret %d size_ret %d", ret_, size_ret);
                                    ret = ret_;
                              });
    }
    return ret;
}

//}
//}
//}
//}
//}
//}

