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
#include "trout_rf_2829.h"
#include "trout_rf_common.h"

static struct trout_fm_info_t trout_fm_info;

#ifndef RF_USE_ADI_MODE
#define RF_USE_SPI_MODE
#endif

void __trout_fm_get_status(void)
{
	READ_REG(FM_REG_SEEK_CNT, &trout_fm_info.seek_cnt);
	READ_REG(FM_REG_CHAN_FREQ_STS, &trout_fm_info.freq_seek);
	READ_REG(FM_REG_FREQ_OFF_STS, &trout_fm_info.freq_offset);
	READ_REG(FM_REG_RF_RSSI_STS, &trout_fm_info.rf_rssi);
	READ_REG(FM_REG_RSSI_STS, &trout_fm_info.rssi);
	READ_REG(FM_REG_INPWR_STS, &trout_fm_info.inpwr_sts);
	READ_REG(FM_REG_WBRSSI_STS, &trout_fm_info.fm_sts);

	trout_fm_info.freq_seek = (trout_fm_info.freq_seek >> 1) + 1;
	trout_fm_info.seek_succes = (trout_fm_info.rssi >> 16) & 0x1;
}

void __trout_fm_show_status(void)
{
	TROUT_PRINT("FM_REG_SEEK_CNT:      0x%x", trout_fm_info.seek_cnt);
	TROUT_PRINT("FM_REG_CHAN_FREQ_STS: 0x%d", trout_fm_info.freq_seek);
	TROUT_PRINT("FM_REG_FREQ_OFF_STS:  0x%x", trout_fm_info.freq_offset);
	TROUT_PRINT("FM_REG_RF_RSSI_STS:   0x%x", trout_fm_info.rf_rssi);

	TROUT_PRINT("FM_REG_INPWR_STS:     0x%x", trout_fm_info.inpwr_sts);
	TROUT_PRINT("FM_REG_WBRSSI_STS:    0x%x", trout_fm_info.fm_sts);
	TROUT_PRINT("FM_REG_RSSI_STS:      0x%x", trout_fm_info.rssi);
	TROUT_PRINT("fm work: 0x%x, fm iq: 0x%04x",
		    (trout_fm_info.fm_sts >> 20) & 0x1,
		    (trout_fm_info.fm_sts >> 16) & 0xf);
	TROUT_PRINT("seek failed: %d  rssi: 0x%08x",
		    (trout_fm_info.rssi >> 16) & 0x1,
		    (trout_fm_info.rssi & 0xFF));
}

int trout_fm_rf_spi_write(u32 addr, u32 data)
{
	u32 spi_data;

	/* Shift the required SPI data bits to MSB */
	spi_data = (data | (addr << 16));

	WRITE_REG(RF_SPI_REG_START_ADDR, spi_data);

	usleep(20);

	return 0;
}

int trout_fm_rf_spi_read(u32 addr, u32 *data)
{
	u32 spi_data;

	spi_data = ((addr << 16) | BIT_31);

	WRITE_REG(RF_SPI_REG_START_ADDR, spi_data);

	usleep(20);

	READ_REG(RF_SPI_REG_START_ADDR, data);

	return 0;
}

int trout_fm_rf_spi_wait_idle(void)
{
	u32 time_out = 0;
	u32 reg_data = 0;

	for (time_out = 0, reg_data |= BIT_0; reg_data & BIT_0; time_out++) {
		if (time_out > 1000)
			return -1;

		READ_REG(FM_REG_SPI_FIFO_STS, &reg_data);
	}

	return 0;
}

int trout_fm_rf_spi_wait_data_ready(void)
{
	u32 time_out = 0;
	u32 reg_data = 0;

	for (time_out = 0, reg_data |= BIT_3; reg_data & BIT_3; time_out++) {
		if (time_out > 1000)
			return -1;

		READ_REG(FM_REG_SPI_FIFO_STS, &reg_data);
	}

	return 0;
}

int trout_fm_cfg_rf_reg(void)
{
	if (WRITE_RF_REG(0x6F, 0X600) < 0)
		return -1;

	if (WRITE_RF_REG(0x451, 0X022) < 0)
		return -1;
	if (WRITE_RF_REG(0x404, 0X0333) < 0)
		return -1;

	if (WRITE_RF_REG(0x400, 0X0011) < 0)
		return -1;

	if (WRITE_RF_REG(0x402, 0X086A) < 0)
		return -1;

	mdelay(100);

	if (WRITE_RF_REG(0x404, 0X0333) < 0)
		return -1;

	return 0;
}

int trout_fm_init(void)
{
	int ret;
	unsigned int reg_data;

	ret = trout_fm_open_clock();
	if (ret < 0) {
		TROUT_PRINT("trout_fm_open_clock failed!");
		return ret;
	}
#ifdef RF_USE_ADI_MODE
	trout_rf_adi_mode();
#else
	trout_rf_spi_mode();
#endif

	ret = trout_fm_reg_cfg();
	if (ret < 0) {
		TROUT_PRINT("trout_fm_reg_cfg failed!");
		return ret;
	}

	ret = trout_fm_cfg_rf_reg();
	if (ret < 0) {
		TROUT_PRINT("trout_fm_cfg_rf_reg failed!");
		return ret;
	}

	ret = trout_fm_iis_pin_cfg();
	if (ret < 0) {
		TROUT_PRINT("trout_fm_iis_pin_cfg failed!");
		return ret;
	}

	READ_REG(FM_REG_ADC_BITMAP, &reg_data);
	reg_data &= ~(0xFFFFUL << 16);
	reg_data |= (0x47EUL << 16);
	WRITE_REG(FM_REG_ADC_BITMAP, reg_data);

	reg_data = (0x402) | (0x404UL << 16);

	WRITE_REG(FM_REG_RF_CTL, reg_data);

	WRITE_REG(FM_REG_DEPHA_SCAL, 0x7);

	mutex_init(&trout_fm_info.mutex);
	atomic_set(&trout_fm_info.fm_searching, 0);

	return 0;
}

int trout_fm_en(void)
{
	u32 reg_data = 0;
	int ret = -EINVAL;

	if (atomic_read(&trout_fm_info.fm_en)) {
		TROUT_PRINT("FM open: already opened, ignore this operation\n");
		return 0;
	}

	READ_REG(FM_REG_FM_EN, &reg_data)

	    reg_data |= BIT_31;
	WRITE_REG(FM_REG_FM_EN, reg_data)

	    atomic_cmpxchg(&trout_fm_info.fm_en, 0, 1);
	return 0;
}

int trout_fm_dis(void)
{
	u32 reg_data = 0;

	READ_REG(FM_REG_FM_EN, &reg_data);

	reg_data &= ~BIT_31;
	WRITE_REG(FM_REG_FM_EN, reg_data);

	if (atomic_read(&trout_fm_info.fm_en))
		atomic_cmpxchg(&trout_fm_info.fm_en, 1, 0);

	return 0;
}

int trout_fm_get_status(int *status)
{
	u32 reg_data = 0;
	READ_REG(FM_REG_FM_EN, &reg_data);

	*status = reg_data >> 31;

	return 0;
}

static int trout_fm_wait_int(int time_out)
{
	ulong jiffies_comp = 0;
	u8 is_timeout = 1;
	int seek_succes = 0;

	jiffies_comp = jiffies;
	do {
		msleep(400);

		READ_REG(FM_REG_INT_STS, &seek_succes);
		READ_REG(FM_REG_RSSI_STS, &trout_fm_info.rssi);

		is_timeout = time_after(jiffies,
					jiffies_comp +
					msecs_to_jiffies(time_out));

		if (is_timeout) {
			TROUT_PRINT("FM search timeout.");
			trout_fm_dis();

			return -EAGAIN;
		}

		if (seek_succes) {
			TROUT_PRINT("FM found frequency.");
			break;
		}
	} while (seek_succes == 0 && is_timeout == 0);

	return 0;
}

int trout_fm_set_tune(u16 freq)
{
	u32 chan;
	int ret;

	TROUT_PRINT("FM set tune to %i MHz.", freq);
	chan = freq;
	trout_fm_dis();

	WRITE_REG(FM_REG_FMCTL_STI, FM_CTL_STI_MODE_TUNE);

	WRITE_REG(FM_REG_CHAN, ((chan - 1) * 2) & 0xffff);

	/* Enable int */
	WRITE_REG(FM_REG_INT_EN, 0x01);

	trout_fm_en();

	ret = trout_fm_wait_int(3000);
	if (ret < 0)
		return ret;

	/* clear interrupt */
	WRITE_REG(FM_REG_INT_CLR, 1);

	trout_fm_info.freq_seek = freq;

	return 0;
}

int trout_fm_get_frequency(u16 *freq)
{
	int ret = -EPERM;

	if (!atomic_read(&trout_fm_info.fm_en)) {
		TROUT_PRINT("Get frequency: FM not open\n");
		return ret;
	}

	TROUT_PRINT("FM get frequency: %i", trout_fm_info.freq_seek);

	*freq = trout_fm_info.freq_seek;
	return 0;
}

int trout_fm_do_seek(u16 freq, u8 seek_dir)
{
	u32 reg_value = 0x0;

	TROUT_PRINT("FM do seek: %iMHz(%i).", freq, seek_dir);

	mutex_lock(&trout_fm_info.mutex);

	trout_fm_dis();

	READ_REG(FM_REG_FM_CTRL, &reg_value);
	if (seek_dir == 0) {
		trout_fm_dis();
		WRITE_REG(FM_REG_FMCTL_STI, FM_CTL_STI_MODE_TUNE);
		WRITE_REG(FM_REG_CHAN, ((freq + 1) * 2) & 0xffff);
		reg_value |= BIT_19;
		trout_fm_en();
	} else {
		trout_fm_dis();
		WRITE_REG(FM_REG_FMCTL_STI, FM_CTL_STI_MODE_TUNE);
		WRITE_REG(FM_REG_CHAN, ((freq - 2) * 2) & 0xffff);
		reg_value &= ~BIT_19;
		trout_fm_en();
	}

	WRITE_REG(FM_REG_FM_CTRL, reg_value);

	WRITE_REG(FM_REG_FMCTL_STI, FM_CTL_STI_MODE_SEEK);

	/* enable int */
	WRITE_REG(FM_REG_INT_EN, 0x01);

	trout_fm_en();

	mutex_unlock(&trout_fm_info.mutex);

	return 0;
}

int trout_fm_seek(u16 frequency, u8 seek_dir, u32 time_out, u16 *freq_found)
{
	int ret = -EPERM;

	TROUT_PRINT("FM seek, freq(%i) dir(%i) timeout(%i).",
		    frequency, seek_dir, time_out);

	if (!atomic_read(&trout_fm_info.fm_en)) {
		TROUT_PRINT("Full search: FM not open\n");
		return ret;
	}

	/* revese seek dir. Temporarily to fit the application. */
	seek_dir ^= 0x1;
	if (!atomic_read(&trout_fm_info.fm_searching)) {
		atomic_cmpxchg(&trout_fm_info.fm_searching, 0, 1);
		if (frequency == 0)
			trout_fm_do_seek(trout_fm_info.freq_seek, seek_dir);
		else
			trout_fm_do_seek(frequency, seek_dir);

	} else {
		return -EBUSY;
	}

	TROUT_PRINT("FM start search....");

	ret = trout_fm_wait_int(time_out);
	atomic_cmpxchg(&trout_fm_info.fm_searching, 1, 0);

	if (ret < 0)
		return ret;

	__trout_fm_get_status();

	__trout_fm_show_status();

	ret = trout_fm_get_frequency(freq_found);

	/* clear interrupt */
	WRITE_REG(FM_REG_INT_CLR, 1);

	return 0;
}

int trout_fm_stop_seek(void)
{
	int ret = -EPERM;
	u32 reg_value = 0x0;

	TROUT_PRINT("FM stop seek.");

	if (atomic_read(&trout_fm_info.fm_searching)) {
		atomic_cmpxchg(&trout_fm_info.fm_searching, 1, 0);

		/* clear seek enable bit of seek register. */
		READ_REG(FM_REG_FMCTL_STI, &reg_value);
		if (reg_value & FM_CTL_STI_MODE_SEEK) {
			reg_value |= FM_CTL_STI_MODE_NORMAL;
			WRITE_REG(FM_REG_FMCTL_STI, reg_value);
		}
		ret = 0;
	}

	return ret;
}

int trout_rf_switch(enum RF_SWITCH_NAME switch_name, enum RF_SWITCH_MODE mode)
{
	u32 reg_data;

	switch (switch_name) {
	case RF_SWITCH_K2_1:
		switch (mode) {
		case RF_SWITCH_MODE_EN:
			READ_REG(0x64f, &reg_data);
			reg_data |= (BIT_11 | BIT_10);
			WRITE_REG(0x64f, reg_data);
			break;
		default:
			READ_REG(0x64f, &reg_data);
			reg_data &= ~(BIT_11 | BIT_10);
			WRITE_REG(0x64f, reg_data);
			break;
		}
		break;
	case RF_SWITCH_K2_2:
		switch (mode) {
		case RF_SWITCH_MODE_EN:
			READ_REG(0x6f, &reg_data);
			reg_data |= (BIT_15);
			WRITE_REG(0x6f, reg_data);
			break;
		default:
			READ_REG(0x6f, &reg_data);
			reg_data &= ~(BIT_15);
			WRITE_REG(0x6f, reg_data);
			break;
		}
		break;
	case RF_SWITCH_K2_3:
	case RF_SWITCH_K2_5:
		switch (mode) {
		case RF_SWITCH_MODE_EN:
			READ_REG(0x64f, &reg_data);
			reg_data |= (BIT_14 | BIT_15);
			WRITE_REG(0x64f, reg_data);
			break;
		default:
			READ_REG(0x64f, &reg_data);
			reg_data &= ~(BIT_14 | BIT_15);
			WRITE_REG(0x64f, reg_data);
			break;
		}
		break;
	case RF_SWITCH_K2_4:
		switch (mode) {
		case RF_SWITCH_MODE_EN:
			READ_REG(0x7a, &reg_data);
			reg_data |= (BIT_2);
			WRITE_REG(0x7a, reg_data);
			break;
		default:
			READ_REG(0x7a, &reg_data);
			reg_data &= ~(BIT_2);
			WRITE_REG(0x7a, reg_data);
			break;
		}
		break;
	case RF_SWITCH_K2_6:
		switch (mode) {
		case RF_SWITCH_MODE_EN:
			READ_REG(0x7a, &reg_data);
			reg_data |= (BIT_9);
			WRITE_REG(0x7a, reg_data);
			break;
		default:
			READ_REG(0x7a, &reg_data);
			reg_data &= ~(BIT_9);
			WRITE_REG(0x7a, reg_data);
			break;
		}
		break;
	case RF_SWITCH_K2_7:
		switch (mode) {
		case RF_SWITCH_MODE_EN:
			READ_REG(0x44f, &reg_data);
			reg_data |= (BIT_14 | BIT_15);
			WRITE_REG(0x44f, reg_data);
			break;
		default:
			READ_REG(0x44f, &reg_data);
			reg_data &= ~(BIT_14 | BIT_15);
			WRITE_REG(0x44f, reg_data);
			break;
		}
		break;
	case RF_SWITCH_K2_8:
		switch (mode) {
		case RF_SWITCH_MODE_EN:
			READ_REG(0x7a, &reg_data);
			reg_data |= (BIT_10);
			WRITE_REG(0x7a, reg_data);
			break;
		default:
			READ_REG(0x7a, &reg_data);
			reg_data &= ~(BIT_10);
			WRITE_REG(0x7a, reg_data);
			break;
		}
		break;
	case RF_SWITCH_K4_1:
		switch (mode) {
		case RF_SWITCH_MODE_EN:
			READ_REG(0x40a, &reg_data);
			reg_data |= (BIT_4);
			WRITE_REG(0x40a, reg_data);
			break;
		default:
			READ_REG(0x40a, &reg_data);
			reg_data &= ~(BIT_4);
			WRITE_REG(0x40a, reg_data);
			break;
		}
		break;
	default:
		return -1;
	}
	return 0;
}
