/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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
#define LOG_NDEBUG 0
#define LOG_TAG "PAL: SpeakerProtectionTI"

#include "SpeakerProtectionTI.h"
#include "PalAudioRoute.h"
#include "ResourceManager.h"
#include "SessionAlsaUtils.h"
#include "kvh2xml.h"
#include <agm/agm_api.h>

#include<fstream>
#include<sstream>
																			
#include "SpeakerProtCommon.h"

#ifdef ENABLE_TAS_SPK_PROT
#define VI_FEEDBACK_MONO_1 "-mono-1"
#define DEFAULT_PERIOD_SIZE 256
#define DEFAULT_PERIOD_COUNT 4
																				
#define NORMAL_MODE 0
#define CALIBRATION_MODE 1
#define FACTORY_TEST_MODE 2
#define V_VALIDATION_MODE 3

#define CALIBRATION_STATUS_SUCCESS 1
#define CALIBRATION_STATUS_FAILURE 0

#define NUM_MAX_CALIB_RETRY 4
#define MAX_SP_SPKRS 2
#define CALIB_DURATION_SECS 2

#define TAS25XX_EFS_CALIB_RE_L      "/efs/tas25xx/calib_re"
#define TAS25XX_EFS_CALIB_TEMP_L    "/efs/tas25xx/amb_temp"
#define TAS25XX_EFS_CALIB_RE_R      "/efs/tas25xx/calib_re_r"
#define TAS25XX_EFS_CALIB_TEMP_R    "/efs/tas25xx/amb_temp_r"
#define TAS25XX_EFS_CALIB_STATUS    "/efs/tas25xx/calibration"

#ifndef PAL_PARAM_ID_SEPAKER_AMP_RUN_CAL
#define PAL_PARAM_ID_SEPAKER_AMP_RUN_CAL 3000
#endif

#define TAS25xx_AMB_TEMP "/sys/class/tas25xx_dev/cmd/temp"
#define TAS25xx_DEV_CMD_CALIB "/sys/class/tas25xx_dev/cmd/calib"
#define TAS25XX_EFS_DRV_OP_MODE "/sys/class/tas25xx_dev/cmd/drv_opmode"
#define TAS25XX_EFS_IV_VBAT "/sys/class/tas25xx_dev/cmd/iv_vbat"

#define MAX_STRING 20

#define GET_MIXER_STR "getParam"
#define SET_MIXER_STR "setParam"

bool SpeakerProtectionTI::isSpkrInUse;
static bool mCalRunning = false;
struct timespec SpeakerProtectionTI::spkrLastTimeUsed;
struct mixer *SpeakerProtectionTI::virtMixer;
struct mixer *SpeakerProtectionTI::hwMixer;
struct pcm * SpeakerProtectionTI::rxPcm = NULL;
struct pcm * SpeakerProtectionTI::txPcm = NULL;
int SpeakerProtectionTI::numberOfChannels;
struct pal_device_info SpeakerProtectionTI::vi_device;
int SpeakerProtectionTI::numberOfRequest;
pal_param_amp_bigdata_t SpeakerProtectionTI::bigdata;
pal_param_amp_ssrm_t SpeakerProtectionTI::ssrm;

int SpeakerProtectionTI::get_calib_data(spCalibData *ptical) {

    int ret = 0;
    int32_t ch_idx = 0;
    float re_f;
    int32_t oneq19 = (1 << 19);

    char *pcalibRefiles[] = {TAS25XX_EFS_CALIB_RE_L, TAS25XX_EFS_CALIB_RE_R};
    char *pcalibTempfiles[] = {TAS25XX_EFS_CALIB_TEMP_L, TAS25XX_EFS_CALIB_TEMP_R};

    for(ch_idx = 0; ch_idx < ptical->num_ch; ch_idx++) {
        ret = get_data_from_file(pcalibRefiles[ch_idx], FLOAT, (void*)&re_f);
        if(ret) {
            return ret;
        }
        ptical->r0_q19[ch_idx] = (int32_t)(re_f * oneq19);

        ret = get_data_from_file(pcalibTempfiles[ch_idx], INT, (void*)&ptical->t0[ch_idx]);
        if(ret) {
            return ret;
        }
    }

    return ret;
}

int SpeakerProtectionTI::get_data_from_file(const char* fname, tisa_calib_data_type_t dtype, void* pdata)
{
    FILE* fp;
    char data_from_file[MAX_STRING] = { 0 };
    int ret = 0;

    fp = fopen(fname, "r");
    if (!fp) {
        PAL_ERR(LOG_TAG, "file %s open failed", fname);
        return -1;
    }

    fscanf(fp, "%s ", data_from_file);

    switch (dtype) {
    case FLOAT:
        {
            float *pdata_fp = (float*)pdata;
            sscanf(data_from_file, "%f", pdata_fp);
            PAL_INFO(LOG_TAG, "data from file %s: %s(string) %05.2lf(float)\n",
                    fname, data_from_file, *pdata_fp);

        }
    break;
    case INT:
        {
            int *pdata_int = (int*)pdata;
            sscanf(data_from_file, "%d", pdata_int);
            PAL_INFO(LOG_TAG,"data from file %s: %s(string) %d(int)\n",
                    fname, data_from_file, *pdata_int);

        }
    break;
    case HEX:
        {
            int *pdata_int = (int*)pdata;
            sscanf(data_from_file, "0x%x", pdata_int);
            PAL_INFO(LOG_TAG,"data from file %s: %s(string) 0x%x(hex)\n",
                    fname, data_from_file, *pdata_int);
        }
    break;
    default:
        {
            PAL_ERR(LOG_TAG,"invalid file access for %s: %d\n", fname, dtype);
            ret = -1;
        }

    }

    fclose(fp);

    return ret;
}

int SpeakerProtectionTI::save_data_to_file(const char* fname, tisa_calib_data_type_t dtype, void *pdata)
{
    FILE* fp;

    fp = fopen(fname, "w");
    if (!fp) {
        PAL_ERR(LOG_TAG, "file %s open failed", fname);
        return -1;
    }

    switch (dtype) {
    case FLOAT:
    {
        float fdata;
        fdata = *((float*)pdata);
        fprintf(fp, "%05.2lf", fdata);
    }
    break;
    case INT:
    {
        int data = *((int*)pdata);
        fprintf(fp, "%d", data);
    }
    break;
    case STRING:
    {
        char *strdata = (char*)pdata;
        fprintf(fp, "%s", strdata);

    }
    break;
    case HEX:
    default:
    break;
    }

    fclose(fp);
    return 0;
}

/* Function to set status of speaker */
void SpeakerProtectionTI::spkrProtSetSpkrStatus(bool enable)
{
    PAL_DBG(LOG_TAG, "Enter");

    if (enable)
        isSpkrInUse = true;
    else {
        isSpkrInUse = false;
        clock_gettime(CLOCK_BOOTTIME, &spkrLastTimeUsed);
        PAL_INFO(LOG_TAG, "Speaker used last time %ld", spkrLastTimeUsed.tv_sec);
    }

    PAL_DBG(LOG_TAG, "Exit");
}

void SpeakerProtectionTI::disconnectFeandBe(std::vector<int> pcmDevIds,
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

    if (deviceMetaData.size) {
        ret = mixer_ctl_set_array(beMetaDataMixerCtrl, (void *)deviceMetaData.buf,
                    deviceMetaData.size);
        free(deviceMetaData.buf);
        deviceMetaData.buf = nullptr;
    } else {
        PAL_ERR(LOG_TAG, "Error: %d, Device Metadata not cleaned up", ret);
        goto exit;
    }

exit:
    return;
}

SpeakerProtectionTI::SpeakerProtectionTI(struct pal_device *device,
                        std::shared_ptr<ResourceManager> Rm):Device(device, Rm)
{
    int status = 0;
    struct pal_device_info devinfo = {};
    FILE *fp = NULL;

    rm = Rm;

    memset(&mDeviceAttr, 0, sizeof(struct pal_device));
    memcpy(&mDeviceAttr, device, sizeof(struct pal_device));

    isSpkrInUse = false;
    mCalRunning = false;

    if (device->id == PAL_DEVICE_OUT_HANDSET) {
        vi_device.channels = 1;
        numberOfChannels = 1;
        PAL_DBG(LOG_TAG, "Device id: %d vi_device.channels: %d numberOfChannels: %d",
                              device->id, vi_device.channels, numberOfChannels);
        return;
    }

    rm->getDeviceInfo(PAL_DEVICE_OUT_SPEAKER, PAL_STREAM_PROXY, "", &devinfo);
    numberOfChannels = devinfo.channels;
    PAL_DBG(LOG_TAG, "Number of Channels %d", numberOfChannels);

    rm->getDeviceInfo(PAL_DEVICE_IN_VI_FEEDBACK, PAL_STREAM_PROXY, "", &vi_device);
    PAL_DBG(LOG_TAG, "Number of Channels for VI path is %d", vi_device.channels);

    // Get current time
    clock_gettime(CLOCK_BOOTTIME, &spkrLastTimeUsed);

    // Getting mixture controls from Resource Manager
    status = rm->getVirtualAudioMixer(&virtMixer);
    if (status) {
        PAL_ERR(LOG_TAG,"virt mixer error %d", status);
    }
    status = rm->getHwAudioMixer(&hwMixer);
    if (status) {
        PAL_ERR(LOG_TAG,"hw mixer error %d", status);
    }

}

SpeakerProtectionTI::~SpeakerProtectionTI()
{
    if (customPayload)
        free(customPayload);

    customPayload = NULL;
    customPayloadSize = 0;
}

/*
 * Function to trigger Processing mode.
 * The parameter that it accepts are below:
 * true - Start Processing Mode
 * false - Stop Processing Mode
 */
int32_t SpeakerProtectionTI::spkrProtProcessingMode(bool flag)
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
    struct vi_r0t0_cfg_t r0t0Array[numberOfChannels];
    struct agmMetaData deviceMetaData(nullptr, 0);
    struct mixer_ctl *beMetaDataMixerCtrl = nullptr;
    FILE *fp;
    std::string backEndName, backEndNameRx;
    std::vector <std::pair<int, int>> keyVector;
    std::vector <std::pair<int, int>> calVector;
    std::shared_ptr<ResourceManager> rm;
    std::ostringstream connectCtrlNameBeVI;
    std::ostringstream connectCtrlNameBeSP;
    std::ostringstream connectCtrlName;
    param_id_sp_vi_op_mode_cfg_t modeConfg;
    param_id_sp_vi_channel_map_cfg_t viChannelMapConfg;
    param_id_sp_op_mode_t spModeConfg;
    std::shared_ptr<Device> dev = nullptr;
    Stream *stream = NULL;
    Session *session = NULL;
    std::vector<Stream*> activeStreams;
    PayloadBuilder* builder = new PayloadBuilder();

    PAL_INFO(LOG_TAG, "Flag %d", flag);
    deviceMutex.lock();


    if (flag) {
        numberOfRequest++;
        if (numberOfRequest > 1) {
            // R0T0 already set, we don't need to process the request
            goto exit;
        }
        PAL_DBG(LOG_TAG, "Custom payload size %zu, Payload %p", customPayloadSize,
                customPayload);

        if (customPayload) {
            free(customPayload);
        }
        customPayloadSize = 0;
        customPayload = NULL;

        spkrProtSetSpkrStatus(flag);
        // Speaker in use. Start the Processing Mode
        rm = ResourceManager::getInstance();
        if (!rm) {
            PAL_ERR(LOG_TAG, "Failed to get resource manager instance");
            goto exit;
        }

        memset(&device, 0, sizeof(device));
        memset(&sAttr, 0, sizeof(sAttr));
        memset(&config, 0, sizeof(config));
        memset(&modeConfg, 0, sizeof(modeConfg));
        memset(&viChannelMapConfg, 0, sizeof(viChannelMapConfg));
        memset(&spModeConfg, 0, sizeof(spModeConfg));

        keyVector.clear();
        calVector.clear();
#ifndef DEBUG_DIS_TX
        // Configure device attribute
       if (vi_device.channels > 1) {
            ch_info.channels = CHANNELS_2;
            ch_info.ch_map[0] = PAL_CHMAP_CHANNEL_FL;
            ch_info.ch_map[1] = PAL_CHMAP_CHANNEL_FR;
        }
        else {
            ch_info.channels = CHANNELS_1;
            ch_info.ch_map[0] = PAL_CHMAP_CHANNEL_FR;
        }

        rm->getChannelMap(&(ch_info.ch_map[0]), vi_device.channels);
        ch_info.channels = vi_device.channels;

        if (mDeviceAttr.id == PAL_DEVICE_OUT_HANDSET)
                 ch_info.ch_map[0] = PAL_CHMAP_CHANNEL_FL;

        switch (vi_device.channels) {
            case 1 :
                ch_info.channels = CHANNELS_1;
            break;
            case 2 :
                ch_info.channels = CHANNELS_2;
            break;
            case 4:
                ch_info.channels = CHANNELS_4;
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

#ifdef ENABLE_TAS_SPK_PROT
        PAL_INFO(LOG_TAG, "vi_device info - device.config.bit_width %d", device.config.bit_width);
        PAL_INFO(LOG_TAG, "vi_device info - device.config.aud_fmt_id %d", device.config.aud_fmt_id);
#endif
        
        // Setup TX path
        device.id = PAL_DEVICE_IN_VI_FEEDBACK;

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
                 if (mDeviceAttr.id == PAL_DEVICE_OUT_HANDSET)
                      calVector.push_back(std::make_pair(SPK_PRO_VI_MAP, LEFT_SPKR));
                 else
                      calVector.push_back(std::make_pair(SPK_PRO_VI_MAP, RIGHT_SPKR));
            break;
            case 2 :
                calVector.push_back(std::make_pair(SPK_PRO_VI_MAP, STEREO_SPKR));
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
        } else {
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
        sAttr.type = PAL_STREAM_LOW_LATENCY;
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
            case 4:
                config.channels = CHANNELS_4;
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

        ret = SessionAlsaUtils::getModuleInstanceId(virtMixer, pcmDevIdTx.at(0),
                        backEndName.c_str(), MODULE_VI, &miid);
        if (0 != ret) {
            PAL_ERR(LOG_TAG, "Failed to get tag info %x, status = %d", MODULE_VI, ret);
            goto free_fe;
        }

        modeConfg.num_speakers = numberOfChannels;
        modeConfg.th_operation_mode = NORMAL_MODE;
        modeConfg.th_quick_calib_flag = 0;

        PAL_ERR(LOG_TAG, "payload PARAM_ID_SP_VI_OP_MODE_CFG 0x%x", PARAM_ID_SP_VI_OP_MODE_CFG);

        builder->payloadSPConfig(&payload, &payloadSize, miid,
                PARAM_ID_SP_VI_OP_MODE_CFG,(void *)&modeConfg);

        PAL_ERR(LOG_TAG, "payload PARAM_ID_SP_VI_OP_MODE_CFG 0x%x", PARAM_ID_SP_VI_OP_MODE_CFG);

        builder->payloadSPConfig(&payload, &payloadSize, miid,
                PARAM_ID_SP_VI_OP_MODE_CFG,(void *)&modeConfg);

        if (payloadSize) {
            ret = updateCustomPayload(payload, payloadSize);
            free(payload);
            if (0 != ret) {
                PAL_ERR(LOG_TAG,"updateCustomPayload Failed for VI_OP_MODE_CFG\n");
                // Not fatal as by default VI module runs in Normal mode
                ret = 0;

		// Setting Channel Map configuration for VI module
		// TODO: Move this to ACDB file
		viChannelMapConfg.num_ch = numberOfChannels * 2;
		payloadSize = 0;

		builder->payloadSPConfig(&payload, &payloadSize, miid,
		        PARAM_ID_SP_VI_CHANNEL_MAP_CFG,(void *)&viChannelMapConfg);
		if (payloadSize) {
		    ret = updateCustomPayload(payload, payloadSize);
		    free(payload);
		    if (0 != ret) {
		        PAL_ERR(LOG_TAG," updateCustomPayload Failed for CHANNEL_MAP_CFG\n");
		    }
		}

            }
        }
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
#endif
        // Setting up SP mode
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

        ret = session->getMIID(backEndNameRx.c_str(), MODULE_SP, &miid);
        if (ret) {
            PAL_ERR(LOG_TAG, "Failed to get tag info %x, status = %d", MODULE_SP, ret);
            goto err_pcm_open;
        }

        // Set the operation mode for SP module
        PAL_DBG(LOG_TAG, "Operation mode for SP %d",
                        rm->mSpkrProtModeValue.operationMode);

        spModeConfg.operation_mode = NORMAL_MODE;

        spCalibData tical;
        tical.num_ch = numberOfChannels;

        memset(tical.r0_q19, 0, sizeof(tical.r0_q19));
        memset(tical.t0, 0, sizeof(tical.t0));

        ret = get_calib_data(&tical);

        if(!ret) {
            builder->payloadTISPConfig(&payload, &payloadSize, miid,
                    TI_PARAM_ID_SP_CALIB_DATA,(void *)&tical);
            if (payloadSize) {
                ret = updateCustomPayload(payload, payloadSize);
                free(payload);
                if (0 != ret) {
                    PAL_ERR(LOG_TAG," updateCustomPayload Failed for TI_PARAM_ID_SP_CALIB_DATA\n");
                }
            }
        } else {
            PAL_ERR(LOG_TAG,
                    "[TI-SmartPA] Error while getting calibration data, calibration data not applied\n");
        }

        int iv_vbat;

        ret = get_data_from_file(TAS25XX_EFS_IV_VBAT, HEX, (void*)&iv_vbat);
        if(!ret) {

            PAL_ERR(LOG_TAG,"iv_vbat 0x%x, iv bitwidth=%d, vbat_mon=%d\n", iv_vbat,
                    iv_vbat & 0xFFFF, (iv_vbat & 0xFFFF0000)>>16);

            builder->payloadTISPConfig(&payload, &payloadSize, miid,
                    TI_PARAM_ID_SP_SET_VBAT_MON,(void *)&iv_vbat);
            if (payloadSize) {
                ret = updateCustomPayload(payload, payloadSize);
                free(payload);
                if (0 != ret) {
                    PAL_ERR(LOG_TAG," updateCustomPayload Failed for TI_PARAM_ID_SP_SET_VBAT_MON\n");
                }
            }
        } else {
            PAL_ERR(LOG_TAG,
                    "[TI-SmartPA] Error while getting iv_vbat format\n");
        }

        int drv_op_mode;

        ret = get_data_from_file(TAS25XX_EFS_DRV_OP_MODE, HEX, (void*)&drv_op_mode);

        PAL_ERR(LOG_TAG,"drv_op_mode 0x%x, bypass=%d, LR mode=%d\n", drv_op_mode,
                (drv_op_mode & 0xF0)>>4, drv_op_mode & 0xF);

        if(!ret) {
            builder->payloadTISPConfig(&payload, &payloadSize, miid,
                    TI_PARAM_ID_SP_SET_DRV_OP_MODE,(void *)&drv_op_mode);
            if (payloadSize) {
                ret = updateCustomPayload(payload, payloadSize);
                free(payload);
                if (0 != ret) {
                    PAL_ERR(LOG_TAG," updateCustomPayload Failed for TI_PARAM_ID_SP_SET_DRV_OP_MODE\n");
                }
            }
        } else {
            PAL_ERR(LOG_TAG,
                    "[TI-SmartPA] Error while getting driver op mode format\n");
        }

        if (customPayloadSize) {
            ret = SessionAlsaUtils::setDeviceCustomPayload(rm, backEndNameRx, customPayload,
                    customPayloadSize);
            if (ret) {
                PAL_ERR(LOG_TAG,
                        "Unable to set custom param for SP_OP_MODE and TI_PARAM_ID_SP_CALIB_DATA");
                goto exit;
            }
        }

        if (customPayloadSize) {
            free(customPayload);
            customPayload = NULL;
            customPayloadSize = 0;
        }

#ifndef DEBUG_DIS_TX
        enableDevice(audioRoute, mSndDeviceName_vi);
        PAL_DBG(LOG_TAG, "pcm start for TX");
        if (pcm_start(txPcm) < 0) {
            PAL_ERR(LOG_TAG, "pcm start failed for TX path");
            goto err_pcm_open;
        }
#endif
        // Free up the local variables
        goto exit;
    }
    else {
        if (numberOfRequest == 0) {
            PAL_ERR(LOG_TAG, "Device not started yet, Stop not expected");
            goto exit;
        }
        numberOfRequest--;
        if (numberOfRequest > 0) {
            // R0T0 already set, we don't need to process the request.
            goto exit;
        }

        smartpa_get_bigdata();

        spkrProtSetSpkrStatus(flag);
#ifndef DEBUG_DIS_TX
        // Speaker not in use anymore. Stop the processing mode
        PAL_DBG(LOG_TAG, "Closing VI path");
        if (txPcm) {
            rm = ResourceManager::getInstance();
            device.id = PAL_DEVICE_IN_VI_FEEDBACK;

            ret = rm->getAudioRoute(&audioRoute);
            if (0 != ret) {
                PAL_ERR(LOG_TAG, "Failed to get the audio_route address status %d", ret);
                goto exit;
            }

           // ret = rm->getSndDeviceName(device.id , mSndDeviceName_vi);
            strlcpy(mSndDeviceName_vi, vi_device.sndDevName.c_str(), DEVICE_NAME_MAX_SIZE);
            rm->getBackendName(device.id, backEndName);
            if (!strlen(backEndName.c_str())) {
                PAL_ERR(LOG_TAG, "Failed to obtain tx backend name for %d", device.id);
                goto exit;
            }
            pcm_stop(txPcm);
            if (pcmDevIdTx.size() != 0) {
                if (isTxFeandBeConnected) {
                    disconnectFeandBe(pcmDevIdTx, backEndName);
                }
                sAttr.type = PAL_STREAM_LOW_LATENCY;
                sAttr.direction = PAL_AUDIO_INPUT_OUTPUT;
                rm->freeFrontEndIds(pcmDevIdTx, sAttr, dir);
                pcmDevIdTx.clear();
            }
            pcm_close(txPcm);
            disableDevice(audioRoute, mSndDeviceName_vi);
            txPcm = NULL;
            goto exit;
        }
#endif
    }

err_pcm_open :
#ifndef DEBUG_DIS_TX
    if (pcmDevIdTx.size() != 0) {
        if (isTxFeandBeConnected) {
            disconnectFeandBe(pcmDevIdTx, backEndName);
        }
        rm->freeFrontEndIds(pcmDevIdTx, sAttr, dir);
        pcmDevIdTx.clear();
    }
    if (txPcm) {
        pcm_close(txPcm);
        disableDevice(audioRoute, mSndDeviceName_vi);
        txPcm = NULL;
    }
#endif
    goto exit;

free_fe:
#ifndef DEBUG_DIS_TX
    if (pcmDevIdTx.size() != 0) {
        if (isTxFeandBeConnected) {
            disconnectFeandBe(pcmDevIdTx, backEndName);
        }
        rm->freeFrontEndIds(pcmDevIdTx, sAttr, dir);
        pcmDevIdTx.clear();
    }
#endif
exit:
    deviceMutex.unlock();
    if(builder) {
       delete builder;
       builder = NULL;
    }
    return ret;
}

void SpeakerProtectionTI::updateSPcustomPayload(int param_id, void *spConfig)
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

    rm->getBackendName(mDeviceAttr.id, backEndName);
    dev = Device::getInstance(&mDeviceAttr, rm);
    ret = rm->getActiveStream_l(activeStreams, dev);
    if ((0 != ret) || (activeStreams.size() == 0)) {
        PAL_ERR(LOG_TAG, " no active stream available");
        goto exit;
    }
    stream = static_cast<Stream *>(activeStreams[0]);
    stream->getAssociatedSession(&session);
    ret = session->getMIID(backEndName.c_str(), MODULE_SP, &miid);
    if (ret) {
        PAL_ERR(LOG_TAG, "Failed to get tag info %x, status = %d", MODULE_SP, ret);
        goto exit;
    }

    if (customPayloadSize) {
        free(customPayload);
        customPayload = NULL;
        customPayloadSize = 0;
    }

    payloadSize = 0;
    builder->payloadTISPConfig(&payload, &payloadSize, miid,
                    param_id, spConfig);
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

int SpeakerProtectionTI::start()
{
    PAL_DBG(LOG_TAG, "Enter");

    if (ResourceManager::isVIRecordStarted) {
        PAL_DBG(LOG_TAG, "record running so just update SP payload");
    }
    else {
        spkrProtProcessingMode(true);
    }

    PAL_DBG(LOG_TAG, "Calling Device start");
    Device::start();
    return 0;
}

int SpeakerProtectionTI::stop()
{
    PAL_DBG(LOG_TAG, "Inside Speaker Protection stop");
    Device::stop();
    if (ResourceManager::isVIRecordStarted) {
        PAL_DBG(LOG_TAG, "record running so no need to proceed");
        ResourceManager::isVIRecordStarted = false;
        return 0;
    }
    spkrProtProcessingMode(false);
    return 0;
}

int32_t SpeakerProtectionTI::getMixerControl(struct mixer_ctl **ctl, unsigned *miid,
        char *mixer_str) {

    std::shared_ptr<Device> dev = nullptr;
    struct pal_device device;
    std::vector<Stream*> activeStreams;
    Session *session = NULL;
    Stream *stream = NULL;
    int ret = 0, status;
    std::shared_ptr<ResourceManager> rm;
    int pcmid;
    std::string backEndName;

    rm = ResourceManager::getInstance();

    rm->getBackendName(mDeviceAttr.id, backEndName);
    if (!strlen(backEndName.c_str())) {
        PAL_ERR(LOG_TAG, "Failed to obtain rx backend name for %d", device.id);
        goto exit;
    }

    memset(&device, 0, sizeof(device));
    device.id = PAL_DEVICE_OUT_SPEAKER;

    dev = Device::getInstance(&mDeviceAttr, rm);

    ret = rm->getActiveStream_l(activeStreams, dev);
    if ((0 != ret) || (activeStreams.size() == 0)) {
        PAL_ERR(LOG_TAG, " no active stream available");
        ret = -EINVAL;
        goto exit;
    }
    stream = static_cast<Stream *>(activeStreams[0]);
    stream->getAssociatedSession(&session);

    ret = session->getMIID(backEndName.c_str(), MODULE_SP, miid);
    if (ret) {
        PAL_ERR(LOG_TAG, "Failed to get tag info %x, status = %d", MODULE_SP, ret);
        goto exit;
    }

    PAL_INFO(LOG_TAG, "%s miid = %x(%d)", __func__, *miid, *miid);

#ifdef ENABLE_TAS_SPK_PROT
    *ctl = session->getFEMixerCtl(mixer_str, &pcmid, PAL_AUDIO_OUTPUT);
#else
    *ctl = session->getFEMixerCtl(mixer_str, &pcmid);
#endif

exit:
    return ret;
}

int32_t SpeakerProtectionTI::runTICalibration()
{
    std::shared_ptr<Device> dev = nullptr;
    struct pal_device device;
    char mSndDeviceName[128] = {0};
    std::string backEndName;
    std::vector<Stream*> activeStreams;
    Session *session = NULL;
    Stream *stream = NULL;
    int ret, status;
    unsigned miid;
    uint8_t* payload = NULL;
    size_t payloadSize = 0;
    static struct mixer *virtMixer;
    std::shared_ptr<ResourceManager> rm;
    int pcmid;
    struct mixer_ctl *ctl = NULL;
    struct mixer_ctl *ctlsetparam = NULL;
    struct pal_stream_attributes sAttr;
    struct pal_device_info devinfo;
    std::vector<int> pcmDevIdsRx;
    std::ostringstream cntrlName;
    char *pcmDeviceName = NULL;
    uint32_t data[8];
    int32_t *pdata;
    int32_t n1q19 = 1 << 19;
    char *pcalibRefiles[] = {TAS25XX_EFS_CALIB_RE_L, TAS25XX_EFS_CALIB_RE_R};
    char *pcalibTempfiles[] = {TAS25XX_EFS_CALIB_TEMP_L, TAS25XX_EFS_CALIB_TEMP_R};

    int calib_success[MAX_SP_SPKRS] = {0};
    int re_range[MAX_SP_SPKRS][2] = {{0}, {0}};
    int calib_passed_for_both_ch = 0;
    int num_retry = 0;
    session_callback sessionCb;
    int payload_size = sizeof(struct agm_event_reg_cfg);

    char *dev_cal_init_str =  "cal_init_blk";
    char *dev_cal_deinit_str =  "cal_deinit_blk";
    char *dev_cal_init_mxr_str[MAX_SP_SPKRS] = {
        "TAS25XX CALIB_INIT_TOP",
        "TAS25XX CALIB_INIT_BOTTOM",
    };

    struct mixer_ctl *calCtrl = NULL;

    char *cal_start_status =  "Enabled";
    char *cal_stop_status =  "Disabled";

    int32_t getRe[MAX_SP_SPKRS] = { 0 };
    int32_t amb_temp[MAX_SP_SPKRS] = { 0 };

    rm = ResourceManager::getInstance();

    PayloadBuilder* builder = new PayloadBuilder();

#if 0
    ret = save_data_to_file(TAS25xx_DEV_CMD_CALIB, STRING, (void*)dev_cal_init_str);
    if(ret) {
        PAL_ERR(LOG_TAG, "Failed to set \"%s\" to file \"%s\"", dev_cal_init_str,
                TAS25xx_DEV_CMD_CALIB);
    }
#else
    if (hwMixer) {
        for (int ch_idx = 0; ch_idx < numberOfChannels; ch_idx++) {
            std::ostringstream calCtrlName;
            calCtrlName << dev_cal_init_mxr_str[ch_idx];
            PAL_DBG(LOG_TAG, "get mixer control: %s", calCtrlName.str().data());
            calCtrl = mixer_get_ctl_by_name(hwMixer, calCtrlName.str().data());
            if (!calCtrl) {
                PAL_ERR(LOG_TAG, "invalid mixer control: %s for hwMixer",
                    calCtrlName.str().data());
            } else {
                PAL_INFO(LOG_TAG, "apply mixer control: %s", calCtrlName.str().data());
                ret = mixer_ctl_set_enum_by_string(calCtrl, "ENABLE");
                if (ret) {
                    PAL_ERR(LOG_TAG, "Error: %d, Mixer control %s set with %s failed", ret,
                    calCtrlName.str().data(), "ENABLE");
                }
            }
        }
    } else {
        PAL_ERR(LOG_TAG, "Error: running calib without device initialisation");
    }

#endif

    ret = save_data_to_file(TAS25XX_EFS_CALIB_STATUS, STRING, (void*)cal_start_status);
    if(ret) {
        PAL_ERR(LOG_TAG, "Failed to set alibration start status to %s",
                TAS25XX_EFS_CALIB_STATUS);
    }

    PAL_INFO(LOG_TAG, "%s mDeviceAttr.id=%d, we are creating %d",
        __func__, mDeviceAttr.id, PAL_DEVICE_OUT_SPEAKER);

    ret = getMixerControl(&ctl, &miid, GET_MIXER_STR);
    if (ret || !ctl) {
        status = -ENOENT;
        PAL_ERR(LOG_TAG, "Error: %d Invalid mixer control: %s\n",
                status, GET_MIXER_STR);
        goto exit;
    }

    ret = getMixerControl(&ctlsetparam, &miid, SET_MIXER_STR);
    if (ret || !ctl) {
        status = -ENOENT;
        PAL_ERR(LOG_TAG, "Error: %d Invalid mixer control: %s\n",
                status, GET_MIXER_STR);
        goto exit;
    }

    for (int ch_idx = 0; ch_idx < numberOfChannels; ch_idx++) {

        builder->payloadTISPConfig(&payload, &payloadSize, miid,
             TAS_CALC_PARAM_IDX(TAS_SA_GET_RE_RANGE, 2, ch_idx+1), (void *)data);
        if (payload) {
            status = mixer_ctl_set_array(ctl, payload, payloadSize);
            if (0 != status) {
                PAL_ERR(LOG_TAG, "Set failed status = %d for TAS_SA_GET_RE_RANGE for ch-%d",
                        status, ch_idx);
                goto exit;
            }
            status = mixer_ctl_get_array(ctl, payload, payloadSize);
            if (0 != status) {
                PAL_ERR(LOG_TAG, "Get failed status = %d for TAS_SA_GET_RE_RANGE for ch-%d",
                        status, ch_idx);
            } else {
                pdata = (int32_t *)(payload + sizeof(struct apm_module_param_data_t));

                re_range[ch_idx][0] = pdata[0] >> 8; /*q27 to q19*/
                re_range[ch_idx][1] = pdata[1] >> 8; /*q27 to q19*/

                PAL_INFO(LOG_TAG,
                    "[TI-SmartPA] Get Re Range 0x%x(%.2lf) - 0x%x(%.2lf) for ch-%d\n",
                    re_range[ch_idx][0],(float)re_range[ch_idx][0] / n1q19 ,
                    re_range[ch_idx][1], (float)re_range[ch_idx][1] / n1q19, ch_idx);

            }

            free(payload);
            payload = NULL;
            payloadSize = 0;
        }
    } /*<< done get Re range */


    while (num_retry < NUM_MAX_CALIB_RETRY) {
        for (int ch_idx = 0; ch_idx < numberOfChannels; ch_idx++) {
            if (calib_success[ch_idx])
                continue;

            builder->payloadTISPConfig(&payload, &payloadSize, miid,
                    TAS_CALC_PARAM_IDX(TAS_SA_CALIB_INIT, 1, ch_idx+1), (void *)data);
            if (payload) {
                status = mixer_ctl_set_array(ctlsetparam, payload, payloadSize);
                if (0 != status) {
                    PAL_ERR(LOG_TAG, "Set failed status = %d for TAS_SA_CALIB_INIT for ch-%d", status, ch_idx);
                }

                free(payload);
                payload = NULL;
                payloadSize = 0;
            }
        } /*<< calib init for failed channels done */

        sleep(CALIB_DURATION_SECS);

        for (int ch_idx = 0; ch_idx < numberOfChannels; ch_idx++) {

            if (calib_success[ch_idx])
                continue;

            builder->payloadTISPConfig(&payload, &payloadSize, miid,
                    TAS_CALC_PARAM_IDX(TAS_SA_GET_RE, 1, ch_idx+1), (void *)data);
            if (payload) {
                status = mixer_ctl_set_array(ctl, payload, payloadSize);
                if (0 != status) {
                    PAL_ERR(LOG_TAG, "Set failed status = %d for TAS_SA_GET_RE for ch-%d", status, ch_idx);
                    goto exit;
                }

                status = mixer_ctl_get_array(ctl, payload, payloadSize);
                if (0 != status) {
                    PAL_ERR(LOG_TAG, "Get failed status = %d for TAS_SA_GET_RE for ch-%d", status, ch_idx);
                } else {
                    pdata = (int32_t *)(payload + sizeof(struct apm_module_param_data_t));
                    PAL_INFO(LOG_TAG,
                        "[TI-SmartPA] Get Re 0x%x (%05.2lf) for ch-%d\n",
                        pdata[0], (float)pdata[0] / n1q19, ch_idx);
                    getRe[ch_idx] = pdata[0];

                }

                free(payload);
                payload = NULL;
                payloadSize = 0;
            }


            builder->payloadTISPConfig(&payload, &payloadSize, miid,
                    TAS_CALC_PARAM_IDX(TAS_SA_CALIB_DEINIT, 1, ch_idx+1), (void *)data);
            if (payload) {
                status = mixer_ctl_set_array(ctlsetparam, payload, payloadSize);
                if (0 != status) {
                    PAL_ERR(LOG_TAG, "Set failed status = %d for TAS_SA_CALIB_DEINIT for ch-%d", status, ch_idx);
                }
                free(payload);
                payload = NULL;
                payloadSize = 0;
            }

            /* Re value within range for that channel */
            if ((getRe[ch_idx] > re_range[ch_idx][0]) && (getRe[ch_idx] < re_range[ch_idx][1])) {

                /* saving the data to /efs partition */
                PAL_DBG(LOG_TAG, "Calibration Passed for ch-%d", ch_idx);

                /* read temp from /sys/class/tas25xx/amb_temp */
                ret = get_data_from_file(TAS25xx_AMB_TEMP, INT, &amb_temp[ch_idx]);
                if(ret) {
                    PAL_ERR(LOG_TAG, "Error while Reading calibartion Temp from file %s for ch-%d",
                            TAS25xx_AMB_TEMP, ch_idx);
                    break;
                }

                calib_success[ch_idx] = 1;
            } else {
                PAL_DBG(LOG_TAG, "Calibration Failed for ch-%d, trial=%d", ch_idx, num_retry);
                calib_success[ch_idx] = 0;
            }
        } /*<< for each channel >> */

        if (numberOfChannels == 2) {
            calib_passed_for_both_ch = calib_success[0] && calib_success[1];
            PAL_DBG(LOG_TAG, "calibratin status %s, trail=%d", calib_passed_for_both_ch ? "Pass": "Fail", num_retry);
            if (calib_passed_for_both_ch)
                break;
        } else if (numberOfChannels == 1) {
            PAL_DBG(LOG_TAG, "calibratin status %s, trail=%d", calib_success[0] ? "Pass": "Fail", num_retry);
            if (calib_success[0])
                break;
        }

        num_retry++;
    } /*<< while >> */

    if(calib_passed_for_both_ch && (numberOfChannels == 2)) {
        for (int ch_idx = 0; ch_idx < numberOfChannels; ch_idx++) {
            /* save re value for reference */
            float re_f = (float)getRe[ch_idx] / n1q19;
            ret = save_data_to_file(pcalibRefiles[ch_idx], FLOAT, (void*)&re_f);
            if(ret) {
                PAL_ERR(LOG_TAG, "Error while saving calibartion Re to file %s for ch-%d",
                    pcalibRefiles[ch_idx], ch_idx);
            }

            ret = save_data_to_file(pcalibTempfiles[ch_idx], INT, (void*)&amb_temp[ch_idx]);
            if(ret) {
                PAL_ERR(LOG_TAG, "Error while saving calibartion Temp to file %s for ch-%d",
                        pcalibTempfiles[ch_idx],
                        ch_idx);
            }
        }
    } else if ((numberOfChannels == 1) && (calib_success[0])) {
        float re_f = (float)getRe[0] / n1q19;
        ret = save_data_to_file(pcalibRefiles[0], FLOAT, (void*)&re_f);
        if(ret) {
            PAL_ERR(LOG_TAG, "Error while saving calibartion Re to file %s for ch-0",
                pcalibRefiles[0]);
        }

        ret = save_data_to_file(pcalibTempfiles[0], INT, (void*)&amb_temp[0]);
        if(ret) {
            PAL_ERR(LOG_TAG, "Error while saving calibartion Temp to file %s for ch-0",
                    pcalibTempfiles[0]);
        }
    }

exit:
#if 0
    ret = save_data_to_file(TAS25xx_DEV_CMD_CALIB, STRING, (void*)dev_cal_deinit_str);
    if(ret) {
        PAL_ERR(LOG_TAG, "Failed to set \"%s\" to file \"%s\"", dev_cal_deinit_str,
                TAS25xx_DEV_CMD_CALIB);
    }
#else
    if (hwMixer) {
        for (int ch_idx = 0; ch_idx < numberOfChannels; ch_idx++) {
            std::ostringstream calCtrlName;
            calCtrlName << dev_cal_init_mxr_str[ch_idx];
            PAL_DBG(LOG_TAG, "get mixer control: %s", calCtrlName.str().data());
            calCtrl = mixer_get_ctl_by_name(hwMixer, calCtrlName.str().data());
            if (!calCtrl) {
                PAL_ERR(LOG_TAG, "invalid mixer control: %s for hwMixer",
                    calCtrlName.str().data());
            } else {
                PAL_INFO(LOG_TAG, "apply mixer control: %s", calCtrlName.str().data());
                ret = mixer_ctl_set_enum_by_string(calCtrl, "DISABLE");
                if (ret) {
                    PAL_ERR(LOG_TAG, "Error: %d, Mixer control %s set with %s failed", ret,
                    calCtrlName.str().data(), "DISABLE");
                }
            }
        }
    }
#endif

    ret = save_data_to_file(TAS25XX_EFS_CALIB_STATUS, STRING, (void*)cal_stop_status);
    if(ret) {
        PAL_ERR(LOG_TAG, "Failed to set update calibration status to %s",
                TAS25XX_EFS_CALIB_STATUS);
    }

    if(builder) {
       delete builder;
       builder = NULL;
    }

    return ret;

}

//TODO:
//CLASS MEMBERS
//bool mCalRunning;
//std::mutex calMutex;
//void SpeakerProtectionTI::spkrCalibrationThread()
//CLASS MEMBERS

void SpeakerProtectionTI::spkrCalibrationThread()
{
    calMutex.lock();

    if (mCalRunning) {
        PAL_INFO(LOG_TAG,
            "Duplicate request ignored PAL_PARAM_ID_SEPAKER_AMP_RUN_CAL");
    } else {
        mCalRunning = true;
        calMutex.unlock();
        runTICalibration();
        calMutex.lock();
        mCalRunning = false;
    }

    calMutex.unlock();
}

int32_t SpeakerProtectionTI::setParameter(uint32_t param_id, void *param)
{
    PAL_DBG(LOG_TAG, "Inside Speaker Protection Set parameters");
    (void ) param;
    int32_t ret = 0;

    switch(param_id) {

    case PAL_PARAM_ID_SEPAKER_AMP_RUN_CAL: // 3000
    {
        PAL_DBG(LOG_TAG, "PAL_PARAM_ID_SEPAKER_AMP_RUN_CAL");
        std::thread tiCalThread(&SpeakerProtectionTI::spkrCalibrationThread, this);
        tiCalThread.detach();
    }
    break;

    case PAL_PARAM_ID_SPEAKER_AMP_TEMPERATURE_RCV:
    case PAL_PARAM_ID_SPEAKER_AMP_TEMPERATURE_SPK:
    {
        uint8_t* payload = NULL;
        size_t payloadSize = 0;
        mixer_ctl *ctlsetparam = NULL;
        unsigned miid;
        PayloadBuilder* builder = new PayloadBuilder();
        int ch_idx = (param_id == PAL_PARAM_ID_SPEAKER_AMP_TEMPERATURE_SPK) ? 0 : 1;

        ret = getMixerControl(&ctlsetparam, &miid, SET_MIXER_STR);
        if (ret || !ctlsetparam) {
            PAL_ERR(LOG_TAG, "Error: %d Invalid mixer control: %s\n",
                    ret, GET_MIXER_STR);
            PAL_ERR(LOG_TAG, "PAL_PARAM_ID_SPEAKER_AMP_TEMPERATURE not set for ch-%d",
                    ch_idx);
        } else {
            pal_param_amp_ssrm_t* param_amp_temp = (pal_param_amp_ssrm_t *)param;
            if (param) {
                PAL_DBG(LOG_TAG, "param_amp_temp temperature %d for ch-%d",
                    param_amp_temp->temperature, ch_idx);

                builder->payloadTISPConfig(&payload, &payloadSize, miid,
                    TAS_CALC_PARAM_IDX(TAS_SA_SET_SKIN_TEMP, 1, ch_idx+1),
                        (void *)&param_amp_temp->temperature);
                if (payload) {
                    ret = mixer_ctl_set_array(ctlsetparam, payload, payloadSize);
                    if (ret) {
                        PAL_ERR(LOG_TAG, "Set failed status = %d for TAS_SA_CALIB_DEINIT for ch-%d",
                            ret, ch_idx);
                    }

                    free(payload);
                    payload = NULL;
                    payloadSize = 0;
                }
            }
        }
    }
    break;

    default:
        PAL_DBG(LOG_TAG, "Unknown param_id 0x%x", param_id);
    }

    return ret;
}

void SpeakerProtectionTI::printBigdata(int ch_idx)
{
    PAL_INFO(LOG_TAG,
        "[TI-SmartPA] big data MaxTemp %d, MaxTemp(keep) %d, MaxExc %d(%f), TOC %d EOC %d for channel-%d\n",
        bigdata.value[MAX_TEMP_L + ch_idx],
        bigdata.value[KEEP_MAX_TEMP_L + ch_idx],
        bigdata.value[MAX_EXCU_L + ch_idx],
        (float)bigdata.value[MAX_EXCU_L + ch_idx]/(1 << 30),
        bigdata.value[TEMP_OVER_L + ch_idx],
        bigdata.value[EXCU_OVER_L + ch_idx],
        ch_idx);
}


void SpeakerProtectionTI::smartpa_get_bigdata() {

    PAL_DBG(LOG_TAG, "Enter:");

    PayloadBuilder* builder = new PayloadBuilder();
    uint8_t* payload = NULL;
    size_t payloadSize = 0;
    std::string backEndName;
    std::shared_ptr<Device> dev = nullptr;
    Stream *stream = NULL;
    Session *session = NULL;
    std::vector<Stream*> activeStreams;
    uint32_t miid = 0, ret;
    tismartpa_exc_temp_stats *pexc_temp_stats[2];
    int status = 0;
    struct mixer_ctl *ctl;
    int pcmid;
    int big_data_param_id[2] = {TI_PARAM_ID_SP_BIG_DATA_L,
            TI_PARAM_ID_SP_BIG_DATA_R};

    PAL_DBG(LOG_TAG, "%d mDeviceAttr.id %d", __LINE__, mDeviceAttr.id);

    rm->getBackendName(mDeviceAttr.id, backEndName);

    dev = Device::getInstance(&mDeviceAttr, rm);

    ret = rm->getActiveStream_l(activeStreams, dev);

    if ((0 != ret) || (activeStreams.size() == 0)) {
        PAL_ERR(LOG_TAG, " no active stream available");
        goto exit;
    }

    stream = static_cast<Stream *>(activeStreams[0]);
    stream->getAssociatedSession(&session);
    ret = session->getMIID(backEndName.c_str(), MODULE_SP, &miid);

#ifdef ENABLE_TAS_SPK_PROT
    ctl = session->getFEMixerCtl("getParam", &pcmid, PAL_AUDIO_OUTPUT);
#else
    ctl = session->getFEMixerCtl("getParam", &pcmid);
#endif

    if (ret) {
        PAL_ERR(LOG_TAG, "Failed to get tag info %x, status = %d", MODULE_SP, ret);
        goto exit;
    }

    PAL_ERR(LOG_TAG, "numberOfChannels %d", numberOfChannels);
    for(int ch_idx = 0; ch_idx < numberOfChannels; ch_idx++)
    {
        payloadSize = 0;
        builder->payloadTISPConfig(&payload, &payloadSize, miid,
                big_data_param_id[ch_idx], NULL);

        if (payload) {
            status = mixer_ctl_set_array(ctl, payload, payloadSize);
            if (0 != status) {
                PAL_ERR(LOG_TAG, "Set failed status = %d for TI_PARAM_ID_SP_BIG_DATA for ch-%d",
                        status, ch_idx);
            }
            status = mixer_ctl_get_array(ctl, payload, payloadSize);
            if (0 != status) {
                PAL_ERR(LOG_TAG, "Get failed status = %d for TI_PARAM_ID_SP_BIG_DATA for ch-%d",
                        status, ch_idx);
            } else {
                pexc_temp_stats[ch_idx] = (tismartpa_exc_temp_stats *)(payload + sizeof(struct apm_module_param_data_t));

                int cur_temperature = pexc_temp_stats[ch_idx]->Temperature_Max;
                int cur_excursion = pexc_temp_stats[ch_idx]->Excursion_Max;

                PAL_INFO(LOG_TAG,
                    "[TI-SmartPA] Get exc_temp_stats MaxTemp %d MaxExc %d(%f)for channel-%d\n",
                    cur_temperature,
                    cur_excursion,
                    (float)cur_excursion/(1 << 30),
                    ch_idx);

                bigdata.value[MAX_TEMP_L + ch_idx] = cur_temperature;
                bigdata.value[KEEP_MAX_TEMP_L + ch_idx] =
                        cur_temperature > bigdata.value[KEEP_MAX_TEMP_L + ch_idx] ? \
                        cur_temperature : bigdata.value[KEEP_MAX_TEMP_L + ch_idx];
                bigdata.value[MAX_EXCU_L + ch_idx] =
                        cur_excursion > bigdata.value[MAX_EXCU_L + ch_idx] ? \
                                cur_excursion : \
                                bigdata.value[MAX_EXCU_L + ch_idx];
                bigdata.value[TEMP_OVER_L + ch_idx] += pexc_temp_stats[ch_idx]->Temprature_over_count;
                bigdata.value[EXCU_OVER_L + ch_idx] += pexc_temp_stats[ch_idx]->Excursion_over_count;

                printBigdata(ch_idx);
            }

            free(payload);
            payload = NULL;
            payloadSize = 0;
        }
    }

exit:
    if(builder) {
        delete builder;
        builder = NULL;
    }
    return;

}

void SpeakerProtectionTI::smartpa_get_temp(void *param, int ch_idx) {
    PayloadBuilder* builder = new PayloadBuilder();
    uint8_t* payload = NULL;
    size_t payloadSize = 0;
    std::string backEndName;
    std::shared_ptr<Device> dev = nullptr;
    Stream *stream = NULL;
    Session *session = NULL;
    std::vector<Stream*> activeStreams;
    uint32_t miid = 0, ret;
    int status;
    int param_id;
    int tempq19, temp;

    int pcmid;
    struct mixer_ctl *ctl;

    param_id = TAS_CALC_PARAM_IDX(TAS_SA_GET_TV, 1, ch_idx+1);

    rm->getBackendName(mDeviceAttr.id, backEndName);
    dev = Device::getInstance(&mDeviceAttr, rm);
    ret = rm->getActiveStream_l(activeStreams, dev);
    if ((0 != ret) || (activeStreams.size() == 0)) {
        PAL_ERR(LOG_TAG, " no active stream available");
        goto exit;
    }
    stream = static_cast<Stream *>(activeStreams[0]);
    stream->getAssociatedSession(&session);
    ret = session->getMIID(backEndName.c_str(), MODULE_SP, &miid);
    if (ret) {
        PAL_ERR(LOG_TAG, "Failed to get tag info %x, status = %d", MODULE_SP, ret);
        goto exit;
    }

#ifdef ENABLE_TAS_SPK_PROT
    ctl = session->getFEMixerCtl("getParam", &pcmid, PAL_AUDIO_OUTPUT);
#else
    ctl = session->getFEMixerCtl("getParam", &pcmid);
#endif
    payloadSize = 0;
    builder->payloadTISPConfig(&payload, &payloadSize, miid,
            param_id, NULL);

    if (payload) {
        status = mixer_ctl_set_array(ctl, payload, payloadSize);
        if (0 != status) {
            PAL_ERR(LOG_TAG, "Set failed status = %d for TAS_SA_GET_TV for ch-%d",
                    status, ch_idx);
        }
        status = mixer_ctl_get_array(ctl, payload, payloadSize);
        if (0 != status) {
            PAL_ERR(LOG_TAG, "Get failed status = %d for TAS_SA_GET_TV for ch-%d",
                    status, ch_idx);
        } else {
            tempq19 = *((int *)(payload + sizeof(struct apm_module_param_data_t)));
            temp = (tempq19 + 0x40000) >> 19;
            PAL_INFO(LOG_TAG,
                "[TI-SmartPA] Get TAS_SA_GET_TV temp %d(%f) for channel-%d\n",
                tempq19,
                (float)tempq19/(1<<19),
                ch_idx);
            memcpy(param, (void*)&temp, sizeof(int));
        }

        free(payload);
        payload = NULL;
        payloadSize = 0;
    }

exit:
    if(builder) {
        delete builder;
        builder = NULL;
    }
    return;
}


int32_t SpeakerProtectionTI::getParameter(uint32_t param_id, void **param)
{
    int i;
    int32_t status = 0;
    switch(param_id) {
        case PAL_PARAM_ID_SPEAKER_AMP_BIGDATA_SAVE_RESET:
        case PAL_PARAM_ID_SPEAKER_AMP_BIGDATA_SAVE:
            PAL_DBG(LOG_TAG, "PAL_PARAM_ID_SPEAKER_AMP_BIGDATA_SAVE");
            for (i = 0; i < numberOfChannels; i++)
                printBigdata(i);
            *param = (void *)&bigdata;
            if (param_id == PAL_PARAM_ID_SPEAKER_AMP_BIGDATA_SAVE_RESET) {
                memset(&bigdata, 0,
                    sizeof(int) * ((NUM_OF_BIGDATA_ITEM - 1) * NUM_OF_SPEAKER));
            }
        break;

        case PAL_PARAM_ID_SPEAKER_AMP_TEMPERATURE_RCV:
        case PAL_PARAM_ID_SPEAKER_AMP_TEMPERATURE_SPK:
        {
            int ch_idx =
                (param_id == PAL_PARAM_ID_SPEAKER_AMP_TEMPERATURE_SPK) ? 0 : 1;
            smartpa_get_temp(&ssrm.temperature, ch_idx);
            *param = (void *)&ssrm.temperature;
             PAL_DBG(LOG_TAG, "TEMPERATURE_RCV return %d", ssrm.temperature);
        }
        break;

        default :
            PAL_ERR(LOG_TAG, "Unsupported operation");
            status = -EINVAL;
        break;
    }
    return status;
}
#endif // ENABLE_TAS_SPK_PROT
