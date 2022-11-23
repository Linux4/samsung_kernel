#ifndef __SPRD_SOC_ID_DRV_H__
#define __SPRD_SOC_ID_DRV_H__

typedef enum {
	AON_CHIP_ID = 0,
	AON_PLAT_ID,
	AON_IMPL_ID,
	AON_MFT_ID,
	AON_VER_ID,
} sprd_soc_id_type_t;

#if defined(CONFIG_SPRD_SOCID)
int sprd_get_soc_id(sprd_soc_id_type_t soc_id_type, u32 *id, int id_len);
#else
static int sprd_get_soc_id(sprd_soc_id_type_t soc_id_type, u32 *id, int id_len)
{
	return -EINVAL;
}
#endif
#endif
