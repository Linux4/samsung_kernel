#ifndef _SMART_AMP_H
#define _SMART_AMP_H

#include <linux/types.h>
#include <dsp/apr_audio-v2.h>
#include <sound/soc.h>

#define SMART_AMP 

/* Below 3 should be same as in aDSP code */
#define AFE_PARAM_ID_SMARTAMP_DEFAULT   0x10001166
#define AFE_SMARTAMP_MODULE_RX          0x11111112  /*Rx module*/
#define AFE_SMARTAMP_MODULE_TX          0x11111111  /*Tx module*/

#define CAPI_V2_TAS_TX_ENABLE 		0x10012D14
#define CAPI_V2_TAS_TX_CFG    		0x10012D16
#define CAPI_V2_TAS_RX_ENABLE 		0x10012D13
#define CAPI_V2_TAS_RX_CFG    		0x10012D15

#define MAX_DSP_PARAM_INDEX		600

#define TAS_PAYLOAD_SIZE 	14
#define TAS_GET_PARAM		1
#define TAS_SET_PARAM		0

#define MAX_CHANNELS 	2
#define LEFT			0
#define RIGHT			1
#define CHANNEL0		1
#define CHANNEL1		2

#define TRUE		1
#define FALSE		0

#define SLAVE1		0x98
#define SLAVE2		0x9A
#define SLAVE3		0x9C
#define SLAVE4		0x9E

#define TAS_SA_IS_SPL_IDX(X)	((((X) >= 3810) && ((X) < 3899)) ? 1 : 0)
#define TAS_CALC_PARAM_IDX(INDEX, LENGTH, CHANNEL)		((INDEX ) | (LENGTH << 16) | (CHANNEL << 24))

struct afe_smartamp_set_params_t {
	uint32_t  payload[TAS_PAYLOAD_SIZE];
} __packed;

struct afe_smartamp_get_params_t {
    uint32_t payload[TAS_PAYLOAD_SIZE];
} __packed;

struct afe_smartamp_get_calib {
	struct apr_hdr hdr;
	struct mem_mapping_hdr mem_hdr;
	struct param_hdr_v3 pdata;
	struct afe_smartamp_get_params_t res_cfg;
} __packed;

struct afe_smartamp_calib_get_resp {
	uint32_t status;
	struct param_hdr_v3 pdata;
	struct afe_smartamp_get_params_t res_cfg;
} __packed;

void smartamp_add_algo(uint8_t channels, int port_id);
void smartamp_remove_algo(void);

int afe_smartamp_get_calib_data(struct afe_smartamp_get_calib *calib_resp,
		uint32_t param_id, uint32_t module_id, uint32_t port_id);

int afe_smartamp_set_calib_data(uint32_t param_id,struct afe_smartamp_set_params_t *prot_config,
		uint8_t length, uint32_t module_id, uint32_t port_id);

int afe_smartamp_algo_ctrl(u8 *user_data, uint32_t param_id,
	uint8_t get_set, uint32_t length, uint32_t module_id);

#endif
