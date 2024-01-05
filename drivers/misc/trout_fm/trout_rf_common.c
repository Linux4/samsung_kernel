#include <linux/miscdevice.h>
#include <linux/sysfs.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/ioctl.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>

#include "trout_fm_ctrl.h"
#include "trout_rf_common.h"

#ifdef CONFIG_RF_SHARK
#include "trout_rf_shark.h"
#endif

#ifdef CONFIG_RF_2829
#include "trout_rf_2829.h"
#endif

#ifdef CONFIG_RF_55
#include "trout_rf_55.h"
#endif

int trout_fm_open_clock(void)
{
	unsigned int reg_data;
	unsigned int com_reg = 0x200E;
	unsigned int com_data = 0x4E4D4143;	/* "NMAC" */

	/* open clock */
	READ_REG(SYS_REG_CLK_CTRL1, &reg_data);

	if (!(reg_data & BIT_8)) {
		reg_data |= BIT_8;
		WRITE_REG(SYS_REG_CLK_CTRL1, reg_data);
	}

	READ_REG(com_reg, &reg_data);
	TROUT_PRINT("ID: 0x%x", reg_data);
	if (reg_data != com_data) {
		TROUT_PRINT("Read ID error! It should be 0x%x", com_data);
		return -1;
	}

	return 0;
}

int trout_fm_reg_cfg(void)
{
	u8 i = 0;
	u32 fm_reg_agc_db_tbl[FM_REG_AGC_DB_TBL_CNT] = {
		15848, 12589, 10000, 7943, 6309,
		5011, 3981, 3162, 2511, 1995,
		1584, 1258, 1000, 794, 630,
		501, 398, 316, 251, 199,
		158, 125, 100, 79, 63,
		50, 39, 31, 25, 19,
		15, 12, 10, 7, 6,
		5, 3, 3, 2, 1, 1
	};

	if (write_fm_regs(fm_reg_init_des,
			  sizeof(fm_reg_init_des) /
			  sizeof(fm_reg_init_des[0])) < 0)
		return -1;

	WRITE_REG(FM_REG_AGC_TBL_CLK, 0x00000001);
	for (i = 0; i < FM_REG_AGC_DB_TBL_CNT; i++)
		WRITE_REG(FM_REG_AGC_DB_TBL_BEGIN + i, fm_reg_agc_db_tbl[i]);

	WRITE_REG(FM_REG_AGC_TBL_CLK, 0x00000000);

	return 0;
}

int trout_fm_rf_write(u32 addr, u32 write_data)
{
	u32 read_data;

	if (trout_fm_rf_spi_write(addr, write_data) == -1) {
		TROUT_PRINT("%02x write error!\n", addr);
		return -1;
	}

	if (addr < 0x0200) {
		if (trout_fm_rf_spi_read(addr, &read_data) == -1) {
			TROUT_PRINT("%02x read error!\n", addr);
			return -1;
		}
		if (write_data == read_data)
			return 0;

		TROUT_PRINT
		    ("trout_fm_rf_write failed! addr(0x%x): r(0x%x) != w(0x%x)",
		     addr, read_data, write_data);

		return -1;
	}

	return 0;
}

int trout_fm_rf_read(u32 addr, u32 *data)
{
	if (trout_fm_rf_spi_read(addr, data) == -1) {
		TROUT_PRINT("%02x read error!\n", addr);
		return -1;
	}

	return 0;
}

int trout_fm_iis_pin_cfg(void)
{
	unsigned int reg_data;

	READ_REG(SYS_REG_PAD_IISDO, &reg_data);
	reg_data |= 0x1;
	WRITE_REG(SYS_REG_PAD_IISDO, reg_data);

	READ_REG(SYS_REG_PAD_IISCLK, &reg_data);
	reg_data |= 0x1;
	WRITE_REG(SYS_REG_PAD_IISCLK, reg_data);

	READ_REG(SYS_REG_PAD_IISLRCK, &reg_data);
	reg_data |= 0x1;
	WRITE_REG(SYS_REG_PAD_IISLRCK, reg_data);

	return 0;
}

int trout_fm_pcm_pin_cfg(void)
{
	unsigned int reg_data;

	READ_REG(SYS_REG_PAD_IISDO, &reg_data);
	reg_data = (reg_data & 0xfffffff8);
	WRITE_REG(SYS_REG_PAD_IISDO, reg_data);

	READ_REG(SYS_REG_PAD_IISCLK, &reg_data);
	reg_data = (reg_data & 0xfffffff8);
	WRITE_REG(SYS_REG_PAD_IISCLK, reg_data);

	READ_REG(SYS_REG_PAD_IISLRCK, &reg_data);
	reg_data = (reg_data & 0xfffffff8);
	WRITE_REG(SYS_REG_PAD_IISLRCK, reg_data);

	return 0;
}


