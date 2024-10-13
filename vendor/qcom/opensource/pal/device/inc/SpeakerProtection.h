/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef SPEAKER_PROT
#define SPEAKER_PROT

#include "Device.h"
#include "sp_vi.h"
#include "sp_rx.h"
#include <tinyalsa/asoundlib.h>
#include <mutex>
#include <condition_variable>
#include <thread>
#include<vector>
#include "apm_api.h"
#include "ResourceManager.h"

class Device;
/*M55  code for SR-QN6887A-01-561 by tangjie at 2023/9/13 start*/
// DEFINES FOR CALIB
#define FSM_RDC_BIN                    "/mnt/vendor/persist/fsalgo_rdc.bin"
#define FSM_RE_RANGE_PATH              "/data/calib_range.config"
#define FS_ALIGN_4BYTE(x)              (((x) + 3) & (~3))
#define FS_PADDING_ALIGN_8BYTE(x)      ((((x) + 7) & 7) ^ 7)
#define FSADSP_PARAM_ID_SET_ALGO_RE25  0x10001FA7
#define FSADSP_PARAM_ID_GET_CALIB_DATA 0x10001FAB

//TODO: customize part, Make modifications based on different projects
#define FSM_DEV_NUM             2
#define FS_SPK_RX_MID           (0x0000410C)

typedef struct fsm_data_info
{
    uint16_t rstrim;
    uint16_t channel;
    uint32_t re25;
} fsm_data_info_t;

typedef struct fsm_algo_calib_data
{
    uint16_t version;
    uint16_t ndev;
    fsm_data_info_t calib_data[2];
} fsm_algo_calib_data_t;

typedef struct fsm_r0_range
{
    float r0_min[FSM_DEV_NUM];
    float r0_max[FSM_DEV_NUM];
} fsm_r0_range_t;

typedef struct calib_data_info_sp
{
    int32_t re25;
    int32_t tempr;
    int32_t reserve1;
    int32_t f0;
    int32_t q;
    int32_t reserve2;
} calib_data_info_t;
/*M55  code for SR-QN6887A-01-561 by tangjie at 2023/9/13 end*/

/*M55 code for SR-QN6887A-01-563 by yingboyang at 20230919 start*/
/************* add by foursemi **************/
// DEFINES FOR ROTATION
#define FSADSP_PARAM_ID_SEND_CROSSFADE_INFO  0x10001FB8
typedef struct fsm_crossfade
{
    int mode;         //mode
    int targetdb;     //targetdb *1000->mdb
    int type1;        //fade type, 0->linear, 1->log
    int transitiont1; //fade time, transitiont1 *1000->ms
    int type2;        //fade type, 0->linear, 1->log
    int transitiont2; //fade time, transitiont2 *1000->ms
    int holdt;        //holdt *1000->ms
    int ch_sel[4];
} fsm_crossfade_t;

typedef enum {
    FSM_CROSSFADE_MODE0,// mode 0, fade out
    FSM_CROSSFADE_MODE1,// mode 1, fade int
    FSM_CROSSFADE_MODE2,// mode 2, fade hold
    FSM_CROSSFADE_MODE3,// mode 3, crossfade
} fsm_crossfade_mode;

/********** add by foursemi end ***************/
/*M55 code for SR-QN6887A-01-563 by yingboyang at 20230919 end*/

#define LPASS_WR_CMD_REG_PHY_ADDR 0x3250300
#define LPASS_RD_CMD_REG_PHY_ADDR 0x3250304
#define LPASS_RD_FIFO_REG_PHY_ADDR 0x3250318
#define CPS_WSA_VBATT_REG_ADDR 0x0003429
#define CPS_WSA_TEMP_REG_ADDR 0x0003422

#define CPS_WSA_VBATT_LOWER_THRESHOLD_1 168
#define CPS_WSA_VBATT_LOWER_THRESHOLD_2 148

typedef enum speaker_prot_cal_state {
    SPKR_NOT_CALIBRATED,     /* Speaker not calibrated  */
    SPKR_CALIBRATED,         /* Speaker calibrated  */
    SPKR_CALIB_IN_PROGRESS,  /* Speaker calibration in progress  */
}spkr_prot_cal_state;

typedef enum speaker_prot_proc_state {
    SPKR_PROCESSING_IN_IDLE,     /* Processing mode in idle state */
    SPKR_PROCESSING_IN_PROGRESS, /* Processing mode in running state */
}spkr_prot_proc_state;

enum {
    SPKR_RIGHT,    /* Right Speaker */
    SPKR_LEFT,     /* Left Speaker */
    SPKR_TOP,      /* Top Speaker */
    SPKR_BOTTOM,   /* Bottom Speaker */
};

struct agmMetaData {
    uint8_t *buf;
    uint32_t size;
    agmMetaData(uint8_t *b, uint32_t s)
        :buf(b),size(s) {}
};

struct spDeviceInfo {
    bool devThreadExit;
    speaker_prot_cal_state deviceCalState;
    int *deviceTempList;
    bool isDeviceInUse;
    bool isDeviceDynamicCalTriggered;
    bool devCalThrdCreated;
    struct timespec deviceLastTimeUsed;
    int numChannels;
    int devNumberOfRequest;
    struct pal_device_info dev_vi_device;
    std::thread mDeviceCalThread;
};

class SpeakerProtection : public Device
{
protected :
    bool spkrProtEnable;
    bool threadExit;
    bool triggerCal;
    int minIdleTime;
    static speaker_prot_cal_state spkrCalState;
    spkr_prot_proc_state spkrProcessingState;
    int *spkerTempList;
    /*M55  code for SR-QN6887A-01-561 by tangjie at 2023/9/6 start*/
    bool isWsaAmp;
   /*M55  code for SR-QN6887A-01-561 by tangjie at 2023/9/6 end*/
    static bool isSpkrInUse;
    static bool calThrdCreated;
    static bool isDynamicCalTriggered;
    static bool viTxSetupThrdCreated;
    static struct timespec spkrLastTimeUsed;
    static struct mixer *virtMixer;
    static struct mixer *hwMixer;
    static struct pcm *rxPcm;
    static struct pcm *txPcm;
    static int numberOfChannels;
    static bool mDspCallbackRcvd;
    static param_id_sp_th_vi_calib_res_cfg_t *callback_data;
    struct pal_device mDeviceAttr;
    std::vector<int> pcmDevIdTx;
    static int calibrationCallbackStatus;
    static int numberOfRequest;
    static struct pal_device_info vi_device;
    struct spDeviceInfo spDevInfo;
    void *viCustomPayload;
    size_t viCustomPayloadSize;

    /*M55  code for SR-QN6887A-01-561/QN6887A-1908 by tangjie at 2023/12/2 start*/
    static fsm_algo_calib_data_t fsmReValue;
    static fsm_r0_range_t fsm_range;
    //static fsm_r0_range_t fsmReRange;
    /*M55  code for SR-QN6887A-01-561/QN6887A-1908 by tangjie at 2023/12/2 end*/

private :
    static bool isSharedBE;
    int populateSpDevInfoCreateCalThread(struct pal_device *device);

public:
    static std::thread mCalThread;
    static std::thread viTxSetupThread;
    static std::condition_variable cv;
    static std::mutex cvMutex;
    std::mutex deviceMutex;
    static std::mutex calibrationMutex;
    static std::mutex calSharedBeMutex;
    void spkrCalibrationThread();
    void spkrCalibrationThreadV2();
    int getSpeakerTemperature(int spkr_pos);
    void spkrCalibrateWait();
    int spkrStartCalibration();
    int spkrStartCalibrationV2();
    int viTxSetupThreadLoop();
    void speakerProtectionInit();
    void speakerProtectionDeinit();
    void getSpeakerTemperatureList();
    int getDeviceTemperatureList();
    static void spkrProtSetSpkrStatus(bool enable);
    void spkrProtSetSpkrStatusV2(bool enable);
    static int setConfig(int type, int tag, int tagValue, int devId, const char *aif);
    bool isSpeakerInUse(unsigned long *sec);

    SpeakerProtection(struct pal_device *device,
                      std::shared_ptr<ResourceManager> Rm);
    ~SpeakerProtection();

    int32_t start();
    int32_t stop();

    int32_t setParameter(uint32_t param_id, void *param) override;
    int32_t getParameter(uint32_t param_id, void **param) override;

    int32_t spkrProtProcessingMode(bool flag);
    int32_t spkrProtProcessingModeV2(bool flag);
    int speakerProtectionDynamicCal();
    void updateSPcustomPayload();
    static int32_t spkrProtSetR0T0Value(vi_r0t0_cfg_t r0t0Array[]);
    static void handleSPCallback (uint64_t hdl, uint32_t event_id, void *event_data,
                                  uint32_t event_size);
    void updateCpsCustomPayload(int miid);
    int updateVICustomPayload(void *payload, size_t size);
    int getCpsDevNumber(std::string mixer);
    int32_t getCalibrationData(void **param);
    int32_t getFTMParameter(void **param);
    void disconnectFeandBe(std::vector<int> pcmDevIds, std::string backEndName);
    /*M55  code for SR-QN6887A-01-561 by tangjie at 2023/9/6 start*/
    void setFsmRdcValue(int device, fsm_algo_calib_data_t *cali_re, int data_len);
    void getFsmCalibRe25(int device, calib_data_info_t *cali_re, int data_len);
    /*M55  code for SR-QN6887A-01-561 by tangjie at 2023/9/6 end*/
    /*M55 code for SR-QN6887A-01-563 by yingboyang at 20230919 start*/
    void setFsmCrossfadeInfo(int device, int mode, int angle);
    /*M55 code for SR-QN6887A-01-563 by yingboyang at 20230919 end*/

    bool canDeviceProceedForCalibration(unsigned long *sec);
    bool isDeviceInUse(unsigned long *sec);
};

class SpeakerFeedback : public Device
{
    protected :
    struct pal_device mDeviceAttr;
    static std::shared_ptr<Device> obj;
    static int numSpeaker;
   /*M55  code for SR-QN6887A-01-561 by tangjie at 2023/9/6 start*/
    bool isWsaAmp;
   /*M55  code for SR-QN6887A-01-561 by tangjie at 2023/9/6 end*/
    public :
    int32_t start();
    int32_t stop();
    SpeakerFeedback(struct pal_device *device,
                    std::shared_ptr<ResourceManager> Rm);
    ~SpeakerFeedback();
    void updateVIcustomPayload();
    static std::shared_ptr<Device> getInstance(struct pal_device *device,
                                               std::shared_ptr<ResourceManager> Rm);
};

#endif
