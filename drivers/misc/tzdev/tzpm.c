#include <linux/module.h>
#include <linux/timer.h>
#include <linux/suspend.h>
#include <linux/device.h>

#include "tzlog.h"
#include "tzpm.h"

struct device_driver tzdev_debug_name = {
	.name = "TzDev"
};

struct device tzdev_debug_subname = {
	.driver = &tzdev_debug_name
};

struct device *tzdev_mcd = &tzdev_debug_subname;


#ifdef CONFIG_TZDEV_MSM_CRYPTO_WORKAROUND
	#include <linux/clk.h>
	#include <linux/err.h>

	struct clk *qc_ce_iface_clk = NULL;
	struct clk *qc_ce_core_clk = NULL;
	struct clk *qc_ce_bus_clk = NULL;

#endif /* CONFIG_TZDEV_MSM_CRYPTO_WORKAROUND */

#if defined(CONFIG_TZDEV_MSM_CRYPTO_WORKAROUND) && defined(TZDEV_USE_DEVICE_TREE)
	#include <linux/of.h>
	#define QSEE_CE_CLK_100MHZ 100000000
	struct clk *qc_ce_core_src_clk = NULL;
#endif /* CONFIG_TZDEV_MSM_CRYPTO_WORKAROUND && TZDEV_USE_DEVICE_TREE */


#ifdef CONFIG_TZDEV_MSM_CRYPTO_WORKAROUND

int qc_pm_clock_initialize(void)
{
	int ret = 0;

#ifdef TZDEV_USE_DEVICE_TREE
	/* Get core clk src */
	qc_ce_core_src_clk = clk_get(tzdev_mcd, "core_clk_src");
	if (IS_ERR(qc_ce_core_src_clk)) {
		ret = PTR_ERR(qc_ce_core_src_clk);
		tzdev_print(0,
				"cannot get core clock src with error: %d",
				ret);
		goto error;
	} else {
		int ce_opp_freq_hz = QSEE_CE_CLK_100MHZ;

		if (of_property_read_u32(tzdev_mcd->of_node,
					 "qcom,ce-opp-freq",
					 &ce_opp_freq_hz)) {
			ce_opp_freq_hz = QSEE_CE_CLK_100MHZ;
			tzdev_print(0,
					"cannot get ce clock frequency. Using %d",
					ce_opp_freq_hz);
		}
		ret = clk_set_rate(qc_ce_core_src_clk, ce_opp_freq_hz);
		if (ret) {
			clk_put(qc_ce_core_src_clk);
			qc_ce_core_src_clk = NULL;
			tzdev_print(0, "cannot set core clock src rate");
			ret = -EIO;
			goto error;
		}
	}
#endif  /* CONFIG_TZDEV_MSM_CRYPTO_WORKAROUND && TZDEV_USE_DEVICE_TREE */

	/* Get core clk */
	qc_ce_core_clk = clk_get(tzdev_mcd, "core_clk");
	if (IS_ERR(qc_ce_core_clk)) {
		ret = PTR_ERR(qc_ce_core_clk);
		tzdev_print(0, "cannot get core clock");
		goto error;
	}
	/* Get Interface clk */
	qc_ce_iface_clk = clk_get(tzdev_mcd, "iface_clk");
	if (IS_ERR(qc_ce_iface_clk)) {
		clk_put(qc_ce_core_clk);
		ret = PTR_ERR(qc_ce_iface_clk);
		tzdev_print(0, "cannot get iface clock");
		goto error;
	}
	/* Get AXI clk */
	qc_ce_bus_clk = clk_get(tzdev_mcd, "bus_clk");
	if (IS_ERR(qc_ce_bus_clk)) {
		clk_put(qc_ce_iface_clk);
		clk_put(qc_ce_core_clk);
		ret = PTR_ERR(qc_ce_bus_clk);
		tzdev_print(0, "cannot get AXI bus clock");
		goto error;
	}

	tzdev_print(0, "obtained crypto clocks\n");
	return ret;

error:
	qc_ce_core_clk = NULL;
	qc_ce_iface_clk = NULL;
	qc_ce_bus_clk = NULL;

	return ret;
}

void qc_pm_clock_finalize(void)
{
	if (qc_ce_bus_clk != NULL)
		clk_put(qc_ce_bus_clk);

	if (qc_ce_iface_clk != NULL)
		clk_put(qc_ce_iface_clk);

	if (qc_ce_core_clk != NULL)
		clk_put(qc_ce_core_clk);

#ifdef TZDEV_USE_DEVICE_TREE
	if (qc_ce_core_src_clk != NULL)
		clk_put(qc_ce_core_src_clk);
#endif  /* CONFIG_TZDEV_MSM_CRYPTO_WORKAROUND && TZDEV_USE_DEVICE_TREE */
}

int qc_pm_clock_enable(void)
{
	int rc = 0;

	rc = clk_prepare_enable(qc_ce_core_clk);
	if (rc) {
		tzdev_print(0, "cannot enable clock");
	} else {
		rc = clk_prepare_enable(qc_ce_iface_clk);
		if (rc) {
			clk_disable_unprepare(qc_ce_core_clk);
			tzdev_print(0, "cannot enable clock");
		} else {
			rc = clk_prepare_enable(qc_ce_bus_clk);
			if (rc) {
				clk_disable_unprepare(qc_ce_iface_clk);
				tzdev_print(0, "cannot enable clock");
			}
		}
	}
	if (rc)
		tzdev_print(0, "tzd: qc_pm_clock_enable() failed!\n");

	return rc;
}

void qc_pm_clock_disable(void)
{
	if (qc_ce_iface_clk != NULL)
		clk_disable_unprepare(qc_ce_iface_clk);

	if (qc_ce_core_clk != NULL)
		clk_disable_unprepare(qc_ce_core_clk);

	if (qc_ce_bus_clk != NULL)
		clk_disable_unprepare(qc_ce_bus_clk);
}

#endif /* CONFIG_TZDEV_MSM_CRYPTO_WORKAROUND */
