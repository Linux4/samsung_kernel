//#include "AudioCommon.h"
//#include <AudioDevice.h>

#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#include "PalCommon.h"
#include "PalApi.h"
#include "fsalgo_dsp_intf.h"
#include "fsalgo_calib.h"
#include "fsalgo_mixer.h"
#include <tinyalsa/asoundlib.h>

#ifdef __LP64__
#define FSM_LIB_PATH  "/vendor/lib64/libfsalgo_intf.so"
#else
#define FSM_LIB_PATH  "/vendor/lib/libfsalgo_intf.so"
#endif

#define CALIB_FILE_BIN "/mnt/vendor/persist/audio/fsalgo_rdc.bin"

#define FSM_CALIB_DEV_NUM   (2)

typedef int (*fsalgo_get_payload)(char *data, unsigned int data_size, uint32_t param_id);
fsalgo_get_payload fsm_monitor_get_data = NULL;

void *fsm_lib_handler = NULL;

extern int fsm_tmixer_ctl_set(char *control, char *str_value);
static pthread_mutex_t g_fsmLock = PTHREAD_MUTEX_INITIALIZER;

static int fsm_init_dsp_intf() {
    int ret = 0;

    fsm_lib_handler = dlopen(FSM_LIB_PATH, RTLD_NOW);
    if (fsm_lib_handler == NULL) {
        PAL_ERR(LOG_TAG, "open lib %s failed", FSM_LIB_PATH);
        ret = FSM_CODE_FAIL;
    }

    fsm_monitor_get_data = (fsalgo_get_payload)dlsym(fsm_lib_handler, "fsalgo_get_payload_from_dsp");
    if(fsm_monitor_get_data == NULL) {
        PAL_ERR(LOG_TAG, "get func failed");
        ret = FSM_CODE_FAIL;
    }

    return ret;
}

static int fsm_get_data_range(struct fsm_data_range *range)
{
    FILE *fp = NULL;
    int data_len = 4;
    int i, j;

    if (range == NULL){
        PAL_ERR(LOG_TAG, "invalid pointer");
        return FSM_CODE_FAIL;
    }

    fp = fopen(FALGO_COEF_RANGE_PATH, "rb");
    if (fp == NULL) {
        PAL_ERR(LOG_TAG, "open path(%s) failed", FALGO_COEF_RANGE_PATH);
        return FSM_CODE_FAIL;
    }

    for (i = 0; i < FSM_CALIB_DEV_NUM; i++) {
        j = fscanf(fp, "%lf %lf %lf %lf", &range[i * data_len + 0], &range[i * data_len + 1],// 4 elements each line
                                          &range[i * data_len + 2], &range[i * data_len + 3]);
        if (j != data_len) {
            PAL_ERR(LOG_TAG, "get coef range failed");
            return FSM_CODE_FAIL;
        }
        PAL_INFO(LOG_TAG, "dev[%d] range re25[%f-%f] f0[%f-%f] ", i, range[i * data_len + 0],
              range[i * data_len + 1], range[i * data_len + 2], range[i * data_len + 3]);
    }
    fclose(fp);
    return FSM_CODE_OK;
}


/*******************************************************
* Function: fsalgo_force_calib
* Description:
*       get f0/r0 data
* Parameters:
        calib_result: store f0/r0 data, such as:
                      for r0 only:
                      calib_result[0] = r0[0]
                      calib_result[1] = r0[1]
                      for r0 and f0:
                      calib_result[0] = r0[0]
                      calib_result[1] = f0[0]
                      calib_result[2] = r0[1]
                      calib_result[3] = f0[1]
********************************************************/
int fsalgo_force_calib(float *calib_result) {
    int ret, i;
    FILE *fp = NULL;
    float r0, f0;
    struct calib_data_info live_data[FSM_CALIB_DEV_NUM];
    struct fsm_data_range spk_range[FSM_CALIB_DEV_NUM];

    if (calib_result == NULL) {
        PAL_ERR(LOG_TAG, "ptr is null");
        goto exit;
    }

    PAL_INFO(LOG_TAG, "process +");
    PAL_INFO(LOG_TAG, "device num:%d", FSM_CALIB_DEV_NUM);
    fsm_get_data_range(spk_range);
    //fsm_init_dsp_intf();
    fsm_tmixer_ctl_set("FSM_Scene", "16");
    usleep(2500 * 1000);
    ret = fsm_get_payload_from_dsp((char *)live_data, sizeof(struct calib_data_info) * FSM_CALIB_DEV_NUM, FSADSP_PARAM_ID_GET_CALIB_DATA);
    if (ret < 0) {
        PAL_ERR(LOG_TAG, "get livedata info failed");
        goto exit;
    }

    fsm_tmixer_ctl_set("FSM_Scene", "0");
    for (i = 0; i < FSM_CALIB_DEV_NUM; i++) {
        PAL_DBG(LOG_TAG, "livedata re25[%d] = %d", i, live_data[i].re25);
        PAL_DBG(LOG_TAG, "livedata tempr[%d] = %d", i, live_data[i].tempr);
        PAL_DBG(LOG_TAG, "livedata f0[%d] = %d", i, live_data[i].f0);
        PAL_DBG(LOG_TAG, "livedata q[%d] = %d", i, live_data[i].q);
        r0 = live_data[i].re25 / 4096.0;
        f0 = live_data[i].f0 / 256.0;
        *calib_result = r0; calib_result++;
        //*calib_result = f0; calib_result++;
        PAL_INFO(LOG_TAG, "r0:%f f0:%f", r0, f0);
        //if (r0 > spk_range[i].r0_max || r0 < spk_range[i].r0_min || f0 > spk_range[i].f0_max || f0 < spk_range[i].f0_min) {
        if (r0 > spk_range[i].r0_max || r0 < spk_range[i].r0_min) {
            PAL_ERR(LOG_TAG, "dev[%d] invalid r0:%f f0:%f", i, r0, f0);
            goto exit;
        }
    }

    fp = fopen(FSM_CALIB_DATA_PATH, "rb+");
    if (fp) {
        for (i = 0; i < FSM_CALIB_DEV_NUM; i++) {
            ret = fwrite(&live_data[i].re25, sizeof(live_data[i].re25), 1, fp);
            PAL_DBG(LOG_TAG, "write data to %s r0[%d]:%f ret=%d", FSM_CALIB_DATA_PATH, i, live_data[i].re25 / 4096.0, ret);
            //fwrite(&live_data[i].f0, sizeof(live_data[i].f0), 1, fp);
        }
        fclose(fp);
    } else {
        PAL_ERR(LOG_TAG, "open %s to store data failed", FSM_CALIB_DATA_PATH);
        goto exit;
    }
    PAL_INFO(LOG_TAG, "process -");

exit:
    if(fp)
        fclose(fp);
    if (fsm_lib_handler)
        dlclose(fsm_lib_handler);
    pthread_mutex_unlock(&g_fsmLock);

    return FSM_CODE_OK;
}

