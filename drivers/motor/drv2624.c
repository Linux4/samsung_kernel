/*
 ** =============================================================================
 ** Copyright (c) 2016  Texas Instruments Inc.
 **
 ** This program is free software; you can redistribute it and/or
 ** modify it under the terms of the GNU General Public License
 ** as published by the Free Software Foundation; either version 2
 ** of the License, or (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** You should have received a copy of the GNU General Public License
 ** along with this program; if not, write to the Free Software
 ** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 **
 ** File:
 **     drv2624.c
 **
 ** Description:
 **     DRV2624 chip driver
 **
 ** =============================================================================
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/semaphore.h>
#include <linux/device.h>
#include <linux/syscalls.h>
#include <asm/uaccess.h>
#include <linux/gpio.h>
#include <linux/sched.h>
#include <linux/spinlock_types.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/firmware.h>
#include <linux/miscdevice.h>
#include <linux/drv2624.h>
#include <linux/of.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include "../staging/android/timed_output.h"
#include <linux/sec_sysfs.h>
#include <linux/kthread.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>

#ifdef CONFIG_SLPI_MOTOR
#include <linux/adsp/slpi_motor.h>
#endif

static struct device *motor_dev;
extern struct class *sec_class;

//#define	AUTOCALIBRATION_ENABLE

static struct drv2624_data *g_DRV2624data = NULL;

static int drv2624_reg_read(struct drv2624_data *pDrv2624data, unsigned char reg)
{
	unsigned int val;
	int ret;

	ret = regmap_read(pDrv2624data->mpRegmap, reg, &val);

	if (ret < 0) {
		dev_err(pDrv2624data->dev, 
				"%s reg=0x%x error %d\n", __FUNCTION__, reg, ret);
		return ret;
	}
	else
		return val;
}

static int drv2624_reg_write(struct drv2624_data *pDrv2624data, 
		unsigned char reg, unsigned char val)
{
	int ret;

	ret = regmap_write(pDrv2624data->mpRegmap, reg, val);
	if (ret < 0) {
		dev_err(pDrv2624data->dev, 
				"%s reg=0x%x, value=0%x error %d\n", 
				__FUNCTION__, reg, val, ret);
	}
	return ret;
}

static int drv2624_bulk_read(struct drv2624_data *pDrv2624data, 
		unsigned char reg, unsigned int count, u8 *buf)
{
	int ret;
	ret = regmap_bulk_read(pDrv2624data->mpRegmap, reg, buf, count);
	if (ret < 0) {
		dev_err(pDrv2624data->dev, 
				"%s reg=0%x, count=%d error %d\n", 
				__FUNCTION__, reg, count, ret);
	}

	return ret;
}

static int drv2624_bulk_write(struct drv2624_data *pDrv2624data, 
		unsigned char reg, unsigned int count, const u8 *buf)
{
	int ret;
	ret = regmap_bulk_write(pDrv2624data->mpRegmap, reg, buf, count);
	if (ret < 0) {
		dev_err(pDrv2624data->dev, 
				"%s reg=0%x, count=%d error %d\n", 
				__FUNCTION__, reg, count, ret);
	}

	return ret;	
}

static int drv2624_set_intensity(struct drv2624_data *pDrv2624data, int intensity)
{
	if ((intensity < 0) || (intensity > MAX_INTENSITY)) {
		pr_err("%s out of range %d\n", __func__, intensity);
		return -EINVAL;
	}

	pDrv2624data->intensity = intensity;

	return 0;
}

static void drv2624_set_frequency(struct drv2624_data *pDrv2624data, int set_freq)
{
	int32_t set_reg;	// value to be set in REG_OL_PERIOD_H, REG_OL_PERIOD_L (0x2e, 0x2f)


	if (pDrv2624data->f_packet_en == false) {
		if (set_freq < 0 || set_freq >= pDrv2624data->freq_num) {
			pr_err("%s out of range %d\n", __func__, set_freq);
			return;
		}
		pDrv2624data->period_h = pDrv2624data->multi_freq1[set_freq];
		pDrv2624data->period_l = pDrv2624data->multi_freq2[set_freq];
		pDrv2624data->frequency = pDrv2624data->multi_freq[set_freq];
	}
	else {
		if (set_freq <= 0 || set_freq > 3500) {
			pr_err("%s out of range %d\n", __func__, set_freq);
			return;
		}

		pDrv2624data->frequency = set_freq;
	
		set_reg = 40626 / set_freq + 3;	// freq = 1 / (reg * step) , 1step = 24.615us,  1000000000 / 24615 = 40626
	
		/* REG_LRA_PERIOD : 0~7bit : 2f | 8~9bit : 2e */
		pDrv2624data->period_h = (set_reg >> 8) & 0xff;
		pDrv2624data->period_l = (set_reg) & 0xff;
	}

	drv2624_reg_write(g_DRV2624data, DRV2624_REG_OL_PERIOD_H, pDrv2624data->period_h);
	drv2624_reg_write(g_DRV2624data, DRV2624_REG_OL_PERIOD_L, pDrv2624data->period_l);
}

static void drv2624_set_rtp_input(struct drv2624_data *pDrv2624data)
{
	int32_t set_reg = 1;
	int limit;

	if (pDrv2624data->f_packet_en) 
		limit = pDrv2624data->rtp_input_limit_OD;
	else  
		limit = pDrv2624data->rtp_input_limit;

	set_reg = pDrv2624data->intensity * limit / 10000;

	if (set_reg == 0) {	// for the case intensity * rtp_input_limit < 10000
		if (pDrv2624data->intensity == 0) 
			set_reg = 0;		
		else set_reg = 1;	
	}
	pDrv2624data->rtp_input_value = set_reg;
	
	drv2624_reg_write(pDrv2624data, DRV2624_REG_RTP_INPUT, set_reg);	
}

static int drv2624_set_bits(struct drv2624_data *pDrv2624data, 
		unsigned char reg, unsigned char mask, unsigned char val)
{
	int ret;
	ret = regmap_update_bits(pDrv2624data->mpRegmap, reg, mask, val);
	if (ret < 0) {
		dev_err(pDrv2624data->dev, 
				"%s reg=%x, mask=0x%x, value=0x%x error %d\n", 
				__FUNCTION__, reg, mask, val, ret);
	}

	return ret;	
}

static int drv2624_set_go_bit(struct drv2624_data *pDrv2624data, unsigned char val)
{
	return drv2624_reg_write(pDrv2624data, DRV2624_REG_GO, (val&0x01));
}

static void drv2624_change_mode(struct drv2624_data *pDrv2624data, unsigned char work_mode)
{
	drv2624_set_bits(pDrv2624data, DRV2624_REG_MODE, DRV2624MODE_MASK , work_mode);
}

static int vibrator_get_time(struct timed_output_dev *dev)
{
	struct drv2624_data *pDrv2624data = container_of(dev, struct drv2624_data, to_dev);

	if (hrtimer_active(&pDrv2624data->timer)) {
		ktime_t r = hrtimer_get_remaining(&pDrv2624data->timer);
		return ktime_to_ms(r);
	}

	return 0;
}

static void drv2624_stop(struct drv2624_data *pDrv2624data)
{
	if (pDrv2624data->mnVibratorPlaying == YES) {
		hrtimer_cancel(&pDrv2624data->timer);
		gpio_set_value(pDrv2624data->msPlatData.mnGpioMODE, STOP);
		pDrv2624data->mnVibratorPlaying = NO;
		wake_unlock(&pDrv2624data->wklock);
		pr_info("[VIB] %s\n", __func__);
#ifdef CONFIG_SLPI_MOTOR
		setSensorCallback(false, pDrv2624data->timevalue);
#endif
	}
}

static void vibrator_enable( struct timed_output_dev *dev, int value)
{
	struct drv2624_data *pDrv2624data = 
		container_of(dev, struct drv2624_data, to_dev);

	flush_kthread_worker(&pDrv2624data->kworker);
	hrtimer_cancel(&pDrv2624data->timer);
	mutex_lock(&pDrv2624data->lock);

	value = (value>MAX_TIMEOUT)?MAX_TIMEOUT:value;
	pDrv2624data->timevalue = value;

	pDrv2624data->mnWorkMode = WORK_IDLE;

	if (value > 0) {
		pr_info("[VIB] turn ON\n");
		wake_lock(&pDrv2624data->wklock);
#ifdef CONFIG_SLPI_MOTOR
		setSensorCallback(true, pDrv2624data->timevalue);
#endif
		if (pDrv2624data->f_packet_en) {
			pDrv2624data->timevalue = pDrv2624data->haptic_eng[0].time;
			drv2624_set_frequency(pDrv2624data, pDrv2624data->haptic_eng[0].freq);
			drv2624_set_intensity(pDrv2624data, pDrv2624data->haptic_eng[0].intensity);
			drv2624_reg_write(pDrv2624data, DRV2624_REG_CONTROL1, 0xd8);//auto braking on
		}
		else {
			drv2624_reg_write(pDrv2624data, DRV2624_REG_CONTROL1, 0xc0);//auto braking off
		}
		
		drv2624_set_rtp_input(pDrv2624data);
		pDrv2624data->mnVibratorPlaying = YES;

		pr_info("[VIB] %s : time = %dms, intensity = %d, rtp_input = %d, frequency = %d (PERIOD_H = 0x%02x , PERIOD_L = 0x%02x)\n", __func__, pDrv2624data->timevalue, pDrv2624data->intensity, pDrv2624data->rtp_input_value, pDrv2624data->frequency, pDrv2624data->period_h, pDrv2624data->period_l);		
		
		gpio_set_value(pDrv2624data->msPlatData.mnGpioMODE, GO);	//trigger ON!
		
		hrtimer_start(&pDrv2624data->timer, 
				ns_to_ktime((u64)pDrv2624data->timevalue * NSEC_PER_MSEC), HRTIMER_MODE_REL);
	}
	else {

		pDrv2624data->f_packet_en = false;
		pDrv2624data->packet_cnt = 0;
		pDrv2624data->packet_size = 0;
		
		if (pDrv2624data->mnVibratorPlaying == YES){
			pr_info("[VIB] turn OFF\n");
			drv2624_stop(pDrv2624data);
		}
		else{
			pr_info("[VIB] already OFF\n");		
		}
	}
	mutex_unlock(&pDrv2624data->lock);
}

static enum hrtimer_restart vibrator_timer_func(struct hrtimer *timer)
{
	struct drv2624_data *pDrv2624data = 
		container_of(timer, struct drv2624_data, timer);
	pr_info("[VIB] %s\n", __func__);
	pDrv2624data->mnWorkMode |= WORK_VIBRATOR;
//	schedule_work(&pDrv2624data->vibrator_work);
	queue_kthread_work(&pDrv2624data->kworker, &pDrv2624data->vibrator_work);

	return HRTIMER_NORESTART;
}

//static void vibrator_work_routine(struct work_struct *work)
static void vibrator_work_routine(struct kthread_work *work)
{
	struct drv2624_data *pDrv2624data = 
		container_of(work, struct drv2624_data, vibrator_work);
	struct hrtimer *timer = &pDrv2624data->timer;
	unsigned char mode;
	pr_info("[VIB] %s\n", __func__);
	mutex_lock(&pDrv2624data->lock);

	if (pDrv2624data->mnWorkMode & WORK_IRQ) {		
		unsigned char status = pDrv2624data->mnIntStatus;	
		if (status & OVERCURRENT_MASK) {
			dev_err(pDrv2624data->dev, 
					"ERROR, Over Current detected!!\n");
		}

		if (status & OVERTEMPRATURE_MASK) {
			dev_err(pDrv2624data->dev, 
					"ERROR, Over Temperature detected!!\n");
		}

		if (status & ULVO_MASK) {
			dev_err(pDrv2624data->dev, 
					"ERROR, VDD drop observed!!\n");
		}

		if (status & PRG_ERR_MASK) {
			dev_err(pDrv2624data->dev, 
					"ERROR, PRG error!!\n");			
		}

		if (status & PROCESS_DONE_MASK) {
			mode = drv2624_reg_read(pDrv2624data, DRV2624_REG_MODE) & DRV2624MODE_MASK;
			if (mode == MODE_CALIBRATION) {
				if ((status&DIAG_MASK) != DIAG_SUCCESS) {
					dev_err(pDrv2624data->dev, "Calibration fail\n");
				} else {
					unsigned char calComp = 
						drv2624_reg_read(pDrv2624data, DRV2624_REG_CAL_COMP);
					unsigned char calBemf = 
						drv2624_reg_read(pDrv2624data, DRV2624_REG_CAL_BEMF);
					unsigned char calBemfGain = 
						drv2624_reg_read(pDrv2624data, DRV2624_REG_CAL_COMP) & BEMFGAIN_MASK;
					dev_info(pDrv2624data->dev, 
							"AutoCal : Comp=0x%x, Bemf=0x%x, Gain=0x%x\n",
							calComp, calBemf, calBemfGain);
				}	
			} else if (mode == MODE_DIAGNOSTIC) {
				if ((status&DIAG_MASK) != DIAG_SUCCESS) {
					dev_err(pDrv2624data->dev, "Diagnostic fail\n");
				} else {
					unsigned char diagZ = drv2624_reg_read(pDrv2624data, DRV2624_REG_DIAG_Z);
					unsigned char diagK = drv2624_reg_read(pDrv2624data, DRV2624_REG_DIAG_K);
					dev_info(pDrv2624data->dev, 
							"Diag : ZResult=0x%x, CurrentK=0x%x\n",
							diagZ, diagK);
				}						
			} else if (mode == MODE_WAVEFORM_SEQUENCER) {
				dev_info(pDrv2624data->dev, 
						"Waveform Sequencer Playback finished\n");
			}

			if (pDrv2624data->mnVibratorPlaying == YES) {
				pDrv2624data->mnVibratorPlaying = NO;
				wake_unlock(&pDrv2624data->wklock);
			}
		}

		pDrv2624data->mnWorkMode &= ~WORK_IRQ;
	}
	if (pDrv2624data->f_packet_en) {
		if (++pDrv2624data->packet_cnt >= pDrv2624data->packet_size) {
			pDrv2624data->f_packet_en = false;
			pDrv2624data->packet_cnt = 0;
			pDrv2624data->packet_size = 0;
			//trigger mode pin stop
			drv2624_stop(pDrv2624data);
		} else {
			drv2624_set_frequency(pDrv2624data, pDrv2624data->haptic_eng[pDrv2624data->packet_cnt].freq);
			drv2624_set_intensity(pDrv2624data, pDrv2624data->haptic_eng[pDrv2624data->packet_cnt].intensity);
			drv2624_set_rtp_input(pDrv2624data);
			pr_info("[VIB] %s: time:%d\n", __func__, pDrv2624data->haptic_eng[pDrv2624data->packet_cnt].time);
			hrtimer_start(timer, ns_to_ktime((u64)pDrv2624data->haptic_eng[pDrv2624data->packet_cnt].time * NSEC_PER_MSEC), HRTIMER_MODE_REL);
		}
	}
	else {
		if(pDrv2624data->mnWorkMode & WORK_VIBRATOR){
			drv2624_stop(pDrv2624data);
			pDrv2624data->mnWorkMode &= ~WORK_VIBRATOR;
		}
	}

	mutex_unlock(&pDrv2624data->lock);
}

#if 0
static int fw_chksum(const struct firmware *fw){
	int sum = 0;
	int i=0;
	int size = fw->size;
	const unsigned char *pBuf = fw->data;

	for (i=0; i< size; i++) {
		if((i>11) && (i<16)) {

		} else {
			sum += pBuf[i];
		}
	}

	return sum;
}

static void drv2624_firmware_load(const struct firmware *fw, void *context)
{
	struct drv2624_data *pDrv2624data = context;
	int size = 0, fwsize = 0, i=0;
	const unsigned char *pBuf = NULL;

	if (fw != NULL) {
		pBuf = fw->data;
		size = fw->size;

		memcpy(&(pDrv2624data->msFwHeader), pBuf, sizeof(struct drv2624_fw_header));
		if ((pDrv2624data->msFwHeader.fw_magic != DRV2624_MAGIC) 
				||(pDrv2624data->msFwHeader.fw_size != size)
				||(pDrv2624data->msFwHeader.fw_chksum != fw_chksum(fw))) {
			dev_err(pDrv2624data->dev,
					"%s, ERROR!! firmware not right:Magic=0x%x,Size=%d,chksum=0x%x\n", 
					__FUNCTION__, pDrv2624data->msFwHeader.fw_magic, 
					pDrv2624data->msFwHeader.fw_size, pDrv2624data->msFwHeader.fw_chksum);
		} else {
			dev_err(pDrv2624data->dev,
					"%s, firmware good\n", __FUNCTION__);

			pBuf += sizeof(struct drv2624_fw_header);

			drv2624_reg_write(pDrv2624data, DRV2624_REG_RAM_ADDR_UPPER, 0);
			drv2624_reg_write(pDrv2624data, DRV2624_REG_RAM_ADDR_LOWER, 0);

			fwsize = size - sizeof(struct drv2624_fw_header);
			for (i = 0; i < fwsize; i++) {
				drv2624_reg_write(pDrv2624data, DRV2624_REG_RAM_DATA, pBuf[i]);
			}			
		}	
	} else {
		dev_err(pDrv2624data->dev,
				"%s, ERROR!! firmware not found\n", __FUNCTION__);
	}
}

static void HapticsFirmwareLoad(const struct firmware *fw, void *context)
{
	struct drv2624_data *pDrv2624data = context;

	mutex_lock(&pDrv2624data->lock);

	drv2624_firmware_load(fw, context);
	release_firmware(fw);

	mutex_unlock(&pDrv2624data->lock);
}
#endif

static int drv2624_set_seq_loop(struct drv2624_data *pDrv2624data, unsigned long arg)
{
	int ret = 0, i;
	struct drv2624_seq_loop seqLoop;
	unsigned char halfSize = DRV2624_SEQUENCER_SIZE / 2;
	unsigned char loop[2] = {0, 0};
	pr_info("[VIB] %s\n", __func__);
	if (copy_from_user(&seqLoop, 
				(void __user *)arg, sizeof(struct drv2624_seq_loop)))
		return -EFAULT;

	for( i=0; i < DRV2624_SEQUENCER_SIZE; i++) {
		if(i < halfSize) {
			loop[0] |= (seqLoop.mpLoop[i] << (i*2));
		} else {
			loop[1] |= (seqLoop.mpLoop[i] << ((i-halfSize)*2));
		}
	}

	ret = drv2624_bulk_write(pDrv2624data, DRV2624_REG_SEQ_LOOP_1, 2, loop);

	return ret;	
}

static int drv2624_set_main(struct drv2624_data *pDrv2624data, unsigned long arg)
{
	int ret = 0;
	struct drv2624_wave_setting mainSetting;
	unsigned char control = 0;

	if (copy_from_user(&mainSetting, 
				(void __user *)arg, sizeof(struct drv2624_wave_setting)))
		return -EFAULT;

	control |= mainSetting.meScale;
	control |= (mainSetting.meInterval << INTERVAL_SHIFT);
	drv2624_set_bits(pDrv2624data, 
			DRV2624_REG_CONTROL2,
			SCALE_MASK | INTERVAL_MASK,
			control);

	drv2624_set_bits(pDrv2624data, 
			DRV2624_REG_MAIN_LOOP,
			0x07, mainSetting.meLoop);

	return ret;	
}

static int drv2624_set_wave_seq(struct drv2624_data *pDrv2624data, unsigned long arg)
{
	int ret = 0;
	struct drv2624_wave_seq waveSeq;

	if (copy_from_user(&waveSeq, 
				(void __user *)arg, sizeof(struct drv2624_wave_seq)))
		return -EFAULT;

	ret = drv2624_bulk_write(pDrv2624data, 
			DRV2624_REG_SEQUENCER_1, DRV2624_SEQUENCER_SIZE, waveSeq.mpWaveIndex);

	return ret;	
}

static int drv2624_get_diag_result(struct drv2624_data *pDrv2624data, unsigned long arg)
{
	int ret = 0;
	struct drv2624_diag_result diagResult;
	unsigned char mode, go;

	memset(&diagResult, 0, sizeof(struct drv2624_diag_result));

	mode = drv2624_reg_read(pDrv2624data, DRV2624_REG_MODE) & DRV2624MODE_MASK;
	if (mode != MODE_DIAGNOSTIC) {
		diagResult.mnFinished = -EFAULT;
		return ret;
	}

	go = drv2624_reg_read(pDrv2624data, DRV2624_REG_GO) & 0x01;
	if (go) {
		diagResult.mnFinished = NO;
	} else {
		diagResult.mnFinished = YES;
		diagResult.mnResult = 
			((pDrv2624data->mnIntStatus & DIAG_MASK) >> DIAG_SHIFT);
		diagResult.mnDiagZ = drv2624_reg_read(pDrv2624data, DRV2624_REG_DIAG_Z);
		diagResult.mnDiagK= drv2624_reg_read(pDrv2624data, DRV2624_REG_DIAG_K);	
	}

	if (copy_to_user((void __user *)arg, &diagResult, sizeof(struct drv2624_diag_result)))
		return -EFAULT;

	return ret;	
}

static int drv2624_get_autocal_result(struct drv2624_data *pDrv2624data, unsigned long arg)
{
	int ret = 0;
	struct drv2624_autocal_result autocalResult;
	unsigned char mode, go;

	memset(&autocalResult, 0, sizeof(struct drv2624_autocal_result));

	mode = drv2624_reg_read(pDrv2624data, DRV2624_REG_MODE) & DRV2624MODE_MASK;
	if (mode != MODE_CALIBRATION) {
		autocalResult.mnFinished = -EFAULT;
		return ret;
	}

	go = drv2624_reg_read(pDrv2624data, DRV2624_REG_GO) & 0x01;
	if (go) {
		autocalResult.mnFinished = NO;
	} else {
		autocalResult.mnFinished = YES;
		autocalResult.mnResult = 
			((pDrv2624data->mnIntStatus & DIAG_MASK) >> DIAG_SHIFT);
		autocalResult.mnCalComp = drv2624_reg_read(pDrv2624data, DRV2624_REG_CAL_COMP);
		autocalResult.mnCalBemf = drv2624_reg_read(pDrv2624data, DRV2624_REG_CAL_BEMF);
		autocalResult.mnCalGain = 
			drv2624_reg_read(pDrv2624data, DRV2624_REG_CAL_COMP) & BEMFGAIN_MASK;
	}

	if (copy_to_user((void __user *)arg, &autocalResult, sizeof(struct drv2624_autocal_result)))
		return -EFAULT;

	return ret;	
}

static int drv2624_file_open(struct inode *inode, struct file *file)
{
	if (!try_module_get(THIS_MODULE)) return -ENODEV;

	file->private_data = (void*)g_DRV2624data;
	return 0;
}

static int drv2624_file_release(struct inode *inode, struct file *file)
{
	file->private_data = (void*)NULL;
	module_put(THIS_MODULE);

	return 0;
}

static long drv2624_file_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct drv2624_data *pDrv2624data = file->private_data;
	//void __user *user_arg = (void __user *)arg;
	int ret = 0;

	mutex_lock(&pDrv2624data->lock);

	switch (cmd) {
		case DRV2624_SET_SEQ_LOOP:
			ret = drv2624_set_seq_loop(pDrv2624data, arg);
			break;

		case DRV2624_SET_MAIN:
			ret = drv2624_set_main(pDrv2624data, arg);
			break;

		case DRV2624_SET_WAV_SEQ:
			ret = drv2624_set_wave_seq(pDrv2624data, arg);
			break;

		case DRV2624_WAVSEQ_PLAY:
			{	
				drv2624_stop(pDrv2624data);	

				wake_lock(&pDrv2624data->wklock);
				pDrv2624data->mnVibratorPlaying = YES;
				drv2624_change_mode(pDrv2624data, MODE_WAVEFORM_SEQUENCER);	
				drv2624_set_go_bit(pDrv2624data, GO);			
			}
			break;

		case DRV2624_STOP:
			{
				drv2624_stop(pDrv2624data);		
			}
			break;

		case DRV2624_RUN_DIAGNOSTIC:
			{
				drv2624_stop(pDrv2624data);	

				wake_lock(&pDrv2624data->wklock);
				pDrv2624data->mnVibratorPlaying = YES;
				drv2624_change_mode(pDrv2624data, MODE_DIAGNOSTIC);			
				drv2624_set_go_bit(pDrv2624data, GO);			
			}
			break;		

		case DRV2624_GET_DIAGRESULT:
			ret = drv2624_get_diag_result(pDrv2624data, arg);
			break;	

		case DRV2624_RUN_AUTOCAL:
			{
				drv2624_stop(pDrv2624data);	

				wake_lock(&pDrv2624data->wklock);
				pDrv2624data->mnVibratorPlaying = YES;
				drv2624_change_mode(pDrv2624data, MODE_CALIBRATION);			
				drv2624_set_go_bit(pDrv2624data, GO);			
			}
			break;		

		case DRV2624_GET_CALRESULT:
			ret = drv2624_get_autocal_result(pDrv2624data, arg);
			break;			
	}

	mutex_unlock(&pDrv2624data->lock);

	return ret;
}

static ssize_t drv2624_file_read(struct file* filp, char* buff, size_t length, loff_t* offset)
{
	struct drv2624_data *pDrv2624data = (struct drv2624_data *)filp->private_data;
	int ret = 0;
	unsigned char value = 0;
	unsigned char *p_kBuf = NULL;

	mutex_lock(&pDrv2624data->lock);

	switch (pDrv2624data->mnFileCmd)
	{
		case HAPTIC_CMDID_REG_READ:
			if (length == 1) {
				ret = drv2624_reg_read(pDrv2624data, pDrv2624data->mnCurrentReg);
				if ( 0 > ret) {
					dev_err(pDrv2624data->dev, "dev read fail %d\n", ret);
					mutex_unlock(&pDrv2624data->lock);
					return ret;
				}
				value = ret;

				ret = copy_to_user(buff, &value, 1);
				if (0 != ret) {
					/* Failed to copy all the data, exit */
					dev_err(pDrv2624data->dev, "copy to user fail %d\n", ret);
					mutex_unlock(&pDrv2624data->lock);
					return 0;
				}	
			} else if (length > 1) {
				p_kBuf = (unsigned char *)kzalloc(length, GFP_KERNEL);
				if (p_kBuf != NULL) {
					ret = drv2624_bulk_read(pDrv2624data, 
							pDrv2624data->mnCurrentReg, length, p_kBuf);
					if ( 0 > ret) {
						dev_err(pDrv2624data->dev, "dev bulk read fail %d\n", ret);
					} else {
						ret = copy_to_user(buff, p_kBuf, length);
						if (0 != ret) {
							dev_err(pDrv2624data->dev, "copy to user fail %d\n", ret);
						}
					}

					kfree(p_kBuf);
				} else {
					dev_err(pDrv2624data->dev, "read no mem\n");
					mutex_unlock(&pDrv2624data->lock);
					return -ENOMEM;
				}
			}
			break;

		case HAPTIC_CMDID_READ_FIRMWARE:
			{
				int i;
				p_kBuf = (unsigned char *)kzalloc(length, GFP_KERNEL);
				if (p_kBuf != NULL) {
					drv2624_reg_write(pDrv2624data, DRV2624_REG_RAM_ADDR_UPPER, pDrv2624data->mnFwAddUpper);
					drv2624_reg_write(pDrv2624data, DRV2624_REG_RAM_ADDR_LOWER, pDrv2624data->mnFwAddLower);

					for (i=0; i < length; i++) {
						p_kBuf[i] = drv2624_reg_read(pDrv2624data, DRV2624_REG_RAM_DATA);
					}

					ret = copy_to_user(buff, p_kBuf, length);
					if (0 != ret) {
						/* Failed to copy all the data, exit */
						dev_err(pDrv2624data->dev, "copy to user fail %d\n", ret);
					}

					kfree(p_kBuf);
				}else{
					dev_err(pDrv2624data->dev, "read no mem\n");
					mutex_unlock(&pDrv2624data->lock);
					return -ENOMEM;
				}
			}
			break;

		default:
			pDrv2624data->mnFileCmd = 0;
			break;
	}

	mutex_unlock(&pDrv2624data->lock);

	return length;
}

static ssize_t drv2624_file_write(struct file* filp, const char* buff, size_t len, loff_t* off)
{
	struct drv2624_data *pDrv2624data = 
		(struct drv2624_data *)filp->private_data;

	mutex_lock(&pDrv2624data->lock);

	pDrv2624data->mnFileCmd = buff[0];

	switch(pDrv2624data->mnFileCmd)
	{
		case HAPTIC_CMDID_REG_READ:
			{
				if (len == 2) {
					pDrv2624data->mnCurrentReg = buff[1];
				} else {
					dev_err(pDrv2624data->dev, " read cmd len %d err\n", (int)len);
				}
				break;
			}

		case HAPTIC_CMDID_REG_WRITE:
			{
				if ((len-1) == 2) {
					drv2624_reg_write(pDrv2624data, buff[1], buff[2]);	
				} else if ((len-1)>2) {
					unsigned char *data = (unsigned char *)kzalloc(len-2, GFP_KERNEL);
					if (data != NULL) {
						if (copy_from_user(data, &buff[2], len-2) != 0) {
							dev_err(pDrv2624data->dev, 
									"%s, reg copy err\n", __FUNCTION__);	
						} else {
							drv2624_bulk_write(pDrv2624data, buff[1], len-2, data);
						}
						kfree(data);
					} else {
						dev_err(pDrv2624data->dev, "memory fail\n");	
					}
				} else {
					dev_err(pDrv2624data->dev, 
							"%s, reg_write len %d error\n", __FUNCTION__, (int)len);
				}
				break;
			}

		case HAPTIC_CMDID_REG_SETBIT:
			{
				int i=1;			
				for (i=1; i< len; ) {
					drv2624_set_bits(pDrv2624data, buff[i], buff[i+1], buff[i+2]);
					i += 3;
				}
				break;
			}	

		case HAPTIC_CMDID_UPDATE_FIRMWARE:
			{
				struct firmware fw;
				unsigned char *fw_buffer = (unsigned char *)kzalloc(len-1, GFP_KERNEL);
				int result = -1;

				drv2624_stop(pDrv2624data);	

				if (fw_buffer != NULL) {	
					fw.size = len-1;

					wake_lock(&pDrv2624data->wklock);
					result = copy_from_user(fw_buffer, &buff[1], fw.size);
					if (result == 0) {
						dev_info(pDrv2624data->dev, 
								"%s, fwsize=%d, f:%x, l:%x\n", 
								__FUNCTION__, (int)fw.size, buff[1], buff[len-1]);
						fw.data = (const unsigned char *)fw_buffer;
						//					drv2624_firmware_load(&fw, (void *)pDrv2624data);	
					}
					wake_unlock(&pDrv2624data->wklock);

					kfree(fw_buffer);
				}
				break;
			}

		case HAPTIC_CMDID_READ_FIRMWARE:
			{
				if (len == 3) {
					pDrv2624data->mnFwAddUpper = buff[2];
					pDrv2624data->mnFwAddLower = buff[1];
				} else {
					dev_err(pDrv2624data->dev, 
							"%s, read fw len error\n", __FUNCTION__);
				}
				break;
			}

		default:
			dev_err(pDrv2624data->dev, "%s, unknown cmd\n", __FUNCTION__);
			break;
	}

	mutex_unlock(&pDrv2624data->lock);

	return len;
}

static struct file_operations fops =
{
	.owner = THIS_MODULE,
	.read = drv2624_file_read,
	.write = drv2624_file_write,
	.unlocked_ioctl = drv2624_file_unlocked_ioctl,
	.open = drv2624_file_open,
	.release = drv2624_file_release,
};

static struct miscdevice drv2624_misc =
{
	.minor = MISC_DYNAMIC_MINOR,
	.name = HAPTICS_DEVICE_NAME,
	.fops = &fops,
};

static int Haptics_init(struct drv2624_data *pDrv2624data)
{
	int ret = 0;
	struct task_struct *kworker_task;

	pDrv2624data->to_dev.name = "vibrator";
	pDrv2624data->to_dev.get_time = vibrator_get_time;
	pDrv2624data->to_dev.enable = vibrator_enable;

	ret = timed_output_dev_register(&(pDrv2624data->to_dev));
	if ( ret < 0) {
		dev_err(pDrv2624data->dev, 
				"drv2624: fail to create timed output dev\n");
		return ret;
	}

	ret = misc_register(&drv2624_misc);
	if (ret) {
		dev_err(pDrv2624data->dev, "drv2624 misc fail: %d\n", ret);
		return ret;
	}

	hrtimer_init(&pDrv2624data->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	pDrv2624data->timer.function = vibrator_timer_func;
	//INIT_WORK(&pDrv2624data->vibrator_work, vibrator_work_routine);
	init_kthread_worker(&pDrv2624data->kworker);
	kworker_task = kthread_run(kthread_worker_fn,
			&pDrv2624data->kworker, "drv2624_haptic");
	if (IS_ERR(kworker_task)) {
		pr_err("Failed to create message pump task\n");
	}
	init_kthread_work(&pDrv2624data->vibrator_work, vibrator_work_routine);

	wake_lock_init(&pDrv2624data->wklock, WAKE_LOCK_SUSPEND, "vibrator");
	mutex_init(&pDrv2624data->lock);

	return 0;
}

#if 0
static void dev_init_platform_data(struct drv2624_data *pDrv2624data)
{
	struct drv2624_platform_data *pDrv2624Platdata = &pDrv2624data->msPlatData;
	struct actuator_data actuator = pDrv2624Platdata->msActuator;
	unsigned char value_temp = 0;
	unsigned char mask_temp = 0;

	if (1)
		return;

	drv2624_set_bits(pDrv2624data, 
			DRV2624_REG_INT_ENABLE, INT_MASK_ALL, INT_ENABLE_ALL);

	drv2624_set_bits(pDrv2624data, 
			DRV2624_REG_MODE, PINFUNC_MASK, (PINFUNC_INT<<PINFUNC_SHIFT));

	if ((actuator.meActuatorType == ERM)||
			(actuator.meActuatorType == LRA)) {
		mask_temp |= ACTUATOR_MASK;
		value_temp |= (actuator.meActuatorType << ACTUATOR_SHIFT);
	}

	if ((pDrv2624Platdata->meLoop == CLOSE_LOOP)||
			(pDrv2624Platdata->meLoop == OPEN_LOOP)) {
		mask_temp |= LOOP_MASK;
		value_temp |= (pDrv2624Platdata->meLoop << LOOP_SHIFT);
	}

	if (value_temp != 0) {
		drv2624_set_bits(pDrv2624data, 
				DRV2624_REG_CONTROL1, 
				mask_temp|AUTOBRK_OK_MASK, value_temp|AUTOBRK_OK_ENABLE);
	}

	if (actuator.mnRatedVoltage != 0) {
		drv2624_reg_write(pDrv2624data, 
				DRV2624_REG_RATED_VOLTAGE, actuator.mnRatedVoltage);
	} else {
		dev_err(pDrv2624data->dev, 
				"%s, ERROR Rated ZERO\n", __FUNCTION__);
	}

	if (actuator.mnOverDriveClampVoltage != 0) {
		drv2624_reg_write(pDrv2624data, 
				DRV2624_REG_OVERDRIVE_CLAMP, actuator.mnOverDriveClampVoltage);
	} else {
		dev_err(pDrv2624data->dev,
				"%s, ERROR OverDriveVol ZERO\n", __FUNCTION__);
	}

	if (actuator.meActuatorType == LRA) {
		unsigned char DriveTime = 5*(1000 - actuator.mnLRAFreq)/actuator.mnLRAFreq;
		unsigned short openLoopPeriod = 
			(unsigned short)((unsigned int)1000000000 / (24619 * actuator.mnLRAFreq)); 

		if (actuator.mnLRAFreq < 125) 
			DriveTime |= (MINFREQ_SEL_45HZ << MINFREQ_SEL_SHIFT);
		drv2624_set_bits(pDrv2624data, 
				DRV2624_REG_DRIVE_TIME, 
				DRIVE_TIME_MASK | MINFREQ_SEL_MASK, DriveTime);	
		drv2624_set_bits(pDrv2624data, 
				DRV2624_REG_OL_PERIOD_H, 0x03, (openLoopPeriod&0x0300)>>8);			
		drv2624_reg_write(pDrv2624data, 
				DRV2624_REG_OL_PERIOD_L, (openLoopPeriod&0x00ff));

		dev_info(pDrv2624data->dev,
				"%s, LRA = %d, DriveTime=0x%x\n", 
				__FUNCTION__, actuator.mnLRAFreq, DriveTime);
	}	
}
#endif
static void drv2624_gpio_init(struct device *dev,
		struct drv2624_platform_data *pdata)
{
	gpio_direction_output(pdata->mnGpioMODE, 0);
	udelay(900);
	gpio_direction_output(pdata->mnGpioNRST, 0);
	udelay(1000);
	gpio_direction_output(pdata->mnGpioNRST, 1);
	udelay(500);
}

#ifdef CONFIG_OF
static int drv2624_parse_dt(struct device *dev,
		struct drv2624_data *data)
{
	struct device_node *node = dev->of_node;
	struct pinctrl *motor_pinctrl;
	int i, err;

	motor_pinctrl = devm_pinctrl_get_select(dev, "motor_default");
	if (IS_ERR(motor_pinctrl)) {
		if (PTR_ERR(motor_pinctrl) == -EPROBE_DEFER)
			pr_err("[VIB]: Error %d\n", -EPROBE_DEFER);
		pr_debug("[VIB]: Target does not use pinctrl\n");
		motor_pinctrl = NULL;
	}

	data->msPlatData.mnGpioPWR = of_get_named_gpio(node, "motor_power", 0);
	if (data->msPlatData.mnGpioPWR) {
		err = gpio_request(data->msPlatData.mnGpioPWR, HAPTICS_DEVICE_NAME"PWR");
		if (err < 0) {
			dev_err(dev, "%s: GPIO %d request MODE error\n", __FUNCTION__, data->msPlatData.mnGpioPWR);				
			return err;
		}
		pr_info("[VIB] %s : get motor_pwr GPIO[%d]\n", __func__, data->msPlatData.mnGpioPWR);
	} else {
		data->msPlatData.mnGpioPWR = -EINVAL;
		pr_info("[VIB] %s : failed get motor_pwr %d\n", __func__, data->msPlatData.mnGpioPWR);
	}

	data->msPlatData.mnGpioMODE = of_get_named_gpio(node, "motor_mode", 0);
	if (data->msPlatData.mnGpioMODE) {
		err = gpio_request(data->msPlatData.mnGpioMODE,HAPTICS_DEVICE_NAME"MODE");
		if (err < 0) {
			dev_err(dev, 
					"%s: GPIO %d request MODE error\n", 
					__FUNCTION__, data->msPlatData.mnGpioMODE);				
			return err;
		}
	}
	pr_info("[VIB] %s : motor_mode %d\n", __func__, gpio_get_value(data->msPlatData.mnGpioMODE));//!@

	data->msPlatData.mnGpioNRST = of_get_named_gpio(node, "motor_rst_n", 0);
	if (data->msPlatData.mnGpioNRST) {
		err = gpio_request(data->msPlatData.mnGpioNRST,HAPTICS_DEVICE_NAME"NRST");
		if (err < 0) {
			dev_err(dev, 
					"%s: GPIO %d request NRST error\n", 
					__FUNCTION__, data->msPlatData.mnGpioNRST);				
			return err;
		}
	}
	pr_info("[VIB] %s : motor_rst_n %d\n", __func__ , gpio_get_value(data->msPlatData.mnGpioNRST));//!@

	err = of_property_read_u32(node, "rtp_input_limit", &data->rtp_input_limit);
	if (err) {
		pr_info("[VIB] %s : no specified max rtp input\n", __func__);
		data->rtp_input_limit = MAX_RTP_INPUT;
	}
	pr_info("[VIB] %s : rtp_input_limit : %d\n", __func__ , data->rtp_input_limit);//!@

	err = of_property_read_u32(node, "overdrive_rtp_input_limit", &data->rtp_input_limit_OD);
	if (err) {
		pr_info("[VIB] %s : no specified max rtp input\n", __func__);
		data->rtp_input_limit_OD = MAX_RTP_INPUT;
	}
	pr_info("[VIB] %s : rtp_input_limit : %d\n", __func__ , data->rtp_input_limit_OD);//!@
	
	err = of_property_read_u32(node, "support_multi_freq", &data->f_multi_freq);
        if (err) {
                pr_info("multi_freq_support_num is not specified so won't support multi freq\n");
                data->f_multi_freq = 0;
        }

	if (data->f_multi_freq) {
		err = of_property_read_u32(node, "multi_freq_support_num", &data->freq_num);
	        if (err) {
        	        pr_info("multi_freq_support_num is not specified set to default 5\n");
                	data->freq_num = 5;
        	}

		data->multi_freq = devm_kzalloc(dev, sizeof(u32)*data->freq_num, GFP_KERNEL);
		if (!data->multi_freq) {
			pr_err("%s: failed to allocate multi_freq data\n", __func__);
			return -1;
		}	

		err = of_property_read_u32_array(node, "multi_freq", data->multi_freq, data->freq_num);
                if (err) {
                        pr_info("%s: Unable to read multi_freq\n", __func__);
			for(i = 0 ; i < data->freq_num ; i++) 
                        	data->multi_freq[i] = DEFAULT_FREQUENCY;
                }

		data->multi_freq1 = devm_kzalloc(dev, sizeof(u32)*data->freq_num, GFP_KERNEL);
		if (!data->multi_freq1) {
			pr_err("%s: failed to allocate multi_freq1 data\n", __func__);
			return -1;
		}	

		err = of_property_read_u32_array(node, "multi_freq1", data->multi_freq1, data->freq_num);
                if (err) {
                        pr_info("%s: Unable to read multi_freq1\n", __func__);
			for(i = 0 ; i < data->freq_num ; i++) 
                        	data->multi_freq1[i] = DEFAULT_FREQUENCY;
                }

		data->multi_freq2 = devm_kzalloc(dev, sizeof(u32)*data->freq_num, GFP_KERNEL);
		if (!data->multi_freq2) {
			pr_err("%s: failed to allocate multi_freq2 data\n", __func__);
			return -1;
		}	

		err = of_property_read_u32_array(node, "multi_freq2", data->multi_freq2, data->freq_num);
                if (err) {
                        pr_info("%s: Unable to read multi_freq2\n", __func__);
			for(i = 0 ; i < data->freq_num ; i++) 
                        	data->multi_freq2[i] = DEFAULT_FREQUENCY;
                }

		/* debugging */
		printk("[VIB] multi_freq[%d] = {",data->freq_num);
		for (i = 0; i < data->freq_num; i++) {
			printk("%d ",data->multi_freq[i]);		
		}
		printk("}\n");
		
		printk("[VIB] multi_freq1[%d] = {",data->freq_num);	
		for (i = 0; i < data->freq_num; i++) {
			printk("0x%02x ",data->multi_freq1[i]);		
		}
		printk("}\n");

		printk("[VIB] multi_freq2[%d] = {",data->freq_num);		
		for (i = 0; i < data->freq_num; i++) {
			printk("0x%02x ",data->multi_freq2[i]);		
		}
		printk("}\n");
		
	} else {
		err = of_property_read_u32(node, "frequency", &data->frequency);
		if (err) {
			pr_info("[VIB] %s : no specified frequency\n", __func__);
			data->frequency = DEFAULT_FREQUENCY;
		}
		pr_info("[VIB] %s : frequency : %d\n", __func__ , data->frequency);//!@
	}
	return 0;
}
#endif
/*
static irqreturn_t drv2624_irq_handler(int irq, void *dev_id)
{
	struct drv2624_data *pDrv2624data = (struct drv2624_data *)dev_id;

	pDrv2624data->mnIntStatus = 
		drv2624_reg_read(pDrv2624data,DRV2624_REG_STATUS);
	if (pDrv2624data->mnIntStatus & INT_MASK) {
		pDrv2624data->mnWorkMode |= WORK_IRQ;
		//		schedule_work(&pDrv2624data->vibrator_work);
		queue_kthread_work(&pDrv2624data->kworker, &pDrv2624data->vibrator_work);
	}
	return IRQ_HANDLED;
}
*/
#ifdef AUTOCALIBRATION_ENABLE	
static int dev_auto_calibrate(struct drv2624_data *pDrv2624data)
{
	wake_lock(&pDrv2624data->wklock);
	pDrv2624data->mnVibratorPlaying = YES;
	drv2624_change_mode(pDrv2624data, MODE_CALIBRATION);
	drv2624_set_go_bit(pDrv2624data, GO);

	return 0;
}
#endif

static struct regmap_config drv2624_i2c_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.cache_type = REGCACHE_NONE,
};

static void drv2624_reg_init(struct drv2624_data *pDrv2624data)
{
	drv2624_reg_write(pDrv2624data, DRV2624_REG_MODE, 0x54);	//MODE : trigger 
	drv2624_reg_write(pDrv2624data, DRV2624_REG_CONTROL1, 0xc0);	//autobraking off
	drv2624_reg_write(pDrv2624data, DRV2624_REG_OVERDRIVE_CLAMP, 0xff);	//should be set to max always
	drv2624_reg_write(pDrv2624data, DRV2624_REG_LOOP_CONTROL, 0x12);
	drv2624_reg_write(pDrv2624data, DRV2624_REG_BLANKING_IDISS_TIME, 0x88);
	drv2624_reg_write(pDrv2624data, DRV2624_REG_OD_CLAMP_SAMPLE_ZC_DET_TIME, 0x0f);

}
static ssize_t haptic_engine_store(struct device *dev,
	struct device_attribute *devattr, const char *buf, size_t count)
{
	struct timed_output_dev *to_dev = dev_get_drvdata(dev);
	struct drv2624_data *pDrv2624data
		= container_of(to_dev, struct drv2624_data, to_dev);
	int i = 0, _data = 0, tmp = 0;

	if (sscanf(buf++, "%4d", &_data) == 1) {
		if (_data > PACKET_MAX_SIZE *3)
			pr_info("%s, [%d] packet size over\n", __func__, _data);
		else {
			pDrv2624data->packet_size = _data / 3;
			pDrv2624data->packet_cnt = 0;
			pDrv2624data->f_packet_en = true;

			buf = strstr(buf, " ");

			for (i = 0; i < pDrv2624data->packet_size; i++) {
				for (tmp = 0; tmp < 3; tmp++) {
					if(buf==NULL){
						pr_err("%s packet data error\n", __func__);
						pDrv2624data->f_packet_en = false;
						return count;

					}

					if (sscanf(buf++, "%5d", &_data) == 1) {
						switch (tmp){
							case 0:
								pDrv2624data->haptic_eng[i].time = _data;
								break;
							case 1:
								pDrv2624data->haptic_eng[i].intensity = _data;
								break;
							case 2:
								pDrv2624data->haptic_eng[i].freq = _data / 10;
								break;
						}
						buf = strstr(buf, " ");
					} else {
						pr_info("%s packet data error\n", __func__);
						pDrv2624data->f_packet_en = false;
						return count;
					}
				}
			}
		}
	}

	return count;
}

static ssize_t haptic_engine_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct timed_output_dev *to_dev = dev_get_drvdata(dev);
	struct drv2624_data *pDrv2624data
		= container_of(to_dev, struct drv2624_data, to_dev);
	int i = 0, tmp = 0;
	char *bufp = buf;

	bufp += snprintf(bufp, PACKET_MAX_SIZE * 3, "\n");
	for (i = 0; i < pDrv2624data->packet_size && pDrv2624data->f_packet_en; i++) {
		for (tmp = 0; tmp < 3; tmp++) {
			switch (tmp) {
				case 0:
					bufp+= snprintf(bufp, PACKET_MAX_SIZE * 3, "%u,", pDrv2624data->haptic_eng[i].time);
					break;
				case 1:
					bufp+= snprintf(bufp, PACKET_MAX_SIZE * 3, "%u,", pDrv2624data->haptic_eng[i].intensity);
					break;
				case 2:
					bufp+= snprintf(bufp, PACKET_MAX_SIZE * 3, "%u,", pDrv2624data->haptic_eng[i].freq);
					break;
			}
		}
	}
	bufp += snprintf(bufp, PACKET_MAX_SIZE * 3, "\n");

	return strlen(buf);
}

static DEVICE_ATTR(haptic_engine, 0660, haptic_engine_show, haptic_engine_store);

static ssize_t set_chip_register(struct device *dev,
		struct device_attribute *devattr, const char *buf, size_t count)

{
	int ret;
	int set_reg;
	int value;

	ret = sscanf(buf, "%2x %2x", &set_reg, &value);
	if (set_reg < 0x0 || set_reg > 0x30) {
		pr_err("[VIB] %s : invalid register number\n", __func__);
		return -EINVAL;
	}
	if (value < 0x0 || value > 0xff) {
		pr_err("[VIB] %s : invalid value\n", __func__);
		return -EINVAL;
	}
	
	ret = drv2624_reg_write(g_DRV2624data, set_reg, value);
	if (ret < 0) {
		dev_err(g_DRV2624data->dev, 
				"%s reg=0x%x, value=0%x error %d\n", 
				__FUNCTION__, set_reg, value, ret);
		return ret;
	}

	printk("[VIB] Set Register : 0x%x Value : 0x%x\n", set_reg, drv2624_reg_read(g_DRV2624data, set_reg));

	return count;

}

static ssize_t get_chip_register(struct device *dev,
		struct device_attribute *devattr, const char *buf, size_t count)

{
	int ret;
	int set_reg;

	ret = sscanf(buf, "%2x", &set_reg);
	if (set_reg < 0x0 || set_reg > 0x30) {
		pr_err("[VIB] %s : invalid register number\n", __func__);
		return -EINVAL;
	}

	ret = drv2624_reg_read(g_DRV2624data, set_reg);
	if (ret < 0) {
		dev_err(g_DRV2624data->dev, 
				"%s reg=0x%x, error %d\n", 
				__FUNCTION__, set_reg, ret);
		return ret;
	}

	printk("[VIB] Get Register : 0x%x Value : 0x%x\n", set_reg, ret);

	return count; 
}

static ssize_t init_chip_register(struct device *dev,
		struct device_attribute *devattr, char *buf)

{
	if (g_DRV2624data->msPlatData.mnGpioNRST) {
		gpio_direction_output(g_DRV2624data->msPlatData.mnGpioNRST, 0);
		udelay(1000);
		gpio_direction_output(g_DRV2624data->msPlatData.mnGpioNRST, 1);
		udelay(500);
	}

	drv2624_reg_init(g_DRV2624data);

	printk("[VIB] Init Register\n");

	return sprintf(buf, "Init success\n");
}

static ssize_t show_vib_tuning(struct device *dev,  
                struct device_attribute *attr, char *buf)
{
	int reg = 0;
	int32_t freq;

	/* REG_LRA_PERIOD : 0~7bit : 2f | 8~9bit : 2e */
	reg += (drv2624_reg_read(g_DRV2624data, DRV2624_REG_OL_PERIOD_H) << 8) & 0xf00;
	reg += drv2624_reg_read(g_DRV2624data, DRV2624_REG_OL_PERIOD_L);
	if (reg <= 0) {
		pr_err("[VIB] : %s negative: failed to read register, 0 : register value = 0\n", __func__);
		return reg;
	} else {
		freq = 40626 / reg;	// freq = 1 / (reg * step) , 1step = 24.615us, 1000000000 / 24615 = 40626
		sprintf(buf, "freq : %d rtp_input_limit : %d\n", freq, g_DRV2624data->rtp_input_limit);
		return strlen(buf);
	}
}

static ssize_t store_vib_tuning(struct device *dev,                        
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;                             
	int set_freq, set_rtp_input_limit;                                           

	retval = sscanf(buf, "%5d %3d", &set_freq, &set_rtp_input_limit);
	if (retval == 0) {                                                    
		pr_info("[VIB]: %s, fail to get vib_tuning value\n", __func__);
		return count;                                                       
	}
	if (set_freq <= 0 || set_freq > MAX_FREQUENCY) {
		pr_err("[VIB] %s : invalid frequency setting number\n", __func__);
		return -EINVAL;
	}
	if (set_rtp_input_limit < 0 || set_rtp_input_limit > MAX_RTP_INPUT) {
		pr_err("[VIB] %s : invalid rtp input setting number\n", __func__);
		return -EINVAL;
	}
	
	drv2624_set_frequency(g_DRV2624data, set_freq);
	g_DRV2624data->rtp_input_limit = set_rtp_input_limit;

	printk("[VIB] %s: freq: %d, rtp_input_limit: %d\n", __func__, set_freq, g_DRV2624data->rtp_input_limit);

	return count;
}                                                           

static DEVICE_ATTR(set_register, 0220, NULL, set_chip_register);
static DEVICE_ATTR(get_register, 0220, NULL, get_chip_register);
static DEVICE_ATTR(init_register, 0440, init_chip_register, NULL);
static DEVICE_ATTR(vib_tuning, 0660, show_vib_tuning, store_vib_tuning);

static struct attribute *sec_motor_attributes[] = {
	&dev_attr_set_register.attr,
	&dev_attr_get_register.attr,
	&dev_attr_init_register.attr,
	&dev_attr_vib_tuning.attr,
	NULL,
};

static struct attribute_group sec_motor_attr_group = {
	.attrs = sec_motor_attributes,
};

static ssize_t intensity_store(struct device *dev,
		struct device_attribute *devattr, const char *buf, size_t count)
{
	struct timed_output_dev *t_dev = dev_get_drvdata(dev);
        struct drv2624_data *data = container_of(t_dev, struct drv2624_data, to_dev);
	int ret= 0;
	int set_intensity = 0;
	
	ret = kstrtoint(buf, 0, &set_intensity);
	if (ret) {
		pr_err("[VIB]: %s failed to get intensity",__func__);
		return ret;
	}

	if ((set_intensity < 0) || (set_intensity > MAX_INTENSITY)) {
                pr_err("[VIB]: %sout of rage\n", __func__);
                return -EINVAL;
        }

	data->intensity = set_intensity;

	return count;
}

static ssize_t intensity_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct timed_output_dev *t_dev = dev_get_drvdata(dev);
        struct drv2624_data *data = container_of(t_dev, struct drv2624_data, to_dev);

	return sprintf(buf, "intensity: %u\n", data->intensity);
}

static DEVICE_ATTR(intensity, 0660, intensity_show, intensity_store);

static ssize_t multi_freq_store(struct device *dev,
                struct device_attribute *attr, const char *buf, size_t count)
{
	struct timed_output_dev *t_dev = dev_get_drvdata(dev);
        struct drv2624_data *data = container_of(t_dev, struct drv2624_data, to_dev);
        int ret = 0, set_freq = 0;

        ret = kstrtoint(buf, 0, &set_freq);
	if (ret) {
		pr_err("[VIB]: %s failed to get multi_freq value",__func__);
		return ret;
	}

        if ((set_freq < 0) || (set_freq >= data->freq_num)) {
                pr_err("[VIB]: %s out of freq range\n", __func__);
                return -EINVAL;
        }
	
        drv2624_set_frequency(data, set_freq);

        return count;
}

static ssize_t multi_freq_show(struct device *dev,
                struct device_attribute *attr, char *buf)
{
	struct timed_output_dev *t_dev = dev_get_drvdata(dev);
        struct drv2624_data *data = container_of(t_dev, struct drv2624_data, to_dev);

        return sprintf(buf, "%s %d\n", data->f_multi_freq ? "MULTI" : "FIXED", data->frequency);

}

static DEVICE_ATTR(multi_freq, 0660, multi_freq_show, multi_freq_store);

static int drv2624_probe(struct i2c_client* client, const struct i2c_device_id* id)
{
	struct drv2624_data *pDrv2624data;
//	struct drv2624_platform_data *pDrv2624Platdata = client->dev.platform_data;
	int err = 0;
	int ret = 0;

	pr_info("[VIB] %s\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
	{
		dev_err(&client->dev, "%s:I2C check failed\n", __FUNCTION__);
		return -ENODEV;
	}

	pDrv2624data = devm_kzalloc(&client->dev, sizeof(struct drv2624_data), GFP_KERNEL);
	if (pDrv2624data == NULL) {
		dev_err(&client->dev, "%s:no memory\n", __FUNCTION__);
		return -ENOMEM;
	}

	pDrv2624data->dev = &client->dev;
	pDrv2624data->mpRegmap = devm_regmap_init_i2c(client, &drv2624_i2c_regmap);
	if (IS_ERR(pDrv2624data->mpRegmap)) {
		err = PTR_ERR(pDrv2624data->mpRegmap);
		dev_err(pDrv2624data->dev, 
				"%s:Failed to allocate register map: %d\n",__FUNCTION__,err);
		return err;
	}

//	memcpy(&pDrv2624data->msPlatData, pDrv2624Platdata, sizeof(struct drv2624_platform_data));

	ret = drv2624_parse_dt(&client->dev, pDrv2624data);                                         
	if (ret) {                                                                            
		pr_err("[%s] drv2624 parse dt failed\n", __func__);                          
		return ret;                                                                   
	}

	g_DRV2624data = pDrv2624data;

	gpio_direction_output(g_DRV2624data->msPlatData.mnGpioPWR, 1);

	drv2624_gpio_init(&client->dev, &pDrv2624data->msPlatData);

	err = drv2624_reg_read(pDrv2624data, DRV2624_REG_ID);
	if (err < 0) {
		dev_err(pDrv2624data->dev, "%s, i2c bus fail (%d)\n", __FUNCTION__, err);
		goto exit_gpio_request_failed;
	} else {
		dev_info(pDrv2624data->dev, "%s, ID status (0x%x)\n", __FUNCTION__, err);
		pDrv2624data->mnDeviceID = err;
	}
	/*	
	if (pDrv2624data->mnDeviceID != DRV2624_ID) {
		dev_err(pDrv2624data->dev, "%s, device_id(%d) fail\n", __FUNCTION__, pDrv2624data->mnDeviceID);
		goto exit_gpio_request_failed;
	}
	 */	
//	dev_init_platform_data(pDrv2624data);
	/*
	if (pDrv2624data->msPlatData.mnGpioINT) {
		err = gpio_request(pDrv2624data->msPlatData.mnGpioINT,HAPTICS_DEVICE_NAME"INT");
		if (err < 0) {
			dev_err(pDrv2624data->dev, 
				"%s: GPIO %d request INT error\n", 
				__FUNCTION__, pDrv2624data->msPlatData.mnGpioINT);					
		goto exit_gpio_request_failed;
	}

	gpio_direction_input(pDrv2624data->msPlatData.mnGpioINT);

	err = request_threaded_irq(client->irq, drv2624_irq_handler,
		NULL, IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
		client->name, pDrv2624data);

	if (err < 0) {
		dev_err(pDrv2624data->dev, "%s: request_irq failed\n", __FUNCTION__);							
		goto exit_gpio_request_failed;
		}
	}
	*/

	Haptics_init(pDrv2624data);

	err = sysfs_create_file(&pDrv2624data->to_dev.dev->kobj,
			&dev_attr_intensity.attr);
	if (err < 0) {
		pr_err("Failed to register sysfs : %d\n", err);
		goto exit_gpio_request_failed;
	}

	err = sysfs_create_file(&pDrv2624data->to_dev.dev->kobj,
				&dev_attr_multi_freq.attr);
	if (err < 0) {
		pr_err("Failed to register multi_freq sysfs : %d\n", err);
		goto exit_gpio_request_failed;
	}
	err = sysfs_create_file(&pDrv2624data->to_dev.dev->kobj,
				&dev_attr_haptic_engine.attr);
	if (err < 0) {
		pr_err("Failed to register haptic_engine sysfs : %d\n", err);
		goto exit_gpio_request_failed;
	}
	/*	err = request_firmware_nowait(THIS_MODULE, 
		FW_ACTION_HOTPLUG,	"drv2624.bin",	&(client->dev), 
		GFP_KERNEL, pDrv2624data, HapticsFirmwareLoad);*/

#ifdef AUTOCALIBRATION_ENABLE	
	err = dev_auto_calibrate(pDrv2624data);
	if(err < 0){
		dev_err(pDrv2624data->dev, "%s, ERROR, calibration fail\n", __FUNCTION__);
	}
#endif

	/* create sysfs*/ 
	motor_dev = device_create(sec_class, NULL, 0, NULL, "motor");
	if (IS_ERR(motor_dev)) {
		pr_info("[VIB]: Failed to create device for samsung vib\n");                          
		goto exit_sec_devices;
	}

	err = sysfs_create_group(&motor_dev->kobj, &sec_motor_attr_group);
	if (err) {
		err = -ENODEV;
		pr_err("[VIB] Failed to create sysfs group\
				for samsung specific motor, err num: %d\n", err);
		goto exit_sysfs;
	}

	i2c_set_clientdata(client,pDrv2624data);

	drv2624_reg_init(pDrv2624data);
	pDrv2624data->intensity = MAX_INTENSITY;
	drv2624_set_frequency(pDrv2624data, pDrv2624data->frequency);
	
	pDrv2624data->f_packet_en = false;
	pDrv2624data->packet_cnt = 0;
	pDrv2624data->packet_size = 0;

	dev_info(pDrv2624data->dev, "drv2624 probe succeeded\n");

	return 0;

exit_sysfs:
	device_destroy(sec_class,motor_dev->devt);
exit_sec_devices:
exit_gpio_request_failed:
	if (pDrv2624data->msPlatData.mnGpioPWR > 0)
		gpio_free(pDrv2624data->msPlatData.mnGpioPWR);

	if (pDrv2624data->msPlatData.mnGpioNRST) {
		gpio_free(pDrv2624data->msPlatData.mnGpioNRST);
	}

	if (pDrv2624data->msPlatData.mnGpioMODE) {
		gpio_free(pDrv2624data->msPlatData.mnGpioMODE);
	}

	dev_err(pDrv2624data->dev, "%s failed, err=%d\n", __FUNCTION__, err);
	return err;
}

static int drv2624_remove(struct i2c_client* client)
{
	struct drv2624_data *pDrv2624data = i2c_get_clientdata(client);

	if (pDrv2624data->msPlatData.mnGpioPWR > 0)
		gpio_free(pDrv2624data->msPlatData.mnGpioPWR);

	if (pDrv2624data->msPlatData.mnGpioNRST)
		gpio_free(pDrv2624data->msPlatData.mnGpioNRST);

	if (pDrv2624data->msPlatData.mnGpioMODE)
		gpio_free(pDrv2624data->msPlatData.mnGpioMODE);

	misc_deregister(&drv2624_misc);

	return 0;
}

static struct i2c_device_id drv2624_id_table[] =
{
	{ HAPTICS_DEVICE_NAME, 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, drv2624_id_table);

#ifdef CONFIG_OF
static struct of_device_id drv2624_dt_ids[] =
{
	{ .compatible = "drv2624" },
	{ },
};
MODULE_DEVICE_TABLE(of, drv2624_dt_ids);
#endif

static struct i2c_driver drv2624_driver =
{
	.driver = {
		.name = HAPTICS_DEVICE_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = drv2624_dt_ids,
#endif
	},
	.id_table = drv2624_id_table,
	.probe = drv2624_probe,
	.remove = drv2624_remove,
};

static int __init drv2624_init(void)
{
	pr_info("[VIB] %s\n", __func__);
	return i2c_add_driver(&drv2624_driver);
}

static void __exit drv2624_exit(void)
{
	i2c_del_driver(&drv2624_driver);
}

module_init(drv2624_init);
module_exit(drv2624_exit);

MODULE_AUTHOR("Texas Instruments Inc.");
MODULE_DESCRIPTION("Driver for "HAPTICS_DEVICE_NAME);
