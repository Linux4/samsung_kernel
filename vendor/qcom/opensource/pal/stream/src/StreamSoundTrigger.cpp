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

#define LOG_TAG "PAL: StreamSoundTrigger"

#include "StreamSoundTrigger.h"

#include <chrono>
#include <unistd.h>

#include "Session.h"
#include "SessionAlsaPcm.h"
#include "ResourceManager.h"
#include "Device.h"
#include "kvh2xml.h"

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
    inBufSize = BUF_SIZE_CAPTURE;
    outBufSize = BUF_SIZE_PLAYBACK;
    inBufCount = NO_OF_BUF;
    outBufCount = NO_OF_BUF;
    model_id_ = 0;
    sm_config_ = nullptr;
    rec_config_ = nullptr;
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
    mDevices.clear();
    mPalDevice.clear();

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

    charging_state_ = rm->GetChargingState();
    PAL_DBG(LOG_TAG, "Charging State %d", charging_state_);

    // get sound trigger platform info
    st_info_ = SoundTriggerPlatformInfo::GetInstance();
    if (!st_info_) {
        PAL_ERR(LOG_TAG, "Failed to get sound trigger platform info");
        throw std::runtime_error("Failed to get sound trigger platform info");
    }

    if (!dattr) {
        PAL_ERR(LOG_TAG,"Error:invalid device arguments");
        throw std::runtime_error("invalid device arguments");
    }

    for (int i=0; i < no_of_devices; i++) {
        mPalDevice.push_back(dattr[i]);
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

    rm->registerStream(this);

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
    cur_state_ = st_idle_;
    prev_state_ = nullptr;
    state_for_restore_ = ST_STATE_NONE;

    // Print the concurrency feature flags supported
    PAL_INFO(LOG_TAG, "capture conc enable %d,voice conc enable %d,voip conc enable %d",
        st_info_->GetConcurrentCaptureEnable(), st_info_->GetConcurrentVoiceCallEnable(),
        st_info_->GetConcurrentVoipCallEnable());

    // check concurrency count from rm
    rm->GetSoundTriggerConcurrencyCount(PAL_STREAM_VOICE_UI, &enable_concurrency_count,
        &disable_concurrency_count);

    // check if lpi should be used
    if (rm->IsLPISupported(PAL_STREAM_VOICE_UI) &&
        !(rm->isNLPISwitchSupported(PAL_STREAM_VOICE_UI) && enable_concurrency_count) &&
        !(rm->IsTransitToNonLPIOnChargingSupported() && charging_state_)) {
        use_lpi_ = true;
    } else {
        use_lpi_ = false;
    }

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
    std::lock_guard<std::mutex> lck(mStreamMutex);
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

    rm->deregisterStream(this);
    if (mStreamAttr) {
        free(mStreamAttr);
    }
    if (gsl_engine_model_) {
        free(gsl_engine_model_);
    }
    if (gsl_conf_levels_) {
        free(gsl_conf_levels_);
    }
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

    PAL_DBG(LOG_TAG, "Exit, status %d", status);
    return status;
}

int32_t StreamSoundTrigger::start() {
    int32_t status = 0;

    PAL_DBG(LOG_TAG, "Enter, stream direction %d", mStreamAttr->direction);

    std::lock_guard<std::mutex> lck(mStreamMutex);
    rejection_notified_ = false;
    std::shared_ptr<StEventConfig> ev_cfg(
       new StStartRecognitionEventConfig(false));
    status = cur_state_->ProcessEvent(ev_cfg);
    if (!status) {
        currentState = STREAM_STARTED;
    }

    PAL_DBG(LOG_TAG, "Exit, status %d", status);
    return status;
}

int32_t StreamSoundTrigger::stop() {
    int32_t status = 0;

    PAL_DBG(LOG_TAG, "Enter, stream direction %d", mStreamAttr->direction);

    std::lock_guard<std::mutex> lck(mStreamMutex);
    std::shared_ptr<StEventConfig> ev_cfg(
       new StStopRecognitionEventConfig(false));
    status = cur_state_->ProcessEvent(ev_cfg);
    if (!status) {
        currentState = STREAM_STOPPED;
    }

    PAL_DBG(LOG_TAG, "Exit, status %d", status);
    return status;
}

int32_t StreamSoundTrigger::read(struct pal_buffer* buf) {
    int32_t size = 0;
    uint32_t sleep_ms = 0;

    PAL_VERBOSE(LOG_TAG, "Enter");

    std::lock_guard<std::mutex> lck(mStreamMutex);
    if (st_info_->GetEnableDebugDumps() && !lab_fd_) {
        ST_DBG_FILE_OPEN_WR(lab_fd_, ST_DEBUG_DUMP_LOCATION,
            "lab_reading", "bin", lab_cnt);
        PAL_DBG(LOG_TAG, "lab data stored in: lab_reading_%d.bin",
            lab_cnt);
        lab_cnt++;
    }

    std::shared_ptr<StEventConfig> ev_cfg(
        new StReadBufferEventConfig((void *)buf));
    size = cur_state_->ProcessEvent(ev_cfg);

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

    PAL_DBG(LOG_TAG, "Enter, get parameter %u", param_id);
    if (gsl_engine_) {
        status = gsl_engine_->GetParameters(param_id, payload);
        if (status)
            PAL_ERR(LOG_TAG, "Failed to get parameters from engine");
    } else if (param_id == PAL_PARAM_ID_WAKEUP_MODULE_VERSION) {
        std::vector<std::shared_ptr<SoundModelConfig>> sm_cfg_list;

        st_info_->GetSmConfigForVersionQuery(sm_cfg_list);
        if (sm_cfg_list.size() == 0) {
            PAL_ERR(LOG_TAG, "No sound model config supports version query");
            return -EINVAL;
        }

        sm_cfg_ = sm_cfg_list[0];
        if (!mDevices.size()) {
            struct pal_device* dattr = new (struct pal_device);
            std::shared_ptr<Device> dev = nullptr;

            // update best device
            pal_device_id_t dev_id = GetAvailCaptureDevice();
            PAL_DBG(LOG_TAG, "Select available caputre device %d", dev_id);

            dev = GetPalDevice(dev_id, dattr, false);
            if (!dev) {
                PAL_ERR(LOG_TAG, "Device creation is failed");
                return -EINVAL;
            }
            mDevices.push_back(dev);
            dev = nullptr;
            delete dattr;
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
        mStreamSelector = sm_cfg_->GetModuleName();
        SetModelType(sm_cfg_->GetModuleType());
        PAL_DBG(LOG_TAG, "Module Type:%d, Name: %s", model_type_, mStreamSelector.c_str());
        mInstanceID = rm->getStreamInstanceID(this);

        gsl_engine_ = SoundTriggerEngine::Create(this, ST_SM_ID_SVA_F_STAGE_GMM,
                                                 model_type_, sm_cfg_);
        if (!gsl_engine_) {
            PAL_ERR(LOG_TAG, "big_sm: gsl engine creation failed");
            status = -ENOMEM;
            goto exit;
        }

        status = gsl_engine_->GetParameters(param_id, payload);
        if (status)
            PAL_ERR(LOG_TAG, "Failed to get parameters from engine");

        rm->resetStreamInstanceID(this, mInstanceID);
    } else {
        PAL_ERR(LOG_TAG, "No gsl engine present");
        status = -EINVAL;
    }

exit:
    if (mDevices.size() > 0) {
        status = mDevices[0]->close();
        device_opened_ = false;
        if (0 != status) {
            PAL_ERR(LOG_TAG, "Device close failed, status %d", status);
        }
    }
    PAL_DBG(LOG_TAG, "Exit status: %d", status);
    return status;
}

int32_t StreamSoundTrigger::setParameters(uint32_t param_id, void *payload) {
    int32_t status = 0;
    pal_param_payload *param_payload = (pal_param_payload *)payload;

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
            break;
        }
        case PAL_PARAM_ID_RECOGNITION_CONFIG: {
            /*
            * Currently spf needs graph stop and start for next detection.
            * Handle this event similar to fresh start config.
            */
            std::shared_ptr<StEventConfig> ev_cfg(
                new StRecognitionCfgEventConfig((void *)param_payload->payload));
            status = cur_state_->ProcessEvent(ev_cfg);
            break;
        }
        case PAL_PARAM_ID_STOP_BUFFERING: {
            /*
            * Currently spf needs graph stop and start for next detection.
            * Handle this event similar to STOP_RECOGNITION.
            */
            std::shared_ptr<StEventConfig> ev_cfg(
                new StStopRecognitionEventConfig(false));
            status = cur_state_->ProcessEvent(ev_cfg);
            if (st_info_->GetEnableDebugDumps()) {
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

    std::lock_guard<std::mutex> lck(mStreamMutex);

    if (!active) {
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
    }

    PAL_DBG(LOG_TAG, "Exit, status %d", status);

    return status;
}

int32_t StreamSoundTrigger::EnableLPI(bool is_enable) {
    std::lock_guard<std::mutex> lck(mStreamMutex);
    if (!rm->IsLPISupported(PAL_STREAM_VOICE_UI)) {
        PAL_DBG(LOG_TAG, "Ignore as LPI not supported");
    } else if (rm->IsTransitToNonLPIOnChargingSupported() && charging_state_) {
        PAL_DBG(LOG_TAG, "Ignore lpi update in car mode");
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
    status = setECRef_l(dev, is_enable);

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

int32_t StreamSoundTrigger::HandleChargingStateUpdate(bool state, bool active) {
    int32_t status = 0;
    int32_t enable_concurrency_count = 0;
    int32_t disable_concurrency_count = 0;

    PAL_DBG(LOG_TAG, "Enter, state %d", state);
    std::lock_guard<std::mutex> lck(mStreamMutex);
    charging_state_ = state;
    if (!rm->IsLPISupported(PAL_STREAM_VOICE_UI)) {
        PAL_DBG(LOG_TAG, "Ignore as LPI not supported");
    } else {
        // check concurrency count from rm
        rm->GetSoundTriggerConcurrencyCount(PAL_STREAM_VOICE_UI,
            &enable_concurrency_count, &disable_concurrency_count);

        // no need to update use_lpi_ if there's concurrency enabled
        if (rm->isNLPISwitchSupported(PAL_STREAM_VOICE_UI) &&
            enable_concurrency_count) {
            PAL_DBG(LOG_TAG, "Ignore lpi update when concurrency enabled");
        } else {
            use_lpi_ = !state;
        }
    }

    std::shared_ptr<StEventConfig> ev_cfg(
        new StChargingStateEventConfig(state, active));
    status = cur_state_->ProcessEvent(ev_cfg);
    if (status) {
        PAL_ERR(LOG_TAG, "Failed to update charging state");
    }

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

std::shared_ptr<Device> StreamSoundTrigger::GetPalDevice(
    pal_device_id_t dev_id, struct pal_device *dev, bool use_rm_profile) {
    std::shared_ptr<CaptureProfile> cap_prof = nullptr;
    std::shared_ptr<Device> device = nullptr;

    if (!dev) {
        PAL_ERR(LOG_TAG, "Invalid pal device object");
        goto exit;
    }

    dev->id = dev_id;

    if (use_rm_profile) {
        cap_prof = rm->GetSoundTriggerCaptureProfile();
        if (!cap_prof) {
            PAL_DBG(LOG_TAG, "Failed to get common capture profile");
            cap_prof = GetCurrentCaptureProfile();
        }
    } else {
        /* TODO: we may need to add input param for this function
        * to indicate the device id we need for capture profile
        */
        cap_prof = GetCurrentCaptureProfile();
    }

    if (!cap_prof) {
        PAL_ERR(LOG_TAG, "Failed to get common capture profile");
        goto exit;
    }
    dev->config.bit_width = cap_prof->GetBitWidth();
    dev->config.ch_info.channels = cap_prof->GetChannels();
    dev->config.sample_rate = cap_prof->GetSampleRate();
    dev->config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE;

    device = Device::getInstance(dev, rm);
    if (!device) {
        PAL_ERR(LOG_TAG, "Failed to get device instance");
        goto exit;
    }

    device->setDeviceAttributes(*dev);
    device->setSndName(cap_prof->GetSndName());

exit:

    return device;
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

struct detection_event_info* StreamSoundTrigger::GetDetectionEventInfo() {
    return (struct detection_event_info *)gsl_engine_->GetDetectionEventInfo();
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
    status = engine->LoadSoundModel(this, sm_data, sm_size);
    if (status) {
        PAL_ERR(LOG_TAG, "big_sm: gsl engine loading model"
               "failed, status %d", status);
        goto error_exit;
    }

    // cache 1st stage model for currency handling
    if (type == ST_SM_ID_SVA_F_STAGE_GMM) {
        gsl_engine_model_ = (uint8_t *)realloc(gsl_engine_model_, sm_size);
        if (!gsl_engine_model_) {
            PAL_ERR(LOG_TAG, "Failed to allocate memory for gsl model");
            goto unload_model;
        }
        ar_mem_cpy(gsl_engine_model_, sm_size, sm_data, sm_size);
        gsl_engine_model_size_ = sm_size;
    }

    return engine;

unload_model:
    status = engine->UnloadSoundModel(this);
    if (status)
        PAL_ERR(LOG_TAG, "Failed to unload sound model, status %d", status);

error_exit:
    return nullptr;
}

/*
 * Return stream instance id for gkv popluation
 * For PDK: always return INSTANCE_1
 * For SVA4: just return stream instance id
 */
uint32_t StreamSoundTrigger::GetInstanceId() {
    if (IS_MODULE_TYPE_PDK(model_type_))
        return INSTANCE_1;
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
    int32_t i = 0;
    struct pal_st_phrase_sound_model *phrase_sm = nullptr;
    struct pal_st_sound_model *common_sm = nullptr;
    uint8_t *ptr = nullptr;
    uint8_t *sm_payload = nullptr;
    uint8_t *sm_data = nullptr;
    int32_t sm_size = 0;
    SML_GlobalHeaderType *global_hdr = nullptr;
    SML_HeaderTypeV3 *hdr_v3 = nullptr;
    SML_BigSoundModelTypeV3 *big_sm = nullptr;
    std::shared_ptr<SoundTriggerEngine> engine = nullptr;
    uint32_t sm_version = SML_MODEL_V2;
    int32_t engine_id = 0;
    std::shared_ptr<EngineCfg> engine_cfg = nullptr;
    class SoundTriggerUUID uuid;

    PAL_DBG(LOG_TAG, "Enter");

    if (!sound_model) {
        PAL_ERR(LOG_TAG, "Exit Invalid sound_model param status %d", status);
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
                   phrase_sm->common.type, phrase_sm->num_phrases, status);
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
    if ((struct pal_st_sound_model *)sm_config_ != sound_model) {
        // Cache to use during SSR and other internal events handling.
        if (sm_config_) {
            free(sm_config_);
        }
        sm_config_ = (struct pal_st_phrase_sound_model *)calloc(1, sm_size);
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
        } else {
            ar_mem_cpy(sm_config_, sizeof(*common_sm),
                             common_sm, sizeof(*common_sm));
            ar_mem_cpy((uint8_t *)sm_config_ +  common_sm->data_offset,
                             common_sm->data_size,
                             (uint8_t *)common_sm + common_sm->data_offset,
                             common_sm->data_size);
        }
    }
    GetUUID(&uuid, sound_model);
    this->sm_cfg_ = this->st_info_->GetSmConfig(uuid);

    /* Update stream attributes as per sound model config */
    updateStreamAttributes();

    /* Create Sound Model Info for stream */
    sm_info_ = new SoundModelInfo();

    if (sound_model->type == PAL_SOUND_MODEL_TYPE_KEYPHRASE) {
        sm_payload = (uint8_t *)common_sm + common_sm->data_offset;
        global_hdr = (SML_GlobalHeaderType *)sm_payload;
        if (global_hdr->magicNumber == SML_GLOBAL_HEADER_MAGIC_NUMBER) {
            // Parse sound model 3.0
            sm_version = SML_MODEL_V3;
            hdr_v3 = (SML_HeaderTypeV3 *)(sm_payload +
                                          sizeof(SML_GlobalHeaderType));
            PAL_DBG(LOG_TAG, "num of sound models = %u", hdr_v3->numModels);
            for (i = 0; i < hdr_v3->numModels; i++) {
                big_sm = (SML_BigSoundModelTypeV3 *)(
                    sm_payload + sizeof(SML_GlobalHeaderType) +
                    sizeof(SML_HeaderTypeV3) +
                    (i * sizeof(SML_BigSoundModelTypeV3)));

                engine_id = static_cast<int32_t>(big_sm->type);
                PAL_INFO(LOG_TAG, "type = %u, size = %u, version = %u.%u",
                         big_sm->type, big_sm->size,
                         big_sm->versionMajor, big_sm->versionMinor);
                if (big_sm->type == ST_SM_ID_SVA_F_STAGE_GMM) {
                    st_module_type_t module_type = (st_module_type_t)big_sm->versionMajor;
                    SetModelType(module_type);
                    this->mStreamSelector = sm_cfg_->GetModuleName(module_type);
                    PAL_DBG(LOG_TAG, "Module type:%d, name: %s",
                        model_type_, mStreamSelector.c_str());
                    this->mInstanceID = this->rm->getStreamInstanceID(this);
                    sm_size = big_sm->size +
                        sizeof(struct pal_st_phrase_sound_model);
                    sm_data = (uint8_t *)calloc(1, sm_size);
                    if (!sm_data) {
                        status = -ENOMEM;
                        PAL_ERR(LOG_TAG, "sm_data allocation failed, status %d",
                                status);
                        goto error_exit;
                    }
                    ar_mem_cpy(sm_data, sizeof(*phrase_sm),
                                     (char *)phrase_sm, sizeof(*phrase_sm));
                    common_sm = (struct pal_st_sound_model *)sm_data;
                    common_sm->data_size = big_sm->size;
                    common_sm->data_offset += sizeof(SML_GlobalHeaderType) +
                        sizeof(SML_HeaderTypeV3) +
                        (hdr_v3->numModels * sizeof(SML_BigSoundModelTypeV3)) +
                        big_sm->offset;
                    ar_mem_cpy(sm_data + sizeof(*phrase_sm), big_sm->size,
                                     (char *)phrase_sm + common_sm->data_offset,
                                     big_sm->size);
                    common_sm->data_offset = sizeof(*phrase_sm);
                    common_sm = (struct pal_st_sound_model *)&phrase_sm->common;

                    UpdateModelId((st_module_type_t)big_sm->versionMajor);
                    gsl_engine_ = HandleEngineLoad(sm_data + sizeof(*phrase_sm),
                                                     big_sm->size, big_sm->type,
                                         (st_module_type_t)big_sm->versionMajor);
                    if (!gsl_engine_) {
                        status = -EINVAL;
                        goto error_exit;
                    }

                    engine_id = static_cast<int32_t>(ST_SM_ID_SVA_F_STAGE_GMM);
                    std::shared_ptr<EngineCfg> engine_cfg(new EngineCfg(
                       engine_id, gsl_engine_, (void *) sm_data, sm_size));

                    AddEngine(engine_cfg);
                } else if (big_sm->type != SML_ID_SVA_S_STAGE_UBM) {
                    if (big_sm->type == SML_ID_SVA_F_STAGE_INTERNAL || (big_sm->type == ST_SM_ID_SVA_S_STAGE_USER &&
                        !(phrase_sm->phrases[0].recognition_mode &
                        PAL_RECOGNITION_MODE_USER_IDENTIFICATION)))
                        continue;
                    sm_size = big_sm->size;
                    ptr = (uint8_t *)sm_payload +
                        sizeof(SML_GlobalHeaderType) +
                        sizeof(SML_HeaderTypeV3) +
                        (hdr_v3->numModels * sizeof(SML_BigSoundModelTypeV3)) +
                        big_sm->offset;
                    sm_data = (uint8_t *)calloc(1, sm_size);
                    if (!sm_data) {
                        status = -ENOMEM;
                        PAL_ERR(LOG_TAG, "Failed to alloc memory for sm_data");
                        goto error_exit;
                    }
                    ar_mem_cpy(sm_data, sm_size, ptr, sm_size);

                    engine = HandleEngineLoad(sm_data, sm_size, big_sm->type,
                                      (st_module_type_t) big_sm->versionMajor);
                    if (!engine) {
                        status = -EINVAL;
                        goto error_exit;
                    }

                    if (big_sm->type & ST_SM_ID_SVA_S_STAGE_KWD) {
                        notification_state_ |= KEYWORD_DETECTION_SUCCESS;
                    } else if (big_sm->type == ST_SM_ID_SVA_S_STAGE_USER) {
                        notification_state_ |= USER_VERIFICATION_SUCCESS;
                    }

                    std::shared_ptr<EngineCfg> engine_cfg(new EngineCfg(
                       engine_id, engine, (void *)sm_data, sm_size));

                    AddEngine(engine_cfg);
                }
            }
            if (!gsl_engine_) {
                PAL_ERR(LOG_TAG, "First stage sound model not present!!");
                goto error_exit;
            }
        } else {
            // Parse sound model 2.0
            sm_size = sizeof(*phrase_sm) + common_sm->data_size;
            sm_data = (uint8_t *)calloc(1, sm_size);
            if (!sm_data) {
                PAL_ERR(LOG_TAG, "Failed to allocate memory for sm_data");
                status = -ENOMEM;
                goto error_exit;
            }
            ar_mem_cpy(sm_data, sizeof(*phrase_sm),
                             (uint8_t *)phrase_sm, sizeof(*phrase_sm));
            ar_mem_cpy(sm_data + sizeof(*phrase_sm), common_sm->data_size,
                             (uint8_t*)phrase_sm + common_sm->data_offset,
                             common_sm->data_size);

            /*
             * For third party models, get module type and name from
             * sound model config directly without passing model type
             * as only one module is mapped to one vendor UUID
             */
            if ((!sm_cfg_->isQCVAUUID() && !sm_cfg_->isQCMDUUID())) {
                SetModelType(sm_cfg_->GetModuleType());
                this->mStreamSelector = sm_cfg_->GetModuleName();
            } else {
                SetModelType(ST_MODULE_TYPE_GMM);
                this->mStreamSelector = sm_cfg_->GetModuleName(ST_MODULE_TYPE_GMM);
            }

            PAL_DBG(LOG_TAG, "Module type:%d name:%s",
                model_type_, mStreamSelector.c_str());

            this->mInstanceID = this->rm->getStreamInstanceID(this);

            gsl_engine_ = HandleEngineLoad(sm_data + sizeof(*phrase_sm),
                                 common_sm->data_size, ST_SM_ID_SVA_F_STAGE_GMM,
                                 model_type_);
            if (!gsl_engine_) {
                status = -EINVAL;
                goto error_exit;
            }

            engine_id = static_cast<int32_t>(ST_SM_ID_SVA_F_STAGE_GMM);
            std::shared_ptr<EngineCfg> engine_cfg(new EngineCfg(
                 engine_id, gsl_engine_, (void *) sm_data, sm_size));

            AddEngine(engine_cfg);
        }
    } else {
        // handle for generic sound model
        common_sm = sound_model;
        sm_size = sizeof(*common_sm) + common_sm->data_size;
        sm_data = (uint8_t *)calloc(1, sm_size);
        if (!sm_data) {
            PAL_ERR(LOG_TAG, "Failed to allocate memory for sm_data");
            status = -ENOMEM;
            goto error_exit;
        }
        ar_mem_cpy(sm_data, sizeof(*common_sm),
            (uint8_t *)common_sm, sizeof(*common_sm));
        ar_mem_cpy(sm_data + sizeof(*common_sm), common_sm->data_size,
            (uint8_t*)common_sm + common_sm->data_offset, common_sm->data_size);
        if ((!sm_cfg_->isQCVAUUID() && !sm_cfg_->isQCMDUUID())) {
            SetModelType(sm_cfg_->GetModuleType());
            this->mStreamSelector = sm_cfg_->GetModuleName();
        } else {
            SetModelType(ST_MODULE_TYPE_GMM);
            this->mStreamSelector = sm_cfg_->GetModuleName(ST_MODULE_TYPE_GMM);
        }
        PAL_DBG(LOG_TAG, "Module type:%d, name:%s",
            model_type_, mStreamSelector.c_str());
        this->mInstanceID = this->rm->getStreamInstanceID(this);

        gsl_engine_ = HandleEngineLoad(sm_data + sizeof(*common_sm),
                                common_sm->data_size, ST_SM_ID_SVA_F_STAGE_GMM,
                                model_type_);
        if (!gsl_engine_) {
            status = -EINVAL;
            goto error_exit;
        }

        engine_id = static_cast<int32_t>(ST_SM_ID_SVA_F_STAGE_GMM);
        std::shared_ptr<EngineCfg> engine_cfg(new EngineCfg(
                engine_id, gsl_engine_, (void *) sm_data, sm_size));

        AddEngine(engine_cfg);
    }
    PAL_DBG(LOG_TAG, "Exit, status %d", status);
    return status;

error_exit:
    /*
     * Free sm_data allocated for engine which fails
     * to create or load sound model first, and then
     * release other engines which created or loaded
     * successfully.
     */
    if (sm_data) {
        free(sm_data);
    }
    for (auto &eng: engines_) {
        if (eng->sm_data_) {
            free(eng->sm_data_);
        }
        eng->GetEngine()->UnloadSoundModel(this);
    }
    engines_.clear();
    gsl_engine_.reset();
    if (reader_) {
        delete reader_;
        reader_ = nullptr;
    }
    if (sm_info_) {
        delete sm_info_;
        sm_info_ = nullptr;
    }
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
    if ((struct pal_st_sound_model *)sm_config_ != sound_model) {
        // Cache to use during SSR and other internal events handling.
        if (sm_config_) {
            free(sm_config_);
        }
        sm_config_ = (struct pal_st_phrase_sound_model *)calloc(1, sm_size);
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
        } else {
            ar_mem_cpy(sm_config_, sizeof(*common_sm),
                         common_sm, sizeof(*common_sm));
            ar_mem_cpy((uint8_t *)sm_config_ +  common_sm->data_offset,
                         common_sm->data_size,
                         (uint8_t *)common_sm + common_sm->data_offset,
                         common_sm->data_size);
        }
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
    struct st_param_header *param_hdr = NULL;
    struct st_hist_buffer_info *hist_buf = NULL;
    struct st_det_perf_mode_info *det_perf_mode = NULL;
    uint8_t *opaque_ptr = NULL;
    unsigned int opaque_size = 0, conf_levels_payload_size = 0;
    uint32_t hist_buffer_duration = 0;
    uint32_t pre_roll_duration = 0;
    uint32_t client_capture_read_delay = 0;
    uint8_t *conf_levels = NULL;
    uint32_t num_conf_levels = 0;
    uint32_t ring_buffer_len = 0;
    uint32_t ring_buffer_size = 0;

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
            PAL_ERR(LOG_TAG, "Failed to allocate rec_config status %d",
                status);
            status = -ENOMEM;
            goto exit;
        }
        ar_mem_cpy(rec_config_, sizeof(struct pal_st_recognition_config),
                         config, sizeof(struct pal_st_recognition_config));
        ar_mem_cpy((uint8_t *)rec_config_ + config->data_offset,
                         config->data_size,
                         (uint8_t *)config + config->data_offset,
                         config->data_size);
    }

    // dump recognition config opaque data
    if (config->data_size > 0 && st_info_->GetEnableDebugDumps()) {
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

    // Parse recognition config
    if (config->data_size > CUSTOM_CONFIG_OPAQUE_DATA_SIZE &&
        sm_cfg_->isQCVAUUID()) {
        opaque_ptr = (uint8_t *)config + config->data_offset;
        while (opaque_size < config->data_size) {
            param_hdr = (struct st_param_header *)opaque_ptr;
            PAL_VERBOSE(LOG_TAG, "key %d, payload size %d",
                        param_hdr->key_id, param_hdr->payload_size);

            switch (param_hdr->key_id) {
              case ST_PARAM_KEY_CONFIDENCE_LEVELS:
                  conf_levels_intf_version_ = *(uint32_t *)(
                      opaque_ptr + sizeof(struct st_param_header));
                  PAL_VERBOSE(LOG_TAG, "conf_levels_intf_version = %u",
                      conf_levels_intf_version_);
                  if (conf_levels_intf_version_ !=
                      CONF_LEVELS_INTF_VERSION_0002) {
                      conf_levels_payload_size =
                          sizeof(struct st_confidence_levels_info);
                  } else {
                      conf_levels_payload_size =
                          sizeof(struct st_confidence_levels_info_v2);
                  }
                  if (param_hdr->payload_size != conf_levels_payload_size) {
                      PAL_ERR(LOG_TAG, "Conf level format error, exiting");
                      status = -EINVAL;
                      goto error_exit;
                  }
                  status = ParseOpaqueConfLevels(opaque_ptr,
                                                 conf_levels_intf_version_,
                                                 &conf_levels,
                                                 &num_conf_levels);
                if (status) {
                    PAL_ERR(LOG_TAG, "Failed to parse opaque conf levels");
                    goto error_exit;
                }

                opaque_size += sizeof(struct st_param_header) +
                    conf_levels_payload_size;
                opaque_ptr += sizeof(struct st_param_header) +
                    conf_levels_payload_size;
                if (status) {
                    PAL_ERR(LOG_TAG, "Parse conf levels failed(status=%d)",
                            status);
                    status = -EINVAL;
                    goto error_exit;
                }
                break;
              case ST_PARAM_KEY_HISTORY_BUFFER_CONFIG:
                  if (param_hdr->payload_size !=
                      sizeof(struct st_hist_buffer_info)) {
                      PAL_ERR(LOG_TAG, "History buffer config format error");
                      status = -EINVAL;
                      goto error_exit;
                  }
                  hist_buf = (struct st_hist_buffer_info *)(opaque_ptr +
                      sizeof(struct st_param_header));
                  hist_buf_duration_ = hist_buf->hist_buffer_duration_msec;
                  pre_roll_duration_ = hist_buf->pre_roll_duration_msec;

                  opaque_size += sizeof(struct st_param_header) +
                      sizeof(struct st_hist_buffer_info);
                  opaque_ptr += sizeof(struct st_param_header) +
                      sizeof(struct st_hist_buffer_info);
                  break;
              case ST_PARAM_KEY_DETECTION_PERF_MODE:
                  if (param_hdr->payload_size !=
                      sizeof(struct st_det_perf_mode_info)) {
                      PAL_ERR(LOG_TAG, "Opaque data format error, exiting");
                      status = -EINVAL;
                      goto error_exit;
                  }
                  det_perf_mode = (struct st_det_perf_mode_info *)
                      (opaque_ptr + sizeof(struct st_param_header));
                  PAL_DBG(LOG_TAG, "set perf mode %d", det_perf_mode->mode);
                  opaque_size += sizeof(struct st_param_header) +
                      sizeof(struct st_det_perf_mode_info);
                  opaque_ptr += sizeof(struct st_param_header) +
                      sizeof(struct st_det_perf_mode_info);
                  break;
              default:
                  PAL_ERR(LOG_TAG, "Unsupported opaque data key id, exiting");
                  status = -EINVAL;
                  goto error_exit;
            }
        }
    } else {
        // get history buffer duration from sound trigger platform xml
        hist_buf_duration_ = sm_cfg_->GetKwDuration();
        pre_roll_duration_ = 0;

        if (sm_cfg_->isQCVAUUID() || sm_cfg_->isQCMDUUID()) {
            status = FillConfLevels(config, &conf_levels, &num_conf_levels);
            if (status) {
                PAL_ERR(LOG_TAG, "Failed to parse conf levels from rc config");
                goto error_exit;
            }
        }
    }

    client_capture_read_delay = sm_cfg_->GetCaptureReadDelay();
    PAL_DBG(LOG_TAG, "history buf len = %d, preroll len = %d, read delay = %d",
        hist_buf_duration_, pre_roll_duration_, client_capture_read_delay);

    status = gsl_engine_->UpdateBufConfig(this, hist_buf_duration_,
                                                pre_roll_duration_);
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
    if (st_info_->GetMmapEnable()) {
        inBufSize = st_info_->GetMmapFrameLength() *
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
        if (engines_[i]->GetEngine()->GetEngineType() ==
            ST_SM_ID_SVA_F_STAGE_GMM) {
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

    // update custom config for 3rd party VA session
    if (!sm_cfg_->isQCVAUUID() && !sm_cfg_->isQCMDUUID()) {
        gsl_engine_->UpdateConfLevels(this, config, nullptr, 0);
    } else {
        if (num_conf_levels > 0) {
            gsl_engine_->UpdateConfLevels(this, config,
                                      conf_levels, num_conf_levels);
 
            gsl_conf_levels_ = (uint8_t *)realloc(gsl_conf_levels_, num_conf_levels);
            if (!gsl_conf_levels_) {
                PAL_ERR(LOG_TAG, "Failed to allocate gsl conf levels memory");
                status = -ENOMEM;
                goto error_exit;
            }
         }
        ar_mem_cpy(gsl_conf_levels_, num_conf_levels, conf_levels, num_conf_levels);
        gsl_conf_levels_size_ = num_conf_levels;
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

    if (st_conf_levels_) {
        free(st_conf_levels_);
        st_conf_levels_ = nullptr;
    }
    if (st_conf_levels_v2_) {
        free(st_conf_levels_v2_);
        st_conf_levels_v2_ = nullptr;
    }
exit:
    if (conf_levels)
        free(conf_levels);

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

    status = GenerateCallbackEvent(&rec_event, &event_size,
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
         * If mutex is locked back here, mark mutex_unlocked_after_cb_ as true
         * so that we can avoid double unlock in SetEngineDetectionState.
         */
        if (!lock_status)
            mutex_unlocked_after_cb_ = true;
    }

    free(rec_event);

    PAL_DBG(LOG_TAG, "Exit, status %d", status);
    return status;
}

void StreamSoundTrigger::PackEventConfLevels(uint8_t *opaque_data) {

    struct st_confidence_levels_info *conf_levels = nullptr;
    struct st_confidence_levels_info_v2 *conf_levels_v2 = nullptr;
    uint32_t i = 0, j = 0, k = 0, user_id = 0, num_user_levels = 0;

    PAL_VERBOSE(LOG_TAG, "Enter");

    /*
     * Update the opaque data of callback event with confidence levels
     * accordingly for all users and keywords from the detection event
     */
    if (conf_levels_intf_version_ != CONF_LEVELS_INTF_VERSION_0002) {
        conf_levels = (struct st_confidence_levels_info *)opaque_data;
        for (i = 0; i < conf_levels->num_sound_models; i++) {
            if (conf_levels->conf_levels[i].sm_id == ST_SM_ID_SVA_F_STAGE_GMM) {
                for (j = 0; j < conf_levels->conf_levels[i].num_kw_levels; j++) {
                    if (j <= sm_info_->GetConfLevelsSize())
                        conf_levels->conf_levels[i].kw_levels[j].kw_level =
                            sm_info_->GetDetConfLevels()[j];
                    else
                        PAL_ERR(LOG_TAG, "unexpected conf size %d < %d",
                            sm_info_->GetConfLevelsSize(), j);

                    num_user_levels =
                        conf_levels->conf_levels[i].kw_levels[j].num_user_levels;
                    for (k = 0; k < num_user_levels; k++) {
                        user_id = conf_levels->conf_levels[i].kw_levels[j].
                            user_levels[k].user_id;
                        if (user_id <= sm_info_->GetConfLevelsSize())
                            conf_levels->conf_levels[i].kw_levels[j].user_levels[k].
                                level = sm_info_->GetDetConfLevels()[user_id];
                        else
                            PAL_ERR(LOG_TAG, "Unexpected conf size %d < %d",
                                sm_info_->GetConfLevelsSize(), user_id);
                    }
                }
            } else if (conf_levels->conf_levels[i].sm_id & ST_SM_ID_SVA_S_STAGE_KWD ||
                       conf_levels->conf_levels[i].sm_id & ST_SM_ID_SVA_S_STAGE_USER) {
                /* Update confidence levels for second stage */
                for (auto& eng: engines_) {
                    if (conf_levels->conf_levels[i].sm_id & ST_SM_ID_SVA_S_STAGE_KWD &&
                        eng->GetEngineId() & ST_SM_ID_SVA_S_STAGE_KWD) {
                        conf_levels->conf_levels[i].kw_levels[0].kw_level =
                            eng->GetEngine()->GetDetectedConfScore();
                        conf_levels->conf_levels[i].kw_levels[0].user_levels[0].level = 0;
                    } else if (conf_levels->conf_levels[i].sm_id & ST_SM_ID_SVA_S_STAGE_USER &&
                        conf_levels->conf_levels[i].sm_id == eng->GetEngineId()) {
                        conf_levels->conf_levels[i].kw_levels[0].kw_level =
                            eng->GetEngine()->GetDetectedConfScore();
                        conf_levels->conf_levels[i].kw_levels[0].user_levels[0].level =
                            eng->GetEngine()->GetDetectedConfScore();
                    }
                }
            }
        }
    } else {
        conf_levels_v2 = (struct st_confidence_levels_info_v2 *)opaque_data;
        for (i = 0; i < conf_levels_v2->num_sound_models; i++) {
            if (conf_levels_v2->conf_levels[i].sm_id == ST_SM_ID_SVA_F_STAGE_GMM) {
                for (j = 0; j < conf_levels_v2->conf_levels[i].num_kw_levels; j++) {
                    if (j <= sm_info_->GetConfLevelsSize())
                            conf_levels_v2->conf_levels[i].kw_levels[j].kw_level =
                                    sm_info_->GetDetConfLevels()[j];
                    else
                        PAL_ERR(LOG_TAG, "unexpected conf size %d < %d",
                            sm_info_->GetConfLevelsSize(), j);

                    PAL_INFO(LOG_TAG, "First stage KW Conf levels[%d]-%d",
                        j, sm_info_->GetDetConfLevels()[j])

                    num_user_levels =
                        conf_levels_v2->conf_levels[i].kw_levels[j].num_user_levels;
                    for (k = 0; k < num_user_levels; k++) {
                        user_id = conf_levels_v2->conf_levels[i].kw_levels[j].
                            user_levels[k].user_id;
                        if (user_id <=  sm_info_->GetConfLevelsSize())
                            conf_levels_v2->conf_levels[i].kw_levels[j].user_levels[k].
                                level = sm_info_->GetDetConfLevels()[user_id];
                        else
                            PAL_ERR(LOG_TAG, "Unexpected conf size %d < %d",
                                sm_info_->GetConfLevelsSize(), user_id);

                        PAL_INFO(LOG_TAG, "First stage User Conf levels[%d]-%d",
                            k, sm_info_->GetDetConfLevels()[user_id])
                    }
                }
            } else if (conf_levels_v2->conf_levels[i].sm_id & ST_SM_ID_SVA_S_STAGE_KWD ||
                       conf_levels_v2->conf_levels[i].sm_id & ST_SM_ID_SVA_S_STAGE_USER) {
                /* Update confidence levels for second stage */
                for (auto& eng: engines_) {
                    if (conf_levels_v2->conf_levels[i].sm_id & ST_SM_ID_SVA_S_STAGE_KWD &&
                        eng->GetEngineId() & ST_SM_ID_SVA_S_STAGE_KWD) {
                        conf_levels_v2->conf_levels[i].kw_levels[0].kw_level =
                            eng->GetEngine()->GetDetectedConfScore();
                        conf_levels_v2->conf_levels[i].kw_levels[0].user_levels[0].level = 0;
                    } else if (conf_levels_v2->conf_levels[i].sm_id & ST_SM_ID_SVA_S_STAGE_USER &&
                        conf_levels_v2->conf_levels[i].sm_id == eng->GetEngineId()) {
                        conf_levels_v2->conf_levels[i].kw_levels[0].kw_level =
                            eng->GetEngine()->GetDetectedConfScore();
                        conf_levels_v2->conf_levels[i].kw_levels[0].user_levels[0].level =
                            eng->GetEngine()->GetDetectedConfScore();
                    }
                }
            }
        }
    }
    PAL_VERBOSE(LOG_TAG, "Exit");
}

void StreamSoundTrigger::FillCallbackConfLevels(uint8_t *opaque_data,
                   uint32_t det_keyword_id, uint32_t best_conf_level) {
    int i = 0;
    struct st_confidence_levels_info_v2 *conf_levels_v2 = nullptr;
    struct st_confidence_levels_info *conf_levels = nullptr;

    if (conf_levels_intf_version_ != CONF_LEVELS_INTF_VERSION_0002) {
        conf_levels = (struct st_confidence_levels_info *)opaque_data;
        for (i = 0; i < conf_levels->num_sound_models; i++) {
            if (conf_levels->conf_levels[i].sm_id == ST_SM_ID_SVA_F_STAGE_GMM) {
                conf_levels->conf_levels[i].kw_levels[det_keyword_id].
                    kw_level = best_conf_level;
                conf_levels->conf_levels[i].kw_levels[det_keyword_id].
                    user_levels[0].level = 0;
                PAL_DBG(LOG_TAG, "First stage returning conf level : %d",
                    best_conf_level);
            } else if (conf_levels->conf_levels[i].sm_id & ST_SM_ID_SVA_S_STAGE_KWD) {
                for (auto& eng: engines_) {
                    if (eng->GetEngineId() & ST_SM_ID_SVA_S_STAGE_KWD) {
                        conf_levels->conf_levels[i].kw_levels[0].kw_level =
                            eng->GetEngine()->GetDetectedConfScore();
                        conf_levels->conf_levels[i].kw_levels[0].user_levels[0].level = 0;
                        PAL_DBG(LOG_TAG, "Second stage keyword conf level: %d",
                            eng->GetEngine()->GetDetectedConfScore());
                    }
                }
            } else if (conf_levels->conf_levels[i].sm_id & ST_SM_ID_SVA_S_STAGE_USER) {
                for (auto& eng: engines_) {
                    if (eng->GetEngineId() == conf_levels->conf_levels[i].sm_id) {
                        conf_levels->conf_levels[i].kw_levels[0].kw_level =
                            eng->GetEngine()->GetDetectedConfScore();
                        conf_levels->conf_levels[i].kw_levels[0].user_levels[0].level =
                            eng->GetEngine()->GetDetectedConfScore();
                        PAL_DBG(LOG_TAG, "Second stage user conf level: %d",
                            eng->GetEngine()->GetDetectedConfScore());
                    }
                }
            }
        }
    } else {
        conf_levels_v2 = (struct st_confidence_levels_info_v2 *)opaque_data;
        for (i = 0; i < conf_levels_v2->num_sound_models; i++) {
            if (conf_levels_v2->conf_levels[i].sm_id == ST_SM_ID_SVA_F_STAGE_GMM) {
                conf_levels_v2->conf_levels[i].kw_levels[det_keyword_id].
                    kw_level = best_conf_level;
                conf_levels_v2->conf_levels[i].kw_levels[det_keyword_id].
                    user_levels[0].level = 0;
                PAL_DBG(LOG_TAG, "First stage returning conf level: %d",
                    best_conf_level);
            } else if (conf_levels_v2->conf_levels[i].sm_id & ST_SM_ID_SVA_S_STAGE_KWD) {
                for (auto& eng: engines_) {
                    if (eng->GetEngineId() & ST_SM_ID_SVA_S_STAGE_KWD) {
                        conf_levels_v2->conf_levels[i].kw_levels[0].kw_level =
                            eng->GetEngine()->GetDetectedConfScore();
                        conf_levels_v2->conf_levels[i].kw_levels[0].user_levels[0].level = 0;
                        PAL_DBG(LOG_TAG, "Second stage keyword conf level: %d",
                            eng->GetEngine()->GetDetectedConfScore());
                    }
                }
            } else if (conf_levels_v2->conf_levels[i].sm_id & ST_SM_ID_SVA_S_STAGE_USER) {
                for (auto& eng: engines_) {
                    if (eng->GetEngineId() == conf_levels_v2->conf_levels[i].sm_id) {
                        conf_levels_v2->conf_levels[i].kw_levels[0].kw_level =
                            eng->GetEngine()->GetDetectedConfScore();
                        conf_levels_v2->conf_levels[i].kw_levels[0].user_levels[0].level =
                            eng->GetEngine()->GetDetectedConfScore();
                        PAL_DBG(LOG_TAG, "Second stage user conf level: %d",
                            eng->GetEngine()->GetDetectedConfScore());
                    }
                }
            }
        }
    }
}

int32_t StreamSoundTrigger::GenerateCallbackEvent(
    struct pal_st_recognition_event **event, uint32_t *evt_size,
    bool detection) {

    struct pal_st_phrase_recognition_event *phrase_event = nullptr;
    struct pal_st_generic_recognition_event *generic_event = nullptr;
    struct st_param_header *param_hdr = nullptr;
    struct st_keyword_indices_info *kw_indices = nullptr;
    struct st_timestamp_info *timestamps = nullptr;
    struct model_stats *det_model_stat = nullptr;
    struct detection_event_info_pdk *det_ev_info_pdk = nullptr;
    struct detection_event_info *det_ev_info = nullptr;
    size_t opaque_size = 0;
    size_t event_size = 0, conf_levels_size = 0;
    uint8_t *opaque_data = nullptr;
    uint32_t start_index = 0, end_index = 0;
    uint8_t *custom_event = nullptr;
    uint32_t det_keyword_id = 0;
    uint32_t best_conf_level = 0;
    uint32_t detection_timestamp_lsw = 0;
    uint32_t detection_timestamp_msw = 0;
    int32_t status = 0;
    int32_t num_models = 0;

    PAL_DBG(LOG_TAG, "Enter");
    *event = nullptr;
    if (sound_model_type_ == PAL_SOUND_MODEL_TYPE_KEYPHRASE) {
        if (model_id_ > 0) {
            det_ev_info_pdk = (struct detection_event_info_pdk *)
                gsl_engine_->GetDetectionEventInfo();
            if (!det_ev_info_pdk) {
                PAL_ERR(LOG_TAG, "detection info multi model not available");
                status = -EINVAL;
                goto exit;
            }
        } else {
            det_ev_info = (struct detection_event_info *)gsl_engine_->
                            GetDetectionEventInfo();
            if (!det_ev_info) {
                PAL_ERR(LOG_TAG, "detection info not available");
                status = -EINVAL;
                goto exit;
            }
        }

        if (conf_levels_intf_version_ != CONF_LEVELS_INTF_VERSION_0002)
            conf_levels_size = sizeof(struct st_confidence_levels_info);
        else
            conf_levels_size = sizeof(struct st_confidence_levels_info_v2);

        opaque_size = (3 * sizeof(struct st_param_header)) +
            sizeof(struct st_timestamp_info) +
            sizeof(struct st_keyword_indices_info) +
            conf_levels_size;

        event_size = sizeof(struct pal_st_phrase_recognition_event) +
                     opaque_size;
        phrase_event = (struct pal_st_phrase_recognition_event *)
                       calloc(1, event_size);
        if (!phrase_event) {
            PAL_ERR(LOG_TAG, "Failed to alloc memory for recognition event");
            status =  -ENOMEM;
            goto exit;
        }

        phrase_event->num_phrases = rec_config_->num_phrases;
        memcpy(phrase_event->phrase_extras, rec_config_->phrases,
               phrase_event->num_phrases *
               sizeof(struct pal_st_phrase_recognition_extra));

        *event = &(phrase_event->common);
        (*event)->status = detection ? PAL_RECOGNITION_STATUS_SUCCESS :
                           PAL_RECOGNITION_STATUS_FAILURE;
        (*event)->type = sound_model_type_;
        (*event)->st_handle = (pal_st_handle_t *)this;
        (*event)->capture_available = rec_config_->capture_requested;
        // TODO: generate capture session
        (*event)->capture_session = 0;
        (*event)->capture_delay_ms = 0;
        (*event)->capture_preamble_ms = 0;
        (*event)->trigger_in_data = true;
        (*event)->data_size = opaque_size;
        (*event)->data_offset = sizeof(struct pal_st_phrase_recognition_event);
        (*event)->media_config.sample_rate =
            mStreamAttr->in_media_config.sample_rate;
        (*event)->media_config.bit_width =
            mStreamAttr->in_media_config.bit_width;
        (*event)->media_config.ch_info.channels =
            mStreamAttr->in_media_config.ch_info.channels;
        (*event)->media_config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE;
        // Filling Opaque data
        opaque_data = (uint8_t *)phrase_event +
                       phrase_event->common.data_offset;

        /* Pack the opaque data confidence levels structure */
        param_hdr = (struct st_param_header *)opaque_data;
        param_hdr->key_id = ST_PARAM_KEY_CONFIDENCE_LEVELS;
        if (conf_levels_intf_version_ !=  CONF_LEVELS_INTF_VERSION_0002)
            param_hdr->payload_size = sizeof(struct st_confidence_levels_info);
        else
            param_hdr->payload_size = sizeof(struct st_confidence_levels_info_v2);
        opaque_data += sizeof(struct st_param_header);
        /* Copy the cached conf levels from recognition config */
        if (conf_levels_intf_version_ != CONF_LEVELS_INTF_VERSION_0002)
            ar_mem_cpy(opaque_data, param_hdr->payload_size,
                    st_conf_levels_, param_hdr->payload_size);
        else
            ar_mem_cpy(opaque_data, param_hdr->payload_size,
                st_conf_levels_v2_, param_hdr->payload_size);
        if (model_id_ > 0) {
            num_models = det_ev_info_pdk->num_detected_models;
            for (int i = 0; i < num_models; ++i) {
                det_model_stat = &det_ev_info_pdk->detected_model_stats[i];
                if (model_id_ == det_model_stat->detected_model_id) {
                    det_keyword_id = det_model_stat->detected_keyword_id;
                    best_conf_level = det_model_stat->best_confidence_level;
                    detection_timestamp_lsw =
                        det_model_stat->detection_timestamp_lsw;
                    detection_timestamp_msw =
                        det_model_stat->detection_timestamp_msw;
                    PAL_DBG(LOG_TAG, "keywordID: %u, best_conf_level: %u",
                            det_keyword_id, best_conf_level);
                    break;
                }
            }
            FillCallbackConfLevels(opaque_data, det_keyword_id, best_conf_level);
        } else {
            PackEventConfLevels(opaque_data);
        }
        opaque_data += param_hdr->payload_size;

        /* Pack the opaque data keyword indices structure */
        param_hdr = (struct st_param_header *)opaque_data;
        param_hdr->key_id = ST_PARAM_KEY_KEYWORD_INDICES;
        param_hdr->payload_size = sizeof(struct st_keyword_indices_info);
        opaque_data += sizeof(struct st_param_header);
        kw_indices = (struct st_keyword_indices_info *)opaque_data;
        kw_indices->version = 0x1;
        reader_->getIndices(&start_index, &end_index);
        kw_indices->start_index = start_index;
        kw_indices->end_index = end_index;
        opaque_data += sizeof(struct st_keyword_indices_info);

        /*
            * Pack the opaque data detection time structure
            * TODO: add support for 2nd stage detection timestamp
            */
        param_hdr = (struct st_param_header *)opaque_data;
        param_hdr->key_id = ST_PARAM_KEY_TIMESTAMP;
        param_hdr->payload_size = sizeof(struct st_timestamp_info);
        opaque_data += sizeof(struct st_param_header);
        timestamps = (struct st_timestamp_info *)opaque_data;
        timestamps->version = 0x1;
        if (model_id_ > 0) {
            timestamps->first_stage_det_event_time = 1000 *
                        ((uint64_t)detection_timestamp_lsw +
                        ((uint64_t)detection_timestamp_msw<<32));
        } else {
            timestamps->first_stage_det_event_time = 1000 *
                ((uint64_t)det_ev_info->detection_timestamp_lsw +
                ((uint64_t)det_ev_info->detection_timestamp_msw << 32));
        }

        // dump detection event opaque data
        if ((*event)->data_offset > 0 && (*event)->data_size > 0 &&
            st_info_->GetEnableDebugDumps()) {
            opaque_data = (uint8_t *)phrase_event + phrase_event->common.data_offset;
            ST_DBG_DECLARE(FILE *det_opaque_fd = NULL; static int det_opaque_cnt = 0);
            ST_DBG_FILE_OPEN_WR(det_opaque_fd, ST_DEBUG_DUMP_LOCATION,
                "det_event_opaque", "bin", det_opaque_cnt);
            ST_DBG_FILE_WRITE(det_opaque_fd, opaque_data, (*event)->data_size);
            ST_DBG_FILE_CLOSE(det_opaque_fd);
            PAL_DBG(LOG_TAG, "detection event opaque data stored in: det_event_opaque_%d.bin",
                det_opaque_cnt);
            det_opaque_cnt++;
        }
    } else if (sound_model_type_ == PAL_SOUND_MODEL_TYPE_GENERIC) {
        gsl_engine_->GetCustomDetectionEvent(&custom_event, &opaque_size);
        event_size = sizeof(struct pal_st_generic_recognition_event) +
                     opaque_size;
        generic_event = (struct pal_st_generic_recognition_event *)
                       calloc(1, event_size);
        if (!generic_event) {
            PAL_ERR(LOG_TAG, "Failed to alloc memory for recognition event");
            status =  -ENOMEM;
            goto exit;

        }

        *event = &(generic_event->common);
        (*event)->status = PAL_RECOGNITION_STATUS_SUCCESS;
        (*event)->type = sound_model_type_;
        (*event)->st_handle = (pal_st_handle_t *)this;
        (*event)->capture_available = rec_config_->capture_requested;
        // TODO: generate capture session
        (*event)->capture_session = 0;
        (*event)->capture_delay_ms = 0;
        (*event)->capture_preamble_ms = 0;
        (*event)->trigger_in_data = true;
        (*event)->data_size = opaque_size;
        (*event)->data_offset = sizeof(struct pal_st_generic_recognition_event);
        (*event)->media_config.sample_rate =
            mStreamAttr->in_media_config.sample_rate;
        (*event)->media_config.bit_width =
            mStreamAttr->in_media_config.bit_width;
        (*event)->media_config.ch_info.channels =
            mStreamAttr->in_media_config.ch_info.channels;
        (*event)->media_config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE;

        // Filling Opaque data
        opaque_data = (uint8_t *)generic_event +
                       generic_event->common.data_offset;
        ar_mem_cpy(opaque_data, opaque_size, custom_event, opaque_size);
    }
    *evt_size = event_size;
exit:
    PAL_DBG(LOG_TAG, "Exit");
    return status;
}

int32_t StreamSoundTrigger::ParseOpaqueConfLevels(
    void *opaque_conf_levels,
    uint32_t version,
    uint8_t **out_conf_levels,
    uint32_t *out_num_conf_levels) {

    int32_t status = 0;
    struct st_confidence_levels_info *conf_levels = nullptr;
    struct st_confidence_levels_info_v2 *conf_levels_v2 = nullptr;
    struct st_sound_model_conf_levels *sm_levels = nullptr;
    struct st_sound_model_conf_levels_v2 *sm_levels_v2 = nullptr;
    int32_t confidence_level = 0;
    int32_t confidence_level_v2 = 0;
    bool gmm_conf_found = false;

    PAL_DBG(LOG_TAG, "Enter");
    if (version != CONF_LEVELS_INTF_VERSION_0002) {
        conf_levels = (struct st_confidence_levels_info *)
            ((char *)opaque_conf_levels + sizeof(struct st_param_header));

        if (!st_conf_levels_) {
             st_conf_levels_ = (struct st_confidence_levels_info *)calloc(1,
                                 sizeof(struct st_confidence_levels_info));
             if (!st_conf_levels_) {
                 PAL_ERR(LOG_TAG, "failed to alloc stream conf_levels_");
                 status = -ENOMEM;
                 goto exit;
             }
        }
        /* Cache to use during detection event processing */
        ar_mem_cpy((uint8_t *)st_conf_levels_, sizeof(struct st_confidence_levels_info),
            (uint8_t *)conf_levels, sizeof(struct st_confidence_levels_info));

        for (int i = 0; i < conf_levels->num_sound_models; i++) {
            sm_levels = &conf_levels->conf_levels[i];
            if (sm_levels->sm_id == ST_SM_ID_SVA_F_STAGE_GMM) {
                gmm_conf_found = true;
                status = FillOpaqueConfLevels((void *)sm_levels,
                    out_conf_levels, out_num_conf_levels, version);
            } else if (sm_levels->sm_id & ST_SM_ID_SVA_S_STAGE_KWD ||
                       sm_levels->sm_id & ST_SM_ID_SVA_S_STAGE_USER) {
                confidence_level =
                    (sm_levels->sm_id & ST_SM_ID_SVA_S_STAGE_KWD) ?
                    sm_levels->kw_levels[0].kw_level:
                    sm_levels->kw_levels[0].user_levels[0].level;
                if (sm_levels->sm_id & ST_SM_ID_SVA_S_STAGE_KWD) {
                    PAL_DBG(LOG_TAG, "second stage keyword confidence level = %d", confidence_level);
                } else {
                    PAL_DBG(LOG_TAG, "second stage user confidence level = %d", confidence_level);
                }
                for (auto& eng: engines_) {
                    if (sm_levels->sm_id & eng->GetEngineId() ||
                        ((eng->GetEngineId() & ST_SM_ID_SVA_S_STAGE_RNN) &&
                        (sm_levels->sm_id & ST_SM_ID_SVA_S_STAGE_PDK))) {
                        eng->GetEngine()->UpdateConfLevels(this, rec_config_,
                            (uint8_t *)&confidence_level, 1);
                    }
                }
            }
        }
    } else {
        conf_levels_v2 = (struct st_confidence_levels_info_v2 *)
            ((char *)opaque_conf_levels + sizeof(struct st_param_header));

        if (!st_conf_levels_v2_) {
            st_conf_levels_v2_ = (struct st_confidence_levels_info_v2 *)calloc(1,
                sizeof(struct st_confidence_levels_info_v2));
            if (!st_conf_levels_v2_) {
                PAL_ERR(LOG_TAG, "failed to alloc stream conf_levels_");
                status = -ENOMEM;
                goto exit;
            }
        }
        /* Cache to use during detection event processing */
        ar_mem_cpy((uint8_t *)st_conf_levels_v2_, sizeof(struct st_confidence_levels_info_v2),
            (uint8_t *)conf_levels_v2, sizeof(struct st_confidence_levels_info_v2));

        for (int i = 0; i < conf_levels_v2->num_sound_models; i++) {
            sm_levels_v2 = &conf_levels_v2->conf_levels[i];
            if (sm_levels_v2->sm_id == ST_SM_ID_SVA_F_STAGE_GMM) {
                gmm_conf_found = true;
                status = FillOpaqueConfLevels((void *)sm_levels_v2,
                    out_conf_levels, out_num_conf_levels, version);
            } else if (sm_levels_v2->sm_id & ST_SM_ID_SVA_S_STAGE_KWD ||
                       sm_levels_v2->sm_id & ST_SM_ID_SVA_S_STAGE_USER) {
                confidence_level_v2 =
                    (sm_levels_v2->sm_id & ST_SM_ID_SVA_S_STAGE_KWD) ?
                    sm_levels_v2->kw_levels[0].kw_level:
                    sm_levels_v2->kw_levels[0].user_levels[0].level;
                if (sm_levels_v2->sm_id & ST_SM_ID_SVA_S_STAGE_KWD) {
                    PAL_DBG(LOG_TAG, "second stage keyword confidence level = %d", confidence_level_v2);
                } else {
                    PAL_DBG(LOG_TAG, "second stage user confidence level = %d", confidence_level_v2);
                }
                for (auto& eng: engines_) {
                    PAL_VERBOSE(LOG_TAG, "sm id %d, engine id %d ",
                        sm_levels_v2->sm_id , eng->GetEngineId());
                    if (sm_levels_v2->sm_id & eng->GetEngineId() ||
                        ((eng->GetEngineId() & ST_SM_ID_SVA_S_STAGE_RNN) &&
                        (sm_levels_v2->sm_id & ST_SM_ID_SVA_S_STAGE_PDK))) {
                        eng->GetEngine()->UpdateConfLevels(this, rec_config_,
                            (uint8_t *)&confidence_level_v2, 1);
                    }
                }
            }
        }
    }

    if (!gmm_conf_found || status) {
        PAL_ERR(LOG_TAG, "Did not receive GMM confidence threshold, error!");
        status = -EINVAL;
    }

exit:
    PAL_DBG(LOG_TAG, "Exit");

    return status;
}

int32_t StreamSoundTrigger::FillConfLevels(
    struct pal_st_recognition_config *config,
    uint8_t **out_conf_levels,
    uint32_t *out_num_conf_levels) {

    int32_t status = 0;
    uint32_t num_conf_levels = 0;
    unsigned int user_level, user_id;
    unsigned int i = 0, j = 0;
    uint8_t *conf_levels = nullptr;
    unsigned char *user_id_tracker = nullptr;
    struct pal_st_phrase_sound_model *phrase_sm = nullptr;

    PAL_DBG(LOG_TAG, "Enter");

    if (!config) {
        status = -EINVAL;
        PAL_ERR(LOG_TAG, "invalid input status %d", status);
        goto exit;
    }

    for (auto& eng: engines_) {
        if (eng->GetEngineId() == ST_SM_ID_SVA_F_STAGE_GMM) {
            phrase_sm = (struct pal_st_phrase_sound_model *)eng->sm_data_;
            break;
        }
    }

    if ((config->num_phrases == 0) ||
        (phrase_sm && config->num_phrases > phrase_sm->num_phrases)) {
        status = -EINVAL;
        PAL_ERR(LOG_TAG, "Invalid phrase data status %d", status);
        goto exit;
    }

    for (i = 0; i < config->num_phrases; i++) {
        num_conf_levels++;
        if (model_id_ == 0) {
            for (j = 0; j < config->phrases[i].num_levels; j++)
                num_conf_levels++;
        }
    }

    conf_levels = (unsigned char*)calloc(1, num_conf_levels);
    if (!conf_levels) {
        status = -ENOMEM;
        PAL_ERR(LOG_TAG, "conf_levels calloc failed, status %d", status);
        goto exit;
    }

    user_id_tracker = (unsigned char *)calloc(1, num_conf_levels);
    if (!user_id_tracker) {
        status = -ENOMEM;
        PAL_ERR(LOG_TAG, "failed to allocate user_id_tracker status %d",
                status);
        goto exit;
    }

    for (i = 0; i < config->num_phrases; i++) {
        PAL_VERBOSE(LOG_TAG, "[%d] kw level %d", i,
        config->phrases[i].confidence_level);
        if (config->phrases[i].confidence_level > ST_MAX_FSTAGE_CONF_LEVEL) {
            PAL_ERR(LOG_TAG, "Invalid kw level %d",
                config->phrases[i].confidence_level);
            status = -EINVAL;
            goto exit;
        }
        for (j = 0; j < config->phrases[i].num_levels; j++) {
            PAL_VERBOSE(LOG_TAG, "[%d] user_id %d level %d ", i,
                        config->phrases[i].levels[j].user_id,
                        config->phrases[i].levels[j].level);
            if (config->phrases[i].levels[j].level > ST_MAX_FSTAGE_CONF_LEVEL) {
                PAL_ERR(LOG_TAG, "Invalid user level %d",
                    config->phrases[i].levels[j].level);
                status = -EINVAL;
                goto exit;
            }
        }
    }

    /* Example: Say the recognition structure has 3 keywords with users
     *      [0] k1 |uid|
     *              [0] u1 - 1st trainer
     *              [1] u2 - 4th trainer
     *              [3] u3 - 3rd trainer
     *      [1] k2
     *              [2] u2 - 2nd trainer
     *              [4] u3 - 5th trainer
     *      [2] k3
     *              [5] u4 - 6th trainer
     *    Output confidence level array will be
     *    [k1, k2, k3, u1k1, u2k1, u2k2, u3k1, u3k2, u4k3]
     */

    for (i = 0; i < config->num_phrases; i++) {
        conf_levels[i] = config->phrases[i].confidence_level;
        if (model_id_ == 0) {
            for (j = 0; j < config->phrases[i].num_levels; j++) {
                user_level = config->phrases[i].levels[j].level;
                user_id = config->phrases[i].levels[j].user_id;
                if ((user_id < config->num_phrases) ||
                     (user_id >= num_conf_levels)) {
                    status = -EINVAL;
                    PAL_ERR(LOG_TAG, "Invalid params user id %d status %d",
                            user_id, status);
                    goto exit;
                } else {
                    if (user_id_tracker[user_id] == 1) {
                        status = -EINVAL;
                        PAL_ERR(LOG_TAG, "Duplicate user id %d status %d", user_id,
                                status);
                        goto exit;
                    }
                    conf_levels[user_id] = (user_level < ST_MAX_FSTAGE_CONF_LEVEL) ?
                        user_level : ST_MAX_FSTAGE_CONF_LEVEL;
                    user_id_tracker[user_id] = 1;
                    PAL_VERBOSE(LOG_TAG, "user_conf_levels[%d] = %d", user_id,
                                conf_levels[user_id]);
                }
            }
        }
    }

    *out_conf_levels = conf_levels;
    *out_num_conf_levels = num_conf_levels;

exit:
    if (status && conf_levels) {
        free(conf_levels);
        *out_conf_levels = nullptr;
        *out_num_conf_levels = 0;
    }

    if (user_id_tracker)
        free(user_id_tracker);

    PAL_DBG(LOG_TAG, "Exit, status %d", status);

    return status;
}

int32_t StreamSoundTrigger::FillOpaqueConfLevels(
    const void *sm_levels_generic,
    uint8_t **out_payload,
    uint32_t *out_payload_size,
    uint32_t version) {

    int status = 0;
    int32_t level = 0;
    unsigned int num_conf_levels = 0;
    unsigned int user_level = 0, user_id = 0;
    unsigned char *conf_levels = nullptr;
    unsigned int i = 0, j = 0;
    unsigned char *user_id_tracker = nullptr;
    struct st_sound_model_conf_levels *sm_levels = nullptr;
    struct st_sound_model_conf_levels_v2 *sm_levels_v2 = nullptr;

    PAL_VERBOSE(LOG_TAG, "Enter");

    /*  Example: Say the recognition structure has 3 keywords with users
     *  |kid|
     *  [0] k1 |uid|
     *         [3] u1 - 1st trainer
     *         [4] u2 - 4th trainer
     *         [6] u3 - 3rd trainer
     *  [1] k2
     *         [5] u2 - 2nd trainer
     *         [7] u3 - 5th trainer
     *  [2] k3
     *         [8] u4 - 6th trainer
     *
     *  Output confidence level array will be
     *  [k1, k2, k3, u1k1, u2k1, u2k2, u3k1, u3k2, u4k3]
     */

    if (version != CONF_LEVELS_INTF_VERSION_0002) {
        sm_levels = (struct st_sound_model_conf_levels *)sm_levels_generic;
        if (!sm_levels) {
            status = -EINVAL;
            PAL_ERR(LOG_TAG, "ERROR. Invalid inputs");
            goto exit;
        }

        for (i = 0; i < sm_levels->num_kw_levels; i++) {
            level = sm_levels->kw_levels[i].kw_level;
            if (level < 0 || level > ST_MAX_FSTAGE_CONF_LEVEL) {
                PAL_ERR(LOG_TAG, "Invalid First stage [%d] kw level %d", i, level);
                status = -EINVAL;
                goto exit;
            } else {
                PAL_DBG(LOG_TAG, "First stage [%d] kw level %d", i, level);
            }
            for (j = 0; j < sm_levels->kw_levels[i].num_user_levels; j++) {
                level = sm_levels->kw_levels[i].user_levels[j].level;
                if (level < 0 || level > ST_MAX_FSTAGE_CONF_LEVEL) {
                    PAL_ERR(LOG_TAG, "Invalid First stage [%d] user_id %d level %d", i,
                        sm_levels->kw_levels[i].user_levels[j].user_id, level);
                    status = -EINVAL;
                    goto exit;
                } else {
                    PAL_DBG(LOG_TAG, "First stage [%d] user_id %d level %d ", i,
                        sm_levels->kw_levels[i].user_levels[j].user_id, level);
                }
            }
        }

        for (i = 0; i < sm_levels->num_kw_levels; i++) {
            num_conf_levels++;
            if (model_id_ == 0) {
                for (j = 0; j < sm_levels->kw_levels[i].num_user_levels; j++)
                    num_conf_levels++;
            }
        }

        PAL_DBG(LOG_TAG, "Number of confidence levels : %d", num_conf_levels);

        if (!num_conf_levels) {
            status = -EINVAL;
            PAL_ERR(LOG_TAG, "ERROR. Invalid num_conf_levels input");
            goto exit;
        }

        conf_levels = (unsigned char*)calloc(1, num_conf_levels);
        if (!conf_levels) {
            status = -ENOMEM;
            PAL_ERR(LOG_TAG, "conf_levels calloc failed, status %d", status);
            goto exit;
        }

        user_id_tracker = (unsigned char *)calloc(1, num_conf_levels);
        if (!user_id_tracker) {
            status = -ENOMEM;
            PAL_ERR(LOG_TAG, "failed to allocate user_id_tracker status %d",
                    status);
            goto exit;
        }

        for (i = 0; i < sm_levels->num_kw_levels; i++) {
            if (i < num_conf_levels) {
                conf_levels[i] = sm_levels->kw_levels[i].kw_level;
            } else {
                status = -EINVAL;
                PAL_ERR(LOG_TAG, "ERROR. Invalid numver of kw levels");
                goto exit;
            }
            if (model_id_ == 0) {
                for (j = 0; j < sm_levels->kw_levels[i].num_user_levels; j++) {
                    user_level = sm_levels->kw_levels[i].user_levels[j].level;
                    user_id = sm_levels->kw_levels[i].user_levels[j].user_id;
                    if ((user_id < sm_levels->num_kw_levels) ||
                        (user_id >= num_conf_levels)) {
                        status = -EINVAL;
                        PAL_ERR(LOG_TAG, "ERROR. Invalid params user id %d>%d",
                                user_id, num_conf_levels);
                        goto exit;
                    } else {
                        if (user_id_tracker[user_id] == 1) {
                            status = -EINVAL;
                            PAL_ERR(LOG_TAG, "ERROR. Duplicate user id %d",
                                    user_id);
                            goto exit;
                        }
                        conf_levels[user_id] = user_level;
                        user_id_tracker[user_id] = 1;
                        PAL_ERR(LOG_TAG, "user_conf_levels[%d] = %d",
                                user_id, conf_levels[user_id]);
                    }
                }
            }
        }
    } else {
        sm_levels_v2 =
            (struct st_sound_model_conf_levels_v2 *)sm_levels_generic;
        if (!sm_levels_v2) {
            status = -EINVAL;
            PAL_ERR(LOG_TAG, "ERROR. Invalid inputs");
            goto exit;
        }

        for (i = 0; i < sm_levels_v2->num_kw_levels; i++) {
            level = sm_levels_v2->kw_levels[i].kw_level;
            if (level < 0 || level > ST_MAX_FSTAGE_CONF_LEVEL) {
                PAL_ERR(LOG_TAG, "Invalid First stage [%d] kw level %d", i, level);
                status = -EINVAL;
                goto exit;
            } else {
                PAL_DBG(LOG_TAG, "First stage [%d] kw level %d", i, level);
            }
            for (j = 0; j < sm_levels_v2->kw_levels[i].num_user_levels; j++) {
                level = sm_levels_v2->kw_levels[i].user_levels[j].level;
                if (level < 0 || level > ST_MAX_FSTAGE_CONF_LEVEL) {
                    PAL_ERR(LOG_TAG, "Invalid First stage [%d] user_id %d level %d", i,
                        sm_levels_v2->kw_levels[i].user_levels[j].user_id, level);
                    status = -EINVAL;
                    goto exit;
                } else {
                    PAL_DBG(LOG_TAG, "First stage [%d] user_id %d level %d ", i,
                        sm_levels_v2->kw_levels[i].user_levels[j].user_id, level);
                }
            }
        }

        for (i = 0; i < sm_levels_v2->num_kw_levels; i++) {
            num_conf_levels++;
            if (model_id_ == 0) {
                for (j = 0; j < sm_levels_v2->kw_levels[i].num_user_levels; j++)
                    num_conf_levels++;
            }
        }

        PAL_DBG(LOG_TAG,"number of confidence levels : %d", num_conf_levels);

        if (!num_conf_levels) {
            status = -EINVAL;
            PAL_ERR(LOG_TAG, "ERROR. Invalid num_conf_levels input");
            goto exit;
        }

        conf_levels = (unsigned char*)calloc(1, num_conf_levels);
        if (!conf_levels) {
            status = -ENOMEM;
            PAL_ERR(LOG_TAG, "conf_levels calloc failed, status %d", status);
            goto exit;
        }

        user_id_tracker = (unsigned char *)calloc(1, num_conf_levels);
        if (!user_id_tracker) {
            status = -ENOMEM;
            PAL_ERR(LOG_TAG, "failed to allocate user_id_tracker status %d",
                    status);
            goto exit;
        }

        for (i = 0; i < sm_levels_v2->num_kw_levels; i++) {
            if (i < num_conf_levels) {
                conf_levels[i] = sm_levels_v2->kw_levels[i].kw_level;
            } else {
                status = -EINVAL;
                PAL_ERR(LOG_TAG, "ERROR. Invalid numver of kw levels");
                goto exit;
            }
            if (model_id_ == 0) {
                for (j = 0; j < sm_levels_v2->kw_levels[i].num_user_levels; j++) {
                    user_level = sm_levels_v2->kw_levels[i].user_levels[j].level;
                    user_id = sm_levels_v2->kw_levels[i].user_levels[j].user_id;
                    if ((user_id < sm_levels_v2->num_kw_levels) ||
                         (user_id >= num_conf_levels)) {
                        status = -EINVAL;
                        PAL_ERR(LOG_TAG, "ERROR. Invalid params user id %d>%d",
                              user_id, num_conf_levels);
                        goto exit;
                    } else {
                        if (user_id_tracker[user_id] == 1) {
                            status = -EINVAL;
                            PAL_ERR(LOG_TAG, "ERROR. Duplicate user id %d",
                                user_id);
                            goto exit;
                        }
                        conf_levels[user_id] = user_level;
                        user_id_tracker[user_id] = 1;
                        PAL_VERBOSE(LOG_TAG, "user_conf_levels[%d] = %d",
                        user_id, conf_levels[user_id]);
                    }
                }
            }
        }
    }

    *out_payload = conf_levels;
    *out_payload_size = num_conf_levels;
    PAL_DBG(LOG_TAG, "Returning number of conf levels : %d", *out_payload_size);
exit:
    if (status && conf_levels) {
        free(conf_levels);
        *out_payload = nullptr;
        *out_payload_size = 0;
    }

    if (user_id_tracker)
        free(user_id_tracker);

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
    if (st_info_->GetSupportDevSwitch() &&
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

            st_stream_.sm_cfg_ = st_stream_.st_info_->GetSmConfig(uuid);

            if (!st_stream_.sm_cfg_) {
                PAL_ERR(LOG_TAG, "Failed to get sound model platform info");
                status = -EINVAL;
                goto err_exit;
            }

            if (!st_stream_.mDevices.size()) {
                struct pal_device* dattr = new (struct pal_device);
                std::shared_ptr<Device> dev = nullptr;

                // update best device
                pal_device_id_t dev_id = st_stream_.GetAvailCaptureDevice();
                PAL_DBG(LOG_TAG, "Select available caputre device %d", dev_id);

                dev = st_stream_.GetPalDevice(dev_id, dattr, false);
                if (!dev) {
                    PAL_ERR(LOG_TAG, "Device creation is failed");
                    status = -EINVAL;
                    goto err_exit;
                }
                st_stream_.mDevices.push_back(dev);
                dev = nullptr;
                delete dattr;
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
            struct pal_device *pal_dev = new struct pal_device;
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

            dev = st_stream_.GetPalDevice(dev_id, pal_dev, false);
            if (!dev) {
                PAL_ERR(LOG_TAG, "Device creation failed");
                status = -EINVAL;
                goto connect_err;
            }

            st_stream_.mDevices.push_back(dev);
        connect_err:
            delete pal_dev;
            break;
        }
        case ST_EV_CONCURRENT_STREAM:
        case ST_EV_CHARGING_STATE: {
            // Avoid handling concurrency before sound model loaded
            if (!st_stream_.sm_config_)
                break;
            std::shared_ptr<CaptureProfile> new_cap_prof = nullptr;
            bool active = false;

            if (ev_cfg->id_ == ST_EV_CONCURRENT_STREAM) {
                StConcurrentStreamEventConfigData *data =
                    (StConcurrentStreamEventConfigData *)ev_cfg->data_.get();
                active = data->is_active_;
            } else if (ev_cfg->id_ == ST_EV_CHARGING_STATE) {
                StChargingStateEventConfigData *data =
                    (StChargingStateEventConfigData *)ev_cfg->data_.get();
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
                        struct pal_device dattr;
                        std::shared_ptr<Device> dev = nullptr;

                        // update best device
                        pal_device_id_t dev_id = st_stream_.GetAvailCaptureDevice();
                        PAL_DBG(LOG_TAG, "Select available caputre device %d", dev_id);

                        dev = st_stream_.GetPalDevice(dev_id, &dattr, false);
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

                    status = st_stream_.gsl_engine_->UpdateConfLevels(&st_stream_,
                        st_stream_.rec_config_, st_stream_.gsl_conf_levels_,
                        st_stream_.gsl_conf_levels_size_);
                    if (0 != status) {
                        PAL_ERR(LOG_TAG, "Failed to update conf levels, status %d",
                            status);
                        goto err_unload;
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

            if (st_stream_.device_opened_ && st_stream_.mDevices.size() > 0) {
                status = st_stream_.mDevices[0]->close();
                if (0 != status) {
                    PAL_ERR(LOG_TAG, "Failed to close device, status %d",
                        status);
                }
            }

            st_stream_.mDevices.clear();

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

            st_stream_.gsl_engine_->ResetBufferReaders(st_stream_.reader_list_);
            if (st_stream_.reader_) {
                delete st_stream_.reader_;
                st_stream_.reader_ = nullptr;
            }
            st_stream_.engines_.clear();
            st_stream_.gsl_engine_->DetachStream(&st_stream_, true);
            st_stream_.reader_list_.clear();
            if (st_stream_.sm_info_) {
                delete st_stream_.sm_info_;
                st_stream_.sm_info_ = nullptr;
            }

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
                } else {
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
            struct pal_device *pal_dev = new struct pal_device;
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

            dev = st_stream_.GetPalDevice(dev_id, pal_dev, false);
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
                st_stream_.rm->registerDevice(dev, &st_stream_);
                TransitTo(ST_STATE_ACTIVE);
            }
        connect_err:
            delete pal_dev;
            break;
        }
        case ST_EV_CONCURRENT_STREAM:
        case ST_EV_CHARGING_STATE: {
            std::shared_ptr<CaptureProfile> new_cap_prof = nullptr;
            bool active = false;

            if (ev_cfg->id_ == ST_EV_CONCURRENT_STREAM) {
                StConcurrentStreamEventConfigData *data =
                    (StConcurrentStreamEventConfigData *)ev_cfg->data_.get();
                active = data->is_active_;
            } else if (ev_cfg->id_ == ST_EV_CHARGING_STATE) {
                StChargingStateEventConfigData *data =
                    (StChargingStateEventConfigData *)ev_cfg->data_.get();
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
                data->is_enable_);
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

                st_stream_.rm->deregisterDevice(dev, &st_stream_);

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
                data->is_enable_);
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
            struct pal_device *pal_dev = new struct pal_device;
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

            dev = st_stream_.GetPalDevice(dev_id, pal_dev, false);
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
            } else {
                st_stream_.rm->registerDevice(dev, &st_stream_);
            }
        connect_err:
            delete pal_dev;
            break;
        }
        case ST_EV_CONCURRENT_STREAM:
        case ST_EV_CHARGING_STATE: {
            std::shared_ptr<CaptureProfile> new_cap_prof = nullptr;
            bool active = false;

            if (ev_cfg->id_ == ST_EV_CONCURRENT_STREAM) {
                StConcurrentStreamEventConfigData *data =
                    (StConcurrentStreamEventConfigData *)ev_cfg->data_.get();
                active = data->is_active_;
            } else if (ev_cfg->id_ == ST_EV_CHARGING_STATE) {
                StChargingStateEventConfigData *data =
                    (StChargingStateEventConfigData *)ev_cfg->data_.get();
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

                st_stream_.rm->deregisterDevice(dev, &st_stream_);

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

                st_stream_.rm->deregisterDevice(dev, &st_stream_);

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
        case ST_EV_CHARGING_STATE:
        case ST_EV_DEVICE_DISCONNECTED:
        case ST_EV_DEVICE_CONNECTED: {
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

                st_stream_.rm->deregisterDevice(dev, &st_stream_);

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
                data->is_enable_);
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
            if (st_stream_.st_info_->GetEnableDebugDumps()) {
                ST_DBG_FILE_WRITE(st_stream_.lab_fd_, buf->buffer, buf->size);
            }
            break;
        }
        case ST_EV_START_RECOGNITION: {
            /*
             * Can happen if client requests next recognition without any config
             * change with/without reading buffers after sending detection event.
             */
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
            /*
             * Can happen if client doesn't read buffers after sending detection
             * event, but requests next recognition with config change.
             * Get to loaded state as START event will start the recognition.
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

                st_stream_.rm->deregisterDevice(dev, &st_stream_);

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
            // Possible with deffered stop if client doesn't start next recognition.
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

                st_stream_.rm->deregisterDevice(dev, &st_stream_);

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
                        eng->GetEngine()->GetEngineType() & ST_SM_ID_SVA_S_STAGE_KWD) ||
                        (data->det_type_ == KEYWORD_DETECTION_REJECT &&
                        eng->GetEngine()->GetEngineType() & ST_SM_ID_SVA_S_STAGE_USER)) {

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

                if (st_stream_.st_info_->GetNotifySecondStageFailure()) {
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
        case ST_EV_CHARGING_STATE:
        case ST_EV_CONCURRENT_STREAM:
        case ST_EV_DEVICE_DISCONNECTED:
        case ST_EV_DEVICE_CONNECTED: {
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

                st_stream_.rm->deregisterDevice(dev, &st_stream_);

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
                data->is_enable_);
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
            }

            if (st_stream_.state_for_restore_ == ST_STATE_ACTIVE) {
                status = st_stream_.SendRecognitionConfig(
                    st_stream_.rec_config_);
                if (0 != status) {
                    PAL_ERR(LOG_TAG,
                        "Failed to send recognition config, status %d", status);
                    break;
                }
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
