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
 *
 * Changes from Qualcomm Innovation Center are provided under the following license:
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
#ifndef ANDROID_SYSTEM_AGMIPC_V1_0_AGM_H
#define ANDROID_SYSTEM_AGMIPC_V1_0_AGM_H

#include <vendor/qti/hardware/AGMIPC/1.0/IAGM.h>
#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>
#include <vector>
#include <cutils/list.h>
#include <agm/agm_api.h>
#include "inc/AGMCallback.h"

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
using ::android::hardware::hidl_handle;
using ::android::sp;

class SrvrClbk
{
    public :

    uint32_t session_id;
    uint32_t event;
    uint64_t clnt_data;
    int pid;

    SrvrClbk()
    {
        session_id = 0;
        event = 0;
        clnt_data = 0;
        pid = 0;
    }
    SrvrClbk(uint32_t sess_id, uint32_t evnt, uint64_t cd, int p_id)
    {
        session_id = sess_id;
        event = evnt;
        clnt_data = cd;
        pid = p_id;
    }

    uint64_t get_clnt_data()
    {
        return clnt_data;
    }

    uint64_t get_session_id()
    {
        return session_id;
    }
};

typedef struct clbk_data {
   struct listnode list;
   uint64_t clbk_clt_data;
   SrvrClbk *srv_clt_data;
} clbk_data;

struct AGM : public IAGM {
    public :
    AGM() {
      agm_initialized = agm_init() == 0?true:false;
    }
    Return<int32_t> ipc_agm_init() override;
    Return<int32_t> ipc_agm_deinit() override;
    Return<int32_t> ipc_agm_dump(const hidl_vec<AgmDumpInfo>& dump_info) override;
    Return<int32_t> ipc_agm_aif_set_media_config(uint32_t aif_id,
                        const hidl_vec<AgmMediaConfig>& media_config) override;
    Return<int32_t> ipc_agm_aif_set_metadata(uint32_t aif_id,
                                   uint32_t size,
                                   const hidl_vec<uint8_t>& metadata) override;
    Return<int32_t> ipc_agm_session_set_metadata(uint32_t session_id,
                                   uint32_t size,
                                   const hidl_vec<uint8_t>& metadata) override;
    Return<int32_t> ipc_agm_session_aif_set_metadata(uint32_t session_id,
                                   uint32_t aif_id,
                                   uint32_t size,
                                   const hidl_vec<uint8_t>& metadata) override;
    Return<int32_t> ipc_agm_session_aif_connect(uint32_t session_id,
                                                uint32_t aif_id,
                                                bool state) override;
    Return<void> ipc_agm_session_aif_get_tag_module_info(uint32_t session_id,
                 uint32_t aif_id,
                 uint32_t size,
                 ipc_agm_session_aif_get_tag_module_info_cb _hidl_cb) override;
    Return<void> ipc_agm_session_get_params(uint32_t session_id,
                                uint32_t size,
                                const hidl_vec<uint8_t>& buff,
                                ipc_agm_session_get_params_cb _hidl_cb) override;
    Return<int32_t> ipc_agm_aif_set_params(uint32_t aif_id,
                                           const hidl_vec<uint8_t>& payload,
                                           uint32_t size) override;
    Return<int32_t> ipc_agm_session_aif_set_params(uint32_t session_id,
                                              uint32_t aif_id,
                                              const hidl_vec<uint8_t>& payload,
                                              uint32_t size) override;
    Return<int32_t> ipc_agm_session_aif_set_cal(uint32_t session_id,
                            uint32_t aif_id,
                            const hidl_vec<AgmCalConfig>& cal_config) override;
    Return<int32_t> ipc_agm_session_set_params(uint32_t session_id,
                                               const hidl_vec<uint8_t>& payload,
                                               uint32_t size) override;
    Return<int32_t> ipc_agm_set_params_with_tag(uint32_t session_id,
                            uint32_t aif_id,
                            const hidl_vec<AgmTagConfig>& tag_config) override;
    Return<int32_t> ipc_agm_set_params_with_tag_to_acdb(uint32_t session_id,
                            uint32_t aif_id,
                            const hidl_vec<uint8_t>& payload,
                            uint32_t size) override;
    Return<int32_t> ipc_agm_set_params_to_acdb_tunnel(
                            const hidl_vec<uint8_t>& payload,
                            uint32_t size) override;
    Return<int32_t> ipc_agm_session_register_for_events(uint32_t session_id,
                         const hidl_vec<AgmEventRegCfg>& evt_reg_cfg) override;
    Return<void> ipc_agm_session_open(uint32_t session_id,
                                    AgmSessionMode sess_mode,
                                    ipc_agm_session_open_cb _hidl_cb) override;
    Return<int32_t> ipc_agm_session_set_config(uint64_t hndl,
                       const hidl_vec<AgmSessionConfig>& session_config,
                       const hidl_vec<AgmMediaConfig>& media_config,
                       const hidl_vec<AgmBufferConfig>& buffer_config) override;
    Return<int32_t> ipc_agm_session_close(uint64_t hndl) override;
    Return<int32_t> ipc_agm_session_prepare(uint64_t hndl) override;
    Return<int32_t> ipc_agm_session_start(uint64_t hndl) override;
    Return<int32_t> ipc_agm_session_stop(uint64_t hndl) override;
    Return<int32_t> ipc_agm_session_pause(uint64_t hndl) override;
    Return<int32_t> ipc_agm_session_flush(uint64_t hndl) override;
    Return<int32_t> ipc_agm_sessionid_flush(uint32_t session_id) override;
    Return<int32_t> ipc_agm_session_resume(uint64_t hndl) override;
    Return<int32_t> ipc_agm_session_suspend(uint64_t hndl) override;
    Return<void> ipc_agm_session_read(uint64_t hndl, uint32_t count,
                                    ipc_agm_session_read_cb _hidl_cb) override;
    Return<void> ipc_agm_session_write(uint64_t hndl,
                                    const hidl_vec<uint8_t>& buff,
                                    uint32_t count,
                                    ipc_agm_session_write_cb _hidl_cb) override;
    Return<int32_t> ipc_agm_get_hw_processed_buff_cnt(uint64_t hndl,
                                                      Direction dir) override;
    Return<void> ipc_agm_get_aif_info_list(uint32_t num_aif_info,
                               ipc_agm_get_aif_info_list_cb _hidl_cb) override;
    Return<int32_t> ipc_agm_session_set_loopback(uint32_t capture_session_id,
                                                 uint32_t playback_session_id,
                                                 bool state) override;
    Return<int32_t> ipc_agm_session_set_ec_ref(uint32_t capture_session_id,
                                               uint32_t aif_id,
                                               bool state) override;
    Return<int32_t> ipc_agm_client_register_callback(const sp<IAGMCallback>& cb) override;
    Return<int32_t> ipc_agm_session_register_callback(uint32_t session_id,
                                                  uint32_t evt_type,
                                                  uint64_t client_data,
                                                  uint64_t clnt_data) override;
    Return<int32_t> ipc_agm_session_eos(uint64_t hndl) override;
    Return<void> ipc_agm_get_session_time(uint64_t hndl,
                                ipc_agm_get_session_time_cb _hidl_cb) override;
    Return<void> ipc_agm_get_buffer_timestamp(uint32_t session_id,
                                ipc_agm_get_buffer_timestamp_cb _hidl_cb) override;
    Return<void> ipc_agm_session_get_buf_info(uint32_t session_id, uint32_t flag,
                                ipc_agm_session_get_buf_info_cb _hidl_cb) override;
    Return<int32_t> ipc_agm_set_gapless_session_metadata(uint64_t hndl,
                                              AgmGaplessSilenceType type,
                                              uint32_t silence) override;
    Return<int32_t> ipc_agm_session_set_non_tunnel_mode_config(uint64_t hndl,
                       const hidl_vec<AgmSessionConfig>& session_config,
                       const hidl_vec<AgmMediaConfig>& in_media_config,
                       const hidl_vec<AgmMediaConfig>& out_media_config,
                       const hidl_vec<AgmBufferConfig>& in_buffer_config,
                       const hidl_vec<AgmBufferConfig>& out_buffer_config) override;
    Return<void> ipc_agm_session_write_with_metadata(uint64_t hndl, const hidl_vec<AgmBuff>& buff,
                                               uint64_t consumed_size,
                                               ipc_agm_session_write_with_metadata_cb) override;
    Return<void> ipc_agm_session_read_with_metadata(uint64_t hndl, const hidl_vec<AgmBuff>& buff,
                                               uint32_t captured_size,
                                               ipc_agm_session_read_with_metadata_cb) override;
    Return<int32_t> ipc_agm_aif_group_set_media_config(uint32_t group_id,
                        const hidl_vec<AgmGroupMediaConfig>& media_config) override;
    Return<void> ipc_agm_get_group_aif_info_list(uint32_t num_groups,
                               ipc_agm_get_aif_info_list_cb _hidl_cb) override;
    Return<int32_t> ipc_agm_session_write_datapath_params(uint32_t session_id,
                               const hidl_vec<AgmBuff>& buff) override;

    int is_agm_initialized() { return agm_initialized;}

private:
    sp<client_death_notifier> mDeathNotifier = NULL;
    bool agm_initialized;
};

}  // namespace implementation
}  // namespace V1_0
}  // namespace AGMIPC
}  // namespace hardware
}  // namespace qti
}  // namespace vendor


#endif  // ANDROID_SYSTEM_AGMIPC_V1_0_AGM_H
