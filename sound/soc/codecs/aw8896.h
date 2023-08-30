#ifndef _AW8896_H_
#define _AW8896_H_
#include <linux/version.h>
#include <sound/control.h>
#include <sound/soc.h>

#if KERNEL_VERSION(4, 19, 1) <= LINUX_VERSION_CODE
#define AW_KERNEL_VER_OVER_4_19_1
#endif

#ifdef AW_KERNEL_VER_OVER_4_19_1
#define aw_snd_soc_codec_t struct snd_soc_component
#define aw_snd_soc_codec_driver_t const struct snd_soc_component_driver
#else
#define aw_snd_soc_codec_t struct snd_soc_codec
#define aw_snd_soc_codec_driver_t const struct snd_soc_codec_driver
#endif

struct aw_componet_codec_ops {
	aw_snd_soc_codec_t *(*aw_snd_soc_kcontrol_codec)(
				struct snd_kcontrol *kcontrol);
	void *(*aw_snd_soc_codec_get_drvdata)(aw_snd_soc_codec_t *codec);
	int (*aw_snd_soc_add_codec_controls)(aw_snd_soc_codec_t *codec,
		const struct snd_kcontrol_new *controls,
		unsigned int num_controls);
	void (*aw_snd_soc_unregister_codec)(struct device *dev);
	int (*aw_snd_soc_register_codec)(struct device *dev,
			aw_snd_soc_codec_driver_t *codec_drv,
			struct snd_soc_dai_driver *dai_drv,
			int num_dai);
};


/*
 * i2c transaction on Linux limited to 64k
 * (See Linux kernel documentation: Documentation/i2c/writing-clients)
 */
#define MAX_I2C_BUFFER_SIZE 65536
#define MAX_RAM_WRITE_BYTE_SIZE         128

#define AW8896_FLAG_DSP_START_ON_MUTE   (1 << 0)
#define AW8896_FLAG_SKIP_INTERRUPTS     (1 << 1)
#define AW8896_FLAG_SAAM_AVAILABLE      (1 << 2)
#define AW8896_FLAG_STEREO_DEVICE       (1 << 3)
#define AW8896_FLAG_MULTI_MIC_INPUTS    (1 << 4)
#define AW8896_FLAG_DSP_START_ON        (1 << 5)

#define AW8896_NUM_RATES                9

#define AW8896_MAX_REGISTER             0xff

#define AWINIC_DSP_I2C_WRITES
#define AW8896_DSP_FW_BASE              0x8c00
#define AW8896_DSP_CFG_BASE             0x8380
#define AW8896_DSP_FW_VER_BASE          0x0f80


enum aw8896_chipid {
	AW8990_ID,
};

enum aw8896_mode_spk_rcv {
	AW8896_SPEAKER_MODE = 0,
	AW8896_RECEIVER_MODE = 1,
};

enum aw8896_dsp_fw_version {
	AW8896_DSP_FW_VER_D = 0,
	AW8896_DSP_FW_VER_E = 1,
};

enum aw8896_dsp_init_state {
	AW8896_DSP_INIT_STOPPED,        /* DSP not running */
	AW8896_DSP_INIT_RECOVER,        /* DSP error detected at runtime */
	AW8896_DSP_INIT_FAIL,           /* DSP init failed */
	AW8896_DSP_INIT_PENDING,        /* DSP start requested */
	AW8896_DSP_INIT_DONE,           /* DSP running */
	AW8896_DSP_INIT_INVALIDATED,    /* DSP was running, requires re-init */
};

enum aw8896_dsp_fw_state {
	AW8896_DSP_FW_NONE = 0,
	AW8896_DSP_FW_PENDING,
	AW8896_DSP_FW_FAIL,
	AW8896_DSP_FW_FAIL_COUNT,
	AW8896_DSP_FW_FAIL_REG_DSP,
	AW8896_DSP_FW_FAIL_PROBE,
	AW8896_DSP_FW_OK,
};

enum aw8896_dsp_cfg_state {
	AW8896_DSP_CFG_NONE = 0,
	AW8896_DSP_CFG_PENDING,
	AW8896_DSP_CFG_FAIL,
	AW8896_DSP_CFG_FAIL_COUNT,
	AW8896_DSP_CFG_FAIL_REG_DSP,
	AW8896_DSP_CFG_FAIL_PROBE,
	AW8896_DSP_CFG_OK,
};


struct aw8896 {
	struct regmap *regmap;
	struct i2c_client *i2c;
	aw_snd_soc_codec_t *codec;
	struct mutex lock;
	int dsp_init;
	int dsp_fw_state;
	int dsp_cfg_state;
	int dsp_fw_len;
	int dsp_cfg_len;
	int sysclk;
	u16 rev;
	int rate;
	struct device *dev;
	struct input_dev *input;

	int rst_gpio;
	int reset_gpio;
	int power_gpio;
	int irq_gpio;

#ifdef CONFIG_DEBUG_FS
	struct dentry *dbg_dir;
#endif
	u8 reg;

	unsigned int flags;
	unsigned int chipid;
	unsigned int init;
	unsigned int spk_rcv_mode;
	unsigned int call_in_kctl;
};

struct aw8896_container {
	int len;
	unsigned char data[];
};




int aw8896_i2c_probe(struct i2c_client *i2c, const struct i2c_device_id *id);
int aw8896_i2c_remove(struct i2c_client *i2c);
#endif
