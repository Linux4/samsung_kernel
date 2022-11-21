/****************************************************************************
 Copyright (C) 2015 Samsung Electronics Co., Ltd. All rights reserved.

 ******************************************************************************/

#include "fm_low_struc.h"
#include "radio-s610.h"
#include "fm_ctrl.h"

extern struct s610_radio *gradio;

void fm_pwron(void)
{
	fmspeedy_set_reg_field(0xFFF226, 0, 0x0001, 1); /* FM reset assert */
	fmspeedy_set_reg(0xFFF212, 0); /*  last power on  */
	fmspeedy_set_reg(0xFFF211, 0); /*  first power on  */
	fmspeedy_set_reg_field(0xFFF227, 0, 0x0001, 1); /* FM reset deassert */
	fmspeedy_set_reg(0xFFF210, 0); /*  FM isolaton disable  */
}

void fm_pwroff(void)
{
	fmspeedy_set_reg_field(0xFFF226, 0, 0x0001, 1); /* FM reset assert */
	fmspeedy_set_reg(0xFFF210, 1); /*  FM isolaton enable  */
	fmspeedy_set_reg(0xFFF211, 1); /*  first power off  */
	fmspeedy_set_reg(0xFFF212, 1); /*  last power off  */
}

void fmspeedy_wakeup(void)
{
	write32(gradio->fmspeedy_base + FMSPDY_CTL, SPDY_WAKEUP);
	udelay(5);
}

void fm_speedy_m_int_enable(void)
{
	u32 getval;

	getval = read32(gradio->fmspeedy_base + FMSPDY_INTR_MASK);
	getval &= 0xFFDF;
	write32(gradio->fmspeedy_base + FMSPDY_INTR_MASK, getval);
	write32(gradio->fmspeedy_base + FMSPDY_STAT, 0x7F);
}

void fm_speedy_m_int_disable(void)
{
	write32(gradio->fmspeedy_base + FMSPDY_INTR_MASK, 0xFFFF);
	write32(gradio->fmspeedy_base + FMSPDY_STAT, 0x7F);
}

void fm_en_speedy_m_int(void)
{
	u32 getval;

	getval = read32(gradio->fmspeedy_base + FMSPDY_INTR_MASK);
	getval &= 0xFFDF;
	write32(gradio->fmspeedy_base + FMSPDY_INTR_MASK, getval);
}

void fm_dis_speedy_m_int(void)
{
	u32 getval;

	getval = read32(gradio->fmspeedy_base + FMSPDY_INTR_MASK);
	getval |= 0x0020;
	write32(gradio->fmspeedy_base + FMSPDY_INTR_MASK, getval);
}

void fm_speedy_m_int_stat_clear(void)
{
	write32(gradio->fmspeedy_base + FMSPDY_STAT, 0x1F);
}

void wait_atomic(void)
{
	while (1) {
		if (!atomic_read(&gradio->is_doing))
			break;
		gradio->wait_atomic++;
		if (gradio->wait_atomic > 0xffffff) {
			gradio->wait_atomic = 0;
			break;
		}
	}
}

u32 fmspeedy_get_reg_core(u32 addr)
{
	u16 ii = 0, jj = 0;
	u32 status1, status2;
	u32 retval = 0;

	fm_dis_speedy_m_int();

	for (ii = 0; ii < 5; ii++) {
		if ((read32(gradio->fmspeedy_base + AUDIO_CTRL)
				& 0x200000) != 0) {
			write32(gradio->fmspeedy_base + FMSPDY_STAT, 0x1F);
			for (jj = 0; jj < 100; jj++) {
				udelay(2);
				status1 = read32(gradio->fmspeedy_base
						+ FMSPDY_STAT);
				status2 = read32(gradio->fmspeedy_base
						+ FMSPDY_MISC_STAT);
				if (((status1 & 0x1F) == 0) &&
						((status2 & 0x01) == 1))
					break;
			}
		} else {
			write32(gradio->fmspeedy_base + FMSPDY_STAT, 0x1F);
			for (jj = 0; jj < 100; jj++) {
				udelay(2);
				status1 = read32(gradio->fmspeedy_base
						+ FMSPDY_STAT);
				if ((status1 & 0x1F) == 0)
					break;
			}
		}

		if (jj > 99)
			break;

		write32(gradio->fmspeedy_base + FMSPDY_CMD,
				FMSPDY_READ | FMSPDY_RANDOM
				| ((addr & 0x01FF) << 7));

		for (jj = 0; jj < 100; jj++) {
			udelay(2);
			status1 = read32(gradio->fmspeedy_base + FMSPDY_STAT);
			if ((status1 & STAT_DONE) == 1)
				break;
		}

		if (jj > 99)
			break;

		if ((status1 & RX_ALL_ERR) == 0) {
			fm_en_speedy_m_int();
			retval = read32(gradio->fmspeedy_base + FMSPDY_DATA);
			break;
		}
	}
	fm_en_speedy_m_int();

	return retval;
}

u32 fmspeedy_get_reg(u32 addr)
{
	u32 data;

	spin_lock_irq(&gradio->slock);

	wait_atomic();
	atomic_set(&gradio->is_doing, 1);
	data = fmspeedy_get_reg_core(addr);
	atomic_set(&gradio->is_doing, 0);

	spin_unlock_irq(&gradio->slock);

	return data;
}

void fmspeedy_set_reg_core(u32 addr, u32 data)
{
	u16 ii, jj;
	u32 status1, status2;

	fm_dis_speedy_m_int();

	for (ii = 0; ii < 5; ii++) {
		write32(gradio->fmspeedy_base + FMSPDY_DATA, data);

		if ((read32(gradio->fmspeedy_base + AUDIO_CTRL)
				& 0x200000) != 0) {
			write32(gradio->fmspeedy_base + FMSPDY_STAT, 0x1F);
			for (jj = 0; jj < 100; jj++) {
				udelay(2);
				status1 = read32(gradio->fmspeedy_base
						+ FMSPDY_STAT);
				status2 = read32(gradio->fmspeedy_base
						+ FMSPDY_MISC_STAT);
				if (((status1 & 0x1F) == 0)
						&& ((status2 & 0x01) == 1)) {
					break;
				}
			}
		} else {
			write32(gradio->fmspeedy_base + FMSPDY_STAT, 0x1F);
			for (jj = 0; jj < 100; jj++) {
				udelay(2);
				status1 = read32(gradio->fmspeedy_base
						+ FMSPDY_STAT);
				if ((status1 & 0x1F) == 0)
					break;
			}
		}

		if (jj > 99)
			break;

		write32(gradio->fmspeedy_base + FMSPDY_CMD,
				FMSPDY_WRITE | FMSPDY_RANDOM
				| ((addr & 0x01FF) << 7));

		for (jj = 0; jj < 100; jj++) {
			udelay(2);
			status1 = read32(gradio->fmspeedy_base + FMSPDY_STAT);
			if ((status1 & STAT_DONE) == 1)
				break;
		}

		if (jj > 99)
			break;

		if ((status1 & RX_ALL_ERR) == 0) {
			write32(gradio->fmspeedy_base + FMSPDY_STAT, 0x1F);
			break;
		}
	}
	fm_en_speedy_m_int();

	if (ii > 4)
		dev_err(gradio->dev, "speedy_set_field write fail\n");
}

void fmspeedy_set_reg(u32 addr, u32 data)
{
	spin_lock_irq(&gradio->slock);

	wait_atomic();
	atomic_set(&gradio->is_doing, 1);
	fmspeedy_set_reg_core(addr, data);
	atomic_set(&gradio->is_doing, 0);

	spin_unlock_irq(&gradio->slock);

}

u32 fmspeedy_get_reg_field_core(u32 addr, u32 shift, u32 mask)
{
	u16 ii, jj;
	u32 status1, status2;

	fm_dis_speedy_m_int();

	for (ii = 0; ii < 5; ii++) {
		if ((read32(gradio->fmspeedy_base
				+ AUDIO_CTRL) & 0x200000) != 0) {
			write32(gradio->fmspeedy_base + FMSPDY_STAT, 0x1F);
			for (jj = 0; jj < 100; jj++) {
				udelay(2);
				status1 = read32(gradio->fmspeedy_base
						+ FMSPDY_STAT);
				status2 = read32(gradio->fmspeedy_base
						+ FMSPDY_MISC_STAT);
				if (((status1 & 0x1F) == 0)
						&& ((status2 & 0x01) == 1))
					break;
			}
		} else {
			write32(gradio->fmspeedy_base + FMSPDY_STAT, 0x1F);
			for (jj = 0; jj < 100; jj++) {
				udelay(2);
				status1 = read32(gradio->fmspeedy_base
						+ FMSPDY_STAT);
				if ((status1 & 0x1F) == 0)
					break;
			}
		}

		if (jj > 99)
			break;

		write32(gradio->fmspeedy_base + FMSPDY_CMD,
				FMSPDY_READ | FMSPDY_RANDOM
				| ((addr & 0x01FF) << 7));

		for (jj = 0; jj < 100; jj++) {
			udelay(2);
			status1 = read32(gradio->fmspeedy_base
					+ FMSPDY_STAT);
			if ((status1 & STAT_DONE) == 1)
				break;
		}

		if (jj > 99)
			break;

		if ((status1 & RX_ALL_ERR) == 0) {
			fm_en_speedy_m_int();
			return ((read32(gradio->fmspeedy_base
					+ FMSPDY_DATA) & (mask))
					>> shift);
		}
	}
	fm_en_speedy_m_int();

	return 0;
}

u32 fmspeedy_get_reg_field(u32 addr, u32 shift, u32 mask)
{
	u32 data;

	spin_lock_irq(&gradio->slock);

	wait_atomic();
	atomic_set(&gradio->is_doing, 1);
	data = fmspeedy_get_reg_field_core(addr, shift, mask);
	atomic_set(&gradio->is_doing, 0);

	spin_unlock_irq(&gradio->slock);

	return data;
}

void fmspeedy_set_reg_field_core(u32 addr, u32 shift, u32 mask, u32 data)
{
	u32 value, value1;
	u16 ii, jj;
	u32 status1, status2;

	fm_dis_speedy_m_int();

	for (ii = 0; ii < 5; ii++) {
		if ((read32(gradio->fmspeedy_base
				+ AUDIO_CTRL) & 0x200000) != 0) {
			write32(gradio->fmspeedy_base + FMSPDY_STAT, 0x1F);
			for (jj = 0; jj < 100; jj++) {
				udelay(2);
				status1 = read32(gradio->fmspeedy_base
						+ FMSPDY_STAT);
				status2 = read32(gradio->fmspeedy_base
						+ FMSPDY_MISC_STAT);
				if (((status1 & 0x1F) == 0)
						&& ((status2 & 0x01) == 1))
					break;
			}
		} else {
			write32(gradio->fmspeedy_base + FMSPDY_STAT, 0x1F);
			for (jj = 0; jj < 100; jj++) {
				udelay(2);
				status1 = read32(gradio->fmspeedy_base
						+ FMSPDY_STAT);
				if ((status1 & 0x1F) == 0)
					break;
			}
		}

		if (jj > 99)
			break;

		write32(gradio->fmspeedy_base + FMSPDY_CMD,
				FMSPDY_READ | FMSPDY_RANDOM
				| ((addr & 0x01FF) << 7));

		for (jj = 0; jj < 100; jj++) {
			udelay(2);
			status1 = read32(gradio->fmspeedy_base
					+ FMSPDY_STAT);
			if ((status1 & STAT_DONE) == 1)
				break;
		}

		if (jj > 99)
			break;

		if ((status1 & RX_ALL_ERR) == 0)
			break;
	}

	if ((ii > 4) || (jj > 99)) {
		fm_en_speedy_m_int();
		dev_err(gradio->dev, "speedy_set_field write fail\n");
		return;
	}

#if 0
	value = (read32(gradio->fmspeedy_base
			+ FMSPDY_DATA) & ~(mask))
		| ((data) << (shift));

#else
	value1 = read32(gradio->fmspeedy_base + FMSPDY_DATA);
	value = (value1 & ~(mask)) | ((data) << (shift));

	if (addr == 0xFFF2A9)
		APIEBUG(gradio, "speedy read %x, %x\n", value1, value);

#endif

	for (ii = 0; ii < 5; ii++) {
		write32(gradio->fmspeedy_base + FMSPDY_DATA, value);

		if ((read32(gradio->fmspeedy_base
				+ AUDIO_CTRL) & 0x200000) != 0) {
			write32(gradio->fmspeedy_base + FMSPDY_STAT, 0x1F);
			for (jj = 0; jj < 100; jj++) {
				udelay(2);
				status1 = read32(gradio->fmspeedy_base
						+ FMSPDY_STAT);
				status2 = read32(gradio->fmspeedy_base
						+ FMSPDY_MISC_STAT);
				if (((status1 & 0x1F) == 0)
						&& ((status2 & 0x01) == 1))
					break;
			}
		} else {
			write32(gradio->fmspeedy_base + FMSPDY_STAT, 0x1F);
			for (jj = 0; jj < 100; jj++) {
				udelay(2);
				status1 = read32(gradio->fmspeedy_base
						+ FMSPDY_STAT);
				if ((status1 & 0x1F) == 0)
					break;
			}
		}

		if (jj > 99)
			break;

		write32(gradio->fmspeedy_base + FMSPDY_CMD,
				FMSPDY_WRITE | FMSPDY_RANDOM
				| ((addr & 0x01FF) << 7));

		for (jj = 0; jj < 100; jj++) {
			udelay(2);
			status1 = read32(gradio->fmspeedy_base
					+ FMSPDY_STAT);
			if ((status1 & STAT_DONE) == 1)
				break;
		}

		if (jj > 99)
			break;

		udelay(10);

		if ((status1 & RX_ALL_ERR) == 0)
			break;
	}

	fm_en_speedy_m_int();

	if ((ii > 4) || (jj > 99))
		dev_err(gradio->dev, "speedy_set_field write fail\n");
}

void fmspeedy_set_reg_field(u32 addr, u32 shift, u32 mask, u32 data)
{
	spin_lock_irq(&gradio->slock);

	wait_atomic();
	atomic_set(&gradio->is_doing, 1);
	fmspeedy_set_reg_field_core(addr, shift, mask, data);
	atomic_set(&gradio->is_doing, 0);

	spin_unlock_irq(&gradio->slock);

}

u32 fmspeedy_get_reg_int_core(u32 addr)
{
	u16 ii = 0, jj = 0;
	u32 status1, status2;
	u32 retval = 0;

	fm_dis_speedy_m_int();

	for (ii = 0; ii < 5; ii++) {
		if ((read32(gradio->fmspeedy_base
				+ AUDIO_CTRL) & 0x200000) != 0) {
			write32(gradio->fmspeedy_base + FMSPDY_STAT, 0x1F);
			for (jj = 0; jj < 100; jj++) {
				udelay(2);
				status1 = read32(gradio->fmspeedy_base
						+ FMSPDY_STAT);
				status2 = read32(gradio->fmspeedy_base
						+ FMSPDY_MISC_STAT);
				if (((status1 & 0x1F) == 0)
						&& ((status2 & 0x01) == 1))
					break;
			}
		} else {
			write32(gradio->fmspeedy_base + FMSPDY_STAT, 0x1F);
			for (jj = 0; jj < 100; jj++) {
				udelay(2);
				status1 = read32(gradio->fmspeedy_base
						+ FMSPDY_STAT);
				if ((status1 & 0x1F) == 0)
					break;
			}
		}

		if (jj > 99)
			break;

		write32(gradio->fmspeedy_base + FMSPDY_CMD,
				FMSPDY_READ | FMSPDY_RANDOM
				| ((addr & 0x01FF) << 7));

		for (jj = 0; jj < 100; jj++) {
			udelay(2);
			status1 = read32(gradio->fmspeedy_base + FMSPDY_STAT);
			if ((status1 & STAT_DONE) == 1)
				break;
		}

		if (jj > 99)
			break;

		if ((status1 & RX_ALL_ERR) == 0) {
			retval = read32(gradio->fmspeedy_base + FMSPDY_DATA);
			break;
		}
	}

	fm_en_speedy_m_int();

	return retval;
}

u32 fmspeedy_get_reg_int(u32 addr)
{
	u32 data;


	wait_atomic();
	atomic_set(&gradio->is_doing, 1);
	data = fmspeedy_get_reg_int_core(addr);
	atomic_set(&gradio->is_doing, 0);


	return data;
}


void fmspeedy_set_reg_int_core(u32 addr, u32 data)
{
	u16 ii, jj;
	u32 status1, status2;

	fm_dis_speedy_m_int();

	for (ii = 0; ii < 5; ii++) {
		write32(gradio->fmspeedy_base + FMSPDY_DATA, data);

		if ((read32(gradio->fmspeedy_base
				+ AUDIO_CTRL) & 0x200000) != 0) {
			write32(gradio->fmspeedy_base + FMSPDY_STAT, 0x1F);
			for (jj = 0; jj < 100; jj++) {
				udelay(2);
				status1 = read32(gradio->fmspeedy_base
						+ FMSPDY_STAT);
				status2 = read32(gradio->fmspeedy_base
						+ FMSPDY_MISC_STAT);
				if (((status1 & 0x1F) == 0)
						&& ((status2 & 0x01) == 1))
					break;
			}
		} else {
			write32(gradio->fmspeedy_base + FMSPDY_STAT, 0x1F);
			for (jj = 0; jj < 100; jj++) {
				udelay(2);
				status1 = read32(gradio->fmspeedy_base
						+ FMSPDY_STAT);
				if ((status1 & 0x1F) == 0)
					break;
			}
		}

		if (jj > 99)
			break;

		write32(gradio->fmspeedy_base + FMSPDY_CMD,
				FMSPDY_WRITE | FMSPDY_RANDOM
				| ((addr & 0x01FF) << 7));

		for (jj = 0; jj < 100; jj++) {
			udelay(2);
			status1 = read32(gradio->fmspeedy_base + FMSPDY_STAT);
			if ((status1 & STAT_DONE) == 1)
				break;
		}

		if (jj > 99)
			break;

		if ((status1 & RX_ALL_ERR) == 0) {
			write32(gradio->fmspeedy_base + FMSPDY_STAT, 0x1F);
			break;
		}
	}

	if (ii > 4)
		dev_err(gradio->dev, "speedy_set retry fail\n");

	fm_en_speedy_m_int();
}

void fmspeedy_set_reg_int(u32 addr, u32 data)
{

	wait_atomic();
	atomic_set(&gradio->is_doing, 1);
	fmspeedy_set_reg_int_core(addr, data);
	atomic_set(&gradio->is_doing, 0);


}

/****************************************************************************
 NAME
 fm_audio_control   -  Audio out enable/disable

 FUNCTION
 Setting registers for Audio
 ****************************************************************************/
void fm_audio_control(struct s610_radio *radio,
		bool audio_out, bool lr_switch,
		u32 req_time, u32 audio_addr)
{
	write32(radio->fmspeedy_base + AUDIO_CTRL,
		((audio_out << 21) | (lr_switch << 20)
		| ((req_time & 0x07FF) << 9)
		| (audio_addr & 0x01FF)));
	udelay(15);
}

