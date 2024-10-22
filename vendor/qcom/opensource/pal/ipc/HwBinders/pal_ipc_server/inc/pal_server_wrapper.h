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

#ifndef ANDROID_SYSTEM_pal_V1_0_pal_H
#define ANDROID_SYSTEM_pal_V1_0_pal_H


#include <vendor/qti/hardware/pal/1.0/IPALCallback.h>
#include <vendor/qti/hardware/pal/1.0/IPAL.h>
#include <android/hidl/allocator/1.0/IAllocator.h>
#include <android/hidl/memory/1.0/IMemory.h>
#include <hidlmemory/mapping.h>
#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>
#include <fmq/EventFlag.h>
#include <fmq/MessageQueue.h>
#include <utils/Thread.h>
#include <utils/RefBase.h>
#include <mutex>
#include "PalApi.h"
#include<log/log.h>

using namespace android;

namespace vendor {
namespace qti {
namespace hardware {
namespace pal {
namespace V1_0 {
namespace implementation {

using ::android::hardware::hidl_array;
using ::android::hardware::hidl_memory;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using IPALCallback = ::vendor::qti::hardware::pal::V1_0::IPALCallback;
using ::android::sp;
using ::android::hardware::MessageQueue;
using ::android::hardware::EventFlag;
using ::android::Thread;
using ::android::status_t;

class PalClientDeathRecipient;


class SrvrClbk : public ::android::RefBase {
    public :
    typedef MessageQueue<uint8_t, kSynchronizedReadWrite> DataMQ;
    typedef MessageQueue<PalReadWriteDoneCommand, kSynchronizedReadWrite> CommandMQ;
    sp<IPALCallback> clbk_binder;
    uint64_t client_data_;
    struct pal_stream_attributes session_attr;
    int pid_;
    bool client_died;
    std::vector<std::pair<int, int>> sharedMemFdList;
    std::unique_ptr<DataMQ> mDataMQ = nullptr;
    std::unique_ptr<CommandMQ> mCommandMQ = nullptr;
    EventFlag* mEfGroup = nullptr;

    SrvrClbk()
    {
        clbk_binder = NULL;
        client_data_ = 0;
        pid_ = 0;
    }
    SrvrClbk(sp<IPALCallback> binder,
             uint64_t client_data, int pid)
    {
        clbk_binder = binder;
        client_data_ = client_data;
        pid_ = pid;
        client_died = false;
    }
    void setSessionAttr(struct pal_stream_attributes *attr)
    {
        memcpy(&session_attr, attr, sizeof(session_attr));
    }
    int32_t callReadWriteTransferThread(PalReadWriteDoneCommand cmd,
                            const uint8_t* data, size_t dataSize);
    int32_t prepare_mq_for_transfer(uint64_t streamHandle, uint64_t cookie);
    ~SrvrClbk()
    {
        ALOGV("%s:%d",__func__,__LINE__);
        if (mEfGroup) {
            EventFlag::deleteEventFlag(&mEfGroup);
        }
    }
};

typedef struct session_info {
    uint64_t session_handle;
    sp<SrvrClbk> callback_binder;
}session_info;

struct client_info {
    int pid;
    std::vector<session_info> mActiveSessions;
    std::mutex mActiveSessionsLock;
};

struct PAL : public IPAL /*, public android::hardware::hidl_death_recipient*/{
    public:
    std::mutex mClientLock;
    PAL()
    {
        sInstance = this;
    }
    ~PAL()
    {
        sInstance = nullptr;
    }
    // TODO: Implement Singleton and use smart pointers to access instance
    static PAL* getInstance()
    {
        return sInstance;
    }
    Return<void> ipc_pal_stream_open(const hidl_vec<PalStreamAttributes>& attributes,
                                    uint32_t noOfDevices,
                                    const hidl_vec<PalDevice>& devices,
                                    uint32_t noOfModifiers,
                                    const hidl_vec<ModifierKV>& modifiers,
                                    const sp<IPALCallback>& cb,
                                    uint64_t ipc_clt_data,
                                    ipc_pal_stream_open_cb _hidl_cb) override;
    Return<int32_t> ipc_pal_stream_close(const uint64_t streamHandle) override;
    Return<int32_t> ipc_pal_stream_start(const uint64_t streamHandle) override;
    Return<int32_t> ipc_pal_stream_stop(const uint64_t streamHandle) override;
    Return<int32_t> ipc_pal_stream_pause(const uint64_t streamHandle) override;
    Return<int32_t> ipc_pal_stream_suspend(const uint64_t streamHandle) override;
    Return<int32_t> ipc_pal_stream_resume(const uint64_t streamHandle) override;
    Return<int32_t> ipc_pal_stream_flush(const uint64_t streamHandle) override;
    Return<int32_t> ipc_pal_stream_drain(const uint64_t streamHandle,
                                   const PalDrainType type) override;
    Return<void> ipc_pal_stream_set_buffer_size(const uint64_t streamHandle,
                                    const PalBufferConfig& rx_buff_cfg,
                                    const PalBufferConfig& tx_buff_cfg,
                                    ipc_pal_stream_set_buffer_size_cb _hidl_cb) override;
    Return<void> ipc_pal_stream_get_buffer_size(const uint64_t streamHandle,
                                    uint32_t in_buf_size,
                                    uint32_t out_buf_size,
                                    ipc_pal_stream_get_buffer_size_cb _hidl_cb) override;
    Return<int32_t> ipc_pal_stream_write(const uint64_t streamHandle,
                                    const hidl_vec<PalBuffer>& buffer) override;
    Return<void> ipc_pal_stream_read(const uint64_t streamHandle,
                                     const hidl_vec<PalBuffer>& buffer,
                                    ipc_pal_stream_read_cb _hidl_cb) override;
    Return<int32_t> ipc_pal_stream_set_param(const uint64_t streamHandle, uint32_t param_id,
                      uint32_t payloadSize, const hidl_memory& paramPayload) override;
    Return<void> ipc_pal_stream_get_param(const uint64_t streamHandle, uint32_t param_id,
                                    ipc_pal_stream_get_param_cb _hidl_cb) override;
    Return<void> ipc_pal_stream_get_device(const uint64_t streamHandle,
                                    ipc_pal_stream_get_device_cb _hidl_cb) override;
    Return<int32_t> ipc_pal_stream_set_device(const uint64_t streamHandle,
                                    uint32_t noOfDevices,
                                    const hidl_vec<PalDevice> &devices) override;
    Return<void> ipc_pal_stream_get_volume(const uint64_t streamHandle,
                                    ipc_pal_stream_get_volume_cb _hidl_cb) override;
    Return<int32_t> ipc_pal_stream_set_volume(const uint64_t streamHandle,
                                    const hidl_vec<PalVolumeData> &vol) override;
    Return<void> ipc_pal_stream_get_mute(const uint64_t streamHandle,
                                    ipc_pal_stream_get_mute_cb _hidl_cb) override;
    Return<int32_t> ipc_pal_stream_set_mute(const uint64_t streamHandle,
                                    bool state) override;
    Return<void> ipc_pal_get_mic_mute(ipc_pal_get_mic_mute_cb _hidl_cb) override;
    Return<int32_t> ipc_pal_set_mic_mute(bool state) override;
    Return<void> ipc_pal_get_timestamp(const uint64_t streamHandle,
                                       ipc_pal_get_timestamp_cb _hidl_cb) override;
    Return<int32_t> ipc_pal_add_remove_effect(const uint64_t streamHandle,
                                              const PalAudioEffect effect,
                                               bool enable) override;
    Return<int32_t> ipc_pal_set_param(uint32_t paramId,
                                      const hidl_vec<uint8_t> &payload,
                                      uint32_t size) override;
    Return<void> ipc_pal_get_param(uint32_t paramId,
                                   ipc_pal_get_param_cb _hidl_cb) override;
    Return<void>ipc_pal_stream_create_mmap_buffer(PalStreamHandle streamHandle,
                              int32_t min_size_frames,
                              ipc_pal_stream_create_mmap_buffer_cb _hidl_cb) override;
    Return<void>ipc_pal_stream_get_mmap_position(PalStreamHandle streamHandle,
                               ipc_pal_stream_get_mmap_position_cb _hidl_cb) override;
    Return<int32_t>ipc_pal_register_global_callback(const sp<IPALCallback>& cb, uint64_t cookie);
    Return<void>ipc_pal_gef_rw_param(uint32_t paramId, const hidl_vec<uint8_t> &param_payload,
                              int32_t payload_size, PalDeviceId dev_id,
                              PalStreamType strm_type, uint8_t dir,
                              ipc_pal_gef_rw_param_cb _hidl_cb) override;
    Return<void>ipc_pal_stream_get_tags_with_module_info(const uint64_t streamHandle,
                                     uint32_t size,
                                     ipc_pal_stream_get_tags_with_module_info_cb _hidl_cb) override;
    sp<PalClientDeathRecipient> mDeathRecipient;
    std::vector<std::shared_ptr<client_info>> mPalClients;
private:
    static PAL* sInstance;
    int find_dup_fd_from_input_fd(const uint64_t streamHandle, int input_fd, int *dup_fd);
    void add_input_and_dup_fd(const uint64_t streamHandle, int input_fd, int dup_fd);
    bool isValidstreamHandle(const uint64_t streamHandle);
};

class PalClientDeathRecipient : public android::hardware::hidl_death_recipient
{
    public:
        PalClientDeathRecipient(const sp<PAL> PalInstance)
        : mPalInstance(PalInstance){}
        void serviceDied(uint64_t cookie,
         const android::wp<::android::hidl::base::V1_0::IBase>& who) override ;
    private:
       sp<PAL> mPalInstance;
       std::mutex mLock;
};

}
}
}
}
}
}
#endif
