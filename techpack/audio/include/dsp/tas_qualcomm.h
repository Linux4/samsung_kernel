/*
 * TAS256X Algorithm Support
 * Qualcomm Platform Support File
 *
 * Author: Vijeth P O
 * Date: 21-05-20
 */

#ifndef __TAS_QUALCOMM__
#define __TAS_QUALCOMM__

#include <dsp/apr_audio-v2.h>

/*Added for DC Detection*/
#define CAPI_V2_TAS_SA_DC_DETECT	0x40404040

#define TAS_PAYLOAD_SIZE        14

struct afe_smartamp_get_set_params_t {
	uint32_t payload[TAS_PAYLOAD_SIZE];
} __packed;

#ifdef INSTANCE_ID_0
struct afe_smartamp_get_calib {
	struct apr_hdr hdr;
	struct mem_mapping_hdr mem_hdr;
	struct param_hdr_v3 pdata;
	struct afe_smartamp_get_set_params_t res_cfg;
} __packed;

struct afe_smartamp_calib_get_resp {
	uint32_t status;
	struct param_hdr_v3 pdata;
	struct afe_smartamp_get_set_params_t res_cfg;
} __packed;
#else
struct afe_smartamp_config_command {
	struct apr_hdr                      hdr;
	struct afe_port_cmd_set_param_v2    param;
	struct afe_port_param_data_v2       pdata;
	struct afe_smartamp_get_set_params_t  prot_config;
} __packed;

struct afe_smartamp_get_calib {
	struct apr_hdr hdr;
	struct afe_port_cmd_get_param_v2   get_param;
	struct afe_port_param_data_v2      pdata;
	struct afe_smartamp_get_set_params_t   res_cfg;
} __packed;

struct afe_smartamp_calib_get_resp {
	uint32_t status;
	struct afe_port_param_data_v2 pdata;
	struct afe_smartamp_get_set_params_t res_cfg;
} __packed;
#endif /*APR_VERSION_V3*/

typedef struct {
	int channel;
} dc_detection_data_t;

void register_tas256x_reset_func(void *fptr, dc_detection_data_t *data);
void afe_tas_smartamp_init(uint32_t rx_module_id, uint32_t tx_module_id);
int afe_tas_smartamp_get_calib_data(uint32_t module_id, uint32_t param_id,
		int32_t length, uint8_t *data, uint16_t port_id);
int afe_tas_smartamp_set_calib_data(uint32_t module_id, uint32_t param_id,
		int32_t length, uint8_t *data, uint16_t port_id);

#endif /*__TAS_QUALCOMM__*/
