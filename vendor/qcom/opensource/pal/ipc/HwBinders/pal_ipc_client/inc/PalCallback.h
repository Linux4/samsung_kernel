/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
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

#pragma once

#include <vendor/qti/hardware/pal/1.0/IPALCallback.h>
#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>
#include <fmq/EventFlag.h>
#include <fmq/MessageQueue.h>
#include <utils/Thread.h>
#include "PalApi.h"


using PalStreamAttributes = ::vendor::qti::hardware::pal::V1_0::PalStreamAttributes;
using PalDevice = ::vendor::qti::hardware::pal::V1_0::PalDevice;
using ModifierKV = ::vendor::qti::hardware::pal::V1_0::ModifierKV;
using PalStreamType = ::vendor::qti::hardware::pal::V1_0::PalStreamType;
using PalStreamFlag = ::vendor::qti::hardware::pal::V1_0::PalStreamFlag;
using PalStreamDirection = ::vendor::qti::hardware::pal::V1_0::PalStreamDirection;
using PalAudioFmt = ::vendor::qti::hardware::pal::V1_0::PalAudioFmt;
using PalDeviceId = ::vendor::qti::hardware::pal::V1_0::PalDeviceId;
using PalDrainType = ::vendor::qti::hardware::pal::V1_0::PalDrainType;
using PalBuffer = ::vendor::qti::hardware::pal::V1_0::PalBuffer;
using PalBufferConfig = ::vendor::qti::hardware::pal::V1_0::PalBufferConfig;
using PalStreamHandle = ::vendor::qti::hardware::pal::V1_0::PalStreamHandle;
using PalChannelVolKv = ::vendor::qti::hardware::pal::V1_0::PalChannelVolKv;
using PalVolumeData = ::vendor::qti::hardware::pal::V1_0::PalVolumeData;
using PalTimeus = ::vendor::qti::hardware::pal::V1_0::PalTimeus;
using PalSessionTime = ::vendor::qti::hardware::pal::V1_0::PalSessionTime;
using PalAudioEffect = ::vendor::qti::hardware::pal::V1_0::PalAudioEffect;
using PalMmapBuffer = ::vendor::qti::hardware::pal::V1_0::PalMmapBuffer;
using PalMmapPosition = ::vendor::qti::hardware::pal::V1_0::PalMmapPosition;
using PalParamPayload = ::vendor::qti::hardware::pal::V1_0::PalParamPayload;
using PalEventReadWriteDonePayload =
                     ::vendor::qti::hardware::pal::V1_0::PalEventReadWriteDonePayload;
using PalMessageQueueFlagBits = ::vendor::qti::hardware::pal::V1_0::PalMessageQueueFlagBits;
using PalReadWriteDoneResult = ::vendor::qti::hardware::pal::V1_0::PalReadWriteDoneResult;
using PalReadWriteDoneCommand = ::vendor::qti::hardware::pal::V1_0::PalReadWriteDoneCommand;
using PalCallbackBuffer = ::vendor::qti::hardware::pal::V1_0::PalCallbackBuffer;
using IPALCallback = ::vendor::qti::hardware::pal::V1_0::IPALCallback;
using android::hardware::hidl_handle;
using android::hardware::hidl_memory;

class server_death_notifier : public android::hardware::hidl_death_recipient
{
    public:
        server_death_notifier(){}
        void serviceDied(uint64_t cookie,
         const android::wp<::android::hidl::base::V1_0::IBase>& who) override ;
};

class client_death_notifier : public android::hardware::hidl_death_recipient
{
    public:
        client_death_notifier(){}
        void serviceDied(uint64_t cookie,
         const android::wp<::android::hidl::base::V1_0::IBase>& who) override ;
};

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
using ::android::sp;
using ::android::hardware::MessageQueue;
using ::android::hardware::EventFlag;
using ::android::Thread;
using ::android::status_t;

struct PalCallback : public IPALCallback {
    typedef MessageQueue<uint8_t, kSynchronizedReadWrite> DataMQ;
    typedef MessageQueue<PalReadWriteDoneCommand, kSynchronizedReadWrite> CommandMQ;

    Return<int32_t> event_callback(uint64_t stream_handle,
                               uint32_t event_id, uint32_t event_data_size,
                               const hidl_vec<uint8_t>& event_data,
                               uint64_t cookie) override;
    Return<void> event_callback_rw_done(uint64_t stream_handle,
                               uint32_t event_id, uint32_t event_data_size,
                               const hidl_vec<PalCallbackBuffer>& event_data,
                               uint64_t cookie) override;
    Return<void> prepare_mq_for_transfer(uint64_t stream_handle,
                                    uint64_t cookie,
                                    prepare_mq_for_transfer_cb _hidl_cb) override;

    PalCallback(pal_stream_callback callBack)
    {
        cb = callBack;
    }
    ~PalCallback();

    protected:
       pal_stream_callback cb;
       std::unique_ptr<DataMQ> mDataMQ = nullptr;
       std::unique_ptr<CommandMQ> mCommandMQ = nullptr;
       EventFlag* mEfGroup = nullptr;
       std::atomic<bool> mStopDataTransferThread = false;
       sp<Thread> mDataTransferThread = nullptr;
       uint64_t mStreamCookie;
};

}  // namespace implementation
}  // namespace V1_0
}  // namespace pal
}  // namespace hardware
}  // namespace qti
}  // namespace vendor
