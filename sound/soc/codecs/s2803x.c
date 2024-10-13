/*
 * Copyright (c) 2015 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/workqueue.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/pm_runtime.h>
#include <linux/modem_notifier.h>

#include <mach/regs-pmu-exynos3475.h>

#include <sound/exynos.h>
#include <sound/jack.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>

#include <sound/exynos_regmap_fw.h>
#include <sound/s2803x.h>
#include "s2803x.h"

/*
 * The sysclk is derived from the audio PLL. The value of the PLL is not always
 * rounded, many times the actual rate is a little bit higher or less than the
 * rounded value.
 *
 * 'clk_set_rate' operation tries to set the given rate or the next lower
 * possible value. Thus if the PLL rate is slightly higher than rounded value,
 * we won't get the given rate through a direct division. This will result in
 * getting a lower clock rate. Asking for a slightly higher clock rate will
 * result in setting appropriate clock rate.
 *
 * The value '100' is a heuristic value and there is no clear rule to derive
 * this value.
 */
#define S2803X_SYS_CLK_FREQ_48KHZ	(24576000U + 100)
#define S2803X_SYS_CLK_FREQ_192KHZ	(49152000U + 100)

#define S2803X_SAMPLE_RATE_48KHZ	48000
#define S2803X_SAMPLE_RATE_192KHZ	192000

#define S2803X_SAMPLE_RATE_8KHZ		8000
#define S2803X_SAMPLE_RATE_16KHZ	16000

#define S2803_ALIVE_ON			0
#define S2803_ALIVE_OFF			1

#define S2803X_RPM_SUSPEND_DELAY_MS	(500)

#define S2803X_FIRMWARE_NAME	"cod3025x-s2803x-aud-fw.bin"

#ifdef CONFIG_SND_SOC_SAMSUNG_VERBOSE_DEBUG
#ifdef dev_dbg
#undef dev_dbg
#endif
#define dev_dbg dev_err
#endif

static void s2803x_cfg_gpio(struct device *dev, const char *name);

enum s2803x_type {
	SRC2803X,
};

static unsigned int s2803x_save_regs_addr[] = {
	S2803X_REG_0A_HQ_CTL,
	S2803X_REG_0D_RMIX_CTL,
	S2803X_REG_0F_DIG_EN,
	S2803X_REG_10_DMIX1,
	S2803X_REG_11_DMIX2,
	S2803X_REG_16_DOUTMX1,
	S2803X_REG_17_DOUTMX2,
};

struct s2803x_priv {
	struct regmap *regmap;
	struct snd_soc_codec *codec;
	struct device *dev;
	void __iomem *regs;
	struct clk *aclk_audmixer;
	struct clk *sclk_audmixer;
	struct clk *sclk_audmixer_bclk0;
	struct clk *sclk_audmixer_bclk1;
	struct clk *sclk_audmixer_bclk2;
	struct clk *dout_audmixer;
	void *sysreg_reset;
	unsigned int sysreg_reset_bit;
	void *sysreg_i2c_id;
	unsigned short i2c_addr;
	atomic_t is_cp_running;
	atomic_t num_active_stream;
	atomic_t use_count_bt;
	struct pinctrl *pinctrl;
	unsigned int *save_regs_val;
	unsigned int aifrate;
	bool is_bck4_mcko_enabled;
	bool is_alc_enabled;
	unsigned long cp_event;
	struct work_struct cp_notification_work;
	struct workqueue_struct *mixer_cp_wq;
};

struct s2803x_priv *s2803x;

/**
 * Return value:
 * true: if the register value cannot be cached, hence we have to read from the
 * register directly
 * false: if the register value can be read from cache
 */
static bool s2803x_volatile_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case S2803X_REG_00_SOFT_RSTB:
		return true;
	default:
		return false;
	}
}

/**
 * Return value:
 * true: if the register value can be read
 * flase: if the register cannot be read
 */
static bool s2803x_readable_register(struct device *dev, unsigned int reg)
{
	if (reg % 4)
		return false;

	switch (reg) {
	case S2803X_REG_00_SOFT_RSTB ... S2803X_REG_11_DMIX2:
	case S2803X_REG_16_DOUTMX1 ... S2803X_REG_17_DOUTMX2:
	case S2803X_REG_68_ALC_CTL ... S2803X_REG_72_ALC_SGR:
		return true;
	default:
		return false;
	}
}

/**
 * Return value:
 * true: if the register value can be modified
 * flase: if the register value cannot be modified
 */
static bool s2803x_writeable_register(struct device *dev, unsigned int reg)
{
	if (reg % 4)
		return false;

	switch (reg) {
	case S2803X_REG_00_SOFT_RSTB ... S2803X_REG_11_DMIX2:
	case S2803X_REG_16_DOUTMX1 ... S2803X_REG_17_DOUTMX2:
	case S2803X_REG_68_ALC_CTL ... S2803X_REG_72_ALC_SGR:
		return true;
	default:
		return false;
	}
}
static const struct regmap_config s2803x_regmap = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,

	.max_register = S2803X_MAX_REGISTER,
	.volatile_reg = s2803x_volatile_register,
	.readable_reg = s2803x_readable_register,
	.writeable_reg = s2803x_writeable_register,
	.cache_type = REGCACHE_NONE,
};

static int s2803x_reset_sys_data(void)
{
	regmap_write(s2803x->regmap, S2803X_REG_00_SOFT_RSTB, 0x0);

	msleep(1);

	regmap_write(s2803x->regmap, S2803X_REG_00_SOFT_RSTB,
			BIT(SOFT_RSTB_DATA_RSTB_SHIFT) |
			BIT(SOFT_RSTB_SYS_RSTB_SHIFT));

	msleep(1);

	return 0;
}

static void s2803x_reset_data(void)
{
	/* Data reset sequence, toggle bit 1 */
	regmap_update_bits(s2803x->regmap, S2803X_REG_00_SOFT_RSTB,
			BIT(SOFT_RSTB_DATA_RSTB_SHIFT),
			0x0);

	regmap_update_bits(s2803x->regmap, S2803X_REG_00_SOFT_RSTB,
			BIT(SOFT_RSTB_DATA_RSTB_SHIFT),
			BIT(SOFT_RSTB_DATA_RSTB_SHIFT));
}

static int s2803x_init_mixer(void)
{
	/**
	 * Set default configuration for AP/CP/BT interfaces
	 *
	 * BCLK = 32fs
	 * LRCLK polarity normal
	 * I2S data format is in I2S standard
	 * I2S data length is 16 bits per sample
	 */
	regmap_write(s2803x->regmap, S2803X_REG_02_IN1_CTL2,
			I2S_XFS_32FS << INCTL2_I2S_XFS_SHIFT |
			LRCLK_POL_LEFT << INCTL2_LRCK_POL_SHIFT |
			I2S_DF_I2S << INCTL2_I2S_DF_SHIFT |
			I2S_DL_16BIT << INCTL2_I2S_DL_SHIFT);

	regmap_write(s2803x->regmap, S2803X_REG_05_IN2_CTL2,
			I2S_XFS_32FS << INCTL2_I2S_XFS_SHIFT |
			LRCLK_POL_LEFT << INCTL2_LRCK_POL_SHIFT |
			I2S_DF_I2S << INCTL2_I2S_DF_SHIFT |
			I2S_DL_16BIT << INCTL2_I2S_DL_SHIFT);

	/* BT Configuration Initialisation */
	/* I2s mode - Mixer Slave - 32 BCK configuration*/
	regmap_write(s2803x->regmap, S2803X_REG_07_IN3_CTL1,
			MIXER_SLAVE << INCTL1_MASTER_SHIFT |
			MPCM_SLOT_32BCK << INCTL1_MPCM_SLOT_SHIFT |
			I2S_PCM_MODE_I2S << INCTL1_I2S_PCM_SHIFT);

	/* 32xfs - i2s format 16bit */
	regmap_write(s2803x->regmap, S2803X_REG_08_IN3_CTL2,
			I2S_XFS_32FS << INCTL2_I2S_XFS_SHIFT |
			LRCLK_POL_LEFT << INCTL2_LRCK_POL_SHIFT |
			I2S_DF_I2S << INCTL2_I2S_DF_SHIFT |
			I2S_DL_16BIT << INCTL2_I2S_DL_SHIFT);

	/*
	 * Below setting only requird for PCM mode, but it has no impact for I2S
	 * mode.
	 */
	/* 0 delay, pcm short frme sync */
	regmap_write(s2803x->regmap, S2803X_REG_09_IN3_CTL3,
			PCM_DAD_0BCK << INCTL3_PCM_DAD_SHIFT |
			PCM_DF_SHORT_FRAME << INCTL3_PCM_DF_SHIFT);

	/* SLOT_L - 1st slot */
	regmap_write(s2803x->regmap, S2803X_REG_0B_SLOT_L,
			SLOT_SEL_1ST_SLOT << SLOT_L_SEL_SHIFT);

	/* SLOT_R - 2nd slot */
	regmap_write(s2803x->regmap, S2803X_REG_0C_SLOT_R,
			SLOT_SEL_2ND_SLOT << SLOT_R_SEL_SHIFT);

	/* T - Slots 2 slots used */
	regmap_write(s2803x->regmap, S2803X_REG_0E_TSLOT,
			TSLOT_USED_2 << TSLOT_SLOT_SHIFT);

	/**
	 * BCK4 output is normal BCK for Universal board, the clock output goes
	 * to voice processor. It should be MCLK for SMDK board, as the clock
	 * output goes to codec as MCLK.
	 */
	if (s2803x->is_bck4_mcko_enabled)
		regmap_write(s2803x->regmap, S2803X_REG_0A_HQ_CTL,
				BIT(HQ_CTL_MCKO_EN_SHIFT) |
				BIT(HQ_CTL_BCK4_MODE_SHIFT));
	else
		regmap_write(s2803x->regmap, S2803X_REG_0A_HQ_CTL,
				BIT(HQ_CTL_MCKO_EN_SHIFT));

	/* Enable digital mixer */
	regmap_write(s2803x->regmap, S2803X_REG_0F_DIG_EN,
			BIT(DIG_EN_MIX_EN_SHIFT));

	/* Reset DATA path */
	s2803x_reset_data();

	return 0;
}

int s2803x_hw_params(struct snd_pcm_substream *substream,
			struct snd_pcm_hw_params *params,
			int bfs, int interface)
{
	int xfs, dl_bit;
	unsigned int hq_mode;
	unsigned int sys_clk_freq;
	unsigned int mpcm_rate;
	unsigned int aifrate;
	int ret;

	if (s2803x == NULL)
		return -EINVAL;

	dev_dbg(s2803x->dev, "(%s) %s called for aif%d\n",
			substream->stream ? "C" : "P", __func__, interface);

	/* Only I2S_DL_16BIT is verified now */
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_U24:
	case SNDRV_PCM_FORMAT_S24:
		dl_bit = I2S_DL_24BIT;
		break;

	case SNDRV_PCM_FORMAT_U16_LE:
	case SNDRV_PCM_FORMAT_S16_LE:
		dl_bit = I2S_DL_16BIT;
		break;

	default:
		dev_err(s2803x->dev, "%s: Unsupported format\n", __func__);
		return -EINVAL;
	}

	/* Only I2S_XFS_32FS is verified now */
	switch (bfs) {
	case 32:
		xfs = I2S_XFS_32FS;
		break;

	case 48:
		xfs = I2S_XFS_48FS;
		break;

	case 64:
		xfs = I2S_XFS_64FS;
		break;

	default:
		dev_err(s2803x->dev, "%s: Unsupported bfs (%d)\n",
				__func__, bfs);
		return -EINVAL;
	}

	switch (interface) {
	case S2803X_IF_AP:
		ret = regmap_update_bits(s2803x->regmap,
			S2803X_REG_02_IN1_CTL2,
			(INCTL2_I2S_XFS_MASK << INCTL2_I2S_XFS_SHIFT) |
			(INCTL2_I2S_DL_MASK << INCTL2_I2S_DL_SHIFT),
			(xfs << INCTL2_I2S_XFS_SHIFT) | dl_bit);

		aifrate = params_rate(params);
		/* Change the clock only when switching the sample rate */
		if (s2803x->aifrate == aifrate)
			break;

		switch (aifrate) {
		case S2803X_SAMPLE_RATE_192KHZ:
			sys_clk_freq = S2803X_SYS_CLK_FREQ_192KHZ;
			hq_mode = HQ_CTL_HQ_EN_MASK;
			break;
		case S2803X_SAMPLE_RATE_48KHZ:
			sys_clk_freq = S2803X_SYS_CLK_FREQ_48KHZ;
			hq_mode = 0;
			break;
		default:
			dev_err(s2803x->dev,
					"%s: Unsupported sample-rate (%u)\n",
					__func__, aifrate);
			return -EINVAL;
		}

		regmap_update_bits(s2803x->regmap, S2803X_REG_0A_HQ_CTL,
				HQ_CTL_HQ_EN_MASK, hq_mode);

		ret = clk_set_rate(s2803x->dout_audmixer, sys_clk_freq);
		if (ret != 0) {
			dev_err(s2803x->dev,
				"%s: Error setting mixer sysclk rate as %u\n",
				__func__, sys_clk_freq);
			return ret;
		}

		s2803x->aifrate = aifrate;
		break;

	case S2803X_IF_CP:
		ret = regmap_update_bits(s2803x->regmap,
			S2803X_REG_05_IN2_CTL2,
			(INCTL2_I2S_XFS_MASK << INCTL2_I2S_XFS_SHIFT) |
			(INCTL2_I2S_DL_MASK << INCTL2_I2S_DL_SHIFT),
			(xfs << INCTL2_I2S_XFS_SHIFT) | dl_bit);
		break;

	case S2803X_IF_BT:
		ret = regmap_update_bits(s2803x->regmap,
			S2803X_REG_08_IN3_CTL2,
			(INCTL2_I2S_XFS_MASK << INCTL2_I2S_XFS_SHIFT) |
			(INCTL2_I2S_DL_MASK << INCTL2_I2S_DL_SHIFT),
			(xfs << INCTL2_I2S_XFS_SHIFT) | dl_bit);

		/*
		 * Sample rate setting only requird for PCM master mode, but the
		 * below configuration have no impact in I2S mode.
		 */
		aifrate = params_rate(params);

		switch (aifrate) {
		case S2803X_SAMPLE_RATE_8KHZ:
			mpcm_rate = MPCM_SRATE_8KHZ;
			break;
		case S2803X_SAMPLE_RATE_16KHZ:
			mpcm_rate = MPCM_SRATE_16KHZ;
			break;
		default:
			dev_err(s2803x->dev,
					"%s: Unsupported BT samplerate (%d)\n",
					__func__, aifrate);
			return -EINVAL;
		}

		ret = regmap_update_bits(s2803x->regmap,
			S2803X_REG_07_IN3_CTL1,
			(INCTL1_MPCM_SRATE_MASK << INCTL1_MPCM_SRATE_SHIFT),
			(mpcm_rate << INCTL1_MPCM_SRATE_SHIFT));

		break;

	default:
		dev_err(s2803x->dev, "%s: Unsupported interface (%d)\n",
				__func__, interface);
		return -EINVAL;
	}

	/* Reset the data path only current substream is active */
	if (atomic_read(&s2803x->num_active_stream) == 1)
		s2803x_reset_data();

	return 0;
}
EXPORT_SYMBOL_GPL(s2803x_hw_params);

void s2803x_startup(s2803x_if_t interface)
{
	if (s2803x == NULL)
		return;

	dev_dbg(s2803x->dev, "aif%d: %s called\n", interface, __func__);

	/*
	 * Runtime resume sequence internally checks is_cp_running variable for
	 * CP call mode. If the value of is_cp_running variable is non-zero, the
	 * system assumes that the it is resuming from CP call mode and skips
	 * the power-domain powering on sequence.
	 *
	 * If this variable is incremented before pm_runtime_get_sync() is
	 * called, the framework won't power on the power-domain even though the
	 * call was made during the start of a call.
	 *
	 * Hence, this sequence of pm_runtime_get_sync(s2803->dev) and
	 * atomic_inc(&s2803x->is_cp_running) must not be changed.
	 */


#ifdef CONFIG_PM_RUNTIME
	pm_runtime_get_sync(s2803x->dev);
#endif

	atomic_inc(&s2803x->num_active_stream);

	if ((interface == S2803X_IF_BT) || (interface == S2803X_IF_CP))
		atomic_inc(&s2803x->is_cp_running);

	if (interface == S2803X_IF_BT) {
		atomic_inc(&s2803x->use_count_bt);
		if (atomic_read(&s2803x->use_count_bt) == 1)
			s2803x_cfg_gpio(s2803x->dev, "default");
	}
}
EXPORT_SYMBOL_GPL(s2803x_startup);

void s2803x_shutdown(s2803x_if_t interface)
{
	if (s2803x == NULL)
		return;

	dev_dbg(s2803x->dev, "aif%d: %s called\n", interface, __func__);

	atomic_dec(&s2803x->num_active_stream);

	if (interface == S2803X_IF_BT) {
		atomic_dec(&s2803x->use_count_bt);
		if (atomic_read(&s2803x->use_count_bt) == 0)
			s2803x_cfg_gpio(s2803x->dev, "bt-idle");
	}

	if ((interface == S2803X_IF_BT) || (interface == S2803X_IF_CP))
		atomic_dec(&s2803x->is_cp_running);

#ifdef CONFIG_PM_RUNTIME
	pm_runtime_put_sync(s2803x->dev);
#endif
}
EXPORT_SYMBOL_GPL(s2803x_shutdown);

/**
 * is_cp_aud_enabled(void): Checks the current status of CP path
 *
 * Returns true if CP audio path is enabled, false otherwise.
 */
bool is_cp_aud_enabled(void)
{
	if (s2803x == NULL)
		return false;

	if (atomic_read(&s2803x->is_cp_running))
		return true;
	else
		return false;
}
EXPORT_SYMBOL_GPL(is_cp_aud_enabled);

void aud_mixer_MCLKO_enable(void)
{
	if (s2803x == NULL)
		return;

	atomic_inc(&s2803x->is_cp_running);

#ifdef CONFIG_PM_RUNTIME
	pm_runtime_get_sync(s2803x->dev);
#endif

}
EXPORT_SYMBOL_GPL(aud_mixer_MCLKO_enable);

void aud_mixer_MCLKO_disable(void)
{
	if (s2803x == NULL)
		return;

	atomic_dec(&s2803x->is_cp_running);

#ifdef CONFIG_PM_RUNTIME
	pm_runtime_put_sync(s2803x->dev);
#endif

}
EXPORT_SYMBOL_GPL(aud_mixer_MCLKO_disable);

/* thread run whenever the cp event received */
static void s2803x_cp_notification_work(struct work_struct *work)
{
	struct s2803x_priv *s2803x =
		container_of(work, struct s2803x_priv, cp_notification_work);
	enum modem_event event = s2803x->cp_event;

	if ((event == MODEM_EVENT_EXIT ||
		event == MODEM_EVENT_RESET || event == MODEM_EVENT_WATCHDOG)) {

		/*
		 * Get runtime PM, To keep the clocks enabled untill below
		 * write executes.
		 */
#ifdef CONFIG_PM_RUNTIME
		pm_runtime_get_sync(s2803x->dev);
#endif
		regmap_update_bits(s2803x->regmap, S2803X_REG_10_DMIX1,
						DMIX1_MIX_EN2_MASK, 0);

#ifdef CONFIG_PM_RUNTIME
		pm_runtime_put_sync(s2803x->dev);
#endif
		dev_dbg(s2803x->dev, "cp dmix path disabled\n");
	}
}

static int s2803x_cp_notification_handler(struct notifier_block *nb,
				unsigned long action, void *data)
{

	dev_dbg(s2803x->dev, "%s called, event = %ld\n", __func__,
							action);

	if (!is_cp_aud_enabled()) {
		dev_dbg(s2803x->dev, "Mixer not active, Exiting..\n");
		return 0;
	}

	s2803x->cp_event = action;
	queue_work(s2803x->mixer_cp_wq, &s2803x->cp_notification_work);

	return 0;
}

struct notifier_block s2803x_cp_nb = {
		.notifier_call = s2803x_cp_notification_handler,
};

/**
 * TLV_DB_SCALE_ITEM
 *
 * (TLV: Threshold Limit Value)
 *
 * For various properties, the dB values don't change linearly with respect to
 * the digital value of related bit-field. At most, they are quasi-linear,
 * that means they are linear for various ranges of digital values. Following
 * table define such ranges of various properties.
 *
 * TLV_DB_RANGE_HEAD(num)
 * num defines the number of linear ranges of dB values.
 *
 * s0, e0, TLV_DB_SCALE_ITEM(min, step, mute),
 * s0: digital start value of this range (inclusive)
 * e0: digital end valeu of this range (inclusive)
 * min: dB value corresponding to s0
 * step: the delta of dB value in this range
 * mute: ?
 *
 * Example:
 *	TLV_DB_RANGE_HEAD(3),
 *	0, 1, TLV_DB_SCALE_ITEM(-2000, 2000, 0),
 *	2, 4, TLV_DB_SCALE_ITEM(1000, 1000, 0),
 *	5, 6, TLV_DB_SCALE_ITEM(3800, 8000, 0),
 *
 * The above code has 3 linear ranges with following digital-dB mapping.
 * (0...6) -> (-2000dB, 0dB, 1000dB, 2000dB, 3000dB, 3800dB, 4600dB),
 *
 * DECLARE_TLV_DB_SCALE
 *
 * This macro is used in case where there is a linear mapping between
 * the digital value and dB value.
 *
 * DECLARE_TLV_DB_SCALE(name, min, step, mute)
 *
 * name: name of this dB scale
 * min: minimum dB value corresponding to digital 0
 * step: the delta of dB value
 * mute: ?
 *
 * NOTE: The information is mostly for user-space consumption, to be viewed
 * alongwith amixer.
 */

/**
 * DONE
 * s2803x_rmix_tlv
 *
 * Range:
 * 0dB, -2.87dB, -6.02dB, -9.28dB, -12.04dB, -14.54dB, -18.06dB, -20.56dB
 *
 * This range is used for following controls
 * RMIX1_LVL, reg(0x0d), shift(0), width(3), invert(1), max(7)
 * RMIX2_LVL, reg(0x0d), shift(4), width(3), invert(1), max(7)
 * MIX1_LVL,  reg(0x10), shift(0), width(3), invert(1), max(7)
 * MIX2_LVL,  reg(0x10), shift(4), width(3), invert(1), max(7)
 * MIX3_LVL,  reg(0x11), shift(0), width(3), invert(1), max(7)
 * MIX4_LVL,  reg(0x11), shift(4), width(3), invert(1), max(7)
 */
static const unsigned int s2803x_mix_tlv[] = {
	TLV_DB_RANGE_HEAD(4),
	0x0, 0x1, TLV_DB_SCALE_ITEM(0, 287, 0),
	0x2, 0x3, TLV_DB_SCALE_ITEM(602, 326, 0),
	0x4, 0x5, TLV_DB_SCALE_ITEM(1204, 250, 0),
	0x6, 0x7, TLV_DB_SCALE_ITEM(1806, 250, 0),
};

/**
 * s2803x_alc_ng_hys_tlv
 *
 * Range: 3dB to 12dB, step 3dB
 *
 * ALC_NG_HYS, reg(0x68), shift(6), width(2), invert(0), max(31)
 */
static const DECLARE_TLV_DB_SCALE(s2803x_alc_ng_hys_tlv, 300, 300, 0);

/**
 * s2803x_alc_max_gain_tlv
 *
 * Range:
 * 0x6c to 0x9c => 0dB to 24dB, step 0.5dB
 *
 * ALC_MAX_GAIN,     reg(0x69), shift(0), width(8), min(0x6c), max(0x9c)
 * ALC_START_GAIN_L, reg(0x71), shift(0), width(8), min(0x6c), max(0x9c)
 * ALC_START_GAIN_R, reg(0x72), shift(0), width(8), min(0x6c), max(0x9c)
 */
static const DECLARE_TLV_DB_SCALE(s2803x_alc_max_gain_tlv, 0, 50, 0);

/**
 * s2803x_alc_min_gain_tlv
 *
 * Range:
 * 0x00 to 0x6c => -54dB to 0dB, step 0.5dB
 *
 * ALC_MIN_GAIN, reg(0x6a), shift(0), width(8), invert(0), max(0x6c)
 */
static const DECLARE_TLV_DB_SCALE(s2803x_alc_min_gain_tlv, -5400, 50, 0);

/**
 * s2803x_alc_lvl_tlv
 *
 * Range: -48dB to 0, step 1.5dB
 *
 * ALC_LVL_L, reg(0x6b), shift(0), width(5), invert(0), max(31)
 * ALC_LVL_R, reg(0x6c), shift(0), width(5), invert(0), max(31)
 */
static const DECLARE_TLV_DB_SCALE(s2803x_alc_lvl_tlv, -4800, 150, 0);


/**
 * s2803x_alc_ng_th_tlv
 *
 * Range: -76.5dB to -30dB, step 1.5dB
 *
 * ALCNGTH, reg(0x70), shift(0), width(5), invert(0), max(31)
 */
static const DECLARE_TLV_DB_SCALE(s2803x_alc_ng_th_tlv, -7650, 150, 0);

/**
 * s2803x_alc_winsel
 *
 * ALC Window-Length Select
 */
static const char *s2803x_alc_winsel_text[] = {
	"fs600", "fs1200", "fs2400", "fs300"
};

static SOC_ENUM_SINGLE_DECL(s2803x_alc_winsel_enum, S2803X_REG_68_ALC_CTL,
				ALC_CTL_WINSEL_SHIFT, s2803x_alc_winsel_text);


/**
 * s2803x_alc_mode
 *
 * ALC Function Select
 */
static const char *s2803x_alc_mode_text[] = {
	"Stereo", "Right", "Left", "Independent"
};

static SOC_ENUM_SINGLE_DECL(s2803x_alc_mode_enum, S2803X_REG_68_ALC_CTL,
				ALC_CTL_ALC_MODE_SHIFT, s2803x_alc_mode_text);



/**
 * ALC Path selection
 */
static const char *s2803x_alc_path_sel_text[] = {
	"ADC", "Mixer"
};

static const struct soc_enum s2803x_alc_path_sel_enum =
SOC_ENUM_SINGLE(S2803X_REG_6D_ALC_HLD, ALC_HLD_ALC_PATH_SEL_SHIFT,
		ARRAY_SIZE(s2803x_alc_path_sel_text), s2803x_alc_path_sel_text);

/**
 * s2803x_mpcm_srate
 *
 * Master PCM sample rate selection
 */
static const char *s2803x_mpcm_master_srate_text[] = {
	"8KHz", "16KHz", "24KHz", "32KHz"
};

static const struct soc_enum s2803x_mpcm_srate1_enum =
	SOC_ENUM_SINGLE(S2803X_REG_01_IN1_CTL1, INCTL1_MPCM_SRATE_SHIFT,
			ARRAY_SIZE(s2803x_mpcm_master_srate_text),
			s2803x_mpcm_master_srate_text);

static const struct soc_enum s2803x_mpcm_srate2_enum =
	SOC_ENUM_SINGLE(S2803X_REG_04_IN2_CTL1, INCTL1_MPCM_SRATE_SHIFT,
			ARRAY_SIZE(s2803x_mpcm_master_srate_text),
			s2803x_mpcm_master_srate_text);

static const struct soc_enum s2803x_mpcm_srate3_enum =
	SOC_ENUM_SINGLE(S2803X_REG_07_IN3_CTL1, INCTL1_MPCM_SRATE_SHIFT,
			ARRAY_SIZE(s2803x_mpcm_master_srate_text),
			s2803x_mpcm_master_srate_text);

/**
 * mpcm_slot_sel
 *
 * Master PCM slot selection
 */
static const char *s2803x_mpcm_slot_text[] = {
	"1 slot", "2 slots", "3 slots", "4 slots"
};

static const struct soc_enum s2803x_mpcm_slot1_enum =
	SOC_ENUM_SINGLE(S2803X_REG_01_IN1_CTL1, INCTL1_MPCM_SLOT_SHIFT,
			ARRAY_SIZE(s2803x_mpcm_slot_text),
			s2803x_mpcm_slot_text);

static const struct soc_enum s2803x_mpcm_slot2_enum =
	SOC_ENUM_SINGLE(S2803X_REG_04_IN2_CTL1, INCTL1_MPCM_SLOT_SHIFT,
			ARRAY_SIZE(s2803x_mpcm_slot_text),
			s2803x_mpcm_slot_text);

static const struct soc_enum s2803x_mpcm_slot3_enum =
	SOC_ENUM_SINGLE(S2803X_REG_07_IN3_CTL1, INCTL1_MPCM_SLOT_SHIFT,
			ARRAY_SIZE(s2803x_mpcm_slot_text),
			s2803x_mpcm_slot_text);

/**
 * bclk_pol
 *
 * Polarity of various bit-clocks
 */
static const char *s2803x_clock_pol_text[] = {
	"Normal", "Inverted"
};

static const struct soc_enum s2803x_bck_pol1_enum =
	SOC_ENUM_SINGLE(S2803X_REG_01_IN1_CTL1, INCTL1_BCK_POL_SHIFT,
			ARRAY_SIZE(s2803x_clock_pol_text),
			s2803x_clock_pol_text);

static const struct soc_enum s2803x_bck_pol2_enum =
	SOC_ENUM_SINGLE(S2803X_REG_04_IN2_CTL1, INCTL1_BCK_POL_SHIFT,
			ARRAY_SIZE(s2803x_clock_pol_text),
			s2803x_clock_pol_text);

static const struct soc_enum s2803x_bck_pol3_enum =
	SOC_ENUM_SINGLE(S2803X_REG_07_IN3_CTL1, INCTL1_BCK_POL_SHIFT,
			ARRAY_SIZE(s2803x_clock_pol_text),
			s2803x_clock_pol_text);

static const struct soc_enum s2803x_lrck_pol1_enum =
	SOC_ENUM_SINGLE(S2803X_REG_02_IN1_CTL2, INCTL2_LRCK_POL_SHIFT,
			ARRAY_SIZE(s2803x_clock_pol_text),
			s2803x_clock_pol_text);

static const struct soc_enum s2803x_lrck_pol2_enum =
	SOC_ENUM_SINGLE(S2803X_REG_05_IN2_CTL2, INCTL2_LRCK_POL_SHIFT,
			ARRAY_SIZE(s2803x_clock_pol_text),
			s2803x_clock_pol_text);

static const struct soc_enum s2803x_lrck_pol3_enum =
	SOC_ENUM_SINGLE(S2803X_REG_08_IN3_CTL2, INCTL2_LRCK_POL_SHIFT,
			ARRAY_SIZE(s2803x_clock_pol_text),
			s2803x_clock_pol_text);

/**
 * i2s_pcm
 *
 * Input Audio Mode
 */
static const char *s2803x_i2s_pcm_text[] = {
	"I2S", "PCM"
};

static const struct soc_enum s2803x_i2s_pcm1_enum =
	SOC_ENUM_SINGLE(S2803X_REG_01_IN1_CTL1, INCTL1_I2S_PCM_SHIFT,
			ARRAY_SIZE(s2803x_i2s_pcm_text),
			s2803x_i2s_pcm_text);

static const struct soc_enum s2803x_i2s_pcm2_enum =
	SOC_ENUM_SINGLE(S2803X_REG_04_IN2_CTL1, INCTL1_I2S_PCM_SHIFT,
			ARRAY_SIZE(s2803x_i2s_pcm_text),
			s2803x_i2s_pcm_text);

static const struct soc_enum s2803x_i2s_pcm3_enum =
	SOC_ENUM_SINGLE(S2803X_REG_07_IN3_CTL1, INCTL1_I2S_PCM_SHIFT,
			ARRAY_SIZE(s2803x_i2s_pcm_text),
			s2803x_i2s_pcm_text);

/**
 * i2s_xfs
 *
 * BCK vs LRCK condition
 */
static const char *s2803x_i2s_xfs_text[] = {
	"32fs", "48fs", "64fs", "64fs"
};

static const struct soc_enum s2803x_i2s_xfs1_enum =
	SOC_ENUM_SINGLE(S2803X_REG_02_IN1_CTL2, INCTL2_I2S_XFS_SHIFT,
			ARRAY_SIZE(s2803x_i2s_xfs_text),
			s2803x_i2s_xfs_text);

static const struct soc_enum s2803x_i2s_xfs2_enum =
	SOC_ENUM_SINGLE(S2803X_REG_05_IN2_CTL2, INCTL2_I2S_XFS_SHIFT,
			ARRAY_SIZE(s2803x_i2s_xfs_text),
			s2803x_i2s_xfs_text);

static const struct soc_enum s2803x_i2s_xfs3_enum =
	SOC_ENUM_SINGLE(S2803X_REG_08_IN3_CTL2, INCTL2_I2S_XFS_SHIFT,
			ARRAY_SIZE(s2803x_i2s_xfs_text),
			s2803x_i2s_xfs_text);

/**
 * i2s_df
 *
 * I2S Data Format
 */
static const char *s2803x_i2s_df_text[] = {
	"I2S", "Left-Justified", "Right-Justified", "Invalid"
};

static const struct soc_enum s2803x_i2s_df1_enum =
	SOC_ENUM_SINGLE(S2803X_REG_02_IN1_CTL2, INCTL2_I2S_DF_SHIFT,
			ARRAY_SIZE(s2803x_i2s_df_text),
			s2803x_i2s_df_text);

static const struct soc_enum s2803x_i2s_df2_enum =
	SOC_ENUM_SINGLE(S2803X_REG_05_IN2_CTL2, INCTL2_I2S_DF_SHIFT,
			ARRAY_SIZE(s2803x_i2s_df_text),
			s2803x_i2s_df_text);

static const struct soc_enum s2803x_i2s_df3_enum =
	SOC_ENUM_SINGLE(S2803X_REG_08_IN3_CTL2, INCTL2_I2S_DF_SHIFT,
			ARRAY_SIZE(s2803x_i2s_df_text),
			s2803x_i2s_df_text);

/**
 * i2s_dl
 *
 * I2S Data Length
 */
static const char *s2803x_i2s_dl_text[] = {
	"16-bit", "18-bit", "20-bit", "24-bit"
};

static const struct soc_enum s2803x_i2s_dl1_enum =
	SOC_ENUM_SINGLE(S2803X_REG_02_IN1_CTL2, INCTL2_I2S_DL_SHIFT,
			ARRAY_SIZE(s2803x_i2s_dl_text),
			s2803x_i2s_dl_text);

static const struct soc_enum s2803x_i2s_dl2_enum =
	SOC_ENUM_SINGLE(S2803X_REG_05_IN2_CTL2, INCTL2_I2S_DL_SHIFT,
			ARRAY_SIZE(s2803x_i2s_dl_text),
			s2803x_i2s_dl_text);

static const struct soc_enum s2803x_i2s_dl3_enum =
	SOC_ENUM_SINGLE(S2803X_REG_08_IN3_CTL2, INCTL2_I2S_DL_SHIFT,
			ARRAY_SIZE(s2803x_i2s_dl_text),
			s2803x_i2s_dl_text);

/**
 * pcm_dad
 *
 * PCM Data Additional Delay
 */
static const char *s2803x_pcm_dad_text[] = {
	"1 bck", "0 bck", "2 bck", "", "3 bck", "", "4 bck", ""
};

static const struct soc_enum s2803x_pcm_dad1_enum =
	SOC_ENUM_SINGLE(S2803X_REG_03_IN1_CTL3, INCTL3_PCM_DAD_SHIFT,
			ARRAY_SIZE(s2803x_pcm_dad_text),
			s2803x_pcm_dad_text);

static const struct soc_enum s2803x_pcm_dad2_enum =
	SOC_ENUM_SINGLE(S2803X_REG_06_IN2_CTL3, INCTL3_PCM_DAD_SHIFT,
			ARRAY_SIZE(s2803x_pcm_dad_text),
			s2803x_pcm_dad_text);

static const struct soc_enum s2803x_pcm_dad3_enum =
	SOC_ENUM_SINGLE(S2803X_REG_09_IN3_CTL3, INCTL3_PCM_DAD_SHIFT,
			ARRAY_SIZE(s2803x_pcm_dad_text),
			s2803x_pcm_dad_text);

/**
 * pcm_df
 *
 * PCM Data Format
 */
static const char *s2803x_pcm_df_text[] = {
	"", "", "", "", "Short Frame", "", "", "",
	"", "", "", "", "Long Frame"
};

static const struct soc_enum s2803x_pcm_df1_enum =
	SOC_ENUM_SINGLE(S2803X_REG_03_IN1_CTL3, INCTL3_PCM_DF_SHIFT,
			ARRAY_SIZE(s2803x_pcm_df_text),
			s2803x_pcm_dad_text);

static const struct soc_enum s2803x_pcm_df2_enum =
	SOC_ENUM_SINGLE(S2803X_REG_06_IN2_CTL3, INCTL3_PCM_DF_SHIFT,
			ARRAY_SIZE(s2803x_pcm_df_text),
			s2803x_pcm_dad_text);

static const struct soc_enum s2803x_pcm_df3_enum =
	SOC_ENUM_SINGLE(S2803X_REG_09_IN3_CTL3, INCTL3_PCM_DF_SHIFT,
			ARRAY_SIZE(s2803x_pcm_df_text),
			s2803x_pcm_dad_text);

/**
 * bck4_mode
 *
 * BCK4 Output Selection
 */
static const char *s2803x_bck4_mode_text[] = {
	"Normal BCK", "MCKO"
};

static const struct soc_enum s2803x_bck4_mode_enum =
	SOC_ENUM_SINGLE(S2803X_REG_0A_HQ_CTL, HQ_CTL_BCK4_MODE_SHIFT,
			ARRAY_SIZE(s2803x_bck4_mode_text),
			s2803x_bck4_mode_text);

/**
 * dout_sel1
 *
 * CH1 Digital Output Selection
 */
static const char *s2803x_dout_sel1_text[] = {
	"DMIX_OUT", "AIF4IN", "RMIX_OUT"
};

static SOC_ENUM_SINGLE_DECL(s2803x_dout_sel1_enum, S2803X_REG_16_DOUTMX1,
		DOUTMX1_DOUT_SEL1_SHIFT, s2803x_dout_sel1_text);

/**
 * dout_sel2
 *
 * CH2 Digital Output Selection
 */
static const char *s2803x_dout_sel2_text[] = {
	"DMIX_OUT", "AIF4IN", "AIF3IN"
};

static SOC_ENUM_SINGLE_DECL(s2803x_dout_sel2_enum, S2803X_REG_16_DOUTMX1,
		DOUTMX1_DOUT_SEL2_SHIFT, s2803x_dout_sel2_text);

/**
 * dout_sel3
 *
 * CH3 Digital Output Selection
 */
static const char *s2803x_dout_sel3_text[] = {
	"DMIX_OUT", "AIF4IN", "AIF2IN"
};

static SOC_ENUM_SINGLE_DECL(s2803x_dout_sel3_enum, S2803X_REG_17_DOUTMX2,
		DOUTMX2_DOUT_SEL3_SHIFT, s2803x_dout_sel3_text);

static const char *s2803x_off_on_text[] = {
	"Off", "On"
};

static const struct soc_enum s2803x_hq_en_enum =
SOC_ENUM_SINGLE(S2803X_REG_0A_HQ_CTL, HQ_CTL_HQ_EN_SHIFT,
		ARRAY_SIZE(s2803x_off_on_text), s2803x_off_on_text);

static const struct soc_enum s2803x_ch3_rec_en_enum =
SOC_ENUM_SINGLE(S2803X_REG_0A_HQ_CTL, HQ_CTL_CH3_SEL_SHIFT,
		ARRAY_SIZE(s2803x_off_on_text), s2803x_off_on_text);

static const struct soc_enum s2803x_mcko_en_enum =
SOC_ENUM_SINGLE(S2803X_REG_0A_HQ_CTL, HQ_CTL_MCKO_EN_SHIFT,
		ARRAY_SIZE(s2803x_off_on_text), s2803x_off_on_text);

static const struct soc_enum s2803x_rmix1_en_enum =
SOC_ENUM_SINGLE(S2803X_REG_0D_RMIX_CTL, RMIX_CTL_RMIX1_EN_SHIFT,
		ARRAY_SIZE(s2803x_off_on_text), s2803x_off_on_text);

static const struct soc_enum s2803x_rmix2_en_enum =
SOC_ENUM_SINGLE(S2803X_REG_0D_RMIX_CTL, RMIX_CTL_RMIX2_EN_SHIFT,
		ARRAY_SIZE(s2803x_off_on_text), s2803x_off_on_text);

static const struct soc_enum mixer_ch1_enable_enum =
SOC_ENUM_SINGLE(S2803X_REG_10_DMIX1, DMIX1_MIX_EN1_SHIFT,
		ARRAY_SIZE(s2803x_off_on_text), s2803x_off_on_text);
static const struct soc_enum mixer_ch2_enable_enum =
SOC_ENUM_SINGLE(S2803X_REG_10_DMIX1, DMIX1_MIX_EN2_SHIFT,
		ARRAY_SIZE(s2803x_off_on_text), s2803x_off_on_text);
static const struct soc_enum mixer_ch3_enable_enum =
SOC_ENUM_SINGLE(S2803X_REG_11_DMIX2, DMIX2_MIX_EN3_SHIFT,
		ARRAY_SIZE(s2803x_off_on_text), s2803x_off_on_text);
static const struct soc_enum mixer_ch4_enable_enum =
SOC_ENUM_SINGLE(S2803X_REG_11_DMIX2, DMIX2_MIX_EN4_SHIFT,
		ARRAY_SIZE(s2803x_off_on_text), s2803x_off_on_text);

static const struct soc_enum mixer_enable_enum =
SOC_ENUM_SINGLE(S2803X_REG_0F_DIG_EN, DIG_EN_MIX_EN_SHIFT,
		ARRAY_SIZE(s2803x_off_on_text), s2803x_off_on_text);
static const struct soc_enum src3_enable_enum =
SOC_ENUM_SINGLE(S2803X_REG_0F_DIG_EN, DIG_EN_SRC3_EN_SHIFT,
		ARRAY_SIZE(s2803x_off_on_text), s2803x_off_on_text);
static const struct soc_enum src2_enable_enum =
SOC_ENUM_SINGLE(S2803X_REG_0F_DIG_EN, DIG_EN_SRC2_EN_SHIFT,
		ARRAY_SIZE(s2803x_off_on_text), s2803x_off_on_text);
static const struct soc_enum src1_enable_enum =
SOC_ENUM_SINGLE(S2803X_REG_0F_DIG_EN, DIG_EN_SRC1_EN_SHIFT,
		ARRAY_SIZE(s2803x_off_on_text), s2803x_off_on_text);

static const struct soc_enum alc_enable_enum =
SOC_ENUM_SINGLE(S2803X_REG_68_ALC_CTL, ALC_CTL_ALC_EN_SHIFT,
		ARRAY_SIZE(s2803x_off_on_text), s2803x_off_on_text);

static const struct soc_enum s2803x_alc_enable_enum =
SOC_ENUM_SINGLE(S2803X_REG_68_ALC_CTL, ALC_CTL_ALC_EN_SHIFT,
		ARRAY_SIZE(s2803x_off_on_text), s2803x_off_on_text);

static const struct soc_enum s2803x_alc_limiter_mode_enum =
SOC_ENUM_SINGLE(S2803X_REG_68_ALC_CTL, ALC_CTL_ALC_LIM_SHIFT,
		ARRAY_SIZE(s2803x_off_on_text), s2803x_off_on_text);

static const struct soc_enum s2803x_alc_start_gain_en_enum =
SOC_ENUM_SINGLE(S2803X_REG_6D_ALC_HLD, ALC_HLD_ST_GAIN_EN_SHIFT,
		ARRAY_SIZE(s2803x_off_on_text), s2803x_off_on_text);

static const struct soc_enum s2803x_noise_gate_enable_enum =
SOC_ENUM_SINGLE(S2803X_REG_70_ALC_NG, ALC_NG_NGAT_SHIFT,
		ARRAY_SIZE(s2803x_off_on_text), s2803x_off_on_text);

/*
 * s2803x_soc_kcontrol_handler: Provide runtime PM handling during the get/put
 * event handler of a specific control
 *
 * Since we don't have regcache support for this device, the get/put events will
 * access the hardware everytime the user-space accesses the device controls. If
 * the audio block is in suspend state, this might will result in OOPs because
 * the power-domain/clocks might be in off state. The best approach would be to
 * enable the device, execute the get/put function and then put the device in
 * suspended state. This function provides helpers for this purpose.
 *
 * Arguments
 * kcontrol/ucontrol: Passed to the framework API
 * func: The default framework API used to manage this event.
 */
static int s2803x_soc_kcontrol_handler(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol,
		int (*func)(struct snd_kcontrol *, struct snd_ctl_elem_value *))
{
	int ret;

	/* Enable the power */
	pm_runtime_get_sync(s2803x->dev);

	ret = func(kcontrol, ucontrol);

	/*
	 * There may be a pending request for accessing another control element,
	 * so use a delayed suspend API so that we don't get into an avoidable
	 * suspend-resume cycle.
	 *
	 * pm_runtime_mark_last_busy: Tells the RPM framework the last activity
	 * time of this device
	 *
	 * pm_runtime_put_autosuspend: It queues the device runtime suspend
	 * request. Internally the framework waits till auto-suspend delay after
	 * the last busy time (updated in previous API) and then runs the
	 * device runtime suspend API.
	 */
	pm_runtime_mark_last_busy(s2803x->dev);
	pm_runtime_put_autosuspend(s2803x->dev);

	return ret;
}

/* Function to get TLV control value */
static int s2803x_snd_soc_get_volsw(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return s2803x_soc_kcontrol_handler(kcontrol, ucontrol,
			snd_soc_get_volsw);
}

/* Function to set TLV control value */
static int s2803x_snd_soc_put_volsw(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return s2803x_soc_kcontrol_handler(kcontrol, ucontrol,
			snd_soc_put_volsw);
}

/* Function to get TLV control value */
static int s2803x_snd_soc_get_volsw_range(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return s2803x_soc_kcontrol_handler(kcontrol, ucontrol,
			snd_soc_get_volsw_range);
}

/* Function to set TLV control value */
static int s2803x_snd_soc_put_volsw_range(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return s2803x_soc_kcontrol_handler(kcontrol, ucontrol,
			snd_soc_put_volsw_range);
}

/* Function to get ENUM control value */
static int s2803x_soc_enum_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return s2803x_soc_kcontrol_handler(kcontrol, ucontrol,
			snd_soc_get_enum_double);
}

/* Function to set ENUM control value */
static int s2803x_soc_enum_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return s2803x_soc_kcontrol_handler(kcontrol, ucontrol,
			snd_soc_put_enum_double);
}

/**
 * struct snd_kcontrol_new s2803x_snd_control
 *
 * Every distinct bit-fields within the CODEC SFR range may be considered
 * as a control elements. Such control elements are defined here.
 *
 * Depending on the access mode of these registers, different macros are
 * used to define these control elements.
 *
 * SOC_ENUM: 1-to-1 mapping between bit-field value and provided text
 * SOC_SINGLE: Single register, value is a number
 * SOC_SINGLE_TLV: Single register, value corresponds to a TLV scale
 * SOC_SINGLE_TLV_EXT: Above + custom get/set operation for this value
 * SOC_SINGLE_RANGE_TLV: Register value is an offset from minimum value
 * SOC_DOUBLE: Two bit-fields are updated in a single register
 * SOC_DOUBLE_R: Two bit-fields in 2 different registers are updated
 */

/**
 * All the data goes into s2803x_snd_controls.
 */
static const struct snd_kcontrol_new s2803x_snd_controls[] = {
	SOC_SINGLE_EXT_TLV("RMIX1_LVL", S2803X_REG_0D_RMIX_CTL,
			RMIX_CTL_RMIX2_LVL_SHIFT,
			BIT(RMIX_CTL_RMIX2_LVL_WIDTH) - 1, 0,
			s2803x_snd_soc_get_volsw, s2803x_snd_soc_put_volsw,
			s2803x_mix_tlv),

	SOC_SINGLE_EXT_TLV("RMIX2_LVL", S2803X_REG_0D_RMIX_CTL,
			RMIX_CTL_RMIX1_LVL_SHIFT,
			BIT(RMIX_CTL_RMIX1_LVL_WIDTH) - 1, 0,
			s2803x_snd_soc_get_volsw, s2803x_snd_soc_put_volsw,
			s2803x_mix_tlv),

	SOC_SINGLE_EXT_TLV("MIX1_LVL", S2803X_REG_10_DMIX1,
			DMIX1_MIX_LVL1_SHIFT,
			BIT(DMIX1_MIX_LVL1_WIDTH) - 1, 0,
			s2803x_snd_soc_get_volsw, s2803x_snd_soc_put_volsw,
			s2803x_mix_tlv),

	SOC_SINGLE_EXT_TLV("MIX2_LVL", S2803X_REG_10_DMIX1,
			DMIX1_MIX_LVL2_SHIFT,
			BIT(DMIX1_MIX_LVL2_WIDTH) - 1, 0,
			s2803x_snd_soc_get_volsw, s2803x_snd_soc_put_volsw,
			s2803x_mix_tlv),

	SOC_SINGLE_EXT_TLV("MIX3_LVL", S2803X_REG_11_DMIX2,
			DMIX2_MIX_LVL3_SHIFT,
			BIT(DMIX2_MIX_LVL3_WIDTH) - 1, 0,
			s2803x_snd_soc_get_volsw, s2803x_snd_soc_put_volsw,
			s2803x_mix_tlv),

	SOC_SINGLE_EXT_TLV("MIX4_LVL", S2803X_REG_11_DMIX2,
			DMIX2_MIX_LVL4_SHIFT,
			BIT(DMIX2_MIX_LVL4_WIDTH) - 1, 0,
			s2803x_snd_soc_get_volsw, s2803x_snd_soc_put_volsw,
			s2803x_mix_tlv),

	SOC_SINGLE_EXT_TLV("ALC NG HYS", S2803X_REG_68_ALC_CTL,
			ALC_CTL_ALC_NG_HYS_SHIFT,
			BIT(ALC_CTL_ALC_NG_HYS_WIDTH) - 1, 0,
			s2803x_snd_soc_get_volsw, s2803x_snd_soc_put_volsw,
			s2803x_alc_ng_hys_tlv),

	SOC_SINGLE_RANGE_EXT_TLV("ALC Max Gain", S2803X_REG_69_ALC_GA1,
			ALC_GA1_ALC_MAX_GAIN_SHIFT,
			ALC_GA1_ALC_MAX_GAIN_MINVAL,
			ALC_GA1_ALC_MAX_GAIN_MAXVAL, 0,
			s2803x_snd_soc_get_volsw_range,
			s2803x_snd_soc_put_volsw_range,
			s2803x_alc_max_gain_tlv),

	SOC_SINGLE_RANGE_EXT_TLV("ALC Min Gain", S2803X_REG_6A_ALC_GA2,
			ALC_GA2_ALC_MIN_GAIN_SHIFT,
			ALC_GA2_ALC_MIN_GAIN_MINVAL,
			ALC_GA2_ALC_MIN_GAIN_MAXVAL, 0,
			s2803x_snd_soc_get_volsw_range,
			s2803x_snd_soc_put_volsw_range,
			s2803x_alc_min_gain_tlv),

	SOC_SINGLE_RANGE_EXT_TLV("ALC Start Gain Left", S2803X_REG_71_ALC_SGL,
			ALC_SGL_START_GAIN_L_SHIFT,
			ALC_SGL_START_GAIN_L_MINVAL,
			ALC_SGL_START_GAIN_L_MAXVAL, 0,
			s2803x_snd_soc_get_volsw_range,
			s2803x_snd_soc_put_volsw_range,
			s2803x_alc_max_gain_tlv),

	SOC_SINGLE_RANGE_EXT_TLV("ALC Start Gain Right", S2803X_REG_72_ALC_SGR,
			ALC_SGR_START_GAIN_R_SHIFT,
			ALC_SGR_START_GAIN_R_MINVAL,
			ALC_SGR_START_GAIN_R_MAXVAL, 0,
			s2803x_snd_soc_get_volsw_range,
			s2803x_snd_soc_put_volsw_range,
			s2803x_alc_max_gain_tlv),

	SOC_SINGLE_EXT_TLV("ALC LVL Left", S2803X_REG_6B_ALC_LVL,
			ALC_LVL_LVL_SHIFT,
			BIT(ALC_LVL_LVL_WIDTH) - 1, 0,
			s2803x_snd_soc_get_volsw, s2803x_snd_soc_put_volsw,
			s2803x_alc_lvl_tlv),

	SOC_SINGLE_EXT_TLV("ALC LVL Right", S2803X_REG_6C_ALC_LVR,
			ALC_LVR_LVL_SHIFT,
			BIT(ALC_LVR_LVL_WIDTH) - 1, 0,
			s2803x_snd_soc_get_volsw, s2803x_snd_soc_put_volsw,
			s2803x_alc_lvl_tlv),

	SOC_SINGLE_EXT_TLV("ALC Noise Gate Threshold", S2803X_REG_70_ALC_NG,
			ALC_NG_ALCNGTH_SHIFT,
			BIT(ALC_NG_ALCNGTH_WIDTH) - 1, 0,
			s2803x_snd_soc_get_volsw, s2803x_snd_soc_put_volsw,
			s2803x_alc_ng_th_tlv),

	SOC_ENUM_EXT("CH1 Master PCM Sample Rate", s2803x_mpcm_srate1_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),
	SOC_ENUM_EXT("CH2 Master PCM Sample Rate", s2803x_mpcm_srate2_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),
	SOC_ENUM_EXT("CH3 Master PCM Sample Rate", s2803x_mpcm_srate3_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),

	SOC_ENUM_EXT("CH1 Master PCM Slot", s2803x_mpcm_slot1_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),
	SOC_ENUM_EXT("CH2 Master PCM Slot", s2803x_mpcm_slot2_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),
	SOC_ENUM_EXT("CH3 Master PCM Slot", s2803x_mpcm_slot3_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),

	SOC_ENUM_EXT("CH1 BCLK Polarity", s2803x_bck_pol1_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),
	SOC_ENUM_EXT("CH2 BCLK Polarity", s2803x_bck_pol2_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),
	SOC_ENUM_EXT("CH3 BCLK Polarity", s2803x_bck_pol3_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),

	SOC_ENUM_EXT("CH1 LRCLK Polarity", s2803x_lrck_pol1_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),
	SOC_ENUM_EXT("CH2 LRCLK Polarity", s2803x_lrck_pol2_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),
	SOC_ENUM_EXT("CH3 LRCLK Polarity", s2803x_lrck_pol3_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),

	SOC_ENUM_EXT("CH1 Input Audio Mode", s2803x_i2s_pcm1_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),
	SOC_ENUM_EXT("CH2 Input Audio Mode", s2803x_i2s_pcm2_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),
	SOC_ENUM_EXT("CH3 Input Audio Mode", s2803x_i2s_pcm3_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),

	SOC_ENUM_EXT("CH1 XFS", s2803x_i2s_xfs1_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),
	SOC_ENUM_EXT("CH2 XFS", s2803x_i2s_xfs2_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),
	SOC_ENUM_EXT("CH3 XFS", s2803x_i2s_xfs3_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),

	SOC_ENUM_EXT("CH1 I2S Format", s2803x_i2s_df1_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),
	SOC_ENUM_EXT("CH2 I2S Format", s2803x_i2s_df2_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),
	SOC_ENUM_EXT("CH3 I2S Format", s2803x_i2s_df3_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),

	SOC_ENUM_EXT("CH1 I2S Data Length", s2803x_i2s_dl1_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),
	SOC_ENUM_EXT("CH2 I2S Data Length", s2803x_i2s_dl2_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),
	SOC_ENUM_EXT("CH3 I2S Data Length", s2803x_i2s_dl3_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),

	SOC_ENUM_EXT("CH1 PCM DAD", s2803x_pcm_dad1_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),
	SOC_ENUM_EXT("CH2 PCM DAD", s2803x_pcm_dad2_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),
	SOC_ENUM_EXT("CH3 PCM DAD", s2803x_pcm_dad3_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),

	SOC_ENUM_EXT("CH1 PCM Data Format", s2803x_pcm_df1_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),
	SOC_ENUM_EXT("CH2 PCM Data Format", s2803x_pcm_df2_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),
	SOC_ENUM_EXT("CH3 PCM Data Format", s2803x_pcm_df3_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),

	SOC_ENUM_EXT("CH3 Rec En", s2803x_ch3_rec_en_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),
	SOC_ENUM_EXT("MCKO En", s2803x_mcko_en_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),

	SOC_ENUM_EXT("RMIX1 En", s2803x_rmix1_en_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),
	SOC_ENUM_EXT("RMIX2 En", s2803x_rmix2_en_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),

	SOC_ENUM_EXT("BCK4 Output Selection", s2803x_bck4_mode_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),

	SOC_ENUM_EXT("HQ En", s2803x_hq_en_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),

	SOC_ENUM_EXT("CH1 DOUT Select", s2803x_dout_sel1_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),
	SOC_ENUM_EXT("CH2 DOUT Select", s2803x_dout_sel2_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),
	SOC_ENUM_EXT("CH3 DOUT Select", s2803x_dout_sel3_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),

	SOC_ENUM_EXT("CH1 Mixer En", mixer_ch1_enable_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),
	SOC_ENUM_EXT("CH2 Mixer En", mixer_ch2_enable_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),
	SOC_ENUM_EXT("CH3 Mixer En", mixer_ch3_enable_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),
	SOC_ENUM_EXT("CH4 Mixer En", mixer_ch4_enable_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),
	SOC_ENUM_EXT("Mixer En", mixer_enable_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),
	SOC_ENUM_EXT("SRC1 En", src1_enable_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),
	SOC_ENUM_EXT("SRC2 En", src2_enable_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),
	SOC_ENUM_EXT("SRC3 En", src3_enable_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),

	SOC_ENUM_EXT("ALC Window Length", s2803x_alc_winsel_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),
	SOC_ENUM_EXT("ALC Mode", s2803x_alc_mode_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),

	SOC_ENUM_EXT("ALC En", s2803x_alc_enable_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),
	SOC_ENUM_EXT("ALC Limiter Mode En", s2803x_alc_limiter_mode_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),
	SOC_ENUM_EXT("ALC Start Gain En", s2803x_alc_start_gain_en_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),

	SOC_SINGLE_EXT("ALC Hold Time", S2803X_REG_6D_ALC_HLD,
			ALC_HLD_HOLD_SHIFT, ALC_HLD_HOLD_MASK, 0,
			s2803x_snd_soc_get_volsw, s2803x_snd_soc_put_volsw),

	SOC_SINGLE_EXT("ALC Attack Time", S2803X_REG_6E_ALC_ATK,
			ALC_ATK_ATTACK_SHIFT, ALC_ATK_ATTACK_MASK, 0,
			s2803x_snd_soc_get_volsw, s2803x_snd_soc_put_volsw),

	SOC_SINGLE_EXT("ALC Decay Time", S2803X_REG_6F_ALC_DCY,
			ALC_DCY_DECAY_SHIFT, ALC_DCY_DECAY_MASK, 0,
			s2803x_snd_soc_get_volsw, s2803x_snd_soc_put_volsw),

	SOC_ENUM_EXT("ALC Path", s2803x_alc_path_sel_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),
	SOC_ENUM_EXT("ALC Noise Gate En", s2803x_noise_gate_enable_enum,
		s2803x_soc_enum_get, s2803x_soc_enum_put),
};

/**
 * s2803_get_clk_mixer
 *
 * This function gets all the clock related to the audio i2smixer and stores in
 * mixer private structure.
 */
static int s2803_get_clk_mixer(struct device *dev)
{
	struct s2803x_priv *s2803x = dev_get_drvdata(dev);

	s2803x->aclk_audmixer = clk_get(dev, "audmixer_aclk");
	if (IS_ERR(s2803x->aclk_audmixer)) {
		dev_err(dev, "audmixer_aclk clk not found\n");
		goto err0;
	}

	s2803x->sclk_audmixer = clk_get(dev, "audmixer_sysclk");
	if (IS_ERR(s2803x->sclk_audmixer)) {
		dev_err(dev, "audmixer_sysclk clk not found\n");
		goto err0;
	}

	s2803x->sclk_audmixer_bclk0 = clk_get(dev, "audmixer_bclk0");
	if (IS_ERR(s2803x->sclk_audmixer_bclk0)) {
		dev_err(dev, "audmixer bclk0 clk not found\n");
		goto err1;
	}

	s2803x->sclk_audmixer_bclk1 = clk_get(dev, "audmixer_bclk1");
	if (IS_ERR(s2803x->sclk_audmixer_bclk1)) {
		dev_err(dev, "audmixer bclk1 clk not found\n");
		goto err2;
	}

	s2803x->sclk_audmixer_bclk2 = clk_get(dev, "audmixer_bclk2");
	if (IS_ERR(s2803x->sclk_audmixer_bclk2)) {
		dev_err(dev, "audmixer bclk2 clk not found\n");
		goto err3;
	}

	s2803x->dout_audmixer = clk_get(dev, "audmixer_dout");
	if (IS_ERR(s2803x->dout_audmixer)) {
		dev_err(dev, "audmixer_dout clk not found\n");
		goto err4;
	}

	clk_prepare_enable(s2803x->aclk_audmixer);

	return 0;

err4:
	clk_put(s2803x->sclk_audmixer_bclk2);
err3:
	clk_put(s2803x->sclk_audmixer_bclk1);
err2:
	clk_put(s2803x->sclk_audmixer_bclk0);
err1:
	clk_put(s2803x->sclk_audmixer);
err0:
	return -1;
}

/**
 * s2803x_clk_put_all
 *
 * This function puts all the clock related to the audio i2smixer
 */
static void s2803x_clk_put_all(struct device *dev)
{
	struct s2803x_priv *s2803x = dev_get_drvdata(dev);

	clk_put(s2803x->sclk_audmixer_bclk2);
	clk_put(s2803x->sclk_audmixer_bclk1);
	clk_put(s2803x->sclk_audmixer_bclk0);
	clk_put(s2803x->sclk_audmixer);
}

/**
 * s2803x_clk_enable
 *
 * This function enables all the clock related to the audio i2smixer
 */
static void s2803x_clk_enable(struct device *dev)
{
	clk_prepare_enable(s2803x->sclk_audmixer);
	clk_prepare_enable(s2803x->sclk_audmixer_bclk0);
	clk_prepare_enable(s2803x->sclk_audmixer_bclk1);
	clk_prepare_enable(s2803x->sclk_audmixer_bclk2);
}

/**
 * s2803x_clk_disable
 *
 * This function disable all the clock related to the audio i2smixer
 */
static void s2803x_clk_disable(struct device *dev)
{
	clk_disable_unprepare(s2803x->sclk_audmixer_bclk0);
	clk_disable_unprepare(s2803x->sclk_audmixer_bclk1);
	clk_disable_unprepare(s2803x->sclk_audmixer_bclk2);
	clk_disable_unprepare(s2803x->sclk_audmixer);
}

static int s2803x_runtime_power_on(struct device *dev)
{
	/* Audio mixer Alive Configuration */
	__raw_writel(S2803_ALIVE_ON, EXYNOS_PMU_AUD_PATH_CFG);

	return 0;

}

/**
 * s2803x_power_off
 *
 * This function does the power off alive configurations for audio i2smixer
 */
static void s2803x_runtime_power_off(struct device *dev)
{
	/* Audio mixer Alive Configuration off */
	__raw_writel(S2803_ALIVE_OFF, EXYNOS_PMU_AUD_PATH_CFG);
}

#define s2803x_RATES SNDRV_PCM_RATE_8000_96000
#define s2803x_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE)

static struct snd_soc_dai_driver s2803x_dai = {
	.name = "HiFi",
	.playback = {
		.stream_name = "Primary",
		.channels_min = 2,
		.channels_max = 2,
		.rates = s2803x_RATES,
		.formats = s2803x_FORMATS,
	},
};

static void post_update_fw(void *context)
{
	struct snd_soc_codec *codec = (struct snd_soc_codec *)context;

	dev_dbg(codec->dev, "(*) %s\n", __func__);

	/* set ap path by defaut*/
	s2803x_init_mixer();

#ifdef CONFIG_PM_RUNTIME
	pm_runtime_put_sync(codec->dev);
#endif
}

static int s2803x_probe(struct snd_soc_codec *codec)
{
	int ret = 0;
	bool update_fw;

	dev_dbg(codec->dev, "(*) %s\n", __func__);

	codec->dev = s2803x->dev;
	s2803x->codec = codec;

	codec->control_data = s2803x->regmap;

	if (of_find_property(s2803x->dev->of_node,
				"samsung,lpass-subip", NULL))
		lpass_register_subip(s2803x->dev, "s2803x");

	if (of_find_property(s2803x->dev->of_node,
				"bck-mcko-mode", NULL))
		s2803x->is_bck4_mcko_enabled = true;
	else
		s2803x->is_bck4_mcko_enabled = false;

	if (of_find_property(s2803x->dev->of_node,
				"update-firmware", NULL))
		update_fw = true;
	else
		update_fw = false;

	s2803x->is_alc_enabled = false;

	/* Initilize default values */
	atomic_set(&s2803x->is_cp_running, 0);
	atomic_set(&s2803x->num_active_stream, 0);
	atomic_set(&s2803x->use_count_bt, 0);
	s2803x->aifrate = 0;

	/* Set Clock for Mixer */
	ret = s2803_get_clk_mixer(codec->dev);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to get clk for mixer\n");
		return ret;
	}

#ifdef CONFIG_PM_RUNTIME
	pm_runtime_enable(s2803x->dev);
	pm_runtime_use_autosuspend(s2803x->dev);
	pm_runtime_set_autosuspend_delay(s2803x->dev,
				S2803X_RPM_SUSPEND_DELAY_MS);
	pm_runtime_get_sync(s2803x->dev);
#endif

	ret = clk_set_rate(s2803x->dout_audmixer, S2803X_SYS_CLK_FREQ_48KHZ);
	if (ret != 0) {
		dev_err(s2803x->dev,
				"%s: Error setting mixer sysclk rate as %u\n",
				__func__, S2803X_SYS_CLK_FREQ_48KHZ);
		return ret;
	}

	if (update_fw)
		exynos_regmap_update_fw(S2803X_FIRMWARE_NAME,
			codec->dev, s2803x->regmap, s2803x->i2c_addr,
			post_update_fw, codec, post_update_fw, codec);
	else
		post_update_fw(codec);

	/* Initialize work queue for cp notification handling */
	INIT_WORK(&s2803x->cp_notification_work, s2803x_cp_notification_work);

	s2803x->mixer_cp_wq = create_singlethread_workqueue("mixer-cp-wq");
	if (s2803x->mixer_cp_wq == NULL) {
		dev_err(codec->dev, "Failed to create mixer-cp-wq\n");
		return -ENOMEM;
	}

	register_modem_event_notifier(&s2803x_cp_nb);

	return ret;
}

static int s2803x_remove(struct snd_soc_codec *codec)
{
	dev_dbg(codec->dev, "(*) %s\n", __func__);

	s2803x_clk_disable(codec->dev);
	s2803x_clk_put_all(codec->dev);
	s2803x_runtime_power_off(codec->dev);

	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_s2803x = {
	.probe = s2803x_probe,
	.remove = s2803x_remove,
	.controls = s2803x_snd_controls,
	.num_controls = ARRAY_SIZE(s2803x_snd_controls),
	.idle_bias_off = true,
};

static int s2803x_apb_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *res;
	struct pinctrl *pinctrl;

	dev_dbg(&pdev->dev, "(*) %s\n", __func__);

	s2803x = devm_kzalloc(&pdev->dev,
			sizeof(struct s2803x_priv), GFP_KERNEL);
	if (s2803x == NULL)
		return -ENOMEM;

	s2803x->save_regs_val = devm_kzalloc(&pdev->dev,
			ARRAY_SIZE(s2803x_save_regs_addr) * sizeof(unsigned
				int), GFP_KERNEL);
	if (!s2803x->save_regs_val)
		return -ENOMEM;

	s2803x->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, s2803x);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "Unable to get SRC2803X SFRs\n");
		return -ENXIO;
	}

	s2803x->regs = devm_request_and_ioremap(&pdev->dev, res);
	if (!s2803x->regs) {
		dev_err(&pdev->dev, "SFR ioremap failed\n");
		return -ENOMEM;
	}

	s2803x->regmap = devm_regmap_init_mmio_clk(&pdev->dev, "audmixer_aclk",
					s2803x->regs, &s2803x_regmap);
	if (IS_ERR(s2803x->regmap)) {
		ret = PTR_ERR(s2803x->regmap);
		dev_err(&pdev->dev, "Failed to allocate regmap: %d\n", ret);
		return ret;
	}

	pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(pinctrl)) {
		dev_warn(&pdev->dev, "did not get pins for Mixer-2803: %li\n",
				PTR_ERR(pinctrl));
	} else {
		s2803x->pinctrl = pinctrl;
	}

	ret = snd_soc_register_codec(&pdev->dev,
			&soc_codec_dev_s2803x, &s2803x_dai, 1);
	if (ret < 0)
		dev_err(&pdev->dev, "Failed to register codec: %d\n", ret);

	return ret;
}

static int s2803x_apb_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

static void s2803x_cfg_gpio(struct device *dev, const char *name)
{
	struct pinctrl_state *pin_state;
	struct s2803x_priv *s2803x = dev_get_drvdata(dev);

	pin_state = pinctrl_lookup_state(s2803x->pinctrl, name);
	if (IS_ERR(pin_state))
		goto err;

	if (pinctrl_select_state(s2803x->pinctrl, pin_state) < 0)
		goto err;

	return;
err:
	dev_err(dev, "Unable to configure mixer gpio as %s\n", name);
	return;
}

static void s2803x_save_regs(struct device *dev)
{
	struct s2803x_priv *s2803x = dev_get_drvdata(dev);
	int i;

	for (i = 0; i < ARRAY_SIZE(s2803x_save_regs_addr); i++)
		regmap_read(s2803x->regmap, s2803x_save_regs_addr[i],
				&s2803x->save_regs_val[i]);
}

static void s2803x_restore_regs(struct device *dev)
{
	struct s2803x_priv *s2803x = dev_get_drvdata(dev);
	int i;

	for (i = 0; i < ARRAY_SIZE(s2803x_save_regs_addr); i++)
		regmap_write(s2803x->regmap, s2803x_save_regs_addr[i],
				s2803x->save_regs_val[i]);
}

#ifdef CONFIG_PM_RUNTIME
static int s2803x_runtime_resume(struct device *dev)
{
	static unsigned int count;

	dev_dbg(dev, "(*) %s (count = %d)\n", __func__, ++count);

	lpass_get_sync(dev);
	s2803x_runtime_power_on(dev);
	s2803x_clk_enable(dev);
	s2803x_cfg_gpio(dev, "bt-idle");
	/* Reset the codec */
	s2803x_reset_sys_data();
	/* set ap path by defaut*/
	s2803x_init_mixer();
	s2803x_restore_regs(dev);
	return 0;
}

static int s2803x_runtime_suspend(struct device *dev)
{
	static unsigned int count;

	dev_dbg(dev, "(*) %s (count = %d)\n", __func__, ++count);

	s2803x_save_regs(dev);
	s2803x_cfg_gpio(dev, "idle");
	s2803x_clk_disable(dev);
	s2803x_runtime_power_off(dev);
	lpass_put_sync(dev);

	return 0;
}

void s2803x_get_sync()
{
	if (s2803x != NULL)
		pm_runtime_get_sync(s2803x->dev);
}

void s2803x_put_sync()
{
	if (s2803x != NULL)
		pm_runtime_put_sync(s2803x->dev);
}
#else
void s2803x_get_sync()
{
}

void s2803x_put_sync()
{
}
#endif

static const struct dev_pm_ops s2803x_pm = {
	SET_SYSTEM_SLEEP_PM_OPS(
			NULL,
			NULL
	)
	SET_RUNTIME_PM_OPS(
			s2803x_runtime_suspend,
			s2803x_runtime_resume,
			NULL
	)
};

static const struct platform_device_id s2803x_apb_id[] = {
	{ "s2803x", 0 },
	{ }
};
MODULE_DEVICE_TABLE(platform, s2803x_id);

static const struct of_device_id s2803x_dt_ids[] = {
	{ .compatible = "samsung,s2803x", },
	{ }
};
MODULE_DEVICE_TABLE(of, s2803x_dt_ids);

static struct platform_driver s2803x_driver = {
	.driver = {
		.name = "s2803x",
		.owner = THIS_MODULE,
		.pm = &s2803x_pm,
		.of_match_table = of_match_ptr(s2803x_dt_ids),
	},
	.probe = s2803x_apb_probe,
	.remove = s2803x_apb_remove,
	.id_table = s2803x_apb_id,
};

module_platform_driver(s2803x_driver);

MODULE_DESCRIPTION("ASoC SRC2803X driver");
MODULE_AUTHOR("Tushar Behera <tushar.b@samsung.com>");
MODULE_AUTHOR("Sayanta Pattanayak <sayanta.p@samsung.com>");
MODULE_AUTHOR("R Chandrasekar <rcsekar@samsung.com>");
MODULE_LICENSE("GPL v2");
MODULE_FIRMWARE(S2803X_FIRMWARE_NAME);
