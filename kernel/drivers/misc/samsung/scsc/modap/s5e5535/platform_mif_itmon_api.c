#include "platform_mif.h"
#include "modap/platform_mif_regmap_api.h"
#include "platform_mif_itmon_api.h"
#include "mif_reg.h"

extern char *ka_patch;

#if IS_ENABLED(CONFIG_EXYNOS_ITMON) || IS_ENABLED(CONFIG_EXYNOS_ITMON_V2)

#define ITMON_ERRCODE_DECERR       (1)

static void wlbt_karam_dump(struct platform_mif *platform)
{
	unsigned int ka_addr = PMU_BOOT_RAM_START;
	unsigned int val;
	unsigned int ka_addr_end;
	struct regmap *regmap = platform_mif_get_regmap(platform, BOOT_CFG);

	if (is_ka_patch_in_fw(platform))
		ka_addr_end = ka_addr + platform->ka_patch_len;
	else
		ka_addr_end = ka_addr + sizeof(ka_patch);

	SCSC_TAG_INFO(PLAT_MIF, "Print KARAM area START:0x%p END:0x%p\n", ka_addr, ka_addr_end);

//	regmap_write(regmap, PMU_BOOT, PMU_BOOT_AP_ACC);

	while (ka_addr < ka_addr_end) {
		regmap_read(regmap, ka_addr, &val);
		SCSC_TAG_INFO(PLAT_MIF, "0x%08x: 0x%08x\n", ka_addr, val);
		ka_addr += (unsigned int)sizeof(*ka_patch);
	}

//	regmap_write(regmap, PMU_BOOT, PMU_BOOT_PMU_ACC);
}

static int wlbt_itmon_notifier(struct notifier_block *nb,
		unsigned long action, void *nb_data)
{
	struct platform_mif *platform = container_of(nb, struct platform_mif, itmon_nb);
	int ret = NOTIFY_DONE;
	struct itmon_notifier *itmon_data = (struct itmon_notifier *)nb_data;

	if(!itmon_data) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "itmon_data is NULL");
		goto error_exit;
	}

	if (itmon_data->dest &&
		(!strncmp("WLBT", itmon_data->dest, sizeof("WLBT") - 1))) {
		platform_wlbt_regdump(&platform->interface);
		if((itmon_data->target_addr >= PMU_BOOT_RAM_START)
			&& (itmon_data->target_addr <= PMU_BOOT_RAM_END))
			wlbt_karam_dump(platform);
		ret = ITMON_S2D_MASK;
	} else if (itmon_data->port &&
		(!strncmp("WLBT", itmon_data->port, sizeof("WLBT") - 1))) {
		platform_wlbt_regdump(&platform->interface);
		if (platform->mem_start)
			SCSC_TAG_INFO(PLAT_MIF, "Physical mem_start addr: 0x%lx\n", platform->mem_start);
#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
        	if (platform->paddr)
			SCSC_TAG_INFO(PLAT_MIF, "Physical memlog_start addr: 0x%lx\n", platform->paddr);
#endif
		SCSC_TAG_INFO(PLAT_MIF, "ITMON Type: %d, Error code: %d\n", itmon_data->read, itmon_data->errcode);
		ret = ((!itmon_data->read) && (itmon_data->errcode == ITMON_ERRCODE_DECERR)) ? ITMON_SKIP_MASK : NOTIFY_OK;
	} else if (itmon_data->master &&
		(!strncmp("WLBT", itmon_data->master, sizeof("WLBT") - 1))) {
		platform_wlbt_regdump(&platform->interface);
		if (platform->mem_start)
			SCSC_TAG_INFO(PLAT_MIF, "Physical mem_start addr: 0x%lx\n", platform->mem_start);
#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
        	if (platform->paddr)
			SCSC_TAG_INFO(PLAT_MIF, "Physical memlog_start addr: 0x%lx\n", platform->paddr);
#endif
		SCSC_TAG_INFO(PLAT_MIF, "ITMON Type: %d, Error code: %d\n", itmon_data->read, itmon_data->errcode);
		ret = ((!itmon_data->read) && (itmon_data->errcode == ITMON_ERRCODE_DECERR)) ? ITMON_SKIP_MASK : NOTIFY_OK;
	}

error_exit:
	return ret;
}

void platform_mif_itmon_api_init(struct platform_mif *platform)
{
	platform->itmon_nb.notifier_call = wlbt_itmon_notifier;
	itmon_notifier_chain_register(&platform->itmon_nb);
}
#else
void platform_mif_itmon_api_init(struct platform_mif *platform)
{
	(void)platform;
}
#endif
