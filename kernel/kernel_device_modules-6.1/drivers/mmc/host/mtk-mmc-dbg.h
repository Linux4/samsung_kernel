/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _MTK_DBG_H_
#define _MTK_DBG_H_

#if IS_ENABLED(CONFIG_DEVICE_MODULES_MMC_DEBUG)
#include <linux/mmc/host.h>
#include <linux/seq_file.h>

#define MSDC_DEBUG_REGISTER_COUNT		0x63

#define PRINTF_REGISTER_BUFFER_SIZE 512
#define ONE_REGISTER_STRING_SIZE    14

#define MSDC_REG_PRINT(OFFSET, VAL, MSG_SZ, MSG_ACCU_SZ, \
	BUF_SZ, BUF, BUF_CUR, SEQ) \
{ \
	if (SEQ) { \
		seq_printf(SEQ, "R[%x]=0x%.8x\n", OFFSET, VAL); \
		continue; \
	} \
	MSG_ACCU_SZ += MSG_SZ; \
	if (MSG_ACCU_SZ >= BUF_SZ) { \
		pr_info("%s", BUF); \
		memset(BUF, 0, BUF_SZ); \
		MSG_ACCU_SZ = MSG_SZ; \
		BUF_CUR = BUF; \
	} \
	snprintf(BUF_CUR, MSG_SZ+1, "[%.3hx:%.8x]", OFFSET, VAL); \
	BUF_CUR += MSG_SZ; \
}

#define MSDC_RST_REG_PRINT_BUF(MSG_ACCU_SZ, BUF_SZ, BUF, BUF_CUR) \
{ \
	MSG_ACCU_SZ = 0; \
	memset(BUF, 0, BUF_SZ); \
	BUF_CUR = BUF; \
}

/*
 * snprintf may return a value of size or "more" to indicate
 * that the output was truncated, thus be careful of "more"
 * case.
 */
#define SPREAD_PRINTF(buff, size, evt, fmt, args...) \
do { \
	if (buff && size && *(size)) { \
		unsigned long var = snprintf(*(buff), *(size), fmt, ##args); \
		if (var > 0) { \
			if (var > *(size)) \
				var = *(size); \
			*(size) -= var; \
			*(buff) += var; \
		} \
	} \
	if (evt) \
		seq_printf(evt, fmt, ##args); \
	if (!buff && !evt) { \
		pr_info(fmt, ##args); \
	} \
} while (0)

/**********************************************************
 * Command dump                                           *
 **********************************************************/
#define MAGIC_CQHCI_DBG_TYPE 5
#define MAGIC_CQHCI_DBG_NUM_L 100
#define MAGIC_CQHCI_DBG_NUM_U 200
#define MAGIC_CQHCI_DBG_NUM_RI 500

#define MAGIC_CQHCI_DBG_TYPE_DCMD 60
/* softirq type */
#define MAGIC_CQHCI_DBG_TYPE_SIRQ 70

#define ERR_INFO_BAG_RING_ORDER     8
#define ERR_INFO_BAG_RING_SIZE      (1 << ERR_INFO_BAG_RING_ORDER)
#define ERR_BIT_SIZE                64

struct err_info_bag {
	DECLARE_BITMAP(err_bitmap, ERR_BIT_SIZE);
	int64_t		irq_timestamp;
	u32			cmd_opcode;
	u32			cmd_arg;
	u32			cmd_rsp;
	unsigned int		blocks;
	unsigned int		bytes_xfered;
};

/*thread_irq err dump*/
#define ERR_CMD_CRC         (0x1 << 0)
#define ERR_CMD_TMO         (0x1 << 1)
#define ERR_DAT_CRC         (0x1 << 2)
#define ERR_DAT_TMO         (0x1 << 3)

#define ERR_CQHCI_CMD_CRC   (0x1 << 16)
#define ERR_CQHCI_CMD_TMO   (0x1 << 17)
#define ERR_CQHCI_DAT_CRC   (0x1 << 18)
#define ERR_CQHCI_DAT_TMO   (0x1 << 19)

#define EARLY_RW 10
#define RA_FOR_PERF 128

enum mmcdbg_cmd_type {
	MMCDBG_CMD_LIST_DUMP	= 0,
	MMCDBG_PWR_MODE_DUMP	= 1,
	MMCDBG_HEALTH_DUMP		= 2,
	MMCDBG_CMD_LIST_ENABLE	= 3,
	MMCDBG_CMD_LIST_DISABLE = 4,
	MMCDBG_UNKNOWN
};

/**********************************************************
 * Function Declaration                                   *
 **********************************************************/
struct msdc_host;
extern void msdc_dump_info(char **buff, unsigned long *size, struct seq_file *m,
	struct msdc_host *host);
extern int mmc_dbg_register(struct mmc_host *mmc);
#else
#define msdc_dump_info(...)
#define mmc_dbg_register(...)
#endif

#endif  /* _MTK_DBG_H_ */
