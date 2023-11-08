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

#define LOG_TAG "PAL: StreamSoundTrigger"

#include "StreamSoundTrigger.h"

#include <chrono>
#include <unistd.h>

#include "Session.h"
#include "SessionAlsaPcm.h"
#include "ResourceManager.h"
#include "Device.h"
#include "kvh2xml.h"
#include "VoiceUIInterface.h"

// TODO: find another way to print debug logs by default
#define ST_DBG_LOGS
#ifdef ST_DBG_LOGS
#define PAL_DBG(LOG_TAG,...)  PAL_INFO(LOG_TAG,__VA_ARGS__)
#endif

#define ST_DEFERRED_STOP_DEALY_MS (1000)
#define ST_MODEL_TYPE_SHIFT       (16)
#define ST_MAX_FSTAGE_CONF_LEVEL  (100)

ST_DBG_DECLARE(static int lab_cnt = 0);

StreamSoundTrigger::StreamSoundTrigger(struct pal_stream_attributes *sattr,
                                       struct pal_device *dattr,
                                       uint32_t no_of_devices,
                                       struct modifier_kv *modifiers __unused,
                                       uint32_t no_of_modifiers __unused,
                                       std::shared_ptr<ResourceManager> rm) {

    class SoundTriggerUUID uuid;
    int32_t enable_concurrency_count = 0;
    int32_t disable_concurrency_count = 0;
    reader_ = nullptr;
    detection_state_ = ENGINE_IDLE;
    notification_state_ = ENGINE_IDLE;
    model_id_ = 0;
    sm_config_ = nullptr;
    rec_config_ = nullptr;
    recognition_mode_ = 0;
    paused_ = false;
    device_opened_ = false;
    pending_stop_ = false;
    currentState = STREAM_IDLE;
    capture_requested_ = false;
    hist_buf_duration_ = 0;
    pre_roll_duration_ = 0;
    conf_levels_intf_version_ = 0;
    st_conf_levels_ = nullptr;
    st_conf_levels_v2_ = nullptr;
    lab_fd_ = nullptr;
    rejection_notified_ = false;
    mutex_unlocked_after_cb_ = false;
    common_cp_update_disable_ = false;
    second_stage_processing_ = false;
    gsl_engine_model_ = nullptr;
    gsl_conf_levels_ = nullptr;
    gsl_engine_ = nullptr;
    vui_intf_ = nullptr;
    sm_cfg_ = nullptr;
    ec_rx_dev_ = nullptr;
    mDevices.clear();

    // Setting default volume to unity
    mVolumeData = (struct pal_volume_data *)malloc(sizeof(struct pal_volume_data)
                      +sizeof(struct pal_channel_vol_kv));
    if (mVolumeData == NULL) {
        PAL_ERR(LOG_TAG, "Failed to allocate memory for volume data");
        throw std::runtime_error("Failed to allocate memory for volume data");
    }
    mVolumeData->no_of_volpair = 1;
    mVolumeData->volume_pair[0].channel_mask = 0x03;
    mVolumeData->volume_pair[0].vol = 1.0f;

    PAL_DBG(LOG_TAG, "Enter");
    // TODO: handle modifiers later
    mNoOfModifiers = 0;
    mModifiers = (struct modifier_kv *) (nullptr);

    // get voice UI platform info
    vui_ptfm_info_ = VoiceUIPlatformInfo::GetInstance();
    if (!vui_ptfm_info_) {
        PAL_ERR(LOG_TAG, "Failed to get voice UI platform info");
        throw std::runtime_error("Failed to get voice UI platform info");
    }

    if (!dattr) {
        PAL_ERR(LOG_TAG,"Error:invalid device arguments");
        throw std::runtime_error("invalid device arguments");
    }

    mStreamAttr = (struct pal_stream_attributes *)calloc(1,
        sizeof(struct pal_stream_attributes));
    if (!mStreamAttr) {
        PAL_ERR(LOG_TAG, "stream attributes allocation failed");
        throw std::runtime_error("stream attributes allocation failed");
    }

    ar_mem_cpy(mStreamAttr, sizeof(pal_stream_attributes),
                     sattr, sizeof(pal_stream_attributes));

    PAL_VERBOSE(LOG_TAG, "Create new Devices with no_of_devices - %d",
                no_of_devices);

    /* assume only one input device */
    if (no_of_devices > 1) {
        std::string err;
        err = "incorrect number of devices expected 1, got " +
            std::to_string(no_of_devices);
        PAL_ERR(LOG_TAG, "%s", err.c_str());
        free(mStreamAttr);
        throw std::runtime_error(err);
    }

    // Create internal states
    st_idle_ = new StIdle(*this);
    st_loaded_ = new StLoaded(*this);
    st_active = new StActive(*this);
    st_detected_ = new StDetected(*this);
    st_buffering_ = new StBuffering(*this);
    st_ssr_ = new StSSR(*this);

    AddState(st_idle_);
    AddState(st_loaded_);
    AddState(st_active);
    AddState(st_detected_);
    AddState(st_buffering_);
    AddState(st_ssr_);

    // Set initial state
    if (rm->cardState == CARD_STATUS_OFFLINE) {
        cur_state_ = st_ssr_;
        prev_state_ = nullptr;
        state_for_restore_ = ST_STATE_IDLE;
    } else {
        cur_state_ = st_idle_;
        prev_state_ = nullptr;
        state_for_restore_ = ST_STATE_NONE;
    }

    rm->registerStream(this);

    // Print the concurrency feature flags supported
    PAL_INFO(LOG_TAG, "capture conc enable %d,voice conc enable %d,voip conc enable %d",
        vui_ptfm_info_->GetConcurrentCaptureEnable(), vui_ptfm_info_->GetConcurrentVoiceCallEnable(),
        vui_ptfm_info_->GetConcurrentVoipCallEnable());

    // check concurrency count from rm
    rm->GetSoundTriggerConcurrencyCount(PAL_STREAM_VOICE_UI, &enable_concurrency_count,
        &disable_concurrency_count);

    // check if lpi should be used
    use_lpi_ = rm->getLPIUsage();

    /*
     * When voice/voip/record is active and concurrency is not
     * supported, mark paused as true, so that start recognition
     * will be skipped and when voice/voip/record stops, stream
     * will be resumed.
     */
    if (disable_concurrency_count) {
        paused_ = true;
    }

    timer_thread_ = std::thread(TimerThread, std::ref(*this));
    timer_stop_waiting_ = false;
    exit_timer_thread_ = false;

    PAL_DBG(LOG_TAG, "Exit");
}

StreamSoundTrigger::~StreamSoundTrigger() {
    mStreamMutex.lock();
    {
        std::lock_guard<std::mutex> lck(timer_mutex_);
        exit_timer_thread_ = true;
        timer_stop_waiting_ = true;
        timer_wait_cond_.notify_one();
        timer_start_cond_.notify_one();
    }
    if (timer_thread_.joinable()) {
        PAL_DBG(LOG_TAG, "Join timer thread");
        timer_thread_.join();
    }

    st_states_.clear();
    engines_.clear();
    mStreamMutex.unlock();

    rm->deregisterStream(this);
    if (mStreamAttr)
        free(mStreamAttr);

    if (gsl_engine_model_)
        free(gsl_engine_model_);

    if (gsl_conf_levels_)
        free(gsl_conf_levels_);

    if (mVolumeData)
        free(mVolumeData);

    if (sm_config_) {
        free(sm_config_);
        sm_config_ = nullptr;
    }

    if (rec_config_) {
        free(rec_config_);
        rec_config_ = nullptr;
    }

    if (reader_) {
        delete reader_;
        reader_ = nullptr;
    }

    if (st_conf_levels_) {
        free(st_conf_levels_);
        st_conf_levels_ = nullptr;
    }
    if (st_conf_levels_v2_) {
        free(st_conf_levels_v2_);
        st_conf_levels_v2_ = nullptr;
    }

    if (st_idle_)
        delete st_idle_;
    if (st_loaded_)
        delete st_loaded_;
    if (st_active)
        delete st_active;
    if (st_detected_)
        delete st_detected_;
    if (st_buffering_)
        delete st_buffering_;
    if (st_ssr_)
        delete st_ssr_;

    mDevices.clear();
    PAL_DBG(LOG_TAG, "Exit");
}

int32_t StreamSoundTrigger::close() {
    int32_t status = 0;

    PAL_DBG(LOG_TAG, "Enter, stream direction %d", mStreamAttr->direction);

    std::lock_guard<std::mutex> lck(mStreamMutex);
    std::shared_ptr<StEventConfig> ev_cfg(new StUnloadEventConfig());
    status = cur_state_->ProcessEvent(ev_cfg);

    if (sm_config_) {
        free(sm_config_);
        sm_config_ = nullptr;
    }

    currentState = STREAM_IDLE;
    PAL_DBG(LOG_TAG, "Exit, status %d", status);
    return status;
}

int32_t StreamSoundTrigger::start() {
    int32_t status = 0;
    stream_state_t prev_state;

    /*
     * Guard with mActiveStreamMutex to avoid concurrent
     * RX stream getting released during EC enable
     */
    rm->lockActiveStream();
    PAL_DBG(LOG_TAG, "Enter, stream direction %d", mStreamAttr->direction);

    std::lock_guard<std::mutex> lck(mStreamMutex);
    // cache current state after mutex locked
    prev_state = currentState;
    currentState = STREAM_STARTED;

    rejection_notified_ = false;
    std::shared_ptr<StEventConfig> ev_cfg(
       new StStartRecognitionEventConfig(false));
    status = cur_state_->ProcessEvent(ev_cfg);
    // restore cached state if start fails
    if (status)
        currentState = prev_state;

    rm->unlockActiveStream();
    PAL_DBG(LOG_TAG, "Exit, status %d", status);
    return status;
}

int32_t StreamSoundTrigger::stop() {
    int32_t status = 0;

    /*
     * Guard with mActiveStreamMutex to avoid concurrent
     * RX stream getting released during EC disable
     */
    rm->lockActiveStream();
    PAL_DBG(LOG_TAG, "Enter, stream direction %d", mStreamAttr->direction);

    std::lock_guard<std::mutex> lck(mStreamMutex);
    currentState = STREAM_STOPPED;

    std::shared_ptr<StEventConfig> ev_cfg(
       new StStopRecognitionEventConfig(false));
    status = cur_state_->ProcessEvent(ev_cfg);

    rm->unlockActiveStream();
    PAL_DBG(LOG_TAG, "Exit, status %d", status);
    return status;
}

int32_t StreamSoundTrigger::read(struct pal_buffer* buf) {
    int32_t size = 0;
    uint32_t sleep_ms = 0;
    uint32_t offset = 0;

    PAL_VERBOSE(LOG_TAG, "Enter");

    if (!buf || !buf->buffer) {
        PAL_ERR(LOG_TAG, "Invalid buffer");
        return -EINVAL;
    }

    std::lock_guard<std::mutex> lck(mStreamMutex);
    if (vui_ptfm_info_->GetEnableDebugDumps() && !lab_fd_) {
        ST_DBG_FILE_OPEN_WR(lab_fd_, ST_DEBUG_DUMP_LOCATION,
            "lab_reading", "bin", lab_cnt);
        PAL_DBG(LOG_TAG, "lab data stored in: lab_reading_%d.bin",
            lab_cnt);
        lab_cnt++;
    }
    if (cur_state_ == st_buffering_ && !this->force_nlpi_vote) {
        rm->voteSleepMonitor(this, true, true);
        this->force_nlpi_vote = true;

        offset = vui_intf_->GetReadOffset();
        if (offset) {
            reader_->advanceReadOffset(offset);
            vui_intf_->SetReadOffset(0);
        }
    }

    std::shared_ptr<StEventConfig> ev_cfg(
        new StReadBufferEventConfig((void *)buf));
    size = cur_state_->ProcessEvent(ev_cfg);

    vui_intf_->ProcessLab(buf->buffer, size);

    /*
     * st stream read pcm data from ringbuffer with almost no
     * delay, sleep for some time after each read even if read
     * fails or no enough data in ring buffer
     */
    if (size <= 0 || reader_->getUnreadSize() < buf->size) {
        sleep_ms = (buf->size * BITS_PER_BYTE * MS_PER_SEC) /
            (sm_cfg_->GetSampleRate() * sm_cfg_->GetBitWidth() *
             sm_cfg_->GetOutChannels());
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
    }

    PAL_VERBOSE(LOG_TAG, "Exit, read size %d", size);

    return size;
}

int32_t StreamSoundTrigger::getParameters(uint32_t param_id, void **payload) {
    int32_t status = 0;
    int32_t ret = 0;
    struct pal_stream_attributes *sAttr = nullptr;
    pal_param_payload *pal_payload = nullptr;

    PAL_DBG(LOG_TAG, "Enter, get parameter %u", param_id);
    if (param_id == PAL_PARAM_ID_STREAM_ATTRIBUTES) {
        pal_payload = (pal_param_payload *)(*payload);
        if (pal_payload->payload_size != sizeof(struct pal_stream_attributes)) {
            PAL_ERR(LOG_TAG, "Invalid payload size %u", pal_payload->payload_size);
            return -EINVAL;
        }
        sAttr = (struct pal_stream_attributes *)(pal_payload->payload);
        status = getStreamAttributes(sAttr);
        if (status)
            PAL_ERR(LOG_TAG, "Failed to get stream attributes");
    } else if (param_id == PAL_PARAM_ID_WAKEUP_MODULE_VERSION) {
        std::vector<std::shared_ptr<VUIStreamConfig>> sm_cfg_list;

        vui_ptfm_info_->GetStreamConfigForVersionQuery(sm_cfg_list);
        if (sm_cfg_list.size() == 0) {
            PAL_ERR(LOG_TAG, "No sound model config supports version query");
            return -EINVAL;
        }

        sm_cfg_ = sm_cfg_list[0];
        if (!sm_cfg_) {
            PAL_ERR(LOG_TAG, "Failed to get sound model config");
            return -EINVAL;
        }

        if (!mDevices.size()) {
            std::shared_ptr<Device> dev = nullptr;

            // update best device
            pal_device_id_t dev_id = GetAvailCaptureDevice();
            PAL_DBG(LOG_TAG, "Select available caputre device %d", dev_id);

            dev = GetPalDevice(this, dev_id);
            if (!dev) {
                PAL_ERR(LOG_TAG, "Device creation is failed");
                return -EINVAL;
            }
            mDevices.push_back(dev);
            dev = nullptr;
        }

        if (mDevices.size() > 0 && !device_opened_) {
            status = mDevices[0]->open();
            if (0 != status) {
                PAL_ERR(LOG_TAG, "Device open failed, status %d", status);
                return status;
            }
            device_opened_ = true;
        }

        cap_prof_ = GetCurrentCaptureProfile();
        /*
         * Get the capture profile and module types to fill selectors
         * used in payload builder to retrieve stream and device PP GKVs
         */
        mDevPPSelector = cap_prof_->GetName();
        PAL_DBG(LOG_TAG, "Devicepp Selector: %s", mDevPPSelector.c_str());
        mStreamSelector = sm_cfg_->GetVUIModuleName();

        model_type_ = sm_cfg_->GetVUIModuleType();
        PAL_DBG(LOG_TAG, "Module Type:%d, Name: %s", model_type_, mStreamSelector.c_str());
        mInstanceID = rm->getStreamInstanceID(this);

        gsl_engine_ = SoundTriggerEngine::Create(this, ST_SM_ID_SVA_F_STAGE_GMM,
                                                 model_type_, sm_cfg_);
        if (!gsl_engine_) {
            PAL_ERR(LOG_TAG, "big_sm: gsl engine creation failed");
            status = -ENOMEM;
            goto release;
        }

        status = gsl_engine_->GetParameters(param_id, payload);
        if (status)
            PAL_ERR(LOG_TAG, "Failed to get parameters from engine %d", status);

release:
        rm->resetStreamInstanceID(this, mInstanceID);
        if (mDevices.size() > 0) {
            ret = mDevices[0]->close();
            device_opened_ = false;
            if (0 != ret) {
                PAL_ERR(LOG_TAG, "Device close failed, status %d", ret);
                status = ret;
            }
        }
    } else if (gsl_engine_) {
        status = gsl_engine_->GetParameters(param_id, payload);
        if (status)
            PAL_ERR(LOG_TAG, "Failed to get parameters from engine, status %d", status);
    } else {
        PAL_ERR(LOG_TAG, "No gsl engine present");
        status = -EINVAL;
    }

    PAL_DBG(LOG_TAG, "Exit status: %d", status);
    return status;
}

int32_t StreamSoundTrigger::setParameters(uint32_t param_id, void *payload) {
    int32_t status = 0;
    pal_param_payload *param_payload = (pal_param_payload *)payload;
    struct pal_st_recognition_config *new_rec_config = nullptr;

    if (param_id != PAL_PARAM_ID_STOP_BUFFERING && !param_payload) {
        PAL_ERR(LOG_TAG, "Invalid payload for param ID: %d", param_id);
        return -EINVAL;
    }

    PAL_DBG(LOG_TAG, "Enter, param id %d", param_id);

    std::lock_guard<std::mutex> lck(mStreamMutex);
    switch (param_id) {
        case PAL_PARAM_ID_LOAD_SOUND_MODEL: {
            std::shared_ptr<StEventConfig> ev_cfg(
                new StLoadEventConfig((void *)param_payload->payload));
            status = cur_state_->ProcessEvent(ev_cfg);
            if (!status)
                currentState = STREAM_OPENED;
            break;
        }
        case PAL_PARAM_ID_RECOGNITION_CONFIG: {
            new_rec_config =
                (struct pal_st_recognition_config *)param_payload->payload;
            std::shared_ptr<StEventConfig> ev_cfg(
                new StRecognitionCfgEventConfig((void *)new_rec_config));
            status = cur_state_->ProcessEvent(ev_cfg);
            break;
        }
        case PAL_PARAM_ID_STOP_BUFFERING: {
            std::shared_ptr<StEventConfig> ev_cfg(
                new StStopBufferingEventConfig());
            status = cur_state_->ProcessEvent(ev_cfg);

            if (vui_ptfm_info_->GetEnableDebugDumps()) {
                ST_DBG_FILE_CLOSE(lab_fd_);
                lab_fd_ = nullptr;
            }
            break;
        }
        default: {
            status = -EINVAL;
            PAL_ERR(LOG_TAG, "Unsupported param %u", param_id);
            break;
        }
    }

    PAL_DBG(LOG_TAG, "Exit, status %d", status);
    return status;
}

int32_t StreamSoundTrigger::HandleConcurrentStream(bool active) {
    int32_t status = 0;
    uint64_t transit_duration = 0;

    if (!active) {
        mStreamMutex.lock();
        transit_start_time_ = std::chrono::steady_clock::now();
        common_cp_update_disable_ = true;
    }

    PAL_DBG(LOG_TAG, "Enter");
    std::shared_ptr<StEventConfig> ev_cfg(
        new StConcurrentStreamEventConfig(active));
    status = cur_state_->ProcessEvent(ev_cfg);

    if (active) {
        transit_end_time_ = std::chrono::steady_clock::now();
        transit_duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                transit_end_time_ - transit_start_time_).count();
        common_cp_update_disable_ = false;
        if (use_lpi_) {
            PAL_INFO(LOG_TAG, "NLPI->LPI switch takes %llums",
                (long long)transit_duration);
        } else {
            PAL_INFO(LOG_TAG, "LPI->NLPI switch takes %llums",
                (long long)transit_duration);
        }
        mStreamMutex.unlock();
    }

    PAL_DBG(LOG_TAG, "Exit, status %d", status);

    return status;
}

int32_t StreamSoundTrigger::EnableLPI(bool is_enable) {
    std::lock_guard<std::mutex> lck(mStreamMutex);
    if (!rm->IsLPISupported(PAL_STREAM_VOICE_UI)) {
        PAL_DBG(LOG_TAG, "Ignore as LPI not supported");
    } else {
        use_lpi_ = is_enable;
    }

    return 0;
}

int32_t StreamSoundTrigger::setECRef(std::shared_ptr<Device> dev, bool is_enable) {
    int32_t status = 0;

    std::lock_guard<std::mutex> lck(mStreamMutex);
    if (use_lpi_) {
        PAL_DBG(LOG_TAG, "EC ref will be handled in LPI/NLPI switch");
        return status;
    }
    // check if SVA device is registered in rm to avoid unnecessary EC set
    if (is_enable) {
        if (mDevices.size() && rm->isDeviceActive_l(mDevices[0], this))
            status = setECRef_l(dev, is_enable);
        else
            status = -ENODEV;
    } else {
        status = setECRef_l(dev, is_enable);
    }

    return status;
}

int32_t StreamSoundTrigger::setECRef_l(std::shared_ptr<Device> dev, bool is_enable) {
    int32_t status = 0;
    std::shared_ptr<StEventConfig> ev_cfg(
        new StECRefEventConfig(dev, is_enable));

    PAL_DBG(LOG_TAG, "Enter, enable %d", is_enable);

    if (!cap_prof_ || !cap_prof_->isECRequired()) {
        PAL_DBG(LOG_TAG, "No need to set ec ref");
        goto exit;
    }

    if (dev && !rm->checkECRef(dev, mDevices[0])) {
        PAL_DBG(LOG_TAG, "No need to set ec ref for unmatching rx device");
        goto exit;
    }

    status = cur_state_->ProcessEvent(ev_cfg);
    if (status) {
        PAL_ERR(LOG_TAG, "Failed to handle ec ref event");
        goto exit;
    }

    if (is_enable) {
        ec_rx_dev_ = dev;
    } else {
        ec_rx_dev_ = nullptr;
    }

exit:
    PAL_DBG(LOG_TAG, "Exit, status %d", status);
    return status;
}

int32_t StreamSoundTrigger::DisconnectDevice(pal_device_id_t device_id) {
    int32_t status = 0;

    PAL_DBG(LOG_TAG, "Enter");
    /*
     * NOTE: mStreamMutex will be unlocked after ConnectDevice handled
     * because device disconnect/connect should be handled sequencely,
     * and no other commands from client should be handled between
     * device disconnect and connect.
     */
    mStreamMutex.lock();
    std::shared_ptr<StEventConfig> ev_cfg(
        new StDeviceDisconnectedEventConfig(device_id));
    status = cur_state_->ProcessEvent(ev_cfg);
    if (status) {
        PAL_ERR(LOG_TAG, "Failed to disconnect device %d", device_id);
    }

    PAL_DBG(LOG_TAG, "Exit, status %d", status);

    return status;
}

int32_t StreamSoundTrigger::ConnectDevice(pal_device_id_t device_id) {
    int32_t status = 0;

    PAL_DBG(LOG_TAG, "Enter");
    std::shared_ptr<StEventConfig> ev_cfg(
        new StDeviceConnectedEventConfig(device_id));
    status = cur_state_->ProcessEvent(ev_cfg);
    if (status) {
        PAL_ERR(LOG_TAG, "Failed to connect device %d", device_id);
    }
    mStreamMutex.unlock();
    PAL_DBG(LOG_TAG, "Exit, status %d", status);

    return status;
}

int32_t StreamSoundTrigger::Resume() {
    int32_t status = 0;

    PAL_DBG(LOG_TAG, "Enter");
    std::lock_guard<std::mutex> lck(mStreamMutex);
    std::shared_ptr<StEventConfig> ev_cfg(new StResumeEventConfig());
    status = cur_state_->ProcessEvent(ev_cfg);
    if (status) {
        PAL_ERR(LOG_TAG, "Resume failed");
    }
    PAL_DBG(LOG_TAG, "Exit, status %d", status);

    return status;
}

int32_t StreamSoundTrigger::Pause() {
    int32_t status = 0;

    PAL_DBG(LOG_TAG, "Enter");
    std::lock_guard<std::mutex> lck(mStreamMutex);
    std::shared_ptr<StEventConfig> ev_cfg(new StPauseEventConfig());
    status = cur_state_->ProcessEvent(ev_cfg);
    if (status) {
        PAL_ERR(LOG_TAG, "Pause failed");
    }
    PAL_DBG(LOG_TAG, "Exit, status %d", status);

    return status;
}

int32_t StreamSoundTrigger::isSampleRateSupported(uint32_t sampleRate) {
    int32_t rc = 0;

    PAL_DBG(LOG_TAG, "sampleRate %u", sampleRate);
    switch (sampleRate) {
      case SAMPLINGRATE_8K:
      case SAMPLINGRATE_16K:
      case SAMPLINGRATE_32K:
      case SAMPLINGRATE_44K:
      case SAMPLINGRATE_48K:
      case SAMPLINGRATE_96K:
      case SAMPLINGRATE_192K:
      case SAMPLINGRATE_384K:
          break;
      default:
          rc = -EINVAL;
          PAL_ERR(LOG_TAG, "sample rate not supported rc %d", rc);
          break;
    }

    return rc;
}

int32_t StreamSoundTrigger::isChannelSupported(uint32_t numChannels) {
    int32_t rc = 0;

    PAL_DBG(LOG_TAG, "numChannels %u", numChannels);
    switch (numChannels) {
      case CHANNELS_1:
      case CHANNELS_2:
      case CHANNELS_3:
      case CHANNELS_4:
      case CHANNELS_5:
      case CHANNELS_5_1:
      case CHANNELS_7:
      case CHANNELS_8:
          break;
      default:
          rc = -EINVAL;
          PAL_ERR(LOG_TAG, "channels not supported rc %d", rc);
          break;
    }
    return rc;
}

int32_t StreamSoundTrigger::isBitWidthSupported(uint32_t bitWidth) {
    int32_t rc = 0;

    PAL_DBG(LOG_TAG, "bitWidth %u", bitWidth);
    switch (bitWidth) {
      case BITWIDTH_16:
      case BITWIDTH_24:
      case BITWIDTH_32:
          break;
      default:
          rc = -EINVAL;
          PAL_ERR(LOG_TAG, "bit width not supported rc %d", rc);
          break;
    }
    return rc;
}

int32_t StreamSoundTrigger::registerCallBack(pal_stream_callback cb,
                                             uint64_t cookie) {
    callback_ = cb;
    cookie_ = cookie;

    PAL_VERBOSE(LOG_TAG, "callback_ = %pK", callback_);

    return 0;
}

int32_t StreamSoundTrigger::getCallBack(pal_stream_callback *cb) {
    if (!cb) {
        PAL_ERR(LOG_TAG, "Invalid cb");
        return -EINVAL;
    }
    // Do not expect this to be called.
    *cb = callback_;
    return 0;
}

int32_t StreamSoundTrigger::SetEngineDetectionState(int32_t det_type) {
    int32_t status = 0;
    bool lock_status = false;

    PAL_DBG(LOG_TAG, "Enter, det_type %d", det_type);
    if (!(det_type & DETECTION_TYPE_ALL)) {
        PAL_ERR(LOG_TAG, "Invalid detection type %d", det_type);
        return -EINVAL;
    }

    /*
     * setEngineDetectionState should only be called when stream
     * is in ACTIVE state(for first stage) or in BUFFERING state
     * (for second stage)
     */
    do {
        lock_status = mStreamMutex.try_lock();
    } while (!lock_status && (GetCurrentStateId() == ST_STATE_ACTIVE ||
        GetCurrentStateId() == ST_STATE_BUFFERING));

    if ((det_type == GMM_DETECTED &&
         GetCurrentStateId() != ST_STATE_ACTIVE) ||
        ((det_type & DETECTION_TYPE_SS) &&
         GetCurrentStateId() != ST_STATE_BUFFERING)) {
        if (lock_status)
            mStreamMutex.unlock();
        PAL_DBG(LOG_TAG, "Exit as stream not in proper state");
        return -EINVAL;
    }

    if (det_type == GMM_DETECTED) {
        rm->acquireWakeLock();
        reader_->updateState(READER_ENABLED);
    }

    std::shared_ptr<StEventConfig> ev_cfg(
       new StDetectedEventConfig(det_type));
    status = cur_state_->ProcessEvent(ev_cfg);

    /*
     * mStreamMutex may get unlocked in handling detection event
     * and not locked back when stream gets stopped/unloaded,
     * when this happens, mutex_unlocked_after_cb_ will be set to
     * true, so check mutex_unlocked_after_cb_ here to avoid
     * double unlock.
     */
    if (!mutex_unlocked_after_cb_)
        mStreamMutex.unlock();
    else
        mutex_unlocked_after_cb_ = false;

    if (det_type == USER_VERIFICATION_REJECT ||
        det_type == KEYWORD_DETECTION_REJECT)
        rm->handleDeferredSwitch();

    PAL_DBG(LOG_TAG, "Exit, status %d", status);
    return status;
}

void StreamSoundTrigger::InternalStopRecognition() {
    int32_t status = 0;

    PAL_DBG(LOG_TAG, "Enter");
    std::lock_guard<std::mutex> lck(mStreamMutex);
    if (pending_stop_) {
        std::shared_ptr<StEventConfig> ev_cfg(
           new StStopRecognitionEventConfig(true));
        status = cur_state_->ProcessEvent(ev_cfg);
    }
    PAL_DBG(LOG_TAG, "Exit, status %d", status);
}

void StreamSoundTrigger::TimerThread(StreamSoundTrigger& st_stream) {
    PAL_DBG(LOG_TAG, "Enter");

    std::unique_lock<std::mutex> lck(st_stream.timer_mutex_);
    while (!st_stream.exit_timer_thread_) {
        st_stream.timer_start_cond_.wait(lck);
        if (st_stream.exit_timer_thread_)
            break;

        st_stream.timer_wait_cond_.wait_for(lck,
            std::chrono::milliseconds(ST_DEFERRED_STOP_DEALY_MS));

        if (!st_stream.timer_stop_waiting_ && !st_stream.exit_timer_thread_) {
            st_stream.timer_mutex_.unlock();
            st_stream.InternalStopRecognition();
            st_stream.timer_mutex_.lock();
        }
    }
    PAL_DBG(LOG_TAG, "Exit");
}

void StreamSoundTrigger::PostDelayedStop() {
    PAL_VERBOSE(LOG_TAG, "Post Delayed Stop for %p", this);
    pending_stop_ = true;
    std::lock_guard<std::mutex> lck(timer_mutex_);
    timer_stop_waiting_ = false;
    timer_start_cond_.notify_one();
}

void StreamSoundTrigger::CancelDelayedStop() {
    PAL_VERBOSE(LOG_TAG, "Cancel Delayed stop for %p", this);
    pending_stop_ = false;
    std::lock_guard<std::mutex> lck(timer_mutex_);
    timer_stop_waiting_ = true;
    timer_wait_cond_.notify_one();
}

std::shared_ptr<SoundTriggerEngine> StreamSoundTrigger::HandleEngineLoad(
    uint8_t *sm_data,
    int32_t sm_size,
    listen_model_indicator_enum type,
    st_module_type_t module_type) {

    int status = 0;
    std::shared_ptr<SoundTriggerEngine> engine = nullptr;

    engine = SoundTriggerEngine::Create(this, type, module_type, sm_cfg_);
    if (!engine) {
        status = -ENOMEM;
        PAL_ERR(LOG_TAG, "engine creation failed for type %u", type);
        goto error_exit;
    }

    // cache 1st stage model for concurrency handling
    if (type == ST_SM_ID_SVA_F_STAGE_GMM) {
        gsl_engine_model_ = (uint8_t *)realloc(gsl_engine_model_, sm_size);
        if (!gsl_engine_model_) {
            PAL_ERR(LOG_TAG, "Failed to allocate memory for gsl model");
            goto error_exit;
        }
        ar_mem_cpy(gsl_engine_model_, sm_size, sm_data, sm_size);
        gsl_engine_model_size_ = sm_size;
        // Create Voice UI Interface object and update to engines
        vui_intf_ = engine->GetVoiceUIInterface();
        if (!vui_intf_) {
            vui_intf_ = VoiceUIInterface::Create(sm_cfg_);
            if (!vui_intf_) {
                PAL_ERR(LOG_TAG, "Failed to create interface, status %d",
                    status);
                goto error_exit;
            }
            engine->SetVoiceUIInterface(vui_intf_);
            vui_intf_->SetSTModuleType(module_type);
        }

        // register stream/model to Voice UI interface
        status = vui_intf_->RegisterModel(this,
            sm_config_, gsl_engine_model_, gsl_engine_model_size_);
        if (status) {
            PAL_ERR(LOG_TAG, "Failed to register stream/model, status %d",
                status);
            goto error_exit;
        }
        UpdateModelId(module_type);
        vui_intf_->SetModelId(this, model_id_);
        vui_intf_->SetRecognitionMode(this, recognition_mode_);
    }

    status = engine->LoadSoundModel(this, sm_data, sm_size);
    if (status) {
        PAL_ERR(LOG_TAG, "big_sm: gsl engine loading model"
               "failed, status %d", status);
        goto error_exit;
    }

    return engine;

error_exit:
    if (gsl_engine_model_)
        free(gsl_engine_model_);
    if (vui_intf_)
        vui_intf_->DeregisterModel(this);

    return nullptr;
}

/*
 * Return stream instance id for gkv popluation
 * For PDK: always return INSTANCE_1 if only single instance of first stage
            sound model is allowed, otherwise return respective instance ID
            till total number of allowed instances.
 * For SVA4: just return stream instance id
 */
uint32_t StreamSoundTrigger::GetInstanceId() {
    if ((IS_MODULE_TYPE_PDK(model_type_) &&
                sm_cfg_->isSingleInstanceStage1()) ||
        (model_type_ == ST_MODULE_TYPE_GMM &&
                sm_cfg_->GetMergeFirstStageSoundModels()))
        return INSTANCE_1;
    else if (IS_MODULE_TYPE_PDK(model_type_))
        return mInstanceID < sm_cfg_->GetSupportedEngineCount() ?
               mInstanceID : sm_cfg_->GetSupportedEngineCount();
    else
        return mInstanceID;
}

void StreamSoundTrigger::GetUUID(class SoundTriggerUUID *uuid,
                                struct pal_st_sound_model *sound_model) {

    uuid->timeLow = (uint32_t)sound_model->vendor_uuid.timeLow;
    uuid->timeMid = (uint16_t)sound_model->vendor_uuid.timeMid;
    uuid->timeHiAndVersion = (uint16_t)sound_model->vendor_uuid.timeHiAndVersion;
    uuid->clockSeq = (uint16_t)sound_model->vendor_uuid.clockSeq;
    uuid->node[0] = (uint8_t)sound_model->vendor_uuid.node[0];
    uuid->node[1] = (uint8_t)sound_model->vendor_uuid.node[1];
    uuid->node[2] = (uint8_t)sound_model->vendor_uuid.node[2];
    uuid->node[3] = (uint8_t)sound_model->vendor_uuid.node[3];
    uuid->node[4] = (uint8_t)sound_model->vendor_uuid.node[4];
    uuid->node[5] = (uint8_t)sound_model->vendor_uuid.node[5];
}

void StreamSoundTrigger::updateStreamAttributes() {

    /*
     * In case of Single mic handset/headset use cases, stream channels > 1
     * is not a valid configuration. Override the stream attribute channels if the
     * device channels is set to 1
     */
    if (mStreamAttr) {
        if (cap_prof_->GetChannels() == CHANNELS_1 &&
            sm_cfg_->GetOutChannels() > CHANNELS_1) {
            mStreamAttr->in_media_config.ch_info.channels = CHANNELS_1;
        } else {
            mStreamAttr->in_media_config.ch_info.channels =
                sm_cfg_->GetOutChannels();
        }
        /* Update channel map in stream attributes to be in sync with channels */
        switch (mStreamAttr->in_media_config.ch_info.channels) {
            case CHANNELS_2:
                mStreamAttr->in_media_config.ch_info.ch_map[0] =
                    PAL_CHMAP_CHANNEL_FL;
                mStreamAttr->in_media_config.ch_info.ch_map[1] =
                    PAL_CHMAP_CHANNEL_FR;
                break;
            case CHANNELS_1:
            default:
                mStreamAttr->in_media_config.ch_info.ch_map[0] =
                    PAL_CHMAP_CHANNEL_FL;
                break;
        }

        mStreamAttr->in_media_config.sample_rate =
            sm_cfg_->GetSampleRate();
        mStreamAttr->in_media_config.bit_width =
            sm_cfg_->GetBitWidth();
    }
}

void StreamSoundTrigger::UpdateModelId(st_module_type_t type) {
    if (IS_MODULE_TYPE_PDK(type) && mInstanceID)
        model_id_ = ((uint32_t)type << ST_MODEL_TYPE_SHIFT) + mInstanceID;
}

/* TODO:
 *   - Need to track vendor UUID
 */
int32_t StreamSoundTrigger::LoadSoundModel(
    struct pal_st_sound_model *sound_model) {

    int32_t status = 0;
    int32_t engine_id = 0;
    std::shared_ptr<SoundTriggerEngine> engine = nullptr;
    std::shared_ptr<EngineCfg> engine_cfg = nullptr;
    std::vector<sm_pair_t> model_list;

    PAL_DBG(LOG_TAG, "Enter");

    status = UpdateSoundModel(sound_model);
    if (status) {
        PAL_ERR(LOG_TAG, "Failed to update sound model, status %d", status);
        goto error_exit;
    }

    /* Update stream attributes as per sound model config */
    updateStreamAttributes();

    // Parse sound model with Voice UI interface
    status = VoiceUIInterface::ParseSoundModel(sm_cfg_,
        sound_model, model_type_, model_list);
    if (status) {
        PAL_ERR(LOG_TAG, "Failed to parse sound model, status %d", status);
        goto error_exit;
    }

    if (!sm_cfg_->isQCVAUUID()) {
        mStreamSelector = sm_cfg_->GetVUIModuleName();
    } else {
        mStreamSelector = sm_cfg_->GetVUIModuleName(model_type_);
    }
    mInstanceID = rm->getStreamInstanceID(this);

    // Create engines by parsing result
    for (auto iter: model_list) {
        engine_id = static_cast<int32_t>(iter.first);
        engine = HandleEngineLoad((uint8_t *)iter.second.first,
                                  iter.second.second,
                                  iter.first, model_type_);
        std::shared_ptr<EngineCfg> engine_cfg(new EngineCfg(
            engine_id, engine, (void *)iter.second.first, iter.second.second));

        AddEngine(engine_cfg);
        if (iter.first == ST_SM_ID_SVA_F_STAGE_GMM) {
            gsl_engine_ = engine;
        } else {
            if (iter.first & ST_SM_ID_SVA_S_STAGE_KWD) {
                notification_state_ |= KEYWORD_DETECTION_SUCCESS;
            } else if (iter.first == ST_SM_ID_SVA_S_STAGE_USER) {
                notification_state_ |= USER_VERIFICATION_SUCCESS;
            }
        }
    }

    // update voice ui interface for second stage engines
    for (auto &eng: engines_) {
        if (eng->GetEngineId() & ST_SM_ID_SVA_S_STAGE_KWD ||
            eng->GetEngineId() == ST_SM_ID_SVA_S_STAGE_USER)
            eng->GetEngine()->SetVoiceUIInterface(vui_intf_);
    }

    goto exit;

error_exit:
    for (auto iter: model_list) {
        if (iter.second.first)
            free(iter.second.first);
    }
    for (auto &eng: engines_) {
        eng->GetEngine()->UnloadSoundModel(this);
    }
    engines_.clear();
    gsl_engine_.reset();
    if (sm_config_) {
        free(sm_config_);
        sm_config_ = nullptr;
    }
exit:
    PAL_DBG(LOG_TAG, "Exit, status %d", status);
    return status;
}

int32_t StreamSoundTrigger::UpdateSoundModel(
    struct pal_st_sound_model *sound_model) {
    int32_t status = 0;
    int32_t sm_size = 0;
    struct pal_st_phrase_sound_model *phrase_sm = nullptr;
    struct pal_st_sound_model *common_sm = nullptr;
    class SoundTriggerUUID uuid;

    PAL_DBG(LOG_TAG, "Enter");

    if (!sound_model) {
        PAL_ERR(LOG_TAG, "Invalid sound_model param status %d", status);
        status = -EINVAL;
        goto exit;
    }
    sound_model_type_ = sound_model->type;

    if (sound_model->type == PAL_SOUND_MODEL_TYPE_KEYPHRASE) {
        phrase_sm = (struct pal_st_phrase_sound_model *)sound_model;
        if ((phrase_sm->common.data_offset < sizeof(*phrase_sm)) ||
            (phrase_sm->common.data_size == 0) ||
            (phrase_sm->num_phrases == 0)) {
            PAL_ERR(LOG_TAG, "Invalid phrase sound model params data size=%d, "
                   "data offset=%d, type=%d phrases=%d status %d",
                   phrase_sm->common.data_size, phrase_sm->common.data_offset,
                   phrase_sm->common.type,phrase_sm->num_phrases, status);
            status = -EINVAL;
            goto exit;
        }
        common_sm = (struct pal_st_sound_model*)&phrase_sm->common;
        sm_size = sizeof(*phrase_sm) + common_sm->data_size;

    } else if (sound_model->type == PAL_SOUND_MODEL_TYPE_GENERIC) {
        if ((sound_model->data_size == 0) ||
            (sound_model->data_offset < sizeof(struct pal_st_sound_model))) {
            PAL_ERR(LOG_TAG, "Invalid generic sound model params data size=%d,"
                    " data offset=%d status %d", sound_model->data_size,
                    sound_model->data_offset, status);
            status = -EINVAL;
            goto exit;
        }
        common_sm = sound_model;
        sm_size = sizeof(*common_sm) + common_sm->data_size;
    } else {
        PAL_ERR(LOG_TAG, "Unknown sound model type - %d status %d",
                sound_model->type, status);
        status = -EINVAL;
        goto exit;
    }
    if (sm_config_ != sound_model) {
        // Cache to use during SSR and other internal events handling.
        if (sm_config_) {
            free(sm_config_);
        }
        sm_config_ = (struct pal_st_sound_model *)calloc(1, sm_size);
        if (!sm_config_) {
            PAL_ERR(LOG_TAG, "sound model config allocation failed, status %d",
                    status);
            status = -ENOMEM;
            goto exit;
        }

        if (sound_model->type == PAL_SOUND_MODEL_TYPE_KEYPHRASE) {
            ar_mem_cpy(sm_config_, sizeof(*phrase_sm),
                         phrase_sm, sizeof(*phrase_sm));
            ar_mem_cpy((uint8_t *)sm_config_ + common_sm->data_offset,
                         common_sm->data_size,
                         (uint8_t *)phrase_sm + common_sm->data_offset,
                         common_sm->data_size);
            recognition_mode_ = phrase_sm->phrases[0].recognition_mode;
        } else {
            ar_mem_cpy(sm_config_, sizeof(*common_sm),
                         common_sm, sizeof(*common_sm));
            ar_mem_cpy((uint8_t *)sm_config_ +  common_sm->data_offset,
                         common_sm->data_size,
                         (uint8_t *)common_sm + common_sm->data_offset,
                         common_sm->data_size);
            recognition_mode_ = PAL_RECOGNITION_MODE_VOICE_TRIGGER;
        }
    }
    GetUUID(&uuid, sound_model);
    this->sm_cfg_ = this->vui_ptfm_info_->GetStreamConfig(uuid);
    if (!this->sm_cfg_) {
        PAL_ERR(LOG_TAG, "Failed to get sound model config");
        status = -EINVAL;
    }
exit:
    PAL_DBG(LOG_TAG, "Exit, status %d", status);
    return status;
}

// TODO: look into how cookies are used here
int32_t StreamSoundTrigger::SendRecognitionConfig(
    struct pal_st_recognition_config *config) {

    int32_t status = 0;
    int32_t i = 0;
    uint32_t hist_buffer_duration = 0;
    uint32_t pre_roll_duration = 0;
    uint32_t client_capture_read_delay = 0;
    uint8_t *conf_levels = NULL;
    uint32_t num_conf_levels = 0;
    uint32_t ring_buffer_len = 0;
    uint32_t ring_buffer_size = 0;
    uint32_t sec_stage_threshold = 0;

    PAL_DBG(LOG_TAG, "Enter");
    if (!vui_intf_) {
        PAL_ERR(LOG_TAG, "VoiceUI Interface not created!");
        return -EINVAL;
    }

    status = UpdateRecognitionConfig(config);
    if (status) {
        PAL_ERR(LOG_TAG, "Failed to update recognition config, status %d",
            status);
        goto error_exit;
    }

    // dump recognition config opaque data
    if (config->data_size > 0 && vui_ptfm_info_->GetEnableDebugDumps()) {
        ST_DBG_DECLARE(FILE *rec_opaque_fd = NULL; static int rec_opaque_cnt = 0);
        ST_DBG_FILE_OPEN_WR(rec_opaque_fd, ST_DEBUG_DUMP_LOCATION,
            "rec_config_opaque", "bin", rec_opaque_cnt);
        ST_DBG_FILE_WRITE(rec_opaque_fd,
            (uint8_t *)rec_config_ + config->data_offset, config->data_size);
        ST_DBG_FILE_CLOSE(rec_opaque_fd);
        PAL_DBG(LOG_TAG, "recognition config opaque data stored in: rec_config_opaque_%d.bin",
            rec_opaque_cnt);
        rec_opaque_cnt++;
    }

    // Parse recognition config with VoiceUI Interface
    status = vui_intf_->ParseRecognitionConfig(this, rec_config_);
    if (status) {
        PAL_ERR(LOG_TAG, "Failed to parse recognition config, status %d",
            status);
        goto error_exit;
    }

    // acquire buffering config for current stream
    vui_intf_->GetBufferingConfigs(this, &hist_buf_duration_,
                                   &pre_roll_duration_);

    // use default value if preroll is not set
    if (pre_roll_duration_ == 0) {
        pre_roll_duration_ = sm_cfg_->GetPreRollDuration();
    }

    client_capture_read_delay = sm_cfg_->GetCaptureReadDelay();
    PAL_DBG(LOG_TAG, "history buf len = %d, preroll len = %d, read delay = %d",
        hist_buf_duration_, pre_roll_duration_, client_capture_read_delay);

    if (!hist_buf_duration_) {
        status = gsl_engine_->UpdateBufConfig(this,
            sm_cfg_->GetKwDuration(), pre_roll_duration_);
    } else {
        status = gsl_engine_->UpdateBufConfig(this,
            hist_buf_duration_, pre_roll_duration_);
    }

    if (status) {
        PAL_ERR(LOG_TAG, "Failed to update buf config, status %d", status);
        goto error_exit;
    }

    /*
     * Get the updated buffer config from engine as multiple streams
     * attached to it might have different buffer configurations
     */
    gsl_engine_->GetUpdatedBufConfig(&hist_buffer_duration,
                                     &pre_roll_duration);

    PAL_INFO(LOG_TAG, "updated hist buf len = %d, preroll len = %d in gsl engine",
        hist_buffer_duration, pre_roll_duration);

    // update input buffer size for mmap usecase
    if (vui_ptfm_info_->GetMmapEnable()) {
        inBufSize = vui_ptfm_info_->GetMmapFrameLength() *
            sm_cfg_->GetSampleRate() * sm_cfg_->GetBitWidth() *
            sm_cfg_->GetOutChannels() / (MS_PER_SEC * BITS_PER_BYTE);
        if (!inBufSize) {
            PAL_ERR(LOG_TAG, "Invalid frame size, use default value");
            inBufSize = BUF_SIZE_CAPTURE;
        }
    }

    // create ring buffer for lab transfer in gsl_engine
    ring_buffer_len = hist_buffer_duration + pre_roll_duration +
        client_capture_read_delay;
    ring_buffer_size = (ring_buffer_len / MS_PER_SEC) * sm_cfg_->GetSampleRate() *
                       sm_cfg_->GetBitWidth() *
                       sm_cfg_->GetOutChannels() / BITS_PER_BYTE;
    status = gsl_engine_->CreateBuffer(ring_buffer_size,
                                       engines_.size(), reader_list_);
    if (status) {
        PAL_ERR(LOG_TAG, "Failed to get ring buf reader, status %d", status);
        goto error_exit;
    }

    /*
     * Assign created readers based on sound model sequence.
     * For first stage engine, assign reader to stream side.
     */
    for (i = 0; i < engines_.size(); i++) {
        if (engines_[i]->GetEngineId() == ST_SM_ID_SVA_F_STAGE_GMM) {
            reader_ = reader_list_[i];
        } else {
            status = engines_[i]->GetEngine()->SetBufferReader(
                reader_list_[i]);
            if (status) {
                PAL_ERR(LOG_TAG, "Failed to set ring buffer reader");
                goto error_exit;
            }
        }
    }

    for (auto &eng: engines_) {
        if (eng->GetEngineId() == ST_SM_ID_SVA_F_STAGE_GMM) {
            vui_intf_->GetWakeupConfigs(this, (void **)&conf_levels, &num_conf_levels);
            eng->GetEngine()->UpdateConfLevels(this, config, conf_levels, num_conf_levels);
            if (num_conf_levels > 0) {
                gsl_conf_levels_ = (uint8_t *)realloc(gsl_conf_levels_,
                                                      num_conf_levels);
                if (!gsl_conf_levels_) {
                    PAL_ERR(LOG_TAG, "Failed to allocate gsl conf levels memory");
                    status = -ENOMEM;
                    goto error_exit;
                }
                ar_mem_cpy(gsl_conf_levels_,
                    num_conf_levels, conf_levels, num_conf_levels);
                gsl_conf_levels_size_ = num_conf_levels;
            }
        } else if (eng->GetEngineId() & ST_SM_ID_SVA_S_STAGE_KWD ||
                   eng->GetEngineId() == ST_SM_ID_SVA_S_STAGE_USER) {
            vui_intf_->GetSecondStageConfLevels(this,
                (listen_model_indicator_enum)eng->GetEngineId(),
                &sec_stage_threshold);
            eng->GetEngine()->UpdateConfLevels(this,
                config, (uint8_t *)&sec_stage_threshold, 1);
        }
    }

    // Update capture requested flag to gsl engine
    if (!config->capture_requested && engines_.size() == 1)
        capture_requested_ = false;
    else
        capture_requested_ = true;
    gsl_engine_->SetCaptureRequested(capture_requested_);
    goto exit;

error_exit:
    if (rec_config_) {
        free(rec_config_);
        rec_config_ = nullptr;
    }

exit:

    PAL_DBG(LOG_TAG, "Exit, status %d", status);
    return status;
}

int32_t StreamSoundTrigger::UpdateRecognitionConfig(
    struct pal_st_recognition_config *config) {
    int32_t status = 0;

    PAL_DBG(LOG_TAG, "Enter");
    if (!config) {
        PAL_ERR(LOG_TAG, "Invalid config");
        status = -EINVAL;
        goto exit;
    }
    if (rec_config_ != config) {
        // Possible due to subsequent detections.
        if (rec_config_) {
            free(rec_config_);
        }
        rec_config_ = (struct pal_st_recognition_config *)calloc(1,
            sizeof(struct pal_st_recognition_config) + config->data_size);
        if (!rec_config_) {
            PAL_ERR(LOG_TAG, "Failed to allocate rec_config status %d", status);
            status =  -ENOMEM;
            goto exit;
        }
        ar_mem_cpy(rec_config_, sizeof(struct pal_st_recognition_config),
                     config, sizeof(struct pal_st_recognition_config));
        ar_mem_cpy((uint8_t *)rec_config_ + config->data_offset,
                     config->data_size,
                     (uint8_t *)config + config->data_offset,
                     config->data_size);
    }
exit:
    PAL_DBG(LOG_TAG, "Exit, status %d", status);
    return status;
}

bool StreamSoundTrigger::compareRecognitionConfig(
   const struct pal_st_recognition_config *current_config,
   struct pal_st_recognition_config *new_config) {
    uint32_t i = 0, j = 0;

    if (!current_config || !new_config)
        return false;

    /*
     * Sometimes if the number of user confidence levels is 0, the
     * pal_st_confidence_level struct will be different between the two
     * configs. So all the values must be checked instead of a memcmp of the
     * whole configs.
     */
    if ((current_config->capture_handle != new_config->capture_handle) ||
        (current_config->capture_device != new_config->capture_device) ||
        (current_config->capture_requested != new_config->capture_requested) ||
        (current_config->num_phrases != new_config->num_phrases) ||
        (current_config->data_size != new_config->data_size) ||
        (current_config->data_offset != new_config->data_offset) ||
        std::memcmp((char *) current_config + current_config->data_offset,
               (char *) new_config + new_config->data_offset,
               current_config->data_size)) {
        return false;
    } else {
        for (i = 0; i < current_config->num_phrases; i++) {
            if ((current_config->phrases[i].id !=
                 new_config->phrases[i].id) ||
                (current_config->phrases[i].recognition_modes !=
                 new_config->phrases[i].recognition_modes) ||
                (current_config->phrases[i].confidence_level !=
                 new_config->phrases[i].confidence_level) ||
                (current_config->phrases[i].num_levels !=
                 new_config->phrases[i].num_levels)) {
                return false;
            } else {
                for (j = 0; j < current_config->phrases[i].num_levels; j++) {
                    if ((current_config->phrases[i].levels[j].user_id !=
                         new_config->phrases[i].levels[j].user_id) ||
                        (current_config->phrases[i].levels[j].level !=
                         new_config->phrases[i].levels[j].level))
                        return false;
                }
            }
        }
        return true;
    }
}

int32_t StreamSoundTrigger::notifyClient(bool detection) {
    int32_t status = 0;
    struct pal_st_recognition_event *rec_event = nullptr;
    uint32_t event_size;
    ChronoSteadyClock_t notify_time;
    uint64_t total_process_duration = 0;
    bool lock_status = false;

    status = vui_intf_->GenerateCallbackEvent(this,
                                             &rec_event,
                                             &event_size,
                                             detection);
    if (status || !rec_event) {
        PAL_ERR(LOG_TAG, "Failed to generate callback event");
        return status;
    }
    if (callback_) {
        // update stream state to stopped before unlock stream mutex
        currentState = STREAM_STOPPED;
        notify_time = std::chrono::steady_clock::now();
        total_process_duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                notify_time - gsl_engine_->GetDetectedTime()).count();
        PAL_INFO(LOG_TAG, "Notify detection event to client,"
            " total processing time: %llums",
            (long long)total_process_duration);
        mStreamMutex.unlock();
        callback_((pal_stream_handle_t *)this, 0, (uint32_t *)rec_event,
                  event_size, (uint64_t)rec_config_->cookie);

        /*
         * client may call unload when we are doing callback with mutex
         * unlocked, which will be blocked in second stage thread exiting
         * as it needs notifyClient to finish. Try lock mutex and check
         * stream states when try lock fails so that we can skip lock
         * when stream is already stopped by client.
         */
        do {
            lock_status = mStreamMutex.try_lock();
        } while (!lock_status && (GetCurrentStateId() == ST_STATE_DETECTED ||
            GetCurrentStateId() == ST_STATE_BUFFERING));

        /*
         * NOTE: Not unlock stream mutex here if mutex is locked successfully
         * in above loop to make stream mutex lock/unlock consistent in vairous
         * cases for calling SetEngineDetectionState(caller of notifyClient).
         * This is because SetEngineDetectionState may also be called by
         * gsl engine to notify GMM detected with second stage enabled, and in
         * this case notifyClient is not called, so we need to unlock stream
         * mutex at end of SetEngineDetectionState, that's why we don't need
         * to unlock stream mutex here.
         * If mutex is not locked here, mark mutex_unlocked_after_cb_ as true
         * so that we can avoid double unlock in SetEngineDetectionState.
         */
        if (!lock_status)
            mutex_unlocked_after_cb_ = true;
    }

    free(rec_event);

    PAL_DBG(LOG_TAG, "Exit, status %d", status);
    return status;
}

void StreamSoundTrigger::SetDetectedToEngines(bool detected) {
    for (auto& eng: engines_) {
        if (eng->GetEngineId() != ST_SM_ID_SVA_F_STAGE_GMM) {
            PAL_VERBOSE(LOG_TAG, "Notify detection event %d to engine %d",
                    detected, eng->GetEngineId());
            eng->GetEngine()->SetDetected(detected);
        }
    }
}

pal_device_id_t StreamSoundTrigger::GetAvailCaptureDevice(){
    if (vui_ptfm_info_->GetSupportDevSwitch() &&
        rm->isDeviceAvailable(PAL_DEVICE_IN_WIRED_HEADSET))
        return PAL_DEVICE_IN_HEADSET_VA_MIC;
    else
        return PAL_DEVICE_IN_HANDSET_VA_MIC;
}

void StreamSoundTrigger::AddEngine(std::shared_ptr<EngineCfg> engine_cfg) {
    for (int32_t i = 0; i < engines_.size(); i++) {
        if (engines_[i] == engine_cfg) {
            PAL_VERBOSE(LOG_TAG, "engine type %d already exists",
                        engine_cfg->id_);
            return;
        }
    }
    PAL_VERBOSE(LOG_TAG, "Add engine %d, gsl_engine %p", engine_cfg->id_,
                gsl_engine_.get());
    engines_.push_back(engine_cfg);
}

std::shared_ptr<CaptureProfile> StreamSoundTrigger::GetCurrentCaptureProfile() {
    std::shared_ptr<CaptureProfile> cap_prof = nullptr;
    bool is_transit_to_nlpi = false;

    is_transit_to_nlpi = rm->CheckForForcedTransitToNonLPI();

    if (GetAvailCaptureDevice() == PAL_DEVICE_IN_HEADSET_VA_MIC) {
        if (is_transit_to_nlpi) {
            cap_prof = sm_cfg_->GetCaptureProfile(
                std::make_pair(ST_OPERATING_MODE_HIGH_PERF_AND_CHARGING,
                    ST_INPUT_MODE_HEADSET));
        } else if (use_lpi_) {
            cap_prof = sm_cfg_->GetCaptureProfile(
                std::make_pair(ST_OPERATING_MODE_LOW_POWER,
                    ST_INPUT_MODE_HEADSET));
        } else {
            cap_prof = sm_cfg_->GetCaptureProfile(
                std::make_pair(ST_OPERATING_MODE_HIGH_PERF,
                    ST_INPUT_MODE_HEADSET));
        }
    } else {
        if (is_transit_to_nlpi) {
            cap_prof = sm_cfg_->GetCaptureProfile(
                std::make_pair(ST_OPERATING_MODE_HIGH_PERF_AND_CHARGING,
                    ST_INPUT_MODE_HANDSET));
        } else if (use_lpi_) {
            cap_prof = sm_cfg_->GetCaptureProfile(
                std::make_pair(ST_OPERATING_MODE_LOW_POWER,
                    ST_INPUT_MODE_HANDSET));
        } else {
            cap_prof = sm_cfg_->GetCaptureProfile(
                std::make_pair(ST_OPERATING_MODE_HIGH_PERF,
                    ST_INPUT_MODE_HANDSET));
        }
    }

    if (cap_prof) {
        PAL_DBG(LOG_TAG, "cap_prof %s: dev_id=0x%x, chs=%d, sr=%d, snd_name=%s, ec_ref=%d",
            cap_prof->GetName().c_str(), cap_prof->GetDevId(),
            cap_prof->GetChannels(), cap_prof->GetSampleRate(),
            cap_prof->GetSndName().c_str(), cap_prof->isECRequired());
    }

    return cap_prof;
}

void StreamSoundTrigger::AddState(StState* state) {
   st_states_.insert(std::make_pair(state->GetStateId(), state));
}

int32_t StreamSoundTrigger::GetCurrentStateId() {
    if (cur_state_)
        return cur_state_->GetStateId();

    return ST_STATE_NONE;
}

int32_t StreamSoundTrigger::GetPreviousStateId() {
    if (prev_state_)
        return prev_state_->GetStateId();

    return ST_STATE_NONE;
}

void StreamSoundTrigger::TransitTo(int32_t state_id) {
    auto it = st_states_.find(state_id);
    if (it == st_states_.end()) {
        PAL_ERR(LOG_TAG, "Unknown transit state %d ", state_id);
        return;
    }
    prev_state_ = cur_state_;
    cur_state_ = it->second;
    auto oldState = stStateNameMap.at(prev_state_->GetStateId());
    auto newState = stStateNameMap.at(it->first);
    PAL_DBG(LOG_TAG, "Stream instance %u: state transitioned from %s to %s",
            mInstanceID, oldState.c_str(), newState.c_str());
}

int32_t StreamSoundTrigger::ProcessInternalEvent(
    std::shared_ptr<StEventConfig> ev_cfg) {
    return cur_state_->ProcessEvent(ev_cfg);
}

int32_t StreamSoundTrigger::StIdle::ProcessEvent(
    std::shared_ptr<StEventConfig> ev_cfg) {

    int32_t status = 0;

    PAL_DBG(LOG_TAG, "StIdle: handle event %d for stream instance %u",
        ev_cfg->id_, st_stream_.mInstanceID);

    switch (ev_cfg->id_) {
        case ST_EV_LOAD_SOUND_MODEL: {
            std::shared_ptr<CaptureProfile> cap_prof = nullptr;
            StLoadEventConfigData *data =
                (StLoadEventConfigData *)ev_cfg->data_.get();
            class SoundTriggerUUID uuid;
            struct pal_st_sound_model * pal_st_sm;

            pal_st_sm = (struct pal_st_sound_model *)data->data_;

            uuid.timeLow = (uint32_t)pal_st_sm->vendor_uuid.timeLow;
            uuid.timeMid = (uint16_t)pal_st_sm->vendor_uuid.timeMid;
            uuid.timeHiAndVersion = (uint16_t)pal_st_sm->vendor_uuid.timeHiAndVersion;
            uuid.clockSeq = (uint16_t)pal_st_sm->vendor_uuid.clockSeq;
            uuid.node[0] = (uint8_t)pal_st_sm->vendor_uuid.node[0];
            uuid.node[1] = (uint8_t)pal_st_sm->vendor_uuid.node[1];
            uuid.node[2] = (uint8_t)pal_st_sm->vendor_uuid.node[2];
            uuid.node[3] = (uint8_t)pal_st_sm->vendor_uuid.node[3];
            uuid.node[4] = (uint8_t)pal_st_sm->vendor_uuid.node[4];
            uuid.node[5] = (uint8_t)pal_st_sm->vendor_uuid.node[5];

            PAL_INFO(LOG_TAG, "Input vendor uuid : %08x-%04x-%04x-%04x-%02x%02x%02x%02x%02x%02x",
                        uuid.timeLow,
                        uuid.timeMid,
                        uuid.timeHiAndVersion,
                        uuid.clockSeq,
                        uuid.node[0],
                        uuid.node[1],
                        uuid.node[2],
                        uuid.node[3],
                        uuid.node[4],
                        uuid.node[5]);

            st_stream_.sm_cfg_ = st_stream_.vui_ptfm_info_->GetStreamConfig(uuid);

            if (!st_stream_.sm_cfg_) {
                PAL_ERR(LOG_TAG, "Failed to get sound model platform info");
                status = -EINVAL;
                goto err_exit;
            }

            if (!st_stream_.mDevices.size()) {
                std::shared_ptr<Device> dev = nullptr;

                // update best device
                pal_device_id_t dev_id = st_stream_.GetAvailCaptureDevice();
                PAL_DBG(LOG_TAG, "Select available caputre device %d", dev_id);

                dev = st_stream_.GetPalDevice(&st_stream_, dev_id);
                if (!dev) {
                    PAL_ERR(LOG_TAG, "Device creation is failed");
                    status = -EINVAL;
                    goto err_exit;
                }
                st_stream_.mDevices.push_back(dev);
                dev = nullptr;
            }

            cap_prof = st_stream_.GetCurrentCaptureProfile();
            st_stream_.cap_prof_ = cap_prof;
            st_stream_.mDevPPSelector = cap_prof->GetName();
            PAL_DBG(LOG_TAG, "devicepp selector: %s", st_stream_.mDevPPSelector.c_str());
            status = st_stream_.LoadSoundModel(pal_st_sm);

            if (0 != status) {
                PAL_ERR(LOG_TAG, "Failed to load sm, status %d", status);
                goto err_exit;
            } else {
                PAL_VERBOSE(LOG_TAG, "Opened the engine and dev successfully");
                TransitTo(ST_STATE_LOADED);
                break;
            }
        err_exit:
            break;
        }
        case ST_EV_UNLOAD_SOUND_MODEL: {
            if (st_stream_.mInstanceID == 0) {
                PAL_DBG(LOG_TAG, "No model is loaded, ignore unload");
                break;
            }

            for (auto& eng: st_stream_.engines_) {
                PAL_DBG(LOG_TAG, "Unload engine %d", eng->GetEngineId());
                status = eng->GetEngine()->UnloadSoundModel(&st_stream_);
                if (0 != status) {
                    PAL_ERR(LOG_TAG, "Unload engine %d failed, status %d",
                            eng->GetEngineId(), status);
                }
                free(eng->sm_data_);
            }

            if (st_stream_.device_opened_ && st_stream_.mDevices.size() > 0) {
                status = st_stream_.mDevices[0]->close();
                if (0 != status) {
                    PAL_ERR(LOG_TAG, "Failed to close device, status %d",
                        status);
                }
            }

            st_stream_.mDevices.clear();

            if(st_stream_.gsl_engine_)
                st_stream_.gsl_engine_->ResetBufferReaders(st_stream_.reader_list_);
            if (st_stream_.reader_) {
                delete st_stream_.reader_;
                st_stream_.reader_ = nullptr;
            }
            st_stream_.engines_.clear();
            if(st_stream_.gsl_engine_)
                st_stream_.gsl_engine_->DetachStream(&st_stream_, true);
            st_stream_.reader_list_.clear();
            if (st_stream_.vui_intf_)
                st_stream_.vui_intf_->DeregisterModel(&st_stream_);
            st_stream_.rm->resetStreamInstanceID(
                &st_stream_,
                st_stream_.mInstanceID);
            break;
        }
        case ST_EV_PAUSE: {
            st_stream_.paused_ = true;
            break;
        }
        case ST_EV_RESUME: {
            st_stream_.paused_ = false;
            break;
        }
        case ST_EV_READ_BUFFER: {
            status = -EIO;
            break;
        }
        case ST_EV_DEVICE_DISCONNECTED: {
            StDeviceDisconnectedEventConfigData *data =
                (StDeviceDisconnectedEventConfigData *)ev_cfg->data_.get();
            pal_device_id_t device_id = data->dev_id_;
            if (st_stream_.mDevices.size() == 0) {
                PAL_DBG(LOG_TAG, "No device to disconnect");
                break;
            } else {
                int curr_device_id = st_stream_.mDevices[0]->getSndDeviceId();
                pal_device_id_t curr_device =
                    static_cast<pal_device_id_t>(curr_device_id);
                if (curr_device != device_id) {
                    PAL_ERR(LOG_TAG, "Device %d not connected, ignore",
                        device_id);
                    break;
                }
            }
            st_stream_.mDevices.clear();
            break;
        }
        case ST_EV_DEVICE_CONNECTED: {
            std::shared_ptr<Device> dev = nullptr;
            StDeviceConnectedEventConfigData *data =
                (StDeviceConnectedEventConfigData *)ev_cfg->data_.get();
            pal_device_id_t dev_id = data->dev_id_;

            // mDevices should be empty as we have just disconnected device
            if (st_stream_.mDevices.size() != 0) {
                PAL_ERR(LOG_TAG, "Invalid operation");
                status = -EINVAL;
                goto connect_err;
            }

            //sm_cfg_ must be initialized, if there was any device associated
            // with this stream earlier
            if (!st_stream_.sm_cfg_) {
                PAL_DBG(LOG_TAG, "Skip device connection as it will be handled in sound model load");
                goto connect_err;
            }

            dev = st_stream_.GetPalDevice(&st_stream_, dev_id);
            if (!dev) {
                PAL_ERR(LOG_TAG, "Device creation failed");
                status = -EINVAL;
                goto connect_err;
            }

            st_stream_.mDevices.push_back(dev);
        connect_err:
            break;
        }
        case ST_EV_CONCURRENT_STREAM: {
            // Avoid handling concurrency before sound model loaded
            if (!st_stream_.sm_config_)
                break;
            std::shared_ptr<CaptureProfile> new_cap_prof = nullptr;
            bool active = false;

            if (ev_cfg->id_ == ST_EV_CONCURRENT_STREAM) {
                StConcurrentStreamEventConfigData *data =
                    (StConcurrentStreamEventConfigData *)ev_cfg->data_.get();
                active = data->is_active_;
            }
            new_cap_prof = st_stream_.GetCurrentCaptureProfile();
            if (new_cap_prof && (st_stream_.cap_prof_ != new_cap_prof)) {
                PAL_DBG(LOG_TAG,
                    "current capture profile %s: dev_id=0x%x, chs=%d, sr=%d, ec_ref=%d\n",
                    st_stream_.cap_prof_->GetName().c_str(),
                    st_stream_.cap_prof_->GetDevId(),
                    st_stream_.cap_prof_->GetChannels(),
                    st_stream_.cap_prof_->GetSampleRate(),
                    st_stream_.cap_prof_->isECRequired());
                PAL_DBG(LOG_TAG,
                    "new capture profile %s: dev_id=0x%x, chs=%d, sr=%d, ec_ref=%d\n",
                    new_cap_prof->GetName().c_str(),
                    new_cap_prof->GetDevId(),
                    new_cap_prof->GetChannels(),
                    new_cap_prof->GetSampleRate(),
                    new_cap_prof->isECRequired());
                if (active) {
                    if (!st_stream_.mDevices.size()) {
                        std::shared_ptr<Device> dev = nullptr;

                        // update best device
                        pal_device_id_t dev_id = st_stream_.GetAvailCaptureDevice();
                        PAL_DBG(LOG_TAG, "Select available caputre device %d", dev_id);

                        dev = st_stream_.GetPalDevice(&st_stream_, dev_id);
                        if (!dev) {
                            PAL_ERR(LOG_TAG, "Device creation is failed");
                            status = -EINVAL;
                            goto err_concurrent;
                        }
                        st_stream_.mDevices.push_back(dev);
                        dev = nullptr;
                    }

                    st_stream_.cap_prof_ = new_cap_prof;
                    st_stream_.mDevPPSelector = new_cap_prof->GetName();
                    PAL_DBG(LOG_TAG, "devicepp selector: %s",
                        st_stream_.mDevPPSelector.c_str());

                    status = st_stream_.gsl_engine_->LoadSoundModel(&st_stream_,
                        st_stream_.gsl_engine_model_,
                        st_stream_.gsl_engine_model_size_);
                    if (0 != status) {
                        PAL_ERR(LOG_TAG, "Failed to load sound model, status %d",
                            status);
                        goto err_concurrent;
                    }

                    if (st_stream_.rec_config_) {
                        status = st_stream_.gsl_engine_->UpdateConfLevels(&st_stream_,
                            st_stream_.rec_config_, st_stream_.gsl_conf_levels_,
                            st_stream_.gsl_conf_levels_size_);
                        if (0 != status) {
                            PAL_ERR(LOG_TAG, "Failed to update conf levels, status %d",
                                status);
                            goto err_unload;
                        }
                    }

                    TransitTo(ST_STATE_LOADED);
                    if (st_stream_.isActive()) {
                        std::shared_ptr<StEventConfig> ev_cfg1(
                            new StStartRecognitionEventConfig(false));
                        status = st_stream_.ProcessInternalEvent(ev_cfg1);
                        if (0 != status) {
                            PAL_ERR(LOG_TAG, "Failed to Start, status %d", status);
                        }
                    }
                }
            } else {
                PAL_INFO(LOG_TAG,"no action needed, same capture profile");
            }
            break;
        err_unload:
            status = st_stream_.gsl_engine_->UnloadSoundModel(&st_stream_);
            if (0 != status)
                PAL_ERR(LOG_TAG, "Failed to unload sound model, status %d", status);

        err_concurrent:
            break;
        }
        case ST_EV_SSR_OFFLINE:
            if (st_stream_.state_for_restore_ == ST_STATE_NONE) {
                st_stream_.state_for_restore_ = ST_STATE_IDLE;
            }
            TransitTo(ST_STATE_SSR);
            break;
        default: {
            PAL_DBG(LOG_TAG, "Unhandled event %d", ev_cfg->id_);
            break;
        }
    }

    return status;
}

int32_t StreamSoundTrigger::StLoaded::ProcessEvent(
    std::shared_ptr<StEventConfig> ev_cfg) {

    int32_t status = 0;

    PAL_DBG(LOG_TAG, "StLoaded: handle event %d for stream instance %u",
        ev_cfg->id_, st_stream_.mInstanceID);

    switch (ev_cfg->id_) {
        case ST_EV_UNLOAD_SOUND_MODEL: {
            int ret = 0;

            for (auto& eng: st_stream_.engines_) {
                PAL_DBG(LOG_TAG, "Unload engine %d", eng->GetEngineId());
                status = eng->GetEngine()->UnloadSoundModel(&st_stream_);
                if (0 != status) {
                    PAL_ERR(LOG_TAG, "Unload engine %d failed, status %d",
                            eng->GetEngineId(), status);
                    status = ret;
                }
                free(eng->sm_data_);
            }

            if (st_stream_.device_opened_ && st_stream_.mDevices.size() > 0) {
                status = st_stream_.mDevices[0]->close();
                if (0 != status) {
                    PAL_ERR(LOG_TAG, "Failed to close device, status %d",
                        status);
                }
            }

            st_stream_.mDevices.clear();

            st_stream_.gsl_engine_->ResetBufferReaders(st_stream_.reader_list_);
            if (st_stream_.reader_) {
                delete st_stream_.reader_;
                st_stream_.reader_ = nullptr;
            }
            st_stream_.engines_.clear();
            st_stream_.gsl_engine_->DetachStream(&st_stream_, true);
            st_stream_.reader_list_.clear();
            if (st_stream_.vui_intf_)
                st_stream_.vui_intf_->DeregisterModel(&st_stream_);
            st_stream_.rm->resetStreamInstanceID(
                &st_stream_,
                st_stream_.mInstanceID);
            st_stream_.notification_state_ = ENGINE_IDLE;
            TransitTo(ST_STATE_IDLE);
            break;
        }
        case ST_EV_RECOGNITION_CONFIG: {
            StRecognitionCfgEventConfigData *data =
                (StRecognitionCfgEventConfigData *)ev_cfg->data_.get();
            status = st_stream_.SendRecognitionConfig(
               (struct pal_st_recognition_config *)data->data_);
            if (0 != status) {
                PAL_ERR(LOG_TAG, "Failed to send recognition config, status %d",
                        status);
            }
            break;
        }
        case ST_EV_RESUME: {
            st_stream_.paused_ = false;
            if (!st_stream_.isActive()) {
                // Possible if App has stopped recognition during active
                // concurrency.
                break;
            }
            // Update conf levels in case conf level is set to 100 in pause
            if (st_stream_.rec_config_) {
                status = st_stream_.SendRecognitionConfig(st_stream_.rec_config_);
                if (0 != status) {
                    PAL_ERR(LOG_TAG, "Failed to send recognition config, status %d",
                        status);
                    break;
                }
            }
            // fall through to start
            [[fallthrough]];
        }
        case ST_EV_START_RECOGNITION: {
#ifndef SEC_AUDIO_SOUND_TRIGGER_TYPE
            if (st_stream_.paused_) {
               break; // Concurrency is active, start later.
            }
#endif
            StStartRecognitionEventConfigData *data =
                (StStartRecognitionEventConfigData *)ev_cfg->data_.get();
            if (!st_stream_.rec_config_) {
                PAL_ERR(LOG_TAG, "Recognition config not set %d", data->restart_);
                status = -EINVAL;
                break;
            }

            /* Update cap dev based on mode and configuration and start it */
            struct pal_device dattr;
            bool backend_update = false;
            std::vector<std::shared_ptr<SoundTriggerEngine>> tmp_engines;
            std::shared_ptr<CaptureProfile> cap_prof = nullptr;

            /*
             * Update common capture profile only in:
             * 1. start recognition excuted
             * 2. resume excuted and current common capture profile is null
             */
            if (!st_stream_.common_cp_update_disable_ &&
                (ev_cfg->id_ == ST_EV_START_RECOGNITION ||
                (ev_cfg->id_ == ST_EV_RESUME &&
                !st_stream_.rm->GetSoundTriggerCaptureProfile()))) {
                backend_update = st_stream_.rm->UpdateSoundTriggerCaptureProfile(
                    &st_stream_, true);
                if (backend_update) {
                    status = rm->StopOtherDetectionStreams(&st_stream_);
                    if (status) {
                        PAL_ERR(LOG_TAG, "Failed to stop other SVA streams");
                    }

                    status = rm->StartOtherDetectionStreams(&st_stream_);
                    if (status) {
                        PAL_ERR(LOG_TAG, "Failed to start other SVA streams");
                    }
                }
            }

            if (st_stream_.mDevices.size() > 0) {
                auto& dev = st_stream_.mDevices[0];
                dev->getDeviceAttributes(&dattr);

                cap_prof = st_stream_.rm->GetSoundTriggerCaptureProfile();
                if (!cap_prof) {
                    PAL_ERR(LOG_TAG, "Invalid capture profile");
                    goto err_exit;
                }

                dattr.config.bit_width = cap_prof->GetBitWidth();
                dattr.config.ch_info.channels = cap_prof->GetChannels();
                dattr.config.sample_rate = cap_prof->GetSampleRate();
                dev->setDeviceAttributes(dattr);

                dev->setSndName(cap_prof->GetSndName());
                if (!st_stream_.device_opened_) {
                    status = dev->open();
                    if (0 != status) {
                        PAL_ERR(LOG_TAG, "Device open failed, status %d", status);
                        break;
                    }
                    st_stream_.device_opened_ = true;
                }
                /* now start the device */
                PAL_DBG(LOG_TAG, "Start device %d-%s", dev->getSndDeviceId(),
                        dev->getPALDeviceName().c_str());

                status = dev->start();
                if (0 != status) {
                    PAL_ERR(LOG_TAG, "Device start failed, status %d", status);
                    dev->close();
                    st_stream_.device_opened_ = false;
                    break;
                } else if (!rm->isDeviceActive_l(dev, &st_stream_)) {
                    st_stream_.rm->registerDevice(dev, &st_stream_);
                }
                PAL_DBG(LOG_TAG, "device started");
            }

            /* Start the engines */
            for (auto& eng: st_stream_.engines_) {
                PAL_VERBOSE(LOG_TAG, "Start st engine %d", eng->GetEngineId());
                status = eng->GetEngine()->StartRecognition(&st_stream_);
                if (0 != status) {
                    PAL_ERR(LOG_TAG, "Start st engine %d failed, status %d",
                            eng->GetEngineId(), status);
                    goto err_exit;
                } else {
                    tmp_engines.push_back(eng->GetEngine());
                }
            }

            if (st_stream_.reader_)
                st_stream_.reader_->reset();

            TransitTo(ST_STATE_ACTIVE);
            break;

        err_exit:
            for (auto& eng: tmp_engines)
                eng->StopRecognition(&st_stream_);

            if (st_stream_.mDevices.size() > 0) {
                if (rm->isDeviceActive_l(st_stream_.mDevices[0], &st_stream_))
                    st_stream_.rm->deregisterDevice(st_stream_.mDevices[0], &st_stream_);
                st_stream_.mDevices[0]->stop();
                st_stream_.mDevices[0]->close();
                st_stream_.device_opened_ = false;
            }

            break;
        }
        case ST_EV_PAUSE: {
            st_stream_.paused_ = true;
            break;
        }
        case ST_EV_READ_BUFFER: {
            status = -EIO;
            break;
        }
        case ST_EV_DEVICE_DISCONNECTED:{
            StDeviceDisconnectedEventConfigData *data =
                (StDeviceDisconnectedEventConfigData *)ev_cfg->data_.get();
            pal_device_id_t device_id = data->dev_id_;
            if (st_stream_.mDevices.size() == 0) {
                PAL_DBG(LOG_TAG, "No device to disconnect");
                break;
            } else {
                int curr_device_id = st_stream_.mDevices[0]->getSndDeviceId();
                pal_device_id_t curr_device =
                    static_cast<pal_device_id_t>(curr_device_id);
                if (curr_device != device_id) {
                    PAL_ERR(LOG_TAG, "Device %d not connected, ignore",
                        device_id);
                    break;
                }
            }
            for (auto& device: st_stream_.mDevices) {
                st_stream_.gsl_engine_->DisconnectSessionDevice(&st_stream_,
                    st_stream_.mStreamAttr->type, device);
                if (st_stream_.device_opened_) {
                    status = device->close();
                    if (0 != status) {
                        PAL_ERR(LOG_TAG, "device %d close failed with status %d",
                            device->getSndDeviceId(), status);
                    }
                    st_stream_.device_opened_ = false;
                }
            }
            st_stream_.mDevices.clear();
            break;
        }
        case ST_EV_DEVICE_CONNECTED: {
            std::shared_ptr<Device> dev = nullptr;
            StDeviceConnectedEventConfigData *data =
                (StDeviceConnectedEventConfigData *)ev_cfg->data_.get();
            pal_device_id_t dev_id = data->dev_id_;
            std::vector<std::shared_ptr<SoundTriggerEngine>> tmp_engines;

            // mDevices should be empty as we have just disconnected device
            if (st_stream_.mDevices.size() != 0) {
                PAL_ERR(LOG_TAG, "Invalid operation");
                status = -EINVAL;
                goto connect_err;
            }

            dev = st_stream_.GetPalDevice(&st_stream_, dev_id);
            if (!dev) {
                PAL_ERR(LOG_TAG, "Dev creation failed");
                status = -EINVAL;
                goto connect_err;
            }
            st_stream_.mDevices.push_back(dev);

            PAL_DBG(LOG_TAG, "Update capture profile and stream attr in device switch");
            st_stream_.cap_prof_ = st_stream_.GetCurrentCaptureProfile();
            st_stream_.mDevPPSelector = st_stream_.cap_prof_->GetName();
            PAL_DBG(LOG_TAG, "Devicepp Selector: %s",
                st_stream_.mDevPPSelector.c_str());
            st_stream_.updateStreamAttributes();

            status = st_stream_.gsl_engine_->SetupSessionDevice(&st_stream_,
                st_stream_.mStreamAttr->type, dev);
            if (0 != status) {
                PAL_ERR(LOG_TAG,
                        "setupSessionDevice for %d failed with status %d",
                        dev->getSndDeviceId(), status);
                st_stream_.mDevices.pop_back();
                goto connect_err;
            }

            if (!st_stream_.device_opened_) {
                status = dev->open();
                if (0 != status) {
                    PAL_ERR(LOG_TAG, "device %d open failed with status %d",
                        dev->getSndDeviceId(), status);
                    goto connect_err;
                }
                st_stream_.device_opened_ = true;
            }

            if (st_stream_.isActive() && !st_stream_.paused_) {
                status = dev->start();
                if (0 != status) {
                    PAL_ERR(LOG_TAG, "device %d start failed with status %d",
                        dev->getSndDeviceId(), status);
                    dev->close();
                    st_stream_.device_opened_ = false;
                    goto connect_err;
                }
            }

            status = st_stream_.gsl_engine_->ConnectSessionDevice(&st_stream_,
                st_stream_.mStreamAttr->type, dev);
            if (0 != status) {
                PAL_ERR(LOG_TAG,
                        "connectSessionDevice for %d failed with status %d",
                        dev->getSndDeviceId(), status);
                st_stream_.mDevices.pop_back();
                dev->close();
                st_stream_.device_opened_ = false;
            } else if (st_stream_.isActive() && !st_stream_.paused_) {
                if (!rm->isDeviceActive_l(dev, &st_stream_))
                    st_stream_.rm->registerDevice(dev, &st_stream_);
                if (st_stream_.second_stage_processing_) {
                    /* Start the engines */
                    for (auto& eng: st_stream_.engines_) {
                        PAL_VERBOSE(LOG_TAG, "Start st engine %d", eng->GetEngineId());
                        status = eng->GetEngine()->StartRecognition(&st_stream_);
                        if (0 != status) {
                            PAL_ERR(LOG_TAG, "Start st engine %d failed, status %d",
                                    eng->GetEngineId(), status);
                            goto err_start;
                        } else {
                            tmp_engines.push_back(eng->GetEngine());
                        }
                    }

                    if (st_stream_.reader_)
                        st_stream_.reader_->reset();
                    st_stream_.second_stage_processing_ = false;
                } else {
                    st_stream_.gsl_engine_->UpdateStateToActive();
                }
                TransitTo(ST_STATE_ACTIVE);
            }
            break;
        err_start:
            for (auto& eng: tmp_engines)
                eng->StopRecognition(&st_stream_);

            if (st_stream_.mDevices.size() > 0) {
                st_stream_.rm->deregisterDevice(st_stream_.mDevices[0], &st_stream_);
                st_stream_.mDevices[0]->stop();
                st_stream_.mDevices[0]->close();
                st_stream_.device_opened_ = false;
            }
        connect_err:
            break;
        }
        case ST_EV_CONCURRENT_STREAM: {
            std::shared_ptr<CaptureProfile> new_cap_prof = nullptr;
            bool active = false;

            if (ev_cfg->id_ == ST_EV_CONCURRENT_STREAM) {
                StConcurrentStreamEventConfigData *data =
                    (StConcurrentStreamEventConfigData *)ev_cfg->data_.get();
                active = data->is_active_;
            }
            new_cap_prof = st_stream_.GetCurrentCaptureProfile();
            if (new_cap_prof && (st_stream_.cap_prof_ != new_cap_prof)) {
                PAL_DBG(LOG_TAG,
                    "current capture profile %s: dev_id=0x%x, chs=%d, sr=%d, ec_ref=%d\n",
                    st_stream_.cap_prof_->GetName().c_str(),
                    st_stream_.cap_prof_->GetDevId(),
                    st_stream_.cap_prof_->GetChannels(),
                    st_stream_.cap_prof_->GetSampleRate(),
                    st_stream_.cap_prof_->isECRequired());
                PAL_DBG(LOG_TAG,
                    "new capture profile %s: dev_id=0x%x, chs=%d, sr=%d, ec_ref=%d\n",
                    new_cap_prof->GetName().c_str(),
                    new_cap_prof->GetDevId(),
                    new_cap_prof->GetChannels(),
                    new_cap_prof->GetSampleRate(),
                    new_cap_prof->isECRequired());
                if (!active) {
                    if (st_stream_.device_opened_ && st_stream_.mDevices.size() > 0) {
                        auto& dev = st_stream_.mDevices[0];
                        status = dev->close();
                        if (0 != status) {
                            PAL_ERR(LOG_TAG, "device %d close failed with status %d",
                                dev->getSndDeviceId(), status);
                        }
                        st_stream_.device_opened_ = false;
                    }
                    st_stream_.mDevices.clear();

                    status = st_stream_.gsl_engine_->ReconfigureDetectionGraph(&st_stream_);
                    if (0 != status) {
                        PAL_ERR(LOG_TAG, "Failed to reconfigure gsl engine, status %d",
                            status);
                        goto err_concurrent;
                    }
                    TransitTo(ST_STATE_IDLE);
                } else {
                    PAL_ERR(LOG_TAG, "Invalid operation");
                    status = -EINVAL;
                }
            } else {
                PAL_INFO(LOG_TAG,"no action needed, same capture profile");
            }
        err_concurrent:
            break;
        }
        case ST_EV_SSR_OFFLINE: {
            if (st_stream_.state_for_restore_ == ST_STATE_NONE) {
                st_stream_.state_for_restore_ = ST_STATE_LOADED;
            }
            std::shared_ptr<StEventConfig> ev_cfg(new StUnloadEventConfig());
            status = st_stream_.ProcessInternalEvent(ev_cfg);
            TransitTo(ST_STATE_SSR);
            break;
        }
        case ST_EV_EC_REF: {
            StECRefEventConfigData *data =
                (StECRefEventConfigData *)ev_cfg->data_.get();
            Stream *s = static_cast<Stream *>(&st_stream_);
            status = st_stream_.gsl_engine_->setECRef(s, data->dev_,
                data->is_enable_, st_stream_.ec_rx_dev_ == nullptr );
            if (status) {
                PAL_ERR(LOG_TAG, "Failed to set EC Ref in gsl engine");
            }
            break;
        }
        case ST_EV_DETECTED: {
            PAL_DBG(LOG_TAG,
                "Keyword detected with invalid state, stop engines");
            /*
                * When detection is ignored here, stop engines to make sure
                * engines are in proper state for next detection/start. For
                * multi VA cases, gsl engine stop is same as restart.
                */
            for (auto& eng: st_stream_.engines_) {
                PAL_VERBOSE(LOG_TAG, "Stop engine %d", eng->GetEngineId());
                status = eng->GetEngine()->StopRecognition(&st_stream_);
                if (status) {
                    PAL_ERR(LOG_TAG, "Stop engine %d failed, status %d",
                            eng->GetEngineId(), status);
                }
            }
            break;
        }
        default: {
            PAL_DBG(LOG_TAG, "Unhandled event %d", ev_cfg->id_);
            break;
        }
    }
    return status;
}

int32_t StreamSoundTrigger::StActive::ProcessEvent(
    std::shared_ptr<StEventConfig> ev_cfg) {

    int32_t status = 0;

    PAL_DBG(LOG_TAG, "StActive: handle event %d for stream instance %u",
        ev_cfg->id_, st_stream_.mInstanceID);

    switch (ev_cfg->id_) {
        case ST_EV_DETECTED: {
            StDetectedEventConfigData *data =
                (StDetectedEventConfigData *) ev_cfg->data_.get();
            if (data->det_type_ != GMM_DETECTED)
                break;
            if (!st_stream_.rec_config_->capture_requested &&
                st_stream_.engines_.size() == 1) {
                TransitTo(ST_STATE_DETECTED);
                if (st_stream_.GetCurrentStateId() == ST_STATE_DETECTED) {
                    st_stream_.PostDelayedStop();
                }
            } else {
                if (st_stream_.engines_.size() > 1)
                    st_stream_.second_stage_processing_ = true;
                TransitTo(ST_STATE_BUFFERING);
                st_stream_.SetDetectedToEngines(true);
            }
            if (st_stream_.engines_.size() == 1) {
                st_stream_.notifyClient(true);
            }
            break;
        }
        case ST_EV_PAUSE: {
            st_stream_.paused_ = true;
            // fall through to stop
            [[fallthrough]];
        }
        case ST_EV_UNLOAD_SOUND_MODEL:
        case ST_EV_STOP_RECOGNITION: {
            // Do not update capture profile when pausing stream
            bool backend_update = false;
            if (!st_stream_.common_cp_update_disable_ &&
                (ev_cfg->id_ == ST_EV_STOP_RECOGNITION ||
                ev_cfg->id_ == ST_EV_UNLOAD_SOUND_MODEL)) {
                backend_update = st_stream_.rm->UpdateSoundTriggerCaptureProfile(
                    &st_stream_, false);
                if (backend_update) {
                    status = rm->StopOtherDetectionStreams(&st_stream_);
                    if (status) {
                        PAL_ERR(LOG_TAG, "Failed to stop other SVA streams");
                    }
                }
            }

            if (st_stream_.mDevices.size() > 0) {
                auto& dev = st_stream_.mDevices[0];
                PAL_VERBOSE(LOG_TAG, "Deregister device %d-%s", dev->getSndDeviceId(),
                    dev->getPALDeviceName().c_str());
                if (rm->isDeviceActive_l(dev, &st_stream_))
                    st_stream_.rm->deregisterDevice(dev, &st_stream_);
            }
            for (auto& eng: st_stream_.engines_) {
                PAL_VERBOSE(LOG_TAG, "Stop engine %d", eng->GetEngineId());
                status = eng->GetEngine()->StopRecognition(&st_stream_);
                if (status) {
                    PAL_ERR(LOG_TAG, "Stop engine %d failed, status %d",
                            eng->GetEngineId(), status);
                }
            }
            if (st_stream_.mDevices.size() > 0) {
                auto& dev = st_stream_.mDevices[0];
                PAL_DBG(LOG_TAG, "Stop device %d-%s", dev->getSndDeviceId(),
                        dev->getPALDeviceName().c_str());
                status = dev->stop();
                if (status)
                    PAL_ERR(LOG_TAG, "Device stop failed, status %d", status);

                PAL_DBG(LOG_TAG, "Close device %d-%s", dev->getSndDeviceId(),
                        dev->getPALDeviceName().c_str());

                status = dev->close();
                st_stream_.device_opened_ = false;
                if (status)
                    PAL_ERR(LOG_TAG, "Device close failed, status %d", status);
            }

            if (backend_update) {
                status = rm->StartOtherDetectionStreams(&st_stream_);
                if (status) {
                    PAL_ERR(LOG_TAG, "Failed to start other SVA streams");
                }
            }
            TransitTo(ST_STATE_LOADED);
            if (ev_cfg->id_ == ST_EV_UNLOAD_SOUND_MODEL) {
                status = st_stream_.ProcessInternalEvent(ev_cfg);
                if (status != 0) {
                    PAL_ERR(LOG_TAG, "Failed to unload sound model, status = %d",
                            status);
                }
            }
            break;
        }
        case ST_EV_EC_REF: {
            StECRefEventConfigData *data =
                (StECRefEventConfigData *)ev_cfg->data_.get();
            Stream *s = static_cast<Stream *>(&st_stream_);
            status = st_stream_.gsl_engine_->setECRef(s, data->dev_,
                data->is_enable_, st_stream_.ec_rx_dev_ == nullptr);
            if (status) {
                PAL_ERR(LOG_TAG, "Failed to set EC Ref in gsl engine");
            }
            break;
        }
        case ST_EV_READ_BUFFER: {
            status = -EIO;
            break;
        }
        case ST_EV_DEVICE_DISCONNECTED: {
            StDeviceDisconnectedEventConfigData *data =
                (StDeviceDisconnectedEventConfigData *)ev_cfg->data_.get();
            pal_device_id_t device_id = data->dev_id_;
            if (st_stream_.mDevices.size() == 0) {
                PAL_DBG(LOG_TAG, "No device to disconnect");
                break;
            } else {
                int curr_device_id = st_stream_.mDevices[0]->getSndDeviceId();
                pal_device_id_t curr_device =
                    static_cast<pal_device_id_t>(curr_device_id);
                if (curr_device != device_id) {
                    PAL_ERR(LOG_TAG, "Device %d not connected, ignore",
                        device_id);
                    break;
                }
            }
            for (auto& device: st_stream_.mDevices) {
                if (rm->isDeviceActive_l(device, &st_stream_))
                    st_stream_.rm->deregisterDevice(device, &st_stream_);
                st_stream_.gsl_engine_->DisconnectSessionDevice(&st_stream_,
                    st_stream_.mStreamAttr->type, device);

                status = device->stop();
                if (0 != status) {
                    PAL_ERR(LOG_TAG, "device stop failed with status %d", status);
                    goto disconnect_err;
                }

                status = device->close();
                st_stream_.device_opened_ = false;
                if (0 != status) {
                    PAL_ERR(LOG_TAG, "device close failed with status %d", status);
                    goto disconnect_err;
                }
            }
        disconnect_err:
            st_stream_.mDevices.clear();
            break;
        }
        case ST_EV_DEVICE_CONNECTED: {
            std::shared_ptr<Device> dev = nullptr;
            StDeviceConnectedEventConfigData *data =
                (StDeviceConnectedEventConfigData *)ev_cfg->data_.get();
            pal_device_id_t dev_id = data->dev_id_;

            // mDevices should be empty as we have just disconnected device
            if (st_stream_.mDevices.size() != 0) {
                PAL_ERR(LOG_TAG, "Invalid operation");
                status = -EINVAL;
                goto connect_err;
            }

            dev = st_stream_.GetPalDevice(&st_stream_, dev_id);
            if (!dev) {
                PAL_ERR(LOG_TAG, "Device creation failed");
                status = -EINVAL;
                goto connect_err;
            }
            st_stream_.mDevices.push_back(dev);

            PAL_DBG(LOG_TAG, "Update capture profile and stream attr in device switch");
            st_stream_.cap_prof_ = st_stream_.GetCurrentCaptureProfile();
            st_stream_.mDevPPSelector = st_stream_.cap_prof_->GetName();
            PAL_DBG(LOG_TAG, "devicepp selector: %s",
                st_stream_.mDevPPSelector.c_str());
            st_stream_.updateStreamAttributes();

            status = st_stream_.gsl_engine_->SetupSessionDevice(&st_stream_,
                st_stream_.mStreamAttr->type, dev);
            if (0 != status) {
                PAL_ERR(LOG_TAG, "setupSessionDevice for %d failed with status %d",
                        dev->getSndDeviceId(), status);
                st_stream_.mDevices.pop_back();
                goto connect_err;
            }

            if (!st_stream_.device_opened_) {
                status = dev->open();
                if (0 != status) {
                    PAL_ERR(LOG_TAG, "device %d open failed with status %d",
                        dev->getSndDeviceId(), status);
                    goto connect_err;
                }
                st_stream_.device_opened_ = true;
            }

            status = dev->start();
            if (0 != status) {
                PAL_ERR(LOG_TAG, "device %d start failed with status %d",
                    dev->getSndDeviceId(), status);
                dev->close();
                st_stream_.device_opened_ = false;
                goto connect_err;
            }

            status = st_stream_.gsl_engine_->ConnectSessionDevice(&st_stream_,
                st_stream_.mStreamAttr->type, dev);
            if (0 != status) {
                PAL_ERR(LOG_TAG, "connectSessionDevice for %d failed with status %d",
                        dev->getSndDeviceId(), status);
                st_stream_.mDevices.pop_back();
                dev->close();
                st_stream_.device_opened_ = false;
            } else if (!rm->isDeviceActive_l(dev, &st_stream_)) {
                st_stream_.rm->registerDevice(dev, &st_stream_);
            }
        connect_err:
            break;
        }
        case ST_EV_CONCURRENT_STREAM: {
            std::shared_ptr<CaptureProfile> new_cap_prof = nullptr;
            bool active = false;

            if (ev_cfg->id_ == ST_EV_CONCURRENT_STREAM) {
                StConcurrentStreamEventConfigData *data =
                    (StConcurrentStreamEventConfigData *)ev_cfg->data_.get();
                active = data->is_active_;
            }
            new_cap_prof = st_stream_.GetCurrentCaptureProfile();
            if (new_cap_prof && (st_stream_.cap_prof_ != new_cap_prof)) {
                PAL_DBG(LOG_TAG,
                    "current capture profile %s: dev_id=0x%x, chs=%d, sr=%d, ec_ref=%d\n",
                    st_stream_.cap_prof_->GetName().c_str(),
                    st_stream_.cap_prof_->GetDevId(),
                    st_stream_.cap_prof_->GetChannels(),
                    st_stream_.cap_prof_->GetSampleRate(),
                    st_stream_.cap_prof_->isECRequired());
                PAL_DBG(LOG_TAG,
                    "new capture profile %s: dev_id=0x%x, chs=%d, sr=%d, ec_ref=%d\n",
                    new_cap_prof->GetName().c_str(),
                    new_cap_prof->GetDevId(),
                    new_cap_prof->GetChannels(),
                    new_cap_prof->GetSampleRate(),
                    new_cap_prof->isECRequired());
                if (!active) {
                    std::shared_ptr<StEventConfig> ev_cfg1(
                        new StStopRecognitionEventConfig(false));
                    status = st_stream_.ProcessInternalEvent(ev_cfg1);
                    if (status) {
                        PAL_ERR(LOG_TAG, "Failed to Stop, status %d", status);
                        break;
                    }

                    status = st_stream_.ProcessInternalEvent(ev_cfg);
                    if (status) {
                        PAL_ERR(LOG_TAG, "Failed to Unload, status %d", status);
                        break;
                    }
                } else {
                    PAL_ERR(LOG_TAG, "Invalid operation");
                    status = -EINVAL;
                }
            } else {
                PAL_INFO(LOG_TAG,"no action needed, same capture profile");
            }
            break;
        }
        case ST_EV_SSR_OFFLINE: {
            if (st_stream_.state_for_restore_ == ST_STATE_NONE) {
                st_stream_.state_for_restore_ = ST_STATE_ACTIVE;
            }
            std::shared_ptr<StEventConfig> ev_cfg1(
                new StStopRecognitionEventConfig(false));
            status = st_stream_.ProcessInternalEvent(ev_cfg1);

            std::shared_ptr<StEventConfig> ev_cfg2(
                new StUnloadEventConfig());
            status = st_stream_.ProcessInternalEvent(ev_cfg2);
            TransitTo(ST_STATE_SSR);
            break;
        }
        default: {
            PAL_DBG(LOG_TAG, "Unhandled event %d", ev_cfg->id_);
            break;
        }
    }
    return status;
}

int32_t StreamSoundTrigger::StDetected::ProcessEvent(
    std::shared_ptr<StEventConfig> ev_cfg) {
    int32_t status = 0;

    PAL_DBG(LOG_TAG, "StDetected: handle event %d for stream instance %u",
        ev_cfg->id_, st_stream_.mInstanceID);

    switch (ev_cfg->id_) {
        case ST_EV_START_RECOGNITION: {
            // Client restarts next recognition without config changed.
            st_stream_.CancelDelayedStop();

            for (auto& eng: st_stream_.engines_) {
                PAL_VERBOSE(LOG_TAG, "Restart engine %d", eng->GetEngineId());
                status = eng->GetEngine()->RestartRecognition(&st_stream_);
                if (status) {
                    PAL_ERR(LOG_TAG, "Restart engine %d failed, status %d",
                            eng->GetEngineId(), status);
                }
            }
            if (st_stream_.reader_)
                st_stream_.reader_->reset();
            if (!status) {
                TransitTo(ST_STATE_ACTIVE);
            } else {
                TransitTo(ST_STATE_LOADED);
            }
            rm->releaseWakeLock();
            break;
        }
        case ST_EV_PAUSE: {
            st_stream_.CancelDelayedStop();
            st_stream_.paused_ = true;
            // fall through to stop
            [[fallthrough]];
        }
        case ST_EV_UNLOAD_SOUND_MODEL:
        case ST_EV_STOP_RECOGNITION: {
            if (st_stream_.mDevices.size() > 0) {
                auto& dev = st_stream_.mDevices[0];
                PAL_VERBOSE(LOG_TAG, "Deregister device %d-%s", dev->getSndDeviceId(),
                    dev->getPALDeviceName().c_str());
                if (rm->isDeviceActive_l(dev, &st_stream_))
                    st_stream_.rm->deregisterDevice(dev, &st_stream_);
            }

            st_stream_.CancelDelayedStop();
            for (auto& eng: st_stream_.engines_) {
                PAL_VERBOSE(LOG_TAG, "Stop engine %d", eng->GetEngineId());
                status = eng->GetEngine()->StopRecognition(&st_stream_);
                if (status) {
                    PAL_ERR(LOG_TAG, "Stop engine %d failed, status %d",
                            eng->GetEngineId(), status);
                }
            }
            if (st_stream_.mDevices.size() > 0) {
                auto& dev = st_stream_.mDevices[0];
                PAL_DBG(LOG_TAG, "Stop device %d-%s", dev->getSndDeviceId(),
                        dev->getPALDeviceName().c_str());
                status = dev->stop();
                if (status)
                    PAL_ERR(LOG_TAG, "Device stop failed, status %d", status);

                status = dev->close();
                st_stream_.device_opened_ = false;
                if (status)
                    PAL_ERR(LOG_TAG, "Device close failed, status %d", status);
            }
            TransitTo(ST_STATE_LOADED);

            if (ev_cfg->id_ == ST_EV_UNLOAD_SOUND_MODEL) {
                status = st_stream_.ProcessInternalEvent(ev_cfg);
                if (status != 0) {
                    PAL_ERR(LOG_TAG, "Failed to unload sound model, status = %d",
                            status);
                }
            }
            rm->releaseWakeLock();
            break;
        }
        case ST_EV_RECOGNITION_CONFIG: {
            StRecognitionCfgEventConfigData *data =
                (StRecognitionCfgEventConfigData *)ev_cfg->data_.get();
            if (st_stream_.compareRecognitionConfig(st_stream_.rec_config_,
                    (struct pal_st_recognition_config *)data->data_)) {
                PAL_DBG(LOG_TAG, "Same recognition config, skip update");
                break;
            }
            if (st_stream_.mDevices.size() > 0) {
                auto& dev = st_stream_.mDevices[0];
                PAL_VERBOSE(LOG_TAG, "Deregister device %d-%s", dev->getSndDeviceId(),
                    dev->getPALDeviceName().c_str());
                if (rm->isDeviceActive_l(dev, &st_stream_))
                    st_stream_.rm->deregisterDevice(dev, &st_stream_);
            }
            /*
             * Client can update config for next recognition.
             * Get to loaded state as START event will start recognition.
             */
            st_stream_.CancelDelayedStop();

            for (auto& eng: st_stream_.engines_) {
                PAL_VERBOSE(LOG_TAG, "Stop engine %d", eng->GetEngineId());
                status = eng->GetEngine()->StopRecognition(&st_stream_);
                if (status) {
                    PAL_ERR(LOG_TAG, "Stop engine %d failed, status %d",
                            eng->GetEngineId(), status);
                }
            }
            if (st_stream_.mDevices.size() > 0) {
                auto& dev = st_stream_.mDevices[0];
                PAL_DBG(LOG_TAG, "Stop device %d-%s", dev->getSndDeviceId(),
                        dev->getPALDeviceName().c_str());
                status = dev->stop();
                if (status)
                    PAL_ERR(LOG_TAG, "Device stop failed, status %d", status);

                status = dev->close();
                st_stream_.device_opened_ = false;
                if (status)
                    PAL_ERR(LOG_TAG, "Device close failed, status %d", status);
            }
            TransitTo(ST_STATE_LOADED);
            status = st_stream_.ProcessInternalEvent(ev_cfg);
            if (status) {
                PAL_ERR(LOG_TAG, "Failed to handle recognition config, status %d",
                        status);
            }
            rm->releaseWakeLock();
            // START event will be handled in loaded state.
            break;
        }
        case ST_EV_RESUME: {
            st_stream_.paused_ = false;
            break;
        }
        case ST_EV_CONCURRENT_STREAM:
        case ST_EV_DEVICE_DISCONNECTED:
        case ST_EV_DEVICE_CONNECTED: {
            if (st_stream_.mDevices.size() > 0) {
                auto& dev = st_stream_.mDevices[0];
                PAL_VERBOSE(LOG_TAG, "Deregister device %d-%s", dev->getSndDeviceId(),
                    dev->getPALDeviceName().c_str());
                if (rm->isDeviceActive_l(dev, &st_stream_))
                    st_stream_.rm->deregisterDevice(dev, &st_stream_);
            }

            st_stream_.CancelDelayedStop();
            for (auto& eng: st_stream_.engines_) {
                PAL_VERBOSE(LOG_TAG, "Stop engine %d", eng->GetEngineId());
                status = eng->GetEngine()->StopRecognition(&st_stream_);
                if (status) {
                    PAL_ERR(LOG_TAG, "Stop engine %d failed, status %d",
                            eng->GetEngineId(), status);
                }
            }
            if (st_stream_.mDevices.size() > 0) {
                auto& dev = st_stream_.mDevices[0];
                PAL_DBG(LOG_TAG, "Stop device %d-%s", dev->getSndDeviceId(),
                        dev->getPALDeviceName().c_str());
                status = dev->stop();
                if (status)
                    PAL_ERR(LOG_TAG, "Device stop failed, status %d", status);

                status = dev->close();
                st_stream_.device_opened_ = false;
                if (0 != status)
                    PAL_ERR(LOG_TAG, "device close failed with status %d", status);
            }
            TransitTo(ST_STATE_LOADED);
            status = st_stream_.ProcessInternalEvent(ev_cfg);
            if (status) {
                PAL_ERR(LOG_TAG, "Failed to handle device connection, status %d",
                        status);
            }
            rm->releaseWakeLock();
            break;
        }
        case ST_EV_SSR_OFFLINE: {
            if (st_stream_.state_for_restore_ == ST_STATE_NONE) {
                st_stream_.state_for_restore_ = ST_STATE_LOADED;
            }
            std::shared_ptr<StEventConfig> ev_cfg1(
                new StStopRecognitionEventConfig(false));
            status = st_stream_.ProcessInternalEvent(ev_cfg1);

            std::shared_ptr<StEventConfig> ev_cfg2(
                new StUnloadEventConfig());
            status = st_stream_.ProcessInternalEvent(ev_cfg2);
            TransitTo(ST_STATE_SSR);
            break;
        }
        case ST_EV_EC_REF: {
            StECRefEventConfigData *data =
                (StECRefEventConfigData *)ev_cfg->data_.get();
            Stream *s = static_cast<Stream *>(&st_stream_);
            status = st_stream_.gsl_engine_->setECRef(s, data->dev_,
                data->is_enable_, st_stream_.ec_rx_dev_ == nullptr);
            if (status) {
                PAL_ERR(LOG_TAG, "Failed to set EC Ref in gsl engine");
            }
            break;
        }
        default: {
            PAL_DBG(LOG_TAG, "Unhandled event %d", ev_cfg->id_);
            break;
        }
    }
    return status;
}

int32_t StreamSoundTrigger::StBuffering::ProcessEvent(
   std::shared_ptr<StEventConfig> ev_cfg) {
    int32_t status = 0;

#ifdef SEC_AUDIO_ADD_FOR_DEBUG
    if (ev_cfg->id_ != ST_EV_READ_BUFFER)
#endif
    PAL_DBG(LOG_TAG, "StBuffering: handle event %d for stream instance %u",
        ev_cfg->id_, st_stream_.mInstanceID);

    switch (ev_cfg->id_) {
        case ST_EV_READ_BUFFER: {
            StReadBufferEventConfigData *data =
                (StReadBufferEventConfigData *)ev_cfg->data_.get();
            struct pal_buffer *buf = (struct pal_buffer *)data->data_;

            if (!st_stream_.reader_) {
                PAL_ERR(LOG_TAG, "no reader exists");
                status = -EINVAL;
                break;
            }
            status = st_stream_.reader_->read(buf->buffer, buf->size);
            if (st_stream_.vui_ptfm_info_->GetEnableDebugDumps()) {
                ST_DBG_FILE_WRITE(st_stream_.lab_fd_, buf->buffer, buf->size);
            }
            break;
        }
        case ST_EV_STOP_BUFFERING: {
            /*
             * Buffering continues in GSL engine side until RestartRecognition
             * called, to avoid ADSP stuck if client takes some time to send
             * start after stop buffering.
             */
            if (st_stream_.force_nlpi_vote) {
                rm->voteSleepMonitor(&st_stream_, false, true);
                st_stream_.force_nlpi_vote = false;
            }
            if (st_stream_.reader_)
                st_stream_.reader_->updateState(READER_DISABLED);

            // post delayed stop in case client does not send next start
            st_stream_.PostDelayedStop();
            break;
        }
        case ST_EV_START_RECOGNITION: {
            /*
             * Can happen if client requests next recognition without any config
             * change with/without reading buffers after sending detection event.
             */
            if (st_stream_.force_nlpi_vote) {
                rm->voteSleepMonitor(&st_stream_, false, true);
                st_stream_.force_nlpi_vote = false;
            }
            StStartRecognitionEventConfigData *data =
                (StStartRecognitionEventConfigData *)ev_cfg->data_.get();
            PAL_DBG(LOG_TAG, "StBuffering: start recognition, is restart %d",
                    data->restart_);
            st_stream_.CancelDelayedStop();

            for (auto& eng: st_stream_.engines_) {
                PAL_VERBOSE(LOG_TAG, "Restart engine %d", eng->GetEngineId());
                status = eng->GetEngine()->RestartRecognition(&st_stream_);
                if (status) {
                    PAL_ERR(LOG_TAG, "Restart engine %d buffering failed, status %d",
                            eng->GetEngineId(), status);
                    break;
                }
            }
            if (st_stream_.reader_)
                st_stream_.reader_->reset();
            if (!status) {
                TransitTo(ST_STATE_ACTIVE);
            } else {
                TransitTo(ST_STATE_LOADED);
            }
            rm->releaseWakeLock();
            break;
        }
        case ST_EV_RECOGNITION_CONFIG: {
            StRecognitionCfgEventConfigData *data =
                (StRecognitionCfgEventConfigData *)ev_cfg->data_.get();
            if (st_stream_.compareRecognitionConfig(st_stream_.rec_config_,
                    (struct pal_st_recognition_config *)data->data_)) {
                PAL_DBG(LOG_TAG, "Same recognition config, skip update");
                break;
            }
            if (st_stream_.mDevices.size() > 0) {
                auto& dev = st_stream_.mDevices[0];
                PAL_VERBOSE(LOG_TAG, "Deregister device %d-%s", dev->getSndDeviceId(),
                    dev->getPALDeviceName().c_str());
                if (rm->isDeviceActive_l(dev, &st_stream_))
                    st_stream_.rm->deregisterDevice(dev, &st_stream_);
            }

            /*
             * Can happen if client doesn't read buffers after sending detection
             * event, but requests next recognition with config change.
             * Get to loaded state as START event will start the recognition.
             */
             if (st_stream_.force_nlpi_vote) {
                 rm->voteSleepMonitor(&st_stream_, false, true);
                 st_stream_.force_nlpi_vote = false;
            }
            st_stream_.CancelDelayedStop();

            for (auto& eng: st_stream_.engines_) {
                PAL_VERBOSE(LOG_TAG, "Stop engine %d", eng->GetEngineId());
                status = eng->GetEngine()->StopRecognition(&st_stream_);
                if (status) {
                    PAL_ERR(LOG_TAG, "Stop engine %d failed, status %d",
                            eng->GetEngineId(), status);
                }
            }
            if (st_stream_.reader_) {
              st_stream_.reader_->reset();
            }
            if (st_stream_.mDevices.size() > 0) {
                auto& dev = st_stream_.mDevices[0];
                PAL_DBG(LOG_TAG, "Stop device %d-%s", dev->getSndDeviceId(),
                        dev->getPALDeviceName().c_str());
                status = dev->stop();
                if (status)
                    PAL_ERR(LOG_TAG, "Device stop failed, status %d", status);

                status = dev->close();
                st_stream_.device_opened_ = false;
                if (0 != status)
                    PAL_ERR(LOG_TAG, "device close failed with status %d", status);
            }
            TransitTo(ST_STATE_LOADED);
            status = st_stream_.ProcessInternalEvent(ev_cfg);
            if (status) {
                PAL_ERR(LOG_TAG, "Failed to handle recognition config, status %d",
                        status);
            }
            rm->releaseWakeLock();
            // START event will be handled in loaded state.
            break;
        }
        case ST_EV_PAUSE: {
            st_stream_.paused_ = true;
            PAL_DBG(LOG_TAG, "StBuffering: Pause");
            // fall through to stop
            [[fallthrough]];
        }
        case ST_EV_UNLOAD_SOUND_MODEL:
        case ST_EV_STOP_RECOGNITION:  {
            if (st_stream_.mDevices.size() > 0) {
                auto& dev = st_stream_.mDevices[0];
                PAL_VERBOSE(LOG_TAG, "Deregister device %d-%s", dev->getSndDeviceId(),
                    dev->getPALDeviceName().c_str());
                if (rm->isDeviceActive_l(dev, &st_stream_))
                    st_stream_.rm->deregisterDevice(dev, &st_stream_);
            }

            // Possible with deffered stop if client doesn't start next recognition.
            if (st_stream_.force_nlpi_vote) {
                rm->voteSleepMonitor(&st_stream_, false, true);
                st_stream_.force_nlpi_vote = false;
            }
            st_stream_.CancelDelayedStop();

            for (auto& eng: st_stream_.engines_) {
                PAL_VERBOSE(LOG_TAG, "Stop engine %d", eng->GetEngineId());
                status = eng->GetEngine()->StopRecognition(&st_stream_);
                if (status) {
                    PAL_ERR(LOG_TAG, "Stop engine %d failed, status %d",
                            eng->GetEngineId(), status);
                }
            }
            if (st_stream_.reader_) {
                st_stream_.reader_->reset();
            }
            if (st_stream_.mDevices.size() > 0) {
                auto& dev = st_stream_.mDevices[0];
                PAL_DBG(LOG_TAG, "Stop device %d-%s", dev->getSndDeviceId(),
                        dev->getPALDeviceName().c_str());
                status = dev->stop();
                if (status)
                    PAL_ERR(LOG_TAG, "Device stop failed, status %d", status);

                status = dev->close();
                st_stream_.device_opened_ = false;
                if (status)
                    PAL_ERR(LOG_TAG, "Device close failed, status %d", status);
            }
            TransitTo(ST_STATE_LOADED);
            if (ev_cfg->id_ == ST_EV_UNLOAD_SOUND_MODEL) {
                status = st_stream_.ProcessInternalEvent(ev_cfg);
                if (status != 0) {
                    PAL_ERR(LOG_TAG, "Failed to unload sound model, status = %d",
                            status);
                }
            }
            rm->releaseWakeLock();
            break;
        }
        case ST_EV_DETECTED: {
            // Second stage detections fall here.
            StDetectedEventConfigData *data =
                (StDetectedEventConfigData *)ev_cfg->data_.get();
            if (data->det_type_ == GMM_DETECTED) {
                break;
            }
            // If second stage has rejected, stop buffering and restart recognition
            if (data->det_type_ == KEYWORD_DETECTION_REJECT ||
                data->det_type_ == USER_VERIFICATION_REJECT) {
                if (st_stream_.rejection_notified_) {
                    PAL_DBG(LOG_TAG, "Already notified client with second stage rejection");
                    break;
                }

                PAL_DBG(LOG_TAG, "Second stage rejected, type %d",
                        data->det_type_);

                for (auto& eng : st_stream_.engines_) {
                    if ((data->det_type_ == USER_VERIFICATION_REJECT &&
                        eng->GetEngineId() & ST_SM_ID_SVA_S_STAGE_KWD) ||
                        (data->det_type_ == KEYWORD_DETECTION_REJECT &&
                        eng->GetEngineId() & ST_SM_ID_SVA_S_STAGE_USER)) {

                        status = eng->GetEngine()->StopRecognition(&st_stream_);
                        if (status) {
                            PAL_ERR(LOG_TAG, "Failed to stop recognition for engines");
                        }
                    }
                }
                st_stream_.second_stage_processing_ = false;
                st_stream_.detection_state_ = ENGINE_IDLE;

                if (st_stream_.reader_) {
                    st_stream_.reader_->reset();
                }

                if (st_stream_.vui_ptfm_info_->GetNotifySecondStageFailure()) {
                    st_stream_.rejection_notified_ = true;
                    st_stream_.notifyClient(false);
                    if (!st_stream_.rec_config_->capture_requested &&
                         st_stream_.GetCurrentStateId() == ST_STATE_BUFFERING)
                    st_stream_.PostDelayedStop();
                } else {
                    PAL_DBG(LOG_TAG, "Notification for second stage rejection is disabled");
                    for (auto& eng : st_stream_.engines_) {
                        status = eng->GetEngine()->RestartRecognition(&st_stream_);
                        if (status) {
                            PAL_ERR(LOG_TAG, "Restart engine %d failed, status %d",
                                  eng->GetEngineId(), status);
                            break;
                        }
                    }
                    if (!status) {
                        TransitTo(ST_STATE_ACTIVE);
                    } else {
                        TransitTo(ST_STATE_LOADED);
                    }
                }
                rm->releaseWakeLock();
                break;
            }
            if (data->det_type_ == KEYWORD_DETECTION_SUCCESS ||
                data->det_type_ == USER_VERIFICATION_SUCCESS) {
                st_stream_.detection_state_ |=  data->det_type_;
            }
            // notify client until both keyword detection/user verification done
            if (st_stream_.detection_state_ == st_stream_.notification_state_) {
                PAL_DBG(LOG_TAG, "Second stage detected");
                st_stream_.second_stage_processing_ = false;
                st_stream_.detection_state_ = ENGINE_IDLE;
                if (!st_stream_.rec_config_->capture_requested) {
                    if (st_stream_.reader_) {
                        st_stream_.reader_->reset();
                    }
                    TransitTo(ST_STATE_DETECTED);
                }
                st_stream_.notifyClient(true);
                if (!st_stream_.rec_config_->capture_requested &&
                    (st_stream_.GetCurrentStateId() == ST_STATE_BUFFERING ||
                     st_stream_.GetCurrentStateId() == ST_STATE_DETECTED)) {
                    st_stream_.PostDelayedStop();
                }
            }
            break;
        }
        case ST_EV_CONCURRENT_STREAM:
        case ST_EV_DEVICE_DISCONNECTED:
        case ST_EV_DEVICE_CONNECTED: {
            if (st_stream_.force_nlpi_vote) {
                rm->voteSleepMonitor(&st_stream_, false, true);
                st_stream_.force_nlpi_vote = false;
            }
            if (st_stream_.mDevices.size() > 0) {
                auto& dev = st_stream_.mDevices[0];
                PAL_VERBOSE(LOG_TAG, "Deregister device %d-%s", dev->getSndDeviceId(),
                    dev->getPALDeviceName().c_str());
                if (rm->isDeviceActive_l(dev, &st_stream_))
                    st_stream_.rm->deregisterDevice(dev, &st_stream_);
            }

            st_stream_.CancelDelayedStop();

            for (auto& eng: st_stream_.engines_) {
                PAL_VERBOSE(LOG_TAG, "Stop engine %d", eng->GetEngineId());
                status = eng->GetEngine()->StopRecognition(&st_stream_);
                if (status) {
                    PAL_ERR(LOG_TAG, "Stop engine %d failed, status %d",
                            eng->GetEngineId(), status);
                }
            }
            if (st_stream_.reader_) {
                st_stream_.reader_->reset();
            }
            if (st_stream_.mDevices.size() > 0) {
                auto& dev = st_stream_.mDevices[0];
                PAL_DBG(LOG_TAG, "Stop device %d-%s", dev->getSndDeviceId(),
                        dev->getPALDeviceName().c_str());
                status = dev->stop();
                if (status)
                    PAL_ERR(LOG_TAG, "Device stop failed, status %d", status);

                status = dev->close();
                st_stream_.device_opened_ = false;
                if (status)
                    PAL_ERR(LOG_TAG, "Device close failed, status %d", status);
            }
            TransitTo(ST_STATE_LOADED);
            status = st_stream_.ProcessInternalEvent(ev_cfg);
            if (status) {
                PAL_ERR(LOG_TAG, "Failed to handle device connection, status %d",
                        status);
            }
            rm->releaseWakeLock();
            // device connection event will be handled in loaded state.
            break;
        }
        case ST_EV_SSR_OFFLINE: {
            if (st_stream_.state_for_restore_ == ST_STATE_NONE) {
                if (st_stream_.second_stage_processing_)
                    st_stream_.state_for_restore_ = ST_STATE_ACTIVE;
                else
                    st_stream_.state_for_restore_ = ST_STATE_LOADED;
            }

            std::shared_ptr<StEventConfig> ev_cfg2(
                new StStopRecognitionEventConfig(false));
            status = st_stream_.ProcessInternalEvent(ev_cfg2);

            std::shared_ptr<StEventConfig> ev_cfg3(
                new StUnloadEventConfig());
            status = st_stream_.ProcessInternalEvent(ev_cfg3);
            TransitTo(ST_STATE_SSR);
            break;
        }
        case ST_EV_EC_REF: {
            StECRefEventConfigData *data =
                (StECRefEventConfigData *)ev_cfg->data_.get();
            Stream *s = static_cast<Stream *>(&st_stream_);
            status = st_stream_.gsl_engine_->setECRef(s, data->dev_,
                data->is_enable_, st_stream_.ec_rx_dev_ == nullptr);
            if (status) {
                PAL_ERR(LOG_TAG, "Failed to set EC Ref in gsl engine");
            }
            break;
        }
        default: {
            PAL_DBG(LOG_TAG, "Unhandled event %d", ev_cfg->id_);
            break;
        }
    }
    return status;
}

int32_t StreamSoundTrigger::StSSR::ProcessEvent(
   std::shared_ptr<StEventConfig> ev_cfg) {
    int32_t status = 0;

    PAL_DBG(LOG_TAG, "StSSR: handle event %d for stream instance %u",
        ev_cfg->id_, st_stream_.mInstanceID);

    switch (ev_cfg->id_) {
        case ST_EV_SSR_ONLINE: {
            TransitTo(ST_STATE_IDLE);
            /*
             * sm_config_ can be NULL if load sound model is failed in
             * previous SSR online event. This scenario can occur if
             * back to back SSR happens in less than 1 sec.
             */
            if (!st_stream_.sm_config_) {
                PAL_ERR(LOG_TAG, "sound model config is NULL");
                break;
            }
            PAL_INFO(LOG_TAG, "stream state for restore %d", st_stream_.state_for_restore_);
            /*
             * For concurrent stream handling, ST state transition
             * occurs from loaded/active to idle and then back to loaded/active.
             * While transitioning back to loaded/active, in case of any failures
             * because of ongoing SSR or other failures restore state remains in idle
             * and sm cfg will be non NULL in this scenario.
             * In this case, reset the state_for_restore based on actual stream state
             * for recovery to happen properly after SSR.
             */
            if (st_stream_.state_for_restore_ == ST_STATE_IDLE) {
                if (st_stream_.currentState == STREAM_STARTED)
                    st_stream_.state_for_restore_ = ST_STATE_ACTIVE;
                else if (st_stream_.currentState == STREAM_OPENED ||
                         st_stream_.currentState == STREAM_STOPPED)
                    st_stream_.state_for_restore_ = ST_STATE_LOADED;
            }
            if (st_stream_.state_for_restore_ == ST_STATE_LOADED ||
                st_stream_.state_for_restore_ == ST_STATE_ACTIVE) {
                std::shared_ptr<StEventConfig> ev_cfg1(
                    new StLoadEventConfig(st_stream_.sm_config_));
                status = st_stream_.ProcessInternalEvent(ev_cfg1);
                if (0 != status) {
                    PAL_ERR(LOG_TAG, "Failed to load sound model, status %d",
                        status);
                    break;
                }
                if (st_stream_.rec_config_) {
                    status = st_stream_.SendRecognitionConfig(
                             st_stream_.rec_config_);
                    if (0 != status) {
                        PAL_ERR(LOG_TAG,
                        "Failed to send recognition config, status %d", status);
                        break;
                    }
                }
            }

            if (st_stream_.state_for_restore_ == ST_STATE_ACTIVE) {
                std::shared_ptr<StEventConfig> ev_cfg2(
                    new StStartRecognitionEventConfig(false));
                status = st_stream_.ProcessInternalEvent(ev_cfg2);
                if (0 != status) {
                    PAL_ERR(LOG_TAG, "Failed to Start, status %d", status);
                    break;
                }
            }
            PAL_DBG(LOG_TAG, "StSSR: event %d handled", ev_cfg->id_);
            st_stream_.state_for_restore_ = ST_STATE_NONE;
            break;
        }
        case ST_EV_LOAD_SOUND_MODEL: {
            if (st_stream_.state_for_restore_ != ST_STATE_IDLE) {
                PAL_ERR(LOG_TAG, "Invalid operation, client state = %d now",
                    st_stream_.state_for_restore_);
                status = -EINVAL;
            } else {
                StLoadEventConfigData *data =
                    (StLoadEventConfigData *)ev_cfg->data_.get();
                status = st_stream_.UpdateSoundModel(
                    (struct pal_st_sound_model *)data->data_);
                if (0 != status) {
                    PAL_ERR(LOG_TAG, "Failed to update sound model, status %d",
                        status);
                } else {
                    st_stream_.state_for_restore_ = ST_STATE_LOADED;
                }
            }
            break;
        }
        case ST_EV_UNLOAD_SOUND_MODEL: {
            if (st_stream_.state_for_restore_ != ST_STATE_LOADED) {
                PAL_ERR(LOG_TAG, "Invalid operation, client state = %d now",
                    st_stream_.state_for_restore_);
                status = -EINVAL;
            } else {
                st_stream_.state_for_restore_ = ST_STATE_IDLE;
            }
            if (st_stream_.vui_intf_)
                st_stream_.vui_intf_->DeregisterModel(&st_stream_);
            break;
        }
        case ST_EV_RECOGNITION_CONFIG: {
            if (st_stream_.state_for_restore_ != ST_STATE_LOADED) {
                PAL_ERR(LOG_TAG, "Invalid operation, client state = %d now",
                    st_stream_.state_for_restore_);
                status = -EINVAL;
            } else {
                StRecognitionCfgEventConfigData *data =
                    (StRecognitionCfgEventConfigData *)ev_cfg->data_.get();
                status = st_stream_.UpdateRecognitionConfig(
                    (struct pal_st_recognition_config *)data->data_);
                if (0 != status) {
                    PAL_ERR(LOG_TAG, "Failed to update recognition config,"
                        "status %d", status);
                }
            }
            break;
        }
        case ST_EV_START_RECOGNITION: {
            if (st_stream_.state_for_restore_ != ST_STATE_LOADED) {
                PAL_ERR(LOG_TAG, "Invalid operation, client state = %d now",
                    st_stream_.state_for_restore_);
                status = -EINVAL;
            } else {
                StStartRecognitionEventConfigData *data =
                    (StStartRecognitionEventConfigData *)ev_cfg->data_.get();
                if (!st_stream_.rec_config_) {
                    PAL_ERR(LOG_TAG, "Recognition config not set %d", data->restart_);
                    status = -EINVAL;
                    break;
                }
                st_stream_.state_for_restore_ = ST_STATE_ACTIVE;
            }
            break;
        }
        case ST_EV_STOP_RECOGNITION: {
            if (st_stream_.state_for_restore_ != ST_STATE_ACTIVE) {
                PAL_ERR(LOG_TAG, "Invalid operation, client state = %d now",
                    st_stream_.state_for_restore_);
                status = -EINVAL;
            } else {
                st_stream_.state_for_restore_ = ST_STATE_LOADED;
            }
            break;
        }
        case ST_EV_PAUSE: {
            st_stream_.paused_ = true;
            break;
        }
        case ST_EV_RESUME: {
            if (st_stream_.paused_) {
                if (st_stream_.currentState == STREAM_STARTED)
                    st_stream_.state_for_restore_ = ST_STATE_ACTIVE;
                st_stream_.paused_ = false;
            }
            break;
        }
        case ST_EV_READ_BUFFER:
            status = -EIO;
            break;
        default: {
            PAL_DBG(LOG_TAG, "Unhandled event %d", ev_cfg->id_);
            break;
        }
    }

    return status;
}

int32_t StreamSoundTrigger::ssrDownHandler() {
    int32_t status = 0;

    std::lock_guard<std::mutex> lck(mStreamMutex);
    common_cp_update_disable_ = true;
    std::shared_ptr<StEventConfig> ev_cfg(new StSSROfflineConfig());
    status = cur_state_->ProcessEvent(ev_cfg);
    common_cp_update_disable_ = false;

    return status;
}

int32_t StreamSoundTrigger::ssrUpHandler() {
    int32_t status = 0;

    std::lock_guard<std::mutex> lck(mStreamMutex);
    common_cp_update_disable_ = true;
    std::shared_ptr<StEventConfig> ev_cfg(new StSSROnlineConfig());
    status = cur_state_->ProcessEvent(ev_cfg);
    common_cp_update_disable_ = false;

    return status;
}

struct st_uuid StreamSoundTrigger::GetVendorUuid()
{
    struct st_uuid uuid;
    if (sm_config_) {
        return sm_config_->vendor_uuid;
    }
    memset(&uuid, 0, sizeof(uuid));
    return uuid;
}
