/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
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
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
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
 *
 *   * Neither the name of Qualcomm Innovation Center, Inc. nor the names of its
 *     contributors may be used to endorse or promote products derived
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

#define LOG_TAG "PAL: Stream"
#include <semaphore.h>
#include "Stream.h"
#include "StreamPCM.h"
#include "StreamInCall.h"
#include "StreamCompress.h"
#include "StreamSoundTrigger.h"
#include "StreamACD.h"
#include "StreamContextProxy.h"
#include "StreamUltraSound.h"
#include "StreamSensorPCMData.h"
#include "Session.h"
#include "SessionAlsaPcm.h"
#include "ResourceManager.h"
#include "Device.h"
#include "USBAudio.h"

std::shared_ptr<ResourceManager> Stream::rm = nullptr;
std::mutex Stream::mBaseStreamMutex;
std::mutex Stream::pauseMutex;
std::condition_variable Stream::pauseCV;


void Stream::handleSoftPauseCallBack(uint64_t hdl, uint32_t event_id,
                                        void *data __unused,
                                        uint32_t event_size __unused) {

    PAL_DBG(LOG_TAG,"Event id %x ", event_id);

    if (event_id == EVENT_ID_SOFT_PAUSE_PAUSE_COMPLETE) {
        PAL_DBG(LOG_TAG, "Pause done");
        pauseCV.notify_all();
    }
}

Stream* Stream::create(struct pal_stream_attributes *sAttr, struct pal_device *dAttr,
    uint32_t noOfDevices, struct modifier_kv *modifiers, uint32_t noOfModifiers)
{
    std::lock_guard<std::mutex> lock(mBaseStreamMutex);
    Stream* stream = NULL;
    int status = 0;
    uint32_t count = 0;
    struct pal_device *palDevsAttr = nullptr;
    std::vector <Stream *> streamsToSwitch;
    struct pal_device streamDevAttr;

    PAL_DBG(LOG_TAG, "Enter.");

    if (!sAttr || ((noOfDevices > 0) && !dAttr)) {
        PAL_ERR(LOG_TAG, "Invalid input paramters");
        goto exit;
    }

    /* get RM instance */
    if (!rm) {
        rm = ResourceManager::getInstance();
        if (!rm) {
            PAL_ERR(LOG_TAG, "ResourceManager getInstance failed");
            goto exit;
        }
    }
    PAL_VERBOSE(LOG_TAG,"get RM instance success and noOfDevices %d \n", noOfDevices);

    if (sAttr->type == PAL_STREAM_NON_TUNNEL || sAttr->type == PAL_STREAM_CONTEXT_PROXY)
        goto stream_create;

    palDevsAttr = (pal_device *)calloc(noOfDevices, sizeof(struct pal_device));

    if (!palDevsAttr) {
        PAL_ERR(LOG_TAG, "palDevsAttr not created");
        goto exit;
    }
    if (sAttr->type == PAL_STREAM_VOICE_CALL_MUSIC)
        goto stream_create;
    for (int i = 0; i < noOfDevices; i++) {
        struct pal_device_info devinfo = {};
        palDevsAttr[i] = {};

        if (sAttr->type == PAL_STREAM_ULTRASOUND) {
            if (i == 0) { // first assign output device
                if (rm->IsDedicatedBEForUPDEnabled())
                    dAttr[i].id = PAL_DEVICE_OUT_ULTRASOUND;
                else
                    dAttr[i].id = PAL_DEVICE_OUT_HANDSET;
            } else { // then assign input device
                dAttr[i].id = PAL_DEVICE_IN_ULTRASOUND_MIC;
            }
        }
        if(noOfDevices > 1 && (dAttr[i].id == PAL_DEVICE_OUT_SPEAKER ||
                                dAttr[i].id == PAL_DEVICE_OUT_WIRED_HEADSET ||
                                dAttr[i].id == PAL_DEVICE_OUT_WIRED_HEADPHONE)) {
           PAL_DBG(LOG_TAG, "isComboHeadsetActive true");
           sAttr->isComboHeadsetActive = true;
        } else {
           PAL_DBG(LOG_TAG, "isComboHeadsetActive false");
           sAttr->isComboHeadsetActive = false;
        }

        //TODO: shift this to rm or somewhere else where we can read the supported config from xml
        palDevsAttr[count].id = dAttr[i].id;
        if (palDevsAttr[count].id == PAL_DEVICE_OUT_USB_DEVICE ||
            palDevsAttr[count].id == PAL_DEVICE_OUT_USB_HEADSET ||
            palDevsAttr[count].id == PAL_DEVICE_IN_USB_DEVICE ||
            palDevsAttr[count].id == PAL_DEVICE_IN_USB_HEADSET ||
            rm->isBtDevice(palDevsAttr[count].id)) {
            palDevsAttr[count].address = dAttr[i].address;
        }

        if (strlen(dAttr[i].custom_config.custom_key)) {
            strlcpy(palDevsAttr[count].custom_config.custom_key, dAttr[i].custom_config.custom_key, PAL_MAX_CUSTOM_KEY_SIZE);
            PAL_DBG(LOG_TAG, "found custom key %s", dAttr[i].custom_config.custom_key);

        } else {
            strlcpy(palDevsAttr[count].custom_config.custom_key, "", PAL_MAX_CUSTOM_KEY_SIZE);
            PAL_DBG(LOG_TAG, "no custom key found");
        }

        if (palDevsAttr[count].address.card_id != DUMMY_SND_CARD) {
            status = rm->getDeviceConfig((struct pal_device *)&palDevsAttr[count], sAttr);
            if (status) {
               PAL_ERR(LOG_TAG, "Not able to get Device config %d", status);
               goto exit;
            }
            // check if it's grouped device and group config needs to update
            rm->lockActiveStream();
            status = rm->checkAndUpdateGroupDevConfig((struct pal_device *)&palDevsAttr[count], sAttr,
                                                    streamsToSwitch, &streamDevAttr, true);
            rm->unlockActiveStream();
            if (status) {
                PAL_ERR(LOG_TAG, "no valid group device config found");
            }
        }

        count++;
    }

stream_create:
    PAL_DBG(LOG_TAG, "stream type 0x%x", sAttr->type);
    if (rm->isStreamSupported(sAttr, palDevsAttr, noOfDevices)) {
        try {
            switch (sAttr->type) {
                case PAL_STREAM_LOW_LATENCY:
                case PAL_STREAM_DEEP_BUFFER:
                case PAL_STREAM_GENERIC:
                case PAL_STREAM_VOIP_TX:
                case PAL_STREAM_VOIP_RX:
                case PAL_STREAM_PCM_OFFLOAD:
                case PAL_STREAM_VOICE_CALL:
                case PAL_STREAM_LOOPBACK:
                case PAL_STREAM_ULTRA_LOW_LATENCY:
                case PAL_STREAM_PROXY:
                case PAL_STREAM_HAPTICS:
                case PAL_STREAM_RAW:
                case PAL_STREAM_VOICE_RECOGNITION:
                    //TODO:for now keeping PAL_STREAM_PLAYBACK_GENERIC for ULLA need to check
                    stream = new StreamPCM(sAttr,
                                           palDevsAttr,
                                           noOfDevices,
                                           modifiers,
                                           noOfModifiers,
                                           rm);
                    break;
                case PAL_STREAM_COMPRESSED:
                    stream = new StreamCompress(sAttr,
                                                palDevsAttr,
                                                noOfDevices,
                                                modifiers,
                                                noOfModifiers,
                                                rm);
                    break;
                case PAL_STREAM_VOICE_UI:
                    stream = new StreamSoundTrigger(sAttr,
                                                    palDevsAttr,
                                                    noOfDevices,
                                                    modifiers,
                                                    noOfModifiers,
                                                    rm);
                    break;
                case PAL_STREAM_VOICE_CALL_RECORD:
                case PAL_STREAM_VOICE_CALL_MUSIC:
                    stream = new StreamInCall(sAttr,
                                              palDevsAttr,
                                              noOfDevices,
                                              modifiers,
                                              noOfModifiers,
                                              rm);
                    break;
                case PAL_STREAM_NON_TUNNEL:
                    stream = new StreamNonTunnel(sAttr,
                                                 NULL,
                                                 0,
                                                 modifiers,
                                                 noOfModifiers,
                                                 rm);
                    break;
                case PAL_STREAM_ACD:
                    stream = new StreamACD(sAttr,
                                           palDevsAttr,
                                           noOfDevices,
                                           modifiers,
                                           noOfModifiers,
                                           rm);
                    break;
                case PAL_STREAM_CONTEXT_PROXY:
                    stream = new StreamContextProxy(sAttr,
                                                    NULL,
                                                    0,
                                                    modifiers,
                                                    noOfModifiers,
                                                    rm);
                    break;
                case PAL_STREAM_ULTRASOUND:
                    stream = new StreamUltraSound(sAttr,
                                                  palDevsAttr,
                                                  noOfDevices,
                                                  modifiers,
                                                  noOfModifiers,
                                                  rm);
                    break;
                case PAL_STREAM_SENSOR_PCM_DATA:
                    stream = new StreamSensorPCMData(sAttr,
                                                     palDevsAttr,
                                                     noOfDevices,
                                                     modifiers,
                                                     noOfModifiers,
                                                     rm);
                    break;
                default:
                    PAL_ERR(LOG_TAG, "unsupported stream type 0x%x", sAttr->type);
                    break;
            }
        }
        catch (const std::exception& e) {
            PAL_ERR(LOG_TAG, "Stream create failed for stream type 0x%x", sAttr->type);
            if (palDevsAttr) {
                free(palDevsAttr);
            }
            throw std::runtime_error(e.what());
        }
    } else {
        PAL_ERR(LOG_TAG,"Requested config not supported");
        goto exit;
    }
exit:
    if (palDevsAttr) {
        free(palDevsAttr);
    }
    if (!stream) {
        PAL_ERR(LOG_TAG, "stream creation failed");
    }

    PAL_DBG(LOG_TAG, "Exit stream %pK create %s", stream,
            stream ? "successful" : "failed");
    return stream;
}

int32_t  Stream::getStreamAttributes(struct pal_stream_attributes *sAttr)
{
    int32_t status = 0;

    if (!sAttr || !mStreamAttr) {
        status = -EINVAL;
        PAL_ERR(LOG_TAG, "Invalid stream attribute pointer, status %d", status);
        goto exit;
    }
    ar_mem_cpy(sAttr, sizeof(pal_stream_attributes), mStreamAttr,
                     sizeof(pal_stream_attributes));
    PAL_VERBOSE(LOG_TAG, "stream_type %d stream_flags %d direction %d",
            sAttr->type, sAttr->flags, sAttr->direction);

exit:
    return status;
}

const std::string& Stream::getStreamSelector() const {
    return mStreamSelector;
}

const std::string& Stream::getDevicePPSelector() const {
    return mDevPPSelector;
}

int32_t  Stream::getModifiers(struct modifier_kv *modifiers,uint32_t *noOfModifiers)
{
    int32_t status = 0;

    if (!mModifiers || !noOfModifiers) {
        status = -EINVAL;
        PAL_ERR(LOG_TAG, "Invalid modifers pointer, status %d", status);
        goto exit;
    }
    ar_mem_cpy (modifiers, sizeof(modifier_kv), mModifiers,
                      sizeof(modifier_kv));
    *noOfModifiers = mNoOfModifiers;
    PAL_DBG(LOG_TAG, "noOfModifiers %u", *noOfModifiers);
exit:
    return status;
}

int32_t Stream::getEffectParameters(void *effect_query)
{
    int32_t status = 0;

    if (!effect_query) {
        PAL_ERR(LOG_TAG, "invalid query");
        return -EINVAL;
    }

    mStreamMutex.lock();
    if (currentState == STREAM_IDLE) {
        PAL_ERR(LOG_TAG, "Invalid stream state: IDLE");
        mStreamMutex.unlock();
        return -EINVAL;
    }
    pal_param_payload *pal_param = (pal_param_payload *)effect_query;
    effect_pal_payload_t *effectPayload = (effect_pal_payload_t *)pal_param->payload;
    status = session->getEffectParameters(this, effectPayload);
    if (status) {
       PAL_ERR(LOG_TAG, "getParameters failed with %d", status);
    }
    mStreamMutex.unlock();

    return status;
}

int32_t Stream::rwACDBParameters(void *payload, uint32_t sampleRate,
                                    bool isParamWrite)
{
    int32_t status = 0;

    mStreamMutex.lock();
    if (currentState == STREAM_IDLE) {
        PAL_ERR(LOG_TAG, "Invalid stream state: IDLE");
        mStreamMutex.unlock();
        return -EINVAL;
    }
    status = session->rwACDBParameters(payload, sampleRate, isParamWrite);
    mStreamMutex.unlock();

    return status;
}

int32_t Stream::getStreamType (pal_stream_type_t* streamType)
{
    int32_t status = 0;

    if (!streamType || !mStreamAttr) {
        status = -EINVAL;
        PAL_ERR(LOG_TAG, "Invalid stream type, status %d", status);
        goto exit;
    }
    *streamType = mStreamAttr->type;
    PAL_DBG(LOG_TAG, "streamType - %d", *streamType);

exit:
    return status;
}

int32_t Stream::getStreamDirection(pal_stream_direction_t *dir)
{
    int32_t status = 0;

    if (!dir || !mStreamAttr) {
        status = -EINVAL;
        PAL_ERR(LOG_TAG, "Invalid inputs, status %d", status);
    } else {
        *dir = mStreamAttr->direction;
        PAL_DBG(LOG_TAG, "stream direction - %d", *dir);
    }
    return status;
}

uint32_t Stream::getRenderLatency()
{
    uint32_t delayMs = 0;

    if (!mStreamAttr) {
        PAL_ERR(LOG_TAG, "invalid mStreamAttr");
        return delayMs;
    }

    switch (mStreamAttr->type) {
    case PAL_STREAM_DEEP_BUFFER:
        delayMs = PAL_DEEP_BUFFER_PLATFORM_DELAY / 1000;
        break;
    case PAL_STREAM_GENERIC:
        delayMs = PAL_GENERIC_PLATFORM_DELAY / 1000;
        break;
    case PAL_STREAM_LOW_LATENCY:
        delayMs = PAL_LOW_LATENCY_PLATFORM_DELAY / 1000;
        break;
    case PAL_STREAM_COMPRESSED:
    case PAL_STREAM_PCM_OFFLOAD:
        delayMs = PAL_PCM_OFFLOAD_PLATFORM_DELAY / 1000;
        break;
    case PAL_STREAM_ULTRA_LOW_LATENCY:
        delayMs = PAL_ULL_PLATFORM_DELAY / 1000;
        break;
    case PAL_STREAM_VOIP_RX:
        delayMs = PAL_VOIP_PLATFORM_DELAY / 1000;
        break;
    default:
        break;
    }

    return delayMs;
}

uint32_t Stream::getLatency()
{
    uint32_t latencyMs = 0;

    if (!mStreamAttr) {
        PAL_ERR(LOG_TAG, "invalid mStreamAttr");
        return latencyMs;
    }

    switch (mStreamAttr->type) {
    case PAL_STREAM_GENERIC:
        latencyMs = PAL_GENERIC_OUTPUT_PERIOD_DURATION *
            PAL_GENERIC_PLAYBACK_PERIOD_COUNT;
        break;
    case PAL_STREAM_DEEP_BUFFER:
        latencyMs = PAL_DEEP_BUFFER_OUTPUT_PERIOD_DURATION *
            PAL_DEEP_BUFFER_PLAYBACK_PERIOD_COUNT;
        break;
    case PAL_STREAM_LOW_LATENCY:
        latencyMs = PAL_LOW_LATENCY_OUTPUT_PERIOD_DURATION *
            PAL_LOW_LATENCY_PLAYBACK_PERIOD_COUNT;
        break;
    case PAL_STREAM_COMPRESSED:
    case PAL_STREAM_PCM_OFFLOAD:
        latencyMs = PAL_PCM_OFFLOAD_OUTPUT_PERIOD_DURATION *
            PAL_PCM_OFFLOAD_PLAYBACK_PERIOD_COUNT;
        break;
    case PAL_STREAM_VOIP_RX:
        latencyMs = PAL_VOIP_OUTPUT_PERIOD_DURATION *
            PAL_VOIP_PLAYBACK_PERIOD_COUNT;
        break;
    default:
        break;
    }

    latencyMs += getRenderLatency();
    return latencyMs;
}

int32_t Stream::getAssociatedDevices(std::vector <std::shared_ptr<Device>> &aDevices)
{
    int32_t status = 0;

    PAL_DBG(LOG_TAG, "no. of devices %zu", mDevices.size());
    for (int32_t i=0; i < mDevices.size(); i++) {
        aDevices.push_back(mDevices[i]);
    }

    return status;
}

void Stream::clearOutPalDevices()
{
    std::vector <struct pal_device>::iterator dIter;

    for (dIter = mPalDevice.begin(); dIter != mPalDevice.end();) {
        if (!rm->isInputDevId((*dIter).id)) {
            mPalDevice.erase(dIter);
        } else {
            dIter++;
        }
    }
}

int32_t Stream::updatePalDevice(struct pal_device *dattr, pal_device_id_t dev_id)
{
    int32_t status = 0;

    PAL_DBG(LOG_TAG, "updatePalDevice from %d to %d", dev_id, dattr->id);
    for (int i = 0; i < mPalDevice.size(); i++) {
        if (dev_id == mPalDevice[i].id) {
            mPalDevice.erase(mPalDevice.begin() + i);
            break;
        }
    }

    mPalDevice.push_back(*dattr);
    return status;
}

int32_t Stream::getAssociatedPalDevices(std::vector <struct pal_device> &palDevices)
{
    int32_t status = 0;

    PAL_DBG(LOG_TAG, "no. of palDevices %zu", mPalDevice.size());
    for (int32_t i=0; i < mPalDevice.size(); i++) {
        palDevices.push_back(mPalDevice[i]);
    }

    return status;
}

int32_t Stream::getSoundCardId()
{
    if (mPalDevice.size()) {
        PAL_DBG(LOG_TAG, "sound card id = 0x%x",
                    mPalDevice[0].address.card_id);
        return mPalDevice[0].address.card_id;
    }

    return -EINVAL;
}

int32_t Stream::getAssociatedSession(Session **s)
{
    int32_t status = 0;

    if (!s) {
        status = -EINVAL;
        PAL_ERR(LOG_TAG, "Invalid session, status %d", status);
        goto exit;
    }
    *s = session;
    PAL_DBG(LOG_TAG, "session %pK", *s);
exit:
    return status;
}

int32_t Stream::getVolumeData(struct pal_volume_data *vData)
{
    int32_t status = 0;

    if (!vData) {
        status = -EINVAL;
        PAL_INFO(LOG_TAG, "Volume Data not set yet");
        goto exit;
    }

    if (mVolumeData != NULL) {
        ar_mem_cpy(vData, sizeof(uint32_t) +
                      (sizeof(struct pal_channel_vol_kv) * (mVolumeData->no_of_volpair)),
                      mVolumeData, sizeof(uint32_t) +
                      (sizeof(struct pal_channel_vol_kv) * (mVolumeData->no_of_volpair)));

        PAL_DBG(LOG_TAG, "num config %x", (mVolumeData->no_of_volpair));
        for(int32_t i=0; i < (mVolumeData->no_of_volpair); i++) {
            PAL_VERBOSE(LOG_TAG,"Volume payload mask:%x vol:%f",
                (mVolumeData->volume_pair[i].channel_mask), (mVolumeData->volume_pair[i].vol));
        }
    } else {
        status = -EINVAL;
        PAL_ERR(LOG_TAG, "volume has not been set, status %d", status);
    }
exit:
    return status;
}

int32_t Stream::setBufInfo(pal_buffer_config *in_buffer_cfg,
                           pal_buffer_config *out_buffer_cfg)
{
    int32_t status = 0;
    struct pal_stream_attributes sattr;
    int16_t nBlockAlignIn, nBlockAlignOut ;        // block size of data

    status = getStreamAttributes(&sattr);
    /*In case of extern mem mode PAL wont get any buffer size info from clients
      If clients still call this api then it could be to notify the metadata size*/
    if (sattr.flags & PAL_STREAM_FLAG_EXTERN_MEM) {
        if (out_buffer_cfg)
            outMaxMetadataSz = out_buffer_cfg->max_metadata_size;
        if (in_buffer_cfg)
            inMaxMetadataSz = in_buffer_cfg->max_metadata_size;
         /*in EXTERN_MEM mode set buf count and size to 0*/
         inBufCount = 0;
         inBufSize = 0;
         outBufCount = 0;
         outBufSize = 0;
    } else {
        if (in_buffer_cfg) {
            PAL_DBG(LOG_TAG, "In Buffer size %zu, In Buffer count %zu metadata sz %zu",
                    in_buffer_cfg->buf_size, in_buffer_cfg->buf_count, in_buffer_cfg->max_metadata_size);
            inBufCount = in_buffer_cfg->buf_count;
            inMaxMetadataSz = in_buffer_cfg->max_metadata_size;
        }
        if (out_buffer_cfg) {
            PAL_DBG(LOG_TAG, "Out Buffer size %zu and Out Buffer count %zu metaData sz %zu",
                    out_buffer_cfg->buf_size, out_buffer_cfg->buf_count, out_buffer_cfg->max_metadata_size);
            outBufCount = out_buffer_cfg->buf_count;
            outMaxMetadataSz = out_buffer_cfg->max_metadata_size;
        }

        if (sattr.direction == PAL_AUDIO_OUTPUT) {
            if(!out_buffer_cfg) {
               status = -EINVAL;
               PAL_ERR(LOG_TAG, "Invalid output buffer size status %d", status);
               goto exit;
            }
            outBufSize = out_buffer_cfg->buf_size;
            nBlockAlignOut = ((sattr.out_media_config.bit_width) / 8) *
                          (sattr.out_media_config.ch_info.channels);
            PAL_DBG(LOG_TAG, "no of buf %zu and send buf %zu", outBufCount, outBufSize);

            //If the read size is not a multiple of BlockAlign;
            //Make sure we read blockaligned bytes from the file.
            if ((outBufSize % nBlockAlignOut) != 0) {
                outBufSize = ((outBufSize / nBlockAlignOut) * nBlockAlignOut);
            }
            out_buffer_cfg->buf_size = outBufSize;

        } else if (sattr.direction == PAL_AUDIO_INPUT) {
            if (!in_buffer_cfg) {
                status = -EINVAL;
                PAL_ERR(LOG_TAG, "Invalid input buffer size status %d", status);
                goto exit;
            }
            inBufSize = in_buffer_cfg->buf_size;
            //inBufSize = (sattr->in_media_config.bit_width) * (sattr->in_media_config.ch_info.channels) * 32;
            nBlockAlignIn = ((sattr.in_media_config.bit_width) / 8) *
                          (sattr.in_media_config.ch_info.channels);
            //If the read size is not a multiple of BlockAlign;
            //Make sure we read blockaligned bytes from the file.
            if ((inBufSize % nBlockAlignIn) != 0) {
                inBufSize = ((inBufSize / nBlockAlignIn) * nBlockAlignIn);
            }
            in_buffer_cfg->buf_size = inBufSize;
        } else {
            if (!in_buffer_cfg || !out_buffer_cfg) {
                status = -EINVAL;
                PAL_ERR(LOG_TAG, "Invalid buffer size status %d", status);
                goto exit;
            }
            outBufSize = out_buffer_cfg->buf_size;
            nBlockAlignOut = ((sattr.out_media_config.bit_width) / 8) *
                          (sattr.out_media_config.ch_info.channels);
            PAL_DBG(LOG_TAG, "no of buf %zu and send buf %zu", outBufCount, outBufSize);

            //If the read size is not a multiple of BlockAlign;
            //Make sure we read blockaligned bytes from the file.
            if ((outBufSize % nBlockAlignOut) != 0) {
                outBufSize = ((outBufSize / nBlockAlignOut) * nBlockAlignOut);
            }
            out_buffer_cfg->buf_size = outBufSize;

            inBufSize = in_buffer_cfg->buf_size;
            nBlockAlignIn = ((sattr.in_media_config.bit_width) / 8) *
                          (sattr.in_media_config.ch_info.channels);
            //If the read size is not a multiple of BlockAlign;
            //Make sure we read blockaligned bytes from the file.
            if ((inBufSize % nBlockAlignIn) != 0) {
                inBufSize = ((inBufSize / nBlockAlignIn) * nBlockAlignIn);
            }
            in_buffer_cfg->buf_size = inBufSize;
        }
    }
exit:
    return status;
}

int32_t Stream::getMaxMetadataSz(size_t *in_max_metdata_sz, size_t *out_max_metadata_sz)
{
    int32_t status = 0;

    if (in_max_metdata_sz)
        *in_max_metdata_sz = inMaxMetadataSz;
    if (out_max_metadata_sz)
        *out_max_metadata_sz = outMaxMetadataSz;

    if (in_max_metdata_sz)
        PAL_DBG(LOG_TAG, "in max metadataSize %zu",
                *in_max_metdata_sz);

    if (out_max_metadata_sz)
        PAL_DBG(LOG_TAG, "in max metadataSize %zu",
                *out_max_metadata_sz);

    return status;
}

int32_t Stream::getBufInfo(size_t *in_buf_size, size_t *in_buf_count,
                           size_t *out_buf_size, size_t *out_buf_count)
{
    int32_t status = 0;

    if (in_buf_size)
        *in_buf_size = inBufSize;
    if (in_buf_count)
        *in_buf_count = inBufCount;
    if (out_buf_size)
        *out_buf_size = outBufSize;
    if (out_buf_count)
        *out_buf_count = outBufCount;

    if (in_buf_size && in_buf_count)
        PAL_DBG(LOG_TAG, "In Buffer size %zu, In Buffer count %zu",
                *in_buf_size, *in_buf_count);

    if (out_buf_size && out_buf_count)
        PAL_DBG(LOG_TAG, "Out Buffer size %zu and Out Buffer count %zu",
                *out_buf_size, *out_buf_count);

    return status;
}

bool Stream::isStreamAudioOutFmtSupported(pal_audio_fmt_t format)
{
    switch (format) {
    case PAL_AUDIO_FMT_PCM_S8:
    case PAL_AUDIO_FMT_PCM_S16_LE:
    case PAL_AUDIO_FMT_PCM_S24_3LE:
    case PAL_AUDIO_FMT_PCM_S24_LE:
    case PAL_AUDIO_FMT_PCM_S32_LE:
    case PAL_AUDIO_FMT_MP3:
    case PAL_AUDIO_FMT_AAC:
    case PAL_AUDIO_FMT_AAC_ADTS:
    case PAL_AUDIO_FMT_AAC_ADIF:
    case PAL_AUDIO_FMT_AAC_LATM:
    case PAL_AUDIO_FMT_WMA_STD:
    case PAL_AUDIO_FMT_ALAC:
    case PAL_AUDIO_FMT_APE:
    case PAL_AUDIO_FMT_WMA_PRO:
    case PAL_AUDIO_FMT_FLAC:
    case PAL_AUDIO_FMT_VORBIS:
    case PAL_AUDIO_FMT_AMR_NB:
    case PAL_AUDIO_FMT_AMR_WB:
    case PAL_AUDIO_FMT_AMR_WB_PLUS:
    case PAL_AUDIO_FMT_EVRC:
    case PAL_AUDIO_FMT_G711:
    case PAL_AUDIO_FMT_QCELP:
        return true;
    default:
        return false;
    }
}

int32_t Stream::getTimestamp(struct pal_session_time *stime)
{
    int32_t status = 0;
    if (!stime) {
        status = -EINVAL;
        PAL_ERR(LOG_TAG, "Invalid session time pointer, status %d", status);
        goto exit;
    }
    if (rm->cardState == CARD_STATUS_OFFLINE) {
        status = -EINVAL;
        PAL_ERR(LOG_TAG, "Sound card offline, status %d", status);
        goto exit;
    }
    status = session->getTimestamp(stime);
    if (0 != status) {
        PAL_ERR(LOG_TAG, "Failed to get session timestamp status %d", status);
        if (errno == -ENETRESET &&
            rm->cardState != CARD_STATUS_OFFLINE) {
            PAL_ERR(LOG_TAG, "Sound card offline, informing RM");
            rm->ssrHandler(CARD_STATUS_OFFLINE);
            status = -EINVAL;
        }
    }
exit:
    return status;
}

int32_t Stream::handleBTDeviceNotReady(bool& a2dpSuspend)
{
    int32_t status = 0;
    struct pal_device dattr = {};
    struct pal_device spkrDattr = {};
    struct pal_device handsetDattr = {};
    std::shared_ptr<Device> dev = nullptr;
    std::shared_ptr<Device> spkrDev = nullptr;
    std::shared_ptr<Device> handsetDev = nullptr;
    pal_param_bta2dp_t *param_bt_a2dp = nullptr;
    std::vector <Stream *> activeStreams;

    a2dpSuspend = false;

    /* SCO device is not ready */
    if (rm->isDeviceAvailable(mDevices, PAL_DEVICE_OUT_BLUETOOTH_SCO) &&
        !rm->isDeviceReady(PAL_DEVICE_OUT_BLUETOOTH_SCO)) {
        // If it's sco + speaker combo device, route to speaker.
        // Otherwise, return -EAGAIN.
        if (rm->isDeviceAvailable(mDevices, PAL_DEVICE_OUT_SPEAKER)) {
            PAL_INFO(LOG_TAG, "BT SCO output device is not ready, route to speaker");
            for (auto iter = mDevices.begin(); iter != mDevices.end();) {
                if ((*iter)->getSndDeviceId() == PAL_DEVICE_OUT_SPEAKER) {
                    iter++;
                    continue;
                }

                // Invoke session API to explicitly update the device metadata
                rm->lockGraph();
                status = session->disconnectSessionDevice(this, mStreamAttr->type, (*iter));
                if (0 != status) {
                    PAL_ERR(LOG_TAG, "disconnectSessionDevice failed:%d", status);
                    rm->unlockGraph();
                    goto exit;
                }

                status = (*iter)->close();
                if (0 != status) {
                    PAL_ERR(LOG_TAG, "device close failed with status %d", status);
                    rm->unlockGraph();
                    goto exit;
                }
                rm->unlockGraph();
                iter = mDevices.erase(iter);
            }
        } else {
            PAL_ERR(LOG_TAG, "BT SCO output device is not ready");
            status = -EAGAIN;
            goto exit;
        }
    }

    /* A2DP device is not ready */
    if (rm->isDeviceAvailable(mDevices, PAL_DEVICE_OUT_BLUETOOTH_A2DP)) {
        dattr.id = PAL_DEVICE_OUT_BLUETOOTH_A2DP;
        dev = Device::getInstance(&dattr, rm);
        if (!dev) {
            status = -ENODEV;
            PAL_ERR(LOG_TAG, "failed to get a2dp device object");
            goto exit;
        }
        dev->getDeviceParameter(PAL_PARAM_ID_BT_A2DP_SUSPENDED,
                        (void **)&param_bt_a2dp);
        if (param_bt_a2dp->a2dp_suspended == false) {
            PAL_DBG(LOG_TAG, "BT A2DP output device is good to go");
            goto exit;
        }

        if (rm->isDeviceAvailable(mDevices, PAL_DEVICE_OUT_SPEAKER)) {
            // If it's a2dp + speaker combo device, route to speaker.
            PAL_INFO(LOG_TAG, "BT A2DP output device is not ready, route to speaker");
            for (auto iter = mDevices.begin(); iter != mDevices.end();) {
                if ((*iter)->getSndDeviceId() == PAL_DEVICE_OUT_SPEAKER) {
                    iter++;
                    continue;
                }

                rm->lockGraph();
                status = session->disconnectSessionDevice(this, mStreamAttr->type, (*iter));
                if (0 != status) {
                    PAL_ERR(LOG_TAG, "disconnectSessionDevice failed:%d", status);
                    rm->unlockGraph();
                    goto exit;
                }

                status = (*iter)->close();
                if (0 != status) {
                    PAL_ERR(LOG_TAG, "device close failed with status %d", status);
                    rm->unlockGraph();
                    goto exit;
                }
                rm->unlockGraph();
                iter = mDevices.erase(iter);
            }
        } else {
            // For non-combo device, mute the stream and route to speaker or handset
            PAL_INFO(LOG_TAG, "BT A2DP output device is not ready");

            // Mark the suspendedDevIds state early - As a2dpResume may happen during this time.
            a2dpSuspend = true;
            suspendedDevIds.clear();
            suspendedDevIds.push_back(PAL_DEVICE_OUT_BLUETOOTH_A2DP);

            for (int i = 0; i < mDevices.size(); i++) {
                rm->lockGraph();
                status = session->disconnectSessionDevice(this, mStreamAttr->type, mDevices[i]);
                if (0 != status) {
                    PAL_ERR(LOG_TAG, "disconnectSessionDevice failed:%d", status);
                    rm->unlockGraph();
                    goto exit;
                }
                status = mDevices[i]->close();
                if (0 != status) {
                    PAL_ERR(LOG_TAG, "device close failed with status %d", status);
                    rm->unlockGraph();
                    goto exit;
                }
                rm->unlockGraph();
            }
            mDevices.clear();

            /* Check whether there's active stream associated with handset or speaker
             * - Device selected to switch by default is speaker.
             * - Check handset as well if no stream on speaker.
             */
            // NOTE: lockGraph is not intended for speaker or handset device
            spkrDattr.id = PAL_DEVICE_OUT_SPEAKER;
            spkrDev = Device::getInstance(&spkrDattr , rm);
            handsetDattr.id = PAL_DEVICE_OUT_HANDSET;
            handsetDev = Device::getInstance(&handsetDattr , rm);
            if (!spkrDev || !handsetDev) {
                status = -ENODEV;
                PAL_ERR(LOG_TAG, "Failed to get speaker or handset instance");
                goto exit;
            }

            dattr.id = spkrDattr.id;
            dev = spkrDev;

            rm->getActiveStream_l(activeStreams, spkrDev);
            if (activeStreams.empty()) {
                rm->getActiveStream_l(activeStreams, handsetDev);
                if (!activeStreams.empty()) {
                    // active streams found on handset
                    dattr.id = PAL_DEVICE_OUT_HANDSET;
                    dev = handsetDev;
                } else {
                    // no active stream found on both speaker and handset, get the deafult
                    pal_device_info devInfo;
                    memset(&devInfo, 0, sizeof(pal_device_info));
                    devInfo.priority = MIN_USECASE_PRIORITY;
                    status = rm->getDeviceConfig(&dattr, NULL);
                    if (!status) {
                        // get the default device info and update snd name
                        rm->getDeviceInfo(dattr.id, (pal_stream_type_t)0,
                                dattr.custom_config.custom_key, &devInfo);
                        rm->updateSndName(dattr.id, devInfo.sndDevName);
                    }
                    dev->setDeviceAttributes(dattr);
                }
            }

            PAL_INFO(LOG_TAG, "mute stream and route to device %d", dattr.id);

            status = dev->open();
            if (0 != status) {
                PAL_ERR(LOG_TAG, "device open failed with status %d", status);
                goto exit;
            }

            mDevices.push_back(dev);
            status = session->setupSessionDevice(this, mStreamAttr->type, dev);
            if (0 != status) {
                PAL_ERR(LOG_TAG, "setupSessionDevice failed:%d", status);
                dev->close();
                mDevices.pop_back();
                goto exit;
            }

            status = session->connectSessionDevice(this, mStreamAttr->type, dev);
            if (0 != status) {
                PAL_ERR(LOG_TAG, "connectSessionDevice failed:%d", status);
                dev->close();
                mDevices.pop_back();
                goto exit;
            }
            dev->getDeviceAttributes(&dattr);
            updatePalDevice(&dattr, dattr.id);
        }
    }

exit:
    return status;
}

int32_t Stream::disconnectStreamDevice(Stream* streamHandle, pal_device_id_t dev_id)
{
    int32_t status = 0;
    mStreamMutex.lock();
    status = disconnectStreamDevice_l(streamHandle, dev_id);
    mStreamMutex.unlock();

    return status;
}

int32_t Stream::disconnectStreamDevice_l(Stream* streamHandle, pal_device_id_t dev_id)
{
    int32_t status = 0;

    if (currentState == STREAM_IDLE) {
        PAL_DBG(LOG_TAG, "stream is in %d state, no need to switch device", currentState);
        status = 0;
        goto exit;
    }

    // Stream does not know if the same device is being used by other streams or not
    // So if any other streams are using the same device that has to be handled outside of stream
    // resouce manager ??
    for (int i = 0; i < mDevices.size(); i++) {
        if (dev_id == mDevices[i]->getSndDeviceId()) {
            PAL_DBG(LOG_TAG, "device %d name %s, going to stop",
                mDevices[i]->getSndDeviceId(), mDevices[i]->getPALDeviceName().c_str());
            if (currentState != STREAM_STOPPED && rm->isDeviceActive_l(mDevices[i], this)) {
                rm->deregisterDevice(mDevices[i], this);
            }
            rm->lockGraph();
            status = session->disconnectSessionDevice(streamHandle, mStreamAttr->type, mDevices[i]);
            if (0 != status) {
                PAL_ERR(LOG_TAG, "disconnectSessionDevice failed:%d", status);
                rm->unlockGraph();
                goto exit;
            }
            if (currentState != STREAM_INIT && currentState != STREAM_STOPPED) {
                status = mDevices[i]->stop();
                if (0 != status) {
                    PAL_ERR(LOG_TAG, "device stop failed with status %d", status);
                    rm->unlockGraph();
                    goto exit;
                }
            }
            rm->unlockGraph();

            status = mDevices[i]->close();
            if (0 != status) {
                PAL_ERR(LOG_TAG, "device close failed with status %d", status);
                goto exit;
            }
            mDevices.erase(mDevices.begin() + i);
            break;
        }
    }

exit:
    return status;
}

int32_t Stream::connectStreamDevice(Stream* streamHandle, struct pal_device *dattr)
{
    int32_t status = 0;
    mStreamMutex.lock();
    status = connectStreamDevice_l(streamHandle, dattr);
    mStreamMutex.unlock();

    return status;
}

int32_t Stream::connectStreamDevice_l(Stream* streamHandle, struct pal_device *dattr)
{
    int32_t status = 0;
    std::shared_ptr<Device> dev = nullptr;
    std::string newBackEndName;
    std::string curBackEndName;

    if (!dattr) {
        PAL_ERR(LOG_TAG, "invalid params");
        status = -EINVAL;
        goto exit;
    }

    dev = Device::getInstance(dattr, rm);
    if (!dev) {
        PAL_ERR(LOG_TAG, "Device creation failed");
        goto exit;
    }

    dev->setDeviceAttributes(*dattr);

    if (currentState == STREAM_IDLE) {
        PAL_DBG(LOG_TAG, "stream is in %d state, no need to switch device", currentState);
        status = 0;
        goto exit;
    }

    /* Avoid stream connecting to devices sharing the same backend.
     * - For A2DP streams may play on combo devices like Speaker and A2DP.
     *   However, if a2dp suspend is called, all streams on a2dp will temporarily
     *   move to speaker. If the stream is already connected to speaker, speaker
     *   will be connected twice.
     * - For multi-recording stream connecting to bt-sco-mic and handset-mic,
     *   if a2dp suspend arrives, stream will switch from bt-sco-mic to speaker-mic.
     *   Hence, both speaker-mic and handset-mic will be enabled.
     */
    rm->getBackendName(dev->getSndDeviceId(), newBackEndName);
    for (auto iter = mDevices.begin(); iter != mDevices.end(); iter++) {
        rm->getBackendName((*iter)->getSndDeviceId(), curBackEndName);
        if (newBackEndName == curBackEndName) {
            PAL_INFO(LOG_TAG,
                "stream is already connected to device %d name %s - return",
                dev->getSndDeviceId(), dev->getPALDeviceName().c_str());
            status = 0;
            goto exit;
        }
    }

    PAL_DBG(LOG_TAG, "device %d name %s, going to start",
        dev->getSndDeviceId(), dev->getPALDeviceName().c_str());

    status = dev->open();
    if (0 != status) {
        PAL_ERR(LOG_TAG, "device %d open failed with status %d",
            dev->getSndDeviceId(), status);
        goto exit;
    }

    mDevices.push_back(dev);
    status = session->setupSessionDevice(streamHandle, mStreamAttr->type, dev);
    if (0 != status) {
        PAL_ERR(LOG_TAG, "setupSessionDevice for %d failed with status %d",
                dev->getSndDeviceId(), status);
        goto dev_close;
    }

    rm->lockGraph();
    if (currentState != STREAM_INIT && currentState != STREAM_STOPPED) {
        status = dev->start();
        if (0 != status) {
            PAL_ERR(LOG_TAG, "device %d name %s, start failed with status %d",
                dev->getSndDeviceId(), dev->getPALDeviceName().c_str(), status);
            rm->unlockGraph();
            goto dev_close;
        }
    }
    status = session->connectSessionDevice(streamHandle, mStreamAttr->type, dev);
    if (0 != status) {
        PAL_ERR(LOG_TAG, "connectSessionDevice failed:%d", status);
        rm->unlockGraph();
        goto dev_stop;
    }
    rm->unlockGraph();
    if (currentState != STREAM_STOPPED && !rm->isDeviceActive_l(dev, this)) {
        rm->registerDevice(dev, this);
    }
    goto exit;

dev_stop:
    dev->stop();

dev_close:
    /* Do not pop the current device from stream, if session connect failed due to SSR down
     * event so that when SSR is up that device will be associated to stream.
     */
    if (status != -ENETRESET) {
        mDevices.pop_back();
    }
    dev->close();

exit:
    /* check if USB is not available restore to default device */
    if (dev && status && (dev->getSndDeviceId() == PAL_DEVICE_OUT_USB_HEADSET ||
                   dev->getSndDeviceId() == PAL_DEVICE_IN_USB_HEADSET)) {
       if (USB::isUsbConnected(dattr->address)) {
           PAL_ERR(LOG_TAG, "USB still connected, connect failed");
       } else {
           status = -ENOSYS;
           PAL_ERR(LOG_TAG, "failed to connect to USB device");
       }

    }
    return status;
}

/*
  legend:
  s1 - current stream
  s2 - existing stream
  d1, d11, d2 - PAL_DEVICE enums
  B1, B2 - backend strings

case 1
  a) s1:  d1(B1) ->  d2(B2)
     OR
  b) s1:  d1(B1) ->  d2(B1)

  resolution: tear down d1 and setup d2.

case 2
  s1:  d1(B1) ->  d1(B1) , d2(B2)

  resolution: check if d1 needs device switch by calling isDeviceSwitchRequired.
    If needed, tear down and setup d1 again. Also setup d2.

case 3:
  s1:  d1(B1), d2(B2) ->  d1(B1)

  resolution: check if d1 needs device switch by calling isDeviceSwitchRequired.
    If needed, tear down and setup d1 again. Also teardown d2.

case 4
  s2->dev d1(B1)
  s1: d2(B2) -> d1(B1)

  resolution: check if s2 is voice call stream, then use same device attribute
    for s1. Else check if isDeviceSwitchRequired() returns true for s2 d1.
    If yes, tear down and setup d1 again for s2. Also, setup d1 for s2.

case 5
  s2->dev d2(B1)
  s1: d11(B2) -> d1(B1)
  resolution: If s2 is voice call stream, then setup d2 for s1 instead of d1
    and do nothing for s2. Else, tear down d2 from s2 and setup d1 for both
    s1 and s2.

case 6
  s2->dev d2(B2)
  s1: d2(B2) -> d1(B2)
  resolution: same handling as case 5

case 7
  s2->dev d1(B1), d2(B2)
  s1: d1(B1) -> d2(B2)

  resolution: nothing to be done for s2+d1. For s2+d2, it is
    same as case 4.

*/
int32_t Stream::switchDevice(Stream* streamHandle, uint32_t numDev, struct pal_device *newDevices)
{
    int32_t status = 0;
    int32_t connectCount = 0, disconnectCount = 0;
    bool isNewDeviceA2dp = false;
    bool isCurDeviceA2dp = false;
    bool isCurrentDeviceProxyOut = false;
    bool isCurrentDeviceDpOut = false;
    bool matchFound = false;
    bool voice_call_switch = false;
    uint32_t curDeviceSlots[PAL_DEVICE_IN_MAX], newDeviceSlots[PAL_DEVICE_IN_MAX];
    std::vector <std::tuple<Stream *, uint32_t>> streamDevDisconnect, sharedBEStreamDev;
    std::vector <std::tuple<Stream *, struct pal_device *>> StreamDevConnect;
    struct pal_device dAttr;
    pal_stream_type_t type;
    struct pal_device_info deviceInfo;
    uint32_t temp_prio = MIN_USECASE_PRIORITY;
    pal_stream_attributes strAttr;
    char CurrentSndDeviceName[DEVICE_NAME_MAX_SIZE] = {0};
    std::vector <Stream *> streamsToSwitch;
    struct pal_device streamDevAttr;
    std::vector <Stream*>::iterator sIter;
    bool VoiceorVoip_call_active = false;
    bool has_out_device = false, has_in_device = false;
    std::vector <struct pal_device>::iterator dIter;
    struct pal_volume_data *volume = NULL;
    pal_device_id_t newBtDevId;
    bool isBtReady = false;

    rm->lockActiveStream();
    mStreamMutex.lock();

    if ((numDev == 0) || (numDev > PAL_DEVICE_IN_MAX) || (!newDevices) || (!streamHandle)) {
        PAL_ERR(LOG_TAG, "invalid param for device switch");
        mStreamMutex.unlock();
        rm->unlockActiveStream();
        return -EINVAL;
    }

    if (rm->cardState == CARD_STATUS_OFFLINE) {
        PAL_ERR(LOG_TAG, "Sound card offline");
        mStreamMutex.unlock();
        rm->unlockActiveStream();
        return 0;
    }

    streamHandle->getStreamAttributes(&strAttr);

    for (int i = 0; i < mDevices.size(); i++) {
        pal_device_id_t curDevId = (pal_device_id_t)mDevices[i]->getSndDeviceId();
        uint32_t tmp = numDev;

        mDevices[i]->getDeviceAttributes(&dAttr);
        if (curDevId == PAL_DEVICE_OUT_BLUETOOTH_A2DP)
            isCurDeviceA2dp = true;

        if (curDevId == PAL_DEVICE_OUT_PROXY)
            isCurrentDeviceProxyOut = true;

        if (curDevId == PAL_DEVICE_OUT_AUX_DIGITAL ||
            curDevId == PAL_DEVICE_OUT_AUX_DIGITAL_1 ||
            curDevId == PAL_DEVICE_OUT_HDMI)
            isCurrentDeviceDpOut = true;

        /* If stream is currently running on same device, then check if
         * it needs device switch. If not needed, then do not add it to
         * streamDevConnect and streamDevDisconnect list. This handles case 2,3.
         */
        matchFound = false;
        for (int j = 0; j < tmp; j++) {
            if (curDevId == newDevices[j].id) {
                matchFound = true;
            }
        }
        /* If mDevice[i] does not match with any of new device,
           then update disconnect list */
        if (matchFound == false) {
            curDeviceSlots[disconnectCount] = i;
            disconnectCount++;
        }
    }

    /* remove members of mPalDevices which has same dir with new devices*/
    for (int i = 0; i < numDev; i++) {
        if (rm->isOutputDevId(newDevices[i].id))
            has_out_device = true;
        if (rm->isInputDevId(newDevices[i].id))
            has_in_device = true;
    }
    for (dIter = mPalDevice.begin(); dIter != mPalDevice.end(); ) {
        if (rm->isOutputDevId((*dIter).id) && has_out_device) {
            dIter = mPalDevice.erase(dIter);
        } else if (rm->isInputDevId((*dIter).id) && has_in_device) {
            dIter = mPalDevice.erase(dIter);
        } else {
            dIter++;
        }
    }

    for (int i = 0; i < numDev; i++) {
        struct pal_device_info devinfo = {};
        bool devReadyStatus = 0;
        uint32_t retryCnt = 20;
        uint32_t retryPeriodMs = 100;
        pal_param_bta2dp_t* param_bt_a2dp = nullptr;
        std::shared_ptr<Device> dev = nullptr;

        /*
         * When A2DP, Out Proxy and DP device is disconnected the
         * music playback is paused and the policy manager sends routing=0
         * But the audioflinger continues to write data until standby time
         * (3sec). As BT is turned off, the write gets blocked.
         * Avoid this by routing audio to speaker until standby.
         */
        // This assumes that PAL_DEVICE_NONE comes as single device
        if ((newDevices[i].id == PAL_DEVICE_NONE) &&
            (((isCurDeviceA2dp == true) && !rm->isDeviceReady(PAL_DEVICE_OUT_BLUETOOTH_A2DP)) ||
             (isCurrentDeviceProxyOut) || (isCurrentDeviceDpOut))) {
            newDevices[i].id = PAL_DEVICE_OUT_SPEAKER;

            if (rm->getDeviceConfig(&newDevices[i], mStreamAttr)) {
                continue;
            }
        }

        if (newDevices[i].id == PAL_DEVICE_NONE) {
            mStreamMutex.unlock();
            rm->unlockActiveStream();
            return 0;
        }
        /* Retry isDeviceReady check is required for BT devices only.
        *  In case of BT disconnection event from BT stack, if stream
        *  is still associated with BT but the BT device is not in
        *  ready state, explicit dev switch from APM to BT keep on retrying
        *  for 2 secs causing audioserver to stuck for processing
        *  disconnection. Thus check for isCurDeviceA2dp and a2dp_suspended
        *  state to avoid unnecessary sleep over 2 secs.
        */
        if (newDevices[i].id == PAL_DEVICE_OUT_BLUETOOTH_A2DP) {
            isNewDeviceA2dp = true;
            newBtDevId = newDevices[i].id;
            dev = Device::getInstance(&newDevices[i], rm);
            if (!dev) {
                status = -ENODEV;
                PAL_ERR(LOG_TAG, "failed to get a2dp device object");
                goto done;
            }
            dev->getDeviceParameter(PAL_PARAM_ID_BT_A2DP_SUSPENDED,
                (void**)&param_bt_a2dp);

            if (!param_bt_a2dp->a2dp_suspended) {
                while (!devReadyStatus && --retryCnt) {
                    devReadyStatus = rm->isDeviceReady(newDevices[i].id);
                    if (devReadyStatus) {
                        isBtReady = true;
                        break;
                    } else if (isCurDeviceA2dp) {
                        break;
                    }

                    usleep(retryPeriodMs * 1000);
                }
            }
        } else {
            devReadyStatus = rm->isDeviceReady(newDevices[i].id);
        }

        if (!devReadyStatus) {
            PAL_ERR(LOG_TAG, "Device %d is not ready", newDevices[i].id);
            if ((newDevices[i].id == PAL_DEVICE_OUT_BLUETOOTH_A2DP) &&
                !(rm->isDeviceAvailable(newDevices, numDev, PAL_DEVICE_OUT_SPEAKER))) {
                /* update suspended device to a2dp and don't route as BT returned error
                 * However it is still possible a2dp routing called as part of a2dp restore
                 */
                PAL_ERR(LOG_TAG, "A2DP profile is not ready, ignoring routing request");
                suspendedDevIds.clear();
                suspendedDevIds.push_back(PAL_DEVICE_OUT_BLUETOOTH_A2DP);
            }
        } else {
            newDeviceSlots[connectCount] = i;
            connectCount++;
        }

        /* store or update palDev before newDevices can be changed */
        updatePalDevice(&(newDevices[i]), newDevices[i].id);
    }

    /*  No new device is ready */
    if ((numDev != 0) && (connectCount == 0)) {
        PAL_INFO(LOG_TAG, "No new device is ready to connect");
        mStreamMutex.unlock();
        rm->unlockActiveStream();
        return 0;
    }

    PAL_INFO(LOG_TAG,"number of active devices %zu, new devices %d", mDevices.size(), connectCount);

    /* created stream device connect and disconnect list */
    streamDevDisconnect.clear();
    StreamDevConnect.clear();

    for (int i = 0; i < connectCount; i++) {
        std::vector <Stream *> activeStreams;
        std::shared_ptr<Device> dev = nullptr;
        pal_device_id_t newDeviceId = newDevices[newDeviceSlots[i]].id;

        sharedBEStreamDev.clear();
        // get active stream device pairs sharing the same backend with new devices.
        rm->getSharedBEActiveStreamDevs(sharedBEStreamDev, newDeviceId);

        streamHandle->getStreamType(&type);

        rm->getDeviceInfo((pal_device_id_t)newDeviceId,
                          type,newDevices[newDeviceSlots[i]].custom_config.custom_key,
                          &deviceInfo);
        PAL_DBG(LOG_TAG,"newDeviceId %d connectCount:%d ",newDeviceId,connectCount);
        if(connectCount > 1 && (newDeviceId == PAL_DEVICE_OUT_SPEAKER ||
                                newDeviceId == PAL_DEVICE_OUT_WIRED_HEADSET ||
                                newDeviceId == PAL_DEVICE_OUT_WIRED_HEADPHONE)) {
           PAL_DBG(LOG_TAG, "isComboHeadsetActive true");
           strAttr.isComboHeadsetActive = true;
        } else {
           PAL_DBG(LOG_TAG, "isComboHeadsetActive false");
           strAttr.isComboHeadsetActive = false;
        }
        /* This can result in below situation:
         * 1) No matching SharedBEStreamDev (handled in else part).
         *    e.g. - case 1a, 2.
         * 2) sharedBEStreamDev having same stream as current stream but different device id.
         *    e.g. case 1b.
         * 3) sharedBEStreamDev having different stream from current but same id as current id.
         *    e.g. - case 4,7.
         * 4) sharedBEStreamDev having different stream from current and different device id.
         *    e.g. case 5,6.
         * 5) sharedBEStreamDev having same stream as current stream and same device id.
         *    e.g. spkr -> spkr + hs. This will only come in list if speaker needs device switch.
         * This list excludes same stream and same id which does not need device switch as it
         * is removed above.
         */
        if (sharedBEStreamDev.size() > 0) {
            for (const auto &elem : sharedBEStreamDev) {
                struct pal_stream_attributes strAttr;
                std::get<0>(elem)->getStreamAttributes(&strAttr);
                if (strAttr.type == PAL_STREAM_VOIP ||
                    strAttr.type == PAL_STREAM_VOIP_RX ||
                    strAttr.type == PAL_STREAM_VOIP_TX ||
                    strAttr.type == PAL_STREAM_VOICE_CALL) {
                    VoiceorVoip_call_active = true;
                    break;
                }
            }
            rm->getSndDeviceName(newDeviceId, CurrentSndDeviceName);
            // update device attr based on prio
            rm->updatePriorityAttr(newDeviceId,
                                   sharedBEStreamDev,
                                   &(newDevices[newDeviceSlots[i]]),
                                   &strAttr);
            for (const auto &elem : sharedBEStreamDev) {
                struct pal_stream_attributes sAttr;
                Stream *sharedStream = std::get<0>(elem);
                struct pal_device curDevAttr;
                std::shared_ptr<Device> curDev = nullptr;
                bool custom_switch = false;

                curDevAttr.id = (pal_device_id_t)std::get<1>(elem);
                curDev = Device::getInstance(&curDevAttr, rm);
                if (!curDev) {
                    PAL_ERR(LOG_TAG, "Getting Device instance failed");
                    continue;
                }
                curDev->getDeviceAttributes(&curDevAttr);

                /* avoid device for voice/voip being switched by low priority switch*/
                if (VoiceorVoip_call_active &&
                    strAttr.type != PAL_STREAM_VOICE_CALL &&
                    strAttr.type != PAL_STREAM_VOIP_RX &&
                    strAttr.type != PAL_STREAM_VOIP_TX &&
                    strAttr.type != PAL_STREAM_VOIP &&
                    curDevAttr.id != newDevices[newDeviceSlots[i]].id) {
                    ar_mem_cpy(&(newDevices[newDeviceSlots[i]]), sizeof(struct pal_device),
                               &curDevAttr, sizeof(struct pal_device));
                    rm->getSndDeviceName(newDevices[newDeviceSlots[i]].id, CurrentSndDeviceName);
                    rm->updatePriorityAttr(newDevices[newDeviceSlots[i]].id,
                                       sharedBEStreamDev,
                                       &(newDevices[newDeviceSlots[i]]),
                                       &strAttr);
                }

                /*
                 * for current stream, if custom key updated, even reset of the attr
                 * like sample rate/channels/bit width/... are the same, still need
                 * to switch device to update custom config like devicePP
                 */
                if (sharedStream == streamHandle) {
                    if (strcmp(newDevices[newDeviceSlots[i]].custom_config.custom_key,
                               curDevAttr.custom_config.custom_key) != 0) {
                        PAL_DBG(LOG_TAG, "found diff custom key is %s, running dev has %s, device switch needed",
                        newDevices[newDeviceSlots[i]].custom_config.custom_key,
                        curDevAttr.custom_config.custom_key);
                        custom_switch = true;
                    }
                }
                /* If prioirty based attr diffs with running dev switch all devices */
                if (rm->doDevAttrDiffer(&(newDevices[newDeviceSlots[i]]),
                                          CurrentSndDeviceName,
                                          &curDevAttr) || custom_switch) {
                    streamDevDisconnect.push_back(elem);
                    StreamDevConnect.push_back({std::get<0>(elem), &newDevices[newDeviceSlots[i]]});
                    matchFound = true;
                    custom_switch = false;
                } else {
                    matchFound = false;
                }
            }
        } else {
            rm->updateSndName(newDeviceId, deviceInfo.sndDevName);
            matchFound = true;
            /*
             * check if virtual port group config needs to update, this handles scenario below:
             *   upd is already active on ultrasound device, active audio stream switched
             *   from other devices to speaker/handset, so group config needs to be updated to
             *   concurrency config
             */
            status = rm->checkAndUpdateGroupDevConfig(&newDevices[newDeviceSlots[i]], mStreamAttr,
                                                        streamsToSwitch, &streamDevAttr, true);
            if (status) {
                PAL_ERR(LOG_TAG, "no valid group device config found");
                streamsToSwitch.clear();
            }
            if (!streamsToSwitch.empty()) {
                for(sIter = streamsToSwitch.begin(); sIter != streamsToSwitch.end(); sIter++) {
                    streamDevDisconnect.push_back({(*sIter), streamDevAttr.id});
                    StreamDevConnect.push_back({(*sIter), &streamDevAttr});
                }
            }
            streamsToSwitch.clear();

            if (!rm->is_multiple_sample_rate_combo_supported) {
            // check if headset config needs to update when speaker is active
                rm->checkSpeakerConcurrency(&newDevices[newDeviceSlots[i]], &strAttr, streamsToSwitch/* not used */, &streamDevAttr);
                if (!streamsToSwitch.empty()) {
                     for(sIter = streamsToSwitch.begin(); sIter != streamsToSwitch.end(); sIter++) {
                         streamDevDisconnect.push_back({(*sIter), streamDevAttr.id});
                         StreamDevConnect.push_back({(*sIter), &streamDevAttr});
                     }
                }
            }

            // check if headset config needs to update when haptics is active
            rm->checkHapticsConcurrency(&newDevices[newDeviceSlots[i]], NULL, streamsToSwitch/* not used */, NULL);
        }
        /*
         * switch all streams that are running on the current device if:
         * 1. switching device for Voice Call
         * 2. switching device for Rx stream currently using same rx device as voice call
         */
        sharedBEStreamDev.clear();
        for (int j = 0; j < mDevices.size(); j++) {
            uint32_t mDeviceId = mDevices[j]->getSndDeviceId();
            if (rm->matchDevDir(newDeviceId, mDeviceId) && newDeviceId != mDeviceId) {
                if (mDeviceId == PAL_DEVICE_OUT_PROXY || newDeviceId == PAL_DEVICE_OUT_PROXY)
                    continue;
                rm->getSharedBEActiveStreamDevs(sharedBEStreamDev, mDevices[j]->getSndDeviceId());
                if (type == PAL_STREAM_VOICE_CALL &&
                    newDeviceId != PAL_DEVICE_OUT_HEARING_AID) {
                    voice_call_switch = true;
                } else if (rm->isOutputDevId(mDevices[j]->getSndDeviceId())) {
                    for (const auto &elem : sharedBEStreamDev) {
                        std::get<0>(elem)->getStreamAttributes(&strAttr);
                        if (strAttr.type == PAL_STREAM_VOICE_CALL &&
                            newDeviceId != PAL_DEVICE_OUT_HEARING_AID) {
                            voice_call_switch = true;
                            break;
                        }
                    }
                }
                if (voice_call_switch) {
                    for (const auto &elem : sharedBEStreamDev) {
                        streamDevDisconnect.push_back(elem);
                        StreamDevConnect.push_back({std::get<0>(elem), &newDevices[newDeviceSlots[i]]});
                    }
                }
            }
        }

        /* Add device associated with current stream to streamDevDisconnect/StreamDevConnect list */
        for (int j = 0; j < disconnectCount; j++) {
            // check to make sure device direction is the same
            if ((mDevices[curDeviceSlots[j]] != NULL) &&  rm->matchDevDir(mDevices[curDeviceSlots[j]]->getSndDeviceId(), newDeviceId)) {
                streamDevDisconnect.push_back({streamHandle, mDevices[curDeviceSlots[j]]->getSndDeviceId()});
                // if something disconnected incoming device and current dev diff so push on a switchwe need to add the deivce
                matchFound = true;
                // for switching from speaker/handset to other devices, if this is the last stream that active
                // on speaker/handset, need to update group config from concurrency to standalone
                // if switching happens between speaker & handset, skip the group config check as they're shared backend
                if ((mDevices[curDeviceSlots[j]]->getDeviceCount() == 1) &&
                    (newDeviceId != PAL_DEVICE_OUT_SPEAKER &&
                     newDeviceId != PAL_DEVICE_OUT_HANDSET)) {
                    mDevices[curDeviceSlots[j]]->getDeviceAttributes(&dAttr);
                    status = rm->checkAndUpdateGroupDevConfig(&dAttr, mStreamAttr, streamsToSwitch, &streamDevAttr, false);
                    if (status) {
                        PAL_ERR(LOG_TAG, "no valid group device config found");
                        streamsToSwitch.clear();
                    }
                    if (!streamsToSwitch.empty()) {
                        for(sIter = streamsToSwitch.begin(); sIter != streamsToSwitch.end(); sIter++) {
                            streamDevDisconnect.push_back({(*sIter), streamDevAttr.id});
                            StreamDevConnect.push_back({(*sIter), &streamDevAttr});
                        }
                    }
                    streamsToSwitch.clear();
                }
            }
        }

        if (matchFound || (disconnectCount == 0)) {
            dev = Device::getInstance(&newDevices[newDeviceSlots[i]],rm);
            StreamDevConnect.push_back({streamHandle, &newDevices[newDeviceSlots[i]]});
        }
    }

    /* Handle scenario when there is only device to disconnect.
       e.g. case 3 : device switch from spkr+hs to spkr */
    if (connectCount == 0) {
        for (int j = 0; j < disconnectCount; j++) {
            if (rm->matchDevDir(mDevices[curDeviceSlots[j]]->getSndDeviceId(), newDevices[0].id))
                streamDevDisconnect.push_back({streamHandle, mDevices[curDeviceSlots[j]]->getSndDeviceId()});
        }
    }

    /* Check if there is device to disconnect or connect */
    if (!streamDevDisconnect.size() && !StreamDevConnect.size()) {
        PAL_INFO(LOG_TAG, "No device to switch, returning");
        mStreamMutex.unlock();
        rm->unlockActiveStream();
        goto done;
    }
    mStreamMutex.unlock();
    rm->unlockActiveStream();

    status = rm->streamDevSwitch(streamDevDisconnect, StreamDevConnect);
    if (status) {
        PAL_ERR(LOG_TAG, "Device switch failed");
    }

done:
    mStreamMutex.lock();
    if (a2dpMuted) {
        volume = (struct pal_volume_data *)calloc(1, (sizeof(uint32_t) +
                              (sizeof(struct pal_channel_vol_kv) * (0xFFFF))));
        if (!volume) {
            PAL_ERR(LOG_TAG, "pal_volume_data memory allocation failure");
            mStreamMutex.unlock();
            rm->unlockActiveStream();
            return -ENOMEM;
        }
        status = streamHandle->getVolumeData(volume);
        if (status) {
            PAL_ERR(LOG_TAG, "getVolumeData failed %d", status);
        }
        a2dpMuted = false;
        status = streamHandle->setVolume(volume); //apply cached volume.
        if (status) {
            PAL_ERR(LOG_TAG, "setVolume failed %d", status);
        }
        mute_l(false);
        if (volume) {
            free(volume);
        }
    }
    if ((numDev > 1) && isNewDeviceA2dp && !isBtReady) {
        suspendedDevIds.clear();
        suspendedDevIds.push_back(newBtDevId);
        suspendedDevIds.push_back(PAL_DEVICE_OUT_SPEAKER);
    } else {
        suspendedDevIds.clear();
    }
    mStreamMutex.unlock();
    return status;
}

bool Stream::checkStreamMatch(pal_device_id_t pal_device_id,
                                        pal_stream_type_t pal_stream_type)
{
    int status = 0;
    struct pal_device dAttr;
    bool match = false;

    if (!mStreamAttr) {
        PAL_ERR(LOG_TAG, "stream attribute is null");
        return false;
    }

    if (pal_stream_type == mStreamAttr->type ||
            pal_stream_type == PAL_STREAM_GENERIC)
        match = true;
    else
        return false;

    //device
    for (int i = 0; i < mDevices.size();i++) {
       status = mDevices[i]->getDeviceAttributes(&dAttr);
       if (0 != status) {
          PAL_ERR(LOG_TAG,"getDeviceAttributes Failed \n");
          return false;
       }
       if (pal_device_id == dAttr.id || pal_device_id == PAL_DEVICE_NONE) {
           match = true;
           // as long as one device matches, it is enough.
           break;
       } else
           match = false;
    }

    return match;
}

int Stream::initStreamSmph()
{
    return sem_init(&mInUse, 0, 1);
}

int Stream::deinitStreamSmph()
{
    return sem_destroy(&mInUse);
}

int Stream::postStreamSmph()
{
    return sem_post(&mInUse);
}

int Stream::waitStreamSmph()
{
    return sem_wait(&mInUse);
}

void Stream::handleStreamException(struct pal_stream_attributes *attributes,
                                   pal_stream_callback cb, uint64_t cookie)
{
    if (!attributes || !cb) {
        PAL_ERR(LOG_TAG,"Invalid params for exception handling \n");
        return;
    }

    if (attributes->type == PAL_STREAM_COMPRESSED) {
         PAL_DBG(LOG_TAG, "Exception for Compressed clip");
         // Notify flinger about the error occurred while creating compress
         // offload session
         cb(NULL, PAL_STREAM_CBK_EVENT_ERROR, 0, 0, cookie);

    }
}

void Stream::setCachedState(stream_state_t state)
{
    mStreamMutex.lock();
    cachedState = state;
    PAL_DBG(LOG_TAG, "set cachedState to %d", cachedState);
    mStreamMutex.unlock();
}

