#ifndef FSALGO_DSP_INTF_H_
#define FSALGO_DSP_INTF_H_
#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#ifdef FEATURE_IPQ_OPENWRT
#include <audio_utils/log.h>
#else
#include <log/log.h>
#endif

#include "PalDefs.h"

#define FSM_OK   (0)
#define FSM_ERR  (-1)
#undef  LOG_TAG
#define LOG_TAG  "PAL:FourSemi:INTF"
#define FS_ALIGN_4BYTE(x)         (((x) + 3) & (~3))
#define FS_ALIGN_8BYTE(x)         (((x) + 7) & (~7))
#define FS_PADDING_ALIGN_8BYTE(x) ((((x) + 7) & 7) ^ 7)

#define FSM_CTL_CONTROL           ("control")
#define FSM_CTL_GET_PARAM         ("getParam")
#define FSM_CTL_SET_PARAM         ("setParam")
#define FSM_CTL_METADATA          ("metadata")
#define FSM_CTL_GET_TAGGED_INFO   ("getTaggedInfo")

struct fsalgo_platform_info {
    uint32_t     miid;
    int     fe_pcm;
    char *pcm_device_name;
};

int fsm_send_payload_to_dsp(char *data, unsigned int data_size, uint32_t param_id);
int fsm_get_payload_from_dsp(char *data, unsigned int data_size, uint32_t param_id);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif
#endif