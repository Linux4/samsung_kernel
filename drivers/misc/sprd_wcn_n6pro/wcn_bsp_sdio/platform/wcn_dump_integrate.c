/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include "bufring.h"
#include "wcn_glb.h"
#include "wcn_glb_reg.h"
#include "wcn_log.h"
#include "wcn_misc.h"
#include "mdbg_type.h"
#include "../include/wcn_dbg.h"

/* units is ms, 2500ms */
#define WCN_DUMP_TIMEOUT 2500

/* use for umw2631_integrate only */
#define WCN_AON_ADDR_OFFSET 0x11000000
#define WCN_DUMP_ADDR_OFFSET 0x00000004

/* magic number, not change it */
#define WCN_DUMP_VERSION_NAME "WCN_DUMP_HEAD__"
#define WCN_DUMP_ALIGN(x) (((x) + 3) & ~3)
/* used for HEAD, so all dump mem in this array.
 * if new member added, please modify the macor XXX_START XXX_end above.
 */
struct wcn_dump_mem_reg {
	/* some CP regs can't dump */
	bool do_dump;
	u32 addr;
	/* 4 btyes align */
	u32 len;
};

struct wcn_dump_section_info {
	/* cp load start addr */
	__le32 start;
	/* cp load end addr */
	__le32 end;
	/* load from file offset */
	__le32 off;
	__le32 reserv;
} __packed;

struct wcn_dump_head_info {
	/* WCN_DUMP_VERSION_NAME */
	u8 version[16];
	/* WCN_DUMP_VERSION_SUB_NAME */
	u8 sub_version[16];
	/* numbers of wcn_dump_section_info */
	__le32 n_sec;
	/* used to check if dump is full */
	__le32 file_size;
	u8 reserv[8];
	struct wcn_dump_section_info section[0];
} __packed;

 #ifdef CONFIG_SC2342_I
static struct wcn_dump_mem_reg s_wcn_dump_regs[] = {
	/* share mem */
	{1, 0, 0x300000},
	/* iram mem */
	{1, 0x10000000, 0x8000}, /* wcn iram */
	{1, 0x18004000, 0x4000}, /* gnss iram */
	/* ap regs */
	{1, 0x402B00CC, 4}, /* PMU_SLEEP_CTRL */
	{1, 0x402B00D4, 4}, /* PMU_SLEEP_STATUS */
	{1, 0x402B0100, 4}, /* PMU_PD_WCN_SYS_CFG */
	{1, 0x402B0104, 4}, /* PMU_PD_WIFI_WRAP_CFG */
	{1, 0x402B0244, 4}, /* PMU_WCN_SYS_DSLP_ENA */
	{1, 0x402B0248, 4}, /* PMU_WIFI_WRAP_DSLP_ENA */
	{1, 0x402E057C, 4}, /* AON_APB_WCN_SYS_CFG2 */
	/* cp regs */
	{0, 0x40060000, 0x300}, /* BTWF_CTRL */
	{0, 0x60300000, 0x400}, /* BTWF_AHB_CTRL */
	{0, 0x40010000, 0x38},  /* BTWF_INTC */
	{0, 0x40020000, 0x10},  /* BTWF_SYSTEM_TIMER */
	{0, 0x40030000, 0x20},  /* BTWF_TIMER0 */
	{0, 0x40030020, 0x20},  /* BTWF_TIMER1 */
	{0, 0x40030040, 0x20},  /* BTWF_TIMER2 */
	{0, 0x40040000, 0x24},  /* BTWF_WATCHDOG */
	{0, 0xd0010000, 0xb4},  /* COM_AHB_CTRL */
	{0, 0xd0020800, 0xc},   /* MANU_CLK_CTRL */
	{0, 0x70000000, 0x10000},  /* WIFI */
	{0, 0x400b0000, 0x850},    /* FM */
	{0, 0x60700000, 0x400},    /* BT_CMD */
	{0, 0x60740000, 0xa400}    /* BT */
};
#endif

#ifdef CONFIG_UMW2631_I
static struct wcn_dump_mem_reg s_wcn_dump_regs[] = {
	/* share mem */
	{1, 0, 0x800000},
	/* iram mem */
	{1, 0x40500000, 0x4000}, /* wcn iram 16k */
	{1, 0x40a50000, 0x8000}, /* gnss iram 32k */
	/* ap regs */
	// {1, 0x402B00CC, 4}, /* PMU_SLEEP_CTRL  */
	{1, 0x640203a8, 4}, /* PMU_PD_WCN_SYS_CFG */
	{1, 0x640203ac, 4}, /* PMU_PD_WIFI_WRAP_CFG */
	{1, 0x6402078c, 4}, /* PMU_WCN_SYS_DSLP_ENA */
	//{1, 0x402B0248, 4}, /* PMU_WIFI_WRAP_DSLP_ENA */
	{1, 0x64020818, 4}, /* PMU_FORCE_DEEP_SLEEP_CFG0 */
	{1, 0x64020860, 4}, /* PMU_SLEEP_STATUS */
	//{1, 0x64000000, 4}, /* AON_APB_WCN_SYS_CFG2  */
	/* cp regs */
	/* top cp AON regs */
	{1, 0x40880000, 0x13c}, /* AON_AHB */
	{1, 0x4080c000, 0x448}, /* AON_APB */
	{1, 0x40800000, 0x400}, /* AON_CLK */
	{0, 0x400f0000, 0x608},  /* WIFI_MAC_AON */
	{1, 0x40130000, 0x424}, /* BTWF_AHB */
	{1, 0x40088000, 0x2bc}, /* BTWF_APB */
	//{1, 0x40844000, 0x48}, /* PRE_DIV_CLK */
	//{0, 0x40060000, 0x300}, /* BTWF_CTRL */
	//{0, 0x60300000, 0x400}, /* BTWF_AHB_CTRL */
	{0, 0x40000000, 0x8000},  /* BTWF_INTC */
	//{0, 0x40010000, 0x8000},  /* BTWF_WATCHDOG */
	{0, 0x40018000, 0x8000},  /* BTWF_SYSTEM_TIMER */
	{0, 0x40020000, 0x8000},  /* BTWF_TIMER0 */
	//{0, 0x40028000, 0x8000},  /* EIC0 */
	{0, 0x40050000, 0x8000},  /* BTWF_TIMER1 */
	/* wifi */
	{1, 0x40300000, 0x18000}, /* WIFI_REG_MEM*/
	{0, 0x40804000, 0xd4},  /* WIFI_CLK_SWITCH_CONTROL */
	{0, 0x400a0000, 0x58},   /* WIFI_GLB */
	{0, 0x400b7000, 0x778},  /* WIFI_DFE */
	{0, 0x400b2000, 0xa90},  /* WIFI_PHY_RX11A */
	{0, 0x400b3000, 0xd8},  /* WIFI_PHY_11B */
	{0, 0x400b0000, 0x80c},  /* WIFI_PHY_TOP */
	{0, 0x400b1000, 0x158},  /* WIFI_PHY_TX_11A */
	{0, 0x400b4000, 0xa74},  /* WIFI_PHY_RFIF */
	{0, 0x400f6000, 0xa8},  /* WIFI_MAC_CE */
	{0, 0x400fe000, 0x84},  /* WIFI_MAC_DC_PD */
	{0, 0x400fc000, 0x914},  /* WIFI_MAC_MH_PD */
	{0, 0x400f8000, 0x2d58},  /* WIFI_MAC_PA_PD */
	{0, 0x400f1000, 0x3720},  /* WIFI_MAC_RTN */
	{0, 0x400d3000, 0x4c},  /* WIFI_SPI_MASTER_RF */

	/* bt/fm */
	{0, 0x40240000, 0x310},    /* BT_ACC_CONTROL */
	{0, 0x40243000, 0x7d0},    /* BT_AON_CONTROL */
	{0, 0x40244000, 0xf4},    /* BT_TIM_EXT */
	//{0, 0x4024f000, 0xcfc},    /* BT_MODEM_CONTROL */
	/* dump reg of merlion if merlion accessible:to be added */
};
#endif

static struct mdbg_ring_t	*mdev_ring;
static gnss_dump_callback gnss_dump_handle;

static int wcn_fill_dump_head_info(struct wcn_dump_mem_reg *mem_cfg, size_t cnt)
{
	unsigned int i, len, head_len;
	struct wcn_dump_mem_reg *mem;
	struct wcn_dump_head_info *head;
	struct wcn_dump_section_info *sec;

	head_len = sizeof(*head) + sizeof(*sec) * cnt;
	head = kzalloc(head_len, GFP_KERNEL);
	if (unlikely(!head)) {
		WCN_ERR("system has no mem for dump mem\n");
		return -1;
	}

	strncpy(head->version, WCN_DUMP_VERSION_NAME,
		strlen(WCN_DUMP_VERSION_NAME) + 1);
	strncpy(head->sub_version, WCN_DUMP_VERSION_SUB_NAME,
		strlen(WCN_DUMP_VERSION_SUB_NAME) + 1);
	head->n_sec = cpu_to_le32(cnt);
	len = head_len;
	for (i = 0; i < cnt; i++) {
		sec = head->section + i;
		mem = mem_cfg + i;
		sec->off = cpu_to_le32(WCN_DUMP_ALIGN(len));
		sec->start = cpu_to_le32(mem->addr);
		sec->end = cpu_to_le32(sec->start + mem->len - 1);
		len += mem->len;
		WCN_INFO("section[%d] [0x%x 0x%x 0x%x]\n",
			 i, le32_to_cpu(sec->start),
			 le32_to_cpu(sec->end), le32_to_cpu(sec->off));
	}
	head->file_size = cpu_to_le32(len + strlen(WCN_DUMP_END_STRING));

	mdbg_ring_write(mdev_ring, head, head_len);
	wake_up_log_wait();
	kfree(head);

	return 0;
}

static void mdbg_dump_str(char *str, int str_len)
{
	if (!str)
		return;

	mdbg_ring_write_timeout(mdev_ring, str, str_len, WCN_DUMP_TIMEOUT);
	wake_up_log_wait();
	WCN_INFO("dump str finish!");
}

static int mdbg_dump_ap_register_data(phys_addr_t addr, u32 len)
{
	u32 value = 0;
	u8 *ptr = NULL;

	ptr = (u8 *)&value;
	wcn_read_data_from_phy_addr(addr, &value, len);
	mdbg_ring_write_timeout(mdev_ring, ptr, len, WCN_DUMP_TIMEOUT);
	wake_up_log_wait();

	return 0;
}

static int mdbg_dump_cp_register_data(u32 addr, u32 len)
{
	struct regmap *regmap;
	u32 i;
	u32 count, trans_size;
	u32 cp_access_type;
	phys_addr_t phy_addr;
	u8 *buf = NULL;
	u8 *ptr = NULL;

	WCN_INFO("start dump cp register!addr:%x,len:%d", addr, len);
	if (unlikely(!mdbg_dev->ring_dev)) {
		WCN_ERR("ring_dev is NULL\n");
		return -1;
	}

	buf = kzalloc(len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6)
		cp_access_type = 1;
	else
		cp_access_type = 0;
	if (cp_access_type) {
		/* direct map */
		phy_addr =  addr + WCN_AON_ADDR_OFFSET;
		for (i = 0; i < len / 4; i++) {
			ptr = buf + i * 4;
			wcn_read_data_from_phy_addr(phy_addr, ptr, 4);
			phy_addr = phy_addr + WCN_DUMP_ADDR_OFFSET;
		}
	} else {
		/* aon funcdma tlb way */
		if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_SHARKL3)
			regmap = wcn_get_btwf_regmap(REGMAP_WCN_REG);
		else
			regmap = wcn_get_btwf_regmap(REGMAP_ANLG_WRAP_WCN);

		wcn_regmap_raw_write_bit(regmap, 0XFF4, addr);
		for (i = 0; i < len / 4; i++) {
			ptr = buf + i * 4;
			wcn_regmap_read(regmap, 0XFFC, (u32 *)ptr);
		}
	}
	count = 0;
	while (count < len) {
		trans_size = (len - count) > DUMP_PACKET_SIZE ?
			DUMP_PACKET_SIZE : (len - count);
		mdbg_ring_write_timeout(mdev_ring, buf + count,
					trans_size, WCN_DUMP_TIMEOUT);
		count += trans_size;
		wake_up_log_wait();
	}

	kfree(buf);
	WCN_INFO("dump cp register finish count %u\n", count);

	return count;
}

static void mdbg_dump_ap_register(struct wcn_dump_mem_reg *mem)
{
	u32 i;

	for (i = WCN_DUMP_AP_REGS_START; i <= WCN_DUMP_AP_REGS_END; i++) {
		mdbg_dump_ap_register_data(mem[i].addr, mem[i].len);
		WCN_INFO("dump ap reg section[%d] ok!\n", i);
	}
}

static void mdbg_dump_cp_aon_register(struct wcn_dump_mem_reg *mem)
{
	u32 i;
	int count;

	for (i = WCN_DUMP_CP2_REGS_START;
		 i <= (WCN_DUMP_CP2_REGS_START + 4); i++) {
		count = mdbg_dump_cp_register_data(mem[i].addr, mem[i].len);
		WCN_INFO("dump cp reg section[%d] %d ok!\n", i, count);
	}
}

static void mdbg_dump_cp_register(struct wcn_dump_mem_reg *mem)
{
	u32 i;
	int count;

	for (i = WCN_DUMP_CP2_REGS_START; i <= WCN_DUMP_CP2_REGS_END; i++) {
		count = mdbg_dump_cp_register_data(mem[i].addr, mem[i].len);
		WCN_INFO("dump cp reg section[%d] %d ok!\n", i, count);
	}
}

static void mdbg_dump_iram(struct wcn_dump_mem_reg *mem)
{
	u32 i;
	int count;

	for (i = WCN_DUMP_CP2_IRAM_START; i <= WCN_DUMP_CP2_IRAM_END; i++) {
		count = mdbg_dump_cp_register_data(mem[i].addr, mem[i].len);
		WCN_INFO("dump iram section[%d] %d ok!\n", i, count);
	}
}

static int mdbg_dump_share_memory(struct wcn_dump_mem_reg *mem)
{
	u32 count, len, trans_size;
	void *virt_addr;
	phys_addr_t base_addr;
	unsigned int cnt;
	unsigned long timeout;

	if (unlikely(!mdbg_dev->ring_dev)) {
		WCN_ERR("ring_dev is NULL\n");
		return -1;
	}
	len = mem[0].len;
	base_addr = wcn_get_btwf_base_addr();
	WCN_INFO("dump sharememory start!");
	WCN_INFO("ring->pbuff=%p, ring->end=%p.\n",
		 mdev_ring->pbuff, mdev_ring->end);
	virt_addr = wcn_mem_ram_vmap_nocache(base_addr, len, &cnt);
	if (!virt_addr) {
		WCN_ERR("wcn_mem_ram_vmap_nocache fail\n");
		return -1;
	}
	count = 0;
	/* 20s timeout */
	timeout = jiffies + msecs_to_jiffies(20000);
	while (count < len) {
		trans_size = (len - count) > DUMP_PACKET_SIZE ?
			DUMP_PACKET_SIZE : (len - count);
		/* copy data from ddr to ring buf  */

		mdbg_ring_write_timeout(mdev_ring, virt_addr + count,
					trans_size, WCN_DUMP_TIMEOUT);
		count += trans_size;
		wake_up_log_wait();
		if (time_after(jiffies, timeout)) {
			WCN_ERR("Dump share mem timeout:count:%u\n", count);
			break;
		}
	}
	wcn_mem_ram_unmap(virt_addr, cnt);
	WCN_INFO("share memory dump finish! total count %u\n", count);

	return 0;
}

void mdbg_dump_gnss_register(gnss_dump_callback callback_func, void *para)
{
	gnss_dump_handle = (gnss_dump_callback)callback_func;
	WCN_INFO("gnss_dump register success!\n");
}

void mdbg_dump_gnss_unregister(void)
{
	gnss_dump_handle = NULL;
}

u32 mdbg_check_wifi_mac_phy(struct wcn_device *wcn_dev)
{
	u32 need_dump_status = 0;
	u32 btwf_wrap_mac_mask = (0x2);
	u32 btwf_wrap_phy_mask = (0x4);

	wcn_regmap_read(wcn_dev->rmap[REGMAP_WCN_AON_APB],
					0x03b0, &need_dump_status);
	WCN_INFO("%s:0x03b0=0x%x\n", __func__, need_dump_status);

	return ((need_dump_status & btwf_wrap_mac_mask) &&
			(need_dump_status & btwf_wrap_phy_mask));
}

static int btwf_dump_mem(void)
{
	u32 cp2_status = 0;
	phys_addr_t sleep_addr;

	if (wcn_get_btwf_power_status() == WCN_POWER_STATUS_OFF) {
		WCN_INFO("wcn power status off:can not dump btwf!\n");
		return -1;
	}

	mdbg_send_atcmd("at+sleep_switch=0\r",
			strlen("at+sleep_switch=0\r"),
			WCN_ATCMD_KERNEL);
	msleep(500);
	sleep_addr = wcn_get_btwf_sleep_addr();
	wcn_read_data_from_phy_addr(sleep_addr, &cp2_status, sizeof(u32));
	mdev_ring = mdbg_dev->ring_dev->ring;
	mdbg_hold_cpu();
	msleep(100);
	mdbg_ring_reset(mdev_ring);
	mdbg_atcmd_clean();
	if (wcn_fill_dump_head_info(s_wcn_dump_regs,
				    ARRAY_SIZE(s_wcn_dump_regs)))
		return -1;
	mdbg_dump_share_memory(s_wcn_dump_regs);
	mdbg_dump_iram(s_wcn_dump_regs);
	mdbg_dump_ap_register(s_wcn_dump_regs);

	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6) {
		if (mdbg_check_wifi_mac_phy(s_wcn_device.btwf_device)) {
			mdbg_dump_cp_register(s_wcn_dump_regs);
			WCN_INFO("dump register ok!\n");
		} else {
			mdbg_dump_cp_aon_register(s_wcn_dump_regs);
			WCN_INFO("dump cp AON register ok!\n");
		}
	} else {
		if (cp2_status == WCN_CP2_STATUS_DUMP_REG) {
			mdbg_dump_cp_register(s_wcn_dump_regs);
			WCN_INFO("dump register ok!\n");
		}
	}

	mdbg_dump_str(WCN_DUMP_END_STRING, strlen(WCN_DUMP_END_STRING));

	return 0;
}

void mdbg_dump_mem(void)
{
	/* dump gnss */
	if (gnss_dump_handle) {
		WCN_INFO("need dump gnss\n");
		gnss_dump_handle();
	}

	/* dump btwf */
	btwf_dump_mem();
}

int dump_arm_reg(void)
{
	mdbg_hold_cpu();

	return 0;
}

static int mdbg_snap_shoot_iram_data(void *buf, u32 addr, u32 len)
{
	struct regmap *regmap;
	u32 i;
	u32 cp_access_type;
	phys_addr_t phy_addr;
	u8 *ptr = NULL;

	WCN_INFO("start snap_shoot iram data!addr:%x,len:%d", addr, len);
	if (marlin_get_module_status() == 0) {
		WCN_ERR("module status off:can not get iram data!\n");
		return -1;
	}

	if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_QOGIRL6)
		cp_access_type = 1;
	else
		cp_access_type = 0;
	if (cp_access_type) {
		/* direct map */
		phy_addr =  addr + WCN_AON_ADDR_OFFSET;
		wcn_read_data_from_phy_addr(phy_addr, ptr, len);
	} else {
		/* aon funcdma tlb way */
		if (wcn_platform_chip_type() == WCN_PLATFORM_TYPE_SHARKL3)
			regmap = wcn_get_btwf_regmap(REGMAP_WCN_REG);
		else
			regmap = wcn_get_btwf_regmap(REGMAP_ANLG_WRAP_WCN);
		wcn_regmap_raw_write_bit(regmap, 0XFF4, addr);
		for (i = 0; i < len / 4; i++) {
			ptr = buf + i * 4;
			wcn_regmap_read(regmap, 0XFFC, (u32 *)ptr);
		}
	}
	WCN_INFO("snap_shoot iram data success\n");

	return 0;
}

int mdbg_snap_shoot_iram(void *buf)
{
	u32 ret;

	ret = mdbg_snap_shoot_iram_data(buf,
			0x18000000, 1024 * 32);

	return ret;
}
