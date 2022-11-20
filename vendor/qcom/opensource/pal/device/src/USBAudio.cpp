/*
 * Copyright (c) 2016, 2018-2021, The Linux Foundation. All rights reserved.

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *  * Neither the name of The Linux Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.

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

#define LOG_TAG "PAL: USB"
#include "USBAudio.h"

#include <cstdio>
#include <cmath>
#include "USBAudio.h"
#include "ResourceManager.h"
#include "PayloadBuilder.h"
#include "Device.h"
#include "kvh2xml.h"

#ifdef SEC_AUDIO_QUICK_USB_DETECTION
#include <unistd.h>
#endif

std::shared_ptr<Device> USB::objRx = nullptr;
std::shared_ptr<Device> USB::objTx = nullptr;

std::shared_ptr<Device> USB::getInstance(struct pal_device *device,
                                             std::shared_ptr<ResourceManager> Rm)
{
    if (!device)
       return NULL;

    if ((device->id == PAL_DEVICE_OUT_USB_DEVICE) ||
        (device->id == PAL_DEVICE_OUT_USB_HEADSET)){
        if (!objRx) {
            std::shared_ptr<Device> sp(new USB(device, Rm));
            objRx = sp;
        }
        return objRx;
    } else if ((device->id == PAL_DEVICE_IN_USB_DEVICE) ||
               (device->id == PAL_DEVICE_IN_USB_HEADSET)){
        if (!objTx) {
            std::shared_ptr<Device> sp(new USB(device, Rm));
            objTx = sp;
        }
        return objTx;
    }

    return NULL;
}

std::shared_ptr<Device> USB::getObject(pal_device_id_t id)
{
    if ((id == PAL_DEVICE_OUT_USB_DEVICE) ||
        (id == PAL_DEVICE_OUT_USB_HEADSET)) {
        if (objRx) {
            if (objRx->getSndDeviceId() == id)
                return objRx;
        }
    } else if ((id == PAL_DEVICE_IN_USB_DEVICE) ||
               (id == PAL_DEVICE_IN_USB_HEADSET)) {
        if (objTx) {
            if (objTx->getSndDeviceId() == id)
                return objTx;
        }
    }
    return NULL;
}

USB::USB(struct pal_device *device, std::shared_ptr<ResourceManager> Rm) :
Device(device, Rm)
{

}

USB::~USB()
{
    PAL_INFO(LOG_TAG, "dtor called");
}

int USB::start()
{
    int status = 0;

    if (customPayload)
        free(customPayload);

    customPayload = NULL;
    customPayloadSize = 0;

    status = configureUsb();
    if (status != 0) {
        PAL_ERR(LOG_TAG,"USB Endpoint Configuration Failed");
        return status;
    }

    status = Device::start();
    return status;
}

int USB::configureUsb()
{
    int status = 0;
    std::string backEndName;
    Stream *stream = NULL;
    std::shared_ptr<Device> dev = nullptr;
    PayloadBuilder* builder = new PayloadBuilder();
    struct usbAudioConfig cfg;
    uint8_t* payload = NULL;
    std::vector<Stream*> activestreams;
    Session *session = NULL;
    size_t payloadSize = 0;
    uint32_t miid = 0;
    int32_t tagId;

    rm->getBackendName(deviceAttr.id, backEndName);
    dev = Device::getInstance(&deviceAttr, rm);
    status = rm->getActiveStream_l(dev, activestreams);
    if ((0 != status) || (activestreams.size() == 0)) {
        PAL_ERR(LOG_TAG, "no active stream available");
        status = -EINVAL;
        goto exit;
    }
#ifdef SEC_AUDIO_USB_GAIN_CONTROL
    setUSBAudioGain();
#endif
    stream = static_cast<Stream *>(activestreams[0]);
    stream->getAssociatedSession(&session);
    if (deviceAttr.id == PAL_DEVICE_IN_USB_HEADSET) {
        tagId = DEVICE_HW_ENDPOINT_TX;
        cfg.usb_token = (deviceAttr.address.card_id << 16)|0x1;
        cfg.svc_interval = 0;
    } else{
        tagId = DEVICE_HW_ENDPOINT_RX;
        cfg.usb_token = deviceAttr.address.card_id << 16;
        cfg.svc_interval = 0;
    }
    status = session->getMIID(backEndName.c_str(), tagId, &miid);
    if (status) {
        PAL_ERR(LOG_TAG, "Failed to get tag info %d, status = %d", tagId, status);
        goto exit;
    }
    builder->payloadUsbAudioConfig(&payload, &payloadSize, miid, &cfg);
    if (payloadSize) {
        status = updateCustomPayload(payload, payloadSize);
        delete[] payload;
        if (0 != status) {
            PAL_ERR(LOG_TAG,"updateCustomPayload Failed\n");
            goto exit;
        }
    }
exit:
    if(builder) {
       delete builder;
       builder = NULL;
    }
    return status;
}

int32_t USB::isSampleRateSupported(unsigned int sampleRate)
{
    int32_t rc = 0;
    PAL_DBG(LOG_TAG, "sampleRate %d", sampleRate);

    if (sampleRate % SAMPLINGRATE_44K == 0)
        return rc;

    switch (sampleRate) {
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
//TBD why do these channels have to be supported, USBs support only 1/2?
int32_t USB::isChannelSupported(unsigned int numChannels)
{
    int32_t rc = 0;
    PAL_DBG(LOG_TAG, "numChannels %u", numChannels);
    switch (numChannels) {
        case CHANNELS_1:
        case CHANNELS_2:
            break;
        default:
            rc = -EINVAL;
            PAL_ERR(LOG_TAG, "channels not supported rc %d", rc);
            break;
    }
    return rc;
}

int USB::init(pal_param_device_connection_t device_conn)
{
    typename std::vector<std::shared_ptr<USBCardConfig>>::iterator iter;
    int ret = 0;

    for (iter = usb_card_config_list_.begin();
         iter != usb_card_config_list_.end(); iter++) {
         if ((*iter)->isConfigCached(device_conn.device_config.usb_addr))
              break;
    }

    if (iter == usb_card_config_list_.end()) {
        std::shared_ptr<USBCardConfig> sp(
            new USBCardConfig(device_conn.device_config.usb_addr));
        if (!sp) {
            PAL_ERR(LOG_TAG, "failed to create new usb_card_config object.");
            return -EINVAL;
        }
        if (isUSBOutDevice(device_conn.id))
            ret = sp->getCapability(USB_PLAYBACK, device_conn.device_config.usb_addr);
        else
            ret = sp->getCapability(USB_CAPTURE, device_conn.device_config.usb_addr);

#ifdef SEC_AUDIO_USB_GAIN_CONTROL
        sp->USBAudioGainControl(device_conn.device_config.usb_addr, device_conn.id);
#endif
        if (ret != -ENOENT)
            usb_card_config_list_.push_back(sp);
    } else {
        PAL_INFO(LOG_TAG, "usb info has been cached.");
    }

    return ret;
}

int USB::deinit(pal_param_device_connection_t device_conn)
{
    typename std::vector<std::shared_ptr<USBCardConfig>>::iterator iter;

    for (iter = usb_card_config_list_.begin();
         iter != usb_card_config_list_.end(); iter++) {
         if ((*iter)->isConfigCached(device_conn.device_config.usb_addr))
              break;
    }

    if (iter != usb_card_config_list_.end()) {
#ifdef SEC_AUDIO_USB_GAIN_CONTROL
        if ((*iter)->getUSBAudioRoute()) {
            audio_route_free((*iter)->getUSBAudioRoute());
        }
#endif
        //TODO: usb_remove_capability
        usb_card_config_list_.erase(iter);
    } else {
        PAL_INFO(LOG_TAG, "usb info has not been cached.");
    }

    return 0;
}

int32_t USB::isBitWidthSupported(unsigned int bitWidth __unused)
{
    return 0;
}

int32_t USB::checkAndUpdateBitWidth(unsigned int *bitWidth __unused)
{
    return 0;
}

int32_t USB::checkAndUpdateSampleRate(unsigned int *sampleRate __unused)
{
    return 0;
}

bool USB::isUSBOutDevice(pal_device_id_t pal_dev_id) {
    if (pal_dev_id == PAL_DEVICE_OUT_USB_DEVICE ||
        pal_dev_id == PAL_DEVICE_OUT_USB_HEADSET)
        return true;
    else
        return false;
}

bool USBCardConfig::isCaptureProfileSupported()
{
    usb_usecase_type_t capture_type = USB_CAPTURE;
    typename std::vector<std::shared_ptr<USBDeviceConfig>>::iterator iter;

    for (iter = usb_device_config_list_.begin();
         iter != usb_device_config_list_.end(); iter++) {
         if ((*iter)->getType() == capture_type)
             return true;
    }

    return false;
}

int USB::getDefaultConfig(pal_param_device_capability_t capability)
{
    typename std::vector<std::shared_ptr<USBCardConfig>>::iterator iter;
    int status = 0;

    if (!isUsbConnected(capability.addr)) {
        PAL_ERR(LOG_TAG, "No usb sound card present");
        return -EINVAL;
    }

    for (iter = usb_card_config_list_.begin();
            iter != usb_card_config_list_.end();
            iter++) {
        if ((*iter)->isConfigCached(capability.addr)) {
            PAL_ERR(LOG_TAG, "usb device is found.");
            // for capture, check if profile is supported or not
            if (capability.is_playback == false) {
                memset(capability.config, 0, sizeof(struct dynamic_media_config));
                if ((*iter)->isCaptureProfileSupported())
                    status = (*iter)->readSupportedConfig(capability.config,
                            capability.is_playback);
            } else {
                status = (*iter)->readSupportedConfig(capability.config,
                        capability.is_playback);
            }
            break;
        }
    }

    if (iter == usb_card_config_list_.end()) {
        PAL_ERR(LOG_TAG, "usb device card=%d device=%d is not found.",
                    capability.addr.card_id, capability.addr.device_num);
        return -EINVAL;
    }

    return status;
}

int USB::selectBestConfig(struct pal_device *dattr,
                          struct pal_stream_attributes *sattr,
                          bool is_playback, struct pal_device_info *devinfo)
{
    typename std::vector<std::shared_ptr<USBCardConfig>>::iterator iter;

    int status = 0;

    for (iter = usb_card_config_list_.begin();
            iter != usb_card_config_list_.end(); iter++) {
        if ((*iter)->isConfigCached(dattr->address)) {
#ifdef SEC_AUDIO_SUPPORT_UHQ
            PAL_INFO(LOG_TAG, "usb device is found. rm->isUHQAEnabled:%d ", rm->stateUHQA);
            status = (*iter)->readBestConfig(&dattr->config, sattr, is_playback,
                                devinfo, rm->stateUHQA);
#else
            PAL_ERR(LOG_TAG, "usb device is found.");
            status = (*iter)->readBestConfig(&dattr->config, sattr, is_playback, devinfo);
#endif
            break;
        }
    }

    if (iter == usb_card_config_list_.end()) {
        PAL_ERR(LOG_TAG, "usb device card=%d device=%d is not found.",
                    dattr->address.card_id, dattr->address.device_num);
        return -EINVAL;
    }

    return status;

}

#ifdef SEC_AUDIO_USB_GAIN_CONTROL
int USB::setUSBAudioGain()
{
    int ret = 0;
    char mSndDeviceName[128] = {0};
    char gain_name[MAX_PATH_LEN] = {0};
    typename std::vector<std::shared_ptr<USBCardConfig>>::iterator iter;

    ret = rm->getSndDeviceName(deviceAttr.id , mSndDeviceName);
    if (0 != ret) {
        PAL_ERR(LOG_TAG, "Failed to obtain snd device name");
        goto done;
    }

    // Add "-gain" post-fix for usb gain control
    snprintf(gain_name, sizeof(gain_name), "%s%s", mSndDeviceName, "-gain");

    for (iter = usb_card_config_list_.begin();
            iter != usb_card_config_list_.end(); iter++) {
        if ((*iter)->isConfigCached(deviceAttr.address)) {
            /* Apply gain only if audio_route library is initialized */
            if ((*iter)->getUSBAudioRoute())
                enableDevice((*iter)->getUSBAudioRoute(), gain_name);
            break;
        }
    }

    if (iter == usb_card_config_list_.end()) {
        PAL_ERR(LOG_TAG, "usb device is not found.");
        goto done;
    }

done:
    return ret;
}
#endif
const unsigned int USBCardConfig::out_chn_mask_[MAX_SUPPORTED_CHANNEL_MASKS] =
    {0x3, 0x80000003, 0x80000007, 0x8000000f, 0x8000001f, 0x8000003f,
    0x8000007f, 0x800000ff};

const unsigned int USBCardConfig::in_chn_mask_[MAX_SUPPORTED_CHANNEL_MASKS] =
    {0x10, 0x80000001, 0xc, 0x80000003, 0x80000007, 0x8000000f, 0x8000001f,
    0x8000003f};

bool USBCardConfig::isConfigCached(struct pal_usb_device_address addr) {
    if(address_.card_id == addr.card_id && address_.device_num == addr.device_num)
        return true;
    else
        return false;
}

void USBCardConfig::setEndian(int endian){
    endian_ = endian;
}

int USBCardConfig::getCapability(usb_usecase_type_t type,
                                        struct pal_usb_device_address addr) {
    int32_t size = 0;
    FILE *fd = NULL;
    int32_t channels_no;
    char *str_start = NULL;
    char *str_end = NULL;
    char *channel_start = NULL;
    char *bit_width_start = NULL;
    char *rates_str_start = NULL;
    char *target = NULL;
    char *read_buf = NULL;
    char *rates_str = NULL;
    char *interval_str_start = NULL;
    char path[128];
    int ret = 0;
    char *bit_width_str = NULL;
    size_t num_read = 0;
    //std::shared_ptr<USBDeviceConfig> usb_device_info = nullptr;

    bool check = false;
#ifdef SEC_AUDIO_QUICK_USB_DETECTION
    int retries = USB_PCM_OPEN_RETRY_CNT;
#endif
    memset(path, 0, sizeof(path));
    PAL_INFO(LOG_TAG, "for %s", (type == USB_PLAYBACK) ?
          PLAYBACK_PROFILE_STR : CAPTURE_PROFILE_STR);

    ret = snprintf(path, sizeof(path), "/proc/asound/card%u/stream0",
             addr.card_id);
    if(ret < 0) {
        PAL_ERR(LOG_TAG, "failed on snprintf (%d) to path %s\n", ret, path);
        goto done;
    }
#ifdef SEC_AUDIO_QUICK_USB_DETECTION
    while (retries--) {
        if (usb_recognition_state(addr.card_id) && (access(path, F_OK) < 0)) {
            PAL_INFO(LOG_TAG, "%s doesn't exist, retrying\n", path);
            usleep(20000);
        } else {
            break;
        }
    }
#endif
    fd = fopen(path, "r");
    if (!fd) {
        PAL_ERR(LOG_TAG, "failed to open config file %s error: %d\n", path, errno);
        ret = -EINVAL;
        goto done;
    }

    read_buf = (char *)calloc(1, USB_BUFF_SIZE + 1);
    if (!read_buf) {
        PAL_ERR(LOG_TAG, "Failed to create read_buf");
        ret = -ENOMEM;
        goto done;
    }

    if ((num_read = fread(read_buf, 1, USB_BUFF_SIZE, fd)) < 0) {
        PAL_ERR(LOG_TAG, "file read error");
        goto done;
    }
    read_buf[num_read] = '\0';

    str_start = strstr(read_buf, ((type == USB_PLAYBACK) ?
                       PLAYBACK_PROFILE_STR : CAPTURE_PROFILE_STR));
    if (str_start == NULL) {
        PAL_INFO(LOG_TAG, "error %s section not found in usb config file",
                ((type == USB_PLAYBACK) ?
               PLAYBACK_PROFILE_STR : CAPTURE_PROFILE_STR));
        ret = -ENOENT;
        goto done;
    }

    str_end = strstr(read_buf, ((type == USB_PLAYBACK) ?
                       CAPTURE_PROFILE_STR : PLAYBACK_PROFILE_STR));

    if (str_end > str_start)
        check = true;

    PAL_INFO(LOG_TAG, "usb_config = %s, check %d\n", str_start, check);

    while (str_start != NULL) {
        str_start = strstr(str_start, "Altset");
        if ((str_start == NULL) || (check  && (str_start >= str_end))) {
            PAL_VERBOSE(LOG_TAG,"done parsing %s\n", str_start);
            break;
        }
        PAL_VERBOSE(LOG_TAG,"remaining string %s\n", str_start);
        str_start += sizeof("Altset");
        std::shared_ptr<USBDeviceConfig> usb_device_info(new USBDeviceConfig());
        if (!usb_device_info) {
            PAL_ERR(LOG_TAG, "error unable to create usb device config object");
            ret = -ENOMEM;
            break;
        }
        usb_device_info->setType(type);
        /* Bit bit_width parsing */
        bit_width_start = strstr(str_start, "Format: ");
        if (bit_width_start == NULL) {
            PAL_INFO(LOG_TAG, "Could not find bit_width string");
            continue;
        }
        target = strchr(bit_width_start, '\n');
        if (target == NULL) {
            PAL_INFO(LOG_TAG, "end of line not found");
            continue;
        }
        size = target - bit_width_start;
        if ((bit_width_str = (char *)malloc(size + 1)) == NULL) {
            PAL_ERR(LOG_TAG, "unable to allocate memory to hold bit width strings");
            ret = -EINVAL;
            break;
        }
        memcpy(bit_width_str, bit_width_start, size);
        bit_width_str[size] = '\0';

        const char *formats[] = {"S32", "S24_3", "S24", "S16", "U32"};
        const int bit_width[] = {32, 24, 24, 16, 32};
        for (size_t i = 0; i < sizeof(formats)/sizeof(formats[0]); i++) {
            const char * s = strstr(bit_width_str, formats[i]);
            if (s) {
                usb_device_info->setBitWidth(bit_width[i]);
                setEndian(strstr(s, "BE") ? 1 : 0);
                break;
            }
        }

        if (bit_width_str)
            free(bit_width_str);

        /* channels parsing */
        channel_start = strstr(str_start, CHANNEL_NUMBER_STR);
        if (channel_start == NULL) {
            PAL_INFO(LOG_TAG, "could not find Channels string");
            continue;
        }
        channels_no = atoi(channel_start + strlen(CHANNEL_NUMBER_STR));
        usb_device_info->setChannels(channels_no);

        /* Sample rates parsing */
        rates_str_start = strstr(str_start, "Rates: ");
        if (rates_str_start == NULL) {
            PAL_INFO(LOG_TAG, "cant find rates string");
            continue;
        }
        target = strchr(rates_str_start, '\n');
        if (target == NULL) {
            PAL_INFO(LOG_TAG, "end of line not found");
            continue;
        }
        size = target - rates_str_start;
        if ((rates_str = (char *)malloc(size + 1)) == NULL) {
            PAL_INFO(LOG_TAG, "unable to allocate memory to hold sample rate strings");
            ret = -EINVAL;

            break;
        }
        memcpy(rates_str, rates_str_start, size);
        rates_str[size] = '\0';
        ret = usb_device_info->getSampleRates(type, rates_str);
        if (rates_str)
            free(rates_str);
        if (ret < 0) {
            PAL_INFO(LOG_TAG, "error unable to get sample rate values");
            continue;
        }
        // Data packet interval is an optional field.
        // Assume 0ms interval if this cannot be read
        // LPASS USB and HLOS USB will figure out the default to use
        usb_device_info->setInterval(DEFAULT_SERVICE_INTERVAL_US);
        interval_str_start = strstr(str_start, DATA_PACKET_INTERVAL_STR);
        if (interval_str_start != NULL) {
            interval_str_start += strlen(DATA_PACKET_INTERVAL_STR);
            ret = usb_device_info->getServiceInterval(interval_str_start);
            if (ret < 0) {
                PAL_INFO(LOG_TAG, "error unable to get service interval, assume default");
            }
        }
        /* Add to list if every field is valid */
        usb_device_config_list_.push_back(usb_device_info);
    }

done:
    if (fd)
        fclose(fd);

    if (read_buf)
        free(read_buf);

    return ret;
}

USBCardConfig::USBCardConfig(struct pal_usb_device_address address) {
    address_ = address;
}

unsigned int USBCardConfig::getMax(unsigned int x, unsigned int y) {
    return (((x) >= (y)) ? (x) : (y));
}

unsigned int USBCardConfig::getMin(unsigned int x, unsigned int y) {
    return (((x) <= (y)) ? (x) : (y));
}

int USBCardConfig::getMaxBitWidth(bool is_playback)
{
    unsigned int max_bw = 16;
    typename std::vector<std::shared_ptr<USBDeviceConfig>>::iterator iter;

    for (iter = usb_device_config_list_.begin();
         iter != usb_device_config_list_.end(); iter++) {
         if ((*iter)->getType() == is_playback)
             max_bw = getMax(max_bw, (*iter)->getBitWidth());
    }

    return max_bw;
}

int USBCardConfig::getMaxChannels(bool is_playback)
{
    unsigned int max_ch = 1;
    typename std::vector<std::shared_ptr<USBDeviceConfig>>::iterator iter;

    for (iter = usb_device_config_list_.begin();
         iter != usb_device_config_list_.end(); iter++) {
         if ((*iter)->getType() == is_playback)
             max_ch = getMax(max_ch, (*iter)->getChannels());
    }

    return max_ch;
}

unsigned int USBCardConfig::getFormatByBitWidth(int bitwidth) {
    unsigned int default_format = PCM_16_BIT;
    switch (bitwidth) {
        case 24:
            // XXX : usb.c returns 24 for s24 and s24_le?
            default_format = PCM_24_BIT_PACKED;
            break;
        case 32:
            default_format = PCM_32_BIT;
            break;
        case 16:
        default :
            default_format = PCM_16_BIT;
            break;
    }

    return default_format;
}

unsigned int USBCardConfig::readDefaultFormat(bool is_playback) {
    int bitwidth = getMaxBitWidth(is_playback);

    return getFormatByBitWidth(bitwidth);
}

unsigned int USBCardConfig::readDefaultSampleRate(bool is_playback) {
    unsigned int sample_rate = 0;
    typename std::vector<std::shared_ptr<USBDeviceConfig>>::iterator iter;

    for (iter = usb_device_config_list_.begin();
         iter != usb_device_config_list_.end(); iter++) {
             if ((*iter)->getType() == is_playback){
                 sample_rate = (*iter)->getDefaultRate();
                 break;
             }
         }

    return sample_rate;
}

unsigned int USBCardConfig::readDefaultChannelMask(bool is_playback) {
    unsigned int ret = 0;
    int channels = getMaxChannels(is_playback);

    if (channels > MAX_HIFI_CHANNEL_COUNT)
        channels = MAX_HIFI_CHANNEL_COUNT;

    if (is_playback) {
        if (channels >= DEFAULT_CHANNEL_COUNT)
            ret = out_chn_mask_[channels - DEFAULT_CHANNEL_COUNT];
    } else {
        if (channels >= MIN_CHANNEL_COUNT)
            ret = in_chn_mask_[channels - MIN_CHANNEL_COUNT];
    }

    return ret;
}

int USBCardConfig::readSupportedConfig(struct dynamic_media_config *config, bool is_playback)
{
    config->format = readDefaultFormat(is_playback);
    config->sample_rate = readDefaultSampleRate(is_playback);
    config->mask = readDefaultChannelMask(is_playback);

    return 0;
}
#ifdef SEC_AUDIO_SUPPORT_UHQ
int USBCardConfig::readBestConfig(struct pal_media_config *config,
                                struct pal_stream_attributes *sattr, bool is_playback,
                                struct pal_device_info *devinfo, pal_uhqa_state state)
#else
int USBCardConfig::readBestConfig(struct pal_media_config *config,
                                struct pal_stream_attributes *sattr, bool is_playback,
                                struct pal_device_info *devinfo)
#endif
{
    typename std::vector<std::shared_ptr<USBDeviceConfig>>::iterator iter;
    USBDeviceConfig *candidate_config = nullptr;
    int max_bit_width = 0;
    int bitwidth = 16;
    int ret = -EINVAL;
    struct pal_media_config media_config;

#ifdef SEC_AUDIO_SUPPORT_UHQ
    if (sattr->type != PAL_STREAM_DEEP_BUFFER &&
        sattr->type != PAL_STREAM_COMPRESSED) {
        PAL_INFO(LOG_TAG, "PAL_UHQ_STATE_NORMAL for type = %d", sattr->type);
        state = PAL_UHQ_STATE_NORMAL;
    }
#endif

    for (iter = usb_device_config_list_.begin();
         iter != usb_device_config_list_.end(); iter++) {
        if ((*iter)->getType() == is_playback) {
            if (is_playback) {
                PAL_INFO(LOG_TAG, "USB output");
                media_config = sattr->out_media_config;
            } else {
                PAL_INFO(LOG_TAG, "USB input");
                media_config = sattr->in_media_config;
            }

             // 1. search for matching bitwidth
             // only one bitwidth for one usb device config.
            bitwidth = (*iter)->getBitWidth();

            if (bitwidth == devinfo->bit_width) {
#ifdef SEC_AUDIO_SUPPORT_UHQ
                if (state == PAL_UHQ_STATE_384KHZ) {
                    bitwidth = getMaxBitWidth(is_playback);
                }
#endif
                config->bit_width = bitwidth;
                PAL_INFO(LOG_TAG, "found matching BitWidth = %d", config->bit_width);
                 /* 2. sample rate: Check if the custom sample rate set for device in RM.xml
                 is supported and then set it, otherwise set the rate based on stream attribute */
#ifdef SEC_AUDIO_SUPPORT_UHQ
                if (is_playback && state != PAL_UHQ_STATE_NORMAL) {
                    if (state == PAL_UHQ_STATE_384KHZ)
                        ret = (*iter)->isCustomRateSupported(SAMPLINGRATE_384K, &config->sample_rate);
                    if (ret != 0)
                        ret = (*iter)->isCustomRateSupported(SAMPLINGRATE_192K, &config->sample_rate);
                    if (ret != 0)
                        ret = (*iter)->isCustomRateSupported(SAMPLINGRATE_96K, &config->sample_rate);
                }
                if (ret != 0)
#endif
                    ret = (*iter)->isCustomRateSupported(devinfo->samplerate, &config->sample_rate);
                if (ret != 0)
                    ret = (*iter)->getBestRate(media_config.sample_rate,
                                    &config->sample_rate);
                PAL_INFO(LOG_TAG, "found matching SampleRate = %d", config->sample_rate);
                // 3. get channel
                ret = (*iter)->getBestChInfo(&media_config.ch_info,
                                    &config->ch_info);
                PAL_INFO(LOG_TAG, "found matching Channels = %d", config->ch_info.channels);
                break;
            } else {
                // if bit width does not match, use highest width.
                if (bitwidth > max_bit_width) {
                    max_bit_width = bitwidth;
                    candidate_config = (*iter).get();
                }
            }
        }
    }
    if (iter == usb_device_config_list_.end()) {
        if (candidate_config) {
            PAL_INFO(LOG_TAG, "Default bitwidth of %d is not supported by USB. Use USB width of %d",
                         devinfo->bit_width, max_bit_width);
#ifdef SEC_AUDIO_SUPPORT_UHQ
            if (state == PAL_UHQ_STATE_384KHZ) {
                bitwidth = getMaxBitWidth(is_playback);
            }
#endif
            config->bit_width = bitwidth;
#ifdef SEC_AUDIO_SUPPORT_UHQ
            if (is_playback && state != PAL_UHQ_STATE_NORMAL) {
                if (state == PAL_UHQ_STATE_384KHZ)
                    ret = candidate_config->isCustomRateSupported(SAMPLINGRATE_384K,
                                    &config->sample_rate);
                if (ret != 0)
                    ret = candidate_config->isCustomRateSupported(SAMPLINGRATE_192K,
                                    &config->sample_rate);
                if (ret != 0)
                    ret = candidate_config->isCustomRateSupported(SAMPLINGRATE_96K,
                                    &config->sample_rate);
            }
            if (ret != 0)
#endif
                ret = candidate_config->isCustomRateSupported(devinfo->samplerate,
                                    &config->sample_rate);
            if (ret != 0)
                ret = candidate_config->getBestRate(media_config.sample_rate,
                                &config->sample_rate);
            ret = candidate_config->getBestChInfo(&media_config.ch_info,
                                &config->ch_info);
        } else {
            PAL_ERR(LOG_TAG, "%s is not supported.", is_playback?"playback":"capture");
            ret = -EINVAL;
        }
    }
    return ret;
}

#ifdef SEC_AUDIO_USB_GAIN_CONTROL
struct audio_route *USBCardConfig::getUSBAudioRoute() {
    return address_.usb_ar;
}

void USBCardConfig::USBAudioGainLoadXml(struct pal_usb_device_address addr)
{
    int ret = 0;
    char xml_path_name[MAX_PATH_LEN] = {0};
    std::string chip = "";
#ifdef SEC_AUDIO_QUICK_USB_DETECTION
    char path[MAX_PATH_LEN] = {0};
    int retries = USB_PCM_OPEN_RETRY_CNT;
#endif

    if (addr.pid == SEC_GRAY_HEADPHONE_PID
        || addr.pid == SEC_GRAY_HEADSET_PID ){
        chip = "gray";
    } else {
        chip = "default";
    }

    /* Form the gain xml name: <mixer_usb>_<chip>.xml */
    ret = snprintf(xml_path_name, sizeof(xml_path_name), "%s%s%s",
        GAIN_XML_PATH, chip.c_str(), ".xml");
    if (ret < 0) {
        PAL_ERR(LOG_TAG, "failed to create path for gain XML\n");
        goto done;
    }

    PAL_INFO(LOG_TAG,"Mixer gain XML = %s\n", xml_path_name);
#ifdef SEC_AUDIO_QUICK_USB_DETECTION
    snprintf(path, sizeof(path), "/dev/snd/controlC%u", addr.card_id);
    while (retries--) {
        if (usb_recognition_state(addr.card_id) && (access(path, F_OK) < 0)) {
            PAL_INFO(LOG_TAG, "%s doesn't exist, retrying\n", path);
            usleep(20000);
        } else {
            break;
        }
    }
#endif
    /* Open the gain xml using the audio_route library */
    address_.usb_ar = audio_route_init(addr.card_id, xml_path_name);

    if (!address_.usb_ar) {
        PAL_ERR(LOG_TAG,"Unable to parse gain XML at - %s\n", xml_path_name);
    }

done:
    return;
}

void USBCardConfig::USBAudioGainControl(struct pal_usb_device_address addr, pal_device_id_t deviceId)
{
    int card, ret = 0;
    char path[MAX_PATH_LEN], read_buf[USBID_SIZE];
    char *endptr;
    int32_t fd = -1;
#ifdef SEC_AUDIO_QUICK_USB_DETECTION
    int retries = USB_PCM_OPEN_RETRY_CNT;
#endif

    card = addr.card_id;

    memset(path, 0, sizeof(path));
    memset(read_buf, 0, sizeof(read_buf));

    /* read usbid file of this sound card to get the VID:PID */
    ret = snprintf(path, sizeof(path), "/proc/asound/card%u/usbid",
             card);
    if (ret < 0) {
        PAL_ERR(LOG_TAG, "failed on snprintf (%d) to path %s\n", ret, path);
        goto done;
    }
#ifdef SEC_AUDIO_QUICK_USB_DETECTION
    while (retries--) {
        if (usb_recognition_state(addr.card_id) && (access(path, F_OK) < 0)) {
            PAL_INFO(LOG_TAG, "%s doesn't exist, retrying\n", path);
            usleep(20000);
        } else {
            break;
        }
    }
#endif

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        PAL_ERR(LOG_TAG, "error failed to open usbid file %s error: %d\n", path, errno);
        ret = -EINVAL;
        goto done;
    }

    ret = read(fd, read_buf, USBID_SIZE);
    if (ret < 0) {
        PAL_ERR(LOG_TAG, "file read error\n");
        goto done;
    }

    /*
     * - Output of "cat /proc/asound/card%u/usbid" is of format "VID:PID"
     * where each VID and PID is 4 chars long.
     */
    addr.vid = (int)strtol(read_buf, &endptr, 16);
    if (endptr == NULL || *endptr == '\0' || *endptr != ':') {
        PAL_ERR(LOG_TAG, "failed to parse USB VID");
        addr.vid = -1;
        goto done;
    }
    addr.pid = (int)strtol((endptr+1), &endptr, 16);

    PAL_INFO(LOG_TAG, "vid = %d, pid = %d\n", addr.vid, addr.pid);

    /* Check the VID and range(s) of PID. Proceed further only if we support */
    if (addr.vid == SEC_VID) {
        USBAudioGainLoadXml(addr);
#ifdef SEC_AUDIO_USB_PROFILING
        if (USB::isUSBOutDevice(deviceId)) {
            PAL_INFO(LOG_TAG, "USB_Profiling_register_card");
        }
#endif
    } else {
        PAL_VERBOSE(LOG_TAG, "Incompatible USB audio device. No GAIN control\n");
    }

done:
    if (fd >= 0) close(fd);
    return;
}
#endif

const unsigned int USBDeviceConfig::supported_sample_rates_[] =
    {384000, 352800, 192000, 176400, 96000, 88200, 64000,
     48000, 44100, 32000, 22050, 16000, 11025, 8000};

void USBDeviceConfig::setBitWidth(unsigned int bit_width) {
    bit_width_ = bit_width;
}

void USBDeviceConfig::setChannels(unsigned int channels) {
    channels_ = channels;
}

void USBDeviceConfig::setType(usb_usecase_type_t type) {
    type_ = type;
}

void USBDeviceConfig::setInterval(unsigned long interval) {
    service_interval_us_ = interval;
}

unsigned int USBDeviceConfig::getBitWidth() {
    return bit_width_;
}

unsigned int USBDeviceConfig::getChannels() {
    return channels_;
}

unsigned int USBDeviceConfig::getType() {
    return type_;
}

unsigned long USBDeviceConfig::getInterval() {
    return service_interval_us_;
}

unsigned int USBDeviceConfig::getDefaultRate() {
    return rates_[0];
}

int USBDeviceConfig::isCustomRateSupported(int requested_rate, unsigned int *best_rate)
{
    int i = 0;
    int cur_rate = 0;
    for (i = 0; i < rate_size_; i++) {
        cur_rate = rates_[i];
        if (requested_rate == cur_rate) {
            *best_rate = requested_rate;
             return 0;
        }
    }
    PAL_INFO(LOG_TAG, "requested rate not supported = %d", requested_rate);
    return -EINVAL;
}

// return 0 if match, else return -EINVAL with default sample rate
int USBDeviceConfig::getBestRate(int requested_rate, unsigned int *best_rate) {
    int i = 0;
    int nearestRate = 0;
    int diff = requested_rate;
    int cur_rate = 0;

    for (i = 0; i < rate_size_; i++) {
        cur_rate = rates_[i];
        if (requested_rate == cur_rate) {
            *best_rate = requested_rate;
            return 0;
        } else if (abs(double(requested_rate - cur_rate)) <= diff) {
            nearestRate = cur_rate;
            diff = abs(double(requested_rate - cur_rate));
        }
        PAL_VERBOSE(LOG_TAG, "nearestRate %d, requested_rate %d", nearestRate, requested_rate);
    }
    if (nearestRate == 0)
        nearestRate = rates_[0];

    *best_rate = nearestRate;

    return 0;
}

// return 0 if match, else return -EINVAL with default sample rate
int USBDeviceConfig::getBestChInfo(struct pal_channel_info *requested_ch_info,
                                        struct pal_channel_info *best_ch_info)
{
    struct pal_channel_info usb_ch_info;

    usb_ch_info.channels = channels_;
    for (int i = 0; i < channels_; i++) {
        usb_ch_info.ch_map[i] = PAL_CHMAP_CHANNEL_FL + i;
    }

    *best_ch_info = usb_ch_info;

    if (channels_ != requested_ch_info->channels)
        PAL_ERR(LOG_TAG, "channel num mismatch. use USB's");

    return 0;
}

int USBDeviceConfig::getSampleRates(int type, char *rates_str) {
    unsigned int i;
    char *next_sr_string, *temp_ptr;
    unsigned int sr, min_sr, max_sr, sr_size = 0;

    /* Sample rate string can be in any of the folloing two bit_widthes:
     * Rates: 8000 - 48000 (continuous)
     * Rates: 8000, 44100, 48000
     * Support both the bit_widths
     */

    PAL_VERBOSE(LOG_TAG, "rates_str %s", rates_str);
    next_sr_string = strtok_r(rates_str, "Rates: ", &temp_ptr);
    if (next_sr_string == NULL) {
        PAL_ERR(LOG_TAG, "could not find min rates string");
        return -EINVAL;
    }
    if (strstr(rates_str, "continuous") != NULL) {
        min_sr = (unsigned int)atoi(next_sr_string);
        next_sr_string = strtok_r(NULL, " ,.-", &temp_ptr);
        if (next_sr_string == NULL) {
            PAL_ERR(LOG_TAG, "could not find max rates string");
            return -EINVAL;
        }
        max_sr = (unsigned int)atoi(next_sr_string);

        for (i = 0; i < MAX_SAMPLE_RATE_SIZE; i++) {
            if (supported_sample_rates_[i] >= min_sr &&
                supported_sample_rates_[i] <= max_sr) {
                // FIXME: we don't support >192KHz in recording path for now
                if ((supported_sample_rates_[i] > SAMPLE_RATE_192000) &&
                        (type == USB_CAPTURE))
                    continue;
                rates_[sr_size++] = supported_sample_rates_[i];
                supported_sample_rates_mask_[type] |= (1<<i);
                PAL_DBG(LOG_TAG, "continuous sample rate supported_sample_rates_[%d] %d",
                        i, supported_sample_rates_[i]);
            }
        }
    } else {
        do {
            sr = (unsigned int)atoi(next_sr_string);
            // FIXME: we don't support >192KHz in recording path for now
            if ((sr > SAMPLE_RATE_192000) && (type == USB_CAPTURE)) {
                next_sr_string = strtok_r(NULL, " ,.-", &temp_ptr);
                continue;
            }

            for (i = 0; i < MAX_SAMPLE_RATE_SIZE; i++) {
                if (supported_sample_rates_[i] == sr) {
                    PAL_DBG(LOG_TAG, "sr %d, supported_sample_rates_[%d] %d -> matches!!",
                              sr, i, supported_sample_rates_[i]);
                    rates_[sr_size++] = supported_sample_rates_[i];
                    supported_sample_rates_mask_[type] |= (1<<i);
                }
            }
            next_sr_string = strtok_r(NULL, " ,.-", &temp_ptr);
        } while (next_sr_string != NULL);
    }
    rate_size_ = sr_size;
    return 0;
}

int USBDeviceConfig::getServiceInterval(const char *interval_str_start)
{
    unsigned long interval = 0;
    char time_unit[8] = {0};
    int multiplier = 0;

    const char *eol = strchr(interval_str_start, '\n');
    if (!eol) {
        PAL_ERR(LOG_TAG, "No EOL found");
        return -1;
    }
    char *tmp = (char *)calloc(1, eol-interval_str_start+1);
    if (!tmp) {
        PAL_ERR(LOG_TAG, "failed to allocate tmp");
        return -1;
    }
    memcpy(tmp, interval_str_start, eol-interval_str_start);
    tmp[eol-interval_str_start] = '\0';
    sscanf(tmp, "%lu %2s", &interval, &time_unit[0]);
    if (!strcmp(time_unit, "us")) {
        multiplier = 1;
    } else if (!strcmp(time_unit, "ms")) {
        multiplier = 1000;
    } else if (!strcmp(time_unit, "s")) {
        multiplier = 1000000;
    } else {
        PAL_ERR(LOG_TAG, "unknown time_unit %s, assume default", time_unit);
        interval = DEFAULT_SERVICE_INTERVAL_US;
        multiplier = 1;
    }
    interval *= multiplier;
    PAL_DBG(LOG_TAG, "set service_interval_us %lu", interval);
    service_interval_us_ = interval;
    free(tmp);

    return 0;
}

bool USB::isUsbAlive(int card)
{
    char path[128];
    (void) snprintf(path, sizeof(path), "/proc/asound/card%u/stream0", card);
    if (access(path,F_OK)) {
        PAL_ERR(LOG_TAG, "failed on snprintf (%d) to path %s\n", card, path);
        return false;
    }

    PAL_INFO(LOG_TAG, "usb card is accessible");
    return true;
}

bool USB::isUsbConnected(struct pal_usb_device_address addr)
{
    if (isUsbAlive(addr.card_id)) {
        PAL_INFO(LOG_TAG, "usb device is connected");
        return true;
    }

    PAL_ERR(LOG_TAG, "usb device is not connected");
    return false;
}

#ifdef SEC_AUDIO_FMRADIO
int32_t USB::setParameter(uint32_t param_id, void *param)
{
    int ret = 0;

    PAL_DBG(LOG_TAG, " Enter.");

    if (param_id == PAL_PARAM_ID_FMRADIO_USB_GAIN) {
        pal_param_fmradio_usb_gain_t* param_fmradio_usb_gain = (pal_param_fmradio_usb_gain_t *)param;
        if (param_fmradio_usb_gain->enable) {
            typename std::vector<std::shared_ptr<USBCardConfig>>::iterator iter;
            for (iter = usb_card_config_list_.begin(); iter != usb_card_config_list_.end(); iter++) {
                if ((*iter)->isConfigCached(deviceAttr.address)) {
                    /* Apply gain only if audio_route library is initialized */
                    if ((*iter)->getUSBAudioRoute()) {
                        enableDevice((*iter)->getUSBAudioRoute(), "fm-usb-headphones-gain");
                    }
                    break;
                }
            }
        } else {
            setUSBAudioGain(); // restore gain
        }
    }
    return ret;
}
#endif