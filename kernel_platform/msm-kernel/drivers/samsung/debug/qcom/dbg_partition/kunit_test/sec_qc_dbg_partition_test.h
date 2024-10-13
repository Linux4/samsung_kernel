#ifndef __KUNIT__SEC_QC_DBG_PARTITION_H__
#define __KUNIT__SEC_QC_DBG_PARTITION_H__

#include <linux/samsung/debug/qcom/sec_qc_dbg_partition.h>

#include "../sec_qc_dbg_partition.h"

extern struct qc_dbg_part_info dbg_part_info[DEBUG_PART_MAX_TABLE];

extern int __dbg_part_parse_dt_bdev_path(struct builder *bd, struct device_node *np);
extern int ____dbg_part_parse_dt_part_table(struct builder *bd, struct device_node *np, struct qc_dbg_part_info *info);
extern bool ____dbg_part_is_valid_index(size_t index, const struct qc_dbg_part_info *info);
extern ssize_t __dbg_part_get_size(size_t index, const struct qc_dbg_part_info *info);

#endif /* __KUNIT__SEC_QC_DBG_PARTITION_H__ */
