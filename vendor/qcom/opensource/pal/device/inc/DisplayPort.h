/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
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

#ifndef DISPLAYPORT_H
#define DISPLAYPORT_H

#include "Device.h"
#include "PalAudioRoute.h"
#include "PalDefs.h"
#include "ResourceManager.h"
#include <system/audio.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

/* HDMI EDID Information */
#define BIT(nr)     (1UL << (nr))
#define MAX_EDID_BLOCKS 10
#define MAX_SHORT_AUDIO_DESC_CNT        30
#define MIN_AUDIO_DESC_LENGTH           3
#define MIN_SPKR_ALLOCATION_DATA_LENGTH 3
#define MAX_CHANNELS_SUPPORTED          8
#define MAX_DISPLAY_DEVICES             3
#define MAX_FRAME_BUFFER_NAME_SIZE      80
#define MAX_CHAR_PER_INT                13

#define PCM_CHANNEL_FL    1  /* Front left channel.                           */
#define PCM_CHANNEL_FR    2  /* Front right channel.                          */
#define PCM_CHANNEL_FC    3  /* Front center channel.                         */
#define PCM_CHANNEL_LS    4  /* Left surround channel.                        */
#define PCM_CHANNEL_RS    5  /* Right surround channel.                       */
#define PCM_CHANNEL_LFE   6  /* Low frequency effect channel.                 */
#define PCM_CHANNEL_CS    7  /* Center surround channel; Rear center channel. */
#define PCM_CHANNEL_LB    8  /* Left back channel; Rear left channel.         */
#define PCM_CHANNEL_RB    9  /* Right back channel; Rear right channel.       */
#define PCM_CHANNEL_TS   10  /* Top surround channel.                         */
#define PCM_CHANNEL_CVH  11  /* Center vertical height channel.               */
#define PCM_CHANNEL_MS   12  /* Mono surround channel.                        */
#define PCM_CHANNEL_FLC  13  /* Front left of center.                         */
#define PCM_CHANNEL_FRC  14  /* Front right of center.                        */
#define PCM_CHANNEL_RLC  15  /* Rear left of center.                          */
#define PCM_CHANNEL_RRC  16  /* Rear right of center.                         */
#define PCM_CHANNEL_LFE2 17  /* Second low frequency channel.                 */
#define PCM_CHANNEL_SL   18  /* Side left channel.                            */
#define PCM_CHANNEL_SR   19  /* Side right channel.                           */
#define PCM_CHANNEL_TFL  20  /* Top front left channel.                       */
#define PCM_CHANNEL_LVH  20  /* Left vertical height channel.                 */
#define PCM_CHANNEL_TFR  21  /* Top front right channel.                      */
#define PCM_CHANNEL_RVH  21  /* Right vertical height channel.                */
#define PCM_CHANNEL_TC   22  /* Top center channel.                           */
#define PCM_CHANNEL_TBL  23  /* Top back left channel.                        */
#define PCM_CHANNEL_TBR  24  /* Top back right channel.                       */
#define PCM_CHANNEL_TSL  25  /* Top side left channel.                        */
#define PCM_CHANNEL_TSR  26  /* Top side right channel.                       */
#define PCM_CHANNEL_TBC  27  /* Top back center channel.                      */
#define PCM_CHANNEL_BFC  28  /* Bottom front center channel.                  */
#define PCM_CHANNEL_BFL  29  /* Bottom front left channel.                    */
#define PCM_CHANNEL_BFR  30  /* Bottom front right channel.                   */
#define PCM_CHANNEL_LW   31  /* Left wide channel.                            */
#define PCM_CHANNEL_RW   32  /* Right wide channel.                           */
#define PCM_CHANNEL_LSD  33  /* Left side direct channel.                     */
#define PCM_CHANNEL_RSD  34  /* Right side direct channel.                    */

#define MAX_HDMI_CHANNEL_CNT 8

#define EXT_DISPLAY_PLUG_STATUS_NOTIFY_ENABLE      0x30
#define EXT_DISPLAY_PLUG_STATUS_NOTIFY_CONNECT     0x01
#define EXT_DISPLAY_PLUG_STATUS_NOTIFY_DISCONNECT  0x00

typedef enum edidAudioFormatId {
    LPCM = 1,
    AC3,
    MPEG1,
    MP3,
    MPEG2_MULTI_CHANNEL,
    AAC,
    DTS,
    ATRAC,
    SACD,
    DOLBY_DIGITAL_PLUS,
    DTS_HD,
    MAT,
    DST,
    WMA_PRO
} edidAudioFormatId;

typedef struct edidAudioBlockInfo {
    edidAudioFormatId formatId;
    int samplingFreqBitmask;
    int bitsPerSampleBitmask;
    int channels;
} edidAudioBlockInfo;

typedef struct edidAudioInfo {
    int audioBlocks;
    unsigned char speakerAllocation[MIN_SPKR_ALLOCATION_DATA_LENGTH];
    edidAudioBlockInfo audioBlocksArray[MAX_EDID_BLOCKS];
    char channelMap[MAX_CHANNELS_SUPPORTED];
    int  channelAllocation;
    unsigned int  channelMask;
} edidAudioInfo;

class DisplayPort : public Device
{
    uint32_t dp_controller;
    uint32_t dp_stream;
protected:
    int configureDpEndpoint();
    static std::shared_ptr<Device> objRx;
    static std::shared_ptr<Device> objTx;
    DisplayPort(struct pal_device *device, std::shared_ptr<ResourceManager> Rm);
public:
    int start();
    int init(pal_param_device_connection_t device_conn);
    int deinit(pal_param_device_connection_t device_conn);
    static std::shared_ptr<Device> getInstance(struct pal_device *device,
                                               std::shared_ptr<ResourceManager> Rm);
    static std::shared_ptr<Device> getObject(pal_device_id_t id);
    static int32_t isSampleRateSupported(uint32_t sampleRate);
    static int32_t isChannelSupported(uint32_t numChannels);
    static int32_t isBitWidthSupported(uint32_t bitWidth);
    static int32_t checkAndUpdateBitWidth(uint32_t *bitWidth);
    static int32_t checkAndUpdateSampleRate(uint32_t *sampleRate);
    static bool isDisplayPortEnabled ();
    void resetEdidInfo();
    static int32_t getDisplayPortCtlIndex(int controller, int stream);
    static int32_t setExtDisplayDevice(struct audio_mixer *mixer, int controller, int stream);
    static int32_t getExtDispType(struct audio_mixer *mixer, int controller, int stream);
    static int getEdidInfo(struct audio_mixer *mixer, int controller, int stream);
    static void cacheEdid(struct audio_mixer *mixer, int controller, int stream);
    static const char * edidFormatToStr(unsigned char format);
    static bool isSampleRateSupported(unsigned char srByte, int samplingRate);
    static unsigned char getEdidBpsByte(unsigned char byte, unsigned char format);
    static bool isSupportedBps(unsigned char bpsByte, int bps);
    static int getHighestEdidSF(unsigned char byte);
    static void updateChannelMap(edidAudioInfo* info);
    static void dumpSpeakerAllocation(edidAudioInfo* info);
    static void updateChannelAllocation(edidAudioInfo* info);
    static void updateChannelMapLpass(edidAudioInfo* info);
    static void retrieveChannelMapLpass(int ca, uint8_t *ch_map, int ch_map_size);
    static void updateChannelMask(edidAudioInfo* info);
    static void dumpEdidData(edidAudioInfo *info);
    static bool getSinkCaps(edidAudioInfo* info, char *edidData);
    static int getDeviceChannelAllocation(int num_channels);
    bool isSupportedSR(edidAudioInfo* info, int sr);
    int getMaxChannel();
    bool isSupportedBps(edidAudioInfo* info, int bps);
    int getHighestSupportedSR();
    int getHighestSupportedBps();
    int updateSysfsNode(const char *path, const char *data, size_t len);
    int getExtDispSysfsNodeIndex(int ext_disp_type);
    int updateExtDispSysfsNode(int node_value, int controller, int stream);
    int updateAudioAckState(int node_value, int controller, int stream);
    int getDeviceAttributes (struct pal_device *dattr) override;
};


#endif //DISPLAYPORT_H
