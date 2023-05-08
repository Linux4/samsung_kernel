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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/poll.h>
#include <linux/fs.h>
#include <linux/irq.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <asm/io.h>
#include <linux/file.h>

#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/of_gpio.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/wakelock.h>

#include <linux/regulator/consumer.h>

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_i2c.h>
#include <linux/kthread.h>

#include "parse_hwinfo.h"

#ifndef CONFIG_64BIT
#include <soc/sprd/dma.h>
#include <soc/sprd/hardware.h>
#include <soc/sprd/board.h>
#include <soc/sprd/regulator.h>
#include <soc/sprd/arch_misc.h>
#include <soc/sprd/adi.h>

#endif
#include <soc/sprd/i2c-sprd.h>

#include <video/sensor_drv_k.h>
#include "sensor_drv_sprd.h"
#include "compat_sensor_drv_k.h"
#include "csi2/csi_api.h"
#ifdef  CONFIG_SC_FPGA
#include "dcam_reg.h"
#endif
#include "power/sensor_power.h"
#include "sensor_reloadinfo_thread.h"

#include <linux/leds.h>
#include <linux/mfd/sm5701_core.h>
#include <../flash/flash.h>

#define SENSOR_CLK                        "clk_sensor"
#define SENSOR_DEVICE_NAME                "sprd_sensor"

#define DEBUG_SENSOR_DRV
#ifdef  DEBUG_SENSOR_DRV
#define SENSOR_PRINT                      pr_debug
#else
#define SENSOR_PRINT(...)
#endif
#define SENSOR_PRINT_ERR                  printk
#define SENSOR_PRINT_HIGH                 printk

#define SENSOR_K_SUCCESS                  0
#define SENSOR_K_FAIL                     (-1)
#define SENSOR_K_FALSE                    0
#define SENSOR_K_TRUE                     1

#ifndef  CONFIG_SC_FPGA
#define LOCAL                             static
#else
#define LOCAL
#endif

#define PNULL                             ((void *)0)

#define NUMBER_MAX                        0x7FFFFFF

#define SENSOR_MINOR                      MISC_DYNAMIC_MINOR
#define SLEEP_MS(ms)                      msleep(ms)

#define SENSOR_I2C_ID                     0
#define SENSOR_I2C_OP_TRY_NUM             3
#define SENSOR_CMD_BITS_8                 1
#define SENSOR_CMD_BITS_16                2
#define SENSOR_I2C_VAL_8BIT               0x00
#define SENSOR_I2C_VAL_16BIT              0x01
#define SENSOR_I2C_REG_8BIT               (0x00 << 1)
#define SENSOR_I2C_REG_16BIT              (0x01 << 1)
#define SENSOR_I2C_CUSTOM                 (0x01 << 2)
#define SENSOR_LOW_EIGHT_BIT              0xff

#define SENSOR_WRITE_DELAY                0xffff
#define DEBUG_STR                         "Error L %d, %s \n"
#define DEBUG_ARGS                        __LINE__,__FUNCTION__
#define SENSOR_MCLK_SRC_NUM               4
#define SENSOR_MCLK_DIV_MAX               4
#define SENSOR_ABS(a)                     ((a) > 0 ? (a) : -(a))
#define SENSOR_LOWEST_ADDR                0x800
#define SENSOR_ADDR_INVALID(addr)         ((unsigned long)(addr) < SENSOR_LOWEST_ADDR)

#if defined(CONFIG_MACH_J1X3G)
#define CAM_FLASH_EN 214
#define CAM_FLASH_TORCH 236
#elif defined(CONFIG_MACH_J3X3G)
#define CAM_FLASH_EN 232
#define CAM_FLASH_TORCH 233
#endif
#define SENSOR_CHECK_ZERO(a)                                   \
	do {                                                       \
		if (SENSOR_ADDR_INVALID(a)) {                          \
			printk("SENSOR, zero pointer \n");                 \
			printk(DEBUG_STR, DEBUG_ARGS);                     \
			return -EFAULT;                                    \
		}                                                      \
	} while(0)

#define SENSOR_CHECK_ZERO_VOID(a)                               \
	do {                                                        \
		if (SENSOR_ADDR_INVALID(a)) {                           \
			printk("SENSOR, zero pointer \n");                  \
			printk(DEBUG_STR, DEBUG_ARGS);                      \
			return;                                             \
		}                                                       \
	} while(0)

struct class *camera_class;

uint32_t flash_torch_status = 0;

LOCAL const struct sensor_mclk_tag c_sensor_mclk_tab[SENSOR_MCLK_SRC_NUM] = {
						{96, "clk_96m"},
						{77, "clk_76m8"},
						{48, "clk_48m"},
						{26, "ext_26m"}
};

LOCAL void* _sensor_k_malloc(struct sensor_file_tag *fd_handle, size_t size)
{
	if (SENSOR_ADDR_INVALID(fd_handle)) {
		printk("SENSOR, zero pointer \n");
		printk(DEBUG_STR, DEBUG_ARGS);
		return PNULL;
	}

	if(PNULL == fd_handle->sensor_mem.buf_ptr) {
		fd_handle->sensor_mem.buf_ptr = vzalloc(size);
		if(PNULL != fd_handle->sensor_mem.buf_ptr) {
			fd_handle->sensor_mem.size = size;
		}

	} else if (size > fd_handle->sensor_mem.size) {
		//realloc memory
		vfree(fd_handle->sensor_mem.buf_ptr);
		fd_handle->sensor_mem.buf_ptr = PNULL;
		fd_handle->sensor_mem.size = 0;
		fd_handle->sensor_mem.buf_ptr = vzalloc(size);
		if (PNULL != fd_handle->sensor_mem.buf_ptr) {
			fd_handle->sensor_mem.size = size;
		}
	}

	return fd_handle->sensor_mem.buf_ptr;
}

LOCAL void _sensor_k_free(struct sensor_file_tag *fd_handle, void *p)
{
	/* memory will not be free */
	return;
}

#if 0
LOCAL struct platform_device*  _sensor_k_get_platform_device(void)
{
	struct device *dev;

	dev = bus_find_device_by_name(&platform_bus_type, NULL, SENSOR_DEVICE_NAME);
	if (!dev) {
		printk("%s error: find device\n", __func__);
		return NULL;
	}

	return to_platform_device(dev);
}
#endif

int sensor_set_pd_level_fromkernel(struct file *file, uint8_t arg, uint32_t sensor_id)
{
	uint32_t ret = SENSOR_K_SUCCESS;
	struct sensor_file_tag *p_file;
	struct sensor_module_tab_tag *p_mod;
	uint8_t pd_level;
	p_file = file->private_data;
	p_mod = p_file->module_data;
	p_file->sensor_id = sensor_id;
	pd_level = arg;

	ret = sensor_k_set_pd_level((uint32_t *)p_file, pd_level);

	return ret;
}

int sensor_k_set_pd_level(uint32_t *fd_handle, uint8_t power_level)
{
	struct sensor_module_tab_tag    *p_mod;
	struct sensor_file_tag          *fd = (struct sensor_file_tag *)fd_handle;

	SENSOR_CHECK_ZERO(fd);
	p_mod = fd->module_data;
	SENSOR_CHECK_ZERO(p_mod);

#ifndef	CONFIG_SC_FPGA_PIN
	get_gpio_id(p_mod->of_node, &fd->gpio_tab.pwn, &fd->gpio_tab.reset, fd->sensor_id);
	SENSOR_PRINT_HIGH("SENSOR: pwn %d \n", fd->gpio_tab.pwn);
	if (0 == power_level) {
		gpio_direction_output(fd->gpio_tab.pwn, 0);
	} else {
		gpio_direction_output(fd->gpio_tab.pwn, 1);
	}
#else
	SENSOR_PRINT_HIGH("CAP_SENSOR_CTRL 0x%lx\n", CAP_SENSOR_CTRL);
	if (0 == power_level) {
		REG_MWR(CAP_SENSOR_CTRL,0x0f,0);
	} else {
		REG_MWR(CAP_SENSOR_CTRL,0x0f,0x0f);
	}
	SENSOR_PRINT_HIGH("CAP_SENSOR_CTRL val=0x%x\n", REG_RD(CAP_SENSOR_CTRL));
#endif

	return SENSOR_K_SUCCESS;
}

LOCAL void _sensor_regulator_disable(struct sensor_file_tag *fd_handle, uint32_t *power_on_count, struct regulator * ptr_cam_regulator)
{
	SENSOR_CHECK_ZERO_VOID(fd_handle);

	while (*power_on_count > 0) {
		regulator_disable(ptr_cam_regulator);
		(*power_on_count)--;
	}
	SENSOR_PRINT("sensor pwr off done: cnt=0x%x, io=%x, av=%x, dv=%x, mo=%x \n", *power_on_count,
		fd_handle->iopower_on_count,
		fd_handle->avddpower_on_count,
		fd_handle->dvddpower_on_count,
		fd_handle->motpower_on_count);

}

LOCAL int _sensor_regulator_enable(struct sensor_file_tag *fd_handle, uint32_t *power_on_count, struct regulator * ptr_cam_regulator)
{
	int err;
	SENSOR_CHECK_ZERO(fd_handle);

	err = regulator_enable(ptr_cam_regulator);
	(*power_on_count)++;

	SENSOR_PRINT("sensor pwr on done: cnt=0x%x, io=%x, av=%x, dv=%x, mo=%x \n", *power_on_count,
		fd_handle->iopower_on_count,
		fd_handle->avddpower_on_count,
		fd_handle->dvddpower_on_count,
		fd_handle->motpower_on_count);

	return err;
}

LOCAL int _sensor_k_get_voltage_value(uint32_t *val)
{
	uint32_t             volt_value = 0;

	SENSOR_CHECK_ZERO(val);

	switch (*val) {
	case SENSOR_VDD_3800MV:
		volt_value = SENSOER_VDD_3800MV;
		break;
	case SENSOR_VDD_3300MV:
		volt_value = SENSOER_VDD_3300MV;
		break;
	case SENSOR_VDD_3000MV:
		volt_value = SENSOER_VDD_3000MV;
		break;
	case SENSOR_VDD_2800MV:
		volt_value = SENSOER_VDD_2800MV;
		break;
	case SENSOR_VDD_2500MV:
		volt_value = SENSOER_VDD_2500MV;
		break;
	case SENSOR_VDD_1800MV:
		volt_value = SENSOER_VDD_1800MV;
		break;
	case SENSOR_VDD_1500MV:
		volt_value = SENSOER_VDD_1500MV;
		break;
	case SENSOR_VDD_1300MV:
		volt_value = SENSOER_VDD_1300MV;
		break;
	case SENSOR_VDD_1200MV:
		volt_value = SENSOER_VDD_1200MV;
		break;
	case SENSOR_VDD_CLOSED:
	case SENSOR_VDD_UNUSED:
	default:
		volt_value = 0;
		break;
	}

	*val = volt_value;

	SENSOR_PRINT("sensor get voltage val: %d \n", *val);

	return SENSOR_K_SUCCESS;
}

LOCAL int _sensor_k_set_voltage(struct sensor_file_tag *fd_handle, uint32_t val, uint32_t type)
{
#ifdef CONFIG_SC_FPGA_LDO
	return 0;
#else
	int                              ret = SENSOR_K_SUCCESS;
	uint32_t                         volt_value = 0;
	uint32_t                         *poweron_count = NULL;
	char                             *regu_name;
	struct sensor_module_tab_tag     *p_mod;
	struct regulator                 **p_regulator = NULL;

	SENSOR_CHECK_ZERO(fd_handle);
	p_mod = fd_handle->module_data;
	SENSOR_CHECK_ZERO(p_mod);

	SENSOR_PRINT("sensor set voltage val: %d \n", val);

	get_regulator_name(p_mod->of_node, &type, fd_handle->sensor_id, &regu_name);

	switch (type) {
	case REGU_CAMAVDD:
		p_regulator = &fd_handle->camavdd_regulator;
		poweron_count = &fd_handle->avddpower_on_count;
		break;

	case REGU_CAMDVDD:
		p_regulator = &fd_handle->camdvdd_regulator;
		poweron_count = &fd_handle->dvddpower_on_count;
		break;

	case REGU_CAMIOVDD:
		p_regulator = &fd_handle->camvio_regulator;
		poweron_count = &fd_handle->iopower_on_count;
		break;

	case REGU_CAMMOT:
		p_regulator = &fd_handle->cammot_regulator;
		poweron_count = &fd_handle->motpower_on_count;
		break;

	default:
		SENSOR_PRINT_ERR("error type = %d\n",type);
		break;
	}

	if ((NULL == poweron_count) || (NULL == p_regulator)) {
		SENSOR_PRINT_ERR("error param");
		return SENSOR_K_FAIL;
	}

	if (NULL == *p_regulator) {
		*p_regulator = regulator_get(NULL, regu_name);
		if (IS_ERR(*p_regulator)) {
			SENSOR_PRINT_ERR("SENSOR:get regu.fail %s %d\n", regu_name, type);
			return SENSOR_K_FAIL;
		}
	}
	volt_value = val;
	_sensor_k_get_voltage_value(&volt_value);

	if (0 != volt_value) {
		ret = regulator_set_voltage(*p_regulator, volt_value, volt_value);
	}
	if (ret) {
		SENSOR_PRINT_ERR("SENSOR:set vol err %d %s %d!.\n", ret, regu_name, type);
		return SENSOR_K_FAIL;
	}

	if (0 != volt_value) {
		ret = _sensor_regulator_enable(fd_handle, poweron_count, *p_regulator);
		if (ret) {
			regulator_put(*p_regulator);
			*p_regulator = NULL;
			SENSOR_PRINT_ERR("SENSOR:can't en %d %s %d!.\n", ret, regu_name, type);
			return SENSOR_K_FAIL;
		}
	} else {
		_sensor_regulator_disable(fd_handle, poweron_count, *p_regulator);
		regulator_put(*p_regulator);
		*p_regulator = NULL;
		SENSOR_PRINT("SENSOR:dis regu.\n");
	}

	SENSOR_PRINT_HIGH("SENSOR:set vol %d %s %d!.\n", volt_value, regu_name, type);

	return ret;
#endif
}

int sensor_set_voltage_cammot_fromkernel(struct file *file, SENSOR_VDD_VAL_E arg, uint32_t sensor_id)
{
	uint32_t ret = SENSOR_K_SUCCESS;
	struct sensor_file_tag *p_file;
	struct sensor_module_tab_tag *p_mod;
	SENSOR_VDD_VAL_E cammot_vdd;
	p_file = file->private_data;
	p_mod = p_file->module_data;
	p_file->sensor_id = sensor_id;
	cammot_vdd = arg;

	ret = sensor_k_set_voltage_cammot((uint32_t *)p_file, (uint32_t)cammot_vdd);

	return ret;
}

int sensor_k_set_voltage_cammot(uint32_t *fd_handle, uint32_t cammot_val)
{
	return _sensor_k_set_voltage((struct sensor_file_tag *)fd_handle, cammot_val, REGU_CAMMOT);
}

int sensor_set_voltage_avdd_fromkernel(struct file *file, SENSOR_VDD_VAL_E arg, uint32_t sensor_id)
{
	uint32_t ret = SENSOR_K_SUCCESS;
	struct sensor_file_tag *p_file;
	struct sensor_module_tab_tag *p_mod;
	SENSOR_VDD_VAL_E avdd;
	p_file = file->private_data;
	p_mod = p_file->module_data;
	p_file->sensor_id = sensor_id;
	avdd = arg;

	ret = sensor_k_set_voltage_avdd((uint32_t *)p_file, (uint32_t)avdd);

	return ret;
}

int sensor_k_set_voltage_avdd(uint32_t *fd_handle, uint32_t avdd_val)
{
	return _sensor_k_set_voltage((struct sensor_file_tag *)fd_handle, avdd_val, REGU_CAMAVDD);
}

int sensor_set_voltage_dvdd_fromkernel(struct file *file, SENSOR_VDD_VAL_E arg, uint32_t sensor_id)
{
	uint32_t ret = SENSOR_K_SUCCESS;
	struct sensor_file_tag *p_file;
	struct sensor_module_tab_tag *p_mod;
	SENSOR_VDD_VAL_E dvdd;
	p_file = file->private_data;
	p_mod = p_file->module_data;
	p_file->sensor_id = sensor_id;
	dvdd = arg;

	ret = sensor_k_set_voltage_dvdd((uint32_t *)p_file, (uint32_t)dvdd);

	return ret;
}

int sensor_k_set_voltage_dvdd(uint32_t *fd_handle, uint32_t dvdd_val)
{
#ifdef CONFIG_SC_FPGA_LDO
	return 0;
#else
	int                              gpio_id = 0;
	struct sensor_module_tab_tag     *p_mod;
	struct sensor_file_tag           *fd = (struct sensor_file_tag *)fd_handle;

	SENSOR_CHECK_ZERO(fd);
	p_mod = fd->module_data;
	SENSOR_CHECK_ZERO(p_mod);
	get_gpio_id_ex(p_mod->of_node, GPIO_MAINDVDD, &gpio_id, fd->sensor_id);
	if (SENSOR_DEV_0 == fd->sensor_id && 0 != gpio_id) {
		SENSOR_PRINT_HIGH("sensor set GPIO_MAINDVDD gpio %d\n", gpio_id);
		if (SENSOR_VDD_CLOSED == dvdd_val) {
			gpio_direction_output(gpio_id, 1);
			gpio_set_value(gpio_id, 0);
		} else {
			gpio_direction_output(gpio_id, 1);
			gpio_set_value(gpio_id, 1);
		}
		return SENSOR_K_SUCCESS;
	}

	gpio_id = 0;
	get_gpio_id_ex(p_mod->of_node, GPIO_SUBDVDD, &gpio_id, fd->sensor_id);
	if (SENSOR_DEV_1 == fd->sensor_id && 0 != gpio_id) {
		SENSOR_PRINT_HIGH("sensor set GPIO_SUBDVDD gpio %d\n", gpio_id);
		if (SENSOR_VDD_CLOSED == dvdd_val) {
			gpio_direction_output(gpio_id, 1);
			gpio_set_value(gpio_id, 0);
		} else {
			gpio_direction_output(gpio_id, 1);
			gpio_set_value(gpio_id, 1);
		}
		return SENSOR_K_SUCCESS;
	}

	get_gpio_id_ex(p_mod->of_node, GPIO_SUB2DVDD, &gpio_id, fd->sensor_id);
	if (SENSOR_DEV_2 == fd->sensor_id && 0 != gpio_id) {
		SENSOR_PRINT_HIGH("sensor set GPIO_SUB2DVDD gpio %d\n", gpio_id);
		if (SENSOR_VDD_CLOSED == dvdd_val) {
			gpio_direction_output(gpio_id, 1);
			gpio_set_value(gpio_id, 0);
		} else {
			gpio_direction_output(gpio_id, 1);
			gpio_set_value(gpio_id, 1);
		}
		return SENSOR_K_SUCCESS;
	}

	return _sensor_k_set_voltage((struct sensor_file_tag *)fd_handle, dvdd_val, REGU_CAMDVDD);
#endif
}

int sensor_set_voltage_iovdd_fromkernel(struct file *file,SENSOR_VDD_VAL_E arg, uint32_t sensor_id)
{
	uint32_t ret = SENSOR_K_SUCCESS;
	struct sensor_file_tag *p_file;
	struct sensor_module_tab_tag *p_mod;
	SENSOR_VDD_VAL_E iovdd;
	p_file = file->private_data;
	p_mod = p_file->module_data;
	p_file->sensor_id = sensor_id;
	iovdd = arg;

	ret = sensor_k_set_voltage_iovdd((uint32_t *)p_file, (uint32_t)iovdd);

	return ret;
}

int sensor_k_set_voltage_iovdd(uint32_t *fd_handle, uint32_t iodd_val)
{
	return _sensor_k_set_voltage((struct sensor_file_tag *)fd_handle, iodd_val, REGU_CAMIOVDD);
}

LOCAL int _select_sensor_mclk(struct sensor_file_tag *fd_handle, uint8_t clk_set, char **clk_src_name,
			uint8_t * clk_div)
{
	uint8_t               i = 0;
	uint8_t               j = 0;
	uint8_t               mark_src = 0;
	uint8_t               mark_div = 0;
	uint8_t               mark_src_tmp = 0;
	int                   clk_tmp = NUMBER_MAX;
	int                   src_delta = NUMBER_MAX;
	int                   src_delta_min = NUMBER_MAX;
	int                   div_delta_min = NUMBER_MAX;
	SENSOR_CHECK_ZERO(fd_handle);

	SENSOR_PRINT("SENSOR sel mclk %d.\n", clk_set);
	if (clk_set > 96 || !clk_src_name || !clk_div) {
		return SENSOR_K_FAIL;
	}
	for (i = 0; i < SENSOR_MCLK_DIV_MAX; i++) {
		clk_tmp = (int)(clk_set * (i + 1));
		src_delta_min = NUMBER_MAX;
		for (j = 0; j < SENSOR_MCLK_SRC_NUM; j++) {
			src_delta = c_sensor_mclk_tab[j].clock - clk_tmp;
			src_delta = SENSOR_ABS(src_delta);
			if (src_delta < src_delta_min) {
				src_delta_min = src_delta;
				mark_src_tmp = j;
			}
		}
		if (src_delta_min < div_delta_min) {
			div_delta_min = src_delta_min;
			mark_src = mark_src_tmp;
			mark_div = i;
		}
	}
	SENSOR_PRINT("src %d, div=%d .\n", mark_src,
		mark_div);

	*clk_src_name = c_sensor_mclk_tab[mark_src].src_name;
	*clk_div = mark_div + 1;

	return SENSOR_K_SUCCESS;
}

int32_t _sensor_k_mipi_clk_en(struct sensor_file_tag *fd_handle, struct device_node *dn)
{
#ifdef CONFIG_SC_FPGA_CLK
	return 0;
#else
	int                     ret = 0;

	SENSOR_CHECK_ZERO(fd_handle);

	if (NULL == fd_handle->mipi_clk) {
		fd_handle->mipi_clk = parse_clk(dn,"clk_dcam_mipi");
	}

	if (IS_ERR(fd_handle->mipi_clk)) {
		printk("SENSOR: get dcam mipi clk error \n");
		return -1;
	} else {
		ret = clk_enable(fd_handle->mipi_clk);
		if (ret) {
			printk("SENSOR: enable dcam mipi clk error %d \n", ret);
			return -1;
		}
	}

	return ret;
#endif
}

int32_t _sensor_k_mipi_clk_dis(struct sensor_file_tag *fd_handle)
{
#ifdef CONFIG_SC_FPGA_CLK
	return 0;
#else
	SENSOR_CHECK_ZERO(fd_handle);

	if (fd_handle->mipi_clk) {
		clk_disable(fd_handle->mipi_clk);
		clk_put(fd_handle->mipi_clk);
		fd_handle->mipi_clk = NULL;
	}
	return 0;
#endif
}

LOCAL int _sensor_k_set_mclk(struct sensor_file_tag *fd_handle, struct device_node *dn, uint32_t mclk)
{
#ifdef CONFIG_SC_FPGA_CLK
	return 0;
#else
	struct clk                *clk_parent = NULL;
	int                       ret;
	char                      *clk_src_name = NULL;
	uint8_t                   clk_div;
	SENSOR_CHECK_ZERO(fd_handle);

	SENSOR_PRINT_HIGH("SENSOR: set mclk org = %d, clk = %d\n",
				fd_handle->sensor_mclk, mclk);

	if ((0 != mclk) && (fd_handle->sensor_mclk != mclk)) {
		if (fd_handle->ccir_clk) {
			clk_disable(fd_handle->ccir_clk);
			SENSOR_PRINT("###sensor ccir clk off ok.\n");
		} else {
			fd_handle->ccir_clk = parse_clk(dn, SENSOR_CLK);
			if (IS_ERR(fd_handle->ccir_clk)) {
				SENSOR_PRINT_ERR("###: Failed: Can't get clock [ccir_mclk]!\n");
				SENSOR_PRINT_ERR("###: s_sensor_clk = %p.\n",fd_handle->ccir_clk);
			} else {
				SENSOR_PRINT("###sensor ccir clk get ok.\n");
			}
		}
		if (mclk > SENSOR_MAX_MCLK) {
			mclk = SENSOR_MAX_MCLK;
		}
		if (SENSOR_K_SUCCESS != _select_sensor_mclk(fd_handle, (uint8_t) mclk, &clk_src_name, &clk_div)) {
			SENSOR_PRINT_ERR("SENSOR:Sensor_SetMCLK select clock source fail.\n");
			return -EINVAL;
		}
		SENSOR_PRINT("clk_src_name=%s, clk_div=%d \n", clk_src_name, clk_div);

		clk_parent = clk_get(NULL, clk_src_name);
		if (!clk_parent) {
			SENSOR_PRINT_ERR("###:clock: failed to get clock [%s] by clk_get()!\n", clk_src_name);
			return -EINVAL;
		}
		SENSOR_PRINT("clk_get clk_src_name=%s done\n", clk_src_name);

		ret = clk_set_parent(fd_handle->ccir_clk, clk_parent);
		if (ret) {
			SENSOR_PRINT_ERR("###:clock: clk_set_parent() failed!parent \n");
			return -EINVAL;
		}
		SENSOR_PRINT("clk_set_parent s_ccir_clk=%s done\n", (char *)(fd_handle->ccir_clk));

		ret = clk_set_rate(fd_handle->ccir_clk, (mclk * SENOR_CLK_M_VALUE));
		if (ret) {
			SENSOR_PRINT_ERR("###:clock: clk_set_rate failed!\n");
			return -EINVAL;
		}
		SENSOR_PRINT("clk_set_rate s_ccir_clk=%s done\n", (char *)(fd_handle->ccir_clk));

		ret = clk_enable(fd_handle->ccir_clk);
		if (ret) {
			SENSOR_PRINT_ERR("###:clock: clk_enable() failed!\n");
		} else {
			SENSOR_PRINT("######ccir enable clk ok\n");
		}

		if (NULL == fd_handle->ccir_enable_clk) {
			fd_handle->ccir_enable_clk	= parse_clk(dn,"clk_ccir");
			if (IS_ERR(fd_handle->ccir_enable_clk)) {
				SENSOR_PRINT_ERR("###: Failed: Can't get clock [clk_ccir]!\n");
				SENSOR_PRINT_ERR("###: ccir_enable_clk = %p.\n", fd_handle->ccir_enable_clk);
				return -EINVAL;
			} else {
				SENSOR_PRINT("###sensor ccir_enable_clk clk_get ok.\n");
			}
			ret = clk_enable(fd_handle->ccir_enable_clk);
			if (ret) {
				SENSOR_PRINT_ERR("###:clock: clk_enable() failed!\n");
			} else {
				SENSOR_PRINT("###ccir enable clk ok\n");
			}
		}

		fd_handle->sensor_mclk = mclk;
		SENSOR_PRINT("SENSOR: set mclk %d Hz.\n",
			fd_handle->sensor_mclk);
	} else if (0 == mclk) {
		if (fd_handle->ccir_clk) {
			clk_disable(fd_handle->ccir_clk);
			SENSOR_PRINT("###sensor clk disable ok.\n");
			clk_put(fd_handle->ccir_clk);
			SENSOR_PRINT("###sensor clk put ok.\n");
			fd_handle->ccir_clk = NULL;
		}

		if (fd_handle->ccir_enable_clk) {
			clk_disable(fd_handle->ccir_enable_clk);
			SENSOR_PRINT("###sensor clk disable ok.\n");
			clk_put(fd_handle->ccir_enable_clk);
			SENSOR_PRINT("###sensor clk put ok.\n");
			fd_handle->ccir_enable_clk = NULL;
		}
		fd_handle->sensor_mclk = 0;
		SENSOR_PRINT("SENSOR: Disable MCLK !!!");
	} else {
		SENSOR_PRINT("SENSOR: Do nothing !! ");
	}
	SENSOR_PRINT_HIGH("SENSOR: set mclk X\n");
	return 0;
#endif
}

int sensor_k_set_mclk(uint32_t *fd_handle, uint32_t mclk)
{
	struct sensor_module_tab_tag    *p_mod;
	struct sensor_file_tag          *fd = (struct sensor_file_tag *)fd_handle;

	SENSOR_CHECK_ZERO(fd);
	p_mod = fd->module_data;
	SENSOR_CHECK_ZERO(p_mod);

	return _sensor_k_set_mclk(fd, p_mod->of_node, mclk);
}

int sensor_set_mclk_fromkernel(struct file *file, uint32_t arg, uint32_t sensor_id)
{
	uint32_t ret = SENSOR_K_SUCCESS;
	struct sensor_file_tag *p_file;
	struct sensor_module_tab_tag *p_mod;
	uint32_t mclk;
	p_file = file->private_data;
	p_mod = p_file->module_data;
	p_file->sensor_id = sensor_id;
	mclk = arg;

	ret = _sensor_k_set_mclk(p_file, p_mod->of_node, mclk);

	return ret;
}

unsigned short sensor_k_get_sensor_i2c_addr(uint32_t *fd_handle, uint32_t sensor_id)
{
	struct sensor_module_tab_tag	*p_mod;
	struct sensor_file_tag			*fd = (struct sensor_file_tag *)fd_handle;

	SENSOR_CHECK_ZERO(fd);
	p_mod = fd->module_data;
	SENSOR_CHECK_ZERO(p_mod);	

	return p_mod->sensor_dev_tab[sensor_id].cur_i2c_client->addr;
}

LOCAL int _sensor_k_reset(struct sensor_file_tag *fd_handle, uint32_t level, uint32_t width)
{
#ifndef	CONFIG_SC_FPGA_PIN
	struct sensor_module_tab_tag    *p_mod;

	SENSOR_CHECK_ZERO(fd_handle);
	p_mod = fd_handle->module_data;
	SENSOR_CHECK_ZERO(p_mod);

	get_gpio_id(p_mod->of_node, &fd_handle->gpio_tab.pwn, &fd_handle->gpio_tab.reset, fd_handle->sensor_id);
	SENSOR_PRINT_HIGH("SENSOR: reset val %d id %d reset %d\n",level, fd_handle->sensor_id, fd_handle->gpio_tab.reset);

	gpio_direction_output(fd_handle->gpio_tab.reset, level);
	gpio_set_value(fd_handle->gpio_tab.reset, level);
	SLEEP_MS(width);
	gpio_set_value(fd_handle->gpio_tab.reset, !level);
#else
	SENSOR_PRINT_HIGH("CAP_SENSOR_CTRL 0x%lx\n", CAP_SENSOR_CTRL);

	REG_MWR(CAP_SENSOR_CTRL,0xf0,level!=0?0xf0:0);
	SLEEP_MS(width);
	REG_MWR(CAP_SENSOR_CTRL,0xf0,level!=0?0:0xf0);
	mdelay(1);
#endif

	return SENSOR_K_SUCCESS;
}

int sensor_k_sensor_sel(uint32_t *fd_handle, uint32_t sensor_id)
{
	struct sensor_file_tag          *fd = (struct sensor_file_tag *)fd_handle;
	SENSOR_CHECK_ZERO(fd);
	fd->sensor_id = sensor_id;

	return SENSOR_K_SUCCESS;
}

int sensor_k_sensor_desel(struct sensor_file_tag *fd_handle, uint32_t sensor_id)
{
	SENSOR_CHECK_ZERO(fd_handle);
	//fd_handle->sensor_id = SENSOR_ID_MAX;

	SENSOR_PRINT_HIGH("sensor desel %d OK.\n", sensor_id);

	return SENSOR_K_SUCCESS;
}

int sensor_set_rst_level_fromkernel(struct file *file, uint32_t arg, uint32_t sensor_id)
{
	uint32_t ret = SENSOR_K_SUCCESS;
	struct sensor_file_tag *p_file;
	struct sensor_module_tab_tag *p_mod;
	uint32_t rst_level;
	p_file = file->private_data;
	p_mod = p_file->module_data;
	p_file->sensor_id = sensor_id;
	rst_level = arg;

	ret = sensor_k_set_rst_level((uint32_t *)p_file, rst_level);

	return ret;
}

int sensor_k_set_rst_level(uint32_t *fd_handle, uint32_t plus_level)
{
	struct sensor_module_tab_tag    *p_mod;
	struct sensor_file_tag          *fd = (struct sensor_file_tag *)fd_handle;

	SENSOR_CHECK_ZERO(fd);
	p_mod = fd->module_data;
	SENSOR_CHECK_ZERO(p_mod);

	get_gpio_id(p_mod->of_node, &fd->gpio_tab.pwn, &fd->gpio_tab.reset, fd->sensor_id);
	SENSOR_PRINT("SENSOR: set rst lvl: lvl %d, rst pin %d \n", plus_level, fd->gpio_tab.reset);

#ifndef CONFIG_SC_FPGA_PIN
	gpio_direction_output(fd->gpio_tab.reset, plus_level);
	gpio_set_value(fd->gpio_tab.reset, plus_level);
#else
	SENSOR_PRINT_HIGH("CAP_SENSOR_CTRL 0x%lx\n", CAP_SENSOR_CTRL);
	REG_MWR(CAP_SENSOR_CTRL,0xf0,plus_level!=0?0xf0:0x0);
	SENSOR_PRINT_HIGH("CAP_SENSOR_CTRL val=0x%x\n", REG_RD(CAP_SENSOR_CTRL));
#endif
	return SENSOR_K_SUCCESS;
}

int sensor_k_set_mipi_level(uint32_t *fd_handle, uint32_t plus_level)
{
	struct sensor_module_tab_tag    *p_mod;
	struct sensor_file_tag          *fd = (struct sensor_file_tag *)fd_handle;
	int gpio_mipi_mode = 0;

	SENSOR_CHECK_ZERO(fd);
	p_mod = fd->module_data;
	SENSOR_CHECK_ZERO(p_mod);

	get_gpio_id_ex(p_mod->of_node, GPIO_MIPI_SWITCH_MODE, &gpio_mipi_mode, fd->sensor_id);
	gpio_direction_output(gpio_mipi_mode, plus_level);
	gpio_set_value(gpio_mipi_mode, plus_level);

	return SENSOR_K_SUCCESS;
}


LOCAL int _Sensor_K_ReadReg(struct sensor_file_tag *fd_handle, struct sensor_reg_bits_tag *pReg)
{
	uint8_t                       cmd[2] = { 0 };
	uint16_t                      w_cmd_num = 0;
	uint16_t                      r_cmd_num = 0;
	uint8_t                       buf_r[2] = { 0 };
	int32_t                       ret = SENSOR_K_SUCCESS;
	struct i2c_msg                msg_r[2];
	uint16_t                      reg_addr;
	int                           i;
	struct sensor_module_tab_tag  *p_mod;
	SENSOR_CHECK_ZERO(fd_handle);

	p_mod = fd_handle->module_data;
	SENSOR_CHECK_ZERO(p_mod);

	reg_addr = pReg->reg_addr;

	if (SENSOR_I2C_REG_16BIT ==(pReg->reg_bits & SENSOR_I2C_REG_16BIT)) {
		cmd[w_cmd_num++] = (uint8_t) ((reg_addr >> 8) & SENSOR_LOW_EIGHT_BIT);
		cmd[w_cmd_num++] = (uint8_t) (reg_addr & SENSOR_LOW_EIGHT_BIT);
	} else {
		cmd[w_cmd_num++] = (uint8_t) reg_addr;
	}

	if (SENSOR_I2C_VAL_16BIT == (pReg->reg_bits & SENSOR_I2C_VAL_16BIT)) {
		r_cmd_num = SENSOR_CMD_BITS_16;
	} else {
		r_cmd_num = SENSOR_CMD_BITS_8;
	}

	for (i = 0; i < SENSOR_I2C_OP_TRY_NUM; i++) {
		msg_r[0].addr = p_mod->sensor_dev_tab[fd_handle->sensor_id].cur_i2c_client->addr;
		msg_r[0].flags = 0;
		msg_r[0].buf = cmd;
		msg_r[0].len = w_cmd_num;
		msg_r[1].addr = p_mod->sensor_dev_tab[fd_handle->sensor_id].cur_i2c_client->addr;
		msg_r[1].flags = I2C_M_RD;
		msg_r[1].buf = buf_r;
		msg_r[1].len = r_cmd_num;
		ret = i2c_transfer(p_mod->sensor_dev_tab[fd_handle->sensor_id].cur_i2c_client->adapter, msg_r, 2);
		if (ret != 2) {
			SENSOR_PRINT_ERR("SENSOR:read reg fail, ret %d, addr 0x%x, reg_addr 0x%x \n",
					ret, p_mod->sensor_dev_tab[fd_handle->sensor_id].cur_i2c_client->addr,reg_addr);
			SLEEP_MS(1);
			ret = SENSOR_K_FAIL;
		} else {
			pReg->reg_value = (r_cmd_num == 1) ? (uint16_t) buf_r[0] : (uint16_t) ((buf_r[0] << 8) + buf_r[1]);
			ret = SENSOR_K_SUCCESS;
			break;
		}
	}

	return ret;
}

int sensor_ReadReg_fromkernel(struct file *file, struct sensor_reg_bits_tag *arg)
{
	uint32_t ret = SENSOR_K_SUCCESS;
	struct sensor_file_tag *p_file;
	struct sensor_module_tab_tag *p_mod;
	struct sensor_reg_bits_tag *reg;
	p_file = file->private_data;
	p_mod = p_file->module_data;
	reg = arg;

	ret = _Sensor_K_ReadReg(p_file, reg);

	return ret;
}

#define SENSOR_WRITE_DELAY_8BIT                       0xff
LOCAL int _Sensor_K_WriteReg(struct sensor_file_tag *fd_handle, struct sensor_reg_bits_tag *pReg)
{
	uint8_t                       cmd[4] = { 0 };
	uint32_t                      index = 0;
	uint32_t                      cmd_num = 0;
	struct i2c_msg                msg_w;
	int32_t                       ret = SENSOR_K_SUCCESS;
	uint16_t                      subaddr;
	uint16_t                      data;
	int                           i;
	struct sensor_module_tab_tag  *p_mod;
	SENSOR_CHECK_ZERO(fd_handle);

	p_mod = fd_handle->module_data;
	SENSOR_CHECK_ZERO(p_mod);

	subaddr = pReg->reg_addr;
	data = pReg->reg_value;

	if (SENSOR_I2C_REG_16BIT ==(pReg->reg_bits & SENSOR_I2C_REG_16BIT)) {
		cmd[cmd_num++] = (uint8_t) ((subaddr >> 8) & SENSOR_LOW_EIGHT_BIT);
		index++;
		cmd[cmd_num++] =  (uint8_t) (subaddr & SENSOR_LOW_EIGHT_BIT);
		index++;
	} else {
		cmd[cmd_num++] = (uint8_t) subaddr;
		index++;
	}

	if (SENSOR_I2C_VAL_16BIT == (pReg->reg_bits & SENSOR_I2C_VAL_16BIT)) {
		cmd[cmd_num++] = (uint8_t) ((data >> 8) & SENSOR_LOW_EIGHT_BIT);
		index++;
		cmd[cmd_num++] = (uint8_t) (data & SENSOR_LOW_EIGHT_BIT);
		index++;
	} else {
		cmd[cmd_num++] = (uint8_t) data;
		index++;
	}


	if ((SENSOR_WRITE_DELAY != subaddr) && (SENSOR_WRITE_DELAY_8BIT != subaddr)) {

		for (i = 0; i < SENSOR_I2C_OP_TRY_NUM; i++) {
			msg_w.addr = p_mod->sensor_dev_tab[fd_handle->sensor_id].cur_i2c_client->addr;
			msg_w.flags = 0;
			msg_w.buf = cmd;
			msg_w.len = index;
			ret = i2c_transfer(p_mod->sensor_dev_tab[fd_handle->sensor_id].cur_i2c_client->adapter, &msg_w, 1);
			if (ret != 1) {
				SENSOR_PRINT_ERR("_Sensor_K_WriteReg failed:i2cAddr=%x, addr=%x, value=%x, bit=%d \n",
						p_mod->sensor_dev_tab[fd_handle->sensor_id].cur_i2c_client->addr, pReg->reg_addr, pReg->reg_value, pReg->reg_bits);
				ret = SENSOR_K_FAIL;
				continue;
			} else {
				ret = SENSOR_K_SUCCESS;
				break;
			}
		}
	}else if(SENSOR_WRITE_DELAY == subaddr){
		SLEEP_MS(data);
	} else if ((SENSOR_WRITE_DELAY_8BIT == subaddr) && (SENSOR_WRITE_DELAY_8BIT != data)) {
			//SENSOR_PRINT_HIGH("_Sensor_K_WriteReg subaddr = %x, data = %x \n", subaddr, data);
			SLEEP_MS(data*10);
		} else {
			SENSOR_PRINT_HIGH("_Sensor_K_WriteReg RegTab END \n");
		}

	return ret;
}


enum cmr_flash_status {
	FLASH_CLOSE	= 0x0,
	FLASH_OPEN		= 0x1,
	FLASH_TORCH	= 0x2, /* User only set flash to close/open/torch state */
	FLASH_AUTO	= 0x3,
	FLASH_CLOSE_AFTER_OPEN	= 0x10, /* Following is set to sensor */
	FLASH_HIGH_LIGHT			= 0x11,
	FLASH_OPEN_ON_RECORDING	= 0x22,
	FLASH_CLOSE_AFTER_AUTOFOCUS	= 0x30,
	FLASH_STATUS_MAX
};

LOCAL int _sensor_k_set_flash(uint32_t flash_mode)
{
	printk("_sensor_k_set_flash : flash_mode 0x%x\n", flash_mode);

	if(flash_torch_status==1)
		return 0;

		switch (flash_mode)
		{
			case FLASH_OPEN: /* Flash on */
			case FLASH_TORCH: /* For torch */
				sprd_flash_on();
				break;

			case FLASH_HIGH_LIGHT:
				sprd_flash_high_light();
				break;

			case FLASH_CLOSE_AFTER_OPEN: /* Close flash */
			case FLASH_CLOSE_AFTER_AUTOFOCUS:
			case FLASH_CLOSE:
				sprd_flash_close();
				break;

			default:
				printk("_sensor_k_set_flash : Unknow mode : Flash_mode 0x%x\n", flash_mode);
				break;
		}

	return 0;
}

LOCAL int _sensor_k_get_flash_level(struct sensor_file_tag *fd_handle, struct sensor_flash_level *level)
{
	level->low_light  = SPRD_FLASH_LOW_CUR;
	level->high_light = SPRD_FLASH_HIGH_CUR;

	SENSOR_PRINT("Sensor get flash lvl: low %d, high %d \n", level->low_light, level->high_light);

	return SENSOR_K_SUCCESS;
}


int _sensor_burst_write_init(struct sensor_file_tag *fd_handle, struct sensor_reg_tag *p_reg_table, uint32_t init_table_size);

LOCAL int _sensor_k_wr_regtab(struct sensor_file_tag *fd_handle, struct sensor_reg_tab_tag *pRegTab)
{
	char                   *pBuff = PNULL;
	uint32_t               cnt = pRegTab->reg_count;
	int                    ret = SENSOR_K_SUCCESS;
	uint32_t               size;
	struct sensor_reg_tag  *sensor_reg_ptr;
	struct sensor_reg_bits_tag reg_bit;
	uint32_t               i;
	int                    rettmp;
	struct timeval         time1, time2;

	do_gettimeofday(&time1);

	size = cnt*sizeof(struct sensor_reg_tag);
	pBuff = _sensor_k_malloc(fd_handle, size);
	if (PNULL == pBuff) {
		ret = SENSOR_K_FAIL;
		SENSOR_PRINT_ERR("sensor W RegTab err:alloc fail, cnt %d, size %d\n", cnt, size);
		goto _Sensor_K_WriteRegTab_return;
	} else {
		SENSOR_PRINT("sensor W RegTab: alloc success, cnt %d, size %d \n",cnt, size);
	}

	if (copy_from_user(pBuff, pRegTab->sensor_reg_tab_ptr, size)) {
		ret = SENSOR_K_FAIL;
		SENSOR_PRINT_ERR("sensor w err:copy user fail, size %d \n", size);
		goto _Sensor_K_WriteRegTab_return;
	}

	sensor_reg_ptr = (struct sensor_reg_tag *)pBuff;

	if (0 == pRegTab->burst_mode) {
		for (i=0; i<cnt; i++) {
			reg_bit.reg_addr  = sensor_reg_ptr[i].reg_addr;
			reg_bit.reg_value = sensor_reg_ptr[i].reg_value;
			reg_bit.reg_bits  = pRegTab->reg_bits;

			rettmp = _Sensor_K_WriteReg(fd_handle, &reg_bit);
			if(SENSOR_K_FAIL == rettmp)
				ret = SENSOR_K_FAIL;
		}
	} else if (SENSOR_I2C_BUST_NB == pRegTab->burst_mode) {
		printk("CAM %s, Line %d, burst_mode=%d, cnt=%d, start \n", __FUNCTION__, __LINE__, pRegTab->burst_mode, cnt);
		ret = _sensor_burst_write_init(fd_handle, sensor_reg_ptr, pRegTab->reg_count);
		printk("CAM %s, Line %d, burst_mode=%d, cnt=%d end\n", __FUNCTION__, __LINE__, pRegTab->burst_mode, cnt);
	}


_Sensor_K_WriteRegTab_return:
	if (PNULL != pBuff)
		_sensor_k_free(fd_handle, pBuff);

	do_gettimeofday(&time2);
	SENSOR_PRINT("sensor w RegTab: done, ret %d, cnt %d, time %d us \n", ret, cnt,
		(uint32_t)((time2.tv_sec - time1.tv_sec)*1000000+(time2.tv_usec - time1.tv_usec)));
	return ret;
}

int sensor_wr_regtab_fromkernel(struct file *file, struct sensor_reg_tab_tag *arg, uint32_t sensor_id)
{
	uint32_t ret = SENSOR_K_SUCCESS;
	struct sensor_file_tag *p_file;
	struct sensor_module_tab_tag *p_mod;
	struct sensor_reg_tab_tag *regTab;
	p_file = file->private_data;
	p_file->sensor_id = sensor_id;
	p_mod = p_file->module_data;
	regTab = arg;

	ret = _sensor_k_wr_regtab(p_file, regTab);

	return ret;
}

LOCAL int _sensor_k_set_i2c_clk(struct sensor_file_tag *fd_handle, uint32_t clock)
{
	struct sensor_module_tab_tag  *p_mod;
	struct i2c_client             *i2c_client;
	SENSOR_CHECK_ZERO(fd_handle);

	p_mod = fd_handle->module_data;
	SENSOR_CHECK_ZERO(p_mod);

	if (NULL != p_mod->sensor_dev_tab[fd_handle->sensor_id].cur_i2c_client) {
		i2c_client = p_mod->sensor_dev_tab[fd_handle->sensor_id].cur_i2c_client;
		sprd_i2c_ctl_chg_clk(i2c_client->adapter->nr, clock);
		SENSOR_PRINT("sensor set i2c id %d clk %d  \n", i2c_client->adapter->nr, clock);
	}
	if (NULL != p_mod->sensor_dev_tab[fd_handle->sensor_id].vcm_i2c_client
		&& 0 == p_mod->sensor_dev_tab[fd_handle->sensor_id].vcm_gpio_i2c_flag) {
		i2c_client = p_mod->sensor_dev_tab[fd_handle->sensor_id].vcm_i2c_client;
		sprd_i2c_ctl_chg_clk(i2c_client->adapter->nr, clock);
		SENSOR_PRINT("sensor set i2c id %d clk %d  \n", i2c_client->adapter->nr, clock);
	}
	SENSOR_PRINT("sensor set i2c clk %d  \n", clock);

	return SENSOR_K_SUCCESS;
}

int sensor_set_i2c_clk_fromkernel(struct file *file, uint32_t arg, uint32_t sensor_id)
{
	uint32_t ret = SENSOR_K_SUCCESS;
	struct sensor_file_tag *p_file;
	struct sensor_module_tab_tag *p_mod;
	uint32_t i2c_clk;
	p_file = file->private_data;
	p_mod = p_file->module_data;
	p_file->sensor_id = sensor_id;
	i2c_clk = arg;

	ret = _sensor_k_set_i2c_clk(p_file, i2c_clk);

	return ret;
}

LOCAL int _sensor_k_wr_i2c(struct sensor_file_tag *fd_handle, struct sensor_i2c_tag *pI2cTab)
{
	char                          *pBuff = PNULL;
	struct i2c_msg                msg_w;
	uint32_t                      cnt = pI2cTab->i2c_count;
	int                           ret = SENSOR_K_FAIL;
	struct sensor_module_tab_tag  *p_mod;
	struct i2c_client             *i2c_client;
	SENSOR_CHECK_ZERO(fd_handle);

	p_mod = fd_handle->module_data;
	SENSOR_CHECK_ZERO(p_mod);

	pBuff = _sensor_k_malloc(fd_handle, cnt);
	if (PNULL == pBuff) {
		SENSOR_PRINT_ERR("sensor W I2C ERR: alloc fail, size %d\n", cnt);
		goto sensor_k_writei2c_return;
	} else {
		SENSOR_PRINT("sensor W I2C: alloc success, size %d\n", cnt);
	}

	if (copy_from_user(pBuff, pI2cTab->i2c_data, cnt)) {
		SENSOR_PRINT_ERR("sensor W I2C ERR: copy user fail, size %d \n", cnt);
		goto sensor_k_writei2c_return;
	}

	msg_w.addr = pI2cTab->slave_addr;
	msg_w.flags = 0;
	msg_w.buf = pBuff;
	msg_w.len = cnt;
#if 1
	if (NULL == p_mod->sensor_dev_tab[fd_handle->sensor_id].vcm_i2c_client) {
		i2c_client = p_mod->sensor_dev_tab[fd_handle->sensor_id].cur_i2c_client;
	} else {
		i2c_client = p_mod->sensor_dev_tab[fd_handle->sensor_id].vcm_i2c_client;
	}
#else
	i2c_client = p_mod->sensor_dev_tab[fd_handle->sensor_id].cur_i2c_client;
#endif
	ret = i2c_transfer(i2c_client->adapter, &msg_w, 1);
	if (ret != 1) {
		SENSOR_PRINT_ERR("SENSOR: w reg fail, ret: %d, addr: 0x%x\n",
		ret, msg_w.addr);
	} else {
		ret = SENSOR_K_SUCCESS;
	}

sensor_k_writei2c_return:
	if(PNULL != pBuff)
		_sensor_k_free(fd_handle, pBuff);

	SENSOR_PRINT("sensor w done, ret %d \n", ret);
	return ret;
}

int sensor_wr_i2c_fromkernel(struct file *file, struct sensor_i2c_tag *arg)
{
	uint32_t ret = SENSOR_K_SUCCESS;
	struct sensor_file_tag *fd_handle = file->private_data;
	struct sensor_module_tab_tag *p_mod;
	struct sensor_i2c_tag *i2cTab = arg;
	char   *pBuff = PNULL;
	struct i2c_msg                msg_w;
	uint32_t cnt = 0;
	struct i2c_client             *i2c_client;

	p_mod = fd_handle->module_data;

	cnt = i2cTab->i2c_count;
	pBuff = _sensor_k_malloc(fd_handle, cnt);
	if (PNULL == pBuff) {
		SENSOR_PRINT_ERR("sensor W I2C ERR: alloc fail, size %d\n", cnt);
		goto sensor_k_writei2c_return;
	} else {
		SENSOR_PRINT("sensor W I2C: alloc success, size %d\n", cnt);
	}

	memcpy(pBuff, i2cTab->i2c_data, cnt);

	msg_w.addr = i2cTab->slave_addr;
	msg_w.flags = 0;
	msg_w.buf = pBuff;
	msg_w.len = cnt;
#if 1
	if (NULL == p_mod->sensor_dev_tab[fd_handle->sensor_id].vcm_i2c_client) {
		i2c_client = p_mod->sensor_dev_tab[fd_handle->sensor_id].cur_i2c_client;
	} else {
		i2c_client = p_mod->sensor_dev_tab[fd_handle->sensor_id].vcm_i2c_client;
	}
#else
	i2c_client = p_mod->sensor_dev_tab[fd_handle->sensor_id].cur_i2c_client;
#endif
	ret = i2c_transfer(i2c_client->adapter, &msg_w, 1);
	if (ret != 1) {
		SENSOR_PRINT_ERR("SENSOR: w reg fail, ret: %d, addr: 0x%x\n",
		ret, msg_w.addr);
	} else {
		ret = SENSOR_K_SUCCESS;
	}

sensor_k_writei2c_return:
	if(PNULL != pBuff)
		_sensor_k_free(fd_handle, pBuff);

	SENSOR_PRINT("sensor w done, ret %d \n", ret);
	return ret;

}


int _sensor_k_rd_i2c(struct sensor_file_tag *fd_handle, struct sensor_i2c_tag *pI2cTab)
{
	struct i2c_msg	   msg_r[2];
	int                i;
	char               *pBuff = PNULL;
	uint32_t           cnt = pI2cTab->i2c_count;
	int                ret = SENSOR_K_FAIL;
	uint16_t           read_num = cnt;
	struct sensor_module_tab_tag  *p_mod;
	struct i2c_client  *i2c_client;
	SENSOR_CHECK_ZERO(fd_handle);

	p_mod = fd_handle->module_data;
	SENSOR_CHECK_ZERO(p_mod);

	/*alloc buffer */
	pBuff = _sensor_k_malloc(fd_handle, cnt);
	if (PNULL == pBuff) {
		ret = SENSOR_K_FAIL;
		SENSOR_PRINT_ERR("sensor rd I2C ERR: alloc fail, size %d\n", cnt);
		goto sensor_k_readi2c_return;
	} else {
		SENSOR_PRINT("sensor rd I2C: alloc success, size %d\n", cnt);
	}

	if (copy_from_user(pBuff, pI2cTab->i2c_data, cnt)) {
		ret = SENSOR_K_FAIL;
		SENSOR_PRINT_ERR("sensor W I2C ERR: copy user fail, size %d \n", cnt);
		goto sensor_k_readi2c_return;
	}


	for (i = 0; i < SENSOR_I2C_OP_TRY_NUM; i++) {
		msg_r[0].addr =  pI2cTab->slave_addr;
		msg_r[0].flags = 0;
		msg_r[0].buf = pBuff;
		msg_r[0].len = cnt;
		msg_r[1].addr =  pI2cTab->slave_addr;
		msg_r[1].flags = I2C_M_RD;
		msg_r[1].buf = pBuff;
		msg_r[1].len = read_num;
#if 1
		if (NULL == p_mod->sensor_dev_tab[fd_handle->sensor_id].vcm_i2c_client) {
			i2c_client = p_mod->sensor_dev_tab[fd_handle->sensor_id].cur_i2c_client;
		} else {
			i2c_client = p_mod->sensor_dev_tab[fd_handle->sensor_id].vcm_i2c_client;
		}
#else
		i2c_client = p_mod->sensor_dev_tab[fd_handle->sensor_id].cur_i2c_client;
#endif
		ret = i2c_transfer(i2c_client->adapter, msg_r, 2);
		if (ret != 2) {
			SENSOR_PRINT_ERR("SENSOR:read reg fail, ret %d, addr 0x%x \n",
					ret, p_mod->sensor_dev_tab[fd_handle->sensor_id].cur_i2c_client->addr);
			SLEEP_MS(20);
			ret = SENSOR_K_FAIL;
		} else {
			ret = SENSOR_K_SUCCESS;
			if (copy_to_user(pI2cTab->i2c_data, pBuff, read_num)) {
				ret = SENSOR_K_FAIL;
				SENSOR_PRINT_ERR("sensor W I2C ERR: copy user fail, size %d \n", cnt);
				goto sensor_k_readi2c_return;
			}
			break;
		}
	}

sensor_k_readi2c_return:
	if(PNULL != pBuff)
		_sensor_k_free(fd_handle, pBuff);

	SENSOR_PRINT("_Sensor_K_ReadI2C, ret %d \n", ret);
	return ret;
}

int sensor_rd_i2c_fromkernel(struct file *f, struct sensor_i2c_tag *arg, uint32_t sensor_id)
{
	int  ret = SENSOR_K_SUCCESS;
	struct sensor_file_tag *fd_handle;
	struct sensor_module_tab_tag *p_mod;
	struct sensor_i2c_tag *i2cTab;
	struct i2c_msg	   msg_r[2];
	struct i2c_client  *i2c_client;
	char   *pBuff ;
	uint32_t   cnt;
	uint16_t read_num;
	int i;
	pBuff = PNULL;
	read_num = 0;
	i2cTab = arg;
	i = 0;
	fd_handle = f->private_data;
	fd_handle->sensor_id = sensor_id;
	p_mod = fd_handle->module_data;

	read_num = i2cTab->i2c_count;
	cnt = i2cTab->i2c_count;
	/*alloc buffer */
	pBuff = _sensor_k_malloc(fd_handle, cnt);
	if (PNULL == pBuff) {
		ret = SENSOR_K_FAIL;
		SENSOR_PRINT_ERR("sensor rd I2C ERR: alloc fail, size %d\n", cnt);
		goto sensor_k_readi2c_return;
	} else {
		SENSOR_PRINT("sensor rd I2C: alloc success, size %d\n", cnt);
	}

	memcpy(pBuff, i2cTab->i2c_data, cnt);

	for (i = 0; i < SENSOR_I2C_OP_TRY_NUM; i++) {
		msg_r[0].addr =  i2cTab->slave_addr;
		msg_r[0].flags = 0;
		msg_r[0].buf = pBuff;
		msg_r[0].len = cnt;
		msg_r[1].addr =  i2cTab->slave_addr;
		msg_r[1].flags = I2C_M_RD;
		msg_r[1].buf = pBuff;
		msg_r[1].len = read_num;
#if 1
		if (NULL == p_mod->sensor_dev_tab[fd_handle->sensor_id].vcm_i2c_client) {
			i2c_client = p_mod->sensor_dev_tab[fd_handle->sensor_id].cur_i2c_client;
		} else {
			i2c_client = p_mod->sensor_dev_tab[fd_handle->sensor_id].vcm_i2c_client;
		}
#else
		i2c_client = p_mod->sensor_dev_tab[fd_handle->sensor_id].cur_i2c_client;
#endif
		ret = i2c_transfer(i2c_client->adapter, msg_r, 2);
		if (ret != 2) {
			SENSOR_PRINT_ERR("SENSOR:read reg fail, ret %d, addr 0x%x \n",
					ret, p_mod->sensor_dev_tab[fd_handle->sensor_id].cur_i2c_client->addr);
			SLEEP_MS(20);
			ret = SENSOR_K_FAIL;
		}
	}

sensor_k_readi2c_return:
	if(PNULL != pBuff)
		_sensor_k_free(fd_handle, pBuff);

	SENSOR_PRINT("_Sensor_K_ReadI2C, ret %d \n", ret);
	return ret;

}

LOCAL int _sensor_csi2_error(uint32_t err_id, uint32_t err_status, void* u_data)
{
	int                      ret = 0;

	printk("V4L2: csi2_error, %d 0x%x \n", err_id, err_status);

	return ret;

}

int sensor_k_open(struct inode *node, struct file *file)
{
	int                            ret = 0;
	struct sensor_file_tag         *p_file;
	struct sensor_module_tab_tag   *p_mod = NULL;// platform_get_drvdata(_sensor_k_get_platform_device());
	struct miscdevice *md = (struct miscdevice *)file->private_data ;

	if (!md) {
		ret = -EFAULT;
		printk("rot_k_open fail miscdevice NULL \n");
		return -1;
	}
	p_mod = (struct sensor_module_tab_tag*)md->this_device->platform_data;
	SENSOR_CHECK_ZERO(p_mod);

	p_file = (struct sensor_file_tag *)vzalloc(sizeof(struct sensor_file_tag));
	SENSOR_CHECK_ZERO(p_file);
	file->private_data = p_file;
	p_file->module_data = p_mod;
	ret = csi_api_malloc(&p_file->csi_handle);
	if (ret) {
		vfree(p_file);
		p_file = NULL;
		return -1;
	}

	if (atomic_inc_return(&p_mod->total_users) == 1) {
		struct device_node *dn = p_mod->of_node;
		ret = clk_mm_i_eb(dn,1);
		wake_lock(&p_mod->wakelock);
	}
	printk("sensor open %d\n", ret);
	return ret;
}

int _sensor_k_close_mipi(struct file *file)
{
	int                            ret = 0;
	struct sensor_file_tag         *p_file = file->private_data;
	struct csi_context             *handle = NULL;
	SENSOR_CHECK_ZERO(p_file);
	handle = p_file->csi_handle;
	if (NULL == handle) {
		printk("handle null\n");
		return -1;
	}
	if (INTERFACE_MIPI == p_file->if_type) {
		if (1 == p_file->mipi_on) {
			csi_api_close(handle, p_file->phy_id);
			_sensor_k_mipi_clk_dis(p_file);
			p_file->mipi_on = 0;
			printk("MIPI off \n");
		} else {
			printk("MIPI already off \n");
		}

	}
	return ret;
}

int sensor_k_release(struct inode *node, struct file *file)
{
	int                            ret = 0;
	struct sensor_file_tag         *p_file = file->private_data;
	struct sensor_module_tab_tag   *p_mod;

	SENSOR_CHECK_ZERO(p_file);
	p_mod = p_file->module_data;
	SENSOR_CHECK_ZERO(p_mod);

	printk("sensor: release \n");
	if (atomic_dec_return(&p_mod->total_users) == 0) {
		struct device_node *dn = p_mod->of_node;
		sensor_k_set_voltage_cammot((uint32_t *)p_file, SENSOR_VDD_CLOSED);
		sensor_k_set_voltage_avdd((uint32_t *)p_file, SENSOR_VDD_CLOSED);
		sensor_k_set_voltage_dvdd((uint32_t *)p_file, SENSOR_VDD_CLOSED);
		sensor_k_set_voltage_iovdd((uint32_t *)p_file, SENSOR_VDD_CLOSED);
		_sensor_k_set_mclk(p_file, dn, 0);
		_sensor_k_close_mipi(file);
		ret = clk_mm_i_eb(dn,0);

		wake_unlock(&p_mod->wakelock);
	}
	if (SENSOR_ADDR_INVALID(p_file)) {
		printk("SENSOR: Invalid addr, %p", p_mod);
	} else {
		if (NULL == p_file->sensor_mem.buf_ptr || 0 == p_file->sensor_mem.size) {
			printk("check !! free size = %d, ptr=%p \n", p_file->sensor_mem.size, p_file->sensor_mem.buf_ptr);
		}
		else {
			vfree(p_file->sensor_mem.buf_ptr);
			p_file->sensor_mem.buf_ptr = PNULL;
			p_file->sensor_mem.size = 0;
		}

		csi_api_free(p_file->csi_handle);
		vfree(p_file);
		p_file = NULL;
		file->private_data = NULL;
	}
	printk("sensor: release %d \n", ret);
	return ret;
}

LOCAL ssize_t sensor_k_read(struct file *filp, char __user *ubuf, size_t cnt, loff_t *gpos)
{
	return 0;
}

LOCAL ssize_t sensor_k_write(struct file *filp, const char __user *ubuf, size_t cnt, loff_t *gpos)
{
	char                          buf[64];
	char                          *pBuff = PNULL;
	struct                        i2c_msg msg_w;
	int                           ret = SENSOR_K_FAIL;
	int                           need_alloc = 1;
	struct i2c_client             *i2c_client = PNULL;
	struct sensor_file_tag        *p_file = filp->private_data;
	struct sensor_module_tab_tag  *p_mod;
	SENSOR_CHECK_ZERO(p_file);

	p_mod = p_file->module_data;
	SENSOR_CHECK_ZERO(p_mod);

	i2c_client = p_mod->sensor_dev_tab[p_file->sensor_id].cur_i2c_client;

	SENSOR_PRINT("sensor w cnt %ld, buf %ld\n", cnt, sizeof(buf));

	if (cnt < sizeof(buf)) {
		pBuff = buf;
		need_alloc = 0;
	}  else {
		pBuff = _sensor_k_malloc(p_file, cnt);
		if (PNULL == pBuff) {
			SENSOR_PRINT_ERR("sensor w ERR: alloc fail, size %d \n", cnt);
			goto sensor_k_write_return;
		} else {
			SENSOR_PRINT("sensor w: alloc success, size %d \n", cnt);
		}
	}

	if (copy_from_user(pBuff, ubuf, cnt)) {
		SENSOR_PRINT_ERR("sensor w ERR: copy user fail, size %d\n", cnt);
		goto sensor_k_write_return;
	}
	printk("sensor clnt addr 0x%x.\n", i2c_client->addr);
	msg_w.addr = i2c_client->addr;
	msg_w.flags = 0;
	msg_w.buf = pBuff;
	msg_w.len = cnt;

	ret = i2c_transfer(i2c_client->adapter, &msg_w, 1);
	if (ret != 1) {
		SENSOR_PRINT_ERR("SENSOR: w reg fail, ret %d, w addr: 0x%x,\n",
				ret, i2c_client->addr);
	} else {
		ret = SENSOR_K_SUCCESS;
	}

sensor_k_write_return:
	if ((PNULL != pBuff) && need_alloc)
		_sensor_k_free(p_file, pBuff);

	SENSOR_PRINT("sensor w done, ret %d \n", ret);
	return ret;
}
#if defined(CONFIG_MACH_J1X3G) || defined(CONFIG_MACH_GTEXSWIFI)
#define I2C_WRITE_BURST_LENGTH 512

static int _burst_write(struct sensor_file_tag *fd_handle, uint16_t reg,  uint16_t val)
{
	int ret = 0;
	uint8_t data[4];
	struct i2c_client *i2c_client = PNULL;
	struct sensor_module_tab_tag  *p_mod;
	struct sensor_file_tag        *p_file = fd_handle;
	p_mod = p_file->module_data;
	SENSOR_CHECK_ZERO(p_mod);
	i2c_client = p_mod->sensor_dev_tab[p_file->sensor_id].cur_i2c_client;
	if (0 == i2c_client)
	{
		SENSOR_PRINT_ERR("SENSOR: _burst_write err, i2c_clnt NULL!.\n");
		return -1;
	}
	data[0] = reg>>8;
	data[1] = reg;
	data[2]=  val>>8;
	data[3] = val;
	if (reg == 0xffff)
	{
		msleep(val); /* Wait for reset to run */
		return 0;
	}
	ret = i2c_master_send(i2c_client, data, 4);
	return (ret < 0) ? ret: 0;
}
int _sensor_burst_write_init(struct sensor_file_tag *fd_handle, struct sensor_reg_tag *p_reg_table, uint32_t init_table_size)
{
	int i = 0, ret=0;
	uint16_t iic_length = 2;
	uint8_t iic_data[3000] = {0};
	struct i2c_client *i2c_client = PNULL;
	struct sensor_module_tab_tag  *p_mod;
	struct sensor_file_tag        *p_file = fd_handle;
	p_mod = p_file->module_data;
	SENSOR_CHECK_ZERO(p_mod);
	i2c_client = p_mod->sensor_dev_tab[p_file->sensor_id].cur_i2c_client;
	if (0 == i2c_client)
	{
		SENSOR_PRINT_ERR("SENSOR: burst w Init err, i2c_clnt NULL!.\n");
		return -1;
	}
	for( i = 0 ; i < init_table_size ; i++ )
	{
		if( p_reg_table[i].reg_addr == 0x0F12 )
		{
			iic_data[iic_length] = p_reg_table[i].reg_value >>8;
			iic_data[iic_length+1] = p_reg_table[i].reg_value & 0x00FF;
			iic_length = iic_length+2;
			if( i == (init_table_size-1) )
			{
				iic_data[0]=0x0F;
				iic_data[1]=0x12;
				i2c_master_send(i2c_client, iic_data, iic_length);
				iic_length =2;
			}
		}
		else
		{
			if(iic_length !=2)
			{
				iic_data[0]=0x0F;
				iic_data[1]=0x12;
				i2c_master_send(i2c_client, iic_data, iic_length);
				iic_length =2;
			}
			ret = _burst_write(fd_handle, p_reg_table[i].reg_addr, p_reg_table[i].reg_value);
			if (ret < 0)
			{
				printk(KERN_NOTICE "======[_sensor_burst_write_init %d]====== \n", ret );
				return ret;
			}
		}
	}
	return 0;
}
#else
int _sensor_burst_write_init(struct sensor_file_tag *fd_handle, struct sensor_reg_tag *p_reg_table, uint32_t init_table_size)
{
	uint32_t                      rtn = 0;
	int                           ret = 0;
	uint32_t                      i = 0;
	uint32_t                      written_num = 0;
	uint16_t                      wr_reg = 0;
	uint16_t                      wr_val = 0;
	uint32_t                      wr_num_once = 0;
	uint8_t                       *p_reg_val_tmp = 0;
	struct i2c_msg                msg_w;
	struct i2c_client             *i2c_client = PNULL;
	struct sensor_file_tag        *p_file = fd_handle;
	struct sensor_module_tab_tag  *p_mod;
	SENSOR_CHECK_ZERO(p_file);

	p_mod = p_file->module_data;
	SENSOR_CHECK_ZERO(p_mod);

	i2c_client = p_mod->sensor_dev_tab[p_file->sensor_id].cur_i2c_client;

	printk("SENSOR: burst w Init\n");
	if (0 == i2c_client) {
		SENSOR_PRINT_ERR("SENSOR: burst w Init err, i2c_clnt NULL!.\n");
		return -1;
	}
	p_reg_val_tmp = (uint8_t*)_sensor_k_malloc(fd_handle, init_table_size*sizeof(uint16_t) + 16);

	if(PNULL == p_reg_val_tmp){
		SENSOR_PRINT_ERR("_sensor_burst_write_init ERROR: alloc is fail, size = %d \n", init_table_size*sizeof(uint16_t) + 16);
		return -1;
	}
	else{
		SENSOR_PRINT_HIGH("_sensor_burst_write_init: alloc success, size = %d \n", init_table_size*sizeof(uint16_t) + 16);
	}


	while (written_num < init_table_size) {
		wr_num_once = 2;

		wr_reg = p_reg_table[written_num].reg_addr;
		wr_val = p_reg_table[written_num].reg_value;
		if (SENSOR_WRITE_DELAY == wr_reg) {
			if (wr_val >= 10) {
				msleep(wr_val);
			} else {
				mdelay(wr_val);
			}
		} else {
			p_reg_val_tmp[0] = (uint8_t)(wr_reg);
			p_reg_val_tmp[1] = (uint8_t)(wr_val);

			if ((0x0e == wr_reg) && (0x01 == wr_val)) {
				for (i = written_num + 1; i < init_table_size; i++) {
					if ((0x0e == wr_reg) && (0x00 == wr_val)) {
						break;
					} else {
						wr_val = p_reg_table[i].reg_value;
						p_reg_val_tmp[wr_num_once+1] = (uint8_t)(wr_val);
						wr_num_once ++;
					}
				}
			}
			msg_w.addr = i2c_client->addr;
			msg_w.flags = 0;
			msg_w.buf = p_reg_val_tmp;
			msg_w.len = (uint32_t)(wr_num_once);
			ret = i2c_transfer(i2c_client->adapter, &msg_w, 1);
			if (ret!=1) {
				SENSOR_PRINT("SENSOR: s err, val {0x%x 0x%x} {0x%x 0x%x} {0x%x 0x%x} {0x%x 0x%x} {0x%x 0x%x} {0x%x 0x%x}.\n",
					p_reg_val_tmp[0],p_reg_val_tmp[1],p_reg_val_tmp[2],p_reg_val_tmp[3],
					p_reg_val_tmp[4],p_reg_val_tmp[5],p_reg_val_tmp[6],p_reg_val_tmp[7],
					p_reg_val_tmp[8],p_reg_val_tmp[9],p_reg_val_tmp[10],p_reg_val_tmp[11]);
					SENSOR_PRINT("SENSOR: i2c w once err\n");
				rtn = 1;
				break;
			}
		}
		written_num += wr_num_once - 1;
	}
	SENSOR_PRINT("SENSOR: burst w Init OK\n");
	_sensor_k_free(fd_handle, p_reg_val_tmp);
	return rtn;
}
#endif

void sensor_k_set_i2c_addr(struct file *file, uint16_t i2c_addr)
{
	struct sensor_module_tab_tag    *p_mod;
	struct sensor_file_tag         *p_file = file->private_data;

	SENSOR_CHECK_ZERO(p_file);
	p_mod = p_file->module_data;
	SENSOR_CHECK_ZERO(p_mod);

	p_mod->sensor_dev_tab[p_file->sensor_id].cur_i2c_client->addr = (p_mod->sensor_dev_tab[p_file->sensor_id].cur_i2c_client->addr & (~0xFF)) |i2c_addr;
}

void sensor_k_set_id(struct file *file, uint32_t sensor_id)
{
	struct sensor_module_tab_tag    *p_mod;
	struct sensor_file_tag         *p_file = file->private_data;
	SENSOR_CHECK_ZERO(p_file);
	p_mod = p_file->module_data;
	SENSOR_CHECK_ZERO(p_mod);
	p_file->sensor_id = sensor_id;
	get_gpio_id(p_mod->of_node, &p_file->gpio_tab.pwn, &p_file->gpio_tab.reset, p_file->sensor_id);
}

LOCAL long sensor_k_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int                            ret = 0;
	struct sensor_module_tab_tag    *p_mod;
	struct sensor_file_tag         *p_file = file->private_data;

	SENSOR_CHECK_ZERO(p_file);
	p_mod = p_file->module_data;
	SENSOR_CHECK_ZERO(p_mod);

	SENSOR_PRINT("SENSOR: ioctl cmd %d id %d \n", cmd, p_file->sensor_id);

	if (SENSOR_IO_SET_ID == cmd)
	{
		mutex_lock(&p_mod->sensor_id_lock);
		ret = copy_from_user(&p_file->sensor_id, (uint32_t *) arg, sizeof(uint32_t));
		mutex_unlock(&p_mod->sensor_id_lock);
	}

	mutex_lock(&p_mod->sensor_dev_tab[p_file->sensor_id].sync_lock);
	switch (cmd) {
	case SENSOR_IO_PD:
		{
			uint8_t power_level;
			SENSOR_PRINT("SENSOR: ioctl SENSOR_IO_PD \n");
			ret = copy_from_user(&power_level, (uint8_t *) arg, sizeof(uint8_t));

			if (0 == ret)
				ret = sensor_k_set_pd_level((uint32_t *)p_file, power_level);
		}
		break;

	case SENSOR_IO_SET_CAMMOT:
		{
			uint32_t vdd_val;
			SENSOR_PRINT("SENSOR: ioctl SENSOR_IO_SET_CAMMOT \n");
			ret = copy_from_user(&vdd_val, (uint32_t *) arg, sizeof(uint32_t));
			if (0 == ret)
				ret = sensor_k_set_voltage_cammot((uint32_t *)p_file, vdd_val);
		}
		break;

	case SENSOR_IO_SET_AVDD:
		{
			uint32_t vdd_val;
			SENSOR_PRINT("SENSOR: ioctl SENSOR_IO_SET_AVDD \n");
			ret = copy_from_user(&vdd_val, (uint32_t *) arg, sizeof(uint32_t));
			if (0 == ret)
				ret = sensor_k_set_voltage_avdd((uint32_t *)p_file, vdd_val);
		}
		break;

	case SENSOR_IO_SET_DVDD:
		{
			uint32_t vdd_val;
			SENSOR_PRINT("SENSOR: ioctl SENSOR_IO_SET_DVDD \n");
			ret = copy_from_user(&vdd_val, (uint32_t *) arg, sizeof(uint32_t));
			if (0 == ret)
				ret = sensor_k_set_voltage_dvdd((uint32_t *)p_file, vdd_val);
		}
		break;

	case SENSOR_IO_SET_IOVDD:
		{
			uint32_t vdd_val;
			SENSOR_PRINT("SENSOR: ioctl SENSOR_IO_SET_IOVDD \n");
			ret = copy_from_user(&vdd_val, (uint32_t *) arg, sizeof(uint32_t));
			if (0 == ret)
				ret = sensor_k_set_voltage_iovdd((uint32_t *)p_file, vdd_val);
		}
		break;

	case SENSOR_IO_SET_MCLK:
		{
			uint32_t mclk;
			SENSOR_PRINT("SENSOR: ioctl SENSOR_IO_SET_MCLK \n");
			ret = copy_from_user(&mclk, (uint32_t *) arg, sizeof(uint32_t));
			if (0 == ret)
				ret = _sensor_k_set_mclk(p_file, p_mod->of_node, mclk);
		}
		break;

	case SENSOR_IO_RST:
		{
			uint32_t rst_val[2];
			SENSOR_PRINT("SENSOR: ioctl SENSOR_IO_RST \n");
			ret = copy_from_user(rst_val, (uint32_t *) arg, 2*sizeof(uint32_t));
			if (0 == ret)
				ret = _sensor_k_reset(p_file, rst_val[0], rst_val[1]);
		}
		break;

	case SENSOR_IO_I2C_INIT:
		{
			uint32_t sensor_id;
			SENSOR_PRINT("SENSOR: ioctl SENSOR_IO_I2C_INIT \n");
			ret = copy_from_user(&sensor_id, (uint32_t *) arg, sizeof(uint32_t));
			if (0 == ret)
				ret = sensor_k_sensor_sel((uint32_t *)p_file, sensor_id);
		}
		break;

	case SENSOR_IO_I2C_DEINIT:
		{
			uint32_t sensor_id;
			SENSOR_PRINT("SENSOR: ioctl SENSOR_IO_I2C_DEINIT \n");
			ret = copy_from_user(&sensor_id, (uint32_t *) arg, sizeof(uint32_t));
			if (0 == ret)
				ret = sensor_k_sensor_desel(p_file, sensor_id);
		}
		break;

	case SENSOR_IO_SET_ID:
		{
			SENSOR_PRINT("SENSOR: ioctl SENSOR_IO_SET_ID \n");
			ret = copy_from_user(&p_file->sensor_id, (uint32_t *) arg, sizeof(uint32_t));
			get_gpio_id(p_mod->of_node, &p_file->gpio_tab.pwn, &p_file->gpio_tab.reset, p_file->sensor_id);
		}
		break;

	case SENSOR_IO_RST_LEVEL:
		{
			uint32_t level;
			SENSOR_PRINT("SENSOR: ioctl SENSOR_IO_RST_LEVEL \n");
			ret = copy_from_user(&level, (uint32_t *) arg, sizeof(uint32_t));
			if (0 == ret)
				ret = sensor_k_set_rst_level((uint32_t *)p_file, level);
		}
		break;

	case SENSOR_IO_SET_MIPI_SWITCH:
		{
			uint32_t level;
			SENSOR_PRINT("SENSOR: ioctl SENSOR_IO_SET_MIPI_SWITCH \n");
			ret = copy_from_user(&level, (uint32_t *) arg, sizeof(uint32_t));
			if (0 == ret)
				ret = sensor_k_set_mipi_level((uint32_t *)p_file, level);
		}
		break;

	case SENSOR_IO_I2C_ADDR:
		{
			uint16_t i2c_addr;
			SENSOR_PRINT("SENSOR: ioctl SENSOR_IO_I2C_ADDR \n");
			ret = copy_from_user(&i2c_addr, (uint16_t *) arg, sizeof(uint16_t));
			if (0 == ret) {
				p_mod->sensor_dev_tab[p_file->sensor_id].cur_i2c_client->addr = (p_mod->sensor_dev_tab[p_file->sensor_id].cur_i2c_client->addr & (~0xFF)) |i2c_addr;
				printk("SENSOR_IO_I2C_ADDR: addr = %x, %x \n", i2c_addr, p_mod->sensor_dev_tab[p_file->sensor_id].cur_i2c_client->addr);
			}
		}
		break;

	case SENSOR_IO_I2C_READ:
		{
			struct sensor_reg_bits_tag reg;
			SENSOR_PRINT("SENSOR: ioctl SENSOR_IO_I2C_READ \n");
			ret = copy_from_user(&reg, (struct sensor_reg_bits_tag *) arg, sizeof(struct sensor_reg_bits_tag));

			if (0 == ret) {
				ret = _Sensor_K_ReadReg(p_file, &reg);
				if(SENSOR_K_FAIL != ret){
					ret = copy_to_user((struct sensor_reg_bits_tag *)arg, &reg, sizeof(struct sensor_reg_bits_tag));
				}
			}
		}
		break;

	case SENSOR_IO_I2C_WRITE:
		{
			struct sensor_reg_bits_tag reg;
			SENSOR_PRINT("SENSOR: ioctl SENSOR_IO_I2C_WRITE \n");
			ret = copy_from_user(&reg, (struct sensor_reg_bits_tag *) arg, sizeof(struct sensor_reg_bits_tag));

			if (0 == ret) {
				ret = _Sensor_K_WriteReg(p_file, &reg);
			}

		}
		break;

	case SENSOR_IO_I2C_WRITE_REGS:
		{
			struct sensor_reg_tab_tag regTab;
			SENSOR_PRINT("SENSOR: ioctl SENSOR_IO_I2C_WRITE_REGS \n");
			ret = copy_from_user(&regTab, (struct sensor_reg_tab_tag *) arg, sizeof(struct sensor_reg_tab_tag));
			if (0 == ret)
				ret = _sensor_k_wr_regtab(p_file, &regTab);
		}
		break;

	case SENSOR_IO_SET_I2CCLOCK:
		{
			uint32_t clock;
			SENSOR_PRINT("SENSOR: ioctl SENSOR_IO_SET_I2CCLOCK \n");
			ret = copy_from_user(&clock, (uint32_t *) arg, sizeof(uint32_t));
			if(0 == ret){
				_sensor_k_set_i2c_clk(p_file, clock);
			}
		}
		break;

	case SENSOR_IO_I2C_WRITE_EXT:
		{
			struct sensor_i2c_tag i2cTab;
			SENSOR_PRINT("SENSOR: ioctl SENSOR_IO_I2C_WRITE_EXT \n");
			ret = copy_from_user(&i2cTab, (struct sensor_i2c_tag*)arg, sizeof(struct sensor_i2c_tag));
			if (0 == ret)
				ret = _sensor_k_wr_i2c(p_file, &i2cTab);
		}
		break;

	case SENSOR_IO_I2C_READ_EXT:
		{
			struct sensor_i2c_tag i2cTab;
			SENSOR_PRINT("SENSOR: ioctl SENSOR_IO_I2C_READ_EXT \n");
			ret = copy_from_user(&i2cTab, (struct sensor_i2c_tag*) arg, sizeof(struct sensor_i2c_tag));
			if (0 == ret)
				ret = _sensor_k_rd_i2c(p_file, &i2cTab);
		}
		break;

	case SENSOR_IO_GET_FLASH_LEVEL:
		{
			struct sensor_flash_level flash_level;
			SENSOR_PRINT("SENSOR: ioctl SENSOR_IO_GET_FLASH_LEVEL \n");
			ret = copy_from_user(&flash_level, (struct sensor_flash_level *) arg, sizeof(struct sensor_flash_level));
			if (0 == ret) {
				ret = _sensor_k_get_flash_level(p_file, &flash_level);
				if(SENSOR_K_FAIL != ret){
					ret = copy_to_user((struct sensor_flash_level *)arg, &flash_level, sizeof(struct sensor_flash_level));
				}
			}
		}
		break;
	case SENSOR_IO_SET_FLASH:
		{
			uint32_t flash_mode;
			ret = copy_from_user(&flash_mode, (uint32_t *) arg, sizeof(uint32_t));
			if (0 == ret)
				ret = _sensor_k_set_flash(flash_mode);
		}
		break;
		
	case SENSOR_IO_GET_SOCID:
		{
			struct sensor_socid_tag	Id  ;
			SENSOR_PRINT("SENSOR: ioctl SENSOR_IO_GET_SOCID \n");
			//Id.d_die=sci_get_chip_id();
			//Id.a_die=sci_get_ana_chip_id()|sci_get_ana_chip_ver();
			SENSOR_PRINT("cpu id 0x%x,0x%x  \n", Id.d_die,Id.a_die);
			ret = copy_to_user((struct sensor_socid_tag *)arg, &Id, sizeof(struct sensor_socid_tag));
		}
		break;
	case SENSOR_IO_IF_CFG:
		{
			struct sensor_if_cfg_tag if_cfg;
			struct csi_context *csi_handle;
			csi_handle = p_file->csi_handle;
			if (NULL == csi_handle) {
				printk("handle null\n");
				return -1;
			}
			SENSOR_PRINT("SENSOR: ioctl SENSOR_IO_IF_CFG \n");
			ret = copy_from_user((void*)&if_cfg, (struct sensor_if_cfg_tag *)arg, sizeof(struct sensor_if_cfg_tag));
			if (0 == ret) {
				if (INTERFACE_OPEN == if_cfg.is_open) {
					if (INTERFACE_MIPI == if_cfg.if_type) {
						if (0 == p_file->mipi_on) {
							_sensor_k_mipi_clk_en(p_file, p_mod->of_node);
							udelay(1);
							ret = csi_api_init(if_cfg.bps_per_lane, if_cfg.phy_id);
							SENSOR_PRINT("csi_api_init: %d \n", ret);
							ret = csi_api_start(csi_handle);
							SENSOR_PRINT("csi_api_start: %d \n", ret);
							ret = csi_reg_isr(csi_handle, _sensor_csi2_error, (void*)p_file);
							SENSOR_PRINT("csi_reg_isr: %d \n", ret);
							ret = csi_set_on_lanes(if_cfg.lane_num);
							SENSOR_PRINT("csi_set_on_lanes: %d \n", ret);
							p_file->mipi_on = 1;
							p_file->phy_id = if_cfg.phy_id;
							p_file->if_type = INTERFACE_MIPI;
							printk("MIPI on, lane %d, bps_per_lane %d, wait 10us \n", if_cfg.lane_num, if_cfg.bps_per_lane);
						} else {
							printk("MIPI already on \n");
						}
					}
				} else {
					if (INTERFACE_MIPI == if_cfg.if_type) {
						if (1 == p_file->mipi_on) {
							csi_api_close(csi_handle, if_cfg.phy_id);
							_sensor_k_mipi_clk_dis(p_file);
							p_file->mipi_on = 0;
							printk("MIPI off \n");
						} else {
							printk("MIPI already off \n");
						}

					}

				}
			}
		}
		break;

		case SENSOR_IO_POWER_CFG:
		{
			struct sensor_power_info_tag pwr_cfg;

			SENSOR_PRINT("SENSOR: ioctl SENSOR_IO_POWER_CFG \n");
			ret = copy_from_user(&pwr_cfg, (struct sensor_power_info_tag*) arg, sizeof(struct sensor_power_info_tag));
			if (0 == ret) {
#if defined(CONFIG_MACH_J1MINI3G)
				if(1 == pwr_cfg.is_on) {
					ret = sensor_power_on((uint32_t *)p_file, pwr_cfg.op_sensor_id, &pwr_cfg.dev0, &pwr_cfg.dev1, &pwr_cfg.dev2);
				} else if (0x10 == pwr_cfg.is_on) {
					ret = sensor_power_init((uint32_t *)p_file, pwr_cfg.op_sensor_id, &pwr_cfg.dev0, &pwr_cfg.dev1, &pwr_cfg.dev2);
				} else {
					ret = sensor_power_off((uint32_t *)p_file, pwr_cfg.op_sensor_id, &pwr_cfg.dev0, &pwr_cfg.dev1, &pwr_cfg.dev2);
				}
#else
				if (pwr_cfg.is_on) {
					ret = sensor_power_on((uint32_t *)p_file, pwr_cfg.op_sensor_id, &pwr_cfg.dev0, &pwr_cfg.dev1, &pwr_cfg.dev2);
				} else {
					ret = sensor_power_off((uint32_t *)p_file, pwr_cfg.op_sensor_id, &pwr_cfg.dev0, &pwr_cfg.dev1, &pwr_cfg.dev2);
				}
#endif
			}
		}
		break;

		case SENSOR_IO_READ_OTPDATA:
		{
			SENSOR_OTP_PARAM_T *para = (SENSOR_OTP_PARAM_T *)arg;
			uint32_t type;
			copy_from_user(&type, &para->type, sizeof(uint32_t));
			printk("SENSOR: ioctl SENSOR_IO_READ_OTPDATA 0x%lx type %d\n", arg, para->type);

			if(type == SENSOR_OTP_PARAM_CHECKSUM) {
				SENSOR_OTP_DATA_INFO_T *chksum = &p_mod->otp_param[p_file->sensor_id].golden;
				if(chksum->data_ptr && chksum->size > 0) {
					copy_to_user(para->buff, chksum->data_ptr, chksum->size);
					copy_to_user(&para->len, &chksum->size, sizeof(uint32_t));
				}
			} else if(type == SENSOR_OTP_PARAM_NORMAL) {

				SENSOR_OTP_DATA_INFO_T *awb = &p_mod->otp_param[p_file->sensor_id].awb;
				SENSOR_OTP_DATA_INFO_T *lsc = &p_mod->otp_param[p_file->sensor_id].lsc;

				if(p_mod->otp_param[p_file->sensor_id].buff && p_mod->otp_param[p_file->sensor_id].len > 0) {
					copy_to_user(para->buff, p_mod->otp_param[p_file->sensor_id].buff,
						p_mod->otp_param[p_file->sensor_id].len);
					copy_to_user(&para->len, &p_mod->otp_param[p_file->sensor_id].len,
						sizeof(uint32_t));
				}
				if(awb->data_ptr && awb->size > 0) {
					copy_to_user(para->awb.data_ptr, awb->data_ptr, awb->size);
					copy_to_user(&para->awb.size, &awb->size, sizeof(uint32_t));
				}
				if(lsc->data_ptr && lsc->size > 0) {
					copy_to_user(para->lsc.data_ptr, lsc->data_ptr, lsc->size);
					copy_to_user(&para->lsc.size, &lsc->size, sizeof(uint32_t));
				}
			} else  {
				printk("SENSOR: ioctl SENSOR_IO_READ_OTPDATA mismatch type \n");
			}
		}
		break;

	default:
		SENSOR_PRINT("sensor_k_ioctl: inv cmd %x  \n", cmd);
		break;
	}

	mutex_unlock(&p_mod->sensor_dev_tab[p_file->sensor_id].sync_lock);
	return (long)ret;
}

LOCAL struct file_operations sensor_fops = {
	.owner = THIS_MODULE,
	.open = sensor_k_open,
	.read = sensor_k_read,
	.write = sensor_k_write,
	.unlocked_ioctl = sensor_k_ioctl,
	.compat_ioctl = compat_sensor_k_ioctl,
	.release = sensor_k_release,
};

LOCAL struct miscdevice sensor_dev = {
	.minor = SENSOR_MINOR,
	.name = SENSOR_DEVICE_NAME,
	.fops = &sensor_fops,
};

LOCAL int sensor_k_register_subdevs(struct platform_device *pdev)
{
	struct device_node             *adapter, *child;
	struct sensor_module_tab_tag   *p_mod = platform_get_drvdata(pdev);

	SENSOR_CHECK_ZERO(p_mod);

	printk("sensor register sub device E\n");
	for_each_compatible_node(adapter, NULL, "sprd,i2c") {
		if (!of_find_device_by_node(adapter)) {
			of_node_put(adapter);
			printk("sensor find device fail\n");
			//return -EPROBE_DEFER;
			return 0;
		}

		for_each_available_child_of_node(adapter, child) {
			struct i2c_client *client;

			client = of_find_i2c_device_by_node(child);
			if (!client) {
				printk("sensor find i2c device fail\n");
				goto e_retry;
			}
			if (0 == strcmp(client->name, SENSOR_DEV0_I2C_NAME)) {
				printk("sensor dev0 i2c device 0x%x %s\n", client->addr, client->name);
				p_mod->sensor_dev_tab[0].cur_i2c_client = client;
			}
			if (0 == strcmp(client->name, SENSOR_DEV1_I2C_NAME)) {
				printk("sensor dev1 i2c device 0x%x %s\n", client->addr, client->name);
				p_mod->sensor_dev_tab[1].cur_i2c_client = client;
			}
			if (0 == strcmp(client->name, SENSOR_DEV2_I2C_NAME)) {
				printk("sensor dev2 i2c device 0x%x %s\n", client->addr, client->name);
				p_mod->sensor_dev_tab[2].cur_i2c_client = client;
			}
			if (0 == strcmp(client->name, SENSOR_VCM0_I2C_NAME)) {
				printk("sensor vcm0 i2c device 0x%x %s\n", client->addr, client->name);
				p_mod->sensor_dev_tab[0].vcm_i2c_client = client;
				p_mod->sensor_dev_tab[0].vcm_gpio_i2c_flag = 0;
			}
		}
	}

	for_each_compatible_node(adapter, NULL, "i2c-gpio") {
		if (!of_find_device_by_node(adapter)) {
			of_node_put(adapter);
			printk("sensor find device fail\n");
			return -EPROBE_DEFER;
		}

		for_each_available_child_of_node(adapter, child) {
			struct i2c_client *client;

			client = of_find_i2c_device_by_node(child);
			if (!client) {
				printk("sensor find vcm %s i2c device fail\n",child->name);
			} else {
				if (0 == strcmp(client->name, SENSOR_VCM0_I2C_NAME)) {
					printk("sensor vcm i2c device 0x%x %s\n", client->addr, client->name);
					p_mod->sensor_dev_tab[0].vcm_i2c_client = client;
					p_mod->sensor_dev_tab[0].vcm_gpio_i2c_flag = 1;
				}
			}
		}
	}


	printk("sensor register sub device success\n");
	return 0;

e_retry:
	of_node_put(child);
	return -EPROBE_DEFER;
}

int sensor_k_probe(struct platform_device *pdev)
{
	int                          ret = 0;
	uint32_t                     tmp = 0;
	int                          i;
	struct sensor_module_tab_tag *p_mod;
#ifndef	CONFIG_SC_FPGA_PIN
	struct sensor_gpio_tag       gpio_tab;
#endif
	int gpio_id = 0;
	int gpio_mipi_en = 0;
	printk(KERN_ALERT "sensor probe called\n");
	p_mod = (struct sensor_module_tab_tag *)vzalloc(sizeof(struct sensor_module_tab_tag));
	SENSOR_CHECK_ZERO(p_mod);

	for (i = 0; i < SENSOR_DEV_MAX; i++) {
		mutex_init(&p_mod->sensor_dev_tab[i].sync_lock);
		atomic_set(&p_mod->sensor_dev_tab[i].users, 0);
	}
	mutex_init(&p_mod->sensor_id_lock);
	wake_lock_init(&p_mod->wakelock, WAKE_LOCK_SUSPEND,
                   "pm_message_wakelock_sensor_k");
	platform_set_drvdata(pdev, p_mod);
	atomic_set(&p_mod->total_users, 0);

	ret = misc_register(&sensor_dev);
	if (ret) {
		printk(KERN_ERR "can't reg miscdev on minor=%d (%d)\n",
			SENSOR_MINOR, ret);
		goto misc_register_error;
	}

	p_mod->of_node = pdev->dev.of_node;
	parse_baseaddress(pdev->dev.of_node);
	sensor_dev.this_device->platform_data = (void*)p_mod;

#ifndef	CONFIG_SC_FPGA_PIN
	for (i = 0; i < SENSOR_DEV_MAX; i++) {
		get_gpio_id(p_mod->of_node, &gpio_tab.pwn, &gpio_tab.reset, i);
		if(gpio_tab.pwn > 0) {
			ret = gpio_request(gpio_tab.pwn, NULL);
			if (ret) {
				tmp = 1;
				printk("sensor: gpio already request pwn %d %d.\n", gpio_tab.pwn, i);
			}
			gpio_direction_output(gpio_tab.pwn, 1);
			gpio_set_value(gpio_tab.pwn, 0);
		} else {
			printk("sensor: can't find gpio pwn %d (errno = %d)\n",
					i, gpio_tab.pwn);
		}
		if(gpio_tab.reset > 0) {
			ret = gpio_request(gpio_tab.reset, NULL);
			if (ret) {
				tmp = 1;
				printk("sensor:  gpio already request reset %d %d.\n", gpio_tab.reset, i);
			}
			gpio_direction_output(gpio_tab.reset, 1);
			gpio_set_value(gpio_tab.reset, 0);
		} else {
			printk("sensor: can't find gpio reset %d (errno = %d)\n",
					i, gpio_tab.pwn);
		}
	}

	get_gpio_id_ex(p_mod->of_node, GPIO_MAINDVDD, &gpio_id, 0);
	if(gpio_id > 0) {
		ret = gpio_request(gpio_id, NULL);
		if (ret) {
			tmp = 1;
			printk("sensor: gpio already request GPIO_MAINDVDD,  gpio_id = %d.\n", gpio_id);
		 }
		gpio_direction_output(gpio_id, 1);
		gpio_set_value(gpio_id, 0);
	} else {
		printk("sensor: can't find GPIO_MAINDVDD (errno = %d)\n",
				gpio_id);
	}

	get_gpio_id_ex(p_mod->of_node, GPIO_SUBDVDD, &gpio_id, 0);
	if(gpio_id > 0) {
		ret = gpio_request(gpio_id, NULL);
		if (ret) {
			tmp = 1;
			printk("sensor: gpio already request GPIO_SUBDVDD,  gpio_id = %d.\n", gpio_id);
		 }
		gpio_direction_output(gpio_id, 1);
		gpio_set_value(gpio_id, 0);
	} else {
		printk("sensor: can't find GPIO_SUBDVDD (errno = %d)\n",
				gpio_id);
	}

	get_gpio_id_ex(p_mod->of_node, GPIO_SUB2DVDD, &gpio_id, 0);
	if(gpio_id > 0) {
		ret = gpio_request(gpio_id, NULL);
		if (ret) {
			tmp = 1;
			printk("sensor: gpio already request GPIO_SUB2DVDD,  gpio_id = %d.\n", gpio_id);
		 }
		gpio_direction_output(gpio_id, 1);
		gpio_set_value(gpio_id, 0);
	} else {
		printk("sensor: can't find GPIO_SUB2DVDD (errno = %d)\n",
				gpio_id);
	}

	get_gpio_id_ex(p_mod->of_node, GPIO_FLASH_EN, &gpio_id, 0);
	if(gpio_id > 0) {
		ret = gpio_request(gpio_id, NULL);
		if (ret) {
			tmp = 1;
			printk("sensor: gpio already request GPIO_FLASH_EN,  gpio_id = %d.\n", gpio_id);
		}
		gpio_direction_output(gpio_id, 1);
		gpio_set_value(gpio_id, 0);
	} else {
		printk("sensor: can't find GPIO_FLASH_EN (errno = %d)\n",
				gpio_id);
	}

	get_gpio_id_ex(p_mod->of_node, GPIO_SWITCH_MODE, &gpio_id, 0);
	if(gpio_id > 0) {
		ret = gpio_request(gpio_id, NULL);
		if (ret) {
			tmp = 1;
			printk("sensor: gpio already request GPIO_SWITCH_MODE,  gpio_id = %d.\n", gpio_id);
		 }
		gpio_direction_output(gpio_id, 1);
		gpio_set_value(gpio_id, 0);
	} else {
		printk("sensor: can't find GPIO_SWITCH_MODE (errno = %d)\n",
				gpio_id);
	}

	get_gpio_id_ex(p_mod->of_node, GPIO_MIPI_SWITCH_EN, &gpio_id, 0);
	if(gpio_id > 0) {
		ret = gpio_request(gpio_id, NULL);
		if (ret) {
			tmp = 1;
			printk("sensor: gpio already request GPIO_MIPI_SWITCH_EN,  gpio_id = %d.\n", gpio_id);
		 }
		gpio_direction_output(gpio_id, 1);
		gpio_set_value(gpio_id, 0);
	} else {
		printk("sensor: can't find GPIO_MIPI_SWITCH_EN (errno = %d)\n",
				gpio_id);
	}

	get_gpio_id_ex(p_mod->of_node, GPIO_MIPI_SWITCH_MODE, &gpio_id, 0);
	if(gpio_id > 0) {
		ret = gpio_request(gpio_id, NULL);
		if (ret) {
			tmp = 1;
			printk("sensor: gpio already request GPIO_MIPI_SWITCH_MODE,  gpio_id = %d.\n", gpio_id);
		}
		gpio_direction_output(gpio_id, 1);
		gpio_set_value(gpio_id, 1);
	} else {
		printk("sensor: can't find GPIO_MIPI_SWITCH_MODE (errno = %d)\n",
				gpio_id);
	}

	get_gpio_id_ex(p_mod->of_node, GPIO_MAINCAM_ID, &gpio_id, 0);
	if(gpio_id > 0) {
		ret = gpio_request(gpio_id, NULL);
		if (ret) {
			tmp = 1;
			printk("sensor: gpio already request GPIO_MAINCAM_ID,  gpio_id = %d.\n", gpio_id);
		}
		gpio_direction_output(gpio_id, 1);
	} else {
		printk("sensor: can't find GPIO_MAINCAM_ID (errno = %d)\n",
				gpio_id);
	}
#endif
#if defined(CONFIG_MACH_J1X3G) || defined(CONFIG_MACH_J3X3G)
	if (!gpio_is_valid(CAM_FLASH_EN)) { // Flash EN
		printk("CAM_FLASH_EN gpio pin error\n");
		return 1;
	}
	gpio_request(CAM_FLASH_EN, "CAM_FLASH_EN");
	gpio_direction_output(CAM_FLASH_EN, 0); // Set Flash Enable GPIO as Output

	if (!gpio_is_valid(CAM_FLASH_TORCH)) { // Flash Torch
		printk("CAM_FLASH_TORCH gpio pin error\n");
		return 1;
	}
	gpio_request(CAM_FLASH_TORCH, "CAM_FLASH_TORCH");
	gpio_direction_output(CAM_FLASH_TORCH, 0); // Set Flash Enable GPIO as Output
#endif

	ret = sensor_k_register_subdevs(pdev);
	if (ret) {
		printk(KERN_ERR "can't reg sub dev=%d (%d)\n",
			SENSOR_MINOR, ret);
		goto misc_register_error;
	}
	kthread_run(sensor_reloadinfo_thread, p_mod->otp_param, "otpreload");

#if defined(CONFIG_MACH_J1MINI3G)		// Set Front Camera GPIO value for DVS
	get_gpio_id(p_mod->of_node, &gpio_tab.pwn, &gpio_tab.reset, 1);
	ret = gpio_request(gpio_tab.pwn, NULL);
	if (ret) {
		printk("sensor: gpio already request pwn %d %d.\n", gpio_tab.pwn, 1);
	}
	gpio_direction_output(gpio_tab.pwn, 0);

	ret = gpio_request(gpio_tab.reset, NULL);
	if (ret) {
		printk("sensor:  gpio already request reset %d %d.\n", gpio_tab.reset, 1);
	}
	gpio_direction_output(gpio_tab.reset, 0);
#endif

	goto exit;

misc_register_error:
	misc_deregister(&sensor_dev);

	if (SENSOR_ADDR_INVALID(p_mod)) {
		printk("SENSOR: Invalid addr, %p", p_mod);
	} else {
		vfree(p_mod);
		p_mod = NULL;
		platform_set_drvdata(pdev, NULL);
	}
exit:
	if (ret) {
		printk(KERN_ERR "sensor prb fail req gpio %d err %d\n",
			tmp, ret);
	} else {
		printk(KERN_ALERT " sensor prb Success\n");
	}
	return ret;
}

struct device_node *get_device_node(void)
{
    return ((struct sensor_module_tab_tag *)(sensor_dev.this_device->platform_data))->of_node;
}


LOCAL int sensor_k_remove(struct platform_device *dev)
{
	struct sensor_module_tab_tag *p_mod = platform_get_drvdata(dev);
#ifndef CONFIG_SC_FPGA_PIN
	struct sensor_gpio_tag       gpio_tab;
	int                          i;
#endif
	int                          gpio_id = 0;

	SENSOR_CHECK_ZERO(p_mod);

	printk(KERN_INFO "sensor remove called !\n");

#ifndef CONFIG_SC_FPGA_PIN
	for (i = 0; i < SENSOR_DEV_MAX; i++) {
		get_gpio_id(p_mod->of_node, &gpio_tab.pwn, &gpio_tab.reset, i);
		gpio_free(gpio_tab.pwn);
		gpio_free(gpio_tab.reset);
	}

       for (i = 0; i < GPIO_CAMMAX; i++) {
	   	get_gpio_id_ex(p_mod->of_node, i, &gpio_id, 0);
		gpio_free(gpio_id);
		gpio_id = 0;
	}
#endif

	misc_deregister(&sensor_dev);
	wake_lock_destroy(&p_mod->wakelock);

	if (SENSOR_ADDR_INVALID(p_mod)) {
		printk("SENSOR: Invalid addr, %p", p_mod);
	} else {
		vfree(p_mod);
		p_mod = NULL;
		platform_set_drvdata(dev, NULL);
	}
	printk(KERN_INFO "sensor remove Success !\n");
	return 0;
}

LOCAL const struct of_device_id of_match_table_sensor[] = {
	{ .compatible = "sprd,sprd_sensor", },
	{ },
};

static struct platform_driver sensor_dev_driver = {
	.probe = sensor_k_probe,
	.remove =sensor_k_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name  = SENSOR_DEVICE_NAME,
		.of_match_table = of_match_ptr(of_match_table_sensor),
		},
};

#if defined(CONFIG_MACH_J1X3G)
#define REAR_SENSOR_NAME            "S5K4ECGA N\n"
#define FRONT_SENSOR_NAME           "SR200PC20M N\n"
#define REAR_SENSOR_TYPE            "SOC\n"
#define FRONT_SENSOR_TYPE           "SOC\n"
#define REAR_CAMERA_INFO            "ISP=SOC;CALMEM=N;READVER=SYSFS;COREVOLT=N;UPGRADE=N;CC=N;OIS=N;"	// 150514, heechul. temporal hardcoding
#define FRONT_CAMERA_INFO           "ISP=SOC;CALMEM=N;READVER=SYSFS;COREVOLT=N;UPGRADE=N;CC=N;OIS=N;"	// 150514, heechul. temporal hardcoding
#elif defined(CONFIG_MACH_J3X3G)
#define REAR_SENSOR_NAME            "J08PLIC01AA J08PLIC01AA\n"
#define REAR_FULL_SENSOR_NAME       "J08PLIC01AA J08PLIC01AA J08PLIC01AA\n"	
#define FRONT_SENSOR_NAME           "S5K5E3YX N\n"
#define FRONT_FULL_SENSOR_NAME      "S5K5E3YX N S5K5E3YX\n"
#define REAR_SENSOR_TYPE            "ISP\n"
#define FRONT_SENSOR_TYPE           "SOC\n"
#define REAR_CAMERA_INFO            "ISP=INT;CALMEM=Y;READVER=SYSFS;COREVOLT=N;UPGRADE=N;CC=N;OIS=N;"
#define FRONT_CAMERA_INFO           "ISP=INT;CALMEM=N;READVER=SYSFS;COREVOLT=N;UPGRADE=N;CC=N;OIS=N;"
#elif defined(CONFIG_MACH_GTEXSWIFI)
#define REAR_SENSOR_NAME            "S5K4ECGA N\n"
#define FRONT_SENSOR_NAME           "SR259 N\n"
#define REAR_SENSOR_TYPE            "SOC\n"
#define FRONT_SENSOR_TYPE           "SOC\n"
#define REAR_CAMERA_INFO            "ISP=SOC;CALMEM=N;READVER=SYSFS;COREVOLT=N;UPGRADE=N;CC=N;OIS=N;"	// 150514, heechul. temporal hardcoding
#define FRONT_CAMERA_INFO           "ISP=SOC;CALMEM=N;READVER=SYSFS;COREVOLT=N;UPGRADE=N;CC=N;OIS=N;"	// 150514, heechul. temporal hardcoding
#elif defined(CONFIG_MACH_J1MINI3G)
#define REAR_SENSOR_NAME            "A05PFII01PA A05PFII01PA\n"
#define REAR_FULL_SENSOR_NAME       "A05PFII01PA A05PFII01PA A05PFII01PA\n"
#define FRONT_SENSOR_NAME           "SR030PC50 N\n"
#define FRONT_FULL_SENSOR_NAME      "SR030PC50 N SR030PC50\n"
#define REAR_SENSOR_TYPE            "ISP\n"
#define FRONT_SENSOR_TYPE           "SOC\n"
#define REAR_CAMERA_INFO            "ISP=INT;CALMEM=Y;READVER=SYSFS;COREVOLT=N;UPGRADE=N;CC=N;OIS=N;"
#define FRONT_CAMERA_INFO           "ISP=INT;CALMEM=N;READVER=SYSFS;COREVOLT=N;UPGRADE=N;CC=N;OIS=N;"
#else
#define REAR_SENSOR_NAME            "S5K4ECGX N\n"
#define FRONT_SENSOR_NAME           "SR200PC20M N\n"
#define REAR_SENSOR_TYPE            "SOC\n"
#define FRONT_SENSOR_TYPE           "SOC\n"
#define REAR_CAMERA_INFO            "ISP=SOC;CALMEM=N;READVER=SYSFS;COREVOLT=N;UPGRADE=N;CC=N;OIS=N;"
#define FRONT_CAMERA_INFO           "ISP=SOC;CALMEM=N;READVER=SYSFS;COREVOLT=N;UPGRADE=N;CC=N;OIS=N;"
#endif

LOCAL int _Sensor_K_SetTorch(uint32_t flash_mode)
{
	printk("_Sensor_K_SetTorch : mode %d, flash_torch_status = %d\n", flash_mode, flash_torch_status);

#if !defined(CONFIG_MACH_J1MINI3G)
	switch (flash_mode)
	{
		case 1:
		case 100:
		  	/* For torch */
			flash_torch_status=1;
			sm5701_led_ready(MOVIE_MODE);
			sm5701_set_fleden(SM5701_FLEDEN_ON_MOVIE);
			break;

		case 0:
			flash_torch_status=0;
			sm5701_set_fleden(SM5701_FLEDEN_DISABLED);
			sm5701_led_ready(LED_DISABLE);			
			break;

		default:
			printk("_Sensor_K_SetTorch : Un-know mode : flash_mode : 0x%x\n", flash_mode);
			break;
	}
#endif
	printk("_Sensor_K_SetTorch : Flash_mode : 0x%x\n", flash_mode);

	return 0;
}

#if defined(CONFIG_MACH_J1X3G)||defined(CONFIG_MACH_GTEXSWIFI)
uint16_t VENDOR_ID = 0xFFFF;
static ssize_t camera_vendorid_show(struct device *dev, struct device_attribute *attr, char *buf)
{	
	int count;	

	count = sprintf(buf, "0x%04X", VENDOR_ID);	
	printk("%s : vendor ID is 0x%04X\n", __func__, VENDOR_ID);	
	return count;
}

static ssize_t camera_vendorid_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int tmp = 0;
	sscanf(buf, "%x", &tmp);	
	VENDOR_ID = tmp;	
	printk("%s : vendor ID is 0x%04X\n", __func__, VENDOR_ID);	
	return size;
}
#endif

static ssize_t Rear_Cam_Sensor_ID(struct device *dev, struct device_attribute *attr, char *buf)
{
#if defined(CONFIG_MACH_J3XLTE) || defined(CONFIG_MACH_J3X3G) || defined(CONFIG_MACH_J1MINI3G)
	char fw_version[12]={0};
	sensor_get_fw_version_otp(fw_version);
	printk("%s : sensor id: %s", __func__, fw_version);
	return sprintf(buf, "%s %s\n", fw_version, fw_version);
#else
	SENSOR_PRINT("Rear_Cam_Sensor_ID\n");
	return sprintf(buf, REAR_SENSOR_NAME);
#endif
}

#if defined(REAR_FULL_SENSOR_NAME)
static ssize_t Rear_Cam_Full_Sensor_ID(struct device *dev, struct device_attribute *attr, char *buf)
{
#if defined(CONFIG_MACH_J3XLTE) || defined(CONFIG_MACH_J3X3G) || defined(CONFIG_MACH_J1MINI3G)
	char fw_version[12]={0};
	sensor_get_fw_version_otp(fw_version);
	printk("%s : sensor id: %s", __func__, fw_version);
	return sprintf(buf, "%s %s %s\n", fw_version, fw_version, fw_version);
#else
	SENSOR_PRINT("Rear_Cam_Full_Sensor_ID\n");
	return sprintf(buf, REAR_FULL_SENSOR_NAME);
#endif
}
#endif

static ssize_t Rear_camera_checkfw_factory(struct device *dev, struct device_attribute *attr, char *buf)
{
#if defined(CONFIG_MACH_J3XLTE) || defined(CONFIG_MACH_J3XNLTE) || defined(CONFIG_MACH_J3X3G)
	char cam_checkfw_factory[3]={0};
	char fw_version[12]={0};
	sensor_get_fw_version_otp(fw_version);

	if(((fw_version[0]=='J') || (fw_version[0]=='L')) &&
		(fw_version[1]=='0') &&
		(fw_version[2]=='8') &&
		(fw_version[3]=='P') &&
		(fw_version[4]=='L') &&
		(fw_version[5]=='I') &&
		(fw_version[6]=='C') &&
		(fw_version[7]=='0') &&
		(fw_version[8]=='0')) {
#if 1
		if(fw_version[10]=='M')
			sprintf(cam_checkfw_factory, "OK");
		else
			sprintf(cam_checkfw_factory, "NG_VER");
#else
		sprintf(cam_checkfw_factory, "OK");
#endif
	} else {
		sprintf(cam_checkfw_factory, "NG_VER");
	}

	printk("%s : %s", __func__, cam_checkfw_factory);
	return sprintf(buf, "%s\n", cam_checkfw_factory);
#else
	return sprintf(buf, "%s\n", "OK");
#endif
}

static ssize_t Rear_camera_checkfw_user(struct device *dev, struct device_attribute *attr, char *buf)
{
#if defined(CONFIG_MACH_J3XLTE) || defined(CONFIG_MACH_J3XNLTE) || defined(CONFIG_MACH_J3X3G)
	char cam_checkfw_user[3]={0};
	char fw_version[12]={0};
	sensor_get_fw_version_otp(fw_version);

	if(((fw_version[0]=='J') || (fw_version[0]=='L')) &&
		(fw_version[1]=='0') &&
		(fw_version[2]=='8') &&
		(fw_version[3]=='P') &&
		(fw_version[4]=='L') &&
		(fw_version[5]=='I') &&
		(fw_version[6]=='C') &&
		(fw_version[7]=='0') &&
		(fw_version[8]=='0')) {
#if 1
		if(fw_version[10]=='M')
			sprintf(cam_checkfw_user, "OK");
		else
			sprintf(cam_checkfw_user, "NG");
#else
		sprintf(cam_checkfw_user, "OK");
#endif
	}
	else {
		sprintf(cam_checkfw_user, "NG");
	}

	printk("%s : %s", __func__, cam_checkfw_user);
	return sprintf(buf, "%s\n", cam_checkfw_user);
#else
	return sprintf(buf, "%s\n", "OK");
#endif
}

static ssize_t Rear_Cam_Sensor_TYPE(struct device *dev, struct device_attribute *attr, char *buf)
{
	SENSOR_PRINT("Cam_Sensor_type\n");
	return sprintf(buf, REAR_SENSOR_TYPE);
}

static ssize_t Rear_Cam_Camera_Info(struct device *dev, struct device_attribute *attr, char *buf)
{
	SENSOR_PRINT("Rear_Cam_Camera_Info\n");
	return sprintf(buf, REAR_CAMERA_INFO);
}

static ssize_t Rear_Cam_store_flash(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int value;
	sscanf(buf, "%d", &value);
	printk("Rear_Cam_store_flash value = %d\n", value);
	_Sensor_K_SetTorch(value);
	return size;
}

static ssize_t Rear_Cam_show_flash(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	SENSOR_PRINT("Rear_Cam_show_flash value\n");
	return sprintf(buf, "%d", flash_torch_status);
}

#if defined(FRONT_SENSOR_NAME)
static ssize_t Front_Cam_Sensor_ID(struct device *dev, struct device_attribute *attr, char *buf)
{
#if defined(CONFIG_MACH_J1MINI3G)
#define FRONT_SENSOR_NAME_SR030      "SR030PC50 N\n"
#define FRONT_SENSOR_NAME_GC0310     "GC0310 N\n"
#define SR030PC50_CHIP_ID_VALUE      0xB8
#define GC0310_CHIP_ID_VALUE         0xA310

	uint32_t pid = 0x0000;
	sensor_get_front_sensor_pid(&pid);
	SENSOR_PRINT("%s : sensor pid: 0x%4x\n", __func__, pid);
	SENSOR_PRINT("Front_Cam_Sensor_ID\n");

	if ( SR030PC50_CHIP_ID_VALUE == pid ) {
		return sprintf(buf, FRONT_SENSOR_NAME_SR030);
	} else if ( GC0310_CHIP_ID_VALUE == pid) {
		return sprintf(buf, FRONT_SENSOR_NAME_GC0310);
	} else {
		return sprintf(buf, FRONT_SENSOR_NAME);
	}
#else
	SENSOR_PRINT("Front_Cam_Sensor_ID\n");
	return sprintf(buf, FRONT_SENSOR_NAME);
#endif
}

#if defined(FRONT_FULL_SENSOR_NAME)
static ssize_t Front_Cam_Full_Sensor_ID(struct device *dev, struct device_attribute *attr, char *buf)
{
#if defined(CONFIG_MACH_J1MINI3G)
#define FRONT_FULL_SENSOR_NAME_SR030     "SR030PC50 N SR030PC50\n"
#define FRONT_FULL_SENSOR_NAME_GC0310    "GC0310 N GC0310\n"
#define SR030PC50_CHIP_ID_VALUE          0xB8
#define GC0310_CHIP_ID_VALUE             0xA310

	uint32_t pid = 0x0000;
	sensor_get_front_sensor_pid(&pid);
	SENSOR_PRINT("%s : sensor pid: 0x%4x\n", __func__, pid);
	SENSOR_PRINT("Front_Cam_Full_Sensor_ID\n");

	if ( SR030PC50_CHIP_ID_VALUE == pid ) {
		return sprintf(buf, FRONT_FULL_SENSOR_NAME_SR030);
	} else if ( GC0310_CHIP_ID_VALUE == pid) {
		return sprintf(buf, FRONT_FULL_SENSOR_NAME_GC0310);
	} else {
		return sprintf(buf, FRONT_SENSOR_NAME);
	}
#else
	SENSOR_PRINT("Front_Cam_Full_Sensor_ID\n");
	return sprintf(buf, FRONT_FULL_SENSOR_NAME);
#endif
}
#endif

static ssize_t Front_Cam_Sensor_TYPE(struct device *dev, struct device_attribute *attr, char *buf)
{
	SENSOR_PRINT("Cam_Sensor_type\n");
	return sprintf(buf, FRONT_SENSOR_TYPE);
}

static ssize_t Front_Cam_Camera_Info(struct device *dev, struct device_attribute *attr, char *buf)
{
	SENSOR_PRINT("Front_Cam_Camera_Info\n");
	return sprintf(buf, FRONT_CAMERA_INFO);
}
#endif

/* Device Attribute List */
static DEVICE_ATTR(rear_camfw, S_IRUGO | S_IXOTH, Rear_Cam_Sensor_ID, NULL); // Read(User, Group, Other), Execute(Other)
#if defined(REAR_FULL_SENSOR_NAME)
static DEVICE_ATTR(rear_camfw_full, S_IRUGO | S_IXOTH, Rear_Cam_Full_Sensor_ID, NULL); // Read(User, Group, Other), Execute(Other)
#endif

static DEVICE_ATTR(rear_checkfw_factory, S_IRUGO | S_IXOTH, Rear_camera_checkfw_factory, NULL);
static DEVICE_ATTR(rear_checkfw_user, S_IRUGO | S_IXOTH, Rear_camera_checkfw_user, NULL);

static DEVICE_ATTR(rear_type, S_IRUGO | S_IXOTH, Rear_Cam_Sensor_TYPE, NULL); // Read(User, Group, Other), Execute(Other)
static DEVICE_ATTR(rear_camtype, S_IRUGO | S_IXOTH, Rear_Cam_Sensor_TYPE, NULL); // Read(User, Group, Other), Execute(Other)
static DEVICE_ATTR(rear_caminfo, S_IRUGO, Rear_Cam_Camera_Info, NULL); // Read(User, Group, Other), Write(User, Group)
static DEVICE_ATTR(rear_flash, S_IRUGO | S_IWUSR | S_IWGRP | S_IXOTH, Rear_Cam_show_flash, Rear_Cam_store_flash); // Read(User, Group, Other), Write(User, Group), Execute(Other)

#if defined(FRONT_SENSOR_NAME)
static DEVICE_ATTR(front_camfw, S_IRUGO | S_IXOTH, Front_Cam_Sensor_ID, NULL); // Read(User, Group, Other), Execute(Other)
#if defined(FRONT_FULL_SENSOR_NAME)
static DEVICE_ATTR(front_camfw_full, S_IRUGO | S_IXOTH, Front_Cam_Full_Sensor_ID, NULL); // Read(User, Group, Other), Execute(Other)
#endif
static DEVICE_ATTR(front_type, S_IRUGO | S_IXOTH, Front_Cam_Sensor_TYPE, NULL); // Read(User, Group, Other), Execute(Other)
static DEVICE_ATTR(front_camtype, S_IRUGO | S_IXOTH, Front_Cam_Sensor_TYPE, NULL); // Read(User, Group, Other), Execute(Other)

static DEVICE_ATTR(front_caminfo, S_IRUGO, Front_Cam_Camera_Info, NULL); // Read(User, Group, Other), Write(User, Group)
#endif

#if defined(CONFIG_MACH_J1X3G)||defined(CONFIG_MACH_GTEXSWIFI)
static struct device_attribute camera_vendorid_attr = {
	.attr = {	
		.name = "rear_vendorid",
		.mode = (S_IRWXU | S_IRWXG | S_IRWXO)},	
	.show = camera_vendorid_show,	
	.store = camera_vendorid_store
};
#endif

int __init sensor_k_init(void)
{
	int err = 0;
	struct device *dev_t_rear = NULL;
	struct device *dev_t_flash = NULL;
	struct device *dev_t_front = NULL;

	printk(KERN_INFO "sensor_k_init called !\n");

	if (platform_driver_register(&sensor_dev_driver) != 0) {
		printk("platform device register Failed \n");
		return SENSOR_K_FAIL;
	}

	flash_torch_status = 0;

	camera_class = class_create(THIS_MODULE, "camera");
	if(IS_ERR(camera_class)) {
		SENSOR_PRINT("Failed to create camera_class!\n");
		platform_driver_unregister(&sensor_dev_driver);
		return PTR_ERR( camera_class );
	}

	dev_t_rear = device_create(camera_class, NULL, 0, "%s", "rear");
	if (IS_ERR(dev_t_rear)){
		platform_driver_unregister(&sensor_dev_driver);
		class_destroy(camera_class);
		SENSOR_PRINT("Failed to create camera_dev!\n");
		return PTR_ERR( dev_t_rear );
	}

	err = device_create_file(dev_t_rear, &dev_attr_rear_camfw);
	if(err) {
		SENSOR_PRINT("Failed to create device file(%s)!\n", dev_attr_rear_camfw.attr.name);
		goto err_make_rear_camfw_file;
	}

#if defined(REAR_FULL_SENSOR_NAME)
	err = device_create_file(dev_t_rear, &dev_attr_rear_camfw_full);
	if(err) {
		SENSOR_PRINT("Failed to create device file(%s)!\n", dev_attr_rear_camfw_full.attr.name);
		goto err_make_rear_camfw_full_file;
	}
#endif

	err = device_create_file(dev_t_rear, &dev_attr_rear_checkfw_factory);
	if(err)
	{
		SENSOR_PRINT("Failed to create device file(%s)!\n", dev_attr_rear_checkfw_factory.attr.name);
		goto err_make_rear_checkfw_factory_file;
	}

	err = device_create_file(dev_t_rear, &dev_attr_rear_checkfw_user);
	if(err)
	{
		SENSOR_PRINT("Failed to create device file(%s)!\n", dev_attr_rear_checkfw_user.attr.name);
		goto err_make_rear_checkfw_user_file;
	}

	err = device_create_file(dev_t_rear, &dev_attr_rear_type);
	if(err) {
		SENSOR_PRINT("Failed to create device file(%s)!\n", dev_attr_rear_type.attr.name);
		goto err_make_rear_type_file;
	}

	err = device_create_file(dev_t_rear, &dev_attr_rear_camtype);
	if(err) {
		SENSOR_PRINT("Failed to create device file(%s)!\n", dev_attr_rear_camtype.attr.name);
		goto err_make_rear_camtype_file;
	}

	err = device_create_file(dev_t_rear, &dev_attr_rear_caminfo);
	if(err) {
		SENSOR_PRINT("Failed to create device file(%s)!\n", dev_attr_rear_caminfo.attr.name);
		goto err_make_rear_camera_info;
	}
	
#if defined(CONFIG_MACH_J1X3G)||defined(CONFIG_MACH_GTEXSWIFI)	
	err = device_create_file(dev_t_rear, &camera_vendorid_attr);
	if(err) {		
		SENSOR_PRINT("Failed to create device file(%s)!\n", camera_vendorid_attr.attr.name);
		goto err_make_camera_vendorid;	
	}
#endif
	
#if defined(CONFIG_MACH_J1X3G) || defined(CONFIG_MACH_J3X3G) 
	/* Prevent CID 63391 : Fix explicit null dereferenced issue (FORWARD_NULL) */
	dev_t_flash = device_create(camera_class, NULL, 0, "%s", "flash");
	if (IS_ERR(dev_t_flash)) {
		SENSOR_PRINT("Failed to create camera_dev flash!\n");
		err = PTR_ERR( dev_t_flash );
		goto err_make_flash_device;
	}

	err = device_create_file(dev_t_flash, &dev_attr_rear_flash);
	if(err) {
		SENSOR_PRINT("Failed to create device file(%s)!\n", dev_attr_rear_flash.attr.name);
		goto err_make_flash_file;
	}
#endif
		
#if defined(FRONT_SENSOR_NAME)
	dev_t_front = device_create(camera_class, NULL, 0, "%s", "front");
	if (IS_ERR(dev_t_front)) {
		SENSOR_PRINT("Failed to create camera_dev front!\n");
		err = PTR_ERR( dev_t_front );
		goto err_make_front_device;
	}

	err = device_create_file(dev_t_front, &dev_attr_front_camfw);
	if(err) {
		SENSOR_PRINT("Failed to create device file(%s)!\n", dev_attr_front_camfw.attr.name);
		goto err_make_front_camfw_file;
	}

#if defined(FRONT_FULL_SENSOR_NAME)
	err = device_create_file(dev_t_front, &dev_attr_front_camfw_full);
	if(err) {
		SENSOR_PRINT("Failed to create device file(%s)!\n", dev_attr_front_camfw_full.attr.name);
		goto err_make_front_camfw_full_file;
	}
#endif

	err = device_create_file(dev_t_front, &dev_attr_front_type);
	if(err) {
		SENSOR_PRINT("Failed to create device file(%s)!\n", dev_attr_front_type.attr.name);
		goto err_make_front_type_file;
	}

	err = device_create_file(dev_t_front, &dev_attr_front_camtype);
	if(err) {
		SENSOR_PRINT("Failed to create device file(%s)!\n", dev_attr_front_camtype.attr.name);
		goto err_make_front_camtype_file;
	}

	err = device_create_file(dev_t_front, &dev_attr_front_caminfo);
	if(err) {
		SENSOR_PRINT("Failed to create device file(%s)!\n", dev_attr_front_caminfo.attr.name);
		goto err_make_front_camera_info;
	}
#endif

	return 0;

#if defined(FRONT_SENSOR_NAME)
	device_remove_file(dev_t_front, &dev_attr_front_caminfo);
err_make_front_camera_info:
	device_remove_file(dev_t_front, &dev_attr_front_camtype);
err_make_front_camtype_file:
	device_remove_file(dev_t_front, &dev_attr_front_type);
err_make_front_type_file:

#if defined(FRONT_FULL_SENSOR_NAME)
	device_remove_file(dev_t_front, &dev_attr_front_camfw_full);
err_make_front_camfw_full_file:
#endif
	device_remove_file(dev_t_front, &dev_attr_front_camfw);
err_make_front_camfw_file:
	device_destroy(camera_class, dev_t_front);
err_make_front_device:
#endif

#if defined(CONFIG_MACH_J1X3G) || defined(CONFIG_MACH_J3X3G)
	device_remove_file(dev_t_flash, &dev_attr_rear_flash);
err_make_flash_file :
	device_destroy(camera_class, dev_t_flash);
err_make_flash_device :
#endif

#if defined(CONFIG_MACH_J1X3G)||defined(CONFIG_MACH_GTEXSWIFI)	
	device_remove_file(dev_t_rear, &camera_vendorid_attr);	
err_make_camera_vendorid:		
#endif
	device_remove_file(dev_t_rear, &dev_attr_rear_caminfo);
err_make_rear_camera_info:
	device_remove_file(dev_t_rear, &dev_attr_rear_camtype);
err_make_rear_camtype_file:
	device_remove_file(dev_t_rear, &dev_attr_rear_type);
err_make_rear_type_file:
	device_remove_file(dev_t_rear, &dev_attr_rear_checkfw_user);
err_make_rear_checkfw_user_file:
	device_remove_file(dev_t_rear, &dev_attr_rear_checkfw_factory);
err_make_rear_checkfw_factory_file:

#if defined(REAR_FULL_SENSOR_NAME)
	device_remove_file(dev_t_rear, &dev_attr_rear_camfw_full);
err_make_rear_camfw_full_file:
#endif

	device_remove_file(dev_t_rear, &dev_attr_rear_camfw);

err_make_rear_camfw_file:
	device_destroy(camera_class,dev_t_rear);
	class_destroy(camera_class);
	platform_driver_unregister(&sensor_dev_driver);

	return err;
}

void sensor_k_exit(void)
{
	printk(KERN_INFO "sensor_k_exit called !\n");
	platform_driver_unregister(&sensor_dev_driver);
}

module_init(sensor_k_init);
module_exit(sensor_k_exit);

MODULE_DESCRIPTION("Sensor Driver");
MODULE_LICENSE("GPL");

