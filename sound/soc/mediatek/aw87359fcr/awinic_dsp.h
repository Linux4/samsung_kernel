#ifndef __AWINIC_DSP_H__
#define __AWINIC_DSP_H__

/*#define AW_MTK_OPEN_DSP_PLATFORM*/

/**********************************************************
 * aw87xxx dsp
***********************************************************/
#define AWINIC_DSP_MSG_HDR_VER (1)

#define INLINE_PARAM_ID_NULL				(0x00000000)
#define INLINE_PARAM_ID_ENABLE_CALI			(0x00000001)
#define INLINE_PARAM_ID_ENABLE_HMUTE			(0x00000002)
#define INLINE_PARAM_ID_F0_Q				(0x00000003)
#define INLINE_PARAM_ID_ACTIVE_FLAG			(0x00000004)
#define INLINE_PARAM_ID_REAL_DATA			(0x00000005)
#define INLINE_PARAM_ID_DIRECT_CURRENT_FLAG		(0x00000006)
#define INLINE_PARAMS_ID_SPK_STATUS			(0x00000007)
#define INLINE_PARAMS_ID_VERSION			(0x00000008)


/*dsp params id*/
#define AFE_PARAM_ID_AWDSP_RX_SET_ENABLE		(0x10013D11)
#define AFE_PARAM_ID_AWDSP_RX_PARAMS			(0x10013D12)
#define AFE_PARAM_ID_AWDSP_TX_SET_ENABLE		(0x10013D13)
#define AFE_PARAM_ID_AWDSP_RX_VMAX_L			(0X10013D17)
#define AFE_PARAM_ID_AWDSP_RX_VMAX_R			(0X10013D18)
#define AFE_PARAM_ID_AWDSP_RX_CALI_CFG_L		(0X10013D19)
#define AFE_PARAM_ID_AWDSP_RX_CALI_CFG_R		(0x10013d1A)
#define AFE_PARAM_ID_AWDSP_RX_RE_L			(0x10013d1B)
#define AFE_PARAM_ID_AWDSP_RX_RE_R			(0X10013D1C)
#define AFE_PARAM_ID_AWDSP_RX_NOISE_L			(0X10013D1D)
#define AFE_PARAM_ID_AWDSP_RX_NOISE_R			(0X10013D1E)
#define AFE_PARAM_ID_AWDSP_RX_F0_L			(0X10013D1F)
#define AFE_PARAM_ID_AWDSP_RX_F0_R			(0X10013D20)
#define AFE_PARAM_ID_AWDSP_RX_REAL_DATA_L		(0X10013D21)
#define AFE_PARAM_ID_AWDSP_RX_REAL_DATA_R		(0X10013D22)
#define AFE_PARAM_ID_AWDSP_RX_MSG			(0X10013D2A)

enum aw_active_algo_st {
	AW_ALGO_DISABLE = -1,
	AW_ACTIVE_ENABLE = 0,
	AW_ACTIVE_DISABLE = 1,
};

enum aw_dsp_msg_type {
	DSP_MSG_TYPE_DATA = 0,
	DSP_MSG_TYPE_CMD = 1,
};

enum afe_param_id_adsp {
	INDEX_PARAMS_ID_RX_PARAMS = 0,
	INDEX_PARAMS_ID_RX_ENBALE,
	INDEX_PARAMS_ID_TX_ENABLE,
	INDEX_PARAMS_ID_RX_VMAX,
	INDEX_PARAMS_ID_RX_CALI_CFG,
	INDEX_PARAMS_ID_RX_RE,
	INDEX_PARAMS_ID_RX_NOISE,
	INDEX_PARAMS_ID_RX_F0,
	INDEX_PARAMS_ID_RX_REAL_DATA,
	INDEX_PARAMS_ID_AWDSP_RX_MSG,
	INDEX_PARAMS_ID_MAX
};

struct aw_dsp_msg_hdr {
	int32_t type;
	int32_t opcode_id;
	int32_t version;
	int32_t reserver[3];
};


bool aw87xx_platform_init(void);
int aw_get_vmax_from_dsp(uint32_t *vmax, int32_t channel);
int aw_set_vmax_to_dsp(uint32_t vmax, int32_t channel);
int aw_get_active_flag(uint32_t *active_flag, int32_t channel);


#endif
