#ifndef FSALGO_CALIB_H
#define FSALGO_CALIB_H

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include <tinyalsa/asoundlib.h>
//#include "agmmixer.h"
#include <agm/agm_api.h>
#include "PalApi.h"
//#include "AudioCommon.h"

#ifdef FEATURE_IPQ_OPENWRT
#include <audio_utils/log.h>
#else
#include <log/log.h>
#endif

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#define FSM_CODE_OK      (0)
#define FSM_CODE_FAIL    (-1)

#define FSM_CALIB_DATA_PATH     "/mnt/vendor/persist/audio/fsalgo_rdc.bin"
//#define FS_CALI_DATA_PATH     "/data/fsalgo_rdc.cal"
//#define FALGO_COEF_RANGE_PATH "/data/misc/audio/claib_range.config"
#define FALGO_COEF_RANGE_PATH "/data/calib_range.config"

#define FSADSP_PARAM_ID_GET_ALGO_LIVEDATA     0x10001FA6
#define FSADSP_PARAM_ID_GET_CALIB_DATA        0x10001FAB
int fsalgo_force_calib(float *calib_result);
struct live_data_info
{
    uint16_t rstrim;
    uint16_t channel;
    uint32_t re25;
    uint32_t f0;
};
#if 0
struct fsm_algo_live_data
{
    uint16_t version;
    uint16_t ndev;
    struct live_data_info live_data[FSM_DEV_CH_MAX];
};
#endif
struct calib_data_info
{
    int32_t re25;
    int32_t tempr;
    int32_t reserve1;
    int32_t f0;
    int32_t q;
    int32_t reserve2;
};

struct fsm_data_range
{
    float r0_min;
    float r0_max;
    float f0_min;
    float f0_max;
};

struct fsm_algo_info {
    uint32_t     miid;
    uint32_t     fe_pcm;
    pthread_t    pthread_id;
    struct mixer *virt_mixer;
    struct mixer *hw_mixer;
    char *pcm_device_name;
    FILE *fp;
    char be_name[128];
};

#define ID_RIFF 0x46464952
#define ID_WAVE 0x45564157
#define ID_FMT  0x20746d66
#define ID_DATA 0x61746164

struct riff_wave_header {
    uint32_t riff_id;
    uint32_t riff_sz;
    uint32_t wave_id;
};

struct chunk_header {
    uint32_t id;
    uint32_t sz;
};

struct chunk_fmt {
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
};

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif
