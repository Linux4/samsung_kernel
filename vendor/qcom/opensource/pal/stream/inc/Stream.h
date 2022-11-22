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

#ifndef STREAM_H_
#define STREAM_H_

#include "PalDefs.h"
#include <algorithm>
#include <vector>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <memory>
#include <mutex>
#include <exception>
#include <errno.h>
#include "PalCommon.h"

typedef enum {
    DATA_MODE_SHMEM = 0,
    DATA_MODE_BLOCKING ,
    DATA_MODE_NONBLOCKING
} dataFlags;

typedef enum {
    STREAM_IDLE = 0,
    STREAM_INIT,
    STREAM_OPENED,
    STREAM_STARTED,
    STREAM_PAUSED,
    STREAM_SUSPENDED,
    STREAM_STOPPED
} stream_state_t;

#define BUF_SIZE_PLAYBACK 1024
#define BUF_SIZE_CAPTURE 960
#define NO_OF_BUF 4
#define MUTE_TAG 0
#define UNMUTE_TAG 1
#define PAUSE_TAG 2
#define RESUME_TAG 3
#define MFC_SR_8K 4
#define MFC_SR_16K 5
#define MFC_SR_32K 6
#define MFC_SR_44K 7
#define MFC_SR_48K 8
#define MFC_SR_96K 9
#define MFC_SR_192K 10
#define MFC_SR_384K 11
#define ECNS_ON_TAG 12
#define ECNS_OFF_TAG 13
#define EC_ON_TAG 14
#define NS_ON_TAG 15
#define CHS_1 16
#define CHS_2 17
#define CHS_3 18
#define CHS_4 19
#define CHS_5 20
#define CHS_6 21
#define CHS_7 22
#define CHS_8 23
#define BW_16 24
#define BW_24 25
#define BW_32 26
#define TTY_MODE 27
#define VOICE_SLOW_TALK_OFF 28
#define VOICE_SLOW_TALK_ON 29
#define VOICE_VOLUME_BOOST 30
#define SPKR_PROT_ENABLE 31
#define INCALL_RECORD_UPLINK 32
#define INCALL_RECORD_DOWNLINK 33
#define INCALL_RECORD_UPLINK_DOWNLINK_MONO 34
#define INCALL_RECORD_UPLINK_DOWNLINK_STEREO 35
#define SPKR_VI_ENABLE 36
#define VOICE_HD_VOICE 37
#define LPI_LOGGING_ON 38
#define LPI_LOGGING_OFF 39
#define DEVICE_MUTE 40
#define DEVICE_UNMUTE 41
#define CHANNEL_INFO 42
#define CHARGE_CONCURRENCY_ON_TAG 43
#define CHARGE_CONCURRENCY_OFF_TAG 44

/* This sleep is added to give time to kernel and
 * spf to recover from SSR so that audio-hal will
 * not continously try to open a session if it fails
 * during SSR.
 */
#define SSR_RECOVERY 10000

/* Soft pause has to wait for ramp period to ensure volume stepping finishes.
 * This period of time was previously consumed in elite before acknowleging
 * pause completion. But it's not the case in Gecko.
 * FIXME: load the ramp period config from acdb.
 */
#define VOLUME_RAMP_PERIOD (100*1000)

class Device;
class ResourceManager;
class Session;

class Stream
{
protected:
    uint32_t mNoOfDevices;
    std::vector <std::shared_ptr<Device>> mDevices;
    std::vector <struct pal_device> mPalDevice;
    Session* session;
    struct pal_stream_attributes* mStreamAttr;
    struct pal_volume_data* mVolumeData = NULL;
    int mGainLevel;
    std::mutex mStreamMutex;
    static std::mutex mBaseStreamMutex; //TBD change this. as having a single static mutex for all instances of Stream is incorrect. Replace
    static std::shared_ptr<ResourceManager> rm;
    struct modifier_kv *mModifiers;
    uint32_t mNoOfModifiers;
    std::string mStreamSelector;
    std::string mDevPPSelector;
    size_t inBufSize;
    size_t outBufSize;
    size_t inBufCount;
    size_t outBufCount;
    size_t outMaxMetadataSz;
    size_t inMaxMetadataSz;
    stream_state_t currentState;
    stream_state_t cachedState;
    uint32_t mInstanceID = 0;
    static std::condition_variable pauseCV;
    static std::mutex pauseMutex;
    int connectToDefaultDevice(Stream* streamHandle, uint32_t dir);
public:
    virtual ~Stream() {};
    pal_stream_callback streamCb;
    uint64_t cookie;
    bool isPaused = false;
    bool a2dpMuted = false;
    bool a2dpPaused = false;
    std::vector<pal_device_id_t> suspendedDevIds;
    virtual int32_t open() = 0;
    virtual int32_t close() = 0;
    virtual int32_t start() = 0;
    virtual int32_t stop() = 0;
    virtual int32_t prepare() = 0;
    virtual int32_t drain(pal_drain_type_t type __unused) {return 0;}
    virtual int32_t setStreamAttributes(struct pal_stream_attributes *sattr) = 0;
    virtual int32_t setVolume(struct pal_volume_data *volume) = 0;
    virtual int32_t mute(bool state) = 0;
    virtual int32_t mute_l(bool state) = 0;
    virtual int32_t pause() = 0;
    virtual int32_t pause_l() = 0;
    virtual int32_t resume() = 0;
    virtual int32_t resume_l() = 0;
    virtual int32_t flush() {return 0;}
    virtual int32_t suspend() {return 0;}
    virtual int32_t read(struct pal_buffer *buf) = 0;

    virtual int32_t addRemoveEffect(pal_audio_effect_t effect, bool enable) = 0; //TBD: make this non virtual and prrovide implementation as StreamPCM and StreamCompressed are doing the same things
    virtual int32_t setParameters(uint32_t param_id, void *payload) = 0;
    virtual int32_t write(struct pal_buffer *buf) = 0; //TBD: make this non virtual and prrovide implementation as StreamPCM and StreamCompressed are doing the same things
    virtual int32_t registerCallBack(pal_stream_callback cb, uint64_t cookie) = 0;
    virtual int32_t getCallBack(pal_stream_callback *cb) = 0;
    virtual int32_t getParameters(uint32_t param_id, void **payload) = 0;
    virtual int32_t setECRef(std::shared_ptr<Device> dev, bool is_enable) = 0;
    virtual int32_t setECRef_l(std::shared_ptr<Device> dev, bool is_enable) = 0;
    virtual int32_t ssrDownHandler() = 0;
    virtual int32_t ssrUpHandler() = 0;
    virtual int32_t createMmapBuffer(int32_t min_size_frames __unused,
                                   struct pal_mmap_buffer *info __unused) {return -EINVAL;}
    virtual int32_t GetMmapPosition(struct pal_mmap_position *position __unused) {return -EINVAL;}
    virtual int32_t getTagsWithModuleInfo(size_t *size __unused, uint8_t *payload __unused) {return -EINVAL;};
    int32_t getStreamAttributes(struct pal_stream_attributes *sattr);
    int32_t getModifiers(struct modifier_kv *modifiers,uint32_t *noOfModifiers);
    const std::string& getStreamSelector() const;
    const std::string& getDevicePPSelector() const;
    int32_t getStreamType(pal_stream_type_t* streamType);
    int32_t getStreamDirection(pal_stream_direction_t *dir);
    uint32_t getRenderLatency();
    uint32_t getLatency();
    int32_t getAssociatedDevices(std::vector <std::shared_ptr<Device>> &adevices);
    int32_t getAssociatedPalDevices(std::vector <struct pal_device> &palDevices);
    int32_t UpdatePalDevice(struct pal_device *dattr,  pal_device_id_t Dev_id);
    int32_t getAssociatedSession(Session** session);
    int32_t setBufInfo(pal_buffer_config *in_buffer_config,
                       pal_buffer_config *out_buffer_config);
    int32_t getBufInfo(size_t *in_buf_size, size_t *in_buf_count,
                       size_t *out_buf_size, size_t *out_buf_count);
    int32_t getMaxMetadataSz(size_t *in_max_metadata_sz, size_t *out_max_metadata_sz);
    int32_t getVolumeData(struct pal_volume_data *vData);
    void setGainLevel(int level) { mGainLevel = level; };
    int getGainLevel() { return mGainLevel; };
    /* static so that this method can be accessed wihtout object */
    static Stream* create(struct pal_stream_attributes *sattr, struct pal_device *dattr,
         uint32_t no_of_devices, struct modifier_kv *modifiers, uint32_t no_of_modifiers);
    bool isStreamAudioOutFmtSupported(pal_audio_fmt_t format);
    int32_t getTimestamp(struct pal_session_time *stime);
    int32_t handleBTDeviceNotReady(bool& a2dpSuspend);
    int disconnectStreamDevice(Stream* streamHandle,  pal_device_id_t dev_id);
    int disconnectStreamDevice_l(Stream* streamHandle,  pal_device_id_t dev_id);
    int connectStreamDevice(Stream* streamHandle, struct pal_device *dattr);
    int connectStreamDevice_l(Stream* streamHandle, struct pal_device *dattr);
    int switchDevice(Stream* streamHandle, uint32_t no_of_devices, struct pal_device *deviceArray);
    bool isGKVMatch(pal_key_vector_t* gkv);
    int32_t getEffectParameters(void *effect_query, size_t *payload_size);
    uint32_t getInstanceId() { return mInstanceID; }
    inline void setInstanceId(uint32_t sid) { mInstanceID = sid; }
    bool checkStreamMatch(pal_device_id_t pal_device_id,
                            pal_stream_type_t pal_stream_type);
#ifdef SEC_AUDIO_COMMON
    bool checkStreamEffectMatch(pal_device_id_t pal_device_id,
                            pal_stream_type_t pal_stream_type,
                            uint32_t param_id);
#endif
    int32_t getEffectParameters(void *effect_query);
    int32_t rwACDBParameters(void *payload, uint32_t sampleRate,
                                bool isParamWrite);
    bool isActive() { return currentState == STREAM_STARTED; }
    bool isA2dpMuted() { return a2dpMuted; }
    /* Detection stream related APIs */
    virtual int32_t Resume() { return 0; }
    virtual int32_t Pause() { return 0; }
    virtual int32_t EnableLPI(bool is_enable) { return 0; }
    virtual int32_t HandleConcurrentStream(bool active) { return 0; }
    virtual int32_t DisconnectDevice(pal_device_id_t device_id) { return 0; }
    virtual int32_t ConnectDevice(pal_device_id_t device_id) { return 0; }
    virtual int32_t HandleChargingStateUpdate(bool state, bool active) { return 0; }
    static void handleSoftPauseCallBack(uint64_t hdl, uint32_t event_id, void *data,
                                                           uint32_t event_size);
    static void handleStreamException(struct pal_stream_attributes *attributes,
                                      pal_stream_callback cb, uint64_t cookie);
    void lockStreamMutex() { mStreamMutex.lock();};
    void unlockStreamMutex() { mStreamMutex.unlock();};
};

class StreamNonTunnel : public Stream
{
public:
   StreamNonTunnel(const struct pal_stream_attributes *sattr, struct pal_device *dattr,
             const uint32_t no_of_devices,
             const struct modifier_kv *modifiers, const uint32_t no_of_modifiers,
             const std::shared_ptr<ResourceManager> rm);
   virtual ~StreamNonTunnel();
   int32_t open() override;
   int32_t close() override;
   int32_t start() override;
   int32_t stop() override;
   int32_t prepare() override;
   int32_t setStreamAttributes( struct pal_stream_attributes *sattr __unused) {return 0;};
   int32_t setVolume( struct pal_volume_data *volume __unused) {return 0;};
   int32_t mute(bool state __unused) {return 0;};
   int32_t mute_l(bool state __unused) {return 0;};
   int32_t pause() {return 0;};
   int32_t pause_l() {return 0;};
   int32_t resume() {return 0;};
   int32_t resume_l() {return 0;};
   int32_t drain(pal_drain_type_t type) override;
   int32_t flush();
   int32_t suspend() override;
   int32_t getTagsWithModuleInfo(size_t *size , uint8_t *payload) override;
   int32_t setBufInfo(size_t *in_buf_size, size_t in_buf_count,
                       size_t *out_buf_size, size_t out_buf_count);

   int32_t addRemoveEffect(pal_audio_effect_t effect __unused, bool enable __unused) {return 0;};
   int32_t read(struct pal_buffer *buf) override;
   int32_t write(struct pal_buffer *buf) override;
   int32_t registerCallBack(pal_stream_callback cb, uint64_t cookie) override;
   int32_t getCallBack(pal_stream_callback *cb) override;
   int32_t getParameters(uint32_t param_id, void **payload) override;
   int32_t setParameters(uint32_t param_id, void *payload) override;
   static int32_t isSampleRateSupported(uint32_t sampleRate);
   static int32_t isChannelSupported(uint32_t numChannels);
   static int32_t isBitWidthSupported(uint32_t bitWidth);
   int32_t setECRef(std::shared_ptr<Device> dev __unused, bool is_enable __unused) {return 0;};
   int32_t setECRef_l(std::shared_ptr<Device> dev __unused, bool is_enable __unused) {return 0;};
   int32_t ssrDownHandler() override;
   int32_t ssrUpHandler() override;
private:
   /*This notifies that the system went through/is in a ssr*/
   bool ssrInNTMode;
};

#endif//STREAM_H_
