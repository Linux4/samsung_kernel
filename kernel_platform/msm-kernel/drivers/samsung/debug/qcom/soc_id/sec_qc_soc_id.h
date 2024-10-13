#ifndef __INTERNAL__SEC_QC_SOC_ID_H__
#define __INTERNAL__SEC_QC_SOC_ID_H__

#include <linux/device.h>

#include <linux/samsung/builder_pattern.h>

union qfprom_jtag {
	u32 raw;
	struct {
		u32 jtag_id:20;
		u32 feature_id:8;
		u32 reserved_0:4;
	};
};

struct jtag_id {
	u32 raw;
};

struct qc_soc_id_drvdata {
	struct builder bd;
	struct device *sec_misc_dev;
	bool use_qfprom_jtag;
	phys_addr_t qfprom_jtag_phys;
	union qfprom_jtag qfprom_jtag_data;
	bool use_jtag_id;
	phys_addr_t jtag_id_phys;
	struct jtag_id jtag_id_data;
};

#endif /* __INTERNAL__SEC_QC_SOC_ID_H__ */
