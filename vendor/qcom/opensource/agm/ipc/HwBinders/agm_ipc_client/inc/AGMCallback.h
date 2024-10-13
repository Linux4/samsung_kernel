/*
 * Copyright (c) 2019, 2021 The Linux Foundation. All rights reserved.
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

#pragma once

#include <vendor/qti/hardware/AGMIPC/1.0/IAGMCallback.h>
#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>
#include <agm/agm_api.h>


using AgmMediaConfig = ::vendor::qti::hardware::AGMIPC::V1_0::AgmMediaConfig;
using AgmGroupMediaConfig = ::vendor::qti::hardware::AGMIPC::V1_0::AgmGroupMediaConfig;
using AgmSessionConfig = ::vendor::qti::hardware::AGMIPC::V1_0::AgmSessionConfig;
using AgmBufferConfig = ::vendor::qti::hardware::AGMIPC::V1_0::AgmBufferConfig;
using Direction = ::vendor::qti::hardware::AGMIPC::V1_0::Direction;
using AifInfo = ::vendor::qti::hardware::AGMIPC::V1_0::AifInfo;
using AgmTagConfig = ::vendor::qti::hardware::AGMIPC::V1_0::AgmTagConfig;
using AgmKeyValue = ::vendor::qti::hardware::AGMIPC::V1_0::AgmKeyValue;
using AgmEventRegCfg = ::vendor::qti::hardware::AGMIPC::V1_0::AgmEventRegCfg;
using AgmCalConfig = ::vendor::qti::hardware::AGMIPC::V1_0::AgmCalConfig;
using IAGMCallback = ::vendor::qti::hardware::AGMIPC::V1_0::IAGMCallback;
using AgmEventCbParams = ::vendor::qti::hardware::AGMIPC::V1_0::AgmEventCbParams;
using AgmReadWriteEventCbParams = ::vendor::qti::hardware::AGMIPC::V1_0::AgmReadWriteEventCbParams;
using AgmSessionMode = ::vendor::qti::hardware::AGMIPC::V1_0::AgmSessionMode;
using AgmGaplessSilenceType = ::vendor::qti::hardware::AGMIPC::V1_0::AgmGaplessSilenceType;
using AgmBuff = ::vendor::qti::hardware::AGMIPC::V1_0::AgmBuff;
using AgmEventReadWriteDonePayload = ::vendor::qti::hardware::AGMIPC::V1_0::AgmEventReadWriteDonePayload;
using android::hardware::hidl_handle;
using android::hardware::hidl_memory;

class server_death_notifier : public android::hardware::hidl_death_recipient
{
    public:
        server_death_notifier()
        {
             cb_ = NULL;
             cookie_ = 0;
        };
        server_death_notifier(agm_service_crash_cb cb, uint64_t cookie = 0)
        {
             cb_ = cb;
             cookie_ = cookie;
        };
        void serviceDied(uint64_t cookie,
         const android::wp<::android::hidl::base::V1_0::IBase>& who) override;
        void register_crash_cb(agm_service_crash_cb cb, uint64_t cookie)
        {
             cb_ = cb;
             cookie_ = cookie;
        };

    private:
        agm_service_crash_cb cb_;
        uint64_t cookie_;
};

class client_death_notifier : public android::hardware::hidl_death_recipient
{
    public:
        client_death_notifier(){}
        void serviceDied(uint64_t cookie,
         const android::wp<::android::hidl::base::V1_0::IBase>& who) override ;
};


class ClntClbk
{
    public :

    uint32_t session_id;
    agm_event_cb clbk_func;
    uint32_t event;
    void* clnt_data;

    ClntClbk()
    {
        session_id = 0;
        clbk_func = NULL;
        event = 0;
        clnt_data = NULL;
    }

    ClntClbk(uint32_t sess_id, agm_event_cb func, uint32_t evnt, void* cd)
    {
        session_id = sess_id;
        clbk_func = func;
        event = evnt;
        clnt_data = cd;
    }

    agm_event_cb get_clbk_func()
    {
        return clbk_func;
    }

    void* get_clnt_data()
    {
        return clnt_data;
    }

    uint32_t get_event()
    {
        return event;
    }
};

namespace vendor {
namespace qti {
namespace hardware {
namespace AGMIPC {
namespace V1_0 {
namespace implementation {

using ::android::hardware::hidl_array;
using ::android::hardware::hidl_memory;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::sp;

struct AGMCallback : public IAGMCallback {
    Return<int32_t> event_callback(uint32_t session_id,
                                const hidl_vec<AgmEventCbParams>& event_params,
                                uint64_t clbk_data) override;
    Return<int32_t> event_callback_rw_done(uint32_t session_id,
                                const hidl_vec<AgmReadWriteEventCbParams>& event_params,
                                uint64_t clbk_data) override;
};

}  // namespace implementation
}  // namespace V1_0
}  // namespace AGMIPC
}  // namespace hardware
}  // namespace qti
}  // namespace vendor
