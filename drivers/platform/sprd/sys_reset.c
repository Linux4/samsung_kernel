/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <soc/sprd/sys_reset.h>
#include <linux/string.h>
#include <asm/bitops.h>

void sprd_set_reboot_mode(const char *cmd)
{
	if(cmd)
		printk("sprd_set_reboot_mode:cmd=%s\n",cmd);
	if (cmd && !(strncmp(cmd, "recovery", 8))) {
		sci_adi_raw_write(ANA_RST_STATUS, HWRST_STATUS_RECOVERY);
	} else if (cmd && !strncmp(cmd, "alarm", 5)) {
		sci_adi_raw_write(ANA_RST_STATUS, HWRST_STATUS_ALARM);
	} else if (cmd && !strncmp(cmd, "fastsleep", 9)) {
		sci_adi_raw_write(ANA_RST_STATUS, HWRST_STATUS_SLEEP);
	} else if (cmd && !strncmp(cmd, "bootloader", 10)) {
		sci_adi_raw_write(ANA_RST_STATUS, HWRST_STATUS_FASTBOOT);
	} else if (cmd && !strncmp(cmd, "panic", 5)) {
		sci_adi_raw_write(ANA_RST_STATUS, HWRST_STATUS_PANIC);
	} else if (cmd && !strncmp(cmd, "special", 7)) {
		sci_adi_raw_write(ANA_RST_STATUS, HWRST_STATUS_SPECIAL);
	} else if (cmd && !strncmp(cmd, "cftreboot", 9)) {
		sci_adi_raw_write(ANA_RST_STATUS, HWRST_STATUS_CFTREBOOT);
	} else if (cmd && !strncmp(cmd, "autodloader", 11)) {
		sci_adi_raw_write(ANA_RST_STATUS, HWRST_STATUS_AUTODLOADER);
	} else if (cmd && !strncmp(cmd, "iqmode", 6)) {
		sci_adi_raw_write(ANA_RST_STATUS, HWRST_STATUS_IQMODE);
	} else if(cmd){
		sci_adi_raw_write(ANA_RST_STATUS, HWRST_STATUS_NORMAL);
	}else{
		sci_adi_raw_write(ANA_RST_STATUS, HWRST_STATUS_SPECIAL);
	}
}

void sprd_turnon_watchdog(unsigned int ms)
{
	uint32_t cnt;

	cnt = (ms * 1000) / WDG_CLK;

	/*enable interface clk*/
	sci_adi_set(ANA_AGEN, AGEN_WDG_EN);
	/*enable work clk*/
	sci_adi_set(ANA_RTC_CLK_EN, AGEN_RTC_WDG_EN);
	sci_adi_raw_write(WDG_LOCK, WDG_UNLOCK_KEY);
	sci_adi_set(WDG_CTRL, WDG_NEW_VER_EN);
	WDG_LOAD_TIMER_VALUE(cnt);
	sci_adi_set(WDG_CTRL, WDG_CNT_EN_BIT | WDG_RST_EN_BIT);
	sci_adi_raw_write(WDG_LOCK, (uint16_t) (~WDG_UNLOCK_KEY));
}

void sprd_turnoff_watchdog(void)
{
	sci_adi_raw_write(WDG_LOCK, WDG_UNLOCK_KEY);
	/*wdg counter stop*/
	sci_adi_clr(WDG_CTRL, WDG_CNT_EN_BIT);
	/*disable the reset mode*/
	sci_adi_clr(WDG_CTRL, WDG_RST_EN_BIT);
	sci_adi_raw_write(WDG_LOCK, (uint16_t) (~WDG_UNLOCK_KEY));
	/*stop the interface and work clk*/
	sci_adi_clr(ANA_AGEN, AGEN_WDG_EN);
	sci_adi_clr(ANA_RTC_CLK_EN, AGEN_RTC_WDG_EN);
}
