// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#define pr_fmt(fmt) "sap " fmt

#include <linux/of.h>
#include <linux/io.h>
#include <linux/soc/mediatek/mtk-mbox.h>
#include <linux/soc/mediatek/mtk_tinysys_ipi.h>
#include "scp_helper.h"
#include "sap.h"

#define B_SAP_CACHE_DBG_EN		(1 << 30)

enum MDUMP_t {
	MDUMP_START,
	MDUMP_L1CACHE,
	MDUMP_REGDUMP,
	MDUMP_TBUF,
	MDUMP_TOTAL,
};

struct reg_save_st {
	uint32_t addr;
	uint32_t size;
};

struct sap_coredump {
	uint8_t *buf;
	uint32_t size;
	uint32_t prefix[MDUMP_TOTAL];
};

struct sap_status_reg {
	uint32_t status;
	uint32_t pc;
	uint32_t lr;
	uint32_t sp;
	uint32_t pc_latch;
	uint32_t lr_latch;
	uint32_t sp_latch;
};

struct sap_device {
	bool enable;
	uint8_t core_id;
	uint32_t l2tcm_offset;
	uint32_t secure_dump_size;
	struct scp_region_info_st *region_info;
	struct scp_region_info_st region_info_copy;
	void __iomem *loader_virt;
	void __iomem *regdump_virt;
	void __iomem *sram_start_virt;
	void __iomem *reg_cfg;
	void __iomem *l1cache;

	struct mtk_mbox_device mbox_dev;
	struct sap_coredump coredump;
	struct sap_status_reg status_reg;
};

static const struct reg_save_st reg_save_list[] = {
	{0x00040000, 0x180},
	{0x00042000, 0x260},
	{0x00043000, 0x120},
};

static struct sap_device sap_dev;
struct mtk_ipi_device sap_ipidev;
EXPORT_SYMBOL(sap_ipidev);

static uint8_t *get_MDUMP_addr(enum MDUMP_t type)
{
	struct sap_coredump *coredump = &sap_dev.coredump;

	return coredump->buf + coredump->prefix[type - 1];
}

static uint32_t get_MDUMP_size(enum MDUMP_t type)
{
	struct sap_coredump *coredump = &sap_dev.coredump;

	return coredump->prefix[type] - coredump->prefix[type - 1];
}

static void sap_l1cache_dump(uint8_t *out, uint8_t *out_end)
{
	uint32_t sec_ctrl_value = 0;
	uint32_t dump_size = out_end - out;

	sec_ctrl_value = readl(R_SEC_CTRL);
	writel(sec_ctrl_value | B_SAP_CACHE_DBG_EN, R_SEC_CTRL);
	memcpy_from_scp((void *)out, sap_dev.l1cache, dump_size);
	writel(sec_ctrl_value, R_SEC_CTRL);
}

static void sap_register_dump(uint8_t *out, uint8_t *out_end)
{
	int i = 0;
	void *from = NULL;
	uint32_t *buf = (uint32_t *)out;
	int size_limit = sizeof(reg_save_list) / sizeof(struct reg_save_st);

	for (i = 0; i < size_limit; i++) {
		if (((void *)buf + reg_save_list[i].size
			+ sizeof(struct reg_save_st)) > (void *)out_end) {
			pr_notice("%s overflow\n", __func__);
			break;
		}
		*buf = reg_save_list[i].addr;
		buf++;
		*buf = reg_save_list[i].size;
		buf++;
		from = sap_dev.regdump_virt + (reg_save_list[i].addr & 0xfffff);
		memcpy_from_scp(buf, from, reg_save_list[i].size);
		buf += (reg_save_list[i].size / sizeof(uint32_t));
	}
}

void sap_trace_buffer_dump(uint8_t *out, uint8_t *out_end)
{
	uint32_t *buf = (uint32_t *)out;
	uint32_t tmp, index, offset, wbuf_ptr;
	void __iomem *cfg_base = sap_dev.reg_cfg;
	int i = 0;

	wbuf_ptr = readl(cfg_base + CFG_TBUF_WPTR_OFFSET);
	tmp = readl(cfg_base + CFG_DBG_CTRL_OFFSET) & (~M_CORE_TBUF_DBG_SEL);
	for (i = 0; i < 16; i++) {
		index = (wbuf_ptr + i) / 2;
		offset = ((wbuf_ptr + i) % 2) * 0x8;
		writel(tmp | (index << S_CORE_TBUF_DBG_SEL),
			cfg_base + CFG_DBG_CTRL_OFFSET);
		*(buf) = readl(cfg_base + CFG_TBUF_DATA31_0_OFFSET + offset);
		*(buf + 1) = readl(cfg_base + CFG_TBUF_DATA63_32_OFFSET + offset);
		pr_notice("tbuf:%02d:0x%08x::0x%08x\n",	i, *buf, *(buf + 1));
		buf += 2;
	}
}

uint32_t sap_get_coredump_size(void)
{
	struct sap_device *dev = &sap_dev;

	return dev->coredump.size;
}

uint32_t sap_crash_dump(uint8_t *dump_buf)
{
	struct sap_device *dev = &sap_dev;
	uint8_t *dump_start = NULL, *dump_end = NULL;

	if (!dev->enable)
		return 0;

	dev->coredump.buf = dump_buf;

	dump_start = get_MDUMP_addr(MDUMP_L1CACHE);
	dump_end = dump_start + get_MDUMP_size(MDUMP_L1CACHE);
	sap_l1cache_dump(dump_start, dump_end);

	dump_start = get_MDUMP_addr(MDUMP_REGDUMP);
	dump_end = dump_start + get_MDUMP_size(MDUMP_REGDUMP);
	sap_register_dump(dump_start, dump_end);

	dump_start = get_MDUMP_addr(MDUMP_TBUF);
	dump_end = dump_start + get_MDUMP_size(MDUMP_TBUF);
	sap_trace_buffer_dump(dump_start, dump_end);

	return dev->coredump.size;
}

void sap_dump_last_regs(void)
{
	struct sap_status_reg *reg = &sap_dev.status_reg;
	void __iomem *cfg_base = sap_dev.reg_cfg;

	if (!READ_ONCE(sap_dev.enable))
		return;

	reg->status = readl(cfg_base + CFG_STATUS_OFFSET);
	reg->pc = readl(cfg_base + CFG_MON_PC_OFFSET);
	reg->lr = readl(cfg_base + CFG_MON_LR_OFFSET);
	reg->sp = readl(cfg_base + CFG_MON_SP_OFFSET);
	reg->pc_latch = readl(cfg_base + CFG_MON_PC_LATCH_OFFSET);
	reg->lr_latch = readl(cfg_base + CFG_MON_LR_LATCH_OFFSET);
	reg->sp_latch = readl(cfg_base + CFG_MON_SP_LATCH_OFFSET);
}

void sap_show_last_regs(void)
{
	struct sap_status_reg *reg = &sap_dev.status_reg;

	if (!READ_ONCE(sap_dev.enable))
		return;

	pr_notice("reg status = %08x\n", reg->status);
	pr_notice("reg pc = %08x\n", reg->pc);
	pr_notice("reg lr = %08x\n", reg->lr);
	pr_notice("reg sp = %08x\n", reg->sp);
	pr_notice("reg pc_latch = %08x\n", reg->pc_latch);
	pr_notice("reg lr_latch = %08x\n", reg->lr_latch);
	pr_notice("reg sp_latch = %08x\n", reg->sp_latch);
}

uint32_t sap_print_last_regs(char *buf, uint32_t size)
{
	struct sap_status_reg *reg = &sap_dev.status_reg;
	uint32_t len = 0;

	if (!READ_ONCE(sap_dev.enable))
		return 0;

	len += scnprintf(buf + len, size - len,
		"sap_status = %08x\n", reg->status);

	len += scnprintf(buf + len, size - len,
		"sap_pc = %08x\n", reg->pc);

	len += scnprintf(buf + len, size - len,
		"sap_lr = %08x\n", reg->lr);

	len += scnprintf(buf + len, size - len,
		"sap_sp = %08x\n", reg->sp);

	len += scnprintf(buf + len, size - len,
		"sap_pc_latch = %08x\n", reg->pc_latch);

	len += scnprintf(buf + len, size - len,
		"sap_lr_latch = %08x\n", reg->lr_latch);

	len += scnprintf(buf + len, size - len,
		"sap_sp_latch = %08x\n", reg->sp_latch);

	return len;
}


uint32_t sap_dump_detail_buff(uint8_t *buff, uint32_t size)
{
	struct sap_device *dev = &sap_dev;
	struct sap_status_reg *reg = &sap_dev.status_reg;

	if (!READ_ONCE(dev->enable))
		return 0;

	return snprintf(buff, size, "sap pc=0x%08x, lr=0x%08x, sp=0x%08x\n",
		reg->pc, reg->lr, reg->sp);
}

uint32_t sap_get_secure_dump_size(void)
{
	struct device_node *node = NULL;
	const char *sap_status = NULL;
	uint32_t dump_size = 0;

	/*
	 * NOTE: callee before sap_device_probe,
	 * directly access device tree get node value.
	 */
	node = of_find_node_by_name(NULL, "sap");
	if (!node) {
		pr_err("Node mediatek,sap not found\n");
		return 0;
	}

	of_property_read_string(node, "status", &sap_status);
	if (strncmp(sap_status, "okay", sizeof("okay"))) {
		pr_err("sap not enabled, skip\n");
		return 0;
	}

	of_property_read_u32(node, "secure-dump-size", &dump_size);
	return dump_size;
}

uint8_t sap_get_core_id(void)
{
	return sap_dev.core_id;
}

bool sap_enabled(void)
{
	return READ_ONCE(sap_dev.enable);
}
EXPORT_SYMBOL_GPL(sap_enabled);

void sap_cfg_reg_write(uint32_t reg_offset, uint32_t value)
{
	void __iomem *cfg_base = sap_dev.reg_cfg;

	writel(value, cfg_base + reg_offset);
}

uint32_t sap_cfg_reg_read(uint32_t reg_offset)
{
	void __iomem *cfg_base = sap_dev.reg_cfg;

	return readl(cfg_base + reg_offset);
}

void sap_wdt_reset(void)
{
	struct sap_device *dev = &sap_dev;

	if (!READ_ONCE(dev->enable))
		return;

	sap_cfg_reg_write(CFG_WDT_CFG_OFFSET, V_INSTANT_WDT);
}

void sap_wdt_clear(void)
{
	struct sap_device *dev = &sap_dev;

	if (!dev->enable)
		return;

	sap_cfg_reg_write(CFG_WDT_CFG_OFFSET, B_WDT_IRQ);
}

static void sap_setup_pin_table(struct mtk_mbox_device *mbox_dev, uint8_t mbox)
{
	int i = 0, last_ofs = 0, last_idx = 0, last_slot = 0, last_sz = 0;
	struct mtk_mbox_pin_send *send_tbl = NULL;
	struct mtk_mbox_pin_recv *recv_tbl = NULL;
	struct mtk_mbox_info *info_tbl = NULL;

	send_tbl = (struct mtk_mbox_pin_send *)mbox_dev->pin_send_table;
	recv_tbl = (struct mtk_mbox_pin_recv *)mbox_dev->pin_recv_table;
	info_tbl = (struct mtk_mbox_info *)mbox_dev->info_table;

	for (i = 0; i < mbox_dev->send_count; i++) {
		if (mbox == send_tbl[i].mbox) {
			send_tbl[i].offset = last_ofs + last_slot;
			send_tbl[i].pin_index = last_idx + last_sz;
			last_idx = send_tbl[i].pin_index;
			if (info_tbl[mbox].is64d == 1) {
				last_sz = DIV_ROUND_UP(
				send_tbl[i].msg_size, 2);
				last_ofs = last_sz * 2;
				last_slot = last_idx * 2;
			} else {
				last_sz = send_tbl[i].msg_size;
				last_ofs = last_sz;
				last_slot = last_idx;
			}
		} else if (mbox < send_tbl[i].mbox)
			break; /* no need to search the rest ipi */
	}

	for (i = 0; i < mbox_dev->recv_count; i++) {
		if (mbox == recv_tbl[i].mbox) {
			recv_tbl[i].offset = last_ofs + last_slot;
			recv_tbl[i].pin_index = last_idx + last_sz;
			last_idx = recv_tbl[i].pin_index;
			if (info_tbl[mbox].is64d == 1) {
				last_sz = DIV_ROUND_UP(
				recv_tbl[i].msg_size, 2);
				last_ofs = last_sz * 2;
				last_slot = last_idx * 2;
			} else {
				last_sz = recv_tbl[i].msg_size;
				last_ofs = last_sz;
				last_slot = last_idx;
			}
		} else if (mbox < recv_tbl[i].mbox)
			break; /* no need to search the rest ipi */
	}


	if (last_idx > 32 ||
		(last_ofs + last_slot) > (info_tbl[mbox].is64d + 1) * 32) {
		pr_notice("mbox%d ofs(%d)/slot(%d) exceed the maximum\n",
			mbox, last_idx, last_ofs + last_slot);
		WARN_ON(1);
	}
}

static bool sap_parse_ipi_table(struct mtk_mbox_device *mbox_dev,
	struct platform_device *pdev)
{
	enum table_item_num {
		send_item_num = 3,
		recv_item_num = 4
	};
	u32 i, ret, mbox, recv_opt, recv_cells_mode;
	u32 recv_cells_num, lock, buf_full_opt, cb_ctx_opt;
	struct device_node *node = pdev->dev.of_node;
	struct mtk_mbox_info *mbox_info = NULL;
	struct mtk_mbox_pin_send *mbox_pin_send = NULL;
	struct mtk_mbox_pin_recv *mbox_pin_recv = NULL;

	of_property_read_u32(node, "mbox-count", &mbox_dev->count);
	if (!mbox_dev->count) {
		pr_err("mbox count not found\n");
		return false;
	}

	ret = of_property_read_u32(node, "#recv_cells_mode", &recv_cells_mode);
	if (ret) {
		recv_cells_num = recv_item_num;
	} else {
		if (recv_cells_mode == 1)
			recv_cells_num = 7;
		else
			recv_cells_num = recv_item_num;
	}

	mbox_dev->send_count = of_property_count_u32_elems(node, "send-table")
				/ send_item_num;
	if (mbox_dev->send_count <= 0) {
		pr_err("ipi send table not found\n");
		return false;
	}

	mbox_dev->recv_count = of_property_count_u32_elems(node,
		"recv-table") / recv_cells_num;
	if (mbox_dev->recv_count <= 0) {
		pr_err("ipi recv table not found\n");
		return false;
	}
	/* alloc and init scp_mbox_info */
	mbox_dev->info_table = vzalloc(sizeof(struct mtk_mbox_info)
		* mbox_dev->count);
	if (!mbox_dev->info_table)
		return false;

	mbox_info = mbox_dev->info_table;
	for (i = 0; i < mbox_dev->count; ++i) {
		mbox_info[i].id = i;
		mbox_info[i].slot = 64;
		mbox_info[i].enable = 1;
		mbox_info[i].is64d = 1;
	}
	/* alloc and init send table */
	mbox_dev->pin_send_table = vzalloc(sizeof(struct mtk_mbox_pin_send)
		* mbox_dev->send_count);
	if (!mbox_dev->pin_send_table)
		return false;

	mbox_pin_send = mbox_dev->pin_send_table;
	for (i = 0; i < mbox_dev->send_count; ++i) {
		ret = of_property_read_u32_index(node, "send-table",
			i * send_item_num, &mbox_pin_send[i].chan_id);
		if (ret) {
			pr_err("get chan_id fail for send_tbl %u\n", i);
			return false;
		}
		ret = of_property_read_u32_index(node, "send-table",
			i * send_item_num + 1, &mbox);
		if (ret || mbox >= mbox_dev->count) {
			pr_err("get mbox id fail for send_tbl %u\n", i);
			return false;
		}
		/* because mbox and recv_opt is a bit-field */
		mbox_pin_send[i].mbox = mbox;
		ret = of_property_read_u32_index(node, "send-table",
			i * send_item_num + 2, &mbox_pin_send[i].msg_size);
		if (ret) {
			pr_err("get msg_size fail for send_tbl %u", i);
			return false;
		}
	}

	/* alloc and init recv table */
	mbox_dev->pin_recv_table = vzalloc(sizeof(struct mtk_mbox_pin_recv)
		* mbox_dev->recv_count);
	if (!mbox_dev->pin_recv_table)
		return false;

	mbox_pin_recv = mbox_dev->pin_recv_table;
	for (i = 0; i < mbox_dev->recv_count; ++i) {
		ret = of_property_read_u32_index(node, "recv-table",
			i * recv_cells_num, &mbox_pin_recv[i].chan_id);
		if (ret) {
			pr_err("get chan_id fail for recv_tbl %u\n", i);
			return false;
		}
		ret = of_property_read_u32_index(node, "recv-table",
			i * recv_cells_num + 1,	&mbox);
		if (ret || mbox >= mbox_dev->count) {
			pr_err("get mbox_id fail for recv_tbl %u\n", i);
			return false;
		}
		/* because mbox and recv_opt is a bit-field */
		mbox_pin_recv[i].mbox = mbox;
		ret = of_property_read_u32_index(node, "recv-table",
			i * recv_cells_num + 2,	&mbox_pin_recv[i].msg_size);
		if (ret) {
			pr_err("get msg_size fail for recv_tbl %u\n", i);
			return false;
		}
		ret = of_property_read_u32_index(node, "recv-table",
			i * recv_cells_num + 3,	&recv_opt);
		if (ret) {
			pr_err("get recv_opt fail for recv_tbl %u\n", i);
			return false;
		}
		/* because mbox and recv_opt is a bit-field */
		mbox_pin_recv[i].recv_opt = recv_opt;
		if (recv_cells_mode == 1) {
			ret = of_property_read_u32_index(node, "recv-table",
				i * recv_cells_num + 4,	&lock);
			if (ret) {
				pr_err("get lock fail for recv_tbl %u\n", i);
				return false;
			}
			/* because lock is a bit-field */
			mbox_pin_recv[i].lock = lock;
			ret = of_property_read_u32_index(node, "recv-table",
				i * recv_cells_num + 5,	&buf_full_opt);
			if (ret) {
				pr_err("get buf_full_opt fail for recv_tbl %u\n", i);
				return false;
			}
			/* because buf_full_opt is a bit-field */
			mbox_pin_recv[i].buf_full_opt = buf_full_opt;
			ret = of_property_read_u32_index(node, "recv-table",
				i * recv_cells_num + 6,	&cb_ctx_opt);
			if (ret) {
				pr_err("get cb_ctx_opt fail for recv_tbl %u\n", i);
				return false;
			}
			/* because cb_ctx_opt is a bit-field */
			mbox_pin_recv[i].cb_ctx_opt = cb_ctx_opt;
		}
	}

	for (i = 0; i < mbox_dev->count; ++i) {
		mbox_info[i].mbdev = mbox_dev;
		if (mtk_mbox_probe(pdev, mbox_info[i].mbdev, i) < 0)
			continue;

		sap_setup_pin_table(mbox_dev, i);
	}

	return true;
}

static int sap_device_probe(struct platform_device *pdev)
{
	struct sap_device *dev = &sap_dev;
	const char *sap_status = NULL;
	uint8_t core_id = 0;
	uint32_t l2tcm_offset = 0;
	struct sap_coredump *coredump = &dev->coredump;
	struct resource *res = NULL;
	uint8_t i = 0;
	int ret = 0;

	ret = of_property_read_string(pdev->dev.of_node, "status", &sap_status);
	if (ret < 0 || strncmp(sap_status, "okay", sizeof("okay"))) {
		pr_info("sap not enabled, no need and skip\n");
		return 0;
	}

	ret = of_property_read_u8(pdev->dev.of_node, "core-id", &core_id);
	if (ret < 0) {
		pr_err("invalid core_id res, %d\n", ret);
		return -EINVAL;
	}

	ret = of_property_read_u32(pdev->dev.of_node,
		"l2tcm-offset", &l2tcm_offset);
	if (ret < 0 || !l2tcm_offset) {
		pr_err("invalid l2tcm offset\n");
		return -EINVAL;
	}

	WRITE_ONCE(dev->enable, true);
	dev->core_id = core_id;
	dev->l2tcm_offset = l2tcm_offset;
	dev->sram_start_virt = SCP_TCM + l2tcm_offset;
	dev->region_info = dev->sram_start_virt + SCP_REGION_INFO_OFFSET;
	memcpy_from_scp(&dev->region_info_copy,
		dev->region_info, sizeof(dev->region_info_copy));

	dev->loader_virt = ioremap_wc(dev->region_info_copy.ap_loader_start,
		dev->region_info_copy.ap_loader_size);

	dev->regdump_virt = ioremap_wc(dev->region_info_copy.regdump_start,
		dev->region_info_copy.regdump_size);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENOMEM;

	dev->reg_cfg = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR((void const *)dev->reg_cfg))
		return -EINVAL;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res)
		return -ENOMEM;

	dev->l1cache = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR((void const *)dev->l1cache))
		return -EINVAL;

	dev->mbox_dev.name = "sap_mboxdev";
	if (!sap_parse_ipi_table(&dev->mbox_dev, pdev))
		return -EINVAL;

	sap_ipidev.name = "sap_ipidev";
	sap_ipidev.id = IPI_DEV_SAP;
	sap_ipidev.mbdev = &dev->mbox_dev;
	sap_ipidev.pre_cb = (ipi_tx_cb_t)scp_awake_lock;
	sap_ipidev.post_cb = (ipi_tx_cb_t)scp_awake_unlock;
	sap_ipidev.prdata = 0;
	ret = mtk_ipi_device_register(&sap_ipidev, pdev,
		&dev->mbox_dev, SAP_IPI_COUNT);
	if (ret < 0) {
		pr_err("register ipi fail %d\n", ret);
		return ret;
	}

	for (i = MDUMP_L1CACHE; i < MDUMP_TOTAL; ++i) {
		ret = of_property_read_u32_index(pdev->dev.of_node,
			"memorydump", i - 1, &coredump->prefix[i]);
		if (ret) {
			pr_notice("Cannot get memorydump size(%d)\n", i - 1);
			return -EINVAL;
		}
		coredump->prefix[i] += coredump->prefix[i - 1];
	}

	coredump->size = coredump->prefix[MDUMP_TOTAL - 1];
	pr_notice("cordump total size %u\n", coredump->size);
	return 0;
}

static int sap_device_remove(struct platform_device *dev)
{
	return 0;
}

static const struct of_device_id sap_of_ids[] = {
	{ .compatible = "mediatek,sap", },
	{}
};

static struct platform_driver mtk_sap_device = {
	.probe = sap_device_probe,
	.remove = sap_device_remove,
	.driver = {
		.name = "sap",
		.owner = THIS_MODULE,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = sap_of_ids,
#endif
	},
};

void sap_restore_l2tcm(void)
{
	struct sap_device *dev = &sap_dev;

	if (!READ_ONCE(dev->enable))
		return;

	memcpy_to_scp(dev->sram_start_virt, (const void *)dev->loader_virt,
		dev->region_info_copy.ap_loader_size);

	memcpy_to_scp(dev->region_info, (const void *)&dev->region_info_copy,
		sizeof(dev->region_info_copy));
}

void sap_init(void)
{
	struct sap_device *dev = &sap_dev;

	dev->enable = false;
	dev->secure_dump_size = 0;
	dev->coredump.buf = NULL;
	dev->coredump.size = 0;
	if (platform_driver_register(&mtk_sap_device))
		pr_err("[SCP] scp probe fail\n");
}
