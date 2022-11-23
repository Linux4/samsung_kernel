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

#define LOG_TAG "PAL: Session"

#include "Session.h"
#include "Stream.h"
#include "ResourceManager.h"
#include "SessionGsl.h"
#include "SessionAlsaPcm.h"
#include "SessionAlsaCompress.h"
#include "SessionAgm.h"
#include "SessionAlsaUtils.h"
#include "SessionAlsaVoice.h"

#include <sstream>

#ifdef SEC_AUDIO_OFFLOAD
#define TAG_COMPRESSED_OFFLOAD_PRE_MFC 0x0C00000C
#endif

Session::Session()
{
    isPauseRegistrationDone = false;

}

Session::~Session()
{

}


Session* Session::makeSession(const std::shared_ptr<ResourceManager>& rm, const struct pal_stream_attributes *sAttr)
{
    if (!rm || !sAttr) {
        PAL_ERR(LOG_TAG,"Invalid parameters passed");
        return nullptr;
    }

    Session* s = (Session*) nullptr;

    switch (sAttr->type) {
        //create compressed if the stream type is compressed
        case PAL_STREAM_COMPRESSED:
            s =  new SessionAlsaCompress(rm);
            break;
        case PAL_STREAM_VOICE_CALL:
            s = new SessionAlsaVoice(rm);
            break;
        case PAL_STREAM_NON_TUNNEL:
            s = new SessionAgm(rm);
            break;
        default:
            s = new SessionAlsaPcm(rm);
            break;
    }
    return s;
}

void Session::getSamplerateChannelBitwidthTags(struct pal_media_config *config,
        uint32_t &mfc_sr_tag, uint32_t &ch_tag, uint32_t &bitwidth_tag)
{
    switch (config->sample_rate) {
        case SAMPLINGRATE_8K :
            mfc_sr_tag = MFC_SR_8K;
            break;
        case SAMPLINGRATE_16K :
            mfc_sr_tag = MFC_SR_16K;
            break;
        case SAMPLINGRATE_32K :
            mfc_sr_tag = MFC_SR_32K;
            break;
        case SAMPLINGRATE_44K :
            mfc_sr_tag = MFC_SR_44K;
            break;
        case SAMPLINGRATE_48K :
            mfc_sr_tag = MFC_SR_48K;
            break;
        case SAMPLINGRATE_96K :
            mfc_sr_tag = MFC_SR_96K;
            break;
        case SAMPLINGRATE_192K :
            mfc_sr_tag = MFC_SR_192K;
            break;
        case SAMPLINGRATE_384K :
            mfc_sr_tag = MFC_SR_384K;
            break;
        default:
            mfc_sr_tag = MFC_SR_48K;
            break;
    }
    switch (config->ch_info.channels) {
        case CHANNELS_1:
            ch_tag = CHS_1;
            break;
        case CHANNELS_2:
            ch_tag = CHS_2;
            break;
        case CHANNELS_3:
            ch_tag = CHS_3;
            break;
        case CHANNELS_4:
            ch_tag = CHS_4;
            break;
        case CHANNELS_5:
            ch_tag = CHS_5;
            break;
        case CHANNELS_6:
            ch_tag = CHS_6;
            break;
        case CHANNELS_7:
            ch_tag = CHS_7;
            break;
        case CHANNELS_8:
            ch_tag = CHS_8;
            break;
        default:
            ch_tag = CHS_1;
            break;
    }
    switch (config->bit_width) {
        case BITWIDTH_16:
            bitwidth_tag = BW_16;
            break;
        case BITWIDTH_24:
            bitwidth_tag = BW_24;
            break;
        case BITWIDTH_32:
            bitwidth_tag = BW_32;
            break;
        default:
            bitwidth_tag = BW_16;
            break;
    }
}

int Session::getEffectParameters(Stream *s __unused, effect_pal_payload_t *effectPayload)
{
    int status = 0;
    uint8_t *ptr = NULL;

    uint8_t *payloadData = NULL;
    size_t payloadSize = 0;
    int device = 0;
    uint32_t miid = 0;
    const char *control = "getParam";
    struct mixer_ctl *ctl;
    pal_effect_custom_payload_t *effectCustomPayload = nullptr;
    std::ostringstream CntrlName;
    PayloadBuilder builder;

    PAL_DBG(LOG_TAG, "Enter.");
    ctl = getFEMixerCtl(control, &device);
    if (!ctl) {
        PAL_ERR(LOG_TAG, "Invalid mixer control: %s\n", CntrlName.str().data());
        status = -ENOENT;
        goto exit;
    }

    if (!rxAifBackEnds.empty()) { /** search in RX GKV */
        status = SessionAlsaUtils::getModuleInstanceId(mixer, device, rxAifBackEnds[0].second.data(),
                effectPayload->tag, &miid);
        if (status) /** if not found, reset miid to 0 again */
            miid = 0;
    }

    if (!txAifBackEnds.empty()) { /** search in TX GKV */
        status = SessionAlsaUtils::getModuleInstanceId(mixer, device, txAifBackEnds[0].second.data(),
                effectPayload->tag, &miid);
        if (status)
            miid = 0;
    }

    if (miid == 0) {
        PAL_ERR(LOG_TAG, "failed to look for module with tagID 0x%x",
                    effectPayload->tag);
        status = -EINVAL;
        goto exit;
    }

    effectCustomPayload = (pal_effect_custom_payload_t *)(effectPayload->payload);
    if (effectPayload->payloadSize < sizeof(pal_effect_custom_payload_t)) {
        status = -EINVAL;
        PAL_ERR(LOG_TAG, "memory for retrieved data is too small");
        goto exit;
    }

    builder.payloadQuery(&payloadData, &payloadSize,
                            miid, effectCustomPayload->paramId,
                            effectPayload->payloadSize - sizeof(uint32_t));
    status = mixer_ctl_set_array(ctl, payloadData, payloadSize);
    if (0 != status) {
        PAL_ERR(LOG_TAG, "Set custom config failed, status = %d", status);
        goto exit;
    }

    status = mixer_ctl_get_array(ctl, payloadData, payloadSize);
    if (0 != status) {
        PAL_ERR(LOG_TAG, "Get custom config failed, status = %d", status);
        goto exit;
    }

    ptr = (uint8_t *)payloadData + sizeof(struct apm_module_param_data_t);
    ar_mem_cpy(effectCustomPayload->data, effectPayload->payloadSize,
                        ptr, effectPayload->payloadSize);

exit:
    if (payloadData)
        free(payloadData);
    PAL_ERR(LOG_TAG, "Exit. status %d", status);
    return status;
}

int Session::rwACDBParameters(void *payload, uint32_t sampleRate,
                                bool isParamWrite)
{
    int status = 0;
    uint8_t *payloadData = NULL;
    size_t payloadSize = 0;
    int device = 0;
    uint32_t miid = 0;
    char const *control = "setParamTagACDB";
    struct mixer_ctl *ctl;
    pal_effect_custom_payload_t *effectCustomPayload = nullptr;
    std::ostringstream CntrlName;
    PayloadBuilder builder;
    pal_param_payload *paramPayload = nullptr;
    agm_acdb_param *effectACDBPayload = nullptr;
    paramPayload = (pal_param_payload *)payload;
    if (!paramPayload)
        return -EINVAL;

    effectACDBPayload = (agm_acdb_param *)(paramPayload->payload);
    if (!effectACDBPayload)
        return -EINVAL;

    PAL_DBG(LOG_TAG, "Enter.");
    ctl = getFEMixerCtl(control, &device);
    if (!ctl) {
        PAL_ERR(LOG_TAG, "Invalid mixer control: %s\n", CntrlName.str().data());
        status = -ENOENT;
        goto exit;
    }

    if (!rxAifBackEnds.empty()) { /** search in RX GKV */
        status = SessionAlsaUtils::getModuleInstanceId(mixer, device,
                rxAifBackEnds[0].second.data(),
                effectACDBPayload->tag, &miid);
        if (status) /** if not found, reset miid to 0 again */
            miid = 0;
    }

    if (!txAifBackEnds.empty()) { /** search in TX GKV */
        status = SessionAlsaUtils::getModuleInstanceId(mixer, device, txAifBackEnds[0].second.data(),
                effectACDBPayload->tag, &miid);
        if (status)
            miid = 0;
    }

    if (miid == 0) {
        PAL_ERR(LOG_TAG, "failed to look for module with tagID 0x%x",
                    effectACDBPayload->tag);
        status = -EINVAL;
        goto exit;
    }

    effectCustomPayload =
        (pal_effect_custom_payload_t *)(effectACDBPayload->blob);

    status = builder.payloadACDBParam(&payloadData, &payloadSize,
                            (uint8_t *)effectACDBPayload,
                            miid, sampleRate);
    if (!payloadData) {
        PAL_ERR(LOG_TAG, "failed to create payload data.");
        goto exit;
    }

   if (isParamWrite) {
        status = mixer_ctl_set_array(ctl, payloadData, payloadSize);
        if (0 != status) {
            PAL_ERR(LOG_TAG, "Set custom config failed, status = %d", status);
            goto exit;
        }
    }

exit:
    free(payloadData);
    PAL_ERR(LOG_TAG, "Exit. status %d", status);
    return status;
}

int Session::updateCustomPayload(void *payload, size_t size)
{
    if (!customPayloadSize) {
        customPayload = calloc(1, size);
    } else {
        customPayload = realloc(customPayload, customPayloadSize + size);
    }

    if (!customPayload) {
        PAL_ERR(LOG_TAG, "failed to allocate memory for custom payload");
        return -ENOMEM;
    }

    memcpy((uint8_t *)customPayload + customPayloadSize, payload, size);
    customPayloadSize += size;
    PAL_INFO(LOG_TAG, "customPayloadSize = %zu", customPayloadSize);
    return 0;
}

int Session::freeCustomPayload(uint8_t **payload, size_t *payloadSize) {
    if (*payload) {
        free(*payload);
        *payload = NULL;
        *payloadSize = 0;
    }
    return 0;
}

int Session::freeCustomPayload() {
    if (customPayload) {
        free(customPayload);
        customPayload = NULL;
        customPayloadSize = 0;
    }
    return 0;
}

int Session::pause(Stream * s __unused)
{
    return 0;
}

int Session::resume(Stream * s __unused)
{
     return 0;
}

int Session::handleDeviceRotation(Stream *s, pal_speaker_rotation_type rotation_type,
        int device, struct mixer *mixer, PayloadBuilder* builder,
        std::vector<std::pair<int32_t, std::string>> rxAifBackEnds)
{
    int status = 0;
    struct pal_stream_attributes sAttr;
    struct pal_device dAttr;
    struct sessionToPayloadParam deviceData;
    uint32_t miid = 0;
    uint8_t* alsaParamData = NULL;
    size_t alsaPayloadSize = 0;
    std::vector<std::shared_ptr<Device>> associatedDevices;
    status = s->getStreamAttributes(&sAttr);
    if (status != 0) {
        PAL_ERR(LOG_TAG,"stream get attributes failed");
        return status;
    }

    if (PAL_AUDIO_OUTPUT== sAttr.direction) {
        status = s->getAssociatedDevices(associatedDevices);
        if (0 != status) {
            PAL_ERR(LOG_TAG,"getAssociatedDevices Failed\n");
            return status;
        }

        for (int i = 0; i < associatedDevices.size(); i++) {
             status = associatedDevices[i]->getDeviceAttributes(&dAttr);
             if (0 != status) {
                 PAL_ERR(LOG_TAG,"get Device Attributes Failed\n");
                 return status;
             }

             if ((PAL_DEVICE_OUT_SPEAKER == dAttr.id) &&
                  (2 == dAttr.config.ch_info.channels)) {
                 /* Get DevicePP MFC MIID and configure to match to device config */
                 /* This has to be done after sending all mixer controls and
                  * before connect
                  */
                status =
                        SessionAlsaUtils::getModuleInstanceId(mixer,
                                                              device,
                                                              rxAifBackEnds[i].second.data(),
                                                              TAG_MFC_SPEAKER_SWAP,
                                                              &miid);
                if (status != 0) {
                    PAL_ERR(LOG_TAG,"getModuleInstanceId failed");
                    return status;
                }
                PAL_DBG(LOG_TAG, "miid : %x id = %d, data %s, dev id = %d\n", miid,
                    device, rxAifBackEnds[i].second.data(), dAttr.id);

                if (rm->activeGroupDevConfig) {
                    if (rm->activeGroupDevConfig->devpp_mfc_cfg.sample_rate)
                        dAttr.config.sample_rate = rm->activeGroupDevConfig->devpp_mfc_cfg.sample_rate;
                    if (rm->activeGroupDevConfig->devpp_mfc_cfg.channels)
                        dAttr.config.ch_info.channels = rm->activeGroupDevConfig->devpp_mfc_cfg.channels;
                }
                deviceData.bitWidth = dAttr.config.bit_width;
                deviceData.sampleRate = dAttr.config.sample_rate;
                deviceData.numChannel = dAttr.config.ch_info.channels;
                deviceData.rotation_type = rotation_type;
                deviceData.ch_info = nullptr;
                builder->payloadMFCConfig((uint8_t **)&alsaParamData,
                                           &alsaPayloadSize, miid, &deviceData);

                if (alsaPayloadSize) {
                    status = updateCustomPayload(alsaParamData, alsaPayloadSize);
#ifdef SEC_AUDIO_EARLYDROP_PATCH
                    free(alsaParamData);
#else
                    delete alsaParamData;
#endif
                    if (0 != status) {
                        PAL_ERR(LOG_TAG,"updateCustomPayload Failed\n");
                        return status;
                    }
                }
                status = SessionAlsaUtils::setMixerParameter(mixer,
                                                             device,
                                                             customPayload,
                                                             customPayloadSize);
                if (status != 0) {
                    PAL_ERR(LOG_TAG,"setMixerParameter failed");
                    return status;
                }
            }
        }
    }
    return status;
}

/* This set slot mask tag for device with virtual port enabled */
int Session::setSlotMask(const std::shared_ptr<ResourceManager>& rm, struct pal_stream_attributes &sAttr,
            struct pal_device &dAttr, const std::vector<int> &pcmDevIds)
{
    int status = 0;
    std::vector <std::pair<int, int>> tkv;
    struct agm_tag_config* tagConfig = NULL;
    const char *setParamTagControl = " setParamTag";
    const char *streamPcm = "PCM";
    const char *streamComp = "COMPRESS";
    const char *streamVoice = "VOICEMMODE";
    const char *feCtl = " control";
    struct mixer_ctl *ctl;
    std::ostringstream tagCntrlName;
    std::ostringstream feName;
    std::string backendname;
    int tkv_size = 0;

    tkv.push_back(std::make_pair(TAG_KEY_SLOT_MASK,rm->activeGroupDevConfig->grp_dev_hwep_cfg.slot_mask));
    tagConfig = (struct agm_tag_config*)malloc(sizeof(struct agm_tag_config) +
                    (tkv.size() * sizeof(agm_key_value)));

    if (!tagConfig) {
        status = -EINVAL;
        goto exit;
    }

    status = SessionAlsaUtils::getTagMetadata(TAG_DEVICE_MUX, tkv, tagConfig);
    if (0 != status) {
        goto exit;
    }

    if (PAL_STREAM_COMPRESSED == sAttr.type) {
        tagCntrlName<<streamComp<<pcmDevIds.at(0)<<setParamTagControl;
        feName<<streamComp<<pcmDevIds.at(0)<<feCtl;
    } else if (PAL_STREAM_VOICE_CALL == sAttr.type) {
        if (sAttr.info.voice_call_info.VSID == VOICEMMODE1 ||
            sAttr.info.voice_call_info.VSID == VOICELBMMODE1){
            tagCntrlName<<streamVoice<<1<<"p"<<setParamTagControl;
            feName<<streamVoice<<1<<"p"<<feCtl;
        } else {
            tagCntrlName<<streamVoice<<2<<"p"<<setParamTagControl;
            feName<<streamVoice<<2<<"p"<<feCtl;
        }
    } else {
        tagCntrlName<<streamPcm<<pcmDevIds.at(0)<<setParamTagControl;
        feName << streamPcm<<pcmDevIds.at(0)<<feCtl;
    }

    // set FE ctl to BE first in case this is called from connectionSessionDevice
    rm->getBackendName(dAttr.id, backendname);
    ctl = mixer_get_ctl_by_name(mixer, feName.str().data());
    if (!ctl) {
        PAL_ERR(LOG_TAG, "Invalid mixer control: %s\n", feName.str().data());
        status = -EINVAL;
        goto exit;
    }
    mixer_ctl_set_enum_by_string(ctl, backendname.c_str());
    ctl = NULL;

    // set tag data
    ctl = mixer_get_ctl_by_name(mixer, tagCntrlName.str().data());
    if (!ctl) {
        PAL_ERR(LOG_TAG, "Invalid mixer control: %s\n", tagCntrlName.str().data());
        status = -EINVAL;
        goto exit;
    }
    tkv_size = tkv.size()*sizeof(struct agm_key_value);
    status = mixer_ctl_set_array(ctl, tagConfig, sizeof(struct agm_tag_config) + tkv_size);
    if (status != 0) {
        PAL_ERR(LOG_TAG,"failed to set the tag calibration %d", status);
    }

exit:
    if (tagConfig)
        free(tagConfig);
    return status;
}

/* This is to set devicePP MFC(if exists) and PSPD MFC */
int Session::configureMFC(const std::shared_ptr<ResourceManager>& rm, struct pal_stream_attributes &sAttr,
            struct pal_device &dAttr, const std::vector<int> &pcmDevIds, const char* intf)
{
    int status = 0;
    std::shared_ptr<Device> dev = nullptr;
    uint8_t* payload = NULL;
    size_t payloadSize = 0;
    struct pal_media_config codecConfig;
    struct sessionToPayloadParam mfcData;
    PayloadBuilder* builder = new PayloadBuilder();
    uint32_t miid = 0;
    bool devicePPMFCSet =  true;

    // clear any cached custom payload
    freeCustomPayload();

    /* Prepare devicePP MFC payload */
    /* Try to set devicePP MFC for virtual port enabled device to match to DMA config */
    if (rm->activeGroupDevConfig &&
            (dAttr.id == PAL_DEVICE_OUT_SPEAKER ||
             dAttr.id == PAL_DEVICE_OUT_HANDSET ||
             dAttr.id == PAL_DEVICE_OUT_ULTRASOUND)) {
        status = SessionAlsaUtils::getModuleInstanceId(mixer, pcmDevIds.at(0), intf,
                                                       TAG_DEVICE_PP_MFC, &miid);
        if (status == 0) {
            PAL_DBG(LOG_TAG, "miid : %x id = %d, data %s, dev id = %d\n", miid,
                    pcmDevIds.at(0), intf, dAttr.id);

            if (rm->activeGroupDevConfig->devpp_mfc_cfg.bit_width)
                mfcData.bitWidth = rm->activeGroupDevConfig->devpp_mfc_cfg.bit_width;
            else
                mfcData.bitWidth = dAttr.config.bit_width;
            if (rm->activeGroupDevConfig->devpp_mfc_cfg.sample_rate)
                mfcData.sampleRate = rm->activeGroupDevConfig->devpp_mfc_cfg.sample_rate;
            else
                mfcData.sampleRate = dAttr.config.sample_rate;
            if (rm->activeGroupDevConfig->devpp_mfc_cfg.channels)
                mfcData.numChannel = rm->activeGroupDevConfig->devpp_mfc_cfg.channels;
            else
                mfcData.numChannel = dAttr.config.ch_info.channels;
            mfcData.rotation_type = PAL_SPEAKER_ROTATION_LR;
            mfcData.ch_info = nullptr;

            builder->payloadMFCConfig((uint8_t**)&payload, &payloadSize, miid, &mfcData);
            if (!payloadSize) {
                PAL_ERR(LOG_TAG, "payloadMFCConfig failed\n");
                status = -EINVAL;
                goto exit;
            }
            status = updateCustomPayload(payload, payloadSize);
#ifdef SEC_AUDIO_EARLYDROP_PATCH
            free(payload);
#else
            delete payload;
#endif
            if (0 != status) {
                PAL_ERR(LOG_TAG,"updateCustomPayload Failed\n");
                goto exit;
            }
        } else {
            PAL_INFO(LOG_TAG,"deviePP MFC doesn't exist for stream %d \n", sAttr.type);
            devicePPMFCSet = false;
        }

        /* set TKV for slot mask */
        setSlotMask(rm, sAttr, dAttr, pcmDevIds);
    }

    /* Prepare PSPD MFC payload */
    /* Get PSPD MFC MIID and configure to match to device config */
    /* This has to be done after sending all mixer controls and before connect */
    status = SessionAlsaUtils::getModuleInstanceId(mixer, pcmDevIds.at(0), intf,
                                                   TAG_DEVICE_MFC_SR, &miid);
    if (status == 0) {
        PAL_DBG(LOG_TAG, "miid : %x id = %d, data %s, dev id = %d\n", miid,
                pcmDevIds.at(0), intf, dAttr.id);

        if (dAttr.id == PAL_DEVICE_OUT_BLUETOOTH_A2DP ||
                dAttr.id == PAL_DEVICE_OUT_BLUETOOTH_SCO) {
            dev = Device::getInstance((struct pal_device *)&dAttr , rm);
            if (!dev) {
                PAL_ERR(LOG_TAG, "Device getInstance failed");
                status = -EINVAL;
                goto exit;
            }
            status = dev->getCodecConfig(&codecConfig);
            if(0 != status) {
                PAL_ERR(LOG_TAG,"getCodecConfig Failed \n");
                goto exit;
            }
            mfcData.bitWidth = codecConfig.bit_width;
            mfcData.sampleRate = codecConfig.sample_rate;
            mfcData.numChannel = codecConfig.ch_info.channels;
            mfcData.rotation_type = PAL_SPEAKER_ROTATION_LR;
            mfcData.ch_info = nullptr;
        } else {
            mfcData.bitWidth = dAttr.config.bit_width;
            if (!devicePPMFCSet && rm->activeGroupDevConfig->devpp_mfc_cfg.sample_rate)
                mfcData.sampleRate = rm->activeGroupDevConfig->devpp_mfc_cfg.sample_rate;
            else
                mfcData.sampleRate = dAttr.config.sample_rate;
            if (!devicePPMFCSet && rm->activeGroupDevConfig->devpp_mfc_cfg.channels)
                mfcData.numChannel = rm->activeGroupDevConfig->devpp_mfc_cfg.channels;
            else
                mfcData.numChannel = dAttr.config.ch_info.channels;
            mfcData.rotation_type = PAL_SPEAKER_ROTATION_LR;
            mfcData.ch_info = nullptr;
        }

        if ((PAL_DEVICE_OUT_SPEAKER == dAttr.id) &&
            (2 == dAttr.config.ch_info.channels)) {
            // Stereo Speakers. Check for the rotation type
            if (PAL_SPEAKER_ROTATION_RL == rm->getCurrentRotationType()) {
                // Rotation is of RL, so need to swap the channels
                mfcData.rotation_type = PAL_SPEAKER_ROTATION_RL;
            }
        }
        if (dAttr.id == PAL_DEVICE_OUT_AUX_DIGITAL ||
            dAttr.id == PAL_DEVICE_OUT_AUX_DIGITAL_1 ||
            dAttr.id == PAL_DEVICE_OUT_HDMI)
            mfcData.ch_info = &dAttr.config.ch_info;

        builder->payloadMFCConfig((uint8_t **)&payload, &payloadSize, miid, &mfcData);
        if (!payloadSize) {
            PAL_ERR(LOG_TAG, "payloadMFCConfig failed\n");
            status = -EINVAL;
            goto exit;
        }

        status = updateCustomPayload(payload, payloadSize);
#ifdef SEC_AUDIO_EARLYDROP_PATCH
        free(payload);
#else
        delete payload;
#endif
        if (0 != status) {
            PAL_ERR(LOG_TAG,"updateCustomPayload Failed\n");
            goto exit;
        }
    } else {
        PAL_ERR(LOG_TAG,"getModuleInstanceId failed");
        if (sAttr.direction == (PAL_AUDIO_INPUT | PAL_AUDIO_OUTPUT))
            status = 0;
    }

#ifdef SEC_AUDIO_OFFLOAD
    /* update TAG_COMPRESSED_OFFLOAD_PRE_MFC for samsung adsp solution */
    if ((sAttr.type == PAL_STREAM_COMPRESSED) &&
        (dAttr.id == PAL_DEVICE_OUT_SPEAKER ||
            dAttr.id == PAL_DEVICE_OUT_USB_HEADSET ||
            dAttr.id == PAL_DEVICE_OUT_BLUETOOTH_A2DP)) {

        struct sessionToPayloadParam preMfcData;
        status = SessionAlsaUtils::getModuleInstanceId(mixer, pcmDevIds.at(0), intf,
                                                    TAG_COMPRESSED_OFFLOAD_PRE_MFC, &miid);
        if (status == 0) {
            PAL_INFO(LOG_TAG, "for OFFLOAD_PRE_MFC miid : %x id = %d, data %s, dev id = %d\n",
                    miid, pcmDevIds.at(0), intf, dAttr.id);

            if (dAttr.id == PAL_DEVICE_OUT_BLUETOOTH_A2DP) {
                preMfcData = mfcData;
            } else {
                preMfcData.bitWidth = BIT_WIDTH_24;
                preMfcData.sampleRate = DEFAULT_SAMPLING_RATE;
                preMfcData.numChannel = DEFAULT_CHANNEL_COUNT;
                preMfcData.rotation_type = PAL_SPEAKER_ROTATION_LR;
                preMfcData.ch_info = nullptr;
            }

            builder->payloadMFCConfig((uint8_t **)&payload, &payloadSize, miid, &preMfcData);
            if (!payloadSize) {
                PAL_ERR(LOG_TAG, "payloadMFCConfig failed\n");
                status = -EINVAL;
                goto exit;
            }

            status = updateCustomPayload(payload, payloadSize);
#ifdef SEC_AUDIO_EARLYDROP_PATCH
            free(payload);
#else
            delete payload;
#endif
            if (0 != status) {
                PAL_ERR(LOG_TAG,"updateCustomPayload Failed\n");
                goto exit;
            }
        } else {
            PAL_ERR(LOG_TAG,"getModuleInstanceId failed");
            if (sAttr.direction == (PAL_AUDIO_INPUT | PAL_AUDIO_OUTPUT))
                status = 0;
        }
    }
#endif

    // set devicePP MFC(if exists) and PSPD MFC configuration
    if (customPayload) {
        status = SessionAlsaUtils::setMixerParameter(mixer, pcmDevIds.at(0),
                                                 customPayload, customPayloadSize);
        freeCustomPayload();
        if (status != 0) {
            PAL_ERR(LOG_TAG,"setMixerParameter failed");
            goto exit;
        }
    }

exit:
    if (builder) {
        delete builder;
        builder = NULL;
    }
    return status;
}
#if 0
int setConfig(Stream * s, pal_stream_type_t sType, configType type, uint32_t tag1,
        uint32_t tag2, uint32_t tag3)
{
    int status = 0;
    uint32_t tagsent = 0;
    struct agm_tag_config* tagConfig = nullptr;
    std::ostringstream tagCntrlName;
    char const *stream = "PCM";
    const char *setParamTagControl = "setParamTag";
    struct mixer_ctl *ctl = nullptr;
    uint32_t tkv_size = 0;

    if (sType == PAL_STREAM_COMPRESSED)
        stream = "COMPRESS";

    switch (type) {
        case MODULE:
            tkv.clear();
            if (tag1)
                builder->populateTagKeyVector(s, tkv, tag1, &tagsent);
            if (tag2)
                builder->populateTagKeyVector(s, tkv, tag2, &tagsent);
            if (tag3)
                builder->populateTagKeyVector(s, tkv, tag3, &tagsent);

            if (tkv.size() == 0) {
                status = -EINVAL;
                goto exit;
            }
            tagConfig = (struct agm_tag_config*)malloc (sizeof(struct agm_tag_config) +
                            (tkv.size() * sizeof(agm_key_value)));
            if(!tagConfig) {
                status = -ENOMEM;
                goto exit;
            }
            status = SessionAlsaUtils::getTagMetadata(tagsent, tkv, tagConfig);
            if (0 != status) {
                goto exit;
            }
            tagCntrlName << stream << pcmDevIds.at(0) << " " << setParamTagControl;
            ctl = mixer_get_ctl_by_name(mixer, tagCntrlName.str().data());
            if (!ctl) {
                PAL_ERR(LOG_TAG, "Invalid mixer control: %s\n", tagCntrlName.str().data());
                return -ENOENT;
            }

            tkv_size = tkv.size() * sizeof(struct agm_key_value);
            status = mixer_ctl_set_array(ctl, tagConfig, sizeof(struct agm_tag_config) + tkv_size);
            if (status != 0) {
                PAL_ERR(LOG_TAG,"failed to set the tag calibration %d", status);
                goto exit;
            }
            ctl = NULL;
            tkv.clear();
            break;
        default:
            status = 0;
            break;
    }

exit:
    return status;
}
#endif

