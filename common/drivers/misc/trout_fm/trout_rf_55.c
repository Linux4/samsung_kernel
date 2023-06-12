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
#include "trout_rf_55.h"
#include "trout_rf_common.h"

enum MODE {
	NONE_MODE = 0,
	WIFI_MODE = 1,
	FM_MODE = 2,
	BT_MODE = 4,
};

static struct trout_fm_info_t trout_fm_info;

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
	addr &= 0x0000ffff;
	WRITE_REG(0x1000 | addr, data);

	return 0;
}

int trout_fm_rf_spi_read(u32 addr, u32 *data)
{
	addr &= 0x0000ffff;
	READ_REG(0x1000 | addr, data);

	return 0;
}

int trout_fm_cfg_rf_reg(void)
{
	WRITE_RF_REG(0x0050, 0x0001);

	WRITE_RF_REG(0x0109, 0x0840);
	WRITE_RF_REG(0x0109, 0x0cc0);
	WRITE_RF_REG(0x0000, 0xffff);

	WRITE_RF_REG(0x00d2, 0x20ff);
	WRITE_RF_REG(0x00d3, 0x487f);
	WRITE_RF_REG(0x0015, 0x8080);
	WRITE_RF_REG(0x00d7, 0x0000);
	WRITE_RF_REG(0x00d8, 0x0000);
	WRITE_RF_REG(0x0007, 0x0600);
	WRITE_RF_REG(0x0080, 0x0000);

	WRITE_RF_REG(0x00d2, 0x202f);
	WRITE_RF_REG(0x00d2, 0x20ef);

	return 0;
}

int trout_fm_set_freq(int freq)
{
	int retry = 0;
	while (retry < 5) {
		retry++;
		trout_fm_set_tune(freq);

		if (((trout_fm_info.rssi >> 16) & 0x1) == 0)
			break;

		msleep(200);
	}
	TROUT_PRINT("Set tune %i times.\n", retry);

	return 0;
}

int trout_fm_init(void)
{
	int ret;
	unsigned int reg_data;

	TROUT_PRINT("Trout RF 55 initial...");

	ret = trout_fm_reg_cfg();
	if (ret < 0) {
		TROUT_PRINT("trout_fm_reg_cfg failed!");
		return ret;
	}

	reg_data = (0x1100) | (0x106fUL << 16);
	WRITE_REG(FM_REG_RF_CTL, reg_data);

	mutex_init(&trout_fm_info.mutex);
	atomic_set(&trout_fm_info.fm_searching, 0);

	return 0;
}

int trout_fm_deinit(void)
{
	return 0;
}

int trout_fm_en(void)
{
	u32 reg_data = 0;

	if (atomic_read(&trout_fm_info.fm_en)) {
		TROUT_PRINT("FM open: already opened, ignore this operation\n");
		return 0;
	}

	READ_REG(FM_REG_FM_EN, &reg_data);

	reg_data |= BIT_31;
	WRITE_REG(FM_REG_FM_EN, reg_data);

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
	unsigned int start_msecond = 0;
	unsigned int end_msecond = 0;

	jiffies_comp = jiffies;
	start_msecond = jiffies_to_msecs(jiffies);
	TROUT_PRINT("start_msecond: %u\n", start_msecond);
	do {
		msleep(20);

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

	end_msecond = jiffies_to_msecs(jiffies);
	TROUT_PRINT("end_msecond: %u\n", end_msecond);
	TROUT_PRINT("used time: %ums\n", end_msecond - start_msecond);
	return 0;
}

int trout_fm_set_tune(u16 freq)
{
	int ret;

	TROUT_PRINT("FM set tune to %i MHz.", freq);

	freq--;

	MxdRfSetFreqHKhzFm(freq);

	trout_fm_dis();

	WRITE_REG(FM_REG_FMCTL_STI, FM_CTL_STI_MODE_TUNE);

	WRITE_REG(FM_REG_CHAN, (freq * 2) & 0xffff);

	/* enable int */
	WRITE_REG(FM_REG_INT_EN, 0x01);

	trout_fm_en();

	ret = trout_fm_wait_int(3000);
	if (ret < 0) {
		TROUT_PRINT("wait fm interrupt timeout.");
		return ret;
	}
	/* clear interrupt */
	WRITE_REG(FM_REG_INT_CLR, 1);

	trout_fm_info.freq_seek = freq;

	__trout_fm_get_status();

	__trout_fm_show_status();

	return 0;
}

int trout_fm_seek(u16 frequency, u8 seek_dir, u32 time_out, u16 *freq_found)
{
	int ret = -EPERM;
	u16 freq = frequency;

	TROUT_PRINT("FM seek, freq(%i) dir(%i) timeout(%i).",
		    frequency, seek_dir, time_out);

	if (!atomic_read(&trout_fm_info.fm_en)) {
		TROUT_PRINT("Full search: FM not open\n");
		return ret;
	}

	if (frequency < MIN_FM_FREQ || frequency > MAX_FM_FREQ)
		frequency = MIN_FM_FREQ;

	if (atomic_read(&trout_fm_info.fm_searching))
		return -EBUSY;

	atomic_cmpxchg(&trout_fm_info.fm_searching, 0, 1);

	seek_dir ^= 0x1;
	while (atomic_read(&trout_fm_info.fm_searching)) {
		if (seek_dir == 0) {
			freq++;
			if (freq > MAX_FM_FREQ)
				freq = MIN_FM_FREQ;
		} else {
			freq--;
			if (freq < MIN_FM_FREQ)
				freq = MAX_FM_FREQ;
		}

		ret = trout_fm_set_tune(freq);
		if (ret < 0) {
			atomic_cmpxchg(&trout_fm_info.fm_searching, 1, 0);
			return ret;
		}

		if (((trout_fm_info.rssi >> 16) & 0x1) == 0)
			break;
	}

	atomic_cmpxchg(&trout_fm_info.fm_searching, 1, 0);

	trout_fm_get_frequency(freq_found);

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

	return 0;
}

