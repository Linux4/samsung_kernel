/*
 * Copyright (C) 2017 Spreadtrum Communications Inc.
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <linux/file.h>
#include <linux/fs.h>
#ifdef CONFIG_WCN_INTEG
#include "gnss.h"
#endif
#include <linux/kthread.h>
#include <linux/printk.h>
#include <linux/sipc.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/unistd.h>
#include <linux/wait.h>
#include <misc/marlin_platform.h>
#include <misc/wcn_bus.h>

#include "wcn_glb.h"
#include "gnss_common.h"
#include "gnss_dump.h"
#include "mdbg_type.h"
#include "../../include/wcn_glb_reg.h"

#define GNSSDUMP_INFO(format, arg...) pr_info("gnss_dump: " format, ## arg)
#define GNSSDUMP_ERR(format, arg...) pr_err("gnss_dump: " format, ## arg)

static struct file *gnss_dump_file;
static	loff_t pos;
#define GNSS_MEMDUMP_PATH			"/data/vendor/gnss/gnssdump.mem"
/* use for umw2631_integrate only */
#define GNSS_DUMP_ADDR_OFFSET 0x00000004

#ifndef CONFIG_WCN_INTEG
struct gnss_mem_dump {
	uint address;
	uint length;
};

/* dump cp firmware firstly, wait for next adding */
static struct gnss_mem_dump gnss_marlin3_dump[] = {
	{GNSS_CP_START_ADDR, GNSS_FIRMWARE_MAX_SIZE}, /* gnss firmware code */
	{GNSS_DRAM_ADDR, GNSS_DRAM_SIZE}, /* gnss dram */
	{GNSS_TE_MEM, GNSS_TE_MEM_SIZE}, /* gnss te mem */
	{GNSS_BASE_AON_APB, GNSS_BASE_AON_APB_SIZE}, /* aon apb */
	{CTL_BASE_AON_CLOCK, CTL_BASE_AON_CLOCK_SIZE}, /* aon clock */
};
#else
struct regmap_dump {
	int regmap_type;
	uint reg;
};
struct cp_reg_dump {
	u32 addr;
	u32 len;
};

/* for sharkle ap reg dump, this order format can't change, just do it */
static struct regmap_dump gnss_sharkle_ap_reg[] = {
	{REGMAP_PMU_APB, 0x00cc}, /* REG_PMU_APB_SLEEP_CTRL */
	{REGMAP_PMU_APB, 0x00d4}, /* REG_PMU_APB_SLEEP_STATUS */
	{REGMAP_AON_APB, 0x057c}, /* REG_AON_APB_WCN_SYS_CFG2 */
	{REGMAP_AON_APB, 0x0578}, /* REG_AON_APB_WCN_SYS_CFG1 */
	{REGMAP_TYPE_NR, (AON_CLK_CORE + CGM_WCN_SHARKLE_CFG)}, /* clk */
	{REGMAP_PMU_APB, 0x0100}, /* REG_PMU_APB_PD_WCN_SYS_CFG */
	{REGMAP_PMU_APB, 0x0104}, /* REG_PMU_APB_PD_WIFI_WRAP_CFG */
	{REGMAP_PMU_APB, 0x0108}, /* REG_PMU_APB_PD_GNSS_WRAP_CFG */
};

/* for pike2 ap reg dump, this order format can't change, just do it */
static struct regmap_dump gnss_pike2_ap_reg[] = {
	{REGMAP_PMU_APB, 0x00cc}, /* REG_PMU_APB_SLEEP_CTRL */
	{REGMAP_PMU_APB, 0x00d4}, /* REG_PMU_APB_SLEEP_STATUS */
	{REGMAP_PMU_APB, 0x0338}, /* REG_PMU_APB_WCN_SYS_CFG_STATUS */
	{REGMAP_AON_APB, 0x00d8}, /* REG_AON_APB_WCN_CONFIG0 */
	{REGMAP_TYPE_NR, (AON_CLK_CORE + CGM_WCN_PIKE2_CFG)}, /* clk */
	{REGMAP_PMU_APB, 0x0050}, /* REG_PMU_APB_PD_WCN_TOP_CFG */
	{REGMAP_PMU_APB, 0x0054}, /* REG_PMU_APB_PD_WCN_WIFI_CFG */
	{REGMAP_PMU_APB, 0x0058}, /* REG_PMU_APB_PD_WCN_GNSS_CFG */
};

/* for sharkl3 ap reg dump, this order format can't change, just do it */
static struct regmap_dump gnss_sharkl3_ap_reg[] = {
	{REGMAP_PMU_APB, 0x00cc}, /* REG_PMU_APB_SLEEP_CTRL */
	{REGMAP_PMU_APB, 0x00d4}, /* REG_PMU_APB_SLEEP_STATUS */
	{REGMAP_AON_APB, 0x057c}, /* REG_AON_APB_WCN_SYS_CFG2 */
	{REGMAP_AON_APB, 0x0578}, /* REG_AON_APB_WCN_SYS_CFG1 */
	{REGMAP_TYPE_NR, (AON_CLK_CORE + CGM_WCN_SHARKLE_CFG)}, /* clk */
	{REGMAP_PMU_APB, 0x0100}, /* REG_PMU_APB_PD_WCN_SYS_CFG */
	{REGMAP_PMU_APB, 0x0104}, /* REG_PMU_APB_PD_WIFI_WRAP_CFG */
	{REGMAP_PMU_APB, 0x0108}, /* REG_PMU_APB_PD_GNSS_WRAP_CFG */
};

/* for sharkl6 cp reg dump */
static struct cp_reg_dump cp_reg[] = {
	{REG_AON_WCN_GNSS_CLK, AON_WCN_GNSS_CLK_LEN},
	{REG_AON_APB_PERI, AON_APB_PERI_LEN},
	{REG_AON_AHB_SYS, AON_AHB_SYS_LEN},
	{REG_AON_CONTROL, AON_CONTROL_LEN},
};

#ifdef CONFIG_SC2342_I
#define GNSS_DUMP_REG_NUMBER 8
#endif
#ifdef CONFIG_UMW2631_I
#define GNSS_DUMP_REG_NUMBER 4
#endif
static char gnss_dump_level; /* 0: default, all, 1: only data, pmu, aon */

#endif

static int gnss_creat_gnss_dump_file(void)
{
	gnss_dump_file = filp_open(GNSS_MEMDUMP_PATH,
		O_RDWR | O_CREAT | O_TRUNC, 0666);
	GNSSDUMP_ERR("gnss_creat_gnss_dump_file entry\n");
	if (IS_ERR(gnss_dump_file)) {
		GNSSDUMP_ERR("%s error is %p\n",
			__func__, gnss_dump_file);
		return -1;
	}
//	if (sys_chmod(GNSS_MEMDUMP_PATH, 0666) != 0)
//		GNSSDUMP_ERR("%s chmod	error\n", __func__);

	return 0;
}

#ifdef CONFIG_WCN_INTEG
static void gnss_write_data_to_phy_addr(phys_addr_t phy_addr,
					      void *src_data, u32 size)
{
	void *virt_addr;

	GNSSDUMP_ERR("gnss_write_data_to_phy_addr entry\n");
	virt_addr = shmem_ram_vmap_nocache(phy_addr, size);
	if (virt_addr) {
		memcpy(virt_addr, src_data, size);
		shmem_ram_unmap(virt_addr);
	} else
		GNSSDUMP_ERR("%s shmem_ram_vmap_nocache fail\n", __func__);
}

static void gnss_read_data_from_phy_addr(phys_addr_t phy_addr,
					  void *tar_data, u32 size)
{
	void *virt_addr;

	GNSSDUMP_ERR("gnss_read_data_from_phy_addr\n");
	virt_addr = shmem_ram_vmap_nocache(phy_addr, size);
	if (virt_addr) {
		memcpy(tar_data, virt_addr, size);
		shmem_ram_unmap(virt_addr);
	} else
		GNSSDUMP_ERR("%s shmem_ram_vmap_nocache fail\n", __func__);
}

static void gnss_soft_reset_release_cpu(u32 type)
{
	struct regmap *regmap;
	phys_addr_t base_addr;
	u32 value = 0;
	u32 value_tmp = 0;
	u32 platform_type = wcn_platform_chip_type();

	if (platform_type == WCN_PLATFORM_TYPE_QOGIRL6) {
		base_addr = MCU_AP_RST_ADDR + GNSS_AP_ACCESS_CP_OFFSET;
		if (type == GNSS_CPU_RESET) {
			/* reset gnss cpu */
			gnss_read_data_from_phy_addr(base_addr,
				(void *)&value_tmp, 4);
			GNSSDUMP_INFO("before rst val=%x\n", value_tmp);
			/* BIT0:rst set */
			value_tmp |= 1;
			gnss_write_data_to_phy_addr(base_addr,
				(void *)&value_tmp, 4);
			GNSSDUMP_INFO("rst val=%x\n", value_tmp);
		} else if (type == GNSS_CPU_RESET_RELEASE) {
			/* release gnss cpu */
			gnss_read_data_from_phy_addr(base_addr,
				(void *)&value_tmp, 4);
			GNSSDUMP_INFO("before rls val=%x\n", value_tmp);
			/* BIT0:rst clear */
			value_tmp &= ~(1);
			gnss_write_data_to_phy_addr(base_addr,
				(void *)&value_tmp, 4);
			GNSSDUMP_INFO("rls val=%x\n", value_tmp);
		}
		return;
	}

	if (platform_type == WCN_PLATFORM_TYPE_SHARKL3)
		regmap = wcn_get_gnss_regmap(REGMAP_WCN_REG);
	else
		regmap = wcn_get_gnss_regmap(REGMAP_ANLG_WRAP_WCN);
	if (type == GNSS_CPU_RESET) {
		/* reset gnss cm4 */
		wcn_regmap_read(regmap, 0X20, &value);
		value |= 1 << 2;
		wcn_regmap_raw_write_bit(regmap, 0X20, value);

		wcn_regmap_read(regmap, 0X24, &value);
		value |= 1 << 3;
		wcn_regmap_raw_write_bit(regmap, 0X24, value);
	} else if (type == GNSS_CPU_RESET_RELEASE) {
		value = 0;
		/* release gnss cm4 */
		wcn_regmap_raw_write_bit(regmap, 0X20, value);
		wcn_regmap_raw_write_bit(regmap, 0X24, value);
	}
}
static void gnss_hold_cpu(void)
{
	u32 value;
	phys_addr_t base_addr;
	phys_addr_t ph_addr;
	int i = 0;

	/* reset cpu */
	gnss_soft_reset_release_cpu(GNSS_CPU_RESET);
	/* set cache flag */
	value = GNSS_CACHE_FLAG_VALUE;
	base_addr = wcn_get_gnss_base_addr();
	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6)
		ph_addr = base_addr + GNSS_CACHE_FLAG_ADDR_L6;
	else
		ph_addr = base_addr + GNSS_CACHE_FLAG_ADDR;
	GNSSDUMP_INFO("val=%x bs=%x ph=%x\n", value, base_addr, ph_addr);
	gnss_write_data_to_phy_addr(ph_addr, (void *)&value, 4);
	/* release cpu */
	gnss_soft_reset_release_cpu(GNSS_CPU_RESET_RELEASE);

	while (i < 3) {
		gnss_read_data_from_phy_addr(ph_addr, (void *)&value, 4);
		if (value == GNSS_CACHE_END_VALUE)
			break;
		i++;
		msleep(50);
	}
	if (value != GNSS_CACHE_END_VALUE)
		GNSSDUMP_ERR("%s gnss cache failed value %d\n", __func__,
			value);
	msleep(200);
}

static int gnss_dump_cp_register_data(u32 addr, u32 len)
{
	struct regmap *map;
	u32 i;
	u32 cp_access_type;
	u32 chip_tp = wcn_platform_chip_type();
	phys_addr_t phy_addr;
	u8 *buf = NULL;
	u8 *pt  = NULL;
	long int ret;
	void  *iram_buffer = NULL;
	mm_segment_t fs;

	GNSSDUMP_INFO(" start dump cp register!addr:%x,len:%d\n", addr, len);
	buf = kzalloc(len, GFP_KERNEL);
	if (!buf) {
		GNSSDUMP_ERR("%s kzalloc buf error\n", __func__);
		return -ENOMEM;
	}

	if (IS_ERR(gnss_dump_file)) {
		gnss_dump_file = filp_open(GNSS_MEMDUMP_PATH,
			O_RDWR | O_CREAT | O_APPEND, 0666);
		if (IS_ERR(gnss_dump_file)) {
			GNSSDUMP_ERR("%s  open file mem error\n", __func__);
			kfree(buf);
			return PTR_ERR(gnss_dump_file);
		}
	}

	iram_buffer = vmalloc(len);
	if (!iram_buffer) {
		GNSSDUMP_ERR("%s vmalloc iram_buffer error\n", __func__);
		kfree(buf);
		return -ENOMEM;
	}
	memset(iram_buffer, 0, len);

	if (chip_tp == WCN_PLATFORM_TYPE_QOGIRL6)
		cp_access_type = 1;
	else
		cp_access_type = 0;

	if (gnss_dump_level == 0) {
		if (cp_access_type) {
			/* direct map */
			phy_addr =  addr + GNSS_AP_ACCESS_CP_OFFSET;
			for (i = 0; i < len / 4; i++) {
				pt = buf + i * 4;
				wcn_read_data_from_phy_addr(phy_addr, pt, 4);
				phy_addr = phy_addr + GNSS_DUMP_ADDR_OFFSET;
			}
		} else {
			/* aon funcdma tlb way */
			if (chip_tp == WCN_PLATFORM_TYPE_SHARKL3)
				map = wcn_get_gnss_regmap(REGMAP_WCN_REG);
			else
				map = wcn_get_gnss_regmap(REGMAP_ANLG_WRAP_WCN);
			wcn_regmap_raw_write_bit(map,
						 ANLG_WCN_WRITE_ADDR, addr);
			for (i = 0; i < len / 4; i++) {
				pt = buf + i * 4;
				wcn_regmap_read(map,
						ANLG_WCN_READ_ADDR, (u32 *)pt);
			}
		}
		memcpy(iram_buffer, buf, len);
	}
	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = gnss_dump_file->f_pos;
	ret = vfs_write(gnss_dump_file,
			(__force const char __user *)iram_buffer,
			len, &pos);
	gnss_dump_file->f_pos = pos;
	kfree(buf);
	vfree(iram_buffer);
	set_fs(fs);
	if (ret != len) {
		GNSSDUMP_ERR("gnss_dump_cp_register_data failed  size is %ld\n",
			ret);
		return -1;
	}
	GNSSDUMP_INFO("gnss_dump_cp_register_data finish  size is  %ld\n",
		ret);

	return ret;
}

static int gnss_dump_ap_register(void)
{
	struct regmap *regmap;
	u32 value[GNSS_DUMP_REG_NUMBER + 1] = {0}; /* [0]board+ [..]reg */
	u32 i = 0;
	mm_segment_t fs;
	u32 len = 0;
	u8 *ptr = NULL;
	int ret;
	void  *apreg_buffer = NULL;
	struct regmap_dump *gnss_ap_reg = NULL;

	GNSSDUMP_INFO("%s ap reg data\n", __func__);
	if (IS_ERR(gnss_dump_file)) {
		gnss_dump_file = filp_open(GNSS_MEMDUMP_PATH,
			O_RDWR | O_CREAT | O_APPEND, 0666);
		if (IS_ERR(gnss_dump_file)) {
			GNSSDUMP_ERR("%s open file mem error\n", __func__);
			return -1;
		}
	}

	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_SHARKLE) {
		gnss_ap_reg = gnss_sharkle_ap_reg;
		len = (GNSS_DUMP_REG_NUMBER + 1) * sizeof(u32);
	} else if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_PIKE2) {
		gnss_ap_reg = gnss_pike2_ap_reg;
		len = (GNSS_DUMP_REG_NUMBER + 1) * sizeof(u32);
	} else if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_SHARKL3) {
		gnss_ap_reg = gnss_sharkl3_ap_reg;
		len = (GNSS_DUMP_REG_NUMBER + 1) * sizeof(u32);
	}

	apreg_buffer = vmalloc(len);
	if (!apreg_buffer)
		return -2;

	ptr = (u8 *)&value[0];
	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_SHARKLE)
		value[0] = 0xF1;
	else if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_PIKE2)
		value[0] = 0xF2;
	else if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_SHARKL3)
		value[0] = 0xF3;
	else
		value[0] = 0xF4;

	for (i = 0; i < GNSS_DUMP_REG_NUMBER; i++) {
		if ((gnss_ap_reg + i)->regmap_type == REGMAP_TYPE_NR) {
			gnss_read_data_from_phy_addr((gnss_ap_reg + i)->reg,
				&value[i+1], 4);
			GNSSDUMP_INFO("%s ap reg[0x%x] data[0x%x]\n", __func__,
				      (gnss_ap_reg + i)->reg, value[i+1]);
		} else {
			regmap =
			wcn_get_gnss_regmap((gnss_ap_reg + i)->regmap_type);
			wcn_regmap_read(regmap, (gnss_ap_reg + i)->reg,
				&value[i+1]);
		}
	}
	memset(apreg_buffer, 0, len);
	memcpy(apreg_buffer, ptr, len);
	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = gnss_dump_file->f_pos;
	ret = vfs_write(gnss_dump_file,
			(__force const char __user *)apreg_buffer,
			len, &pos);
	gnss_dump_file->f_pos = pos;
	vfree(apreg_buffer);
	set_fs(fs);
	if (ret != len)
		GNSSDUMP_ERR("%s not write completely,ret is 0x%x\n", __func__,
			ret);

	return 0;
}

static void gnss_dump_cp_register(void)
{
	u32 count;
	int i;

	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6) {
		for (i = 0; i < CP_REG_NUM; i++) {
			count = gnss_dump_cp_register_data(cp_reg[i].addr,
					 cp_reg[i].len);
			GNSSDUMP_INFO("dump cp_reg%d %u\n", i, count);
		}
		return;
	}
	count = gnss_dump_cp_register_data(DUMP_REG_GNSS_APB_CTRL_ADDR,
			DUMP_REG_GNSS_APB_CTRL_LEN);
	GNSSDUMP_INFO("gnss dump gnss_apb_ctrl_reg %u ok!\n", count);

	count = gnss_dump_cp_register_data(DUMP_REG_GNSS_AHB_CTRL_ADDR,
			DUMP_REG_GNSS_AHB_CTRL_LEN);
	GNSSDUMP_INFO("gnss dump manu_clk_ctrl_reg %u ok!\n", count);

	count = gnss_dump_cp_register_data(DUMP_COM_SYS_CTRL_ADDR,
			DUMP_COM_SYS_CTRL_LEN);
	GNSSDUMP_INFO("gnss dump com_sys_ctrl_reg %u ok!\n", count);

	count = gnss_dump_cp_register_data(DUMP_WCN_CP_CLK_CORE_ADDR,
			DUMP_WCN_CP_CLK_LEN);
	GNSSDUMP_INFO("gnss dump manu_clk_ctrl_reg %u ok!\n", count);
}

static void gnss_dump_register(void)
{
	gnss_dump_ap_register();
	gnss_dump_cp_register();
	GNSSDUMP_INFO("gnss dump register ok!\n");
}

static void gnss_dump_iram(void)
{
	u32 count;
#ifdef CONFIG_UMW2631_I
	u32 count_pchannel;
#endif
	count = gnss_dump_cp_register_data(GNSS_DUMP_IRAM_START_ADDR,
			GNSS_CP_IRAM_DATA_NUM * 4);
	GNSSDUMP_INFO("gnss dump iram %u ok!\n", count);
#ifdef CONFIG_UMW2631_I
	count_pchannel = gnss_dump_cp_register_data(
			GNSS_DUMP_IRAM_START_ADDR_PCHANNEL,
			GNSS_PCHANNEL_IRAM_DATA_NUM * 4);
	GNSSDUMP_INFO("gnss pchannel dump iram %u ok!\n", count_pchannel);
#endif
}

static int gnss_dump_share_memory(u32 len)
{
	void *virt_addr;
	phys_addr_t base_addr;
	long int ret;
	mm_segment_t fs;
	void  *ddr_buffer = NULL;

	if (len == 0)
		return -1;

	fs = get_fs();
	set_fs(KERNEL_DS);
	base_addr = wcn_get_gnss_base_addr();
	GNSSDUMP_ERR(" %s base_addr is 0x%x\n", __func__, base_addr);
	virt_addr = shmem_ram_vmap_nocache(base_addr, len);
	if (!virt_addr) {
		GNSSDUMP_ERR(" %s shmem_ram_vmap_nocache fail\n", __func__);
		return -1;
	}

	if (IS_ERR(gnss_dump_file)) {
		gnss_dump_file = filp_open(GNSS_MEMDUMP_PATH,
			O_RDWR | O_CREAT | O_APPEND, 0666);
		if (IS_ERR(gnss_dump_file)) {
			GNSSDUMP_ERR("%s open file mem error\n", __func__);
			return PTR_ERR(gnss_dump_file);
		}
	}

	ddr_buffer = vmalloc(len);
	if (!ddr_buffer) {
		GNSSDUMP_ERR(" %s vmalloc ddr_buffer fail\n", __func__);
		return -1;
	}
	memset(ddr_buffer, 0, len);
	memcpy(ddr_buffer, virt_addr, len);
	pos = gnss_dump_file->f_pos;
	ret = vfs_write(gnss_dump_file,	(__force const char __user *)ddr_buffer,
			len, &pos);
	gnss_dump_file->f_pos = pos;
	shmem_ram_unmap(virt_addr);
	vfree(ddr_buffer);
	if (ret != len) {
		GNSSDUMP_ERR("%s dump ddr error,data len is %ld\n", __func__,
			ret);
		return -1;
	}
	GNSSDUMP_INFO("gnss dump share memory  size = %ld\n", ret);
#ifdef CONFIG_UMW2631_I
	GNSSDUMP_ERR("%s dump sipc buffer start ", __func__);
	base_addr = GNSS_DUMP_IRAM_START_ADDR_SIPC;
	virt_addr = shmem_ram_vmap_nocache(base_addr, SIPC_BUFFER_DATA_NUM);
	if (!virt_addr) {
		GNSSDUMP_ERR(" %s shmem for sipc buffer is fail\n", __func__);
		return -1;
	}

	if (IS_ERR(gnss_dump_file)) {
		gnss_dump_file = filp_open(GNSS_MEMDUMP_PATH,
			O_RDWR | O_CREAT | O_APPEND, 0666);
		if (IS_ERR(gnss_dump_file)) {
			GNSSDUMP_ERR("%s open file mem error\n", __func__);
			return PTR_ERR(gnss_dump_file);
		}
	}

	ddr_buffer = vmalloc(SIPC_BUFFER_DATA_NUM);
	if (!ddr_buffer) {
		GNSSDUMP_ERR(" %s vmalloc ddr_buffer fail\n", __func__);
		return -1;
	}
	memset(ddr_buffer, 0, SIPC_BUFFER_DATA_NUM);
	memcpy(ddr_buffer, virt_addr, SIPC_BUFFER_DATA_NUM);
	pos = gnss_dump_file->f_pos;
	ret = vfs_write(gnss_dump_file,
			(__force const char __user *)ddr_buffer,
			SIPC_BUFFER_DATA_NUM, &pos);
	gnss_dump_file->f_pos = pos;
	shmem_ram_unmap(virt_addr);
	set_fs(fs);
	vfree(ddr_buffer);
	if (ret != SIPC_BUFFER_DATA_NUM) {
		GNSSDUMP_ERR("%s dump ddr error,sipc data is %ld\n", __func__,
			     ret);
		return -1;
	}

	GNSSDUMP_INFO("gnss dump sipc buffer size = %ld\n", ret);
#endif
	return 0;
}

static int gnss_integrated_dump_mem(void)
{
	int ret = 0;

	GNSSDUMP_INFO("gnss_dump_mem entry\n");
	gnss_hold_cpu();
	ret = gnss_creat_gnss_dump_file();
	if (ret == -1) {
		GNSSDUMP_ERR("%s create mem dump file  error\n", __func__);
		return -1;
	}
	ret = gnss_dump_share_memory(GNSS_SHARE_MEMORY_SIZE);
	gnss_dump_iram();
	gnss_dump_register();
	if (gnss_dump_file != NULL)
		filp_close(gnss_dump_file, NULL);

	return ret;
}

#else
static int gnss_ext_hold_cpu(void)
{
	uint temp = 0;
	int ret = 0;

	GNSSDUMP_INFO("%s entry\n", __func__);
	temp = BIT_GNSS_APB_MCU_AP_RST_SOFT;
	ret = sprdwcn_bus_reg_write(REG_GNSS_APB_MCU_AP_RST + GNSS_SET_OFFSET,
		&temp, 4);
	if (ret < 0) {
		GNSSDUMP_ERR("%s write reset reg error:%d\n", __func__, ret);
		return ret;
	}
	temp = GNSS_ARCH_EB_REG_BYPASS;
	ret = sprdwcn_bus_reg_write(GNSS_ARCH_EB_REG + GNSS_SET_OFFSET,
				    &temp, 4);
	if (ret < 0)
		GNSSDUMP_ERR("%s write bypass reg error:%d\n", __func__, ret);

	return ret;
}

static int gnss_ext_dump_data(unsigned int start_addr, int len)
{
	u8 *buf = NULL;
	int ret = 0, count = 0, trans = 0;
	mm_segment_t fs;

	GNSSDUMP_INFO("%s, addr:%x,len:%d\n", __func__, start_addr, len);
	buf = kzalloc(DUMP_PACKET_SIZE, GFP_KERNEL);
	if (!buf) {
		GNSSDUMP_ERR("%s kzalloc buf error\n", __func__);
		return -ENOMEM;
	}

	if (IS_ERR(gnss_dump_file)) {
		gnss_dump_file = filp_open(GNSS_MEMDUMP_PATH,
			O_RDWR | O_CREAT | O_APPEND, 0666);
		if (IS_ERR(gnss_dump_file)) {
			GNSSDUMP_ERR("%s  open file mem error\n", __func__);
			kfree(buf);
			return PTR_ERR(gnss_dump_file);
		}
	}
	fs = get_fs();
	set_fs(KERNEL_DS);
	while (count < len) {
		trans = (len - count) > DUMP_PACKET_SIZE ?
				 DUMP_PACKET_SIZE : (len - count);
		ret = sprdwcn_bus_direct_read(start_addr + count, buf, trans);
		if (ret < 0) {
			GNSSDUMP_ERR("%s read error:%d\n", __func__, ret);
			goto dump_data_done;
		}
		count += trans;
		pos = gnss_dump_file->f_pos;
		ret = vfs_write(gnss_dump_file,
				(__force const char __user *)buf,
				trans, &pos);
		gnss_dump_file->f_pos = pos;
		if (ret != trans) {
			GNSSDUMP_ERR("%s failed size is %d, ret %d\n", __func__,
				      len, ret);
			goto dump_data_done;
		}
	}
	GNSSDUMP_INFO("%s finish %d\n", __func__, len);
	ret = 0;

dump_data_done:
	kfree(buf);
	set_fs(fs);
	return ret;
}

static int gnss_ext_dump_mem(void)
{
	int ret = 0;
	int i = 0;

	GNSSDUMP_INFO("%s entry\n", __func__);
	gnss_ext_hold_cpu();
	ret = gnss_creat_gnss_dump_file();
	if (ret == -1) {
		GNSSDUMP_ERR("%s create file error\n", __func__);
		return -1;
	}
	for (i = 0; i < ARRAY_SIZE(gnss_marlin3_dump); i++)
		if (gnss_ext_dump_data(gnss_marlin3_dump[i].address,
			gnss_marlin3_dump[i].length)) {
			GNSSDUMP_ERR("%s dumpdata i %d error\n", __func__, i);
			break;
		}
	if (gnss_dump_file != NULL)
		filp_close(gnss_dump_file, NULL);
	return ret;
}
#endif

int gnss_dump_mem(char flag)
{
#ifdef CONFIG_WCN_INTEG
	gnss_dump_level = flag;
	return gnss_integrated_dump_mem();
#else
	return gnss_ext_dump_mem();
#endif
}
