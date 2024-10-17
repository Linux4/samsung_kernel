#include "scsc_mif_abs.h"
int platform_mif_manual_pmu_reset_assert(
	struct scsc_mif_abs *interface)
{
#if 0
	int ret;
	unsigned long timeout;

	/* WLBT_INT_EN[PWR_REQ_F] = 1 enable */
	ret = regmap_update_bits(platform->pmureg, WLBT_INT_EN,
			PWR_REQ_F, 0x0);
	if (ret < 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
				"Failed to update WLBT_INT_EN[PWR_REQ_F] %d\n", ret);
		return ret;
	}
	regmap_read(platform->pmureg, WLBT_INT_EN, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
		"updated successfully : WLBT_INT_EN[PWR_REQ_F] 0x%x\n", val);

	/* WLBT_INT_EN[TCXO_REQ_F] = 1 enable */
	ret = regmap_update_bits(platform->pmureg, WLBT_INT_EN,
			TCXO_REQ_F, 0x0);
	if (ret < 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
				"Failed to update WLBT_INT_EN[TCXO_REQ_F] %d\n", ret);
		return ret;
	}
	regmap_read(platform->pmureg, WLBT_INT_EN, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
		"updated successfully : WLBT_INT_EN[TCXO_REQ_F] 0x%x\n", val);

	/* WLBT_CTRL_NS[WLBT_ACTIVE_EN] = 0 */
	ret = regmap_update_bits(platform->pmureg, WLBT_CTRL_NS, WLBT_ACTIVE_EN, 0x0);
	if (ret < 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to Set WLBT_CTRL_NS[WLBT_ACTIVE_EN]: %d\n", ret);
	}
	regmap_read(platform->pmureg, WLBT_CTRL_NS, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
		"updated successfully WLBT_CTRL_NS[WLBT_ACTIVE_EN]: 0x%x\n", val);

	/* Power Down */
	ret = regmap_write_bits(platform->pmureg, WLBT_CONFIGURATION,
		LOCAL_PWR_CFG, 0x0);
	if (ret < 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to update WLBT_CONFIGURATION[LOCAL_PWR_CFG]: %d\n", ret);
		return ret;
	}

	regmap_read(platform->pmureg, WLBT_CONFIGURATION, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
		"updated successfully WLBT_CONFIGURATION[LOCAL_PWR_CFG]: 0x%x\n", val);

	/* Wait for power Off WLBT_STATUS[WLBT_STATUS_BIT0] = 0 */
	timeout = jiffies + msecs_to_jiffies(500);
	do {
		regmap_read(platform->pmureg, WLBT_STATUS, &val);
		val &= (u32)WLBT_STATUS_BIT0;
		if (val == 0) {
			SCSC_TAG_INFO(PLAT_MIF, "WLBT_STATUS 0x%x\n", val);
			/* re affirming power down by reading WLBT_STATES */
			/* STATES[7:0] = 0x80 for Power Down */
			regmap_read(platform->pmureg, WLBT_STATES, &val);
			SCSC_TAG_INFO(PLAT_MIF, "Power down complete: WLBT_STATES 0x%x\n", val);
			return 0;
		}
	} while (time_before(jiffies, timeout));

	/* Timed out */
	SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Timeout waiting for WLBT_STATUS status\n");
	regmap_read(platform->pmureg, WLBT_STATUS, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_STATUS 0x%x\n", val);
	regmap_read(platform->pmureg, WLBT_DEBUG, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_DEBUG 0x%x\n", val);
	regmap_read(platform->pmureg, WLBT_STATES, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_STATES 0x%x\n", val);
#endif
	return -1;
}

int manual_pmu_reset_release(struct scsc_mif_abs *interface, int *init_done)
{
	return -1;
}
