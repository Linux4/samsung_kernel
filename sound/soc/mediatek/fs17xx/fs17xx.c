/**
 * Copyright (C) Fourier Semiconductor Inc. 2016-2020. All rights reserved.
 * 2019-01-29 File created.
 */

#include "fsm_public.h"
#if defined(CONFIG_FSM_FS1758)

#define FS1758_STATUS         0xF000
#define FS1758_BOVDS          0x0000
#define FS1758_OTPDS          0x0200
#define FS1758_OVDS           0x0300
#define FS1758_UVDS           0x0400
#define FS1758_OCDS           0x0500
#define FS1758_OTWDS          0x0800
#define FS1758_BOPS           0x0900
#define FS1758_SPKS           0x0A00
#define FS1758_SPKT           0x0B00

#define FS1758_DEVID          0xF001
#define FS1758_REVID          0xF002

#define FS1758_ANASTAT        0xF003
#define FS1758_DIGSTAT        0xF004

#define FS1758_SPKRS          0xF007
#define FS1758_SPKTEMP        0xF008

#define FS1758_CHIPINI        0xF00E

#define FS1758_PWRCTRL        0xF010
#define FS1758_PWDN           0x0010
#define FS1758_I2CR           0x0110

#define FS1758_SYSCTRL        0xF011
#define FS1758_OSCEN          0x0011
#define FS1758_VBGEN          0x0111
#define FS1758_ZMEN           0x0211
#define FS1758_ADCEN          0x0311
#define FS1758_BSTPD          0x0611
#define FS1758_AMPEN          0x0711

#define FS1758_SPKCOEF        0xF014
#define FS1758_CALIBEN        0x0415

#define FS1758_GAINCTRL       0xF018

#define FS1758_ACCKEY         0xF01F

#define FS1758_BSTCTRL        0xF020
#define FS1758_SSEND          0x0F20
#define FS1758_VOUTSEL        0x4820
#define FS1758_ILIMSEL        0x2420
#define FS1758_BSTMODE        0x1020

#define FS1758_PDCTRL         0xF02A

#define FS1758_DACCTRL        0xF030

#define FS1758_LNMCTRL        0xF03F
#define FS1758_LNMODE         0x0F3F

#define FS1758_TSCTRL         0xF04C
#define FS1758_OFFSTA         0x0E4C
#define FS1758_OFF_AUTOEN     0x0D4C
#define FS1758_TSEN           0x034C

#define FS1758_AGC1           0x3059
#define FS1758_AGC1RT         0x3059
#define FS1758_AGC2           0x305A
#define FS1758_AGC3           0x305B
#define FS1758_AGC4           0x305C

#define FS1758_SPKERR         0xF08C
#define FS1758_SPKTCFGH       0xF08D
#define FS1758_SPKTCFGL       0xF08E

#define FS1758_BSGCTRL        0xF0BC
#define FS1758_BSGEN          0x0FBC

#define FS1758_FW_NAME        "fs17xx.fsm"
#define FS1758_RS2RL_RATIO    (1680)


static int fs1758_i2c_reset(fsm_dev_t *fsm_dev)
{
	uint16_t val;
	int ret;
	int i;

	if (!fsm_dev) {
		return -EINVAL;
	}

	for (i = 0; i < FSM_I2C_RETRY; i++) {
		fsm_reg_write(fsm_dev, REG(FS1758_PWRCTRL), 0x0002); // reset nack
		fsm_reg_read(fsm_dev, REG(FS1758_PWRCTRL), NULL); // dummy read
		fsm_delay_ms(15); // 15ms
		ret = fsm_reg_write(fsm_dev, REG(FS1758_PWRCTRL), 0x0001);
		ret |= fsm_reg_read(fsm_dev, REG(FS1758_CHIPINI), &val);
		if ((val == 0x0003) || (val == 0x0300)) { // init finished
			break;
		}
	}
	if (i == FSM_I2C_RETRY) {
		pr_addr(err, "retry timeout");
		ret = -ETIMEDOUT;
	}

	return ret;
}

int fs1758_set_prot(fsm_dev_t *fsm_dev)
{
	uint16_t threshold[5];
	uint16_t zmdata;
	uint16_t value;
	int ret;

	if (fsm_dev == NULL) {
		return -EINVAL;
	}

	pr_addr(info, "re25:%d", fsm_dev->re25);
	if (fsm_dev->re25 == 0) {
		if (fsm_dev->re25_dft == 0) {
			pr_addr(info, "not calibrated yet, Gain=6dB");
			ret = fsm_reg_write(fsm_dev, REG(FS1758_GAINCTRL), 0x0004); // 6dB
			return ret;
		}
		fsm_dev->re25 = fsm_dev->re25_dft;

	}

	zmdata = fsm_cal_spkr_zmimp(fsm_dev, fsm_dev->re25);
	threshold[0] = fsm_cal_threshold(fsm_dev, 5, zmdata); // SPKERR
	threshold[1] = fsm_cal_threshold(fsm_dev, 6, zmdata); // T1
	threshold[2] = fsm_cal_threshold(fsm_dev, 7, zmdata); // T2
	threshold[3] = fsm_cal_threshold(fsm_dev, 8, zmdata); // T3
	threshold[4] = fsm_cal_threshold(fsm_dev, 9, zmdata); // T4
	value = HIGH8(threshold[0]) & 0x00FF;
	ret = fsm_reg_write(fsm_dev, REG(FS1758_SPKERR), value);
	// SPKTCFGH, [15:8]=T1, [7:0]=T2
	value = (threshold[1] & 0xFF00) | (HIGH8(threshold[2]) & 0x00FF);
	ret |= fsm_reg_write(fsm_dev, REG(FS1758_SPKTCFGH), value);
	// SPKTCFGL, [15:8]=T3, [7:0]=T4
	value = (threshold[3] & 0xFF00) | (HIGH8(threshold[4]) & 0x00FF);
	ret |= fsm_reg_write(fsm_dev, REG(FS1758_SPKTCFGL), value);
	fsm_dev->state.calibrated = true;

	return ret;
}

int fs1758_reg_init(fsm_dev_t *fsm_dev)
{
	fsm_config_t *cfg = fsm_get_config();
	int ret;

	if (fsm_dev == NULL || cfg == NULL) {
		return -EINVAL;
	}

	ret = fs1758_i2c_reset(fsm_dev);
	ret |= fsm_reg_write(fsm_dev, REG(FS1758_SPKCOEF), (fsm_dev->tcoef << 1));
	ret |= fsm_write_reg_tbl(fsm_dev, FSM_SCENE_COMMON);
	ret |= fsm_write_reg_tbl(fsm_dev, cfg->next_scene);
	fsm_dev->cur_scene = cfg->next_scene;
	if (ret) {
		pr_addr(err, "init failed:%d", ret);
		fsm_dev->state.dev_inited = false;
	}

	return ret;
}

int fs1758_start_up(fsm_dev_t *fsm_dev)
{
	int ret = 0;

	if (!fsm_dev) {
		return -EINVAL;
	}
	if (fsm_dev->revid == 0x0011) {
		ret |= fsm_reg_write(fsm_dev, REG(FS1758_BSGCTRL), 0x9433);
	}
		ret |= fs1758_set_prot(fsm_dev);
	// 0x1100EF; all enable
	ret |= fsm_reg_write(fsm_dev, REG(FS1758_SYSCTRL), 0x008F);
	// 0x100000; power up
	ret |= fsm_reg_write(fsm_dev, REG(FS1758_PWRCTRL), 0x0000);
	fsm_dev->amp_on = true;
	// fsm_delay_ms(10);
	fsm_dev->errcode = ret;

	return ret;
}

int fs1758_set_mute(fsm_dev_t *fsm_dev, int mute)
{
	int ret = 0;

	if (mute) {
		ret = fsm_set_bf(fsm_dev, FS1758_LNMODE, 0);
	}

	return ret;
}

int fs1758_shut_down(fsm_dev_t *fsm_dev)
{
	int ret;

	if (fsm_dev == NULL) {
		return -EINVAL;
	}
	fsm_dev->amp_on = false;
	ret = fsm_reg_write(fsm_dev, REG(FS1758_PWRCTRL), 0x0001);
	ret |= fsm_reg_write(fsm_dev, REG(FS1758_SYSCTRL), 0x0000);
	fsm_delay_ms(10);

	return ret;
}

int fs1758_pre_calib(fsm_dev_t *fsm_dev)
{
	fsm_config_t *cfg = fsm_get_config();
	reg_temp_t sysctrl;
	uint16_t bstctrl;
	uint16_t tsctrl;
	int force;
	int ret;

	if (fsm_dev == NULL || !cfg) {
		return -EINVAL;
	}
	force = cfg->force_calib;
	pr_addr(info, "force:%d, calibrated:%d",
			force, fsm_dev->state.calibrated);
	if (!force && fsm_dev->state.calibrated) {
		pr_addr(info, "already calibrated");
		return 0;
	}
	fsm_dev->state.calibrated = false;
	fsm_dev->re25 = 0;
	ret = fsm_set_bf(fsm_dev, FS1758_LNMODE, 0);
	// need amplifier off
	ret = fsm_reg_read(fsm_dev, REG(FS1758_SYSCTRL), &sysctrl.new_val);
	sysctrl.old_val = set_bf_val(&sysctrl.new_val, FS1758_AMPEN, 0);
	ret |= fsm_reg_write(fsm_dev, REG(FS1758_SYSCTRL), sysctrl.new_val);
	// wait stable
	fsm_delay_ms(20);
	ret |= fsm_access_key(fsm_dev, 1);
	ret |= fsm_reg_read(fsm_dev, REG(FS1758_BSTCTRL), &bstctrl);
	set_bf_val(&bstctrl, FS1758_VOUTSEL, 0xE); // 7.5V
	set_bf_val(&bstctrl, FS1758_BSTMODE, 2); // adp mode
	ret |= fsm_reg_write(fsm_dev, REG(FS1758_BSTCTRL), bstctrl);
	ret |= fsm_set_bf(fsm_dev, FS1758_CALIBEN, 1);
	ret |= fsm_set_bf(fsm_dev, FS1758_BSGEN, 0);
	ret |= fsm_reg_write(fsm_dev, 0x8C, 0x0000);
	ret |= fsm_reg_write(fsm_dev, 0x8D, 0x0000);
	ret |= fsm_reg_write(fsm_dev, 0x8E, 0x0000);
	ret |= fsm_reg_write(fsm_dev, 0x88, 0xE000);
	ret |= fsm_reg_write(fsm_dev, 0x89, 0x0031);
	ret |= fsm_access_key(fsm_dev, 0);
	// recover sysctrl
	ret |= fsm_reg_write(fsm_dev, REG(FS1758_SYSCTRL), sysctrl.old_val);
	fsm_delay_ms(10);
	ret |= fsm_reg_multiread(fsm_dev, REG(FS1758_TSCTRL), &tsctrl);
	if (get_bf_val(FS1758_OFFSTA, tsctrl)
				&& get_bf_val(FS1758_TSEN, tsctrl)) {
		// disable ts first
		ret |= fsm_reg_write(fsm_dev, REG(FS1758_TSCTRL), 0x7000);
	}
	ret |= fsm_reg_write(fsm_dev, REG(FS1758_TSCTRL), 0x1008); // ts on
	// fsm_reg_dump(fsm_dev);
	fsm_dev->cur_scene = 0; // clear scene
	fsm_dev->state.re25_runin = true;
	fsm_dev->errcode = ret;
	if (ret) {
		pr_addr(err, "error:%d, stop test", ret);
		fsm_dev->state.re25_runin = false;
	}

	return ret;
}

int fs1758_cal_zmdata(fsm_dev_t *fsm_dev)
{
	uint16_t zmdata;
	int re25;
	int ret;

	if (!fsm_dev) {
		return -EINVAL;
	}
	if (!fsm_dev->state.re25_runin) {
		return 0;
	}
	ret = fsm_reg_multiread(fsm_dev, REG(FS1758_SPKTEMP), &zmdata);
	if (ret) {
		pr_addr(err, "get zmdata fail:%d", ret);
		return -EINVAL;
	}
	if (zmdata == 0 || zmdata == 0xFFFF) {
		pr_addr(warning, "invalid data:%04X, skip", zmdata);
		fsm_dev->re25 = (zmdata == 0 ? 65535 : 0);
		return -EINVAL;
	}
	re25 = fsm_cal_spkr_zmimp(fsm_dev, zmdata);
	if (fsm_dev->rapp > 0) {
		re25 -= fsm_dev->rapp;
	}
	if (fsm_dev->re25 <= 0 || fsm_dev->re25 > re25) {
		fsm_dev->re25 = re25;
	}
	pr_addr(info, "re25:%d-%d, rapp:%d",
			fsm_dev->re25, re25, fsm_dev->rapp);

	return -EINVAL;
}

int fs1758_post_calib(fsm_dev_t *fsm_dev)
{
	fsm_config_t *cfg = fsm_get_config();
	int ret = 0;

	if (fsm_dev == NULL || !cfg || fsm_dev->tdata == NULL) {
		return -EINVAL;
	}
	if (!fsm_dev->state.re25_runin) {
		pr_addr(info, "invalid running state");
		return 0;
	}
	fsm_dev->state.re25_runin = false;
	ret |= fsm_set_bf(fsm_dev, FS1758_CALIBEN, 0);
	fsm_dev->errcode |= ret;

	FSM_FUNC_EXIT(ret);
	return ret;
}

int fs1758_pre_f0_test(fsm_dev_t *fsm_dev)
{
	uint16_t value;
	int retry = 0;
	int ret;

	if (!fsm_dev) {
		return -EINVAL;
	}
	fsm_dev->f0 = 0;
	fsm_dev->state.f0_runing = true;
	fsm_dev->errcode = 0;
	if (fsm_dev->tdata) {
		memset(&fsm_dev->tdata->f0, 0, sizeof(struct f0_data));
	}
pre_f0_start:
	// disable ts
	ret = fsm_reg_write(fsm_dev, REG(FS1758_TSCTRL), 0x7623);
	fsm_delay_ms(35);
	// i2c reset
	ret = fs1758_i2c_reset(fsm_dev);
	ret |= fsm_reg_write(fsm_dev, REG(FS1758_SYSCTRL), 0x0001);
	ret |= fsm_access_key(fsm_dev, 1);
	// DC/DC mode: Boost; Vo=8.5V; Ilim: 4.2A
	ret |= fsm_reg_write(fsm_dev, REG(FS1758_BSTCTRL), 0x1851);
	// PTDAC configuration to boost pilot amplitude to ~300mVrms
	ret |= fsm_reg_write(fsm_dev, REG(FS1758_PDCTRL), 0x003F);
	ret |= fsm_reg_write(fsm_dev, REG(FS1758_TSCTRL), 0x1008);
	ret |= fsm_reg_write(fsm_dev, 0xC6, 0x0100);
	ret |= fsm_reg_write(fsm_dev, 0x86, 0x0000);
	ret |= fsm_reg_write(fsm_dev, 0x82, 0xDBFA);
	ret |= fsm_reg_write(fsm_dev, 0x83, 0x0001);
	ret |= fsm_reg_write(fsm_dev, 0x82, 0x2AFD);
	ret |= fsm_reg_write(fsm_dev, 0x83, 0x000F);
	ret |= fsm_reg_write(fsm_dev, 0x82, 0xDBFA);
	ret |= fsm_reg_write(fsm_dev, 0x83, 0x0001);
	ret |= fsm_reg_write(fsm_dev, 0x82, 0xE92A);
	ret |= fsm_reg_write(fsm_dev, 0x83, 0x0006);
	ret |= fsm_reg_write(fsm_dev, 0x82, 0x17AC);
	ret |= fsm_reg_write(fsm_dev, 0x83, 0x000D);
	ret |= fsm_reg_write(fsm_dev, 0x82, 0x6a3e);
	ret |= fsm_reg_write(fsm_dev, 0x83, 0x0007);
	ret |= fsm_reg_write(fsm_dev, 0x82, 0x809b);
	ret |= fsm_reg_write(fsm_dev, 0x83, 0x0002);
	ret |= fsm_reg_write(fsm_dev, 0x82, 0xc1c5);
	ret |= fsm_reg_write(fsm_dev, 0x83, 0x000e);
	ret |= fsm_reg_write(fsm_dev, 0x82, 0xae6a);
	ret |= fsm_reg_write(fsm_dev, 0x83, 0x000a);
	ret |= fsm_reg_write(fsm_dev, 0x82, 0xe30a);
	ret |= fsm_reg_write(fsm_dev, 0x83, 0x000f);

	ret |= fsm_reg_write(fsm_dev, 0x4D, 0x1302);
	ret |= fsm_reg_write(fsm_dev, 0x89, 0x0012);
	ret |= fsm_reg_write(fsm_dev, 0x88, 0xA000);
	ret |= fsm_reg_write(fsm_dev, 0x80, 0x1700);
	// power on chip
	fsm_reg_write(fsm_dev, REG(FS1758_PWRCTRL), 0x0000);
	// power on function block
	ret |= fsm_reg_write(fsm_dev, REG(FS1758_SYSCTRL), 0x008F);
	fsm_delay_ms(35);
	ret |= fsm_reg_write(fsm_dev, 0x4D, 0xD302);
	ret |= fsm_access_key(fsm_dev, 0);
	fsm_delay_ms(50);
	ret |= fsm_get_bf(fsm_dev, 0x0F4E, &value);
	if (value == 0 && retry++ < 3) {
		pr_addr(info, "retry times:%d", retry);
		goto pre_f0_start;
	}
	fsm_dev->errcode = ret;

	return ret;
}

int fs1758_f0_test(fsm_dev_t *fsm_dev)
{
	fsm_config_t *cfg = fsm_get_config();
	uint16_t value;
	int f0;
	int ret;

	if (!fsm_dev || !cfg) {
		return -EINVAL;
	}
	if (fsm_dev->f0 != 0) {
		return 0;
	}
	ret = fsm_reg_multiread(fsm_dev, 0x4E, &value);
	// pr_addr(info, "wait status:%04X", value);
	if (!ret && (value & 0x8000) == 0) { // bit[15] = 0
		f0 = 48 * ((value >> 8) & 0x003F) + 192;
		if (fsm_dev->tdata) {
			fsm_dev->tdata->test_f0 = f0;
		}
		fsm_dev->f0 = f0;
		pr_addr(info, "F0(Hz):%d", f0);
		// disalbe f0 test
		ret |= fsm_reg_write(fsm_dev, 0x4D, 0x1302);
	} else if (!ret && (value & 0x8000) != 0) {
		// not ready yet
		return -EINVAL;
	}
	fsm_dev->errcode |= ret;

	FSM_ADDR_EXIT(ret);
	return ret;
}

int fs1758_post_f0_test(fsm_dev_t *fsm_dev)
{
	int ret = 0;

	if (!fsm_dev) {
		return -EINVAL;
	}
	// clear SPKCOEF for retry init
	ret |= fsm_reg_write(fsm_dev, REG(FS1758_SPKCOEF), 0x0000);
	ret |= fsm_reg_write(fsm_dev, REG(FS1758_TSCTRL), 0x7000);
	fsm_delay_ms(30);
	fs1758_shut_down(fsm_dev);
	fsm_dev->state.f0_runing = false;
	fsm_dev->errcode |= ret;

	return ret;
}

int fs1758_get_livedata(fsm_dev_t *fsm_dev, int *data)
{
	int amb_data[2] = { 0 };
	int ret = 0;

	if (fsm_dev == NULL || data == NULL) {
		return -EINVAL;
	}
	ret = fsm_get_amb_tempr(amb_data);
	if (ret) {
		pr_addr(err, "get amb data fail:%d", ret);
	}
	data[0] = fsm_dev->re25 * 4; // R0 x4096
	data[1] = amb_data[0] * 4096; // T0 x4096
	data[2] = amb_data[1]; // Vbat x1000
	data[3] = fsm_dev->f0 * 256; // F0 x256
	data[4] = 2; // Q
	data[5] = 0; // rsvd
	pr_addr(info, "R0:%d,T0:%d,Vbat:%d,F0:%d",
			data[0], data[1], data[2], data[3]);

	return ret;
}

void fs1758_ops(fsm_dev_t *fsm_dev)
{
	fsm_config_t *cfg = fsm_get_config();

	if (!fsm_dev || !cfg) {
		return;
	}

	fsm_set_fw_name(FS1758_FW_NAME);
	fsm_dev->dev_ops.reg_init = fs1758_reg_init;
	fsm_dev->dev_ops.start_up = fs1758_start_up;
	fsm_dev->dev_ops.set_mute = fs1758_set_mute;
	fsm_dev->dev_ops.shut_down = fs1758_shut_down;
	fsm_dev->dev_ops.pre_calib = fs1758_pre_calib;
	fsm_dev->dev_ops.cal_zmdata = fs1758_cal_zmdata;
	fsm_dev->dev_ops.post_calib = fs1758_post_calib;
	fsm_dev->dev_ops.pre_f0_test = fs1758_pre_f0_test;
	fsm_dev->dev_ops.f0_test = fs1758_f0_test;
	fsm_dev->dev_ops.post_f0_test = fs1758_post_f0_test;
	fsm_dev->dev_ops.get_livedata = fs1758_get_livedata;
	fsm_dev->compat.RS2RL_RATIO = FS1758_RS2RL_RATIO;
	cfg->nondsp_mode = false;
	cfg->store_otp = false;
}
#endif
