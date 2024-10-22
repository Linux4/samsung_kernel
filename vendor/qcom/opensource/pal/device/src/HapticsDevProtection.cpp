/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the
 * disclaimer below) provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.

 *   * Neither the name of Qualcomm Innovation Center, Inc. nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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

#define LOG_TAG "PAL: HapticsDevProtection"


#include "HapticsDevProtection.h"
#include "PalAudioRoute.h"
#include "ResourceManager.h"
#include "SessionAlsaUtils.h"
#include "kvh2xml.h"
#include <agm/agm_api.h>

#include<fstream>
#include<sstream>

#ifndef PAL_HAP_DEVP_TEMP_PATH
#define PAL_HAP_DEVP_TEMP_PATH "/data/misc/audio/audio.cal"
#endif

#define PAL_HP_VI_PER_PATH "/data/vendor/audio/haptics_persistent.cal"

#define MIN_HAPTICS_DEV_IDLE_SEC (60 * 30)
#define WAKEUP_MIN_IDLE_CHECK (1000 * 30)

#define HAPTICS_DEV_RIGHT_WSA_TEMP "HapticsDevRight WSA Temp"
#define HAPTICS_DEV_LEFT_WSA_TEMP "HapticsDevLeft WSA Temp"

#define HAPTICS_DEV_RIGHT_WSA_DEV_NUM "HapticsDevRight WSA Get DevNum"
#define HAPTICS_DEV_LEFT_WSA_DEV_NUM "HapticsDevLeft WSA Get DevNum"

#define TZ_TEMP_MIN_THRESHOLD    (-30)
#define TZ_TEMP_MAX_THRESHOLD    (80)

/*Set safe temp value to 40C*/
#define SAFE_HAPTICS_DEV_TEMP 40
#define SAFE_HAPTICS_DEV_TEMP_Q6 (SAFE_HAPTICS_DEV_TEMP * (1 << 6))

#define MIN_RESISTANCE_HAPTICS_DEV_Q24 (2 * (1 << 24))

#define DEFAULT_PERIOD_SIZE 256
#define DEFAULT_PERIOD_COUNT 4

#define NORMAL_MODE 0
#define CALIBRATION_MODE 1
#define FACTORY_TEST_MODE 2

#define CALIBRATION_STATUS_SUCCESS 3
#define CALIBRATION_STATUS_FAILURE 4

std::thread HapticsDevProtection::mCalThread;
std::condition_variable HapticsDevProtection::cv;
std::mutex HapticsDevProtection::cvMutex;
std::mutex HapticsDevProtection::calibrationMutex;

bool HapticsDevProtection::isHapDevInUse;
bool HapticsDevProtection::calThrdCreated;
bool HapticsDevProtection::isDynamicCalTriggered = false;
struct timespec HapticsDevProtection::devLastTimeUsed;
struct mixer *HapticsDevProtection::virtMixer;
struct mixer *HapticsDevProtection::hwMixer;
haptics_dev_prot_cal_state HapticsDevProtection::hapticsDevCalState;
struct pcm * HapticsDevProtection::rxPcm = NULL;
struct pcm * HapticsDevProtection::txPcm = NULL;
struct param_id_haptics_th_vi_r0_get_param_t * HapticsDevProtection::callback_data;
int HapticsDevProtection::numberOfChannels;
struct pal_device_info HapticsDevProtection::vi_device;
int HapticsDevProtection::calibrationCallbackStatus;
int HapticsDevProtection::numberOfRequest;
bool HapticsDevProtection::mDspCallbackRcvd;
std::shared_ptr<Device> HapticsDevFeedback::obj = nullptr;
int HapticsDevFeedback::numDevice;

std::string getDefaultHapticsDevTempCtrl(uint8_t haptics_dev_pos)
{
    switch(haptics_dev_pos)
    {
        case HAPTICS_DEV_LEFT:
            return std::string(HAPTICS_DEV_LEFT_WSA_TEMP);
        break;
        case HAPTICS_DEV_RIGHT:
            [[fallthrough]];
        default:
            return std::string(HAPTICS_DEV_RIGHT_WSA_TEMP);
    }
}

/* Function to check if  HapticsDevice is in use or not.
 * It returns the time as well for which HapticsDevice is not in use.
 */
bool HapticsDevProtection::isHapticsDevInUse(unsigned long *sec)
{
    struct timespec temp;
    PAL_DBG(LOG_TAG, "Enter");

    if (!sec) {
        PAL_ERR(LOG_TAG, "Improper argument");
        return false;
    }

    if (isHapDevInUse) {
        PAL_INFO(LOG_TAG, " HapticsDevice in use");
        *sec = 0;
        return true;
    } else {
        PAL_INFO(LOG_TAG, " HapticsDevice not in use");
        clock_gettime(CLOCK_BOOTTIME, &temp);
        *sec = temp.tv_sec - devLastTimeUsed.tv_sec;
    }

    PAL_DBG(LOG_TAG, "Idle time %ld", *sec);

    return false;
}

/* Function to set status of HapticsDevice */
void HapticsDevProtection::HapticsDevProtSetDevStatus(bool enable)
{
    PAL_DBG(LOG_TAG, "Enter");

    if (enable)
        isHapDevInUse = true;
    else {
        isHapDevInUse = false;
        clock_gettime(CLOCK_BOOTTIME, &devLastTimeUsed);
        PAL_INFO(LOG_TAG, " HapticsDevice used last time %ld", devLastTimeUsed.tv_sec);
    }

    PAL_DBG(LOG_TAG, "Exit");
}

/* Wait function for WAKEUP_MIN_IDLE_CHECK  */
void HapticsDevProtection::HapticsDevCalibrateWait()
{
    std::unique_lock<std::mutex> lock(cvMutex);
    cv.wait_for(lock,
            std::chrono::milliseconds(WAKEUP_MIN_IDLE_CHECK));
}

// Callback from DSP for Ressistance value
void HapticsDevProtection::handleHPCallback (uint64_t hdl __unused, uint32_t event_id,
                                            void *event_data, uint32_t event_size)
{
    param_id_haptics_th_vi_r0_get_param_t  *param_data = nullptr;

    PAL_DBG(LOG_TAG, "Got event from DSP %x", event_id);

    if (event_id == EVENT_ID_HAPTICS_VI_CALIBRATION) {
        // Received callback for Calibration state
        param_data = (param_id_haptics_th_vi_r0_get_param_t  *) event_data;
        PAL_DBG(LOG_TAG, "Calibration state %d", param_data->state);

        if (param_data->state == CALIBRATION_STATUS_SUCCESS) {
            PAL_DBG(LOG_TAG, "Calibration is successfull");
            callback_data = (param_id_haptics_th_vi_r0_get_param_t  *) calloc(1, event_size);
            if (!callback_data) {
                PAL_ERR(LOG_TAG, "Unable to allocate memory");
            } else {
                callback_data->state = param_data->state;
                callback_data->r0_cali_q24[0] = param_data->r0_cali_q24[0];
                }
            }
            mDspCallbackRcvd = true;
            calibrationCallbackStatus = CALIBRATION_STATUS_SUCCESS;
            cv.notify_all();
    } else {
            PAL_DBG(LOG_TAG, "Calibration is unsuccessfull");
            // Restart the calibration and abort current run.
            mDspCallbackRcvd = true;
            calibrationCallbackStatus = CALIBRATION_STATUS_FAILURE;
            cv.notify_all();
    }
}


int HapticsDevProtection::getDevTemperature(int haptics_dev_pos)
{
    struct mixer_ctl *ctl;
    std::string mixer_ctl_name;
    int status = 0;

    PAL_DBG(LOG_TAG, "Enter  HapticsDevice Get Temperature %d", haptics_dev_pos);
    if (mixer_ctl_name.empty()) {
        PAL_DBG(LOG_TAG, "Using default mixer control");
        mixer_ctl_name = getDefaultHapticsDevTempCtrl(haptics_dev_pos);
    }

    PAL_DBG(LOG_TAG, "audio_mixer %pK", hwMixer);

    ctl = mixer_get_ctl_by_name(hwMixer, mixer_ctl_name.c_str());
    if (!ctl) {
        PAL_ERR(LOG_TAG, "Invalid mixer control: %s\n", mixer_ctl_name.c_str());
        status = -EINVAL;
        return status;
    }

    status = mixer_ctl_get_value(ctl, 0);

    PAL_DBG(LOG_TAG, "Exiting  HapticsDevice Get Temperature %d", status);

    return status;
}

void HapticsDevProtection::disconnectFeandBe(std::vector<int> pcmDevIds,
                                         std::string backEndName) {

    std::ostringstream disconnectCtrlName;
    std::ostringstream disconnectCtrlNameBe;
    struct mixer_ctl *disconnectCtrl = NULL;
    struct mixer_ctl *beMetaDataMixerCtrl = nullptr;
    struct agmMetaData deviceMetaData(nullptr, 0);
    uint32_t devicePropId[] = { 0x08000010, 2, 0x2, 0x5 };
    std::vector <std::pair<int, int>> emptyKV;
    int ret = 0;

    emptyKV.clear();

    SessionAlsaUtils::getAgmMetaData(emptyKV, emptyKV,
                    (struct prop_data *)devicePropId, deviceMetaData);
    if (!deviceMetaData.size) {
        ret = -ENOMEM;
        PAL_ERR(LOG_TAG, "Error: %d, Device metadata is zero", ret);
        goto exit;
    }

    disconnectCtrlNameBe<< backEndName << " metadata";
    beMetaDataMixerCtrl = mixer_get_ctl_by_name(virtMixer, disconnectCtrlNameBe.str().data());
    if (!beMetaDataMixerCtrl) {
        ret = -EINVAL;
        PAL_ERR(LOG_TAG, "Error: %d, invalid mixer control %s", ret, backEndName.c_str());
        goto exit;
    }

    if (deviceMetaData.size) {
        ret = mixer_ctl_set_array(beMetaDataMixerCtrl, (void *)deviceMetaData.buf,
                    deviceMetaData.size);
        free(deviceMetaData.buf);
        deviceMetaData.buf = nullptr;
    }else {
        PAL_ERR(LOG_TAG, "Error: %d, Device Metadata not cleaned up", ret);
        goto exit;
    }

    disconnectCtrlName << "PCM" << pcmDevIds.at(0) << " disconnect";
    disconnectCtrl = mixer_get_ctl_by_name(virtMixer, disconnectCtrlName.str().data());
    if (!disconnectCtrl) {
        ret = -EINVAL;
        PAL_ERR(LOG_TAG, "Error: %d, invalid mixer control: %s", ret, disconnectCtrlName.str().data());
        goto exit;
    }
    ret = mixer_ctl_set_enum_by_string(disconnectCtrl, backEndName.c_str());
    if (ret) {
        PAL_ERR(LOG_TAG, "Error: %d, Mixer control %s set with %s failed", ret,
        disconnectCtrlName.str().data(), backEndName.c_str());
    }
exit:
    return;
}

int HapticsDevProtection::HapticsDevStartCalibration()
{
    FILE *fp;
    struct pal_device device, deviceRx;
    struct pal_channel_info ch_info;
    struct pal_stream_attributes sAttr;
    struct pcm_config config;
    struct mixer_ctl *connectCtrl = NULL;
    struct audio_route *audioRoute = NULL;
    struct agm_event_reg_cfg event_cfg;
    struct agmMetaData deviceMetaData(nullptr, 0);
    struct mixer_ctl *beMetaDataMixerCtrl = nullptr;
    int ret = 0, status = 0, dir = 0, i = 0, flags = 0, payload_size = 0;
    uint32_t miid = 0;
    char mSndDeviceName_rx[128] = {0};
    char mSndDeviceName_vi[128] = {0};
    uint8_t* payload = NULL;
    size_t payloadSize = 0;
    uint32_t devicePropId[] = {0x08000010, 1, 0x2};
    bool isTxStarted = false, isRxStarted = false;
    bool isTxFeandBeConnected = false, isRxFeandBeConnected = false;
    std::string backEndNameTx, backEndNameRx;
    std::vector <std::pair<int, int>> keyVector, calVector;
    std::vector<int> pcmDevIdsRx, pcmDevIdsTx;
    std::shared_ptr<ResourceManager> rm;
    std::ostringstream connectCtrlName;
    std::ostringstream connectCtrlNameRx;
    std::ostringstream connectCtrlNameBe;
    std::ostringstream connectCtrlNameBeVI;
    param_id_haptics_vi_op_mode_param_t modeConfg;
    param_id_haptics_op_mode HpRxModeConfg;
    param_id_haptics_vi_channel_map_cfg_t HapticsviChannelMapConfg;
    param_id_haptics_vi_ex_FTM_mode_param_t HapticsviExModeConfg;
    session_callback sessionCb;

    std::unique_lock<std::mutex> calLock(calibrationMutex);

    memset(&device, 0, sizeof(device));
    memset(&deviceRx, 0, sizeof(deviceRx));
    memset(&sAttr, 0, sizeof(sAttr));
    memset(&config, 0, sizeof(config));
    memset(&modeConfg, 0, sizeof(modeConfg));
    memset(&HapticsviChannelMapConfg, 0, sizeof(HapticsviChannelMapConfg));
    memset(&HpRxModeConfg, 0, sizeof(HpRxModeConfg));
    memset(&HapticsviExModeConfg, 0, sizeof(HapticsviExModeConfg));

    sessionCb = handleHPCallback;
    PayloadBuilder* builder = new PayloadBuilder();

    keyVector.clear();
    calVector.clear();

    PAL_DBG(LOG_TAG, "Enter");

    if (customPayloadSize) {
        free(customPayload);
        customPayloadSize = 0;
    }

    rm = ResourceManager::getInstance();
    if (!rm) {
        PAL_ERR(LOG_TAG, "Error: %d Failed to get resource manager instance", -EINVAL);
        goto exit;
    }

    // Configure device attribute
    rm->getChannelMap(&(ch_info.ch_map[0]), vi_device.channels);
    switch (vi_device.channels) {
        case 1 :
            ch_info.channels = CHANNELS_1;
        break;
        case 2 :
            ch_info.channels = CHANNELS_2;
        break;
        default:
            PAL_DBG(LOG_TAG, "Unsupported channel. Set default as 2");
            ch_info.channels = CHANNELS_2;
        break;
    }

    device.config.ch_info = ch_info;
    device.config.sample_rate = vi_device.samplerate;
    device.config.bit_width = vi_device.bit_width;
    device.config.aud_fmt_id = rm->getAudioFmt(vi_device.bit_width);

    // Setup TX path
    ret = rm->getAudioRoute(&audioRoute);
    if (0 != ret) {
        PAL_ERR(LOG_TAG, "Failed to get the audio_route name status %d", ret);
        goto exit;
    }

    device.id = PAL_DEVICE_IN_HAPTICS_VI_FEEDBACK;
    ret = rm->getSndDeviceName(device.id , mSndDeviceName_vi);
    if (0 != ret) {
        PAL_ERR(LOG_TAG, "Failed to obtain tx snd device name for %d", device.id);
        goto exit;
    }

    PAL_DBG(LOG_TAG, "got the audio_route name %s", mSndDeviceName_vi);

    rm->getBackendName(device.id, backEndNameTx);
    if (!strlen(backEndNameTx.c_str())) {
        PAL_ERR(LOG_TAG, "Failed to obtain tx backend name for %d", device.id);
        goto exit;
    }

    ret = PayloadBuilder::getDeviceKV(device.id, keyVector);
    if (0 != ret) {
        PAL_ERR(LOG_TAG, "Failed to obtain device KV for %d", device.id);
        goto exit;
    }

    // Enable VI module
    //TBD
    switch(numberOfChannels) {
        case 1 :
            // TODO: check it from RM.xml for left or right configuration
            calVector.push_back(std::make_pair(HAPTICS_PRO_VI_MAP, HAPTICS_VI_LEFT_MONO));
        break;
        case 2 :
            calVector.push_back(std::make_pair(HAPTICS_PRO_VI_MAP, HAPTICS_VI_LEFT_RIGHT));
        break;
        default :
            PAL_ERR(LOG_TAG, "Unsupported channel");
            goto exit;
    }

    SessionAlsaUtils::getAgmMetaData(keyVector, calVector,
                    (struct prop_data *)devicePropId, deviceMetaData);
    if (!deviceMetaData.size) {
        PAL_ERR(LOG_TAG, "VI device metadata is zero");
        ret = -ENOMEM;
        goto exit;
    }

    connectCtrlNameBeVI<< backEndNameTx << " metadata";
    beMetaDataMixerCtrl = mixer_get_ctl_by_name(virtMixer, connectCtrlNameBeVI.str().data());
    if (!beMetaDataMixerCtrl) {
        PAL_ERR(LOG_TAG, "invalid mixer control for VI : %s", backEndNameTx.c_str());
        ret = -EINVAL;
        goto exit;
    }

    if (deviceMetaData.size) {
        ret = mixer_ctl_set_array(beMetaDataMixerCtrl, (void *)deviceMetaData.buf,
                    deviceMetaData.size);
        free(deviceMetaData.buf);
        deviceMetaData.buf = nullptr;
    }
    else {
        PAL_ERR(LOG_TAG, "Device Metadata not set for TX path");
        ret = -EINVAL;
        goto exit;
    }

    ret = SessionAlsaUtils::setDeviceMediaConfig(rm, backEndNameTx, &device);
    if (ret) {
        PAL_ERR(LOG_TAG, "setDeviceMediaConfig for feedback device failed");
        goto exit;
    }

    sAttr.type = PAL_STREAM_HAPTICS;
    sAttr.direction = PAL_AUDIO_INPUT_OUTPUT;
    dir = TX_HOSTLESS;
    pcmDevIdsTx = rm->allocateFrontEndIds(sAttr, dir);
    if (pcmDevIdsTx.size() == 0) {
        PAL_ERR(LOG_TAG, "allocateFrontEndIds failed");
        ret = -ENOSYS;
        goto exit;
    }

    connectCtrlName << "PCM" << pcmDevIdsTx.at(0) << " connect";
    connectCtrl = mixer_get_ctl_by_name(virtMixer, connectCtrlName.str().data());
    if (!connectCtrl) {
        PAL_ERR(LOG_TAG, "invalid mixer control: %s", connectCtrlName.str().data());
        goto free_fe;
    }
    ret = mixer_ctl_set_enum_by_string(connectCtrl, backEndNameTx.c_str());
    if (ret) {
        PAL_ERR(LOG_TAG, "Mixer control %s set with %s failed: %d",
        connectCtrlName.str().data(), backEndNameTx.c_str(), ret);
        goto free_fe;
    }

    isTxFeandBeConnected = true;

    config.rate = vi_device.samplerate;
    switch (vi_device.bit_width) {
        case 32 :
            config.format = PCM_FORMAT_S32_LE;
        break;
        case 24 :
            config.format = PCM_FORMAT_S24_LE;
        break;
        case 16:
            config.format = PCM_FORMAT_S16_LE;
        break;
        default:
            PAL_DBG(LOG_TAG, "Unsupported bit width. Set default as 16");
            config.format = PCM_FORMAT_S16_LE;
        break;
    }

    switch (vi_device.channels) {
        case 1 :
            config.channels = CHANNELS_1;
        break;
        case 2 :
            config.channels = CHANNELS_2;
        break;
        default:
            PAL_DBG(LOG_TAG, "Unsupported channel. Set default as 2");
            config.channels = CHANNELS_2;
        break;
    }
    config.period_size = DEFAULT_PERIOD_SIZE;
    config.period_count = DEFAULT_PERIOD_COUNT;
    config.start_threshold = 0;
    config.stop_threshold = INT_MAX;
    config.silence_threshold = 0;

    flags = PCM_IN;

    // Setting the mode of VI module
    modeConfg.th_operation_mode = CALIBRATION_MODE;

    ret = SessionAlsaUtils::getModuleInstanceId(virtMixer, pcmDevIdsTx.at(0),
                                                backEndNameTx.c_str(),
                                                MODULE_HAPTICS_VI, &miid);
    if (0 != ret) {
        PAL_ERR(LOG_TAG, "Failed to get tag info %x, status = %d", MODULE_HAPTICS_VI, ret);
        goto free_fe;
    }

    builder->payloadHapticsDevPConfig(&payload, &payloadSize, miid,
            PARAM_ID_HAPTICS_VI_OP_MODE_PARAM,(void *)&modeConfg);
    if (payloadSize) {
        ret = updateCustomPayload(payload, payloadSize);
        free(payload);
        if (0 != ret) {
            PAL_ERR(LOG_TAG,"updateCustomPayload Failed for VI_OP_MODE_CFG\n");
            // Fatal error as calibration mode will not be set
            goto free_fe;
        }
    }

    // Setting Channel Map configuration for VI module
    // TODO: Move this to ACDB
    HapticsviChannelMapConfg.num_ch = numberOfChannels *2;
    payloadSize = 0;

    builder->payloadHapticsDevPConfig(&payload, &payloadSize, miid,
              PARAM_ID_HAPTICS_VI_CHANNEL_MAP_CFG,(void *)&HapticsviChannelMapConfg);
    if (payloadSize) {
        ret = updateCustomPayload(payload, payloadSize);
        free(payload);
        if (0 != ret) {
            PAL_ERR(LOG_TAG," updateCustomPayload Failed for CHANNEL_MAP_CFG\n");
            // Not a fatal error
            ret = 0;
        }
    }

    // Setting Excursion mode
    HapticsviExModeConfg.ex_FTM_mode_enable_flag = 0; // Normal Mode
    payloadSize = 0;

    builder->payloadHapticsDevPConfig(&payload, &payloadSize, miid,
            PARAM_ID_HAPTICS_VI_EX_FTM_MODE_PARAM,(void *)&HapticsviExModeConfg);
    if (payloadSize) {
        ret = updateCustomPayload(payload, payloadSize);
        free(payload);
        if (0 != ret) {
            PAL_ERR(LOG_TAG," updateCustomPayload Failed for EX_VI_MODE_CFG\n");
            // Not a fatal error
            ret = 0;
        }
    }

    // Setting the values for VI module
    if (customPayloadSize) {
        ret = SessionAlsaUtils::setDeviceCustomPayload(rm, backEndNameTx,
                    customPayload, customPayloadSize);
        if (ret) {
            PAL_ERR(LOG_TAG, "Unable to set custom param for mode");
            goto free_fe;
        }
    }

    txPcm = pcm_open(rm->getVirtualSndCard(), pcmDevIdsTx.at(0), flags, &config);
    if (!txPcm) {
        PAL_ERR(LOG_TAG, "txPcm open failed");
        goto free_fe;
    }

    if (!pcm_is_ready(txPcm)) {
        PAL_ERR(LOG_TAG, "txPcm open not ready");
        goto err_pcm_open;
    }

    //Register for VI module callback
    PAL_DBG(LOG_TAG, "registering event for VI module");
    payload_size = sizeof(struct agm_event_reg_cfg);

    event_cfg.event_id = EVENT_ID_HAPTICS_VI_CALIBRATION;
    event_cfg.event_config_payload_size = 0;
    event_cfg.is_register = 1;

    ret = SessionAlsaUtils::registerMixerEvent(virtMixer, pcmDevIdsTx.at(0),
                      backEndNameTx.c_str(), MODULE_HAPTICS_VI, (void *)&event_cfg,
                      payload_size);
    if (ret) {
        PAL_ERR(LOG_TAG, "Unable to register event to DSP");
        // Fatal Error. Calibration Won't work
        goto err_pcm_open;
    }

    // Register to mixtureControlEvents and wait for the R0T0 values
    ret = rm->registerMixerEventCallback(pcmDevIdsTx, sessionCb, (uint64_t)this, true);
    if (ret != 0) {
        PAL_ERR(LOG_TAG, "Failed to register callback to rm");
        // Fatal Error. Calibration Won't work
        goto err_pcm_open;
    }

    enableDevice(audioRoute, mSndDeviceName_vi);

    PAL_DBG(LOG_TAG, "pcm start for TX path");
    if (pcm_start(txPcm) < 0) {
        PAL_ERR(LOG_TAG, "pcm start failed for TX path");
        ret = -ENOSYS;
        goto err_pcm_open;
    }
    isTxStarted = true;

    // Setup RX path
    deviceRx.id = PAL_DEVICE_OUT_HAPTICS_DEVICE;
    ret = rm->getSndDeviceName(deviceRx.id, mSndDeviceName_rx);
    if (0 != ret) {
        PAL_ERR(LOG_TAG, "Failed to obtain the rx snd device name");
        goto err_pcm_open;
    }

    rm->getChannelMap(&(deviceRx.config.ch_info.ch_map[0]), numberOfChannels);

    switch (numberOfChannels) {
        case 1 :
            deviceRx.config.ch_info.channels = CHANNELS_1;
        break;
        case 2 :
            deviceRx.config.ch_info.channels = CHANNELS_2;
        break;
        default:
            PAL_DBG(LOG_TAG, "Unsupported channel. Set default as 2");
            deviceRx.config.ch_info.channels = CHANNELS_2;
        break;
    }
    // TODO: Fetch these data from rm.xml
    deviceRx.config.sample_rate = SAMPLINGRATE_48K;
    deviceRx.config.bit_width = BITWIDTH_16;
    deviceRx.config.aud_fmt_id = PAL_AUDIO_FMT_PCM_S16_LE;

    rm->getBackendName(deviceRx.id, backEndNameRx);
    if (!strlen(backEndNameRx.c_str())) {
        PAL_ERR(LOG_TAG, "Failed to obtain tx backend name for %d", deviceRx.id);
        goto err_pcm_open;
    }

    keyVector.clear();
    calVector.clear();

    PayloadBuilder::getDeviceKV(deviceRx.id, keyVector);
    if (0 != ret) {
        PAL_ERR(LOG_TAG, "Failed to obtain device KV for %d", deviceRx.id);
        goto err_pcm_open;
    }

    // Enable the Haptics Gen module
    switch (numberOfChannels) {
        case 1 :
            // TODO: Fetch the configuration from RM.xml
            calVector.push_back(std::make_pair(HAPTICS_PRO_DEV_MAP, HAPTICS_RIGHT_MONO));
        break;
        case 2 :
            calVector.push_back(std::make_pair(HAPTICS_PRO_DEV_MAP, HAPTICS_LEFT_RIGHT));
        break;
        default :
            PAL_ERR(LOG_TAG, "Unsupported channels for HapticsDevice ");
            goto err_pcm_open;
    }

    SessionAlsaUtils::getAgmMetaData(keyVector, calVector,
                    (struct prop_data *)devicePropId, deviceMetaData);
    if (!deviceMetaData.size) {
        PAL_ERR(LOG_TAG, "device metadata is zero");
        ret = -ENOMEM;
        goto err_pcm_open;
    }

    connectCtrlNameBe<< backEndNameRx << " metadata";

    beMetaDataMixerCtrl = mixer_get_ctl_by_name(virtMixer, connectCtrlNameBe.str().data());
    if (!beMetaDataMixerCtrl) {
        PAL_ERR(LOG_TAG, "invalid mixer control: %s", backEndNameRx.c_str());
        ret = -EINVAL;
        goto err_pcm_open;
    }

    if (deviceMetaData.size) {
        ret = mixer_ctl_set_array(beMetaDataMixerCtrl, (void *)deviceMetaData.buf,
                                    deviceMetaData.size);
        free(deviceMetaData.buf);
        deviceMetaData.buf = nullptr;
    }
    else {
        PAL_ERR(LOG_TAG, "Device Metadata not set for RX path");
        ret = -EINVAL;
        goto err_pcm_open;
    }

    ret = SessionAlsaUtils::setDeviceMediaConfig(rm, backEndNameRx, &deviceRx);
    if (ret) {
        PAL_ERR(LOG_TAG, "setDeviceMediaConfig for HapticsDevice failed");
        goto err_pcm_open;
    }

    /* Retrieve Hostless PCM device id */
    sAttr.type = PAL_STREAM_HAPTICS;
    sAttr.direction = PAL_AUDIO_INPUT_OUTPUT;
    dir = RX_HOSTLESS;
    pcmDevIdsRx = rm->allocateFrontEndIds(sAttr, dir);
    if (pcmDevIdsRx.size() == 0) {
        PAL_ERR(LOG_TAG, "allocateFrontEndIds failed");
        ret = -ENOSYS;
        goto err_pcm_open;
    }

    connectCtrlNameRx << "PCM" << pcmDevIdsRx.at(0) << " connect";
    connectCtrl = mixer_get_ctl_by_name(virtMixer, connectCtrlNameRx.str().data());
    if (!connectCtrl) {
        PAL_ERR(LOG_TAG, "invalid mixer control: %s", connectCtrlNameRx.str().data());
        ret = -ENOSYS;
        goto err_pcm_open;
    }
    ret = mixer_ctl_set_enum_by_string(connectCtrl, backEndNameRx.c_str());
    if (ret) {
        PAL_ERR(LOG_TAG, "Mixer control %s set with %s failed: %d",
        connectCtrlNameRx.str().data(), backEndNameRx.c_str(), ret);
        goto err_pcm_open;
    }
    isRxFeandBeConnected = true;

    config.rate = SAMPLINGRATE_48K;
    config.format = PCM_FORMAT_S16_LE;
    if (numberOfChannels > 1)
        config.channels = CHANNELS_2;
    else
        config.channels = CHANNELS_1;
    config.period_size = DEFAULT_PERIOD_SIZE;
    config.period_count = DEFAULT_PERIOD_COUNT;
    config.start_threshold = 0;
    config.stop_threshold = INT_MAX;
    config.silence_threshold = 0;

    flags = PCM_OUT;

    // Set the operation mode for SP module
    HpRxModeConfg.operation_mode = CALIBRATION_MODE;
    ret = SessionAlsaUtils::getModuleInstanceId(virtMixer, pcmDevIdsRx.at(0),
                                                backEndNameRx.c_str(),
                                                MODULE_HAPTICS_GEN, &miid);
    if (0 != ret) {
        PAL_ERR(LOG_TAG, "Failed to get miid info %x, status = %d", MODULE_SP, ret);
        goto err_pcm_open;
    }

    payloadSize = 0;
    builder->payloadHapticsDevPConfig(&payload, &payloadSize, miid,
            PARAM_ID_HAPTICS_OP_MODE,(void *)&HpRxModeConfg);
    if (payloadSize) {
        if (customPayloadSize) {
            free(customPayload);
            customPayloadSize = 0;
        }

        ret = updateCustomPayload(payload, payloadSize);
        free(payload);
        if (0 != ret) {
            PAL_ERR(LOG_TAG," updateCustomPayload Failed\n");
            // Fatal error as SP module will not run in Calibration mode
            goto err_pcm_open;
        }
    }

    // Setting the values for SP module
    if (customPayloadSize) {
        ret = SessionAlsaUtils::setDeviceCustomPayload(rm, backEndNameRx,
                    customPayload, customPayloadSize);
        if (ret) {
            PAL_ERR(LOG_TAG, "Unable to set custom param for SP mode");
            free(customPayload);
            customPayloadSize = 0;
            goto err_pcm_open;
        }
    }

    rxPcm = pcm_open(rm->getVirtualSndCard(), pcmDevIdsRx.at(0), flags, &config);
    if (!rxPcm) {
        PAL_ERR(LOG_TAG, "pcm open failed for RX path");
        ret = -ENOSYS;
        goto err_pcm_open;
    }

    if (!pcm_is_ready(rxPcm)) {
        PAL_ERR(LOG_TAG, "pcm open not ready for RX path");
        ret = -ENOSYS;
        goto err_pcm_open;
    }


    enableDevice(audioRoute, mSndDeviceName_rx);
    PAL_DBG(LOG_TAG, "pcm start for RX path");
    if (pcm_start(rxPcm) < 0) {
        PAL_ERR(LOG_TAG, "pcm start failed for RX path");
        ret = -ENOSYS;
        goto err_pcm_open;
    }
    isRxStarted = true;

    hapticsDevCalState = HAPTICS_DEV_CALIB_IN_PROGRESS;

    PAL_DBG(LOG_TAG, "Waiting for the event from DSP or PAL");

    // TODO: Make this to wait in While loop
    cv.wait(calLock);

    // Store the R0T0 values
    if (mDspCallbackRcvd) {
        if (calibrationCallbackStatus == CALIBRATION_STATUS_SUCCESS) {
            PAL_DBG(LOG_TAG, "Calibration is done");
            fp = fopen(PAL_HAP_DEVP_TEMP_PATH, "wb");
            if (!fp) {
                PAL_ERR(LOG_TAG, "Unable to open file for write");
            } else {
                PAL_DBG(LOG_TAG, "Write the R0T0 value to file");
                for (i = 0; i < numberOfChannels; i++) {
                    fwrite(&callback_data->r0_cali_q24[i],
                                sizeof(callback_data->r0_cali_q24[i]), 1, fp);
                    fwrite(&devTempList[i], sizeof(int16_t), 1, fp);
                }
                hapticsDevCalState = HAPTICS_DEV_CALIBRATED;
                free(callback_data);
                fclose(fp);
            }
        }
        else if (calibrationCallbackStatus == CALIBRATION_STATUS_FAILURE) {
            PAL_DBG(LOG_TAG, "Calibration is not done");
            hapticsDevCalState = HAPTICS_DEV_NOT_CALIBRATED;
            // reset the timer for retry
            clock_gettime(CLOCK_BOOTTIME, &devLastTimeUsed);
        }
    }

err_pcm_open :
    if (txPcm) {
        event_cfg.is_register = 0;

        status = SessionAlsaUtils::registerMixerEvent(virtMixer, pcmDevIdsTx.at(0),
                        backEndNameTx.c_str(), MODULE_HAPTICS_VI, (void *)&event_cfg,
                        payload_size);
        if (status) {
            PAL_ERR(LOG_TAG, "Unable to deregister event to DSP");
        }
        status = rm->registerMixerEventCallback (pcmDevIdsTx, sessionCb, (uint64_t)this, false);
        if (status) {
            PAL_ERR(LOG_TAG, "Failed to deregister callback to rm");
        }
        if (isTxStarted)
            pcm_stop(txPcm);

        pcm_close(txPcm);
        disableDevice(audioRoute, mSndDeviceName_vi);

        txPcm = NULL;
    }

    if (rxPcm) {
        if (isRxStarted)
            pcm_stop(rxPcm);
        pcm_close(rxPcm);
        disableDevice(audioRoute, mSndDeviceName_rx);
        rxPcm = NULL;
    }

free_fe:
    if (pcmDevIdsRx.size() != 0) {
        if (isRxFeandBeConnected) {
            disconnectFeandBe(pcmDevIdsRx, backEndNameRx);
        }
        rm->freeFrontEndIds(pcmDevIdsRx, sAttr, RX_HOSTLESS);
    }

    if (pcmDevIdsTx.size() != 0) {
        if (isTxFeandBeConnected) {
            disconnectFeandBe(pcmDevIdsTx, backEndNameTx);
        }
        rm->freeFrontEndIds(pcmDevIdsTx, sAttr, TX_HOSTLESS);
    }
    pcmDevIdsRx.clear();
    pcmDevIdsTx.clear();

exit:

    if (!mDspCallbackRcvd) {
        // the lock is unlocked due to processing mode. It will be waiting
        // for the unlock. So notify it.
        PAL_DBG(LOG_TAG, "Unlocked due to processing mode");
        hapticsDevCalState = HAPTICS_DEV_NOT_CALIBRATED;
        clock_gettime(CLOCK_BOOTTIME, &devLastTimeUsed);
        cv.notify_all();
    }

    if (ret != 0) {
        // Error happened. Reset timer
        clock_gettime(CLOCK_BOOTTIME, &devLastTimeUsed);
    }

    if(builder) {
       delete builder;
       builder = NULL;
    }
    PAL_DBG(LOG_TAG, "Exiting");
    return ret;
}

/**
  * This function sets the temperature of each HapticsDevics.
  * Currently values are supported like:
  * devTempList[0] - Right  HapticsDevice Temperature
  * devTempList[1] - Left  HapticsDevice Temperature
  */
void HapticsDevProtection::getHapticsDevTemperatureList()
{
    int i = 0;
    int value;
    PAL_DBG(LOG_TAG, "Enter  HapticsDevice Get Temperature List");

    for(i = 0; i < numberOfChannels; i++) {
         value = getDevTemperature(i);
         PAL_DBG(LOG_TAG, "Temperature %d ", value);
         devTempList[i] = value;
    }
    PAL_DBG(LOG_TAG, "Exit  HapticsDevice Get Temperature List");
}

void HapticsDevProtection::HapticsDevCalibrationThread()
{
    unsigned long sec = 0;
    bool proceed = false;
    int i;

    while (!threadExit) {
        PAL_DBG(LOG_TAG, "Inside calibration while loop");
        proceed = false;
        if (isHapticsDevInUse(&sec)) {
            PAL_DBG(LOG_TAG, "HapticsDev in use. Wait for proper time");
            HapticsDevCalibrateWait();
            PAL_DBG(LOG_TAG, "Waiting done");
            continue;
        }
        else {
            PAL_DBG(LOG_TAG, "HapticsDev not in use");
            if (isDynamicCalTriggered) {
                PAL_DBG(LOG_TAG, "Dynamic Calibration triggered");
            }
            else if (sec < minIdleTime) {
                PAL_DBG(LOG_TAG, "HapticsDev not idle for minimum time. %lu", sec);
                HapticsDevCalibrateWait();
                PAL_DBG(LOG_TAG, "Waited for HapticsDev to be idle for min time");
                continue;
            }
            proceed = true;
        }

        if (proceed) {
            PAL_DBG(LOG_TAG, "Getting temperature of HapticsDev");
            getHapticsDevTemperatureList();

            for (i = 0; i < numberOfChannels; i++) {
                if ((devTempList[i] != -EINVAL) &&
                    (devTempList[i] < TZ_TEMP_MIN_THRESHOLD ||
                     devTempList[i] > TZ_TEMP_MAX_THRESHOLD)) {
                    PAL_ERR(LOG_TAG, "Temperature out of range. Retry");
                    HapticsDevCalibrateWait();
                    continue;
                }
            }
            for (i = 0; i < numberOfChannels; i++) {
                // Converting to Q6 format
                devTempList[i] = (devTempList[i]*(1<<6));
            }
        }
        else {
            continue;
        }

        // Check whether HapticsDevice  was in use in the meantime when temperature
        // was being read.
        proceed = false;
        if (isHapticsDevInUse(&sec)) {
            PAL_DBG(LOG_TAG, " HapticsDevice in use. Wait for proper time");
            HapticsDevCalibrateWait();
            PAL_DBG(LOG_TAG, "Waiting done");
            continue;
        }
        else {
            PAL_DBG(LOG_TAG, " HapticsDevice not in use");
            if (isDynamicCalTriggered) {
                PAL_DBG(LOG_TAG, "Dynamic calibration triggered");
            }
            else if (sec < minIdleTime) {
                PAL_DBG(LOG_TAG, " HapticsDevice not idle for minimum time. %lu", sec);
                HapticsDevCalibrateWait();
                PAL_DBG(LOG_TAG, "Waited for HapticsDevice to be idle for min time");
                continue;
            }
            proceed = true;
        }

        if (proceed) {
            // Start calibrating the HapticsDevice.
            PAL_DBG(LOG_TAG, " HapticsDevice not in use, start calibration");
            HapticsDevStartCalibration();
            if (hapticsDevCalState == HAPTICS_DEV_CALIBRATED) {
                threadExit = true;
            }
        }
        else {
            continue;
        }
    }
    isDynamicCalTriggered = false;
    calThrdCreated = false;
    PAL_DBG(LOG_TAG, "Calibration done, exiting the thread");
}

HapticsDevProtection::HapticsDevProtection(struct pal_device *device,
                        std::shared_ptr<ResourceManager> Rm):HapticsDev(device, Rm)
{
    int status = 0;
    struct pal_device_info devinfo = {};
    FILE *fp;

    minIdleTime = MIN_HAPTICS_DEV_IDLE_SEC;

    rm = Rm;

    memset(&mDeviceAttr, 0, sizeof(struct pal_device));
    memcpy(&mDeviceAttr, device, sizeof(struct pal_device));

    threadExit = false;
    calThrdCreated = false;

    triggerCal = false;
    hapticsDevCalState = HAPTICS_DEV_NOT_CALIBRATED;
    hapticsDevProcessingState = HAPTICS_DEV_PROCESSING_IN_IDLE;

    isHapDevInUse = false;

    rm->getDeviceInfo(PAL_DEVICE_OUT_HAPTICS_DEVICE, PAL_STREAM_PROXY, "", &devinfo);
    numberOfChannels = devinfo.channels;
    PAL_DBG(LOG_TAG, "Number of Channels %d", numberOfChannels);

    rm->getDeviceInfo(PAL_DEVICE_IN_HAPTICS_VI_FEEDBACK, PAL_STREAM_PROXY, "", &vi_device);
    PAL_DBG(LOG_TAG, "Number of Channels for VI path is %d", vi_device.channels);

    devTempList = new int [numberOfChannels];
    // Get current time
    clock_gettime(CLOCK_BOOTTIME, &devLastTimeUsed);

    // Getting mixer controls from Resource Manager
    status = rm->getVirtualAudioMixer(&virtMixer);
    if (status) {
        PAL_ERR(LOG_TAG,"virt mixer error %d", status);
    }
    status = rm->getHwAudioMixer(&hwMixer);
    if (status) {
        PAL_ERR(LOG_TAG,"hw mixer error %d", status);
    }

    calibrationCallbackStatus = 0;
    mDspCallbackRcvd = false;

/*    fp = fopen(PAL_HAP_DEVP_TEMP_PATH, "rb");
    if (fp) {
        PAL_DBG(LOG_TAG, "Cal File exists. Reading from it");
        hapticsDevCalState = HAPTICS_DEV_CALIBRATED;
    }
    else {
        PAL_DBG(LOG_TAG, "Calibration Not done");
        mCalThread = std::thread(&HapticsDevProtection::HapticsDevCalibrationThread,
                            this);
        calThrdCreated = true;
    }*/
}

HapticsDevProtection::~HapticsDevProtection()
{
    if (devTempList)
        delete[] devTempList;
}

/*
 * Function to trigger Processing mode.
 * The parameter that it accepts are below:
 * true - Start Processing Mode
 * false - Stop Processing Mode
 */
int32_t HapticsDevProtection::HapticsDevProtProcessingMode(bool flag)
{
    int ret = 0, dir = TX_HOSTLESS, flags, viParamId = 0;
    char mSndDeviceName_vi[128] = {0};
    uint8_t* payload = NULL;
    uint32_t devicePropId[] = {0x08000010, 1, 0x2};
    uint32_t miid = 0;
    bool isTxFeandBeConnected = true;
    size_t payloadSize = 0;
    struct pal_device device;
    struct pal_channel_info ch_info;
    struct pal_stream_attributes sAttr;
    struct pcm_config config;
    struct mixer_ctl *connectCtrl = NULL;
    struct audio_route *audioRoute = NULL;
    struct agmMetaData deviceMetaData(nullptr, 0);
    struct mixer_ctl *beMetaDataMixerCtrl = nullptr;
    FILE *fp;
    std::string backEndName, backEndNameRx;
    std::vector <std::pair<int, int>> keyVector;
    std::vector <std::pair<int, int>> calVector;
    std::shared_ptr<ResourceManager> rm;
    std::ostringstream connectCtrlNameBeVI;
    std::ostringstream connectCtrlNameBeHP;
    std::ostringstream connectCtrlName;
    param_id_haptics_th_vi_r0t0_set_param_t *hpR0T0confg;
    param_id_haptics_ex_vi_persistent *VIpeValue;
    param_id_haptics_vi_op_mode_param_t modeConfg;
    param_id_haptics_vi_channel_map_cfg_t HapticsviChannelMapConfg;
    param_id_haptics_op_mode hpModeConfg;
    //param_id_haptics_vi_ex_FTM_mode_param_t HapticsviExModeConfg;
    std::shared_ptr<Device> dev = nullptr;
    Stream *stream = NULL;
    Session *session = NULL;
    std::vector<Stream*> activeStreams;
    PayloadBuilder* builder = new PayloadBuilder();
    std::unique_lock<std::mutex> lock(calibrationMutex);

    PAL_DBG(LOG_TAG, "Flag %d", flag);
    deviceMutex.lock();


    if (flag) {
        if (hapticsDevCalState == HAPTICS_DEV_CALIB_IN_PROGRESS) {
            // Close the Graphs
            cv.notify_all();
            // Wait for cleanup
            cv.wait(lock);
            hapticsDevCalState = HAPTICS_DEV_NOT_CALIBRATED;
            txPcm = NULL;
            rxPcm = NULL;
            PAL_DBG(LOG_TAG, "Stopped calibration mode");
        }
        numberOfRequest++;
        if (numberOfRequest > 1) {
            // R0T0 already set, we don't need to process the request.
            goto exit;
        }
        PAL_DBG(LOG_TAG, "Custom payload size %zu, Payload %p", customPayloadSize,
                customPayload);

        if (customPayload) {
            free(customPayload);
        }
        customPayloadSize = 0;
        customPayload = NULL;

        HapticsDevProtSetDevStatus(flag);
        //  HapticsDevice in use. Start the Processing Mode
        rm = ResourceManager::getInstance();
        if (!rm) {
            PAL_ERR(LOG_TAG, "Failed to get resource manager instance");
            goto exit;
        }

        memset(&device, 0, sizeof(device));
        memset(&sAttr, 0, sizeof(sAttr));
        memset(&config, 0, sizeof(config));
        memset(&modeConfg, 0, sizeof(modeConfg));
        memset(&HapticsviChannelMapConfg, 0, sizeof(HapticsviChannelMapConfg));
        //memset(&HapticsviExModeConfg, 0, sizeof(HapticsviExModeConfg));
        memset(&hpModeConfg, 0, sizeof(hpModeConfg));

        keyVector.clear();
        calVector.clear();

        if (customPayload) {
            free(customPayload);
            customPayloadSize = 0;
            customPayload = NULL;
        }

        // Configure device attribute
       if (vi_device.channels > 1) {
            ch_info.channels = CHANNELS_2;
            ch_info.ch_map[0] = PAL_CHMAP_CHANNEL_FL;
            ch_info.ch_map[1] = PAL_CHMAP_CHANNEL_FR;
        }
        else {
            ch_info.channels = CHANNELS_1;
            ch_info.ch_map[0] = PAL_CHMAP_CHANNEL_FL;
        }


        device.config.ch_info = ch_info;
        device.config.sample_rate = vi_device.samplerate;
        device.config.bit_width = vi_device.bit_width;
        device.config.aud_fmt_id = rm->getAudioFmt(vi_device.bit_width);

        // Setup TX path
        device.id = PAL_DEVICE_IN_HAPTICS_VI_FEEDBACK;

        ret = rm->getAudioRoute(&audioRoute);
        if (0 != ret) {
            PAL_ERR(LOG_TAG, "Failed to get the audio_route address status %d", ret);
            goto exit;
        }

        ret = rm->getSndDeviceName(device.id , mSndDeviceName_vi);
        if (0 != ret) {
            PAL_ERR(LOG_TAG, "Failed to obtain tx snd device name for %d", device.id);
            goto exit;
        }

        PAL_DBG(LOG_TAG, "get the audio route %s", mSndDeviceName_vi);

        rm->getBackendName(device.id, backEndName);
        if (!strlen(backEndName.c_str())) {
            PAL_ERR(LOG_TAG, "Failed to obtain tx backend name for %d", device.id);
            goto exit;
        }

        PayloadBuilder::getDeviceKV(device.id, keyVector);
        if (0 != ret) {
            PAL_ERR(LOG_TAG, "Failed to obtain device KV for %d", device.id);
            goto exit;
        }

        // Enable the VI module
        switch (numberOfChannels) {
            case 1 :
                calVector.push_back(std::make_pair(HAPTICS_PRO_VI_MAP, HAPTICS_VI_LEFT_MONO));
            break;
            case 2 :
                calVector.push_back(std::make_pair(HAPTICS_PRO_VI_MAP, HAPTICS_VI_LEFT_RIGHT));
            break;
            default :
                PAL_ERR(LOG_TAG, "Unsupported channel");
                goto exit;
        }

        SessionAlsaUtils::getAgmMetaData(keyVector, calVector,
                (struct prop_data *)devicePropId, deviceMetaData);
        if (!deviceMetaData.size) {
            PAL_ERR(LOG_TAG, "VI device metadata is zero");
            ret = -ENOMEM;
            goto exit;
        }
        connectCtrlNameBeVI<< backEndName << " metadata";
        beMetaDataMixerCtrl = mixer_get_ctl_by_name(virtMixer,
                                    connectCtrlNameBeVI.str().data());
        if (!beMetaDataMixerCtrl) {
            PAL_ERR(LOG_TAG, "invalid mixer control for VI : %s", backEndName.c_str());
            ret = -EINVAL;
            goto exit;
        }

        if (deviceMetaData.size) {
            ret = mixer_ctl_set_array(beMetaDataMixerCtrl, (void *)deviceMetaData.buf,
                        deviceMetaData.size);
            free(deviceMetaData.buf);
            deviceMetaData.buf = nullptr;
        }
        else {
            PAL_ERR(LOG_TAG, "Device Metadata not set for TX path");
            ret = -EINVAL;
            goto exit;
        }

        ret = SessionAlsaUtils::setDeviceMediaConfig(rm, backEndName, &device);
        if (ret) {
            PAL_ERR(LOG_TAG, "setDeviceMediaConfig for feedback device failed");
            goto exit;
        }

        /* Retrieve Hostless PCM device id */
        sAttr.type = PAL_STREAM_HAPTICS;
        sAttr.direction = PAL_AUDIO_INPUT_OUTPUT;
        dir = TX_HOSTLESS;
        pcmDevIdTx = rm->allocateFrontEndIds(sAttr, dir);
        if (pcmDevIdTx.size() == 0) {
            PAL_ERR(LOG_TAG, "allocateFrontEndIds failed");
            ret = -ENOSYS;
            goto exit;
        }

        connectCtrlName << "PCM" << pcmDevIdTx.at(0) << " connect";
        connectCtrl = mixer_get_ctl_by_name(virtMixer, connectCtrlName.str().data());
        if (!connectCtrl) {
            PAL_ERR(LOG_TAG, "invalid mixer control: %s", connectCtrlName.str().data());
            goto free_fe;
        }

        ret = mixer_ctl_set_enum_by_string(connectCtrl, backEndName.c_str());
        if (ret) {
            PAL_ERR(LOG_TAG, "Mixer control %s set with %s failed: %d",
            connectCtrlName.str().data(), backEndName.c_str(), ret);
            goto free_fe;
        }

        isTxFeandBeConnected = true;

        config.rate = vi_device.samplerate;
        switch (vi_device.bit_width) {
            case 32 :
                config.format = PCM_FORMAT_S32_LE;
            break;
            case 24 :
                config.format = PCM_FORMAT_S24_LE;
            break;
            case 16 :
                config.format = PCM_FORMAT_S16_LE;
            break;
            default:
                PAL_DBG(LOG_TAG, "Unsupported bit width. Set default as 16");
                config.format = PCM_FORMAT_S16_LE;
            break;
        }

        switch (vi_device.channels) {
            case 1 :
                config.channels = CHANNELS_1;
            break;
            case 2 :
                config.channels = CHANNELS_2;
            break;
            default :
                PAL_DBG(LOG_TAG, "Unsupported channel. Set default as 2");
                config.channels = CHANNELS_2;
            break;
        }
        config.period_size = DEFAULT_PERIOD_SIZE;
        config.period_count = DEFAULT_PERIOD_COUNT;
        config.start_threshold = 0;
        config.stop_threshold = INT_MAX;
        config.silence_threshold = 0;

        flags = PCM_IN;

        // Setting the mode of VI module
        modeConfg.th_operation_mode = NORMAL_MODE;

        ret = SessionAlsaUtils::getModuleInstanceId(virtMixer, pcmDevIdTx.at(0),
                        backEndName.c_str(), MODULE_HAPTICS_VI, &miid);
        if (0 != ret) {
            PAL_ERR(LOG_TAG, "Failed to get tag info %x, status = %d", MODULE_HAPTICS_VI, ret);
            goto free_fe;
        }

        builder->payloadHapticsDevPConfig(&payload, &payloadSize, miid,
                                 PARAM_ID_HAPTICS_VI_OP_MODE_PARAM,(void *)&modeConfg);
        if (payloadSize) {
            ret = updateCustomPayload(payload, payloadSize);
            free(payload);
            if (0 != ret) {
                PAL_ERR(LOG_TAG," updateCustomPayload Failed for VI_OP_MODE_CFG\n");
                // Not fatal as by default VI module runs in Normal mode
                ret = 0;
            }
        }

        // Setting Channel Map configuration for VI module
        // TODO: Move this to ACDB file
        HapticsviChannelMapConfg.num_ch = numberOfChannels * 2;
        payloadSize = 0;

        builder->payloadHapticsDevPConfig(&payload, &payloadSize, miid,
           PARAM_ID_HAPTICS_VI_CHANNEL_MAP_CFG,(void *)&HapticsviChannelMapConfg);
        if (payloadSize) {
            ret = updateCustomPayload(payload, payloadSize);
            free(payload);
            if (0 != ret) {
                PAL_ERR(LOG_TAG," updateCustomPayload Failed for CHANNEL_MAP_CFG\n");
            }
        }

        //Set persistent params.
        getAndsetPersistentParameter(false);

        // Setting the values for VI module
        if (customPayloadSize) {
            ret = SessionAlsaUtils::setDeviceCustomPayload(rm, backEndName,
                            customPayload, customPayloadSize);
            if (ret) {
                PAL_ERR(LOG_TAG, "Unable to set custom param for mode");
                goto free_fe;
            }
        }

        txPcm = pcm_open(rm->getVirtualSndCard(), pcmDevIdTx.at(0), flags, &config);
        if (!txPcm) {
            PAL_ERR(LOG_TAG, "txPcm open failed");
            goto free_fe;
        }

        if (!pcm_is_ready(txPcm)) {
            PAL_ERR(LOG_TAG, "txPcm open not ready");
            goto err_pcm_open;
        }

        rm->getBackendName(mDeviceAttr.id, backEndNameRx);
        if (!strlen(backEndNameRx.c_str())) {
            PAL_ERR(LOG_TAG, "Failed to obtain rx backend name for %d", mDeviceAttr.id);
            goto err_pcm_open;
        }

        dev = Device::getInstance(&mDeviceAttr, rm);

        ret = rm->getActiveStream_l(activeStreams, dev);
        if ((0 != ret) || (activeStreams.size() == 0)) {
            PAL_ERR(LOG_TAG, " no active stream available");
            ret = -EINVAL;
            goto err_pcm_open;
        }

        stream = static_cast<Stream *>(activeStreams[0]);
        stream->getAssociatedSession(&session);

        ret = session->getMIID(backEndNameRx.c_str(), MODULE_HAPTICS_GEN, &miid);
        if (ret) {
            PAL_ERR(LOG_TAG, "Failed to get tag info %x, status = %d", MODULE_HAPTICS_GEN, ret);
            goto err_pcm_open;
        }

        // Set the operation mode for Haptics module

        PAL_INFO(LOG_TAG, "Normal mode being used");
        hpModeConfg.operation_mode = NORMAL_MODE;

        payloadSize = 0;
        builder->payloadHapticsDevPConfig(&payload, &payloadSize, miid,
                PARAM_ID_HAPTICS_OP_MODE,(void *)&hpModeConfg);
        if (payloadSize) {
            if (customPayload) {
                free (customPayload);
                customPayloadSize = 0;
                customPayload = NULL;
            }
            ret = updateCustomPayload(payload, payloadSize);
            free(payload);
            if (0 != ret) {
                PAL_ERR(LOG_TAG," updateCustomPayload Failed\n");
            }
        }

        enableDevice(audioRoute, mSndDeviceName_vi);
        PAL_DBG(LOG_TAG, "pcm start for TX");
        if (pcm_start(txPcm) < 0) {
            PAL_ERR(LOG_TAG, "pcm start failed for TX path");
            goto err_pcm_open;
        }

        // Free up the local variables
        goto exit;
    }
    else {
        numberOfRequest--;
        if (numberOfRequest > 0) {
            // R0T0 already set, we don't need to process the request.
            goto exit;
        }
        HapticsDevProtSetDevStatus(flag);
        //  HapticsDevice not in use anymore. Stop the processing mode
        PAL_DBG(LOG_TAG, "Closing VI path");
        if (txPcm) {
            rm = ResourceManager::getInstance();
            device.id = PAL_DEVICE_IN_HAPTICS_VI_FEEDBACK;

            ret = rm->getAudioRoute(&audioRoute);
            if (0 != ret) {
                PAL_ERR(LOG_TAG, "Failed to get the audio_route address status %d", ret);
                goto exit;
            }

            ret = rm->getSndDeviceName(device.id , mSndDeviceName_vi);
            rm->getBackendName(device.id, backEndName);
            if (!strlen(backEndName.c_str())) {
                PAL_ERR(LOG_TAG, "Failed to obtain tx backend name for %d", device.id);
                goto exit;
            }
            //Get persistent param for future use.
            getAndsetPersistentParameter(true);

            pcm_stop(txPcm);
            pcm_close(txPcm);
            disableDevice(audioRoute, mSndDeviceName_vi);
            txPcm = NULL;
            sAttr.type = PAL_STREAM_HAPTICS;
            sAttr.direction = PAL_AUDIO_INPUT_OUTPUT;
            goto free_fe;
        }
    }

err_pcm_open :
    if (txPcm) {
        pcm_close(txPcm);
        disableDevice(audioRoute, mSndDeviceName_vi);
        txPcm = NULL;
    }

free_fe:
    if (pcmDevIdTx.size() != 0) {
        if (isTxFeandBeConnected) {
            disconnectFeandBe(pcmDevIdTx, backEndName);
        }
        rm->freeFrontEndIds(pcmDevIdTx, sAttr, dir);
        pcmDevIdTx.clear();
    }
exit:
    deviceMutex.unlock();
    if(builder) {
       delete builder;
       builder = NULL;
    }
    return ret;
}

void HapticsDevProtection::updateHPcustomPayload()
{
    PayloadBuilder* builder = new PayloadBuilder();
    uint8_t* payload = NULL;
    size_t payloadSize = 0;
    std::string backEndName;
    std::shared_ptr<Device> dev = nullptr;
    Stream *stream = NULL;
    Session *session = NULL;
    std::vector<Stream*> activeStreams;
    uint32_t miid = 0, ret;
    param_id_haptics_op_mode hpModeConfg;

    rm->getBackendName(mDeviceAttr.id, backEndName);
    dev = Device::getInstance(&mDeviceAttr, rm);
    ret = rm->getActiveStream_l(activeStreams, dev);
    if ((0 != ret) || (activeStreams.size() == 0)) {
        PAL_ERR(LOG_TAG, " no active stream available");
        goto exit;
    }
    stream = static_cast<Stream *>(activeStreams[0]);
    stream->getAssociatedSession(&session);
    ret = session->getMIID(backEndName.c_str(), MODULE_HAPTICS_GEN, &miid);
    if (ret) {
        PAL_ERR(LOG_TAG, "Failed to get tag info %x, status = %d", MODULE_HAPTICS_GEN, ret);
        goto exit;
    }

    if (customPayloadSize) {
        free(customPayload);
        customPayloadSize = 0;
    }

    hpModeConfg.operation_mode = NORMAL_MODE;
    payloadSize = 0;
    builder->payloadHapticsDevPConfig(&payload, &payloadSize, miid,
                    PARAM_ID_HAPTICS_OP_MODE,(void *)&hpModeConfg);
    if (payloadSize) {
        ret = updateCustomPayload(payload, payloadSize);
        free(payload);
        if (0 != ret) {
            PAL_ERR(LOG_TAG," updateCustomPayload Failed\n");
        }
    }

exit:
    if(builder) {
       delete builder;
       builder = NULL;
    }
    return;
}


int HapticsDevProtection::HapticsDevProtectionDynamicCal()
{
    int ret = 0;

    PAL_DBG(LOG_TAG, "Enter");

    if (calThrdCreated) {
        PAL_DBG(LOG_TAG, "Calibration already triggered Thread State %d",
                        calThrdCreated);
        return ret;
    }

    threadExit = false;
    hapticsDevCalState = HAPTICS_DEV_NOT_CALIBRATED;

    calibrationCallbackStatus = 0;
    mDspCallbackRcvd = false;

    calThrdCreated = true;
    isDynamicCalTriggered = true;

    std::thread dynamicCalThread(&HapticsDevProtection::HapticsDevCalibrationThread, this);

    dynamicCalThread.detach();

    PAL_DBG(LOG_TAG, "Exit");

    return ret;
}

int HapticsDevProtection::start()
{
    PAL_DBG(LOG_TAG, "Enter");

    if (ResourceManager::isVIRecordStarted) {
        PAL_DBG(LOG_TAG, "record running so just update SP payload");
        updateHPcustomPayload();
    }
    else {
        HapticsDevProtProcessingMode(true);
    }

    PAL_DBG(LOG_TAG, "Calling Device start");
    Device::start();
    return 0;
}

int HapticsDevProtection::stop()
{
    PAL_DBG(LOG_TAG, "Inside  HapticsDevice Protection stop");
    Device::stop();
    if (ResourceManager::isVIRecordStarted) {
        PAL_DBG(LOG_TAG, "record running so no need to proceed");
        ResourceManager::isVIRecordStarted = false;
        return 0;
    }
    HapticsDevProtProcessingMode(false);
    return 0;
}


int32_t HapticsDevProtection::setParameter(uint32_t param_id, void *param)
{
    PAL_DBG(LOG_TAG, "Inside  HapticsDevice Protection Set parameters");
    (void ) param;
    if (param_id == PAL_SP_MODE_DYNAMIC_CAL)
        HapticsDevProtectionDynamicCal();
    return 0;
}

int32_t HapticsDevProtection::getAndsetPersistentParameter(bool flag)
{
    int size = 0, status = 0, ret = 0;
    uint32_t miid = 0;
    const char *getParamControl = "getParam";
    char *pcmDeviceName = NULL;
    uint8_t* payload = NULL;
    size_t payloadSize = 0;
    struct mixer_ctl *ctl;
    FILE *fp;
    std::ostringstream cntrlName;
    std::ostringstream resString;
    std::string backendName;
    param_id_haptics_ex_vi_persistent ViPe;
    PayloadBuilder* builder = new PayloadBuilder();
    param_id_haptics_ex_vi_persistent *VIpeValue;

    pcmDeviceName = rm->getDeviceNameFromID(pcmDevIdTx.at(0));
    if (pcmDeviceName) {
        cntrlName<<pcmDeviceName<<" "<<getParamControl;
    }
    else {
        PAL_ERR(LOG_TAG, "Error: %d Unable to get Device name\n", -EINVAL);
        goto exit;
    }

    ctl = mixer_get_ctl_by_name(virtMixer, cntrlName.str().data());
    if (!ctl) {
        status = -ENOENT;
        PAL_ERR(LOG_TAG, "Error: %d Invalid mixer control: %s\n", status,cntrlName.str().data());
        goto exit;
    }
    rm->getBackendName(PAL_DEVICE_IN_HAPTICS_VI_FEEDBACK, backendName);
    if (!strlen(backendName.c_str())) {
        status = -ENOENT;
        PAL_ERR(LOG_TAG, "Error: %d Failed to obtain VI backend name", status);
        goto exit;
    }

    status = SessionAlsaUtils::getModuleInstanceId(virtMixer, pcmDevIdTx.at(0),
                        backendName.c_str(), MODULE_HAPTICS_VI, &miid);
    if (0 != status) {
        PAL_ERR(LOG_TAG, "Error: %d Failed to get tag info %x", status, MODULE_HAPTICS_VI);
        goto exit;
    }
    if (flag) {
        builder->payloadHapticsDevPConfig (&payload, &payloadSize, miid,
                              PARAM_ID_HAPTICS_EX_VI_PERSISTENT, &ViPe);

        status = mixer_ctl_set_array(ctl, payload, payloadSize);
        if (0 != status) {
            PAL_ERR(LOG_TAG, "Set failed status = %d", status);
            goto exit;
        }

        memset(payload, 0, payloadSize);

        status = mixer_ctl_get_array(ctl, payload, payloadSize);
        if (0 != status) {
            PAL_ERR(LOG_TAG, "Get failed status = %d", status);
        }
        else {
            VIpeValue = (param_id_haptics_ex_vi_persistent *) (payload +
                            sizeof(struct apm_module_param_data_t));
            fp = fopen(PAL_HP_VI_PER_PATH, "wb");
            if (!fp) {
                PAL_ERR(LOG_TAG, "Unable to open file for write");
            } else {
                PAL_DBG(LOG_TAG, "Write the Vi persistant value to file");
                for (int i = 0; i < numberOfChannels; i++) {
                    PAL_ERR(LOG_TAG, "persistent values VIpeValue->Re_ohm_q24[i] =%d",VIpeValue->Re_ohm_q24[i]);
                    fwrite(&VIpeValue->Re_ohm_q24[i], sizeof(VIpeValue->Re_ohm_q24[i]),
                                             1, fp);
                    PAL_ERR(LOG_TAG, "persistent values VIpeValue->Le_mH_q24[i] =%d",VIpeValue->Le_mH_q24[i]);
                    fwrite(&VIpeValue->Le_mH_q24[i], sizeof(VIpeValue->Le_mH_q24[i]),
                                             1, fp);
                    PAL_ERR(LOG_TAG, "persistent values VIpeValue->Bl_q24[i] =%d",VIpeValue->Bl_q24[i]);
                    fwrite(&VIpeValue->Bl_q24[i], sizeof(VIpeValue->Bl_q24[i]),
                                             1, fp);
                    PAL_ERR(LOG_TAG, "persistent values VIpeValue->Rms_KgSec_q24[i] =%d",VIpeValue->Rms_KgSec_q24[i]);
                    fwrite(&VIpeValue->Rms_KgSec_q24[i], sizeof(VIpeValue->Rms_KgSec_q24[i]),
                                             1, fp);
                    PAL_ERR(LOG_TAG, "persistent values VIpeValue->Kms_Nmm_q24[i] =%d",VIpeValue->Kms_Nmm_q24[i]);
                    fwrite(&VIpeValue->Kms_Nmm_q24[i], sizeof(VIpeValue->Kms_Nmm_q24[i]),
                                             1, fp);
                    PAL_ERR(LOG_TAG, "persistent values VIpeValue->Fres_Hz_q20[i] =%d",VIpeValue->Fres_Hz_q20[i]);
                    fwrite(&VIpeValue->Fres_Hz_q20[i], sizeof(VIpeValue->Fres_Hz_q20[i]),
                                             1, fp);
                }
                fclose(fp);
            }
        }
    }
    else {
        fp = fopen(PAL_HP_VI_PER_PATH, "rb");
        if (fp) {
            VIpeValue = (param_id_haptics_ex_vi_persistent *)calloc(1,
                               sizeof(param_id_haptics_ex_vi_persistent));
            if (!VIpeValue) {
                PAL_ERR(LOG_TAG," payload creation Failed\n");
                return 0;
            }
            PAL_DBG(LOG_TAG, "update Vi persistant value from file");
            for (int i = 0; i < numberOfChannels; i++) {
                    fread(&VIpeValue->Re_ohm_q24[i], sizeof(VIpeValue->Re_ohm_q24[i]),
                                             1, fp);
                     PAL_ERR(LOG_TAG, "persistent values VIpeValue->Re_ohm_q24[i] =%d",VIpeValue->Re_ohm_q24[i]);
                    fread(&VIpeValue->Le_mH_q24[i], sizeof(VIpeValue->Le_mH_q24[i]),
                                             1, fp);
                     PAL_ERR(LOG_TAG, "persistent values VIpeValue->Le_mH_q24[i] =%d",VIpeValue->Le_mH_q24[i]);
                    fread(&VIpeValue->Bl_q24[i], sizeof(VIpeValue->Bl_q24[i]),
                                             1, fp);
                     PAL_ERR(LOG_TAG, "persistent values VIpeValue->Bl_q24[i] =%d",VIpeValue->Bl_q24[i]);
                    fread(&VIpeValue->Rms_KgSec_q24[i], sizeof(VIpeValue->Rms_KgSec_q24[i]),
                                             1, fp);
                    PAL_ERR(LOG_TAG, "persistent values VIpeValue->Rms_KgSec_q24[i] =%d",VIpeValue->Rms_KgSec_q24[i]);
                    fread(&VIpeValue->Kms_Nmm_q24[i], sizeof(VIpeValue->Kms_Nmm_q24[i]),
                                             1, fp);
                     PAL_ERR(LOG_TAG, "persistent values VIpeValue->Kms_Nmm_q24[i] =%d",VIpeValue->Kms_Nmm_q24[i]);
                    fread(&VIpeValue->Fres_Hz_q20[i], sizeof(VIpeValue->Fres_Hz_q20[i]),
                                             1, fp);
                     PAL_ERR(LOG_TAG, "persistent values VIpeValue->Fres_Hz_q20[i] =%d",VIpeValue->Fres_Hz_q20[i]);
            }
            fclose(fp);
//            return 0;
        }
        else {
            PAL_DBG(LOG_TAG, "Use the default Persistent values");
            return 0;
        }
        payloadSize = 0;
        builder->payloadHapticsDevPConfig(&payload, &payloadSize, miid,
                   PARAM_ID_HAPTICS_EX_VI_PERSISTENT,(void *)VIpeValue);
        if (payloadSize) {
            ret = updateCustomPayload(payload, payloadSize);
            free(payload);
            free(VIpeValue);
            if (0 != ret) {
                PAL_ERR(LOG_TAG," updateCustomPayload Failed\n");
                ret = 0;
            }
        }
    }
exit :
    if(builder) {
       delete builder;
       builder = NULL;
    }
    return ret;
}

/*
 * VI feedack related functionalities
 */

void HapticsDevFeedback::updateVIcustomPayload()
{
    PayloadBuilder* builder = new PayloadBuilder();
    uint8_t* payload = NULL;
    size_t payloadSize = 0;
    std::string backEndName;
    std::shared_ptr<Device> dev = nullptr;
    Stream *stream = NULL;
    Session *session = NULL;
    std::vector<Stream*> activeStreams;
    uint32_t miid = 0, ret = 0;
    struct param_id_haptics_th_vi_r0t0_set_param_t r0t0Value;
    FILE *fp = NULL;
    param_id_haptics_th_vi_r0t0_set_param_t *hpR0T0confg;
    param_id_haptics_vi_op_mode_param_t modeConfg;
    param_id_haptics_vi_channel_map_cfg_t HapticsviChannelMapConfg;
    param_id_haptics_ex_vi_ftm_set_cfg HapticsviExModeConfg;

    rm->getBackendName(mDeviceAttr.id, backEndName);
    dev = Device::getInstance(&mDeviceAttr, rm);
    ret = rm->getActiveStream_l(activeStreams, dev);
    if ((0 != ret) || (activeStreams.size() == 0)) {
        PAL_ERR(LOG_TAG, " no active stream available");
        goto exit;
    }
    stream = static_cast<Stream *>(activeStreams[0]);
    stream->getAssociatedSession(&session);
    ret = session->getMIID(backEndName.c_str(), MODULE_HAPTICS_VI, &miid);
    if (ret) {
        PAL_ERR(LOG_TAG, "Failed to get tag info %x, status = %d", MODULE_HAPTICS_VI, ret);
        goto exit;
    }

    if (customPayloadSize) {
        free(customPayload);
        customPayloadSize = 0;
    }

    memset(&modeConfg, 0, sizeof(modeConfg));
    memset(&HapticsviChannelMapConfg, 0, sizeof(HapticsviChannelMapConfg));
    memset(&HapticsviExModeConfg, 0, sizeof(HapticsviExModeConfg));
    memset(&r0t0Value, 0, sizeof(struct param_id_haptics_th_vi_r0t0_set_param_t));

    // Setting the mode of VI module
    modeConfg.th_operation_mode = NORMAL_MODE;
    builder->payloadHapticsDevPConfig(&payload, &payloadSize, miid,
                             PARAM_ID_HAPTICS_VI_OP_MODE_PARAM,(void *)&modeConfg);
    if (payloadSize) {
        ret = updateCustomPayload(payload, payloadSize);
        free(payload);
        if (0 != ret) {
            PAL_ERR(LOG_TAG," updateCustomPayload Failed for VI_OP_MODE_CFG\n");
        }
    }

    // Setting Channel Map configuration for VI module
    HapticsviChannelMapConfg.num_ch = 1;
    payloadSize = 0;

    builder->payloadHapticsDevPConfig(&payload, &payloadSize, miid,
                    PARAM_ID_HAPTICS_VI_CHANNEL_MAP_CFG,(void *)&HapticsviChannelMapConfg);
    if (payloadSize) {
        ret = updateCustomPayload(payload, payloadSize);
        free(payload);
        if (0 != ret) {
            PAL_ERR(LOG_TAG," updateCustomPayload Failed for CHANNEL_MAP_CFG\n");
        }
    }

    hpR0T0confg = (param_id_haptics_th_vi_r0t0_set_param_t *)calloc(1,
                         sizeof(param_id_haptics_th_vi_r0t0_set_param_t));
    if (!hpR0T0confg) {
        PAL_ERR(LOG_TAG," updateCustomPayload Failed\n");
        return;
    }
    hpR0T0confg->num_channels = numDevice;
    fp = fopen(PAL_HAP_DEVP_TEMP_PATH, "rb");
    if (fp) {
        PAL_DBG(LOG_TAG, " HapticsDevice calibrated. Send calibrated value");
        for (int i = 0; i < numDevice; i++) {
            fread(&r0t0Value.r0_cali_q24[i],
                    sizeof(r0t0Value.r0_cali_q24[i]), 1, fp);
            fread(&r0t0Value.t0_cali_q6[i],
                    sizeof(r0t0Value.t0_cali_q6[i]), 1, fp);
        }
    }
    else {
        PAL_DBG(LOG_TAG, " HapticsDevice not calibrated. Send safe value");
        for (int i = 0; i < numDevice; i++) {
            r0t0Value.r0_cali_q24[i] = MIN_RESISTANCE_HAPTICS_DEV_Q24;
            r0t0Value.t0_cali_q6[i] = SAFE_HAPTICS_DEV_TEMP_Q6;
        }
    }

    payloadSize = 0;
    builder->payloadHapticsDevPConfig(&payload, &payloadSize, miid,
                    PARAM_ID_HAPTICS_TH_VI_R0T0_SET_PARAM,(void *)hpR0T0confg);
    if (payloadSize) {
        ret = updateCustomPayload(payload, payloadSize);
        free(payload);
        free(hpR0T0confg);
        if (0 != ret) {
            PAL_ERR(LOG_TAG," updateCustomPayload Failed\n");
        }
    }
exit:
    if(builder) {
       delete builder;
       builder = NULL;
    }
    return;
}

HapticsDevFeedback::HapticsDevFeedback(struct pal_device *device,
                                std::shared_ptr<ResourceManager> Rm):HapticsDev(device, Rm)
{
    struct pal_device_info devinfo = {};

    memset(&mDeviceAttr, 0, sizeof(struct pal_device));
    memcpy(&mDeviceAttr, device, sizeof(struct pal_device));
    rm = Rm;


    rm->getDeviceInfo(mDeviceAttr.id, PAL_STREAM_PROXY, mDeviceAttr.custom_config.custom_key, &devinfo);
    numDevice = devinfo.channels;
}

HapticsDevFeedback::~HapticsDevFeedback()
{
}

int32_t HapticsDevFeedback::start()
{
    ResourceManager::isVIRecordStarted = true;
    // Do the customPayload configuration for VI path and call the Device::start
    PAL_DBG(LOG_TAG," Feedback start\n");
    updateVIcustomPayload();
    Device::start();

    return 0;
}

int32_t HapticsDevFeedback::stop()
{
    ResourceManager::isVIRecordStarted = false;
    PAL_DBG(LOG_TAG," Feedback stop\n");
    Device::stop();

    return 0;
}

std::shared_ptr<Device> HapticsDevFeedback::getInstance(struct pal_device *device,
                                                     std::shared_ptr<ResourceManager> Rm)
{
    PAL_DBG(LOG_TAG," Feedback getInstance\n");
    if (!obj) {
        std::shared_ptr<Device> sp(new HapticsDevFeedback(device, Rm));
        obj = sp;
    }
    return obj;
}
