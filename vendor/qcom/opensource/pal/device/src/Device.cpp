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

#define LOG_TAG "PAL: Device"

#include "Device.h"
#include <tinyalsa/asoundlib.h>
#include "ResourceManager.h"
#include "SessionAlsaUtils.h"
#include "Device.h"
#include "Speaker.h"
#include "SpeakerProtection.h"
#include "Headphone.h"
#include "USBAudio.h"
#include "SpeakerMic.h"
#include "Stream.h"
#include "HeadsetMic.h"
#include "HandsetMic.h"
#include "HandsetVaMic.h"
#include "HeadsetVaMic.h"
#include "Handset.h"
#include "Bluetooth.h"
#include "DisplayPort.h"
#include "RTProxy.h"
#include "FMDevice.h"
#include "HapticsDev.h"
#include "UltrasoundDevice.h"
#include "ExtEC.h"

#define MAX_CHANNEL_SUPPORTED 2

std::shared_ptr<Device> Device::getInstance(struct pal_device *device,
                                                 std::shared_ptr<ResourceManager> Rm)
{
    if (!device || !Rm) {
        PAL_ERR(LOG_TAG, "Invalid input parameters");
        return NULL;
    }

    PAL_VERBOSE(LOG_TAG, "Enter device id %d", device->id);

    //TBD: decide on supported devices from XML and not in code
    switch (device->id) {
    case PAL_DEVICE_NONE:
        PAL_DBG(LOG_TAG,"device none");
        return nullptr;
    case PAL_DEVICE_OUT_HANDSET:
        PAL_VERBOSE(LOG_TAG, "handset device");
        return Handset::getInstance(device, Rm);
    case PAL_DEVICE_OUT_SPEAKER:
        PAL_VERBOSE(LOG_TAG, "speaker device");
        return Speaker::getInstance(device, Rm);
    case PAL_DEVICE_IN_VI_FEEDBACK:
        PAL_VERBOSE(LOG_TAG, "speaker feedback device");
        return SpeakerFeedback::getInstance(device, Rm);
    case PAL_DEVICE_OUT_WIRED_HEADSET:
    case PAL_DEVICE_OUT_WIRED_HEADPHONE:
        PAL_VERBOSE(LOG_TAG, "headphone device");
        return Headphone::getInstance(device, Rm);
    case PAL_DEVICE_OUT_USB_DEVICE:
    case PAL_DEVICE_OUT_USB_HEADSET:
    case PAL_DEVICE_IN_USB_DEVICE:
    case PAL_DEVICE_IN_USB_HEADSET:
        PAL_VERBOSE(LOG_TAG, "USB device");
        return USB::getInstance(device, Rm);
    case PAL_DEVICE_IN_HANDSET_MIC:
        PAL_VERBOSE(LOG_TAG, "HandsetMic device");
        return HandsetMic::getInstance(device, Rm);
    case PAL_DEVICE_IN_SPEAKER_MIC:
        PAL_VERBOSE(LOG_TAG, "speakerMic device");
        return SpeakerMic::getInstance(device, Rm);
    case PAL_DEVICE_IN_WIRED_HEADSET:
        PAL_VERBOSE(LOG_TAG, "HeadsetMic device");
        return HeadsetMic::getInstance(device, Rm);
    case PAL_DEVICE_IN_HANDSET_VA_MIC:
        PAL_VERBOSE(LOG_TAG, "HandsetVaMic device");
        return HandsetVaMic::getInstance(device, Rm);
    case PAL_DEVICE_IN_BLUETOOTH_SCO_HEADSET:
    case PAL_DEVICE_OUT_BLUETOOTH_SCO:
        PAL_VERBOSE(LOG_TAG, "BTSCO device");
        return BtSco::getInstance(device, Rm);
    case PAL_DEVICE_IN_BLUETOOTH_A2DP:
    case PAL_DEVICE_OUT_BLUETOOTH_A2DP:
        PAL_VERBOSE(LOG_TAG, "BTA2DP device");
        return BtA2dp::getInstance(device, Rm);
    case PAL_DEVICE_OUT_AUX_DIGITAL:
    case PAL_DEVICE_OUT_AUX_DIGITAL_1:
    case PAL_DEVICE_OUT_HDMI:
        PAL_VERBOSE(LOG_TAG, "Display Port device");
        return DisplayPort::getInstance(device, Rm);
    case PAL_DEVICE_IN_HEADSET_VA_MIC:
        PAL_VERBOSE(LOG_TAG, "HeadsetVaMic device");
        return HeadsetVaMic::getInstance(device, Rm);
    case PAL_DEVICE_OUT_PROXY:
        PAL_VERBOSE(LOG_TAG, "RTProxyOut device");
        return RTProxyOut::getInstance(device, Rm);
    case PAL_DEVICE_OUT_HEARING_AID:
        PAL_VERBOSE(LOG_TAG, "RTProxy Hearing Aid device");
        return RTProxyOut::getInstance(device, Rm);
    case PAL_DEVICE_OUT_HAPTICS_DEVICE:
        PAL_VERBOSE(LOG_TAG, "Haptics Device");
        return HapticsDev::getInstance(device, Rm);
    case PAL_DEVICE_IN_PROXY:
        PAL_VERBOSE(LOG_TAG, "RTProxy device");
        return RTProxy::getInstance(device, Rm);
    case PAL_DEVICE_IN_TELEPHONY_RX:
        PAL_VERBOSE(LOG_TAG, "RTProxy Telephony Rx device");
        return RTProxy::getInstance(device, Rm);
    case PAL_DEVICE_IN_FM_TUNER:
        PAL_VERBOSE(LOG_TAG, "FM device");
        return FMDevice::getInstance(device, Rm);
    case PAL_DEVICE_IN_ULTRASOUND_MIC:
    case PAL_DEVICE_OUT_ULTRASOUND:
        PAL_VERBOSE(LOG_TAG, "Ultrasound device");
        return UltrasoundDevice::getInstance(device, Rm);
    case PAL_DEVICE_IN_EXT_EC_REF:
        PAL_VERBOSE(LOG_TAG, "ExtEC device");
        return ExtEC::getInstance(device, Rm);
    default:
        PAL_ERR(LOG_TAG,"Unsupported device id %d",device->id);
        return nullptr;
    }
}

std::shared_ptr<Device> Device::getObject(pal_device_id_t dev_id)
{

    switch(dev_id) {
    case PAL_DEVICE_NONE:
        PAL_DBG(LOG_TAG,"device none");
        return nullptr;
    case PAL_DEVICE_OUT_HANDSET:
        PAL_VERBOSE(LOG_TAG, "handset device");
        return Handset::getObject();
    case PAL_DEVICE_OUT_SPEAKER:
        PAL_VERBOSE(LOG_TAG, "speaker device");
        return Speaker::getObject();
    case PAL_DEVICE_OUT_WIRED_HEADSET:
    case PAL_DEVICE_OUT_WIRED_HEADPHONE:
        PAL_VERBOSE(LOG_TAG, "headphone device");
        return Headphone::getObject(dev_id);
    case PAL_DEVICE_OUT_USB_DEVICE:
    case PAL_DEVICE_OUT_USB_HEADSET:
    case PAL_DEVICE_IN_USB_DEVICE:
    case PAL_DEVICE_IN_USB_HEADSET:
        PAL_VERBOSE(LOG_TAG, "USB device");
        return USB::getObject(dev_id);
    case PAL_DEVICE_OUT_AUX_DIGITAL:
    case PAL_DEVICE_OUT_AUX_DIGITAL_1:
    case PAL_DEVICE_OUT_HDMI:
        PAL_VERBOSE(LOG_TAG, "Display Port device");
        return DisplayPort::getObject(dev_id);
    case PAL_DEVICE_IN_HANDSET_MIC:
        PAL_VERBOSE(LOG_TAG, "handset mic device");
        return HandsetMic::getObject();
    case PAL_DEVICE_IN_SPEAKER_MIC:
        PAL_VERBOSE(LOG_TAG, "speaker mic device");
        return SpeakerMic::getObject();
    case PAL_DEVICE_IN_WIRED_HEADSET:
        PAL_VERBOSE(LOG_TAG, "headset mic device");
        return HeadsetMic::getObject();
    case PAL_DEVICE_OUT_BLUETOOTH_A2DP:
    case PAL_DEVICE_IN_BLUETOOTH_A2DP:
        PAL_VERBOSE(LOG_TAG, "BT A2DP device %d", dev_id);
        return BtA2dp::getObject(dev_id);
    case PAL_DEVICE_OUT_BLUETOOTH_SCO:
    case PAL_DEVICE_IN_BLUETOOTH_SCO_HEADSET:
        PAL_VERBOSE(LOG_TAG, "BT SCO device %d", dev_id);
        return BtSco::getObject(dev_id);
    case PAL_DEVICE_OUT_PROXY:
    case PAL_DEVICE_IN_PROXY:
    case PAL_DEVICE_OUT_HEARING_AID:
    case PAL_DEVICE_IN_TELEPHONY_RX:
        PAL_VERBOSE(LOG_TAG, "RTProxy device %d", dev_id);
        return RTProxy::getObject();
    case PAL_DEVICE_IN_FM_TUNER:
        PAL_VERBOSE(LOG_TAG, "FMDevice %d", dev_id);
        return FMDevice::getObject();
    case PAL_DEVICE_IN_ULTRASOUND_MIC:
    case PAL_DEVICE_OUT_ULTRASOUND:
        PAL_VERBOSE(LOG_TAG, "Ultrasound device %d", dev_id);
        return UltrasoundDevice::getObject(dev_id);
    case PAL_DEVICE_IN_EXT_EC_REF:
        PAL_VERBOSE(LOG_TAG, "ExtEC device %d", dev_id);
        return ExtEC::getObject();
    case PAL_DEVICE_IN_HANDSET_VA_MIC:
        PAL_VERBOSE(LOG_TAG, "Handset VA Mic device %d", dev_id);
        return HandsetVaMic::getObject();
    case PAL_DEVICE_IN_HEADSET_VA_MIC:
        PAL_VERBOSE(LOG_TAG, "Headset VA Mic device %d", dev_id);
        return HeadsetVaMic::getObject();
    default:
        PAL_ERR(LOG_TAG,"Unsupported device id %d",dev_id);
        return nullptr;
    }
}

Device::Device(struct pal_device *device, std::shared_ptr<ResourceManager> Rm)
{
    rm = Rm;

    memset(&deviceAttr, 0, sizeof(struct pal_device));
    ar_mem_cpy(&deviceAttr, sizeof(struct pal_device), device,
                     sizeof(struct pal_device));

    mPALDeviceName.clear();
    customPayload = NULL;
    customPayloadSize = 0;
    strlcpy(mSndDeviceName, "", DEVICE_NAME_MAX_SIZE);
    mCurrentPriority = MIN_USECASE_PRIORITY;
    PAL_DBG(LOG_TAG,"device instance for id %d created", device->id);

}

Device::Device()
{
    strlcpy(mSndDeviceName, "", DEVICE_NAME_MAX_SIZE);
    mCurrentPriority = MIN_USECASE_PRIORITY;
    mPALDeviceName.clear();
}

Device::~Device()
{
    if (customPayload)
        free(customPayload);

    customPayload = NULL;
    customPayloadSize = 0;
    mCurrentPriority = MIN_USECASE_PRIORITY;
    PAL_DBG(LOG_TAG,"device instance for id %d destroyed", deviceAttr.id);
}

int Device::getDeviceAttributes(struct pal_device *dattr)
{
    if (!dattr) {
        PAL_ERR(LOG_TAG, "Invalid device attributes");
        return  -EINVAL;
    }

    ar_mem_cpy(dattr, sizeof(struct pal_device),
            &deviceAttr, sizeof(struct pal_device));

    return 0;
}

int Device::getCodecConfig(struct pal_media_config *config __unused)
{
    return 0;
}


int Device::getDefaultConfig(pal_param_device_capability_t capability __unused)
{
    return 0;
}

int Device::setDeviceAttributes(struct pal_device dattr)
{
    int status = 0;

    mDeviceMutex.lock();
    PAL_INFO(LOG_TAG,"DeviceAttributes for Device Id %d updated", dattr.id);
    ar_mem_cpy(&deviceAttr, sizeof(struct pal_device), &dattr,
                     sizeof(struct pal_device));

    mDeviceMutex.unlock();
    return status;
}

int Device::updateCustomPayload(void *payload, size_t size)
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

void* Device::getCustomPayload()
{
    return customPayload;
}

size_t Device::getCustomPayloadSize()
{
    return customPayloadSize;
}

int Device::getSndDeviceId()
{
    PAL_VERBOSE(LOG_TAG,"Device Id %d acquired", deviceAttr.id);
    return deviceAttr.id;
}

void Device::getCurrentSndDevName(char *name){
    strlcpy(name, mSndDeviceName, DEVICE_NAME_MAX_SIZE);
}

std::string Device::getPALDeviceName()
{
    PAL_VERBOSE(LOG_TAG, "Device name %s acquired", mPALDeviceName.c_str());
    return mPALDeviceName;
}

int Device::init(pal_param_device_connection_t device_conn __unused)
{
    return 0;
}

int Device::deinit(pal_param_device_connection_t device_conn __unused)
{
    return 0;
}

int Device::open()
{
    int status = 0;

    mDeviceMutex.lock();
    mPALDeviceName = rm->getPALDeviceName(this->deviceAttr.id);
    PAL_INFO(LOG_TAG, "Enter. deviceCount %d for device id %d (%s)", deviceCount,
            this->deviceAttr.id, mPALDeviceName.c_str());

    devObj = Device::getInstance(&deviceAttr, rm);

    if (deviceCount == 0) {
        std::string backEndName;
        rm->getBackendName(this->deviceAttr.id, backEndName);
        if (strlen(backEndName.c_str())) {
            SessionAlsaUtils::setDeviceMediaConfig(rm, backEndName, &(this->deviceAttr));
        }

        status = rm->getAudioRoute(&audioRoute);
        if (0 != status) {
            PAL_ERR(LOG_TAG, "Failed to get the audio_route address status %d", status);
            goto exit;
        }
        status = rm->getSndDeviceName(deviceAttr.id , mSndDeviceName); //getsndName

        if (!UpdatedSndName.empty()) {
            PAL_DBG(LOG_TAG,"Update sndName %s, currently %s",
                    UpdatedSndName.c_str(), mSndDeviceName);
            strlcpy(mSndDeviceName, UpdatedSndName.c_str(), DEVICE_NAME_MAX_SIZE);
        }

        PAL_DBG(LOG_TAG, "audio_route %pK SND device name %s", audioRoute, mSndDeviceName);
        if (0 != status) {
            PAL_ERR(LOG_TAG, "Failed to obtain the device name from ResourceManager status %d", status);
            goto exit;
        }
        enableDevice(audioRoute, mSndDeviceName);
    }
    ++deviceCount;

exit:
    PAL_INFO(LOG_TAG, "Exit. deviceCount %d for device id %d (%s), exit status: %d", deviceCount,
            this->deviceAttr.id, mPALDeviceName.c_str(), status);
    mDeviceMutex.unlock();
    return status;
}

int Device::close()
{
    int status = 0;
    mDeviceMutex.lock();
    PAL_INFO(LOG_TAG, "Enter. deviceCount %d for device id %d (%s)", deviceCount,
            this->deviceAttr.id, mPALDeviceName.c_str());
    if (deviceCount > 0) {
        --deviceCount;

       if (deviceCount == 0) {
           PAL_DBG(LOG_TAG, "Disabling device %d with snd dev %s", deviceAttr.id, mSndDeviceName);
           disableDevice(audioRoute, mSndDeviceName);
           mCurrentPriority = MIN_USECASE_PRIORITY;
           deviceStartStopCount = 0;
       }
    }
    PAL_INFO(LOG_TAG, "Exit. deviceCount %d for device id %d (%s), exit status %d", deviceCount,
            this->deviceAttr.id, mPALDeviceName.c_str(), status);
    mDeviceMutex.unlock();
    return status;
}

int Device::prepare()
{
    return 0;
}

int Device::start()
{
    int status = 0;

    mDeviceMutex.lock();
    status = start_l();
    mDeviceMutex.unlock();

    return status;
}

// must be called with mDeviceMutex held
int Device::start_l()
{
    int status = 0;
    std::string backEndName;

    PAL_DBG(LOG_TAG, "Enter. deviceCount %d deviceStartStopCount %d"
        " for device id %d (%s)", deviceCount, deviceStartStopCount,
            this->deviceAttr.id, mPALDeviceName.c_str());
    if (0 == deviceStartStopCount) {
        rm->getBackendName(deviceAttr.id, backEndName);
        if (!strlen(backEndName.c_str())) {
            PAL_ERR(LOG_TAG, "Error: Backend name not defined for %d in xml file\n", deviceAttr.id);
            status = -EINVAL;
            goto exit;
        }

        SessionAlsaUtils::setDeviceMediaConfig(rm, backEndName, &deviceAttr);

        if (customPayloadSize) {
            status = SessionAlsaUtils::setDeviceCustomPayload(rm, backEndName,
                                        customPayload, customPayloadSize);
            if (status)
                 PAL_ERR(LOG_TAG, "Error: Dev setParam failed for %d\n",
                                   deviceAttr.id);
        }
    }
    deviceStartStopCount++;
exit :
    PAL_DBG(LOG_TAG, "Exit, status %d", status);
    return status;
}

int Device::stop()
{
    int status = 0;

    mDeviceMutex.lock();
    status = stop_l();
    mDeviceMutex.unlock();

    return status;
}

// must be called with mDeviceMutex held
int Device::stop_l()
{
    PAL_DBG(LOG_TAG, "Enter. deviceCount %d deviceStartStopCount %d"
        " for device id %d (%s)", deviceCount, deviceStartStopCount,
            this->deviceAttr.id, mPALDeviceName.c_str());

    if (deviceStartStopCount > 0) {
        --deviceStartStopCount;
    }

    return 0;
}

int32_t Device::setDeviceParameter(uint32_t param_id __unused, void *param __unused)
{
    return 0;
}

int32_t Device::getDeviceParameter(uint32_t param_id __unused, void **param __unused)
{
    return 0;
}

int32_t Device::setParameter(uint32_t param_id __unused, void *param __unused)
{
    return 0;
}

int32_t Device::getParameter(uint32_t param_id __unused, void **param __unused)
{
    return 0;
}
