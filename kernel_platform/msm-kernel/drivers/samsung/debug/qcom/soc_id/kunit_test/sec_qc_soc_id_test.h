#ifndef __KUNIT__SEC_QC_SOC_ID_TEST_H__
#define __KUNIT__SEC_QC_SOC_ID_TEST_H__

#include "../sec_qc_soc_id.h"

extern int __qc_soc_id_parse_dt_qfprom_jtag(struct builder *bd, struct device_node *np);
extern int __qc_soc_id_parse_dt_jtag_id(struct builder *bd, struct device_node *np);

#endif /* __KUNIT__SEC_QC_SOC_ID_TEST_H__ */
