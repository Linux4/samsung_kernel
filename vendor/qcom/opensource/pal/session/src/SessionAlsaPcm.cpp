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

#define LOG_TAG "PAL: SessionAlsaPcm"

#include "SessionAlsaPcm.h"
#include "SessionAlsaUtils.h"
#include "Stream.h"
#include "ResourceManager.h"
#include "detection_cmn_api.h"
#include "acd_api.h"
#include <agm/agm_api.h>
#include <asps/asps_acm_api.h>
#include <sstream>
#include <string>
#include "detection_cmn_api.h"
#include "audio_dam_buffer_api.h"
#include "apm_api.h"
#include "us_detect_api.h"

#ifdef SEC_AUDIO_COMMON
#include "SecPalDefs.h"
#endif

#define SESSION_ALSA_MMAP_DEFAULT_OUTPUT_SAMPLING_RATE (48000)
#define SESSION_ALSA_MMAP_PERIOD_SIZE (SESSION_ALSA_MMAP_DEFAULT_OUTPUT_SAMPLING_RATE/1000)
#define SESSION_ALSA_MMAP_PERIOD_COUNT_MIN 64
#define SESSION_ALSA_MMAP_PERIOD_COUNT_MAX 2048
#define SESSION_ALSA_MMAP_PERIOD_COUNT_DEFAULT (SESSION_ALSA_MMAP_PERIOD_COUNT_MAX)

SessionAlsaPcm::SessionAlsaPcm(std::shared_ptr<ResourceManager> Rm)
{
   rm = Rm;
   builder = new PayloadBuilder();
   customPayload = NULL;
   customPayloadSize = 0;
   eventPayload = NULL;
   eventPayloadSize = 0;
   pcm = NULL;
   pcmRx = NULL;
   pcmTx = NULL;
   pcmEcTx = NULL;
   mState = SESSION_IDLE;
   ecRefDevId = PAL_DEVICE_OUT_MIN;
   streamHandle = NULL;
}

SessionAlsaPcm::~SessionAlsaPcm()
{
   delete builder;

}


int SessionAlsaPcm::prepare(Stream * s __unused)
{
   return 0;
}

int SessionAlsaPcm::open(Stream * s)
{
    int status = 0;
    struct pal_stream_attributes sAttr;
    std::vector<std::shared_ptr<Device>> associatedDevices;
    int ldir = 0;
    std::vector<int> pcmId;

    PAL_DBG(LOG_TAG,"Enter");
    status = s->getStreamAttributes(&sAttr);
    streamHandle = s;
    if (0 != status) {
        PAL_ERR(LOG_TAG,"getStreamAttributes Failed \n");
        goto exit;
    }
    if (sAttr.type != PAL_STREAM_VOICE_CALL_RECORD &&
        sAttr.type != PAL_STREAM_VOICE_CALL_MUSIC  &&
        sAttr.type != PAL_STREAM_CONTEXT_PROXY) {
        status = s->getAssociatedDevices(associatedDevices);
        if (0 != status) {
            PAL_ERR(LOG_TAG,"getAssociatedDevices Failed \n");
            goto exit;
        }

        rm->getBackEndNames(associatedDevices, rxAifBackEnds, txAifBackEnds);
        if (rxAifBackEnds.empty() && txAifBackEnds.empty()) {
            status = -EINVAL;
            PAL_ERR(LOG_TAG, "no backend specified for this stream");
            goto exit;

        }
    }
    status = rm->getVirtualAudioMixer(&mixer);
    if (status) {
        PAL_ERR(LOG_TAG,"mixer error");
        goto exit;
    }
    if (sAttr.direction == PAL_AUDIO_INPUT) {
        if (sAttr.type == PAL_STREAM_ACD ||
            sAttr.type == PAL_STREAM_SENSOR_PCM_DATA)
            ldir = TX_HOSTLESS;

        pcmDevIds = rm->allocateFrontEndIds(sAttr, ldir);
        if (pcmDevIds.size() == 0) {
            PAL_ERR(LOG_TAG, "allocateFrontEndIds failed");
            status = -EINVAL;
            goto exit;
        }
    } else if (sAttr.direction == PAL_AUDIO_OUTPUT) {
        pcmDevIds = rm->allocateFrontEndIds(sAttr, 0);
        if (pcmDevIds.size() == 0) {
            PAL_ERR(LOG_TAG, "allocateFrontEndIds failed");
            status = -EINVAL;
            goto exit;
        }
    } else {
        pcmDevRxIds = rm->allocateFrontEndIds(sAttr, RX_HOSTLESS);
        pcmDevTxIds = rm->allocateFrontEndIds(sAttr, TX_HOSTLESS);
        if (!pcmDevRxIds.size() || !pcmDevTxIds.size()) {
            PAL_ERR(LOG_TAG, "allocateFrontEndIds failed");
            status = -EINVAL;
            goto exit;
        }
    }
    switch (sAttr.direction) {
        case PAL_AUDIO_INPUT:
            status = SessionAlsaUtils::open(s, rm, pcmDevIds, txAifBackEnds);
            if (status) {
                PAL_ERR(LOG_TAG, "session alsa open failed with %d", status);
                rm->freeFrontEndIds(pcmDevIds, sAttr, ldir);
            }
            break;
        case PAL_AUDIO_OUTPUT:
            status = SessionAlsaUtils::open(s, rm, pcmDevIds, rxAifBackEnds);
            if (status) {
                PAL_ERR(LOG_TAG, "session alsa open failed with %d", status);
                rm->freeFrontEndIds(pcmDevIds, sAttr, 0);
            }
            else if ((sAttr.type == PAL_STREAM_PCM_OFFLOAD) ||
                     (sAttr.type == PAL_STREAM_DEEP_BUFFER) ||
                     (sAttr.type == PAL_STREAM_LOW_LATENCY)) {
                     // Register for SoftPause callback for
                     // only playback related streams
                     status = rm->registerMixerEventCallback(pcmDevIds,
                         sessionCb, cbCookie, true);
                     if (status != 0) {
                         PAL_ERR(LOG_TAG, "Failed to register callback to rm");
                         // If registration fails for this then pop noise
                         // issue will come. It isn't fatal so not throwing error.
                         status = 0;
                         isPauseRegistrationDone = false;
                     }
                     else
                         isPauseRegistrationDone = true;
            }
            break;
        case PAL_AUDIO_INPUT | PAL_AUDIO_OUTPUT:
            status = SessionAlsaUtils::open(s, rm, pcmDevRxIds, pcmDevTxIds,
                    rxAifBackEnds, txAifBackEnds);
            if (status) {
                PAL_ERR(LOG_TAG, "session alsa open failed with %d", status);
                rm->freeFrontEndIds(pcmDevRxIds, sAttr, RX_HOSTLESS);
                rm->freeFrontEndIds(pcmDevTxIds, sAttr, TX_HOSTLESS);
            }
            break;
        default:
            PAL_ERR(LOG_TAG,"unsupported direction");
            break;
    }

    if (status)
        goto exit;

    if (sAttr.type == PAL_STREAM_VOICE_UI ||
        sAttr.type == PAL_STREAM_ACD ||
        sAttr.type == PAL_STREAM_CONTEXT_PROXY ||
        sAttr.type == PAL_STREAM_ULTRASOUND) {
        switch (sAttr.type) {
            case PAL_STREAM_VOICE_UI:
            case PAL_STREAM_CONTEXT_PROXY:
            case PAL_STREAM_ACD:
                pcmId = pcmDevIds;
                break;
            case PAL_STREAM_ULTRASOUND:
                pcmId = pcmDevTxIds;
                break;
            default:
                break;
        }
        status = rm->registerMixerEventCallback(pcmId,
                             sessionCb, cbCookie, true);
        if (status != 0) {
            PAL_ERR(LOG_TAG, "Failed to register callback to rm");
        }
    }
exit:
    PAL_DBG(LOG_TAG,"Exit status: %d", status);
    return status;
}

int SessionAlsaPcm::setConfig(Stream * s, configType type, uint32_t tag1,
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
    PAL_DBG(LOG_TAG,"Enter tags: %d %d %d", tag1, tag2, tag3);
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
            tagConfig = (struct agm_tag_config*)malloc(sizeof(struct agm_tag_config) +
                            (tkv.size() * sizeof(agm_key_value)));
            if (!tagConfig) {
                status = -ENOMEM;
                goto exit;
            }
            status = SessionAlsaUtils::getTagMetadata(tagsent, tkv, tagConfig);
            if (0 != status) {
#ifdef SEC_AUDIO_EARLYDROP_PATCH
                if (tagConfig)
                    free(tagConfig);
#endif
                goto exit;
            }
            tagCntrlName << stream << pcmDevIds.at(0) << " " << setParamTagControl;
            ctl = mixer_get_ctl_by_name(mixer, tagCntrlName.str().data());
            if (!ctl) {
                PAL_ERR(LOG_TAG, "Invalid mixer control: %s\n", tagCntrlName.str().data());
#ifdef SEC_AUDIO_EARLYDROP_PATCH
                if (tagConfig)
                    free(tagConfig);
#endif
                return -ENOENT;
            }

            tkv_size = tkv.size() * sizeof(struct agm_key_value);
            status = mixer_ctl_set_array(ctl, tagConfig, sizeof(struct agm_tag_config) + tkv_size);
            if (status != 0) {
#ifdef SEC_AUDIO_EARLYDROP_PATCH
                if (tagConfig)
                    free(tagConfig);
#endif
                PAL_ERR(LOG_TAG,"failed to set the tag calibration %d", status);
                goto exit;
            }
            ctl = NULL;
            if (tagConfig)
                free(tagConfig);
            tkv.clear();
            break;
        default:
            status = 0;
            break;
    }

exit:
    PAL_DBG(LOG_TAG,"exit status: %d ", status);
    return status;
}

struct mixer_ctl* SessionAlsaPcm::getFEMixerCtl(const char *controlName, int *device)
{
    *device = pcmDevIds.at(0);
    std::ostringstream CntrlName;
    struct mixer_ctl *ctl;

    CntrlName << "PCM" <<pcmDevIds.at(0) << " " << controlName;
    ctl = mixer_get_ctl_by_name(mixer, CntrlName.str().data());
    if (!ctl) {
        PAL_ERR(LOG_TAG, "Invalid mixer control: %s\n", CntrlName.str().data());
        return nullptr;
    }

    return ctl;
}

uint32_t SessionAlsaPcm::getMIID(const char *backendName, uint32_t tagId, uint32_t *miid)
{
    int status = 0;
    int device = 0;
    struct pal_stream_attributes sAttr;

    if (!streamHandle) {
        PAL_ERR(LOG_TAG, "Session handle not found");
        status = -EINVAL;
        goto exit;
    }
    status = streamHandle->getStreamAttributes(&sAttr);
    if (0 != status) {
        PAL_ERR(LOG_TAG,"getStreamAttributes Failed \n");
        goto exit;
    }
    if (sAttr.direction == (PAL_AUDIO_INPUT | PAL_AUDIO_OUTPUT)) {
        switch (tagId) {
            case DEVICE_HW_ENDPOINT_TX:
            case BT_PLACEHOLDER_DECODER:
            case COP_DEPACKETIZER_V2:
                if (!pcmDevTxIds.size()){
                    PAL_ERR(LOG_TAG,"pcmDevTxIds not found \n");
                    status = -EINVAL;
                    goto exit;
                }
                device = pcmDevTxIds.at(0);
                break;
            case DEVICE_HW_ENDPOINT_RX:
            case BT_PLACEHOLDER_ENCODER:
            case SS_SBC:
            case SS_SSC:
            case COP_PACKETIZER_V2:
            case COP_PACKETIZER_V0:
            case SS_COP_PACKETIZER:
            case MODULE_SP:
                if (!pcmDevRxIds.size()){
                    PAL_ERR(LOG_TAG,"pcmDevRxIds not found \n");
                    status = -EINVAL;
                    goto exit;
                }
                device = pcmDevRxIds.at(0);
                break;
            case RAT_RENDER:
            case BT_PCM_CONVERTER:
                if(strstr(backendName,"TX"))
                    device = pcmDevTxIds.at(0);
                else
                    device = pcmDevRxIds.at(0);
                break;
            default:
                PAL_INFO(LOG_TAG, "Unsupported loopback tag info %x",tagId);
                status = -EINVAL;
                goto exit;
        }
    } else {
        if (!pcmDevIds.size()){
            PAL_ERR(LOG_TAG,"pcmDevIds not found \n");
            status = -EINVAL;
            goto exit;
        }
        device = pcmDevIds.at(0);
    }
    /* REPLACE THIS WITH STORED INFO DURING INITIAL SETUP */
    if (backendName) {
        status = SessionAlsaUtils::getModuleInstanceId(mixer,
            device, backendName, tagId, miid);
    } else {
        status = SessionAlsaUtils::getModuleInstanceId(mixer,
            device, txAifBackEnds[0].second.data(), tagId, miid);
    }

exit:
    if (0 != status)
        PAL_ERR(LOG_TAG, "Failed to get tag info %x, status = %d", tagId, status);

    return status;
}

int SessionAlsaPcm::setConfig(Stream * s, configType type, int tag)
{
    int status = 0;
    uint32_t tagsent;
    struct agm_tag_config *tagConfig = nullptr;
    struct agm_cal_config *calConfig = nullptr;
    const char *setParamTagControl = "setParamTag";
    const char *stream = "PCM";
    const char *setCalibrationControl = "setCalibration";
    struct mixer_ctl *ctl;
    std::ostringstream tagCntrlName;
    std::ostringstream calCntrlName;
    pal_stream_attributes sAttr;
    int tag_config_size = 0;
    int cal_config_size = 0;

    status = s->getStreamAttributes(&sAttr);
    if (status != 0) {
        PAL_ERR(LOG_TAG,"stream get attributes failed");
        return status;
    }
    PAL_DBG(LOG_TAG,"Enter tag: %d", tag);
    switch (type) {
        case MODULE:
            tkv.clear();
            status = builder->populateTagKeyVector(s, tkv, tag, &tagsent);
            if (0 != status) {
                PAL_ERR(LOG_TAG,"Failed to set the tag configuration\n");
                goto exit;
            }

            if (tkv.size() == 0) {
                status = -EINVAL;
                goto exit;
            }

            tag_config_size = sizeof(struct agm_tag_config) +
                              tkv.size() * sizeof(struct agm_key_value);
            tagConfig = (struct agm_tag_config*)malloc(tag_config_size);

            if (!tagConfig) {
                status = -EINVAL;
                goto exit;
            }

            status = SessionAlsaUtils::getTagMetadata(tagsent, tkv, tagConfig);
            if (0 != status) {
                goto exit;
            }

            if (PAL_STREAM_LOOPBACK == sAttr.type) {
                if (pcmDevRxIds.size() > 0)
                    tagCntrlName << stream << pcmDevRxIds.at(0) << " " << setParamTagControl;
            } else {
                if (pcmDevIds.size() > 0)
                    tagCntrlName << stream << pcmDevIds.at(0) << " " << setParamTagControl;
            }

            if (tagCntrlName.str().length() == 0) {
                status = -EINVAL;
                goto exit;
            }

            ctl = mixer_get_ctl_by_name(mixer, tagCntrlName.str().data());
            if (!ctl) {
                PAL_ERR(LOG_TAG, "Invalid mixer control: %s\n", tagCntrlName.str().data());
                status = -ENOENT;
                goto exit;
            }

            status = mixer_ctl_set_array(ctl, tagConfig, tag_config_size);
            if (status != 0) {
                PAL_ERR(LOG_TAG,"failed to set the tag calibration %d", status);
                goto exit;
            }
            tkv.clear();
            break;
        case CALIBRATION:
            status = builder->populateCalKeyVector(s, ckv, tag);
            if (0 != status) {
                PAL_ERR(LOG_TAG,"Failed to set the calibration data\n");
                goto exit;
            }

            if (ckv.size() == 0) {
                status = -EINVAL;
                goto exit;
            }

            cal_config_size = sizeof(struct agm_cal_config) +
                              (ckv.size() * sizeof(agm_key_value));
            calConfig = (struct agm_cal_config*)malloc(cal_config_size);

            if (!calConfig) {
                status = -EINVAL;
                goto exit;
            }

            status = SessionAlsaUtils::getCalMetadata(ckv, calConfig);
            if (PAL_STREAM_LOOPBACK == sAttr.type) {
                if (pcmDevRxIds.size() > 0)
                    calCntrlName << stream << pcmDevRxIds.at(0) << " " << setCalibrationControl;
            } else {
                if (pcmDevIds.size() > 0)
                    calCntrlName << stream << pcmDevIds.at(0) << " " << setCalibrationControl;
            }

            if (calCntrlName.str().length() == 0) {
                status = -EINVAL;
                goto exit;
            }

            ctl = mixer_get_ctl_by_name(mixer, calCntrlName.str().data());
            if (!ctl) {
                PAL_ERR(LOG_TAG, "Invalid mixer control: %s\n", calCntrlName.str().data());
                status = -ENOENT;
                goto exit;
            }

            status = mixer_ctl_set_array(ctl, calConfig, cal_config_size);
            if (status != 0) {
                PAL_ERR(LOG_TAG,"failed to set the tag calibration %d", status);
                goto exit;
            }
            ckv.clear();
            break;
        default:
            PAL_ERR(LOG_TAG, "invalid type %d", type);
            status = -EINVAL;
            goto exit;
    }

exit:
    if (tagConfig)
        free(tagConfig);
    if (calConfig)
        free(calConfig);

    PAL_DBG(LOG_TAG,"exit status: %d ", status);
    return status;
}

int SessionAlsaPcm::setTKV(Stream * s, configType type, effect_pal_payload_t *effectPayload)
{
    int status = 0;
    uint32_t tagsent;
    struct agm_tag_config* tagConfig = nullptr;
    const char *setParamTagControl = "setParamTag";
    const char *stream = "PCM";
    struct mixer_ctl *ctl;
    std::ostringstream tagCntrlName;
    int tkv_size = 0;
    pal_stream_attributes sAttr;

    status = s->getStreamAttributes(&sAttr);
    if (status != 0) {
        PAL_ERR(LOG_TAG, "stream get attributes failed");
        return status;
    }

    switch (type) {
        case MODULE:
        {
            pal_key_vector_t *pal_kvpair = (pal_key_vector_t *)effectPayload->payload;
            uint32_t num_tkvs =  pal_kvpair->num_tkvs;
            for (int i = 0; i < num_tkvs; i++) {
                    tkv.push_back(std::make_pair(pal_kvpair->kvp[i].key, pal_kvpair->kvp[i].value));
            }

            if (tkv.size() == 0) {
                status = -EINVAL;
                goto exit;
            }

            tagConfig = (struct agm_tag_config*)malloc(sizeof(struct agm_tag_config) +
                            (tkv.size() * sizeof(agm_key_value)));

            if(!tagConfig) {
                status = -ENOMEM;
                goto exit;
            }

            tagsent = effectPayload->tag;
            status = SessionAlsaUtils::getTagMetadata(tagsent, tkv, tagConfig);
            if (0 != status) {
                goto exit;
            }

            if (PAL_STREAM_LOOPBACK == sAttr.type) {
                tagCntrlName<<stream<<pcmDevRxIds.at(0)<<" "<<setParamTagControl;
            } else {
                tagCntrlName<<stream<<pcmDevIds.at(0)<<" "<<setParamTagControl;
            }

            ctl = mixer_get_ctl_by_name(mixer, tagCntrlName.str().data());
            if (!ctl) {
                PAL_ERR(LOG_TAG, "Invalid mixer control: %s\n", tagCntrlName.str().data());
                status = -ENOENT;
                goto exit;
            }
            PAL_VERBOSE(LOG_TAG, "mixer control: %s\n", tagCntrlName.str().data());

            tkv_size = tkv.size()*sizeof(struct agm_key_value);
            status = mixer_ctl_set_array(ctl, tagConfig, sizeof(struct agm_tag_config) + tkv_size);
            if (status != 0) {
                PAL_ERR(LOG_TAG,"failed to set the tag calibration %d", status);
                goto exit;
            }
            ctl = NULL;
            tkv.clear();

            break;
        }
        default:
            PAL_ERR(LOG_TAG,"invalid type ");
            status = -EINVAL;
            goto exit;
    }

exit:
    PAL_DBG(LOG_TAG,"exit status:%d ", status);
    if (tagConfig) {
        free(tagConfig);
        tagConfig = nullptr;
    }

    return status;
}

void SessionAlsaPcm::deRegisterAdmStream(Stream *s)
{
    if (rm->admDeregisterStreamFn)
        rm->admDeregisterStreamFn(rm->admData, static_cast<void *>(s));
}

void SessionAlsaPcm::registerAdmStream(Stream *s, pal_stream_direction_t dir,
        pal_stream_flags_t flags, struct pcm *pcm, struct pcm_config *cfg)
{
    switch (dir) {
    case PAL_AUDIO_INPUT:
        if (rm->admRegisterInputStreamFn) {
            rm->admRegisterInputStreamFn(rm->admData, static_cast<void *>(s));

            if (flags & PAL_STREAM_FLAG_MMAP_MASK) {
                if (rm->admSetConfigFn)
                    rm->admSetConfigFn(rm->admData, static_cast<void *>(s),
                            pcm, cfg);
            }
        }
        break;
    case PAL_AUDIO_OUTPUT:
        if (rm->admRegisterOutputStreamFn) {
            rm->admRegisterOutputStreamFn(rm->admData, static_cast<void *>(s));

            if (flags & PAL_STREAM_FLAG_MMAP_MASK) {
                if (rm->admSetConfigFn)
                    rm->admSetConfigFn(rm->admData, static_cast<void *>(s),
                            pcm, cfg);
            }
        }
        break;
    default:
        break;
    }
}

void SessionAlsaPcm::requestAdmFocus(Stream *s,  long ns)
{
    if (rm->admRequestFocusV2Fn)
        rm->admRequestFocusV2Fn(rm->admData, static_cast<void *>(s),
                ns);
    else if (rm->admRequestFocusFn)
        rm->admRequestFocusFn(rm->admData, static_cast<void *>(s));
}

void SessionAlsaPcm::releaseAdmFocus(Stream *s)
{
    if (rm->admAbandonFocusFn)
        rm->admAbandonFocusFn(rm->admData, static_cast<void *>(s));
}

int SessionAlsaPcm::start(Stream * s)
{
    struct pcm_config config;
    struct pal_stream_attributes sAttr;
    int32_t status = 0;
    std::vector<std::shared_ptr<Device>> associatedDevices;
    struct pal_device dAttr;
    struct pal_media_config codecConfig;
    struct sessionToPayloadParam streamData;
    uint8_t* payload = NULL;
    size_t payloadSize = 0;
    uint32_t miid;
    int payload_size = 0;
    struct agm_event_reg_cfg event_cfg;
    struct agm_event_reg_cfg *acd_event_cfg = nullptr;
    int tagId = 0;
    int DeviceId;

    PAL_DBG(LOG_TAG,"Enter");

    rm->voteSleepMonitor(s, true);
    status = s->getStreamAttributes(&sAttr);
    if (status != 0) {
        PAL_ERR(LOG_TAG,"stream get attributes failed");
        goto exit;
    }

    if (mState == SESSION_IDLE) {
        s->getBufInfo(&in_buf_size,&in_buf_count,&out_buf_size,&out_buf_count);
        memset(&config, 0, sizeof(config));

        if (sAttr.direction == PAL_AUDIO_INPUT) {
            config.rate = sAttr.in_media_config.sample_rate;
            config.format =
                   SessionAlsaUtils::palToAlsaFormat((uint32_t)sAttr.in_media_config.aud_fmt_id);
            config.channels = sAttr.in_media_config.ch_info.channels;
            config.period_size = SessionAlsaUtils::bytesToFrames(in_buf_size,
                config.channels, config.format);
            config.period_count = in_buf_count;
        } else {
            config.rate = sAttr.out_media_config.sample_rate;
            config.format =
                   SessionAlsaUtils::palToAlsaFormat((uint32_t)sAttr.out_media_config.aud_fmt_id);
            config.channels = sAttr.out_media_config.ch_info.channels;
            config.period_size = SessionAlsaUtils::bytesToFrames(out_buf_size,
                config.channels, config.format);
            config.period_count = out_buf_count;
        }
        config.start_threshold = 0;
        config.stop_threshold = 0;
        config.silence_threshold = 0;

        switch(sAttr.direction) {
            case PAL_AUDIO_INPUT:
                if(SessionAlsaUtils::isMmapUsecase(sAttr)) {
                    config.start_threshold = 0;
                    config.stop_threshold = INT32_MAX;
                    config.silence_threshold = 0;
                    config.silence_size = 0;
                    config.avail_min = config.period_size;
                    pcm = pcm_open(rm->getVirtualSndCard(), pcmDevIds.at(0),
                        PCM_IN |PCM_MMAP| PCM_NOIRQ, &config);
                } else {
                    pcm = pcm_open(rm->getVirtualSndCard(), pcmDevIds.at(0), PCM_IN, &config);
                }

                if (!pcm) {
                    PAL_ERR(LOG_TAG, "pcm open failed");
                    status = errno;
                    goto exit;
                }

                if (!pcm_is_ready(pcm)) {
                    PAL_ERR(LOG_TAG, "pcm open not ready");
                    status = errno;
                    goto exit;
                }
                break;
            case PAL_AUDIO_OUTPUT:
                if(SessionAlsaUtils::isMmapUsecase(sAttr)) {
                    config.start_threshold = config.period_size * 8;
                    config.stop_threshold = INT32_MAX;
                    config.silence_threshold = 0;
                    config.silence_size = 0;
                    config.avail_min = config.period_size;
                    pcm = pcm_open(rm->getVirtualSndCard(), pcmDevIds.at(0),
                        PCM_OUT |PCM_MMAP| PCM_NOIRQ, &config);
                } else {
                    pcm = pcm_open(rm->getVirtualSndCard(), pcmDevIds.at(0), PCM_OUT, &config);
                }

                if (!pcm) {
                    PAL_ERR(LOG_TAG, "pcm open failed");
                    status = errno;
                    goto exit;
                }

                if (!pcm_is_ready(pcm)) {
                    PAL_ERR(LOG_TAG, "pcm open not ready");
                    status = errno;
                    goto exit;
                }
                break;
            case PAL_AUDIO_INPUT | PAL_AUDIO_OUTPUT:
                pcmRx = pcm_open(rm->getVirtualSndCard(), pcmDevRxIds.at(0), PCM_OUT, &config);
                if (!pcmRx) {
                    PAL_ERR(LOG_TAG, "pcm-rx open failed");
                    status = errno;
                    goto exit;
                }

                if (!pcm_is_ready(pcmRx)) {
                    PAL_ERR(LOG_TAG, "pcm-rx open not ready");
                    status = errno;
                    goto exit;
                }
                pcmTx = pcm_open(rm->getVirtualSndCard(), pcmDevTxIds.at(0), PCM_IN, &config);
                if (!pcmTx) {
                    PAL_ERR(LOG_TAG, "pcm-tx open failed");
                    status = errno;
                    goto exit;
                }

                if (!pcm_is_ready(pcmTx)) {
                    PAL_ERR(LOG_TAG, "pcm-tx open not ready");
                    status = errno;
                    goto exit;
                }
                break;
        }
        mState = SESSION_OPENED;

        if (SessionAlsaUtils::isMmapUsecase(sAttr) &&
                !(sAttr.flags & PAL_STREAM_FLAG_MMAP_NO_IRQ_MASK))
            registerAdmStream(s, sAttr.direction, sAttr.flags, pcm, &config);
    }
    if (sAttr.type == PAL_STREAM_VOICE_UI) {
        payload_size = sizeof(struct agm_event_reg_cfg);
        memset(&event_cfg, 0, sizeof(event_cfg));
        event_cfg.event_config_payload_size = 0;
        event_cfg.is_register = 1;
        event_cfg.event_id = EVENT_ID_DETECTION_ENGINE_GENERIC_INFO;
        event_cfg.module_instance_id = svaMiid;
        SessionAlsaUtils::registerMixerEvent(mixer, pcmDevIds.at(0),
            (void *)&event_cfg, payload_size);
    } else if (sAttr.type == PAL_STREAM_ULTRASOUND && RegisterForEvents) {
        payload_size = sizeof(struct agm_event_reg_cfg);

        memset(&event_cfg, 0, sizeof(event_cfg));
        event_cfg.event_config_payload_size = 0;
        event_cfg.is_register = 1;
        event_cfg.event_id = EVENT_ID_GENERIC_US_DETECTION;
        tagId = ULTRASOUND_DETECTION_MODULE;
        DeviceId = pcmDevTxIds.at(0);
        SessionAlsaUtils::registerMixerEvent(mixer, DeviceId,
                txAifBackEnds[0].second.data(), tagId, (void *)&event_cfg,
                payload_size);
    } else if(sAttr.type == PAL_STREAM_ACD) {
        if (eventPayload) {
            payload_size = sizeof(struct agm_event_reg_cfg) + eventPayloadSize;

            acd_event_cfg = (struct agm_event_reg_cfg *)calloc(1, payload_size);
            if (acd_event_cfg) {
                acd_event_cfg->event_id = eventId;
                acd_event_cfg->event_config_payload_size = eventPayloadSize;
                acd_event_cfg->is_register = 1;
                memcpy(acd_event_cfg->event_config_payload, eventPayload, eventPayloadSize);
                SessionAlsaUtils::registerMixerEvent(mixer, pcmDevIds.at(0),
                    txAifBackEnds[0].second.data(), CONTEXT_DETECTION_ENGINE, (void *)acd_event_cfg,
                    payload_size);
                free(acd_event_cfg);
            } else {
                PAL_ERR(LOG_TAG, "get acd_event_cfg instance memory allocation failed");
                status = -ENOMEM;
                goto exit;
            }
        }
    } else if(sAttr.type == PAL_STREAM_CONTEXT_PROXY) {
        status = register_asps_event(1);
    }

    switch (sAttr.direction) {
        case PAL_AUDIO_INPUT:
            if ((sAttr.type != PAL_STREAM_VOICE_UI) &&
                (sAttr.type != PAL_STREAM_ACD) &&
                (sAttr.type != PAL_STREAM_CONTEXT_PROXY) &&
                (sAttr.type != PAL_STREAM_SENSOR_PCM_DATA) &&
                (SessionAlsaUtils::isMmapUsecase(sAttr) == false)) {
                /* Get MFC MIID and configure to match to stream config */
                /* This has to be done after sending all mixer controls and before connect */
                if (sAttr.type != PAL_STREAM_VOICE_CALL_RECORD)
                    status = SessionAlsaUtils::getModuleInstanceId(mixer, pcmDevIds.at(0),
                                                                txAifBackEnds[0].second.data(),
                                                                TAG_STREAM_MFC_SR, &miid);
                else
                    status = SessionAlsaUtils::getModuleInstanceId(mixer, pcmDevIds.at(0),
                                                                "ZERO", TAG_STREAM_MFC_SR, &miid);
                if (status != 0) {
                    PAL_ERR(LOG_TAG,"getModuleInstanceId failed");
                    goto exit;
                }
                if (sAttr.type != PAL_STREAM_VOICE_CALL_RECORD) {
                    PAL_ERR(LOG_TAG, "miid : %x id = %d, data %s\n", miid,
                        pcmDevIds.at(0), txAifBackEnds[0].second.data());
                } else {
                    PAL_ERR(LOG_TAG, "miid : %x id = %d\n", miid, pcmDevIds.at(0));
                }

                if (isPalPCMFormat(sAttr.in_media_config.aud_fmt_id))
                    streamData.bitWidth = ResourceManager::palFormatToBitwidthLookup(sAttr.in_media_config.aud_fmt_id);
                else
                    streamData.bitWidth = sAttr.in_media_config.bit_width;
                streamData.sampleRate = sAttr.in_media_config.sample_rate;
                streamData.numChannel = sAttr.in_media_config.ch_info.channels;
                streamData.rotation_type = PAL_SPEAKER_ROTATION_LR;
                streamData.ch_info = nullptr;
                builder->payloadMFCConfig(&payload, &payloadSize, miid, &streamData);
                if (payloadSize && payload) {
                    status = updateCustomPayload(payload, payloadSize);
                    freeCustomPayload(&payload, &payloadSize);
                    if (0 != status) {
                        PAL_ERR(LOG_TAG,"updateCustomPayload Failed\n");
                        goto exit;
                    }
                }
                status = SessionAlsaUtils::setMixerParameter(mixer, pcmDevIds.at(0),
                                                             customPayload, customPayloadSize);
                freeCustomPayload();
                if (status != 0) {
                    PAL_ERR(LOG_TAG,"setMixerParameter failed");
                    goto exit;
                }
                if (sAttr.type == PAL_STREAM_VOICE_CALL_RECORD) {
                    status = SessionAlsaUtils::getModuleInstanceId(mixer, pcmDevIds.at(0),
                                                                "ZERO", RAT_RENDER, &miid);
                    if (status != 0) {
                        PAL_ERR(LOG_TAG,"getModuleInstanceId failed");
                        goto exit;
                    }
                    PAL_INFO(LOG_TAG, "miid : %x id = %d\n", miid, pcmDevIds.at(0));
                    codecConfig.bit_width = sAttr.in_media_config.bit_width;
                    codecConfig.sample_rate = 48000;
                    codecConfig.aud_fmt_id =  sAttr.in_media_config.aud_fmt_id;
                    if (sAttr.info.voice_rec_info.record_direction == INCALL_RECORD_VOICE_UPLINK_DOWNLINK) {
                        codecConfig.ch_info.channels = 2;
                    } else {
                        codecConfig.ch_info.channels = sAttr.in_media_config.ch_info.channels;
                    }
                    builder->payloadRATConfig(&payload, &payloadSize, miid, &codecConfig);
                    if (payloadSize && payload) {
                        status = updateCustomPayload(payload, payloadSize);
                        freeCustomPayload(&payload, &payloadSize);
                        if (0 != status) {
                            PAL_ERR(LOG_TAG,"updateCustomPayload Failed\n");
                            goto exit;
                        }
                    }
                    status = SessionAlsaUtils::setMixerParameter(mixer, pcmDevIds.at(0),
                                                                 customPayload, customPayloadSize);
                    freeCustomPayload();
                    if (status != 0) {
                        PAL_ERR(LOG_TAG,"setMixerParameter failed for RAT render");
                        goto exit;
                    }
                    switch (sAttr.info.voice_rec_info.record_direction) {
                        case INCALL_RECORD_VOICE_UPLINK:
                            tagId = INCALL_RECORD_UPLINK;
                            break;
                        case INCALL_RECORD_VOICE_DOWNLINK:
                            tagId = INCALL_RECORD_DOWNLINK;
                            break;
                        case INCALL_RECORD_VOICE_UPLINK_DOWNLINK:
                            if (sAttr.in_media_config.ch_info.channels == 2)
                                tagId = INCALL_RECORD_UPLINK_DOWNLINK_STEREO;
                            else
                                tagId = INCALL_RECORD_UPLINK_DOWNLINK_MONO;
                            break;
                    }
                    status = setConfig(s, MODULE, tagId);
                    if (status)
                        PAL_ERR(LOG_TAG, "Failed to set incall record params status = %d", status);
                }
            } else if (sAttr.type == PAL_STREAM_VOICE_UI || (sAttr.type == PAL_STREAM_ACD)) {
                SessionAlsaUtils::setMixerParameter(mixer,
                    pcmDevIds.at(0), customPayload, customPayloadSize);
                freeCustomPayload();            }
            if (ResourceManager::isLpiLoggingEnabled()) {
                PAL_INFO(LOG_TAG, "LPI data logging Param ON");
                /* No error check as TAG/TKV may not required for non LPI usecases */
                setConfig(s, MODULE, LPI_LOGGING_ON);
            }

            status = pcm_start(pcm);
            if (status) {
                status = errno;
                PAL_ERR(LOG_TAG, "pcm_start failed %d", status);
            }
            break;
        case PAL_AUDIO_OUTPUT:
            if (sAttr.type == PAL_STREAM_VOICE_CALL_MUSIC) {
                status = SessionAlsaUtils::getModuleInstanceId(mixer, pcmDevIds.at(0),
                                                                "ZERO", RAT_RENDER, &miid);
                if (status != 0) {
                    PAL_ERR(LOG_TAG,"getModuleInstanceId failed");
                    goto exit;
                }
                PAL_INFO(LOG_TAG, "miid : %x id = %d\n", miid, pcmDevIds.at(0));
                codecConfig.bit_width = sAttr.out_media_config.bit_width;
                codecConfig.sample_rate = sAttr.out_media_config.sample_rate;
                codecConfig.aud_fmt_id = sAttr.out_media_config.aud_fmt_id;
                codecConfig.ch_info.channels = sAttr.out_media_config.ch_info.channels;
                builder->payloadRATConfig(&payload, &payloadSize, miid, &codecConfig);
                if (payloadSize && payload) {
                    status = updateCustomPayload(payload, payloadSize);
                    freeCustomPayload(&payload, &payloadSize);
                    if (0 != status) {
                        PAL_ERR(LOG_TAG,"updateCustomPayload Failed\n");
                        goto exit;
                    }
                }
                status = SessionAlsaUtils::setMixerParameter(mixer, pcmDevIds.at(0),
                                                             customPayload, customPayloadSize);
                freeCustomPayload();
                if (status != 0) {
                    PAL_ERR(LOG_TAG,"setMixerParameter failed for RAT render");
                    goto exit;
                }
                goto pcm_start;
            }
            status = s->getAssociatedDevices(associatedDevices);
            if (0 != status) {
                PAL_ERR(LOG_TAG,"getAssociatedDevices Failed\n");
                goto exit;
            }
            for (int i = 0; i < associatedDevices.size();i++) {
                status = associatedDevices[i]->getDeviceAttributes(&dAttr);
                if (0 != status) {
                    PAL_ERR(LOG_TAG,"get Device Attributes Failed\n");
                    goto exit;
                }

                status = configureMFC(rm, sAttr, dAttr, pcmDevIds,
                            rxAifBackEnds[i].second.data());
                if (status != 0) {
                    PAL_ERR(LOG_TAG,"configure MFC failed");
                    goto exit;
                }
            }

pcm_start:
            status = pcm_start(pcm);
            if (status) {
                status = errno;
                PAL_ERR(LOG_TAG, "pcm_start failed %d", status);
            }
            if (!status && isPauseRegistrationDone) {
                // Stream supports Soft Pause and registration with RM is
                // successful. So register for Soft pause callback from adsp.
                payload_size = sizeof(struct agm_event_reg_cfg);
                memset(&event_cfg, 0, sizeof(event_cfg));

                event_cfg.event_id = EVENT_ID_SOFT_PAUSE_PAUSE_COMPLETE;
                event_cfg.event_config_payload_size = 0;
                event_cfg.is_register = 1;

                status = SessionAlsaUtils::registerMixerEvent(mixer, pcmDevIds.at(0),
                            rxAifBackEnds[0].second.data(), TAG_PAUSE, (void *)&event_cfg,
                            payload_size);
                if (status != 0) {
                    PAL_ERR(LOG_TAG, "Register for Pause failed %d", status);
                    // If registration fails for this then pop issue will come.
                    // It isn't fatal so not throwing error.
                    status = 0;
                    isPauseRegistrationDone = false;
                }
            }
            break;
        case PAL_AUDIO_INPUT | PAL_AUDIO_OUTPUT:
            status = s->getAssociatedDevices(associatedDevices);
            if (0 != status) {
                PAL_ERR(LOG_TAG,"getAssociatedDevices Failed");
                goto exit;
            }
            for (int i = 0; i < associatedDevices.size(); i++) {
                if (!SessionAlsaUtils::isRxDevice(
                            associatedDevices[i]->getSndDeviceId()))
                    continue;

                status = associatedDevices[i]->getDeviceAttributes(&dAttr);
                if (0 != status) {
                    PAL_ERR(LOG_TAG,"get Device Attributes Failed");
                    goto exit;
                }
                status = configureMFC(rm, sAttr, dAttr, pcmDevRxIds,
                            rxAifBackEnds[0].second.data());
                if (status != 0) {
                    PAL_ERR(LOG_TAG,"configure MFC failed");
                    goto exit;
                }
            }
            status = pcm_start(pcmRx);
            if (status) {
                status = errno;
                PAL_ERR(LOG_TAG, "pcm_start rx failed %d", status);
            }
            status = pcm_start(pcmTx);
            if (status) {
                status = errno;
                PAL_ERR(LOG_TAG, "pcm_start tx failed %d", status);
            }
           break;
    }
    // Setting the volume as in stream open, no default volume is set.
    if (sAttr.type != PAL_STREAM_ACD &&
        sAttr.type != PAL_STREAM_VOICE_UI &&
        sAttr.type != PAL_STREAM_CONTEXT_PROXY &&
        sAttr.type != PAL_STREAM_ULTRASOUND &&
        sAttr.type != PAL_STREAM_SENSOR_PCM_DATA &&
        sAttr.type != PAL_STREAM_HAPTICS) {
        if (setConfig(s, CALIBRATION, TAG_STREAM_VOLUME) != 0) {
            PAL_ERR(LOG_TAG,"Setting volume failed");
        }
    }

    mState = SESSION_STARTED;

exit:
    if (status != 0)
        rm->voteSleepMonitor(s, false);
    PAL_DBG(LOG_TAG,"Exit status: %d", status);
    return status;
}

int SessionAlsaPcm::stop(Stream * s)
{
    int status = 0;
    struct pal_stream_attributes sAttr;
    struct agm_event_reg_cfg event_cfg;
    int payload_size = 0;
    int tagId;
    int DeviceId;

    PAL_DBG(LOG_TAG,"Enter");
    status = s->getStreamAttributes(&sAttr);
    if (status != 0) {
        PAL_ERR(LOG_TAG,"stream get attributes failed");
        return status;
    }
    switch (sAttr.direction) {
        case PAL_AUDIO_INPUT:
            if (pcm && isActive()) {
                status = pcm_stop(pcm);
                if (status) {
                    status = errno;
                    PAL_ERR(LOG_TAG, "pcm_stop failed %d", status);
                }
            }
        break;
        case PAL_AUDIO_OUTPUT:
            if (pcm && isActive()) {
                status = pcm_stop(pcm);
                if (status) {
                    status = errno;
                    PAL_ERR(LOG_TAG, "pcm_stop failed %d", status);
                }
            }

            if (!status && isPauseRegistrationDone) {
                // Stream supports Soft Pause and was registered with RM
                // sucessfully. Thus Deregister callback for Soft Pause
                payload_size = sizeof(struct agm_event_reg_cfg);

                memset(&event_cfg, 0, sizeof(event_cfg));
                event_cfg.event_id = EVENT_ID_SOFT_PAUSE_PAUSE_COMPLETE;
                event_cfg.event_config_payload_size = 0;
                event_cfg.is_register = 0;
                SessionAlsaUtils::registerMixerEvent(mixer, pcmDevIds.at(0),
                        rxAifBackEnds[0].second.data(), TAG_PAUSE, (void *)&event_cfg,
                        payload_size);
                isPauseRegistrationDone = false;
            }
            break;
        case PAL_AUDIO_INPUT | PAL_AUDIO_OUTPUT:
            if (pcmRx && isActive()) {
                status = pcm_stop(pcmRx);
                if (status) {
                    status = errno;
                    PAL_ERR(LOG_TAG, "pcm_stop - rx failed %d", status);
                }
            }
            if (pcmTx && isActive()) {
                status = pcm_stop(pcmTx);
                if (status) {
                    status = errno;
                    PAL_ERR(LOG_TAG, "pcm_stop - tx failed %d", status);
                }
            }
            break;
    }
   rm->voteSleepMonitor(s, false);
    mState = SESSION_STOPPED;

    if (sAttr.type == PAL_STREAM_VOICE_UI) {
        payload_size = sizeof(struct agm_event_reg_cfg);
        memset(&event_cfg, 0, sizeof(event_cfg));
        event_cfg.event_config_payload_size = 0;
        event_cfg.is_register = 0;
        event_cfg.event_id = EVENT_ID_DETECTION_ENGINE_GENERIC_INFO;
        event_cfg.module_instance_id = svaMiid;
        SessionAlsaUtils::registerMixerEvent(mixer, pcmDevIds.at(0),
            (void *)&event_cfg, payload_size);
    } else if (sAttr.type == PAL_STREAM_ULTRASOUND && RegisterForEvents) {
        payload_size = sizeof(struct agm_event_reg_cfg);
        memset(&event_cfg, 0, sizeof(event_cfg));
        event_cfg.event_config_payload_size = 0;
        event_cfg.is_register = 0;
        event_cfg.event_id = EVENT_ID_GENERIC_US_DETECTION;
        tagId = ULTRASOUND_DETECTION_MODULE;
        DeviceId = pcmDevTxIds.at(0);
        RegisterForEvents = false;
        SessionAlsaUtils::registerMixerEvent(mixer, DeviceId,
                txAifBackEnds[0].second.data(), tagId, (void *)&event_cfg,
                payload_size);
    } else if (sAttr.type == PAL_STREAM_ACD) {
        if (eventPayload == NULL)
            goto exit;

        payload_size = sizeof(struct agm_event_reg_cfg);
        memset(&event_cfg, 0, sizeof(event_cfg));
        event_cfg.event_id = eventId;
        event_cfg.event_config_payload_size = 0;
        event_cfg.is_register = 0;
        if (!txAifBackEnds.empty()) {
           SessionAlsaUtils::registerMixerEvent(mixer, pcmDevIds.at(0),
               txAifBackEnds[0].second.data(), CONTEXT_DETECTION_ENGINE, (void *)&event_cfg,
               payload_size);
        }
    } else if(sAttr.type == PAL_STREAM_CONTEXT_PROXY) {
        status = register_asps_event(0);
    }
exit:
    PAL_DBG(LOG_TAG,"Exit status: %d", status);
    return status;
}

int SessionAlsaPcm::close(Stream * s)
{
    int status = 0;
    struct pal_stream_attributes sAttr;
    std::string backendname;
    int32_t beDevId = 0;
    std::vector<std::shared_ptr<Device>> associatedDevices;
    int ldir = 0;
    std::vector<int> pcmId;

    PAL_DBG(LOG_TAG,"Enter");
    status = s->getStreamAttributes(&sAttr);
    if (status != 0) {
        PAL_ERR(LOG_TAG,"stream get attributes failed");
        goto exit;
    }
    if (sAttr.type != PAL_STREAM_VOICE_CALL_RECORD &&
        sAttr.type != PAL_STREAM_VOICE_CALL_MUSIC  &&
        sAttr.type != PAL_STREAM_CONTEXT_PROXY) {
        status = s->getAssociatedDevices(associatedDevices);
        if (status != 0) {
            PAL_ERR(LOG_TAG,"getAssociatedDevices Failed\n");
            goto exit;
        }
    }
    freeDeviceMetadata.clear();

    switch (sAttr.direction) {
        case PAL_AUDIO_INPUT:
            for (auto &dev: associatedDevices) {
                beDevId = dev->getSndDeviceId();
                rm->getBackendName(beDevId, backendname);
                PAL_DBG(LOG_TAG, "backendname %s", backendname.c_str());
                if (dev->getDeviceCount() > 1) {
                    PAL_DBG(LOG_TAG, "Tx dev still active\n");
                    freeDeviceMetadata.push_back(std::make_pair(backendname, 0));
                } else {
                    freeDeviceMetadata.push_back(std::make_pair(backendname, 1));
                    PAL_DBG(LOG_TAG, "Tx dev not active");
                }
            }
            status = SessionAlsaUtils::close(s, rm, pcmDevIds, txAifBackEnds, freeDeviceMetadata);
            if (status) {
                PAL_ERR(LOG_TAG, "session alsa close failed with %d", status);
            }
            if (SessionAlsaUtils::isMmapUsecase(sAttr) &&
                !(sAttr.flags & PAL_STREAM_FLAG_MMAP_NO_IRQ_MASK))
                deRegisterAdmStream(s);
            if (pcm)
                status = pcm_close(pcm);
            if (status) {
                status = errno;
                PAL_ERR(LOG_TAG, "pcm_close failed %d", status);
            }
            if (sAttr.type == PAL_STREAM_ACD ||
                sAttr.type == PAL_STREAM_SENSOR_PCM_DATA)
                ldir = TX_HOSTLESS;

            rm->freeFrontEndIds(pcmDevIds, sAttr, ldir);
            pcm = NULL;
            break;
        case PAL_AUDIO_OUTPUT:
            for (auto &dev: associatedDevices) {
                beDevId = dev->getSndDeviceId();
                rm->getBackendName(beDevId, backendname);
                PAL_DBG(LOG_TAG, "backendname %s", backendname.c_str());
                if (dev->getDeviceCount() > 1) {
                    PAL_DBG(LOG_TAG, "Rx dev still active");
                    freeDeviceMetadata.push_back(std::make_pair(backendname, 0));
                } else {
                    PAL_DBG(LOG_TAG, "Rx dev not active");
                    freeDeviceMetadata.push_back(std::make_pair(backendname, 1));
                }
            }
            status = SessionAlsaUtils::close(s, rm, pcmDevIds, rxAifBackEnds, freeDeviceMetadata);
            if (status) {
                PAL_ERR(LOG_TAG, "session alsa close failed with %d", status);
            }
            if (SessionAlsaUtils::isMmapUsecase(sAttr) &&
                !(sAttr.flags & PAL_STREAM_FLAG_MMAP_NO_IRQ_MASK))
                deRegisterAdmStream(s);
            if (pcm)
                status = pcm_close(pcm);
            if (status) {
                status = errno;
                PAL_ERR(LOG_TAG, "pcm_close failed %d", status);
            }
            // Deregister callback for Soft Pause
            if (!status && isPauseRegistrationDone) {
                status = rm->registerMixerEventCallback(pcmDevIds,
                    sessionCb, cbCookie, false);
                if (status != 0) {
                    PAL_DBG(LOG_TAG, "Failed to deregister callback to rm");
                    status = 0;
                }
                isPauseRegistrationDone = false;
            }
            rm->freeFrontEndIds(pcmDevIds, sAttr, 0);
            pcm = NULL;
            break;
        case PAL_AUDIO_INPUT | PAL_AUDIO_OUTPUT:
            for (auto &dev: associatedDevices) {
                beDevId = dev->getSndDeviceId();
                rm->getBackendName(beDevId, backendname);
                PAL_DBG(LOG_TAG, "backendname %s", backendname.c_str());
                if (dev->getDeviceCount() > 1) {
                    PAL_DBG(LOG_TAG, "dev %d still active", beDevId);
                    freeDeviceMetadata.push_back(std::make_pair(backendname, 0));
                } else {
                    PAL_DBG(LOG_TAG, "dev %d not active", beDevId);
                    freeDeviceMetadata.push_back(std::make_pair(backendname, 1));
                }
            }
            status = SessionAlsaUtils::close(s, rm, pcmDevRxIds, pcmDevTxIds,
                    rxAifBackEnds, txAifBackEnds, freeDeviceMetadata);
            if (status) {
                PAL_ERR(LOG_TAG, "session alsa close failed with %d", status);
            }
            if (pcmRx)
                status = pcm_close(pcmRx);
            if (status) {
                status = errno;
                PAL_ERR(LOG_TAG, "pcm_close - rx failed %d", status);
            }
            if (pcmTx)
                status = pcm_close(pcmTx);
            if (status) {
                status = errno;
               PAL_ERR(LOG_TAG, "pcm_close - tx failed %d", status);
            }
            rm->freeFrontEndIds(pcmDevRxIds, sAttr, RX_HOSTLESS);
            rm->freeFrontEndIds(pcmDevTxIds, sAttr, TX_HOSTLESS);
            pcmRx = NULL;
            pcmTx = NULL;
            break;
    }

    mState = SESSION_IDLE;

    if (sAttr.type == PAL_STREAM_VOICE_UI ||
        sAttr.type == PAL_STREAM_ACD ||
        sAttr.type == PAL_STREAM_CONTEXT_PROXY ||
        sAttr.type == PAL_STREAM_ULTRASOUND) {
        switch (sAttr.type) {
            case PAL_STREAM_VOICE_UI:
            case PAL_STREAM_ACD:
            case PAL_STREAM_CONTEXT_PROXY:
                pcmId = pcmDevIds;
                break;
            case PAL_STREAM_ULTRASOUND:
                pcmId = pcmDevTxIds;
                break;
            default:
                break;
        }
        status = rm->registerMixerEventCallback(pcmId,
                      sessionCb, cbCookie, false);
        if (status != 0) {
            PAL_ERR(LOG_TAG, "Failed to deregister callback to rm");
        }
    }

    freeCustomPayload();
    if (eventPayload) {
        free(eventPayload);
        eventPayload = NULL;
        eventPayloadSize = 0;
        eventId = 0;
    }
exit:
    ecRefDevId = PAL_DEVICE_OUT_MIN;
    PAL_DBG(LOG_TAG,"Exit status: %d", status);
    return status;
}

/* TODO: Check if this can be moved to Session class */
int SessionAlsaPcm::disconnectSessionDevice(Stream *streamHandle,
        pal_stream_type_t streamType, std::shared_ptr<Device> deviceToDisconnect)
{
    std::vector<std::shared_ptr<Device>> deviceList;
    struct pal_device dAttr;
    std::vector<std::pair<int32_t, std::string>> rxAifBackEndsToDisconnect;
    std::vector<std::pair<int32_t, std::string>> txAifBackEndsToDisconnect;
    int32_t status = 0;

    deviceList.push_back(deviceToDisconnect);
    rm->getBackEndNames(deviceList, rxAifBackEndsToDisconnect,
            txAifBackEndsToDisconnect);
    deviceToDisconnect->getDeviceAttributes(&dAttr);

    if (!rxAifBackEndsToDisconnect.empty()) {
        int cnt = 0;
        if (streamType != PAL_STREAM_ULTRASOUND)
            status = SessionAlsaUtils::disconnectSessionDevice(streamHandle, streamType, rm,
                     dAttr, (pcmDevIds.size() ? pcmDevIds : pcmDevRxIds), rxAifBackEndsToDisconnect);
        else
            status = SessionAlsaUtils::disconnectSessionDevice(streamHandle, streamType, rm,
                     dAttr, pcmDevTxIds, pcmDevRxIds, rxAifBackEndsToDisconnect);

        for (const auto &elem : rxAifBackEnds) {
            cnt++;
            for (const auto &disConnectElem : rxAifBackEndsToDisconnect) {
                if (std::get<0>(elem) == std::get<0>(disConnectElem)) {
                    rxAifBackEnds.erase(rxAifBackEnds.begin() + cnt - 1, rxAifBackEnds.begin() + cnt);
                    cnt--;
                    break;
                }
            }
        }
    }
    if (!txAifBackEndsToDisconnect.empty()) {
        int cnt = 0;
        status = SessionAlsaUtils::disconnectSessionDevice(streamHandle, streamType, rm,
            dAttr, (pcmDevIds.size() ? pcmDevIds : pcmDevTxIds), txAifBackEndsToDisconnect);

        for (const auto &elem : txAifBackEnds) {
            cnt++;
            for (const auto &disConnectElem : txAifBackEndsToDisconnect) {
                if (std::get<0>(elem) == std::get<0>(disConnectElem)) {
                    txAifBackEnds.erase(txAifBackEnds.begin() + cnt - 1, txAifBackEnds.begin() + cnt);
                    cnt--;
                    break;
                }
            }
        }
    }

    return status;
}

int SessionAlsaPcm::setupSessionDevice(Stream* streamHandle, pal_stream_type_t streamType,
        std::shared_ptr<Device> deviceToConnect)
{
    std::vector<std::shared_ptr<Device>> deviceList;
    struct pal_device dAttr;
    std::vector<std::pair<int32_t, std::string>> rxAifBackEndsToConnect;
    std::vector<std::pair<int32_t, std::string>> txAifBackEndsToConnect;
    int32_t status = 0;

    deviceList.push_back(deviceToConnect);
    rm->getBackEndNames(deviceList, rxAifBackEndsToConnect,
            txAifBackEndsToConnect);
    deviceToConnect->getDeviceAttributes(&dAttr);

    if (!rxAifBackEndsToConnect.empty())
        status = SessionAlsaUtils::setupSessionDevice(streamHandle, streamType, rm,
            dAttr, (pcmDevIds.size() ? pcmDevIds : pcmDevRxIds), rxAifBackEndsToConnect);

    if (!txAifBackEndsToConnect.empty())
        status = SessionAlsaUtils::setupSessionDevice(streamHandle, streamType, rm,
            dAttr, (pcmDevIds.size() ? pcmDevIds : pcmDevTxIds), txAifBackEndsToConnect);

    return status;
}

int SessionAlsaPcm::connectSessionDevice(Stream* streamHandle, pal_stream_type_t streamType,
        std::shared_ptr<Device> deviceToConnect)
{
    std::vector<std::shared_ptr<Device>> deviceList;
    struct pal_device dAttr;
    std::vector<std::pair<int32_t, std::string>> rxAifBackEndsToConnect;
    std::vector<std::pair<int32_t, std::string>> txAifBackEndsToConnect;
    int32_t status = 0;

    deviceList.push_back(deviceToConnect);
    rm->getBackEndNames(deviceList, rxAifBackEndsToConnect,
            txAifBackEndsToConnect);
    deviceToConnect->getDeviceAttributes(&dAttr);

    if (!rxAifBackEndsToConnect.empty()) {
        if (streamType != PAL_STREAM_ULTRASOUND)
            status = SessionAlsaUtils::connectSessionDevice(this, streamHandle, streamType, rm,
                     dAttr, (pcmDevIds.size() ? pcmDevIds : pcmDevRxIds), rxAifBackEndsToConnect);
        else
            status = SessionAlsaUtils::connectSessionDevice(this, streamHandle, streamType, rm,
                     dAttr, pcmDevTxIds, pcmDevRxIds, rxAifBackEndsToConnect);

        if (!status) {
            for (const auto &elem : rxAifBackEndsToConnect)
                rxAifBackEnds.push_back(elem);
        } else {
            PAL_ERR(LOG_TAG, "failed to connect rxAifBackEnds: %d",
                    (pcmDevIds.size() ? pcmDevIds.at(0) : pcmDevRxIds.at(0)));
        }
    }

    if (!txAifBackEndsToConnect.empty()) {
        status = SessionAlsaUtils::connectSessionDevice(this, streamHandle, streamType, rm,
            dAttr, (pcmDevIds.size() ? pcmDevIds : pcmDevTxIds), txAifBackEndsToConnect);

        if (!status) {
            for (const auto &elem : txAifBackEndsToConnect)
                txAifBackEnds.push_back(elem);
        } else {
            PAL_ERR(LOG_TAG, "failed to connect txAifBackEnds: %d",
                    (pcmDevIds.size() ? pcmDevIds.at(0) : pcmDevTxIds.at(0)));
        }
    }

    return status;
}

int SessionAlsaPcm::read(Stream *s, int tag __unused, struct pal_buffer *buf, int * size)
{
    int status = 0, bytesRead = 0, bytesToRead = 0, offset = 0, pcmReadSize = 0;
    struct pal_stream_attributes sAttr;

    PAL_VERBOSE(LOG_TAG,"Enter")
    status = s->getStreamAttributes(&sAttr);
    if (status != 0) {
        PAL_ERR(LOG_TAG,"stream get attributes failed");
        return status;
    }
    while (1) {
        offset = bytesRead + buf->offset;
        bytesToRead = buf->size - offset;
        if (!bytesToRead)
            break;
        if ((bytesToRead / in_buf_size) >= 1)
            pcmReadSize = in_buf_size;
        else
            pcmReadSize = bytesToRead;
        void *data = buf->buffer;
        data = static_cast<char*>(data) + offset;

        if(SessionAlsaUtils::isMmapUsecase(sAttr))
        {
            long ns = 0;
            if (sAttr.in_media_config.sample_rate)
                ns = pcm_bytes_to_frames(pcm, pcmReadSize)*1000000000LL/
                    sAttr.in_media_config.sample_rate;
            requestAdmFocus(s, ns);
            status =  pcm_mmap_read(pcm, data,  pcmReadSize);
            releaseAdmFocus(s);
        } else {
            status =  pcm_read(pcm, data,  pcmReadSize);
        }

        if ((0 != status) || (pcmReadSize == 0)) {
            PAL_ERR(LOG_TAG,"Failed to read data %d bytes read %d", status, pcmReadSize);
            break;
        }

#ifdef SEC_AUDIO_DUMP
        sec_pal_read_pcm(&sAttr, data, pcmReadSize, PCM_DUMP_PURE);
#endif
        bytesRead += pcmReadSize;
    }

    *size = bytesRead;
    PAL_VERBOSE(LOG_TAG,"exit bytesRead:%d status:%d ", bytesRead, status);
    return status;
}

int SessionAlsaPcm::write(Stream *s, int tag, struct pal_buffer *buf, int * size,
                          int flag)
{
    int status = 0, bytesWritten = 0, bytesRemaining = 0, offset = 0;
    uint32_t sizeWritten = 0;
    struct pal_stream_attributes sAttr;


    PAL_VERBOSE(LOG_TAG,"Enter buf:%p tag:%d flag:%d", buf, tag, flag);

    status = s->getStreamAttributes(&sAttr);
    if (status != 0) {
        PAL_ERR(LOG_TAG,"stream get attributes failed");
        return status;
    }

    if (pcm == NULL) {
        PAL_ERR(LOG_TAG,"PCM is NULL");
        return -EINVAL;
    }

    void *data = nullptr;

    bytesRemaining = buf->size;

    while ((bytesRemaining / out_buf_size) > 1) {
        offset = bytesWritten + buf->offset;
        data = buf->buffer;
        data = static_cast<char *>(data) + offset;
        sizeWritten = out_buf_size;  //initialize 0
        if (pcm && (mState == SESSION_FLUSHED)) {
            status = pcm_start(pcm);
            if (status) {
                status = errno;
                PAL_ERR(LOG_TAG, "pcm_start failed %d", status);
                goto exit;
            }
            mState = SESSION_STARTED;
        } else if (!pcm) {
            PAL_ERR(LOG_TAG, "pcm is NULL");
            status = -EINVAL;
            goto exit;
        }
#ifdef SEC_AUDIO_DUMP
        sec_pal_write_pcm(&sAttr, data, sizeWritten, PCM_DUMP_LAST);
#endif
        if (SessionAlsaUtils::isMmapUsecase(sAttr)) {
            long ns = 0;
            if (sAttr.out_media_config.sample_rate)
                ns = pcm_bytes_to_frames(pcm, sizeWritten)*1000000000LL/
                    sAttr.out_media_config.sample_rate;
            PAL_DBG(LOG_TAG,"1.bufsize:%u ns:%ld", sizeWritten, ns);
            requestAdmFocus(s, ns);
            status =  pcm_mmap_write(pcm, data,  sizeWritten);
            releaseAdmFocus(s);
        } else {
            status =  pcm_write(pcm, data,  sizeWritten);
        }

        if (0 != status) {
            PAL_ERR(LOG_TAG,"Failed to write the data");
            goto exit;
        }
        bytesWritten += sizeWritten;
        __builtin_sub_overflow(bytesRemaining, sizeWritten, &bytesRemaining);
    }
    offset = bytesWritten + buf->offset;
    sizeWritten = bytesRemaining;
    data = buf->buffer;
    if (pcm && (mState == SESSION_FLUSHED)) {
        status = pcm_start(pcm);
        if (status) {
            status = errno;
            PAL_ERR(LOG_TAG, "pcm_start failed %d", status);
            goto exit;
        }
        mState = SESSION_STARTED;
    } else if (!pcm) {
        PAL_ERR(LOG_TAG, "pcm is NULL");
        status = -EINVAL;
        goto exit;
    }

    data = static_cast<char *>(data) + offset;
#ifdef SEC_AUDIO_DUMP
    sec_pal_write_pcm(&sAttr, data, sizeWritten, PCM_DUMP_LAST);
#endif
    if (SessionAlsaUtils::isMmapUsecase(sAttr)) {
        if (sizeWritten) {
            long ns = 0;
            if (sAttr.out_media_config.sample_rate)
                ns = pcm_bytes_to_frames(pcm, sizeWritten)*1000000000LL/
                    sAttr.out_media_config.sample_rate;
            PAL_DBG(LOG_TAG,"2.bufsize:%u ns:%ld", sizeWritten, ns);
            requestAdmFocus(s, ns);
            status =  pcm_mmap_write(pcm, data,  sizeWritten);
            releaseAdmFocus(s);
            if (status != 0) {
                PAL_ERR(LOG_TAG,"Error! pcm_mmap_write failed");
                goto exit;
            }
        }
    } else {
        status =  pcm_write(pcm, data,  sizeWritten);
        if (status != 0) {
            PAL_ERR(LOG_TAG,"Error! pcm_write failed");
            goto exit;
        }
    }
    bytesWritten += sizeWritten;
    *size = bytesWritten;
exit:
    PAL_VERBOSE(LOG_TAG,"exit status: %d", status);
    return status;
}

int SessionAlsaPcm::readBufferInit(Stream * /*streamHandle*/, size_t /*noOfBuf*/, size_t /*bufSize*/,
                                   int /*flag*/)
{
    return 0;
}
int SessionAlsaPcm::writeBufferInit(Stream * /*streamHandle*/, size_t /*noOfBuf*/, size_t /*bufSize*/,
                                    int /*flag*/)
{
    return 0;
}

void SessionAlsaPcm::setEventPayload(uint32_t event_id, void *payload, size_t payload_size)
{
    eventId = event_id;
    if (eventPayload)
        free(eventPayload);

    eventPayloadSize = payload_size;
    eventPayload = calloc(1, payload_size);
    if (!eventPayload) {
        PAL_ERR(LOG_TAG, "Memory alloc failed for eventPayload");
        return;
    }
    memcpy(eventPayload, payload, payload_size);
}

int SessionAlsaPcm::setParameters(Stream *streamHandle, int tagId, uint32_t param_id, void *payload)
{
    int status = 0;
    int device = 0;
    uint8_t* paramData = NULL;
    size_t paramSize = 0;
    uint32_t miid = 0;
    effect_pal_payload_t *effectPalPayload = nullptr;
    struct pal_stream_attributes sAttr;

    PAL_DBG(LOG_TAG, "Enter. param id: %d", param_id);
    if (pcmDevIds.size() > 0)
        device = pcmDevIds.at(0);
    switch (param_id) {
        case PAL_PARAM_ID_DEVICE_ROTATION:
        {
            pal_param_device_rotation_t *rotation =
                                         (pal_param_device_rotation_t *)payload;
            status = handleDeviceRotation(streamHandle, rotation->rotation_type,
                                          device, mixer, builder, rxAifBackEnds);
            goto exit;
        }
        case PAL_PARAM_ID_LOAD_SOUND_MODEL:
        case PAL_PARAM_ID_UNLOAD_SOUND_MODEL:
        case PAL_PARAM_ID_WAKEUP_ENGINE_CONFIG:
        case PAL_PARAM_ID_WAKEUP_BUFFERING_CONFIG:
        case PAL_PARAM_ID_WAKEUP_ENGINE_RESET:
        case PAL_PARAM_ID_WAKEUP_CUSTOM_CONFIG:
        {
            struct apm_module_param_data_t* header =
                (struct apm_module_param_data_t *)payload;
            svaMiid = header->module_instance_id;
            paramData = (uint8_t *)payload;
            paramSize = PAL_ALIGN_8BYTE(header->param_size +
                sizeof(struct apm_module_param_data_t));
            if (mState == SESSION_IDLE) {
                if (!customPayloadSize) {
                    customPayload = (uint8_t *)calloc(1, paramSize);
                } else {
                    customPayload = (uint8_t *)realloc(customPayload,
                        customPayloadSize + paramSize);
                }
                if (!customPayload) {
                    status = -ENOMEM;
                    PAL_ERR(LOG_TAG, "failed to allocate memory for custom payload");
                    goto free_payload;
                }

                ar_mem_cpy((uint8_t *)customPayload + customPayloadSize, paramSize,
                    paramData, paramSize);
                customPayloadSize += paramSize;
                PAL_INFO(LOG_TAG, "customPayloadSize = %zu", customPayloadSize);
            } else {
                status = SessionAlsaUtils::setMixerParameter(mixer,
                    pcmDevIds.at(0), paramData, paramSize);
                if (status) {
                    PAL_ERR(LOG_TAG, "Failed to set mixer param, status = %d",
                        status);
                    goto exit;
                }
            }
            break;
        }
        case PAL_PARAM_ID_UIEFFECT:
        {
            pal_effect_custom_payload_t *customPayload;
            pal_param_payload *param_payload = (pal_param_payload *)payload;
            effectPalPayload = (effect_pal_payload_t *)(param_payload->payload);
            status = streamHandle->getStreamAttributes(&sAttr);
            if (status != 0) {
                PAL_ERR(LOG_TAG,"stream get attributes failed");
                goto exit;
            }

            if (PAL_AUDIO_INPUT == sAttr.direction)
                status = SessionAlsaUtils::getModuleInstanceId(mixer, device,
                                                               txAifBackEnds[0].second.data(),
                                                               tagId, &miid);
            else if (PAL_AUDIO_OUTPUT == sAttr.direction)
                status = SessionAlsaUtils::getModuleInstanceId(mixer, device,
                                                               rxAifBackEnds[0].second.data(),
                                                               tagId, &miid);
            else {
                if (pcmDevRxIds.size() > 0)
                    device = pcmDevRxIds.at(0);
                status = SessionAlsaUtils::getModuleInstanceId(mixer, device,
                                                               rxAifBackEnds[0].second.data(),
                                                               tagId, &miid);
                if (status) {
                    if (pcmDevTxIds.size() > 0)
                        device = pcmDevTxIds.at(0);
                    status = SessionAlsaUtils::getModuleInstanceId(mixer, device,
                                                               txAifBackEnds[0].second.data(),
                                                               tagId, &miid);
                }
            }
            if (0 != status) {
                PAL_ERR(LOG_TAG, "Failed to get tag info %x, status = %d", tagId, status);
                break;
            } else {
                customPayload = (pal_effect_custom_payload_t *)effectPalPayload->payload;
                status = builder->payloadCustomParam(&paramData, &paramSize,
                            customPayload->data,
                            effectPalPayload->payloadSize - sizeof(uint32_t),
                            miid, customPayload->paramId);
                if (status != 0) {
                    PAL_ERR(LOG_TAG, "payloadCustomParam failed. status = %d",
                                status);
                    break;
                }
                status = SessionAlsaUtils::setMixerParameter(mixer,
                                                             device,
                                                             paramData,
                                                             paramSize);
                PAL_INFO(LOG_TAG, "mixer set param status=%d\n", status);
#ifdef SEC_AUDIO_EARLYDROP_PATCH
                freeCustomPayload(&paramData, &paramSize);
#endif
            }
            break;
        }
        case PAL_PARAM_ID_BT_A2DP_TWS_CONFIG:
        {
            pal_bt_tws_payload *tws_payload = (pal_bt_tws_payload *)payload;
            status = SessionAlsaUtils::getModuleInstanceId(mixer, device,
                               rxAifBackEnds[0].second.data(), tagId, &miid);
            if (0 != status) {
                PAL_ERR(LOG_TAG, "Failed to get tag info %x, status = %d", tagId, status);
                return status;
            }

            builder->payloadTWSConfig(&paramData, &paramSize, miid,
                    tws_payload->isTwsMonoModeOn, tws_payload->codecFormat);
            if (paramSize) {
                status = SessionAlsaUtils::setMixerParameter(mixer, device,
                                               paramData, paramSize);
                PAL_INFO(LOG_TAG, "mixer set tws config status=%d\n", status);
                freeCustomPayload(&paramData, &paramSize);
            }
            return 0;
        }
        case PAL_PARAM_ID_BT_A2DP_LC3_CONFIG:
        {
            pal_bt_lc3_payload *lc3_payload = (pal_bt_lc3_payload *)payload;
            status = SessionAlsaUtils::getModuleInstanceId(mixer, device,
                               rxAifBackEnds[0].second.data(), tagId, &miid);
            if (0 != status) {
                PAL_ERR(LOG_TAG, "Failed to get tag info %x, status = %d", tagId, status);
                return status;
            }

            builder->payloadLC3Config(&paramData, &paramSize, miid,
                    lc3_payload->isLC3MonoModeOn);
            if (paramSize) {
                status = SessionAlsaUtils::setMixerParameter(mixer, device,
                                               paramData, paramSize);
                PAL_INFO(LOG_TAG, "mixer set lc3 config status=%d\n", status);
                freeCustomPayload(&paramData, &paramSize);
            }
            return 0;
        }
#ifdef SEC_PRODUCT_FEATURE_BLUETOOTH_SUPPORT_A2DP_OFFLOAD
        case PAL_PARAM_ID_BT_A2DP_SUSPENDED:
        {
            pal_param_ss_pack_a2dp_suspend_t *a2dp_suspend_payload = (pal_param_ss_pack_a2dp_suspend_t *)payload;
            status = SessionAlsaUtils::getModuleInstanceId(mixer, device,
                               rxAifBackEnds[0].second.data(), tagId, &miid);
            if (0 != status) {
                PAL_ERR(LOG_TAG, "Failed to get tag info %x, status = %d", tagId, status);
                return status;
            }

            PAL_INFO(LOG_TAG, "mixer set a2dp_suspend %d\n", a2dp_suspend_payload->a2dp_suspend);

            builder->payloadSsPackSuspendConfig(&paramData, &paramSize,
                miid, a2dp_suspend_payload->a2dp_suspend);

            if (paramSize) {
                status = SessionAlsaUtils::setMixerParameter(mixer, device,
                                               paramData, paramSize);
                PAL_INFO(LOG_TAG, "mixer set a2dp_suspend status=%d\n", status);
#ifdef SEC_AUDIO_EARLYDROP_PATCH
                delete [] paramData;
                paramSize = 0;
#else
                free(paramData);
#endif
            }
            return 0;
        }
        case PAL_PARAM_ID_BT_A2DP_DYN_BITRATE:
        {
            pal_param_bta2dp_dynbitrate_t *dynbitrate_payload = (pal_param_bta2dp_dynbitrate_t *)payload;
            status = SessionAlsaUtils::getModuleInstanceId(mixer, device,
                               rxAifBackEnds[0].second.data(), tagId, &miid);
            if (0 != status) {
                PAL_ERR(LOG_TAG, "Failed to get tag info %x, status = %d", tagId, status);
                return status;
            }

            PAL_INFO(LOG_TAG, "mixer set bitrate %d\n", dynbitrate_payload->bitrate);

            builder->payloadSsPackBitrate(&paramData, &paramSize,
                 miid, dynbitrate_payload->bitrate);
            if (paramSize) {
                status = SessionAlsaUtils::setMixerParameter(mixer, device,
                                               paramData, paramSize);
                PAL_INFO(LOG_TAG, "mixer set bitrate status=%d\n", status);
#ifdef SEC_AUDIO_EARLYDROP_PATCH
                delete [] paramData;
                paramSize = 0;
#else
                free(paramData);
#endif
            }
            return 0;
        }
        case PAL_PARAM_ID_BT_A2DP_SBM_CONFIG:
        {
            pal_param_bta2dp_sbm_t *sbm_payload = (pal_param_bta2dp_sbm_t *)payload;
            status = SessionAlsaUtils::getModuleInstanceId(mixer, device,
                               rxAifBackEnds[0].second.data(), tagId, &miid);
            if (0 != status) {
                PAL_ERR(LOG_TAG, "Failed to get tag info %x, status = %d", tagId, status);
                return status;
            }

            PAL_INFO(LOG_TAG, "mixer set sbm config firstParam=%d, secondParam=%d\n",
                                    sbm_payload->firstParam, sbm_payload->secondParam);

            builder->payloadSsPackSbmConfig(&paramData, &paramSize,
                 miid, sbm_payload);
            if (paramSize) {
                status = SessionAlsaUtils::setMixerParameter(mixer, device,
                                               paramData, paramSize);
                PAL_INFO(LOG_TAG, "mixer set sbm config status=%d\n", status);
#ifdef SEC_AUDIO_EARLYDROP_PATCH
                delete [] paramData;
                paramSize = 0;
#endif
            }
            return 0;
        }
#endif
        case PAL_PARAM_ID_MODULE_CONFIG:
        {
            pal_param_payload *param_payload = (pal_param_payload *)payload;
            if (param_payload->payload_size) {
                 status = SessionAlsaUtils::setMixerParameter(mixer, device,
                                                              param_payload->payload,
                                                              param_payload->payload_size);
                 PAL_INFO(LOG_TAG, "mixer set module config status=%d\n", status);
            }
            return 0;
        }
        case PAL_PARAM_ID_UPD_REGISTER_FOR_EVENTS:
        {
            pal_param_payload *param_payload = (pal_param_payload *)payload;
            pal_param_upd_event_detection_t *detection_payload =
                                   (pal_param_upd_event_detection_t *)param_payload->payload;
            RegisterForEvents = detection_payload->register_status;
            return 0;
        }
        case PAL_PARAM_ID_VOLUME_USING_SET_PARAM:
        {
            pal_param_payload *param_payload = (pal_param_payload *)payload;
            pal_volume_data *vdata = (struct pal_volume_data *)param_payload->payload;
            status = streamHandle->getStreamAttributes(&sAttr);
            if (sAttr.direction == PAL_AUDIO_OUTPUT) {
                status = SessionAlsaUtils::getModuleInstanceId(mixer, device,
                        rxAifBackEnds[0].second.data(), TAG_STREAM_VOLUME, &miid);
            } else if (sAttr.direction == PAL_AUDIO_INPUT) {
                status = SessionAlsaUtils::getModuleInstanceId(mixer, device,
                        txAifBackEnds[0].second.data(), TAG_STREAM_VOLUME, &miid);
            } else if (sAttr.direction == (PAL_AUDIO_INPUT | PAL_AUDIO_OUTPUT)) {
                status = -EINVAL;
                if (pcmDevRxIds.size()) {
                    device = pcmDevRxIds.at(0);
                    status = SessionAlsaUtils::getModuleInstanceId(mixer, device,
                            rxAifBackEnds[0].second.data(), TAG_STREAM_VOLUME, &miid);
                    if (status) {
                        if (pcmDevTxIds.size() > 0)
                            device = pcmDevTxIds.at(0);
                        status = SessionAlsaUtils::getModuleInstanceId(mixer, device,
                                                                       txAifBackEnds[0].second.data(),
                                                                       tagId, &miid);
                    }
                }
            } else {
                status = 0;
                goto exit;
            }
            if (0 != status) {
                PAL_ERR(LOG_TAG, "Failed to get tag info %x, dir: %d (%d)", tagId,
                       sAttr.direction, status);
                goto exit;
            }

            builder->payloadVolumeConfig(&paramData, &paramSize, miid, vdata);
            if (paramSize) {
                status = SessionAlsaUtils::setMixerParameter(mixer, device,
                                               paramData, paramSize);
                PAL_INFO(LOG_TAG, "mixer set volume config status=%d\n", status);
#ifdef SEC_AUDIO_EARLYDROP_PATCH
                delete [] paramData;
                paramSize = 0;
#else
                freeCustomPayload(&paramData, &paramSize);
#endif
            }
            return 0;

        }
        default:
            status = -EINVAL;
            PAL_ERR(LOG_TAG, "Unsupported param id %u status %d", param_id, status);
            goto exit;
    }

    if (!paramData) {
        status = -ENOMEM;
        PAL_ERR(LOG_TAG, "failed to get payload status %d", status);
        goto exit;
    }

    PAL_VERBOSE(LOG_TAG, "%pK - payload and %zu size", paramData , paramSize);

free_payload :
    free(paramData);
exit:
    PAL_DBG(LOG_TAG, "Exit. status %d", status);
    return status;
}

int SessionAlsaPcm::register_asps_event(uint32_t reg)
{
    int32_t status = 0;
    struct agm_event_reg_cfg *event_cfg = nullptr;
    uint32_t payload_size = sizeof(struct agm_event_reg_cfg);
    event_cfg = (struct agm_event_reg_cfg *)calloc(1, payload_size);
    if (!event_cfg) {
        status = -ENOMEM;
        return status;
    }
    event_cfg->event_config_payload_size = 0;
    event_cfg->is_register = reg;
    event_cfg->event_id = EVENT_ID_ASPS_GET_SUPPORTED_CONTEXT_IDS;
    event_cfg->module_instance_id = ASPS_MODULE_INSTANCE_ID;

    SessionAlsaUtils::registerMixerEvent(mixer, pcmDevIds.at(0),
            (void *)event_cfg, payload_size);

    event_cfg->event_id = EVENT_ID_ASPS_SENSOR_REGISTER_REQUEST;
    SessionAlsaUtils::registerMixerEvent(mixer, pcmDevIds.at(0),
            (void *)event_cfg, payload_size);

    event_cfg->event_id = EVENT_ID_ASPS_SENSOR_DEREGISTER_REQUEST;
    SessionAlsaUtils::registerMixerEvent(mixer, pcmDevIds.at(0),
            (void *)event_cfg, payload_size);

    event_cfg->event_id = EVENT_ID_ASPS_CLOSE_ALL;
    SessionAlsaUtils::registerMixerEvent(mixer, pcmDevIds.at(0),
            (void *)event_cfg, payload_size);
#ifdef SEC_AUDIO_EARLYDROP_PATCH
    free(event_cfg);
#endif
    return status;
}

int SessionAlsaPcm::setECRef(Stream *s, std::shared_ptr<Device> rx_dev, bool is_enable)
{
    int status = 0;
    struct pal_stream_attributes sAttr;
    std::vector <std::shared_ptr<Device>> rxDeviceList;
    std::vector <std::string> backendNames;
    std::vector <std::shared_ptr<Device>> extEcTxDeviceList;
    std::vector <std::string> extEcbackendNames;
    int32_t extEcbackendId;
    struct pcm_config config;
    std::shared_ptr<Device> dev = nullptr;
    struct pal_device device;
    struct pal_device rxDevAttr;
    struct pal_device_info rxDevInfo;

    PAL_DBG(LOG_TAG, "Enter");
    if (!s) {
        PAL_ERR(LOG_TAG, "Invalid stream or rx device");
        status = -EINVAL;
        goto exit;
    }

    status = s->getStreamAttributes(&sAttr);
    if (status != 0) {
        PAL_ERR(LOG_TAG, "stream get attributes failed");
        goto exit;
    }

    if (sAttr.direction != PAL_AUDIO_INPUT) {
        PAL_ERR(LOG_TAG, "EC Ref cannot be set to output stream");
        status = -EINVAL;
        goto exit;
    }

    rxDevInfo.isExternalECRefEnabledFlag = 0; //set by default to 0 to pass through
    if (rx_dev) {
        rx_dev->getDeviceAttributes(&rxDevAttr);
        PAL_DBG(LOG_TAG, "rx_dev id is %d", rxDevAttr.id);
        rm->getDeviceInfo(rxDevAttr.id, sAttr.type, rxDevAttr.custom_config.custom_key, &rxDevInfo);
    }


    if (rxDevInfo.isExternalECRefEnabledFlag) {
        device.id = PAL_DEVICE_IN_EXT_EC_REF;
        rm->getDeviceConfig(&device, &sAttr);
        dev = Device::getInstance(&device, rm);
        if (!dev) {
            PAL_ERR(LOG_TAG, "getInstance failed");
            goto exit;
        }
    }


    if (!is_enable) {
        if (ecRefDevId == PAL_DEVICE_OUT_MIN) {
            PAL_DBG(LOG_TAG, "EC ref not enabled, skip disabling");
            goto exit;
        } else if (rx_dev && ecRefDevId != rx_dev->getSndDeviceId()) {
            PAL_DBG(LOG_TAG, "Invalid rx dev %d for disabling EC ref, "
                "rx dev %d already enabled", rx_dev->getSndDeviceId(), ecRefDevId);
            goto exit;
        }

        if (pcmEcTx) {
            status = pcm_stop(pcmEcTx);
            if (status) {
                status = errno;
                PAL_ERR(LOG_TAG, "pcm_stop - ec_tx failed %d", status);
            }
            if (dev) {
                dev->stop();
            }
            status = pcm_close(pcmEcTx);
            if (status) {
                status = errno;
                PAL_ERR(LOG_TAG, "pcm_close - ec_tx failed %d", status);
            }
            if (dev) {
                dev->close();
            }
            rm->freeFrontEndEcTxIds(pcmDevEcTxIds);
            pcmEcTx = NULL;
        }

        status = SessionAlsaUtils::setECRefPath(mixer, pcmDevIds.at(0), "ZERO");
        if (status) {
            PAL_ERR(LOG_TAG, "Failed to disable EC Ref, status %d", status);
            goto exit;
        }
        ecRefDevId = PAL_DEVICE_OUT_MIN;
    } else if (is_enable && rx_dev) {
        if (rx_dev && ecRefDevId == rx_dev->getSndDeviceId()) {
            PAL_DBG(LOG_TAG, "EC Ref already set for dev %d", ecRefDevId);
            goto exit;
        }
        // TODO: handle EC Ref switch case also
        rxDeviceList.push_back(rx_dev);
        backendNames = rm->getBackEndNames(rxDeviceList);
        if (rxDevInfo.isExternalECRefEnabledFlag) {
            PAL_DBG(LOG_TAG, "Ext EC Ref flag is enabled");
            extEcTxDeviceList.push_back(dev);
            pcmDevEcTxIds = rm->allocateFrontEndExtEcIds();
            if (pcmDevEcTxIds.size() == 0) {
                PAL_ERR(LOG_TAG, "ResourceManger::getBackEndNames returned no EXT_EC device Ids");
                goto exit;
            }
            status = dev->open();
            if (0 != status) {
                PAL_ERR(LOG_TAG, "dev open failed");
                rm->freeFrontEndEcTxIds(pcmDevEcTxIds);
                goto exit;
            }
            status = dev->start();
            if (0 != status) {
                PAL_ERR(LOG_TAG, "dev start failed");
                dev->close();
                rm->freeFrontEndEcTxIds(pcmDevEcTxIds);
                goto exit;
            }
            extEcbackendId = extEcTxDeviceList[0]->getSndDeviceId();
            extEcbackendNames = rm->getBackEndNames(extEcTxDeviceList);
            status = SessionAlsaUtils::openDev(rm, pcmDevEcTxIds, extEcbackendId,
                extEcbackendNames.at(0).c_str());
            if (0 != status) {
                PAL_ERR(LOG_TAG, "SessionAlsaUtils::openDev failed");
                dev->stop();
                dev->close();
                rm->freeFrontEndEcTxIds(pcmDevEcTxIds);
                goto exit;
            }
            config.rate = rxDevAttr.config.sample_rate;
            config.format =
                   SessionAlsaUtils::palToAlsaFormat((uint32_t)sAttr.out_media_config.aud_fmt_id);
            config.channels = rxDevAttr.config.ch_info.channels;
            config.period_size = 256;
            config.period_count = 4;
            config.start_threshold = 0;
            config.stop_threshold = 0;
            config.silence_threshold = 0;
            pcmEcTx = pcm_open(rm->getVirtualSndCard(), pcmDevEcTxIds.at(0), PCM_IN, &config);
            if (!pcmEcTx) {
                PAL_ERR(LOG_TAG, "pcm-ec-tx open failed");
                dev->stop();
                dev->close();
                rm->freeFrontEndEcTxIds(pcmDevEcTxIds);
                status = errno;
                goto exit;
            }
            if (!pcm_is_ready(pcmEcTx)) {
                PAL_ERR(LOG_TAG, " pcm-ec-tx open not ready");
                pcmEcTx = NULL;
                dev->stop();
                dev->close();
                rm->freeFrontEndEcTxIds(pcmDevEcTxIds);
                status = errno;
                goto exit;
            }
            status = pcm_start(pcmEcTx);
            if (status) {
                status = errno;
                PAL_ERR(LOG_TAG, "pcm_start ec_tx failed %d", status);
                pcm_close(pcmEcTx);
                pcmEcTx = NULL;
                dev->stop();
                dev->close();
                rm->freeFrontEndEcTxIds(pcmDevEcTxIds);
                status = errno;
                goto exit;
            }
            PAL_INFO(LOG_TAG, "backend name %s", extEcbackendNames.at(0).c_str());
        } else {
            status = SessionAlsaUtils::setECRefPath(mixer, pcmDevIds.at(0),
                     backendNames[0].c_str());
        }
        dev = nullptr;
        if (status) {
            PAL_ERR(LOG_TAG, "Failed to enable EC Ref, status %d", status);
            goto exit;
        }
        ecRefDevId = static_cast<pal_device_id_t>(rx_dev->getSndDeviceId());
    } else {
        PAL_ERR(LOG_TAG, "Invalid operation");
        status = -EINVAL;
        goto exit;
    }
exit:
    PAL_DBG(LOG_TAG, "Exit, status: %d", status);
    return status;
}

int SessionAlsaPcm::getParameters(Stream *s __unused, int tagId, uint32_t param_id, void **payload)
{
    int status = 0;
    uint8_t *ptr = NULL;
    uint8_t *config = NULL;
    uint8_t *payloadData = NULL;
    size_t payloadSize = 0;
    size_t configSize = 0;
    int device = pcmDevIds.at(0);
    uint32_t miid = 0;
    const char *control = "getParam";
    const char *stream = "PCM";
    struct mixer_ctl *ctl;
    std::ostringstream CntrlName;
    PAL_DBG(LOG_TAG, "Enter.");

    CntrlName << stream << pcmDevIds.at(0) << " " << control;
    ctl = mixer_get_ctl_by_name(mixer, CntrlName.str().data());
    if (!ctl) {
        PAL_ERR(LOG_TAG, "Invalid mixer control: %s\n", CntrlName.str().data());
        status = -ENOENT;
        goto exit;
    }

    if (!rxAifBackEnds.empty()) { /** search in RX GKV */
        status = SessionAlsaUtils::getModuleInstanceId(mixer, device, rxAifBackEnds[0].second.data(),
                tagId, &miid);
        if (status) /** if not found, reset miid to 0 again */
            miid = 0;
    }

    if (!txAifBackEnds.empty()) { /** search in TX GKV */
        status = SessionAlsaUtils::getModuleInstanceId(mixer, device, txAifBackEnds[0].second.data(),
                tagId, &miid);
        if (status)
            miid = 0;
    }

    if (miid == 0) {
        PAL_ERR(LOG_TAG, "failed to look for module with tagID 0x%x", tagId);
        status = -EINVAL;
        goto exit;
    }


    switch (param_id) {
        case PAL_PARAM_ID_DIRECTION_OF_ARRIVAL:
        {
            configSize = sizeof(struct ffv_doa_tracking_monitor_t);
            builder->payloadDOAInfo(&payloadData, &payloadSize, miid);
            break;
        }
        case PAL_PARAM_ID_WAKEUP_MODULE_VERSION:
        {
            payloadData = (uint8_t *)*payload;
            struct apm_module_param_data_t *header =
                (struct apm_module_param_data_t *)payloadData;
            configSize = header->param_size;
            payloadSize = PAL_ALIGN_8BYTE(
                configSize + sizeof(struct apm_module_param_data_t));
            break;
        }
        default:
            status = EINVAL;
            PAL_ERR(LOG_TAG, "Unsupported param id %u status %d", param_id, status);
            goto exit;
    }

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
    config = (uint8_t *)calloc(1, configSize);
    if (!config) {
        PAL_ERR(LOG_TAG, "Failed to allocate memory for config");
        status = -ENOMEM;
        goto exit;
    }

    ar_mem_cpy(config, configSize, ptr, configSize);
    *payload = (void *)config;


exit:
    freeCustomPayload(&payloadData, &payloadSize);
    PAL_DBG(LOG_TAG, "Exit. status %d", status);
    return status;
}

int SessionAlsaPcm::registerCallBack(session_callback cb, uint64_t cookie)
{
    sessionCb = cb;
    cbCookie = cookie;
    return 0;
}

int SessionAlsaPcm::getTimestamp(struct pal_session_time *stime)
{
    int status = 0;

    if (!spr_miid) {
        status = SessionAlsaUtils::getModuleInstanceId(mixer,
                pcmDevIds.at(0), rxAifBackEnds[0].second.data(),
                STREAM_SPR, &spr_miid);
        if (0 != status) {
            PAL_ERR(LOG_TAG, "Failed to get tag info %x, status = %d",
                    STREAM_SPR, status);
            return status;
        }
    }
    status = SessionAlsaUtils::getTimestamp(mixer, pcmDevIds, spr_miid, stime);
    if (0 != status)
       PAL_ERR(LOG_TAG, "getTimestamp failed status = %d", status);

    return status;
}

int SessionAlsaPcm::drain(pal_drain_type_t type __unused)
{
    return 0;
}

int SessionAlsaPcm::flush()
{
    int status = 0;

    if (!pcm) {
        PAL_ERR(LOG_TAG, "Pcm is invalid");
        return -EINVAL;
    }
    PAL_VERBOSE(LOG_TAG,"Enter flush\n");
    if (pcm && isActive()) {
        status = pcm_stop(pcm);

        if (!status)
            mState = SESSION_FLUSHED;
        else
            status = errno;
    }

    PAL_VERBOSE(LOG_TAG,"status %d\n", status);

    return status;
}

bool SessionAlsaPcm::isActive()
{
    PAL_VERBOSE(LOG_TAG, "state = %d", mState);
    return mState == SESSION_STARTED;
}

int SessionAlsaPcm::getTagsWithModuleInfo(Stream *s, size_t *size __unused, uint8_t *payload)
{
    int status = 0;
    struct pal_stream_attributes sAttr;
    int DeviceId;

    PAL_DBG(LOG_TAG,"Enter");
    status = s->getStreamAttributes(&sAttr);
    if (0 != status) {
        PAL_ERR(LOG_TAG,"getStreamAttributes Failed \n");
        return status;
    }

    if(sAttr.type == PAL_STREAM_ULTRASOUND)
       DeviceId = pcmDevTxIds.at(0);
    else
       DeviceId = pcmDevIds.at(0);

    status = SessionAlsaUtils::getTagsWithModuleInfo(mixer, DeviceId,
                                  txAifBackEnds[0].second.data(), payload);
    if (0 != status)
        PAL_ERR(LOG_TAG, "get tags failed = %d", status);

    return status;
}

void SessionAlsaPcm::adjustMmapPeriodCount(struct pcm_config *config, int32_t min_size_frames)
{
    int periodCountRequested = (min_size_frames + config->period_size - 1)
                               / config->period_size;
    int periodCount = SESSION_ALSA_MMAP_PERIOD_COUNT_MIN;

    PAL_VERBOSE(LOG_TAG, "original config.period_size = %d config.period_count = %d",
                config->period_size, config->period_count);

    while (periodCount < periodCountRequested &&
        (periodCount * 2) < SESSION_ALSA_MMAP_PERIOD_COUNT_MAX) {
        periodCount *= 2;
    }
    config->period_count = periodCount;

    PAL_VERBOSE(LOG_TAG, "requested config.period_count = %d",
                config->period_count);

}


int SessionAlsaPcm::createMmapBuffer(Stream *s, int32_t min_size_frames,
                                   struct pal_mmap_buffer *info)
{
    unsigned int offset1 = 0;
    unsigned int frames1 = 0;
    const char *step = "enter";
    uint32_t buffer_size;
    struct pcm_config config;
    struct pal_stream_attributes sAttr;
    int32_t status = 0;
    unsigned int pcm_flags = 0;
    const char *control = "getBufInfo";
    const char *stream = "PCM";
    struct mixer_ctl *ctl;
    std::ostringstream CntrlName;
    struct agm_buf_info buf_info;

    status = s->getStreamAttributes(&sAttr);
    if (status != 0) {
        PAL_ERR(LOG_TAG, "stream get attributes failed");
        return status;
    }

    if (info == NULL || !(min_size_frames > 0 && min_size_frames < INT32_MAX)) {
        PAL_ERR(LOG_TAG, "info = %p, min_size_frames = %d",
                info, min_size_frames);
        return -EINVAL;
    }

    if (!(((sAttr.type == PAL_STREAM_ULTRA_LOW_LATENCY) &&
            (sAttr.flags & PAL_STREAM_FLAG_MMAP_NO_IRQ)) ||
            (sAttr.type == PAL_STREAM_VOICE_UI) || (sAttr.type == PAL_STREAM_ACD))) {
         PAL_ERR(LOG_TAG, "called on stream type [%d] flags[%d]",
            sAttr.type, sAttr.flags);
         return -ENOSYS;
     }

    if (mState == SESSION_IDLE) {
        s->getBufInfo(&in_buf_size,&in_buf_count,&out_buf_size,&out_buf_count);
        memset(&config, 0, sizeof(config));

        switch(sAttr.direction) {
            case PAL_AUDIO_INPUT:
                pcm_flags = PCM_IN | PCM_MMAP | PCM_NOIRQ | PCM_MONOTONIC;
                config.rate = sAttr.in_media_config.sample_rate;
                config.format =
                    SessionAlsaUtils::palToAlsaFormat((uint32_t)sAttr.in_media_config.aud_fmt_id);
                config.channels = sAttr.in_media_config.ch_info.channels;
                config.period_size = SessionAlsaUtils::bytesToFrames(in_buf_size,
                    config.channels, config.format);
                config.period_count = in_buf_count;
                config.start_threshold = 0;
                config.stop_threshold = INT32_MAX;
                config.silence_threshold = 0;
                config.silence_size = 0;
                config.avail_min = config.period_size;
                break;
            case PAL_AUDIO_OUTPUT:
                pcm_flags = PCM_OUT | PCM_MMAP | PCM_NOIRQ | PCM_MONOTONIC;
                config.rate = sAttr.out_media_config.sample_rate;
                config.format =
                    SessionAlsaUtils::palToAlsaFormat((uint32_t)sAttr.out_media_config.aud_fmt_id);
                config.channels = sAttr.out_media_config.ch_info.channels;
                config.period_size = SessionAlsaUtils::bytesToFrames(out_buf_size,
                    config.channels, config.format);
                config.period_count = out_buf_count;
                config.start_threshold = config.period_size * 8;
                config.stop_threshold = INT32_MAX;
                config.silence_threshold = 0;
                config.silence_size = 0;
                config.avail_min = config.period_size;
                break;
            case PAL_AUDIO_INPUT | PAL_AUDIO_OUTPUT:
                return -EINVAL;
                break;
        }

        this->adjustMmapPeriodCount(&config, min_size_frames);

        PAL_DBG(LOG_TAG, "Opening PCM device card_id(%d) device_id(%d), channels %d",
                rm->getVirtualSndCard(), pcmDevIds.at(0), config.channels);

        pcm = pcm_open(rm->getVirtualSndCard(), pcmDevIds.at(0),
                             pcm_flags, &config);
        if (!pcm) {
            PAL_ERR(LOG_TAG, "pcm open failed");
            step = "open";
            status = errno;
            goto exit;
        }

        if (!pcm_is_ready(pcm)) {
            PAL_ERR(LOG_TAG, "pcm open not ready");
            pcm = nullptr;
            step = "open";
            status = errno;
            goto exit;
        }

         status = pcm_mmap_begin(pcm, &info->buffer, &offset1, &frames1);
         if (status < 0)  {
             step = "begin";
             goto exit;
         }

         info->flags = 0;
         info->buffer_size_frames = pcm_get_buffer_size(pcm);
         buffer_size = pcm_frames_to_bytes(pcm, info->buffer_size_frames);
         info->burst_size_frames = config.period_size;


        CntrlName << stream << pcmDevIds.at(0) << " " << control;
        ctl = mixer_get_ctl_by_name(mixer, CntrlName.str().data());
        if (!ctl) {
            PAL_ERR(LOG_TAG, "Invalid mixer control: %s\n", CntrlName.str().data());
            status = -ENOENT;
            goto exit;
        }

        //TODO call a mixer control to get the fd.
        memset(&buf_info, 0, sizeof(buf_info));
        status = mixer_ctl_get_array(ctl, (void *)&buf_info, sizeof(struct agm_buf_info));
        if (status < 0) {
            // Fall back to non exclusive mode
            info->fd = pcm_get_poll_fd(pcm);
        } else {
            info->fd = buf_info.data_buf_fd;
            //mmap_shared_memory_fd = buf_info->shared_memory_fd; // for closing later

            PAL_VERBOSE(LOG_TAG, "opened shared_memory_fd = %d",
                info->fd);

            if (buf_info.data_buf_size < buffer_size) {
                status = -EINVAL;
                step = "mmap";
                goto exit;
            }
            info->flags |= PAL_MMMAP_BUFF_FLAGS_APP_SHAREABLE;
        }
        memset(info->buffer, 0, pcm_frames_to_bytes(pcm,info->buffer_size_frames));

        status = pcm_mmap_commit(pcm, 0, SESSION_ALSA_MMAP_PERIOD_SIZE);
        if (status < 0) {
            step = "commit";
            goto exit;
        }

        //TODO
        //out->mmap_time_offset_nanos = get_mmap_out_time_offset();
        PAL_DBG(LOG_TAG, "got mmap buffer address %pK info->buffer_size_frames %d",
                info->buffer, info->buffer_size_frames);
        mState = SESSION_OPENED;
    }

 exit:
     if (status < 0) {
        if (pcm == NULL) {
            PAL_ERR(LOG_TAG,"%s - %d",step, status);
        } else {
            PAL_ERR(LOG_TAG,"%s - %d",step, status);
            if (pcm) {
                pcm_close(pcm);
                pcm = NULL;
            }
        }
     } else {
         status = 0;
     }
     return status;
 }

 int SessionAlsaPcm::GetMmapPosition(Stream *s, struct pal_mmap_position *position)
 {
    int status = 0;
    struct pal_stream_attributes sAttr;
    struct timespec ts = { 0, 0 };

#ifndef SEC_AUDIO_ADD_FOR_DEBUG
    PAL_DBG(LOG_TAG,"enter");
#endif

    if (pcm == NULL) {
        return -ENOSYS;
    }

    status = s->getStreamAttributes(&sAttr);
    if (status != 0) {
        PAL_ERR(LOG_TAG,"stream get attributes failed");
        return status;
    }

     if (position == NULL) {
         return -EINVAL;
     }

    if (!(((sAttr.type == PAL_STREAM_ULTRA_LOW_LATENCY) &&
            (sAttr.flags & PAL_STREAM_FLAG_MMAP_NO_IRQ)) ||
            (sAttr.type == PAL_STREAM_VOICE_UI) || (sAttr.type == PAL_STREAM_ACD))) {
         PAL_ERR(LOG_TAG, "called on stream type [%d] flags[%d]",
            sAttr.type, sAttr.flags);
         return -ENOSYS;
     }

     status = pcm_mmap_get_hw_ptr(pcm, (unsigned int *)&position->position_frames, &ts);
     if (status < 0) {
         status = -errno;
         PAL_ERR(LOG_TAG, "%d", status);
         return status;
     }
     position->time_nanoseconds = ts.tv_sec*1000000000LL + ts.tv_nsec
             /*+ out->mmap_time_offset_nanos*/;
#ifndef SEC_AUDIO_ADD_FOR_DEBUG
     PAL_DBG(LOG_TAG,"Exit status: %d", status);
#endif
     return status;
 }

// NOTE: only used by Voice UI for Google hotword api query
int SessionAlsaPcm::openGraph(Stream *s) {
    int status = 0;
    struct pcm_config config;
    struct pal_stream_attributes sAttr;
    std::vector<std::shared_ptr<Device>> associatedDevices;

    PAL_DBG(LOG_TAG, "Enter");
    status = s->getStreamAttributes(&sAttr);
    if (status != 0) {
        PAL_ERR(LOG_TAG, "stream get attributes failed");
        goto exit;
    }

    if (sAttr.type != PAL_STREAM_VOICE_UI) {
        PAL_ERR(LOG_TAG, "Invalid stream type %d", sAttr.type);
        status = -EINVAL;
        goto exit;
    }

    status = open(s);
    if (status != 0) {
        PAL_ERR(LOG_TAG, "Failed to open session, status = %d", status);
        goto exit;
    }

    if (mState == SESSION_IDLE) {
        s->getBufInfo(&in_buf_size,&in_buf_count,&out_buf_size,&out_buf_count);
        memset(&config, 0, sizeof(config));

        config.rate = sAttr.in_media_config.sample_rate;
        config.format =
            SessionAlsaUtils::palToAlsaFormat((uint32_t)sAttr.in_media_config.aud_fmt_id);
        config.channels = sAttr.in_media_config.ch_info.channels;
        config.period_size = SessionAlsaUtils::bytesToFrames(in_buf_size,
            config.channels, config.format);
        config.period_count = in_buf_count;
        config.start_threshold = 0;
        config.stop_threshold = 0;
        config.silence_threshold = 0;

        pcm = pcm_open(rm->getVirtualSndCard(), pcmDevIds.at(0), PCM_IN, &config);

        if (!pcm) {
            PAL_ERR(LOG_TAG, "pcm open failed");
            status = errno;
            goto exit;
        }

        if (!pcm_is_ready(pcm)) {
            PAL_ERR(LOG_TAG, "pcm open not ready");
            status = errno;
            goto exit;
        }

        mState = SESSION_OPENED;
    } else {
        PAL_ERR(LOG_TAG, "Invalid session state %d", mState);
        status = -EINVAL;
        goto exit;
    }
exit:
    PAL_DBG(LOG_TAG,"Exit status: %d", status);
    return status;
}
