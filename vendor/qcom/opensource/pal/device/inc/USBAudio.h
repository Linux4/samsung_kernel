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

#ifndef USB_H
#define USB_H

#include "Device.h"
#include "ResourceManager.h"
#include "PalAudioRoute.h"
#include "SessionAlsaUtils.h"
#ifdef SEC_AUDIO_COMMON
#include "SecPalDefs.h"
#endif

#include <vector>

#define USB_BUFF_SIZE           2048
#define CHANNEL_NUMBER_STR      "Channels: "
#define PLAYBACK_PROFILE_STR    "Playback:"
#define CAPTURE_PROFILE_STR     "Capture:"
#define DATA_PACKET_INTERVAL_STR "Data packet interval:"
#define USB_SIDETONE_GAIN_STR   "usb_sidetone_gain"
// Supported sample rates for USB
#define USBID_SIZE                16
/* support positional and index masks to 8ch */
#define MAX_SUPPORTED_CHANNEL_MASKS 8
#define MAX_HIFI_CHANNEL_COUNT 8
#define MIN_CHANNEL_COUNT 1
#define DEFAULT_CHANNEL_COUNT 2
#define  MAX_SAMPLE_RATE_SIZE 14
#define DEFAULT_SERVICE_INTERVAL_US    0

#ifdef SEC_AUDIO_USB_GAIN_CONTROL
#define MAX_PATH_LEN   128
#define GAIN_XML_PATH    "/vendor/etc/mixer_usb_"

// Define all IDs for samsung usb headset
#define SEC_VID                     0x04e8
#define SEC_GRAY_HEADPHONE_PID      0xa04b
#define SEC_GRAY_HEADSET_PID        0xa04c
#endif

typedef enum usb_usecase_type{
    USB_CAPTURE = 0,
    USB_PLAYBACK,
} usb_usecase_type_t;

// one card supports multiple devices
class USBDeviceConfig {
protected:
    unsigned int bit_width_;
    unsigned int channels_;
    unsigned int rate_size_;
    unsigned int rates_[MAX_SAMPLE_RATE_SIZE];
    unsigned long service_interval_us_;
    usb_usecase_type_t type_;
    unsigned int supported_sample_rates_mask_[2];
public:
    void setBitWidth(unsigned int bit_width);
    unsigned int getBitWidth();
    void setChannels(unsigned int channels);
    unsigned int getChannels();
    void setType(usb_usecase_type_t type);
    unsigned int getType();
    void setInterval(unsigned long interval);
    unsigned long getInterval();
    unsigned int getDefaultRate();
    int getSampleRates(int type, char *rates_str);
    int getBestRate(int requested_rate, unsigned int *best_rate);
    int getBestChInfo(struct pal_channel_info *requested_ch_info,
                        struct pal_channel_info *best);
    int getServiceInterval(const char *interval_str_start);
    static const unsigned int supported_sample_rates_[MAX_SAMPLE_RATE_SIZE];
    int isCustomRateSupported(int requested_rate, unsigned int *best_rate);
};

class USBCardConfig {
protected:
     struct pal_usb_device_address address_;
    int endian_;
    std::vector <std::shared_ptr<USBDeviceConfig>> usb_device_config_list_;
public:
    USBCardConfig(struct pal_usb_device_address address);
    bool isConfigCached(struct pal_usb_device_address addr);
    void setEndian(int endian);
    int getCapability(usb_usecase_type_t type, struct pal_usb_device_address addr);
    int getMaxBitWidth(bool is_playback);
    int getMaxChannels(bool is_playback);
    unsigned int getFormatByBitWidth(int bitwidth);
    unsigned int readDefaultFormat(bool is_playback);
    unsigned int readDefaultSampleRate(bool is_playback);
    unsigned int readDefaultChannelMask(bool is_playback);
    int readSupportedConfig(dynamic_media_config_t *config, bool is_playback);
#ifdef SEC_AUDIO_SUPPORT_UHQ
    int readBestConfig(struct pal_media_config *config,
                                    struct pal_stream_attributes *sattr,
                                    bool is_playback, struct pal_device_info *devinfo,
                                    pal_uhqa_state uhqa_state);
#else
    int readBestConfig(struct pal_media_config *config,
                                    struct pal_stream_attributes *sattr,
                                    bool is_playback, struct pal_device_info *devinfo);
#endif
    unsigned int getMax(unsigned int a, unsigned int b);
    unsigned int getMin(unsigned int a, unsigned int b);
    static const unsigned int out_chn_mask_[MAX_SUPPORTED_CHANNEL_MASKS];
    static const unsigned int in_chn_mask_[MAX_SUPPORTED_CHANNEL_MASKS];
    bool isCaptureProfileSupported();
#ifdef SEC_AUDIO_USB_GAIN_CONTROL
    struct audio_route *getUSBAudioRoute();
    void USBAudioGainLoadXml(struct pal_usb_device_address addr);
    void USBAudioGainControl(struct pal_usb_device_address addr, pal_device_id_t deviceId);
#endif
};

class USB : public Device
{
protected:
    static std::shared_ptr<Device> objRx;
    static std::shared_ptr<Device> objTx;
    std::vector <std::shared_ptr<USBCardConfig>> usb_card_config_list_;
    int configureUsb();
    USB(struct pal_device *device, std::shared_ptr<ResourceManager> Rm);
public:
    int start();
    int init(pal_param_device_connection_t device_conn);
    int deinit(pal_param_device_connection_t device_conn);
    int getDefaultConfig(pal_param_device_capability_t capability);
    static bool isUsbConnected(struct pal_usb_device_address addr);
    static bool isUsbAlive(int card);
    int selectBestConfig(struct pal_device *dattr,
                                   struct pal_stream_attributes *sattr,
                                   bool is_playback, struct pal_device_info *devinfo);
#ifdef SEC_AUDIO_USB_GAIN_CONTROL
    int setUSBAudioGain();
#endif
#ifdef SEC_AUDIO_FMRADIO
    int32_t setParameter(uint32_t param_id, void *param) override;
#endif
    static std::shared_ptr<Device> getInstance(struct pal_device *device,
                                               std::shared_ptr<ResourceManager> Rm);
    static int32_t isSampleRateSupported(unsigned int sampleRate);
    static int32_t isChannelSupported(unsigned int numChannels);
    static int32_t isBitWidthSupported(unsigned int bitWidth);
    static int32_t checkAndUpdateBitWidth(unsigned int *bitWidth);
    static int32_t checkAndUpdateSampleRate(unsigned int *sampleRate);
    static bool isUSBOutDevice(pal_device_id_t);
    static std::shared_ptr<Device> getObject(pal_device_id_t id);
    ~USB();
};


#endif //USB_H
