/* drivers/motor/isa1000.c

 * Copyright (C) 2014 Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt) "[VIB] isa1000_vib: " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/hrtimer.h>
#include <linux/pwm.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/vibrator/sec_vibrator.h>

#if defined(CONFIG_MTK_PWM)
struct pwm_spec_config {
	u32 pwm_no;
	u32 mode;
	u32 clk_div;
	u32 clk_src;
	u8 intr;
	u8 pmic_pad;

	union {
		/* for old mode */
		struct _PWM_OLDMODE_REGS {
			u16 IDLE_VALUE;
			u16 GUARD_VALUE;
			u16 GDURATION;
			u16 WAVE_NUM;
			u16 DATA_WIDTH;
			u16 THRESH;
		} PWM_MODE_OLD_REGS;

		/* for fifo mode */
		struct _PWM_MODE_FIFO_REGS {
			u32 IDLE_VALUE;
			u32 GUARD_VALUE;
			u32 STOP_BITPOS_VALUE;
			u16 HDURATION;
			u16 LDURATION;
			u32 GDURATION;
			u32 SEND_DATA0;
			u32 SEND_DATA1;
			u32 WAVE_NUM;
		} PWM_MODE_FIFO_REGS;

		/* for memory mode */
		struct _PWM_MODE_MEMORY_REGS {
			u32 IDLE_VALUE;
			u32 GUARD_VALUE;
			u32 STOP_BITPOS_VALUE;
			u16 HDURATION;
			u16 LDURATION;
			u16 GDURATION;
			dma_addr_t BUF0_BASE_ADDR;
			u32 BUF0_SIZE;
			u16 WAVE_NUM;
		} PWM_MODE_MEMORY_REGS;

		/* for RANDOM mode */
		struct _PWM_MODE_RANDOM_REGS {
			u16 IDLE_VALUE;
			u16 GUARD_VALUE;
			u32 STOP_BITPOS_VALUE;
			u16 HDURATION;
			u16 LDURATION;
			u16 GDURATION;
			dma_addr_t BUF0_BASE_ADDR;
			u32 BUF0_SIZE;
			dma_addr_t BUF1_BASE_ADDR;
			u32 BUF1_SIZE;
			u16 WAVE_NUM;
			u32 VALID;
		} PWM_MODE_RANDOM_REGS;

		/* for seq mode */
		struct _PWM_MODE_DELAY_REGS {
			/* u32 ENABLE_DELAY_VALUE; */
			u16 PWM3_DELAY_DUR;
			u32 PWM3_DELAY_CLK;
			/* 0: block clock source, 1: block/1625 clock source */
			u16 PWM4_DELAY_DUR;
			u32 PWM4_DELAY_CLK;
			u16 PWM5_DELAY_DUR;
			u32 PWM5_DELAY_CLK;
		} PWM_MODE_DELAY_REGS;

	};
};

enum INFRA_CLK_SRC_CTRL {
	CLK_32K = 0x00,
		CLK_26M = 0x01,
	CLK_78M = 0x2,
	CLK_SEL_TOPCKGEN = 0x3,
};

enum PWM_MODE_ENUM {
	PWM_MODE_MIN,
		PWM_MODE_OLD = PWM_MODE_MIN,
	PWM_MODE_FIFO,
	PWM_MODE_MEMORY,
	PWM_MODE_RANDOM,
	PWM_MODE_DELAY,
	PWM_MODE_INVALID,
};

enum PWM_CLK_DIV {
	CLK_DIV_MIN,
	CLK_DIV1 = CLK_DIV_MIN,
	CLK_DIV2,
	CLK_DIV4,
	CLK_DIV8,
	CLK_DIV16,
	CLK_DIV32,
	CLK_DIV64,
	CLK_DIV128,
	CLK_DIV_MAX
};

enum PWM_CLOCK_SRC_ENUM {
	PWM_CLK_SRC_MIN,
	PWM_CLK_OLD_MODE_BLOCK = PWM_CLK_SRC_MIN,
	PWM_CLK_OLD_MODE_32K,
	PWM_CLK_NEW_MODE_BLOCK,
	PWM_CLK_NEW_MODE_BLOCK_DIV_BY_1625,
	PWM_CLK_SRC_NUM,
	PWM_CLK_SRC_INVALID,
};

enum PWM_CON_IDLE_BIT {
	IDLE_FALSE,
	IDLE_TRUE,
	IDLE_MAX
};

enum PWM_CON_GUARD_BIT {
	GUARD_FALSE,
	GUARD_TRUE,
	GUARD_MAX
};

s32 pwm_set_spec_config(struct pwm_spec_config *conf);
void mt_pwm_disable(u32 pwm_no, u8 pmic_pad);
void  mt_pwm_clk_sel_hal(u32 pwm, u32 clk_src);
//#include <mt-plat/mtk_pwm.h>   //chipset based inclusion selection though MTK_PLATFORM in makefile
//#include <mach/mtk_pwm_hal.h>


void mt_pwm_power_on(u32 pwm_no, bool pmic_pad);


static struct pwm_spec_config motor_pwm_config = {
	.pwm_no = 0,
	.mode = PWM_MODE_FIFO,                           /*mode used for pwm generation*/
	.clk_div = CLK_DIV2,                             /*clk src division factor*/
	.clk_src = PWM_CLK_NEW_MODE_BLOCK,
	.pmic_pad = 0,
	.PWM_MODE_FIFO_REGS.IDLE_VALUE = IDLE_FALSE,
	.PWM_MODE_FIFO_REGS.GUARD_VALUE = GUARD_FALSE,
	.PWM_MODE_FIFO_REGS.STOP_BITPOS_VALUE = 61,      /*(value + 1) number of bits used in pwm generation*/
	.PWM_MODE_FIFO_REGS.HDURATION = 7,               /*number of clk cycles for which high bit is set*/
	.PWM_MODE_FIFO_REGS.LDURATION = 7,               /*number of clk cycles for which low bit is set*/
	.PWM_MODE_FIFO_REGS.GDURATION = 0,
	.PWM_MODE_FIFO_REGS.SEND_DATA0 = 0xFFFFFFFE,     /*first pwm data block writing starts from LSB*/
	.PWM_MODE_FIFO_REGS.SEND_DATA1 = 0x1FFFFF,       /*second pwm data block writing starts from LSB*/
	.PWM_MODE_FIFO_REGS.WAVE_NUM = 0,
};
#endif // CONFIG_MTK_PWM


#define ISA1000_DIVIDER		128
#define FREQ_DIVIDER		NSEC_PER_SEC / ISA1000_DIVIDER * 10

struct isa1000_pdata {
 	int gpio_en;
	const char *regulator_name;
	struct pwm_device *pwm;
	struct regulator *regulator;
	const char *motor_type;

	int frequency;
	int duty_ratio;
	/* for multi-frequency */
	int freq_nums;
	u32 *freq_array;

#if defined(CONFIG_SEC_VIBRATOR)
	bool calibration;
	int steps;
	int *intensities;
	int *haptic_intensities;
#endif
};

struct isa1000_ddata {
	struct isa1000_pdata *pdata;
	struct sec_vibrator_drvdata sec_vib_ddata;
	int max_duty;
	int duty;
	int period;
};

static int isa1000_vib_set_frequency(struct device *dev, int num)
{
	struct isa1000_ddata *ddata = dev_get_drvdata(dev);

	if (num >= 0 && num < ddata->pdata->freq_nums) {
		ddata->period = FREQ_DIVIDER / ddata->pdata->freq_array[num];
		ddata->duty = ddata->max_duty =
			(ddata->period * ddata->pdata->duty_ratio) / 100;
	} else if (num >= HAPTIC_ENGINE_FREQ_MIN &&
			num <= HAPTIC_ENGINE_FREQ_MAX) {
		ddata->period = FREQ_DIVIDER / num;
		ddata->duty = ddata->max_duty =
			(ddata->period * ddata->pdata->duty_ratio) / 100;
	} else {
		pr_err("%s out of range %d\n", __func__, num);
		return -EINVAL;
	}

	return 0;
}

#if defined(CONFIG_MTK_PWM)
static int isa1000_vib_set_ratio(int ratio)
{
	int send_data1_bits_limit;
	int bit_size = motor_pwm_config.PWM_MODE_FIFO_REGS.STOP_BITPOS_VALUE + 1;

	send_data1_bits_limit = bit_size * ratio / 100;
	send_data1_bits_limit -= (bit_size / 2);

	pr_info("send_data1_bits_limit set to %d\n", send_data1_bits_limit);

	return send_data1_bits_limit;
}
#endif

static int isa1000_vib_set_intensity(struct device *dev, int intensity)
{
	struct isa1000_ddata *ddata = dev_get_drvdata(dev);
	int duty = ddata->period >> 1;
	int ret = 0;
	int ratio = 100;
#if defined(CONFIG_MTK_PWM)
	int send_data1_bits_limit;
#endif

	if (intensity < -(MAX_INTENSITY) || MAX_INTENSITY < intensity) {
		pr_err("%s out of range %d\n", __func__, intensity);
		return -EINVAL;
	}

	if (intensity == MAX_INTENSITY)
		duty = ddata->max_duty;
	else if (intensity == -(MAX_INTENSITY))
		duty = ddata->period - ddata->max_duty;
	else if (intensity != 0)
		duty += (ddata->max_duty - duty) * intensity / MAX_INTENSITY;

	ddata->duty = duty;

	ratio = sec_vibrator_recheck_ratio(&ddata->sec_vib_ddata);

#if defined(CONFIG_MTK_PWM)
	send_data1_bits_limit = isa1000_vib_set_ratio(ratio);
	motor_pwm_config.PWM_MODE_FIFO_REGS.SEND_DATA1 = (1 << (send_data1_bits_limit * intensity / MAX_INTENSITY)) - 1;
	mt_pwm_power_on(motor_pwm_config.pwm_no, 0);
	mt_pwm_clk_sel_hal(0, CLK_26M);
#else
	ret = pwm_config(ddata->pdata->pwm, duty, ddata->period);
	if (ret < 0) {
		pr_err("failed to config pwm %d\n", ret);
		return ret;
	}
#endif
	if (intensity != 0) {
#if defined(CONFIG_MTK_PWM)
		ret = pwm_set_spec_config(&motor_pwm_config);
#else
		ret = pwm_enable(ddata->pdata->pwm);
#endif
		if (ret < 0)
			pr_err("failed to enable pwm %d\n", ret);
	} else {
#if defined(CONFIG_MTK_PWM)
		mt_pwm_disable(motor_pwm_config.pwm_no, motor_pwm_config.pmic_pad);
#else
		pwm_disable(ddata->pdata->pwm);
#endif
	}

	return ret;
}

static void isa1000_regulator_en(struct isa1000_ddata *ddata, bool en)
{
	int ret;

	if (ddata->pdata->regulator == NULL)
		return;

	if (en && !regulator_is_enabled(ddata->pdata->regulator)) {
		ret = regulator_enable(ddata->pdata->regulator);
		if (ret)
			pr_err("regulator_enable returns: %d\n", ret);
	} else if (!en && regulator_is_enabled(ddata->pdata->regulator)) {
		ret = regulator_disable(ddata->pdata->regulator);
		if (ret)
			pr_err("regulator_disable returns: %d\n", ret);
	}
}

static void isa1000_gpio_en(struct isa1000_ddata *ddata, bool en)
{
	if (gpio_is_valid(ddata->pdata->gpio_en))
		gpio_direction_output(ddata->pdata->gpio_en, en);
}

static int isa1000_vib_enable(struct device *dev, bool en)
{
	struct isa1000_ddata *ddata = dev_get_drvdata(dev);

	if (en) {
		isa1000_regulator_en(ddata, en);
		isa1000_gpio_en(ddata, en);
	} else {
		isa1000_gpio_en(ddata, en);
		isa1000_regulator_en(ddata, en);
	}

	return 0;
}

static int isa1000_get_motor_type(struct device *dev, char *buf)
{
	struct isa1000_ddata *ddata = dev_get_drvdata(dev);
	int ret = snprintf(buf, VIB_BUFSIZE, "%s\n", ddata->pdata->motor_type);

	return ret;
}

#if defined(CONFIG_SEC_VIBRATOR)
static bool isa1000_get_calibration(struct device *dev)
{
	struct isa1000_ddata *ddata = dev_get_drvdata(dev);
	struct isa1000_pdata *pdata = ddata->pdata;

	return pdata->calibration;
}

static int isa1000_get_step_size(struct device *dev, int *step_size)
{
	struct isa1000_ddata *ddata = dev_get_drvdata(dev);
	struct isa1000_pdata *pdata = ddata->pdata;

	pr_info("%s step_size=%d\n", __func__, pdata->steps);

	if (pdata->steps == 0)
		return -ENODATA;

	*step_size = pdata->steps;

	return 0;
}


static int isa1000_get_intensities(struct device *dev, int *buf)
{
	struct isa1000_ddata *ddata = dev_get_drvdata(dev);
	struct isa1000_pdata *pdata = ddata->pdata;
	int i;

	if (pdata->intensities[1] == 0)
		return -ENODATA;

	for (i = 0; i < pdata->steps; i++)
		buf[i] = pdata->intensities[i];

	return 0;
}

static int isa1000_set_intensities(struct device *dev, int *buf)
{
	struct isa1000_ddata *ddata = dev_get_drvdata(dev);
	struct isa1000_pdata *pdata = ddata->pdata;
	int i;

	for (i = 0; i < pdata->steps; i++)
		pdata->intensities[i] = buf[i];

	return 0;
}

static int isa1000_get_haptic_intensities(struct device *dev, int *buf)
{
	struct isa1000_ddata *ddata = dev_get_drvdata(dev);
	struct isa1000_pdata *pdata = ddata->pdata;
	int i;

	if (pdata->haptic_intensities[1] == 0)
		return -ENODATA;

	for (i = 0; i < pdata->steps; i++)
		buf[i] = pdata->haptic_intensities[i];

	return 0;
}

static int isa1000_set_haptic_intensities(struct device *dev, int *buf)
{
	struct isa1000_ddata *ddata = dev_get_drvdata(dev);
	struct isa1000_pdata *pdata = ddata->pdata;
	int i;

	for (i = 0; i < pdata->steps; i++)
		pdata->haptic_intensities[i] = buf[i];

	return 0;
}
#endif /* if defined(CONFIG_SEC_VIBRATOR) */

static const struct sec_vibrator_ops isa1000_multi_freq_vib_ops = {
	.enable = isa1000_vib_enable,
	.set_intensity = isa1000_vib_set_intensity,
	.set_frequency = isa1000_vib_set_frequency,
	.get_motor_type = isa1000_get_motor_type,
#if defined(CONFIG_SEC_VIBRATOR)
	.get_calibration = isa1000_get_calibration,
	.get_step_size = isa1000_get_step_size,
	.get_intensities = isa1000_get_intensities,
	.set_intensities = isa1000_set_intensities,
	.get_haptic_intensities = isa1000_get_haptic_intensities,
	.set_haptic_intensities = isa1000_set_haptic_intensities,
#endif
};

static const struct sec_vibrator_ops isa1000_single_freq_vib_ops = {
	.enable = isa1000_vib_enable,
	.set_intensity = isa1000_vib_set_intensity,
	.get_motor_type = isa1000_get_motor_type,
#if defined(CONFIG_SEC_VIBRATOR)
	.get_calibration = isa1000_get_calibration,
	.get_step_size = isa1000_get_step_size,
	.get_intensities = isa1000_get_intensities,
	.set_intensities = isa1000_set_intensities,
	.get_haptic_intensities = isa1000_get_haptic_intensities,
	.set_haptic_intensities = isa1000_set_haptic_intensities,
#endif
};

#if defined(CONFIG_SEC_VIBRATOR)
static int of_sec_vibrator_dt(struct isa1000_pdata *pdata, struct device_node *np)
{
	int ret = 0;
	int i;
	unsigned int val = 0;
	int *intensities = NULL;

	pr_info("%s\n", __func__);
	pdata->calibration = false;

	/* number of steps */
	ret = of_property_read_u32(np, "samsung,steps", &val);
	if (ret) {
		pr_err("%s out of range(%d)\n", __func__, val);
		return -EINVAL;
	}
	pdata->steps = (int)val;

	/* allocate memory for intensities */
	pdata->intensities = kmalloc_array(pdata->steps, sizeof(int), GFP_KERNEL);
	if (!pdata->intensities)
		return -ENOMEM;
	intensities = pdata->intensities;

	/* intensities */
	ret = of_property_read_u32_array(np, "samsung,intensities", intensities, pdata->steps);
	if (ret) {
		pr_err("intensities are not specified\n");
		ret = -EINVAL;
		goto err_getting_int;
	}

	for (i = 0; i < pdata->steps; i++) {
		if ((intensities[i] < 0) || (intensities[i] > MAX_INTENSITY)) {
			pr_err("%s out of range(%d)\n", __func__, intensities[i]);
			ret = -EINVAL;
			goto err_getting_int;
		}
		}
	intensities = NULL;

	/* allocate memory for haptic_intensities */
	pdata->haptic_intensities = kmalloc_array(pdata->steps, sizeof(int), GFP_KERNEL);
	if (!pdata->haptic_intensities) {
		ret = -ENOMEM;
		goto err_alloc_haptic;
	}
	intensities = pdata->haptic_intensities;

	/* haptic intensities */
	ret = of_property_read_u32_array(np, "samsung,haptic_intensities", intensities, pdata->steps);
	if (ret) {
		pr_err("haptic_intensities are not specified\n");
		ret = -EINVAL;
		goto err_haptic;
	}
	for (i = 0; i < pdata->steps; i++) {
		if ((intensities[i] < 0) || (intensities[i] > MAX_INTENSITY)) {
			pr_err("%s out of range(%d)\n", __func__, intensities[i]);
			ret = -EINVAL;
			goto err_haptic;
		}
	}

	/* update calibration statue */
	pdata->calibration = true;

	return ret;

err_haptic:
	kfree(pdata->haptic_intensities);
err_alloc_haptic:
	pdata->haptic_intensities = NULL;
err_getting_int:
	kfree(pdata->intensities);
	pdata->intensities = NULL;
	pdata->steps = 0;

	return ret;
}
#endif /* if defined(CONFIG_SEC_VIBRATOR) */

static struct isa1000_pdata *isa1000_get_devtree_pdata(struct device *dev)
{
	struct device_node *node;
	struct isa1000_pdata *pdata;
	u32 temp;
	int ret = 0;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return NULL;

	node = dev->of_node;
	if (!node) {
		pr_err("%s: error to get dt node\n", __func__);
		goto err_out;
	}

	ret = of_property_read_u32(node, "isa1000,multi_frequency", &temp);
	if (ret) {
		pr_info("%s: multi_frequency isn't used\n", __func__);
		pdata->freq_nums = 0;
	} else
		pdata->freq_nums = (int)temp;

	if (pdata->freq_nums) {
		pdata->freq_array = devm_kzalloc(dev,
				sizeof(u32)*pdata->freq_nums, GFP_KERNEL);
		if (!pdata->freq_array) {
			pr_err("%s: failed to allocate freq_array data\n",
					__func__);
			goto err_out;
		}

		ret = of_property_read_u32_array(node, "isa1000,frequency",
				pdata->freq_array, pdata->freq_nums);
		if (ret) {
			pr_err("%s: error to get dt node frequency\n",
					__func__);
			goto err_out;
		}

		pdata->frequency = pdata->freq_array[0];
	} else {
		ret = of_property_read_u32(node,
				"isa1000,frequency", &temp);
		if (ret) {
			pr_err("%s: error to get dt node frequency\n",
					__func__);
			goto err_out;
		} else
			pdata->frequency = (int)temp;
	}

	ret = of_property_read_u32(node, "isa1000,duty_ratio",
			&pdata->duty_ratio);
	if (ret) {
		pr_err("%s: error to get dt node duty_ratio\n", __func__);
		goto err_out;
	}

	ret = of_property_read_string(node, "isa1000,regulator_name",
			&pdata->regulator_name);
	if (!ret) {
		pdata->regulator = regulator_get(NULL, pdata->regulator_name);
		if (IS_ERR(pdata->regulator)) {
			ret = PTR_ERR(pdata->regulator);
			pdata->regulator = NULL;
			pr_err("%s: regulator get fail\n", __func__);
			goto err_out;
		}
	} else {
		pr_info("%s: regulator isn't used\n", __func__);
		pdata->regulator = NULL;
	}

	pdata->gpio_en = of_get_named_gpio(node, "isa1000,gpio_en", 0);
	if (gpio_is_valid(pdata->gpio_en)) {
		ret = gpio_request(pdata->gpio_en, "isa1000,gpio_en");
		if (ret) {
			pr_err("%s: motor gpio request fail(%d)\n",
				__func__, ret);
			goto err_out;
		}
		ret = gpio_direction_output(pdata->gpio_en, 0);
	} else {
		pr_info("%s: gpio isn't used\n", __func__);
	}

#if defined(CONFIG_MTK_PWM)
	mt_pwm_power_on(0, 0);
#else
	pdata->pwm = devm_of_pwm_get(dev, node, NULL);
	if (IS_ERR(pdata->pwm)) {
		pr_err("%s: error to get pwms : %d?\n", __func__, IS_ERR(pdata->pwm));
		goto err_out;
	}
#endif

	ret = of_property_read_string(node, "isa1000,motor_type",
			&pdata->motor_type);
	if (ret)
		pr_err("%s: motor_type is undefined\n", __func__);

#if defined(CONFIG_SEC_VIBRATOR)
	ret = of_sec_vibrator_dt(pdata, node);
	if (ret < 0)
		pr_err("sec_vibrator dt read fail : %d\n", ret);
#endif

	return pdata;

err_out:
	return ERR_PTR(ret);
}

static int isa1000_probe(struct platform_device *pdev)
{
	struct isa1000_pdata *pdata = dev_get_platdata(&pdev->dev);
	struct isa1000_ddata *ddata;

	if (!pdata) {
#if defined(CONFIG_OF)
		pdata = isa1000_get_devtree_pdata(&pdev->dev);
		if (IS_ERR(pdata)) {
			pr_err("there is no device tree!\n");
			return -ENODEV;
		}
#else
		pr_err("there is no platform data!\n");
#endif
	}

	ddata = devm_kzalloc(&pdev->dev, sizeof(*ddata), GFP_KERNEL);
	if (!ddata) {
		pr_err("failed to alloc\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, ddata);
	ddata->pdata = pdata;
	ddata->period = FREQ_DIVIDER / pdata->frequency;
	ddata->max_duty = ddata->duty =
		ddata->period * ddata->pdata->duty_ratio / 100;

	ddata->sec_vib_ddata.dev = &pdev->dev;
	if (pdata->freq_nums)
		ddata->sec_vib_ddata.vib_ops = &isa1000_multi_freq_vib_ops;
	else
		ddata->sec_vib_ddata.vib_ops = &isa1000_single_freq_vib_ops;
	sec_vibrator_register(&ddata->sec_vib_ddata);

	pr_info("%s - done\n", __func__);

	return 0;
}

static int isa1000_remove(struct platform_device *pdev)
{
	struct isa1000_ddata *ddata = platform_get_drvdata(pdev);

	sec_vibrator_unregister(&ddata->sec_vib_ddata);
	isa1000_vib_enable(&pdev->dev, false);
	return 0;
}

static int isa1000_suspend(struct platform_device *pdev,
			pm_message_t state)
{
	isa1000_vib_enable(&pdev->dev, false);
	return 0;
}

static int isa1000_resume(struct platform_device *pdev)
{
	return 0;
}

#if defined(CONFIG_OF)
static struct of_device_id isa1000_dt_ids[] = {
	{ .compatible = "imagis,isa1000_vibrator" },
	{ }
};
MODULE_DEVICE_TABLE(of, isa1000_dt_ids);
#endif /* CONFIG_OF */

static struct platform_driver isa1000_driver = {
	.probe		= isa1000_probe,
	.remove		= isa1000_remove,
	.suspend	= isa1000_suspend,
	.resume		= isa1000_resume,
	.driver = {
		.name	= "isa1000",
		.owner	= THIS_MODULE,
		.of_match_table	= of_match_ptr(isa1000_dt_ids),
	},
};

static int __init isa1000_init(void)
{
	return platform_driver_register(&isa1000_driver);
}
module_init(isa1000_init);

static void __exit isa1000_exit(void)
{
	platform_driver_unregister(&isa1000_driver);
}
module_exit(isa1000_exit);

MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("isa1000 vibrator driver");
