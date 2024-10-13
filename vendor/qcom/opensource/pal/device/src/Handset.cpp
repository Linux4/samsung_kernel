/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
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

#define LOG_TAG "PAL: Handset"
#include "Handset.h"
#include "ResourceManager.h"
#include "Device.h"
#include "kvh2xml.h"

std::shared_ptr<Device> Handset::obj = nullptr;

std::shared_ptr<Device> Handset::getObject()
{
    return obj;
}

std::shared_ptr<Device> Handset::getInstance(struct pal_device *device,
                                             std::shared_ptr<ResourceManager> Rm)
{
    if (!obj) {
        std::shared_ptr<Device> sp(new Handset(device, Rm));
        obj = sp;
    }
    return obj;
}


Handset::Handset(struct pal_device *device, std::shared_ptr<ResourceManager> Rm) :
Device(device, Rm)
{
#ifdef SEC_AUDIO_SPEAKER_AMP_RCV_BOOST_MODE
    rm = Rm;
    call2GTdma = false;
    ampBoostMode = false;
#endif
}

Handset::~Handset()
{

}

int32_t Handset::isSampleRateSupported(uint32_t sampleRate)
{
    int32_t rc = 0;
    PAL_DBG(LOG_TAG, "sampleRate %u", sampleRate);
    switch (sampleRate) {
        case SAMPLINGRATE_48K:
        case SAMPLINGRATE_96K:
            break;
        default:
            rc = -EINVAL;
            PAL_ERR(LOG_TAG, "sample rate not supported rc %d", rc);
            break;
    }
    return rc;
}

int32_t Handset::isChannelSupported(uint32_t numChannels)
{
    int32_t rc = 0;
    PAL_DBG(LOG_TAG, "numChannels %u", numChannels);
    switch (numChannels) {
        case CHANNELS_1:
            break;
        default:
            rc = -EINVAL;
            PAL_ERR(LOG_TAG, "channels not supported rc %d", rc);
            break;
    }
    return rc;
}

int32_t Handset::isBitWidthSupported(uint32_t bitWidth)
{
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

#ifdef SEC_AUDIO_SPEAKER_AMP_RCV_BOOST_MODE
int Handset::start()
{
    setAmpBoostMode();
    return Device::start();
}

int Handset::stop()
{
    setAmpBoostMode(true);
    return Device::stop();
}

int32_t Handset::setParameter(uint32_t param_id, void *param)
{
    if (param_id == PAL_PARAM_ID_CALL_2G_TDMA) {
        pal_param_call_2g_tdma_t* param_call_2g_tdma = (pal_param_call_2g_tdma_t *)param;
        call2GTdma = param_call_2g_tdma->enable;
        setAmpBoostMode();
    }
    return 0;
}

static bool isPalStreamVoiceCall(Stream *s)
{
    pal_stream_type_t type;
    int ret = s->getStreamType(&type);
    return ((0 == ret) && (type == PAL_STREAM_VOICE_CALL));
}

// requested by HW : to avoid noise during 2tdma cp call via handset.
void Handset::setAmpBoostMode(bool stop)
{
    int status = 0;
    struct mixer *mixer = NULL;
    struct mixer_ctl *ctl = NULL;
    char mixerCtlName[MIXER_PATH_MAX_LENGTH] = "RCV aw_dev_0_force_boost_8v_mode";
    char* bypassMode[] = { "Disable", "Enable" };
    bool mode = ampBoostMode;

    std::shared_ptr<Device> dev = nullptr;
    std::vector<Stream*> activestreams;
    dev = Device::getInstance(&deviceAttr, rm);
    if (!dev) {
        PAL_ERR(LOG_TAG, "Device getInstance failed");
        goto exit;
    }
    status = rm->getActiveStream_l(activestreams, dev);
    if ((0 != status)
            || (activestreams.size() == 0) // No active streams via handset
            || (stop && Device::getDeviceCount() == 1)) { // Stop the last device
        mode = false;
    } else {
        auto iter = std::find_if(activestreams.begin(), activestreams.end(), isPalStreamVoiceCall);
        // 2G TDMA RCV CP Call
        mode = (call2GTdma && (iter != activestreams.end()));
    }

    status = rm->getHwAudioMixer(&mixer);
    if (status) {
        PAL_ERR(LOG_TAG," mixer error");
        goto exit;
    }
    ctl = mixer_get_ctl_by_name(mixer, mixerCtlName);
    if (!ctl) {
        PAL_ERR(LOG_TAG, "Could not get ctl for mixer cmd - %s", mixerCtlName);
        goto exit;
    }

    if (mode != ampBoostMode) {
        int idx = mode ? 1 : 0;
        PAL_INFO(LOG_TAG, "Boost Mode - %s", bypassMode[idx]);
        mixer_ctl_set_enum_by_string(ctl, bypassMode[idx]);
        ampBoostMode = mode;
    }
exit:
    return;
}
#endif
