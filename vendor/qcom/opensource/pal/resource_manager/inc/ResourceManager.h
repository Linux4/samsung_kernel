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

#ifndef RESOURCE_MANAGER_H
#define RESOURCE_MANAGER_H
#include <algorithm>
#include <vector>
#include <memory>
#include <iostream>
#include <thread>
#include <mutex>
#include <string>
#include "audio_route/audio_route.h"
#include <tinyalsa/asoundlib.h>
#include "PalCommon.h"
#include <array>
#include <map>
#include <expat.h>
#include <stdio.h>
#include <queue>
#include <deque>
#include <unordered_map>
#include "PalDefs.h"
#include "ChargerListener.h"
#include "SndCardMonitor.h"
#include "SoundTriggerPlatformInfo.h"
#include "ACDPlatformInfo.h"
#include "ContextManager.h"

#ifdef SEC_AUDIO_COMMON
#include "SecPalDefs.h"
#endif

typedef enum {
    RX_HOSTLESS = 1,
    TX_HOSTLESS,
} hostless_dir_t;

#define audio_mixer mixer

#define VENDOR_CONFIG_PATH_MAX_LENGTH 128
#define AUDIO_PARAMETER_KEY_NATIVE_AUDIO "audio.nat.codec.enabled"
#define AUDIO_PARAMETER_KEY_NATIVE_AUDIO_MODE "native_audio_mode"
#define AUDIO_PARAMETER_KEY_MAX_SESSIONS "max_sessions"
#define AUDIO_PARAMETER_KEY_MAX_NT_SESSIONS "max_nonTunnel_sessions"
#define AUDIO_PARAMETER_KEY_LOG_LEVEL "logging_level"
#define AUDIO_PARAMETER_KEY_CONTEXT_MANAGER_ENABLE "context_manager_enable"
#define AUDIO_PARAMETER_KEY_HIFI_FILTER "hifi_filter"
#define AUDIO_PARAMETER_KEY_LPI_LOGGING "lpi_logging_enable"
#define AUDIO_PARAMETER_KEY_UPD_DEDICATED_BE "upd_dedicated_be"
#define AUDIO_PARAMETER_KEY_DUAL_MONO "dual_mono"
#define MAX_PCM_NAME_SIZE 50
#define MAX_STREAM_INSTANCES (sizeof(uint64_t) << 3)
#define MIN_USECASE_PRIORITY 0xFFFFFFFF
#if LINUX_ENABLED
#if defined(__LP64__)
#define ADM_LIBRARY_PATH "/usr/lib64/libadm.so"
#else
#define ADM_LIBRARY_PATH "/usr/lib/libadm.so"
#endif
#else
#ifdef __LP64__
#define ADM_LIBRARY_PATH "/vendor/lib64/libadm.so"
#else
#define ADM_LIBRARY_PATH "/vendor/lib/libadm.so"
#endif
#endif

using InstanceListNode_t = std::vector<std::pair<int32_t, bool>> ;
using nonTunnelInstMap_t = std::unordered_map<uint32_t, bool>;

typedef enum {
    TAG_ROOT,
    TAG_CARD,
    TAG_DEVICE,
    TAG_PLUGIN,
    TAG_DEV_PROPS,
    TAG_NONE,
    TAG_MIXER,
} snd_card_defs_xml_tags_t;

typedef enum {
    TAG_RESOURCE_ROOT,
    TAG_RESOURCE_MANAGER_INFO,
    TAG_DEVICE_PROFILE,
    TAG_IN_DEVICE,
    TAG_OUT_DEVICE,
    TAG_USECASE,
    TAG_CONFIG_VOICE,
    TAG_CONFIG_MODE_MAP,
    TAG_CONFIG_MODE_PAIR,
    TAG_GAIN_LEVEL_MAP,
    TAG_GAIN_LEVEL_PAIR,
    TAG_INSTREAMS,
    TAG_INSTREAM,
    TAG_POLICIES,
    TAG_ECREF,
    TAG_CUSTOMCONFIG,
    TAG_LPI_VOTE_STREAM,
    TAG_SLEEP_MONITOR_LPI_STREAM,
    TAG_CONFIG_VOLUME,
    TAG_CONFIG_VOLUME_SET_PARAM_SUPPORTED_STREAM,
    TAG_CONFIG_VOLUME_SET_PARAM_SUPPORTED_STREAMS,
} resource_xml_tags_t;

typedef enum {
    PCM,
    COMPRESS,
    VOICE1,
    VOICE2,
    ExtEC,
} stream_supported_type;

typedef enum {
    ST_PAUSE = 1,
    ST_RESUME,
    ST_ENABLE_LPI,
    ST_HANDLE_CONCURRENT_STREAM,
    ST_HANDLE_CONNECT_DEVICE,
    ST_HANDLE_DISCONNECT_DEVICE,
    ST_HANDLE_CHARGING_STATE,
} st_action;

typedef enum
{
    GRP_DEV_CONFIG_INVALID = -1,
    GRP_UPD_RX,
    GRP_HANDSET,
    GRP_SPEAKER,
    GRP_SPEAKER_VOICE,
    GRP_UPD_RX_HANDSET,
    GRP_UPD_RX_SPEAKER,
    GRP_DEV_CONFIG_IDX_MAX,
} group_dev_config_idx_t;

struct xml_userdata {
    char data_buf[1024];
    size_t offs;

    unsigned int card;
    bool card_found;
    bool card_parsed;
    bool resourcexml_parsed;
    bool voice_info_parsed;
    bool gain_lvl_parsed;
    snd_card_defs_xml_tags_t current_tag;
    bool is_parsing_sound_trigger;
    bool is_parsing_acd;
    bool is_parsing_group_device;
    group_dev_config_idx_t group_dev_idx;
    resource_xml_tags_t tag;
    bool inCustomConfig;
};

typedef enum {
    DEFAULT = 0,
    HOSTLESS,
    NON_TUNNEL,
    NO_CONFIG,
} sess_mode_t;

struct deviceCap {
    int deviceId;
    char name[MAX_PCM_NAME_SIZE];
    stream_supported_type type;
    int playback;
    int record;
    sess_mode_t sess_mode;
};

typedef enum {
    SIDETONE_OFF,
    SIDETONE_HW,
    SIDETONE_SW,
} sidetone_mode_t;

typedef enum {
    AUDIO_BIT_WIDTH_8 = 8,
    AUDIO_BIT_WIDTH_DEFAULT_16 = 16,
    AUDIO_BIT_WIDTH_24 = 24,
    AUDIO_BIT_WIDTH_32 = 32,
} audio_bit_width_t;

struct usecase_custom_config_info
{
    std::string key;
    std::string sndDevName;
    int channel;
    sidetone_mode_t sidetoneMode;
    int samplerate;
    uint32_t priority;
    uint32_t bit_width;
};

struct usecase_info {
    int type;
    int samplerate;
    sidetone_mode_t sidetoneMode;
    std::string sndDevName;
    int channel;
    std::vector<usecase_custom_config_info> config;
    uint32_t priority;
    uint32_t bit_width;
};

struct pal_device_info {
    int channels;
    int max_channels;
    int samplerate;
    std::string sndDevName;
    bool isExternalECRefEnabledFlag;
    uint32_t priority;
    bool fractionalSRSupported;
    bool channels_overwrite;
    bool samplerate_overwrite;
    bool sndDevName_overwrite;
    bool bit_width_overwrite;
    uint32_t bit_width;
};

struct vsid_modepair {
    unsigned int key;
    unsigned int value;
};

struct vsid_info {
     int vsid;
     std::vector<vsid_modepair> modepair;
     int loopback_delay;
#ifdef SEC_AUDIO_LOOPBACK_TEST
     int loopback_mode;
     int loopback_mode_delay[PAL_LOOPBACK_MODE_MAX];
#endif
};

struct volume_set_param_info {
    int isVolumeUsingSetParam;
    std::vector<uint32_t> streams_;
};

struct tx_ecinfo {
    int tx_stream_type;
    std::vector<int> disabled_rx_streams;
};

enum {
    NATIVE_AUDIO_MODE_SRC = 1,
    NATIVE_AUDIO_MODE_TRUE_44_1,
    NATIVE_AUDIO_MODE_MULTIPLE_MIX_IN_CODEC,
    NATIVE_AUDIO_MODE_MULTIPLE_MIX_IN_DSP,
    NATIVE_AUDIO_MODE_INVALID
};

struct nativeAudioProp {
   bool rm_na_prop_enabled;
   bool ui_na_prop_enabled;
   int na_mode;
};

typedef struct devpp_mfc_config
{
    uint32_t sample_rate;
    uint32_t channels;
    uint32_t bit_width;
} devpp_mfc_config_t;

typedef struct group_dev_hwep_config_ctl
{
    uint32_t sample_rate;
    uint32_t channels;
    uint32_t aud_fmt_id;
    uint32_t slot_mask;
} group_dev_hwep_config_t;

typedef struct group_dev_config
{
    std::string snd_dev_name;
    devpp_mfc_config_t  devpp_mfc_cfg;
    group_dev_hwep_config_t grp_dev_hwep_cfg;
} group_dev_config_t;

static const constexpr uint32_t DEFAULT_NT_SESSION_TYPE_COUNT = 2;

enum NTStreamTypes_t : uint32_t {
    NT_PATH_ENCODE = 0,
    NT_PATH_DECODE
};

typedef void (*session_callback)(uint64_t hdl, uint32_t event_id, void *event_data,
                uint32_t event_size);
bool isPalPCMFormat(uint32_t fmt_id);

typedef void* (*adm_init_t)();
typedef void (*adm_deinit_t)(void *);
typedef void (*adm_register_output_stream_t)(void *, void*);
typedef void (*adm_register_input_stream_t)(void *, void*);
typedef void (*adm_deregister_stream_t)(void *, void*);
typedef void (*adm_request_focus_t)(void *, void*);
typedef void (*adm_abandon_focus_t)(void *, void*);
typedef void (*adm_set_config_t)(void *, void*,
        struct pcm *, struct pcm_config *);
typedef void (*adm_request_focus_v2_t)(void *, void*, long);
typedef void (*adm_on_routing_change_t)(void *, void*);
typedef int (*adm_request_focus_v2_1_t)(void *, void*, long);

class Device;
class Stream;
class StreamPCM;
class StreamCompress;
class StreamSoundTrigger;
class StreamACD;
class StreamInCall;
class StreamNonTunnel;
class SoundTriggerEngine;
class SndCardMonitor;
class StreamUltraSound;
class ContextManager;
class StreamSensorPCMData;
class StreamContextProxy;

struct deviceIn {
    int deviceId;
    int max_channel;
    int channel;
    int samplerate;
    std::vector<usecase_info> usecase;
    // dev ids supporting ec ref
    std::vector<pal_device_id_t> rx_dev_ids;
    /*
     * map dynamically maintain ec ref count, key for this map
     * is rx device id, which is present in rx_dev_ids, and value
     * for this map is a vector of all active tx streams using
     * this rx device as ec ref. For each Tx stream, we have a
     * EC ref count, indicating number of Rx streams which uses
     * this rx device as output device and also not disabled stream
     * type to the Tx stream. E.g., for SVA and Recording stream,
     * LL playback with speaker may only count for Recording stream
     * when ll barge-in is not enabled.
     */
    std::map<int, std::vector<std::pair<Stream *, int>>> ec_ref_count_map;
    std::string sndDevName;
    bool isExternalECRefEnabled;
    bool fractionalSRSupported;
    uint32_t bit_width;
};

class ResourceManager
{

private:
    //both of the below are update on register and deregister stream
    int mPriorityHighestPriorityActiveStream; //priority of the highest priority active stream
    Stream* mHighestPriorityActiveStream; //pointer to the highest priority active stream
    int getNumFEs(const pal_stream_type_t sType) const;
    bool ifVoiceorVoipCall (pal_stream_type_t streamType) const;
    int getCallPriority(bool ifVoiceCall) const;
    int getStreamAttrPriority (const pal_stream_attributes* sAttr) const;
    template <class T>

    void getHigherPriorityActiveStreams(const int inComingStreamPriority,
                                        std::vector<Stream*> &activestreams,
                                        std::vector<T> sourcestreams);
    const std::vector<int> allocateVoiceFrontEndIds(std::vector<int> listAllPcmVoiceFrontEnds,
                                  const int howMany);
    int getDeviceDefaultCapability(pal_param_device_capability_t capability);

    int handleScreenStatusChange(pal_param_screen_state_t screen_state);
    int handleDeviceRotationChange(pal_param_device_rotation_t rotation_type);
    int handleDeviceConnectionChange(pal_param_device_connection_t connection_state);
    int32_t streamDevDisconnect(std::vector <std::tuple<Stream *, uint32_t>> streamDevDisconnectList);
    int32_t streamDevConnect(std::vector <std::tuple<Stream *, struct pal_device *>> streamDevConnectList);
    int32_t streamDevDisconnect_l(std::vector <std::tuple<Stream *, uint32_t>> streamDevDisconnectList);
    int32_t streamDevConnect_l(std::vector <std::tuple<Stream *, struct pal_device *>> streamDevConnectList);
    void ssrHandlingLoop(std::shared_ptr<ResourceManager> rm);
    int updateECDeviceMap(std::shared_ptr<Device> rx_dev,
                        std::shared_ptr<Device> tx_dev,
                        Stream *tx_str, int count, bool is_txstop);
    static bool isBitWidthSupported(uint32_t bitWidth);
    uint32_t getNTPathForStreamAttr(const pal_stream_attributes attr);
    ssize_t getAvailableNTStreamInstance(const pal_stream_attributes attr);
protected:
    std::list <Stream*> mActiveStreams;
    std::list <StreamPCM*> active_streams_ll;
    std::list <StreamPCM*> active_streams_ulla;
    std::list <StreamPCM*> active_streams_ull;
    std::list <StreamPCM*> active_streams_db;
    std::list <StreamPCM*> active_streams_po;
    std::list <StreamPCM*> active_streams_proxy;
    std::list <StreamPCM*> active_streams_haptics;
    std::list <StreamPCM*> active_streams_raw;
    std::list <StreamInCall*> active_streams_incall_record;
    std::list <StreamNonTunnel*> active_streams_non_tunnel;
    std::list <StreamInCall*> active_streams_incall_music;
    std::list <StreamCompress*> active_streams_comp;
    std::list <StreamSoundTrigger*> active_streams_st;
    std::list <StreamACD*> active_streams_acd;
    std::list <StreamUltraSound*> active_streams_ultrasound;
    std::list <StreamSensorPCMData*> active_streams_sensor_pcm_data;
    std::list <StreamContextProxy*> active_streams_context_proxy;
    std::vector <std::pair<std::shared_ptr<Device>, Stream*>> active_devices;
    std::vector <std::shared_ptr<Device>> plugin_devices_;
    std::vector <pal_device_id_t> avail_devices_;
    bool bOverwriteFlag;
    bool screen_state_;
    bool charging_state_;
    bool is_charger_online_;
    bool is_concurrent_boost_state_;
    pal_speaker_rotation_type rotation_type_;
    bool isDeviceSwitch = false;
    static std::mutex mResourceManagerMutex;
    static std::mutex mGraphMutex;
    static std::mutex mActiveStreamMutex;
    static std::mutex mSleepMonitorMutex;
    static int snd_virt_card;
    static int snd_hw_card;

    static std::shared_ptr<ResourceManager> rm;
    static struct audio_route* audio_route;
    static struct audio_mixer* audio_virt_mixer;
    static struct audio_mixer* audio_hw_mixer;
    static std::vector <int> streamTag;
    static std::vector <int> streamPpTag;
    static std::vector <int> mixerTag;
    static std::vector <int> devicePpTag;
    static std::vector <int> deviceTag;
    static std::vector<std::pair<int32_t, int32_t>> devicePcmId;
    static std::vector<std::pair<int32_t, std::string>> deviceLinkName;
    static std::vector<int> listAllFrontEndIds;
    static std::vector<int> listAllPcmPlaybackFrontEnds;
    static std::vector<int> listAllPcmRecordFrontEnds;
    static std::vector<int> listAllPcmHostlessRxFrontEnds;
    static std::vector<int> listAllNonTunnelSessionIds;
    static std::vector<int> listAllPcmHostlessTxFrontEnds;
    static std::vector<int> listAllCompressPlaybackFrontEnds;
    static std::vector<int> listAllCompressRecordFrontEnds;
    static std::vector<int> listFreeFrontEndIds;
    static std::vector<int> listAllPcmVoice1RxFrontEnds;
    static std::vector<int> listAllPcmVoice1TxFrontEnds;
    static std::vector<int> listAllPcmVoice2RxFrontEnds;
    static std::vector<int> listAllPcmVoice2TxFrontEnds;
    static std::vector<int> listAllPcmExtEcTxFrontEnds;
    static std::vector<int> listAllPcmInCallRecordFrontEnds;
    static std::vector<int> listAllPcmInCallMusicFrontEnds;
    static std::vector<int> listAllPcmContextProxyFrontEnds;
    static std::vector<std::pair<int32_t, std::string>> listAllBackEndIds;
    static std::vector<std::pair<int32_t, std::string>> sndDeviceNameLUT;
    static std::vector<deviceCap> devInfo;
    static std::map<std::pair<uint32_t, std::string>, std::string> btCodecMap;
    static std::map<std::string, uint32_t> btFmtTable;
    static std::vector<deviceIn> deviceInfo;
    static std::vector<tx_ecinfo> txEcInfo;
    static struct vsid_info vsidInfo;
    static struct volume_set_param_info volumeSetParamInfo_;
    static std::vector<struct pal_amp_db_and_gain_table> gainLvlMap;
    static SndCardMonitor *sndmon;
    static std::vector <uint32_t> lpi_vote_streams_;
    /* condition variable for which ssrHandlerLoop will wait */
    static std::condition_variable cv;
    static std::mutex cvMutex;
    static std::queue<card_status_t> msgQ;
    static std::thread workerThread;
    std::vector<std::pair<std::string, InstanceListNode_t>> STInstancesLists;
    uint64_t stream_instances[PAL_STREAM_MAX];
    uint64_t in_stream_instances[PAL_STREAM_MAX];
    static int mixerEventRegisterCount;
    static int concurrencyEnableCount;
    static int concurrencyDisableCount;
    static int ACDConcurrencyEnableCount;
    static int ACDConcurrencyDisableCount;
    static int SNSPCMDataConcurrencyEnableCount;
    static int SNSPCMDataConcurrencyDisableCount;
    static int wake_lock_fd;
    static int wake_unlock_fd;
    static uint32_t wake_lock_cnt;
    static bool lpi_logging_;
    std::map<int, std::pair<session_callback, uint64_t>> mixerEventCallbackMap;
    static std::thread mixerEventTread;
    std::shared_ptr<CaptureProfile> SoundTriggerCaptureProfile;
    ResourceManager();
    ContextManager *ctxMgr;
    int32_t lpi_counter_;
    int32_t nlpi_counter_;
    int sleepmon_fd_;
    static std::map<group_dev_config_idx_t, std::shared_ptr<group_dev_config_t>> groupDevConfigMap;
    std::array<std::shared_ptr<nonTunnelInstMap_t>, DEFAULT_NT_SESSION_TYPE_COUNT> mNTStreamInstancesList;
#ifdef SEC_AUDIO_CALL_VOIP
    float voice_volume;
#endif
public:
    ~ResourceManager();
    static bool mixerClosed;
    enum card_status_t cardState;
    bool ssrStarted = false;
    /* Variable to store whether Speaker protection is enabled or not */
    static bool isSpeakerProtectionEnabled;
    static bool isChargeConcurrencyEnabled;
    static bool isCpsEnabled;
    static pal_audio_fmt_t bitFormatSupported;
    static bool isVbatEnabled;
    static bool isRasEnabled;
    static bool isGaplessEnabled;
    static bool isContextManagerEnabled;
    static bool isDualMonoEnabled;
    /* Variable to store which speaker side is being used for call audio.
     * Valid for Stereo case only
     */
    static bool isMainSpeakerRight;
    /* Variable to store Quick calibration time for Speaker protection */
    static int spQuickCalTime;
    /* Variable to store the mode request for Speaker protection */
    pal_spkr_prot_payload mSpkrProtModeValue;
    pal_global_callback globalCb = NULL;
    uint32_t num_proxy_channels = 0;
    /* Flag to store the state of VI record */
    static bool isVIRecordStarted;
    /* Flag to indicate if shared backend is enabled for UPD */
    static bool isUpdDedicatedBeEnabled;
#ifdef SEC_AUDIO_SUPPORT_UHQ
    static pal_uhqa_state stateUHQA;
#endif

    uint64_t cookie;
    int initSndMonitor();
    int initContextManager();
    void deInitContextManager();
    adm_init_t admInitFn = NULL;
    adm_deinit_t admDeInitFn = NULL;
    adm_register_output_stream_t admRegisterOutputStreamFn = NULL;
    adm_register_input_stream_t admRegisterInputStreamFn = NULL;
    adm_deregister_stream_t admDeregisterStreamFn = NULL;
    adm_request_focus_t admRequestFocusFn = NULL;
    adm_abandon_focus_t admAbandonFocusFn = NULL;
    adm_set_config_t admSetConfigFn = NULL;
    adm_request_focus_v2_t admRequestFocusV2Fn = NULL;
    adm_on_routing_change_t admOnRoutingChangeFn = NULL;
    adm_request_focus_v2_1_t  admRequestFocus_v2_1Fn = NULL;
    void *admData = NULL;
    void *admLibHdl = NULL;
    static void *cl_lib_handle;
    static cl_init_t cl_init;
    static cl_deinit_t cl_deinit;
    static cl_set_boost_state_t cl_set_boost_state;
    static std::shared_ptr<group_dev_config_t> activeGroupDevConfig;

    /* checks config for both stream and device */
    bool isStreamSupported(struct pal_stream_attributes *attributes,
                           struct pal_device *devices, int no_of_devices);
    int32_t getDeviceConfig(struct pal_device *deviceattr,
                            struct pal_stream_attributes *attributes);
    /*getDeviceInfo - updates channels, fluence info of the device*/
    void getDeviceInfo(pal_device_id_t deviceId, pal_stream_type_t type,
                       std::string key, struct pal_device_info *devinfo);
    bool getEcRefStatus(pal_stream_type_t tx_streamtype,pal_stream_type_t rx_streamtype);
    int32_t getVsidInfo(struct vsid_info  *info);
    int32_t getVolumeSetParamInfo(struct volume_set_param_info *volinfo);
    void getChannelMap(uint8_t *channel_map, int channels);
    int registerStream(Stream *s);
    int deregisterStream(Stream *s);
    int registerDevice(std::shared_ptr<Device> d, Stream *s);
    int deregisterDevice(std::shared_ptr<Device> d, Stream *s);
    int registerDevice_l(std::shared_ptr<Device> d, Stream *s);
    int deregisterDevice_l(std::shared_ptr<Device> d, Stream *s);
    int registerMixerEventCallback(const std::vector<int> &DevIds,
                                   session_callback callback,
                                   uint64_t cookie, bool is_register);
    bool isDeviceActive(std::shared_ptr<Device> d, Stream *s);
    bool isDeviceActive_l(std::shared_ptr<Device> d, Stream *s);
    int addPlugInDevice(std::shared_ptr<Device> d,
                        pal_param_device_connection_t connection_state);
    int removePlugInDevice(pal_device_id_t device_id,
                           pal_param_device_connection_t connection_state);
    /* bIsUpdated - to specify if the config is updated by rm */
    int checkAndGetDeviceConfig(struct pal_device *device ,bool* bIsUpdated);
    static void getFileNameExtn(const char* in_snd_card_name, char* file_name_extn);
    int init_audio();
    void loadAdmLib();
    static int init();
    static void deinit();
    static std::shared_ptr<ResourceManager> getInstance();
    static int XmlParser(std::string xmlFile);
    static void updatePcmId(int32_t deviceId, int32_t pcmId);
    static void updateLinkName(int32_t deviceId, std::string linkName);
    static void updateSndName(int32_t deviceId, std::string sndName);
    static void updateBackEndName(int32_t deviceId, std::string backEndName);
    static void updateBtCodecMap(std::pair<uint32_t, std::string> key, std::string value);
    static std::string getBtCodecLib(uint32_t codecFormat, std::string codecType);
    int getGainLevelMapping(struct pal_amp_db_and_gain_table *mapTbl, int tblSize);

    int setParameter(uint32_t param_id, void *param_payload,
                     size_t payload_size);
    int setParameter(uint32_t param_id, void *param_payload,
                     size_t payload_size, pal_device_id_t pal_device_id,
                     pal_stream_type_t pal_stream_type);
    int setDeviceParamConfig(uint32_t param_id, std::shared_ptr<Device> dev,
                             int tag);
    int rwParameterACDB(uint32_t param_id, void *param_payload,
                     size_t payload_size, pal_device_id_t pal_device_id,
                     pal_stream_type_t pal_stream_type, uint32_t sample_rate,
                     uint32_t instance_id, bool is_param_write, bool is_play);
    int getParameter(uint32_t param_id, void **param_payload,
                     size_t *payload_size, void *query = nullptr);
    int getParameter(uint32_t param_id, void *param_payload,
                     size_t payload_size, pal_device_id_t pal_device_id,
                     pal_stream_type_t pal_stream_type);
    int getVirtualSndCard();
    int getHwSndCard();
    int getPcmDeviceId(int deviceId);
    int getAudioRoute(struct audio_route** ar);
    int getVirtualAudioMixer(struct audio_mixer **am);
    int getHwAudioMixer(struct audio_mixer **am);
    int getActiveStream(std::shared_ptr<Device> d, std::vector<Stream*> &activestreams);
    int getActiveStream_l(std::shared_ptr<Device> d, std::vector<Stream*> &activestreams);
    int getOrphanStream(std::vector<Stream*> &orphanstreams, std::vector<Stream*> &retrystreams);
    int getOrphanStream_l(std::vector<Stream*> &orphanstreams, std::vector<Stream*> &retrystreams);
    int getActiveDevices(std::vector<std::shared_ptr<Device>> &deviceList);
    int getSndDeviceName(int deviceId, char *device_name);
    int getDeviceEpName(int deviceId, std::string &epName);
    int getBackendName(int deviceId, std::string &backendName);
    int getStreamTag(std::vector <int> &tag);
    int getDeviceTag(std::vector <int> &tag);
    int getMixerTag(std::vector <int> &tag);
    int getStreamPpTag(std::vector <int> &tag);
    int getDevicePpTag(std::vector <int> &tag);
    int getDeviceDirection(uint32_t beDevId);
    const std::vector<int> allocateFrontEndIds (const struct pal_stream_attributes,
                                                int lDirection);
    const std::vector<int> allocateFrontEndExtEcIds ();
    void freeFrontEndEcTxIds (const std::vector<int> f);
    void freeFrontEndIds (const std::vector<int> f,
                          const struct pal_stream_attributes,
                          int lDirection);
    const std::vector<std::string> getBackEndNames(const std::vector<std::shared_ptr<Device>> &deviceList) const;
    void getSharedBEDevices(std::vector<std::shared_ptr<Device>> &deviceList, std::shared_ptr<Device> inDevice) const;
    void getBackEndNames( const std::vector<std::shared_ptr<Device>> &deviceList,
                          std::vector<std::pair<int32_t, std::string>> &rxBackEndNames,
                          std::vector<std::pair<int32_t, std::string>> &txBackEndNames) const;
    bool updateDeviceConfig(std::shared_ptr<Device> *inDev,
             struct pal_device *inDevAttr, const pal_stream_attributes* inStrAttr);
    int32_t forceDeviceSwitch(std::shared_ptr<Device> inDev, struct pal_device *newDevAttr);
    const std::string getPALDeviceName(const pal_device_id_t id) const;
    bool isNonALSACodec(const struct pal_device *device) const;
    bool isNLPISwitchSupported(pal_stream_type_t type);
    bool IsLPISupported(pal_stream_type_t type);
    bool IsLowLatencyBargeinSupported(pal_stream_type_t type);
    bool IsAudioCaptureConcurrencySupported(pal_stream_type_t type);
    bool IsVoiceCallConcurrencySupported(pal_stream_type_t type);
    bool IsVoipConcurrencySupported(pal_stream_type_t type);
    bool IsTransitToNonLPIOnChargingSupported();
    bool IsDedicatedBEForUPDEnabled();
    void GetSoundTriggerConcurrencyCount(pal_stream_type_t type, int32_t *enable_count, int32_t *disable_count);
    bool GetChargingState() const { return charging_state_; }
    bool getChargerOnlineState(void) const { return is_charger_online_; }
    bool getConcurrentBoostState(void) const { return is_concurrent_boost_state_; }
    bool CheckForForcedTransitToNonLPI();
    void GetVoiceUIProperties(struct pal_st_properties *qstp);
    int HandleDetectionStreamAction(pal_stream_type_t type, int32_t action, void *data);
    void HandleStreamPauseResume(pal_stream_type_t st_type, bool active);
    std::shared_ptr<CaptureProfile> GetACDCaptureProfileByPriority(
        StreamACD *s, std::shared_ptr<CaptureProfile> cap_prof_priority);
    std::shared_ptr<CaptureProfile> GetSVACaptureProfileByPriority(
        StreamSoundTrigger *s, std::shared_ptr<CaptureProfile> cap_prof_priority);
    std::shared_ptr<CaptureProfile> GetSPDCaptureProfileByPriority(
        StreamSensorPCMData *s, std::shared_ptr<CaptureProfile> cap_prof_priority);
    std::shared_ptr<CaptureProfile> GetCaptureProfileByPriority(Stream *s);
    bool UpdateSoundTriggerCaptureProfile(Stream *s, bool is_active);
    std::shared_ptr<CaptureProfile> GetSoundTriggerCaptureProfile();
    int SwitchSoundTriggerDevices(bool connect_state, pal_device_id_t device_id);
    static void mixerEventWaitThreadLoop(std::shared_ptr<ResourceManager> rm);
    bool isCallbackRegistered() { return (mixerEventRegisterCount > 0); }
    int handleMixerEvent(struct mixer *mixer, char *mixer_str);
    int StopOtherDetectionStreams(void *st);
    int StartOtherDetectionStreams(void *st);
    void GetConcurrencyInfo(pal_stream_type_t st_type,
                         pal_stream_type_t in_type, pal_stream_direction_t dir,
                         bool *rx_conc, bool *tx_conc, bool *conc_en);
    void ConcurrentStreamStatus(pal_stream_type_t type,
                                pal_stream_direction_t dir,
                                bool active);
    void HandleConcurrenyForSoundTriggerStreams(pal_stream_type_t type,
                                pal_stream_direction_t dir,
                                bool active);
    std::shared_ptr<Device> getActiveEchoReferenceRxDevices(Stream *tx_str);
    std::shared_ptr<Device> getActiveEchoReferenceRxDevices_l(Stream *tx_str);
    std::vector<Stream*> getConcurrentTxStream(
        Stream *rx_str, std::shared_ptr<Device> rx_device);
    std::vector<Stream*> getConcurrentTxStream_l(
        Stream *rx_str, std::shared_ptr<Device> rx_device);
    bool checkECRef(std::shared_ptr<Device> rx_dev,
                    std::shared_ptr<Device> tx_dev);

    static void endTag(void *userdata __unused, const XML_Char *tag_name);
    static void snd_reset_data_buf(struct xml_userdata *data);
    static void snd_process_data_buf(struct xml_userdata *data, const XML_Char *tag_name);
    static void process_device_info(struct xml_userdata *data, const XML_Char *tag_name);
    static void process_input_streams(struct xml_userdata *data, const XML_Char *tag_name);
    static void process_config_voice(struct xml_userdata *data, const XML_Char *tag_name);
    static void process_config_volume(struct xml_userdata *data, const XML_Char *tag_name);
    static void process_lpi_vote_streams(struct xml_userdata *data, const XML_Char *tag_name);
    static void process_kvinfo(const XML_Char **attr, bool overwrite);
    static void process_voicemode_info(const XML_Char **attr);
    static void process_gain_db_to_level_map(struct xml_userdata *data, const XML_Char **attr);
    static void processCardInfo(struct xml_userdata *data, const XML_Char *tag_name);
    static void processBTCodecInfo(const XML_Char **attr);
    static void startTag(void *userdata __unused, const XML_Char *tag_name, const XML_Char **attr);
    static void snd_data_handler(void *userdata, const XML_Char *s, int len);
    static void processDeviceIdProp(struct xml_userdata *data, const XML_Char *tag_name);
    static void processDeviceCapability(struct xml_userdata *data, const XML_Char *tag_name);
    static void process_group_device_config(struct xml_userdata *data, const char* tag, const char** attr);
    static int getNativeAudioSupport();
    static int setNativeAudioSupport(int na_mode);
    static void getNativeAudioParams(struct str_parms *query,struct str_parms *reply,char *value, int len);
    static int setConfigParams(struct str_parms *parms);
    static int setNativeAudioParams(struct str_parms *parms,char *value, int len);
    static int setLoggingLevelParams(struct str_parms *parms,char *value, int len);
    static int setContextManagerEnableParam(struct str_parms *parms,char *value, int len);
    static int setLpiLoggingParams(struct str_parms *parms, char *value, int len);
    static int setUpdDedicatedBeEnableParam(struct str_parms *parms,char *value, int len);
    static int setDualMonoEnableParam(struct str_parms *parms,char *value, int len);
    static bool isLpiLoggingEnabled();
    static void processConfigParams(const XML_Char **attr);
    static bool isValidDevId(int deviceId);
    static bool isOutputDevId(int deviceId);
    static bool isInputDevId(int deviceId);
    static bool matchDevDir(int devId1, int devId2);
    static int convertCharToHex(std::string num);
    static pal_stream_type_t getStreamType(std::string stream_name);
    static pal_device_id_t getDeviceId(std::string device_name);
    bool getScreenState();
    bool isDeviceAvailable(pal_device_id_t id);
    bool isDeviceAvailable(std::vector<std::shared_ptr<Device>> devices, pal_device_id_t id);
    bool isDeviceAvailable(struct pal_device *devices, uint32_t devCount, pal_device_id_t id);
    bool isDeviceReady(pal_device_id_t id);
    static bool isBtScoDevice(pal_device_id_t id);
    int32_t a2dpSuspend();
    int32_t a2dpResume();
    int32_t a2dpCaptureSuspend();
    int32_t a2dpCaptureResume();
    bool isPluginDevice(pal_device_id_t id);
    bool updateDevAttr(struct pal_device *activeDevAttr, const struct pal_device_info *inDevInfo,
         struct pal_device *inDevAttr);
    bool isDpDevice(pal_device_id_t id);

    /* Separate device reference counts are maintained in PAL device and GSL device SGs.
     * lock graph is to sychronize these reference counts during device and session operations
     */
    void lockGraph() { mGraphMutex.lock(); };
    void unlockGraph() { mGraphMutex.unlock(); };
    void getSharedBEActiveStreamDevs(std::vector <std::tuple<Stream *, uint32_t>> &activeStreamDevs,
                                     int dev_id);
    int32_t streamDevSwitch(std::vector <std::tuple<Stream *, uint32_t>> streamDevDisconnectList,
                            std::vector <std::tuple<Stream *, struct pal_device *>> streamDevConnectList);
    char* getDeviceNameFromID(uint32_t id);
    int getPalValueFromGKV(pal_key_vector_t *gkv, int key);
    pal_speaker_rotation_type getCurrentRotationType();
    void ssrHandler(card_status_t state);
    int32_t getSidetoneMode(pal_device_id_t deviceId, pal_stream_type_t type,
                            sidetone_mode_t *mode);
    int getStreamInstanceID(Stream *str);
    int resetStreamInstanceID(Stream *str);
    int resetStreamInstanceID(Stream *str, uint32_t sInstanceID);
    static void setGaplessMode(const XML_Char **attr);
    static int initWakeLocks(void);
    static void deInitWakeLocks(void);
    void acquireWakeLock();
    void releaseWakeLock();
    static void process_custom_config(const XML_Char **attr);
    static void process_usecase();
    void getVendorConfigPath(char* config_file_path, int path_size);
    pal_device_id_t CheckAndUpdateDeviceForUPD(pal_device_id_t *device_id);
    void restoreDevice(std::shared_ptr<Device> dev);
    bool doDevAttrDiffer(struct pal_device *inDevAttr,
                         const char *CurrentSndDeviceName,
                         struct pal_device *curDevAttr);
    int updatePriorityAttr(pal_device_id_t dev_id,
                           std::vector <std::tuple<Stream *, uint32_t>> activestreams,
                           struct pal_device *incomingDev,
                           const pal_stream_attributes* currentStrAttr);
    bool compareAndUpdateDevAttr(const struct pal_device *Dev1Attr,
                                 const struct pal_device_info *Dev1Info,
                                 struct pal_device *Dev2Attr,
                                 const struct pal_device_info *Dev2Info);
    int32_t voteSleepMonitor(Stream *str, bool vote);
    static uint32_t palFormatToBitwidthLookup(const pal_audio_fmt_t format);
    void chargerListenerFeatureInit();
    static void chargerListenerInit(charger_status_change_fn_t);
    static void chargerListenerDeinit();
    static void onChargerListenerStatusChanged(int event_type, int status,
                                                 bool concurrent_state);
    int chargerListenerSetBoostState(bool state);
    static bool isGroupConfigAvailable(group_dev_config_idx_t idx);
    int checkAndUpdateGroupDevConfig(struct pal_device *deviceattr,
                                 const struct pal_stream_attributes *sAttr,
                                 std::vector<Stream*> &streamsToSwitch,
                                 struct pal_device *streamDevAttr,
                                 bool streamEnable);
#ifdef SEC_AUDIO_CALL_VOIP
    void setVoiceVolume(float volume) { voice_volume = volume; };
    float getVoiceVolume() { return voice_volume; };
#endif
};

#endif
