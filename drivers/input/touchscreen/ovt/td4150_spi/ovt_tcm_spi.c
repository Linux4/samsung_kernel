/*
 * Synaptics TCM touchscreen driver
 *
 * Copyright (C) 2017-2018 Synaptics Incorporated. All rights reserved.
 *
 * Copyright (C) 2017-2018 Scott Lin <scott.lin@tw.synaptics.com>
 * Copyright (C) 2018-2019 Ian Su <ian.su@tw.synaptics.com>
 * Copyright (C) 2018-2019 Joey Zhou <joey.zhou@synaptics.com>
 * Copyright (C) 2018-2019 Yuehao Qiu <yuehao.qiu@synaptics.com>
 * Copyright (C) 2018-2019 Aaron Chen <aaron.chen@tw.synaptics.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * INFORMATION CONTAINED IN THIS DOCUMENT IS PROVIDED "AS-IS," AND SYNAPTICS
 * EXPRESSLY DISCLAIMS ALL EXPRESS AND IMPLIED WARRANTIES, INCLUDING ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE,
 * AND ANY WARRANTIES OF NON-INFRINGEMENT OF ANY INTELLECTUAL PROPERTY RIGHTS.
 * IN NO EVENT SHALL SYNAPTICS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, PUNITIVE, OR CONSEQUENTIAL DAMAGES ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OF THE INFORMATION CONTAINED IN THIS DOCUMENT, HOWEVER CAUSED
 * AND BASED ON ANY THEORY OF LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, AND EVEN IF SYNAPTICS WAS ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE. IF A TRIBUNAL OF COMPETENT JURISDICTION DOES
 * NOT PERMIT THE DISCLAIMER OF DIRECT DAMAGES OR ANY OTHER DAMAGES, SYNAPTICS'
 * TOTAL CUMULATIVE LIABILITY TO ANY PARTY SHALL NOT EXCEED ONE HUNDRED U.S.
 * DOLLARS.
 */

#include <linux/spi/spi.h>
#include <linux/of_gpio.h>
#include "ovt_tcm_core.h"

static unsigned char *buf;

static unsigned int buf_size;

static struct spi_transfer *xfer;

static struct ovt_tcm_bus_io bus_io;

static struct ovt_tcm_hw_interface hw_if;

static struct platform_device *ovt_tcm_spi_device;

#if IS_ENABLED(CONFIG_SPI_MT65XX)
const struct mtk_chip_config synap_spi_ctrdata = {
	.rx_mlsb = 1,
	.tx_mlsb = 1,
	.sample_sel = 0,

	.cs_setuptime = 518,
	.cs_holdtime = 0,
	.cs_idletime = 0,
	.deassert_mode = false,
	.tick_delay = 0,
};
#endif

#ifdef CONFIG_OF
static int parse_dt(struct device *dev, struct ovt_tcm_board_data *bdata)
{
	int retval;
	u32 value;
	struct property *prop;
	struct device_node *np = dev->of_node;
	const char *name;
	u32 px_zone[3] = { 0 };
	int lcd_id1_gpio = 0, lcd_id2_gpio = 0, lcd_id3_gpio = 0, dt_lcdtype;
	int fw_name_cnt;
	int lcdtype_cnt;
	int fw_sel_idx = 0;
#if IS_ENABLED(CONFIG_EXYNOS_DPU30)
	int lcdtype = 0;
	int connected;

	connected = get_lcd_info("connected");
	if (connected < 0) {
		input_err(true, dev, "%s: Failed to get lcd info\n", __func__);
		return -EINVAL;
	}

	if (!connected) {
		input_err(true, dev, "%s: lcd is disconnected\n", __func__);
		return -ENODEV;
	}

	input_info(true, dev, "%s: lcd is connected\n", __func__);

	lcdtype = get_lcd_info("id");
	if (lcdtype < 0) {
		input_err(true, dev, "%s: Failed to get lcd info\n", __func__);
		return -EINVAL;
	}
#endif

	fw_name_cnt = of_property_count_strings(np, "ovt,fw_name");

	if (fw_name_cnt == 0) {
		input_err(true, dev, "%s: no fw_name in DT\n", __func__);
		return -EINVAL;

	} else if (fw_name_cnt == 1) {
		retval = of_property_read_u32(np, "ovt,lcdtype", &dt_lcdtype);
		if (retval < 0) {
			input_err(true, dev, "%s: failed to read ovt,lcdtype\n", __func__);

		} else {
			input_info(true, dev, "%s: fw_name_cnt(1), ap lcdtype=0x%06X & dt lcdtype=0x%06X\n",
								__func__, lcdtype, dt_lcdtype);
			if (lcdtype != dt_lcdtype) {
				input_err(true, dev, "%s: panel mismatched, unload driver\n", __func__);
				return -EINVAL;
			}
		}
	} else {

		lcd_id1_gpio = of_get_named_gpio(np, "ovt,lcdid1-gpio", 0);
		if (gpio_is_valid(lcd_id1_gpio))
			input_info(true, dev, "%s: lcd id1_gpio %d(%d)\n", __func__, lcd_id1_gpio, gpio_get_value(lcd_id1_gpio));
		else {
			input_err(true, dev, "%s: Failed to get ovt,lcdid1-gpio\n", __func__);
			return -EINVAL;
		}

		lcd_id2_gpio = of_get_named_gpio(np, "ovt,lcdid2-gpio", 0);
		if (gpio_is_valid(lcd_id2_gpio))
			input_info(true, dev, "%s: lcd id2_gpio %d(%d)\n", __func__, lcd_id2_gpio, gpio_get_value(lcd_id2_gpio));
		else {
			input_err(true, dev, "%s: Failed to get ovt,lcdid2-gpio\n", __func__);
			return -EINVAL;
		}

		/* support lcd id3 */
		lcd_id3_gpio = of_get_named_gpio(np, "ovt,lcdid3-gpio", 0);
		if (gpio_is_valid(lcd_id3_gpio)) {
			input_info(true, dev, "%s: lcd id3_gpio %d(%d)\n", __func__, lcd_id3_gpio, gpio_get_value(lcd_id3_gpio));
			fw_sel_idx = (gpio_get_value(lcd_id3_gpio) << 2) | (gpio_get_value(lcd_id2_gpio) << 1) | gpio_get_value(lcd_id1_gpio);

		} else {
			input_err(true, dev, "%s: Failed to get ovt,lcdid3-gpio and use #1 &#2 id\n", __func__);
			fw_sel_idx = (gpio_get_value(lcd_id2_gpio) << 1) | gpio_get_value(lcd_id1_gpio);
		}

		lcdtype_cnt = of_property_count_u32_elems(np, "ovt,lcdtype");
		input_info(true, dev, "%s: fw_name_cnt(%d) & lcdtype_cnt(%d) & fw_sel_idx(%d)\n",
					__func__, fw_name_cnt, lcdtype_cnt, fw_sel_idx);

		if (lcdtype_cnt <= 0 || fw_name_cnt <= 0 || lcdtype_cnt <= fw_sel_idx || fw_name_cnt <= fw_sel_idx) {
			input_err(true, dev, "%s: abnormal lcdtype & fw name count, fw_sel_idx(%d)\n", __func__, fw_sel_idx);
			return -EINVAL;
		}
		of_property_read_u32_index(np, "ovt,lcdtype", fw_sel_idx, &dt_lcdtype);
		input_info(true, dev, "%s: lcd id(%d), ap lcdtype=0x%06X & dt lcdtype=0x%06X\n",
						__func__, fw_sel_idx, lcdtype, dt_lcdtype);

	}

	of_property_read_string_index(np, "ovt,fw_name", fw_sel_idx, &bdata->fw_name);
	if (bdata->fw_name == NULL || strlen(bdata->fw_name) == 0) {
		input_err(true, dev, "%s: Failed to get fw name\n", __func__);
		return -EINVAL;
	} else {
		input_info(true, dev, "%s: fw name(%s)\n", __func__, bdata->fw_name);
	}

	prop = of_find_property(np, "ovt,irq-gpio", NULL);
	if (prop && prop->length) {
		bdata->irq_gpio = of_get_named_gpio_flags(np, "ovt,irq-gpio", 0,
				(enum of_gpio_flags *)&bdata->irq_flags);
	} else {
		bdata->irq_gpio = -1;
	}

	retval = of_property_read_u32(np, "ovt,irq-on-state", &value);
	if (retval < 0)
		bdata->irq_on_state = 0;
	else
		bdata->irq_on_state = value;

	
	prop = of_find_property(np, "ovt,cs-gpio", NULL);
	if (prop && prop->length) {
		bdata->cs_gpio = of_get_named_gpio_flags(np, "ovt,cs-gpio", 0,
				(enum of_gpio_flags *)&bdata->cs_gpio);
	} else {
		bdata->cs_gpio = -1;
	}

	retval = of_property_read_string(np, "ovt,pwr-reg-name", &name);
	if (retval < 0)
		bdata->pwr_reg_name = NULL;
	else
		bdata->pwr_reg_name = name;

	retval = of_property_read_string(np, "ovt,bus-reg-name", &name);
	if (retval < 0)
		bdata->bus_reg_name = NULL;
	else
		bdata->bus_reg_name = name;

	prop = of_find_property(np, "ovt,power-gpio", NULL);
	if (prop && prop->length) {
		bdata->power_gpio = of_get_named_gpio_flags(np, "ovt,power-gpio", 0, NULL);
	} else {
		bdata->power_gpio = -1;
	}

	prop = of_find_property(np, "ovt,power-on-state", NULL);
	if (prop && prop->length) {
		retval = of_property_read_u32(np, "ovt,power-on-state", &value);
		if (retval < 0) {
			input_err(true, dev,
					"Failed to read ovt,power-on-state property\n");
			return retval;
		} else {
			bdata->power_on_state = value;
		}
	} else {
		bdata->power_on_state = 0;
	}

	prop = of_find_property(np, "ovt,power-delay-ms", NULL);
	if (prop && prop->length) {
		retval = of_property_read_u32(np, "ovt,power-delay-ms", &value);
		if (retval < 0) {
			input_err(true, dev,
					"Failed to read ovt,power-delay-ms property\n");
			return retval;
		} else {
			bdata->power_delay_ms = value;
		}
	} else {
		bdata->power_delay_ms = 0;
	}

	prop = of_find_property(np, "ovt,reset-gpio", NULL);
	if (prop && prop->length) {
		bdata->reset_gpio = of_get_named_gpio_flags(np,
				"ovt,reset-gpio", 0, NULL);
	} else {
		bdata->reset_gpio = -1;
	}

	prop = of_find_property(np, "ovt,reset-on-state", NULL);
	if (prop && prop->length) {
		retval = of_property_read_u32(np, "ovt,reset-on-state",
				&value);
		if (retval < 0) {
			input_err(true, dev,
					"Failed to read ovt,reset-on-state property\n");
			return retval;
		} else {
			bdata->reset_on_state = value;
		}
	} else {
		bdata->reset_on_state = 0;
	}

	prop = of_find_property(np, "ovt,reset-active-ms", NULL);
	if (prop && prop->length) {
		retval = of_property_read_u32(np, "ovt,reset-active-ms",
				&value);
		if (retval < 0) {
			input_err(true, dev,
					"Failed to read ovt,reset-active-ms property\n");
			return retval;
		} else {
			bdata->reset_active_ms = value;
		}
	} else {
		bdata->reset_active_ms = 0;
	}

	prop = of_find_property(np, "ovt,reset-delay-ms", NULL);
	if (prop && prop->length) {
		retval = of_property_read_u32(np, "ovt,reset-delay-ms",
				&value);
		if (retval < 0) {
			input_err(true, dev,
					"Unable to read ovt,reset-delay-ms property\n");
			return retval;
		} else {
			bdata->reset_delay_ms = value;
		}
	} else {
		bdata->reset_delay_ms = 0;
	}

	prop = of_find_property(np, "ovt,tpio-reset-gpio", NULL);
	if (prop && prop->length) {
		bdata->tpio_reset_gpio = of_get_named_gpio_flags(np,
				"ovt,tpio-reset-gpio", 0, NULL);
	} else {
		bdata->tpio_reset_gpio = -1;
	}

	prop = of_find_property(np, "ovt,x-flip", NULL);
	bdata->x_flip = prop > 0 ? true : false;

	prop = of_find_property(np, "ovt,y-flip", NULL);
	bdata->y_flip = prop > 0 ? true : false;

	prop = of_find_property(np, "ovt,swap-axes", NULL);
	bdata->swap_axes = prop > 0 ? true : false;

	prop = of_find_property(np, "ovt,byte-delay-us", NULL);
	if (prop && prop->length) {
		retval = of_property_read_u32(np, "ovt,byte-delay-us", &value);
		if (retval < 0) {
			input_err(true, dev,
					"Unable to read ovt,byte-delay-us property\n");
			return retval;
		} else {
			bdata->byte_delay_us = value;
		}
	} else {
		bdata->byte_delay_us = 0;
	}

	prop = of_find_property(np, "ovt,block-delay-us", NULL);
	if (prop && prop->length) {
		retval = of_property_read_u32(np, "ovt,block-delay-us", &value);
		if (retval < 0) {
			input_err(true, dev,
					"Unable to read ovt,block-delay-us property\n");
			return retval;
		} else {
			bdata->block_delay_us = value;
		}
	} else {
		bdata->block_delay_us = 0;
	}

	prop = of_find_property(np, "ovt,spi-mode", NULL);
	if (prop && prop->length) {
		retval = of_property_read_u32(np, "ovt,spi-mode",
				&value);
		if (retval < 0) {
			input_err(true, dev,
					"Unable to read ovt,spi-mode property\n");
			return retval;
		} else {
			bdata->spi_mode = value;
		}
	} else {
		bdata->spi_mode = 0;
	}

	prop = of_find_property(np, "ovt,ubl-max-freq", NULL);
	if (prop && prop->length) {
		retval = of_property_read_u32(np, "ovt,ubl-max-freq",
				&value);
		if (retval < 0) {
			input_err(true, dev,
					"Unable to read ovt,ubl-max-freq property\n");
			return retval;
		} else {
			bdata->ubl_max_freq = value;
		}
	} else {
		bdata->ubl_max_freq = 0;
	}

	prop = of_find_property(np, "ovt,ubl-byte-delay-us", NULL);
	if (prop && prop->length) {
		retval = of_property_read_u32(np, "ovt,ubl-byte-delay-us",
				&value);
		if (retval < 0) {
			input_err(true, dev,
					"Unable to read ovt,ubl-byte-delay-us property\n");
			return retval;
		} else {
			bdata->ubl_byte_delay_us = value;
		}
	} else {
		bdata->ubl_byte_delay_us = 0;
	}

	if (of_property_read_string(np, "ovt,regulator_lcd_vdd", &bdata->regulator_lcd_vdd))
		input_err(true, dev, "%s: Failed to get regulator_dvdd name property\n", __func__);

	if (of_property_read_string(np, "ovt,regulator_lcd_reset", &bdata->regulator_lcd_reset))
		input_err(true, dev, "%s: Failed to get regulator_dvdd name property\n", __func__);

	if (of_property_read_string(np, "ovt,regulator_lcd_bl", &bdata->regulator_lcd_bl))
		input_err(true, dev, "%s: Failed to get regulator_dvdd name property\n", __func__);

	if (of_property_read_string(np, "ovt,regulator_lcd_vsp", &bdata->regulator_lcd_vsp))
		input_err(true, dev, "%s: Failed to get regulator_lcd_vsp name property\n", __func__);

	if (of_property_read_string(np, "ovt,regulator_lcd_vsn", &bdata->regulator_lcd_vsn))
		input_err(true, dev, "%s: Failed to get regulator_lcd_vsn name property\n", __func__);

	bdata->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(bdata->pinctrl))
		input_err(true, dev, "%s: could not get pinctrl\n", __func__);

	if (of_property_read_u32_array(np, "ovt,area-size", px_zone, 3)) {
		input_info(true, dev, "Failed to get zone's size\n");
		bdata->area_indicator = 48;
		bdata->area_navigation = 96;
		bdata->area_edge = 60;
	} else {
		bdata->area_indicator = px_zone[0];
		bdata->area_navigation = px_zone[1];
		bdata->area_edge = px_zone[2];
	}
	input_info(true, dev, "%s : zone's size - indicator:%d, navigation:%d, edge:%d\n",
		__func__, bdata->area_indicator, bdata->area_navigation ,bdata->area_edge);

	bdata->enable_settings_aot = of_property_read_bool(np, "ovt,enable_settings_aot");
	bdata->support_ear_detect = of_property_read_bool(np, "ovt,support_ear_detect_mode");
	bdata->prox_lp_scan_enabled = of_property_read_bool(np, "ovt,prox_lp_scan_enabled");
	bdata->enable_sysinput_enabled = of_property_read_bool(np, "ovt,enable_sysinput_enabled");
	input_info(true, dev, "%s: AOT:%d, ED:%d, LPSCAN:%d, SE:%d\n",
		__func__, bdata->enable_settings_aot, bdata->support_ear_detect, bdata->prox_lp_scan_enabled, bdata->enable_sysinput_enabled);

	return 0;
}
#endif

static int ovt_tcm_spi_alloc_mem(struct ovt_tcm_hcd *tcm_hcd,
		unsigned int count, unsigned int size)
{
	static unsigned int xfer_count;
	struct spi_device *spi = to_spi_device(tcm_hcd->pdev->dev.parent);

	if (count > xfer_count) {
		kfree(xfer);
		xfer = kcalloc(count, sizeof(*xfer), GFP_KERNEL);
		if (!xfer) {
			input_err(true, &spi->dev, "Failed to allocate memory for xfer\n");
			xfer_count = 0;
			return -ENOMEM;
		}
		xfer_count = count;
	} else {
		memset(xfer, 0, count * sizeof(*xfer));
	}

	if (size > buf_size) {
		if (buf_size)
			kfree(buf);
		buf = kmalloc(size, GFP_KERNEL);
		if (!buf) {
			input_err(true, &spi->dev, "Failed to allocate memory for buf\n");
			buf_size = 0;
			return -ENOMEM;
		}
		buf_size = size;
	}

	return 0;
}

static int ovt_tcm_spi_rmi_read(struct ovt_tcm_hcd *tcm_hcd,
		unsigned short addr, unsigned char *data, unsigned int length)
{
	int retval;
	unsigned int idx;
	unsigned int mode;
	unsigned int byte_count;
	struct spi_message msg;
	struct spi_device *spi = to_spi_device(tcm_hcd->pdev->dev.parent);
	const struct ovt_tcm_board_data *bdata = tcm_hcd->hw_if->bdata;

	if (tcm_hcd->lp_state == PWR_OFF) {
		input_err(true, tcm_hcd->pdev->dev.parent, "power off in suspend\n");
		return -EIO;
	}

	mutex_lock(&tcm_hcd->io_ctrl_mutex);

	spi_message_init(&msg);

	byte_count = length + 2;

	if (bdata->ubl_byte_delay_us == 0)
		retval = ovt_tcm_spi_alloc_mem(tcm_hcd, 2, byte_count);
	else
		retval = ovt_tcm_spi_alloc_mem(tcm_hcd, byte_count, 3);
	if (retval < 0) {
		input_err(true, &spi->dev, "Failed to allocate memory\n");
		goto exit;
	}

	buf[0] = (unsigned char)(addr >> 8) | 0x80;
	buf[1] = (unsigned char)addr;

	if (bdata->ubl_byte_delay_us == 0) {
		xfer[0].len = 2;
		xfer[0].tx_buf = buf;
		xfer[0].speed_hz = bdata->ubl_max_freq;
		spi_message_add_tail(&xfer[0], &msg);
		memset(&buf[2], 0xff, length);
		xfer[1].len = length;
		xfer[1].tx_buf = &buf[2];
		xfer[1].rx_buf = data;
		if (bdata->block_delay_us)
			xfer[1].delay_usecs = bdata->block_delay_us;
		xfer[1].speed_hz = bdata->ubl_max_freq;
		spi_message_add_tail(&xfer[1], &msg);
	} else {
		buf[2] = 0xff;
		for (idx = 0; idx < byte_count; idx++) {
			xfer[idx].len = 1;
			if (idx < 2) {
				xfer[idx].tx_buf = &buf[idx];
			} else {
				xfer[idx].tx_buf = &buf[2];
				xfer[idx].rx_buf = &data[idx - 2];
			}
			xfer[idx].delay_usecs = bdata->ubl_byte_delay_us;
			if (bdata->block_delay_us && (idx == byte_count - 1))
				xfer[idx].delay_usecs = bdata->block_delay_us;
			xfer[idx].speed_hz = bdata->ubl_max_freq;
			spi_message_add_tail(&xfer[idx], &msg);
		}
	}

	mode = spi->mode;
	spi->mode = SPI_MODE_3;

	if (bdata->cs_gpio >= 0)
		gpio_set_value(bdata->cs_gpio, 0);

	retval = spi_sync(spi, &msg);
	if (retval == 0) {
		retval = length;
	} else {
		input_err(true, &spi->dev,
				"Failed to complete SPI transfer, error = %d\n", retval);
	}

	if (bdata->cs_gpio >= 0)
		gpio_set_value(bdata->cs_gpio, 1);

	spi->mode = mode;

exit:
	mutex_unlock(&tcm_hcd->io_ctrl_mutex);

	return retval;
}

static int ovt_tcm_spi_rmi_write(struct ovt_tcm_hcd *tcm_hcd,
		unsigned short addr, unsigned char *data, unsigned int length)
{
	int retval;
	unsigned int mode;
	unsigned int byte_count;
	struct spi_message msg;
	struct spi_device *spi = to_spi_device(tcm_hcd->pdev->dev.parent);
	const struct ovt_tcm_board_data *bdata = tcm_hcd->hw_if->bdata;

	if (tcm_hcd->lp_state == PWR_OFF) {
		input_err(true, tcm_hcd->pdev->dev.parent, "power off in suspend\n");
		return -EIO;
	}

	mutex_lock(&tcm_hcd->io_ctrl_mutex);

	spi_message_init(&msg);

	byte_count = length + 2;

	retval = ovt_tcm_spi_alloc_mem(tcm_hcd, 1, byte_count);
	if (retval < 0) {
		input_err(true, &spi->dev, "Failed to allocate memory\n");
		goto exit;
	}

	buf[0] = (unsigned char)(addr >> 8) & ~0x80;
	buf[1] = (unsigned char)addr;
	retval = secure_memcpy(&buf[2], buf_size - 2, data, length, length);
	if (retval < 0) {
		input_err(true, &spi->dev, "Failed to copy write data\n");
		goto exit;
	}

	xfer[0].len = byte_count;
	xfer[0].tx_buf = buf;
	if (bdata->block_delay_us)
		xfer[0].delay_usecs = bdata->block_delay_us;
	spi_message_add_tail(&xfer[0], &msg);

	mode = spi->mode;
	spi->mode = SPI_MODE_3;

	if (bdata->cs_gpio >= 0)
		gpio_set_value(bdata->cs_gpio, 0);

	retval = spi_sync(spi, &msg);
	if (retval == 0) {
		retval = length;
	} else {
		input_err(true, &spi->dev,
				"Failed to complete SPI transfer, error = %d\n", retval);
	}
	if (bdata->cs_gpio >= 0)
		gpio_set_value(bdata->cs_gpio, 1);

	spi->mode = mode;

exit:
	mutex_unlock(&tcm_hcd->io_ctrl_mutex);

	return retval;
}

static int ovt_tcm_spi_read(struct ovt_tcm_hcd *tcm_hcd, unsigned char *data,
		unsigned int length)
{
	int retval;
	unsigned int idx;
	struct spi_message msg;
	struct spi_device *spi = to_spi_device(tcm_hcd->pdev->dev.parent);
	const struct ovt_tcm_board_data *bdata = tcm_hcd->hw_if->bdata;

	if (tcm_hcd->lp_state == PWR_OFF) {
		input_err(true, tcm_hcd->pdev->dev.parent, "power off in suspend\n");
		return -EIO;
	}

	mutex_lock(&tcm_hcd->io_ctrl_mutex);

	spi_message_init(&msg);

	if (bdata->byte_delay_us == 0)
		retval = ovt_tcm_spi_alloc_mem(tcm_hcd, 1, length);
	else
		retval = ovt_tcm_spi_alloc_mem(tcm_hcd, length, 1);
	if (retval < 0) {
		input_err(true, tcm_hcd->pdev->dev.parent, "Failed to allocate memory\n");
		goto exit;
	}

	if (bdata->byte_delay_us == 0) {
		memset(buf, 0xff, length);
		xfer[0].len = length;
		xfer[0].tx_buf = buf;
		xfer[0].rx_buf = data;
		if (bdata->block_delay_us)
			xfer[0].delay_usecs = bdata->block_delay_us;
		spi_message_add_tail(&xfer[0], &msg);
	} else {
		buf[0] = 0xff;
		for (idx = 0; idx < length; idx++) {
			xfer[idx].len = 1;
			xfer[idx].tx_buf = buf;
			xfer[idx].rx_buf = &data[idx];
			xfer[idx].delay_usecs = bdata->byte_delay_us;
			if (bdata->block_delay_us && (idx == length - 1))
				xfer[idx].delay_usecs = bdata->block_delay_us;
			spi_message_add_tail(&xfer[idx], &msg);
		}
	}

	if (bdata->cs_gpio >= 0)
		gpio_set_value(bdata->cs_gpio, 0);

	retval = spi_sync(spi, &msg);
	if (retval == 0) {
		retval = length;
	} else {
		input_err(true, &spi->dev,
				"Failed to complete SPI transfer, error = %d\n", retval);
	}

	if (bdata->cs_gpio >= 0)
		gpio_set_value(bdata->cs_gpio, 1);
exit:
	mutex_unlock(&tcm_hcd->io_ctrl_mutex);

	return retval;
}

static int ovt_tcm_spi_write(struct ovt_tcm_hcd *tcm_hcd, unsigned char *data,
		unsigned int length)
{
	int retval;
	unsigned int idx;
	struct spi_message msg;
	struct spi_device *spi = to_spi_device(tcm_hcd->pdev->dev.parent);
	const struct ovt_tcm_board_data *bdata = tcm_hcd->hw_if->bdata;

	if (tcm_hcd->lp_state == PWR_OFF) {
		input_err(true, tcm_hcd->pdev->dev.parent, "power off in suspend\n");
		return -EIO;
	}

	mutex_lock(&tcm_hcd->io_ctrl_mutex);

	spi_message_init(&msg);

	if (bdata->byte_delay_us == 0)
		retval = ovt_tcm_spi_alloc_mem(tcm_hcd, 1, 0);
	else
		retval = ovt_tcm_spi_alloc_mem(tcm_hcd, length, 0);
	if (retval < 0) {
		input_err(true, &spi->dev, "Failed to allocate memory\n");
		goto exit;
	}

	if (bdata->byte_delay_us == 0) {
		xfer[0].len = length;
		xfer[0].tx_buf = data;
		if (bdata->block_delay_us)
			xfer[0].delay_usecs = bdata->block_delay_us;
		spi_message_add_tail(&xfer[0], &msg);
	} else {
		for (idx = 0; idx < length; idx++) {
			xfer[idx].len = 1;
			xfer[idx].tx_buf = &data[idx];
			xfer[idx].delay_usecs = bdata->byte_delay_us;
			if (bdata->block_delay_us && (idx == length - 1))
				xfer[idx].delay_usecs = bdata->block_delay_us;
			spi_message_add_tail(&xfer[idx], &msg);
		}
	}

	if (bdata->cs_gpio >= 0)
		gpio_set_value(bdata->cs_gpio, 0);

	retval = spi_sync(spi, &msg);
	if (retval == 0) {
		retval = length;
	} else {
		input_err(true, &spi->dev,
				"Failed to complete SPI transfer, error = %d\n", retval);
	}

	if (bdata->cs_gpio >= 0)
		gpio_set_value(bdata->cs_gpio, 1);

exit:
	mutex_unlock(&tcm_hcd->io_ctrl_mutex);

	return retval;
}

static int ovt_tcm_spi_probe(struct spi_device *spi)
{
	int retval;

#ifdef CONFIG_OF
	hw_if.bdata = devm_kzalloc(&spi->dev, sizeof(*hw_if.bdata), GFP_KERNEL);
	if (!hw_if.bdata) {
		input_err(true, &spi->dev, "Failed to allocate memory for board data\n");
		return -ENOMEM;
	}
	retval = parse_dt(&spi->dev, hw_if.bdata);
	if (retval < 0) {
		input_err(true, &spi->dev, "%s : parse_dt failed\n", __func__);
		return -EINVAL;
	}
#else
	hw_if.bdata = spi->dev.platform_data;
#endif

	if (spi->master->flags & SPI_MASTER_HALF_DUPLEX) {
		input_err(true, &spi->dev, "Full duplex not supported by host\n");
		return -EIO;
	}

	ovt_tcm_spi_device = platform_device_alloc(PLATFORM_DRIVER_NAME, 0);
	if (!ovt_tcm_spi_device) {
		input_err(true, &spi->dev, "Failed to allocate platform device\n");
		return -ENOMEM;
	}

	switch (hw_if.bdata->spi_mode) {
	case 0:
		spi->mode = SPI_MODE_0;
		break;
	case 1:
		spi->mode = SPI_MODE_1;
		break;
	case 2:
		spi->mode = SPI_MODE_2;
		break;
	case 3:
		spi->mode = SPI_MODE_3;
		break;
	}

	bus_io.type = BUS_SPI;
	bus_io.read = ovt_tcm_spi_read;
	bus_io.write = ovt_tcm_spi_write;
	bus_io.rmi_read = ovt_tcm_spi_rmi_read;
	bus_io.rmi_write = ovt_tcm_spi_rmi_write;

	hw_if.bus_io = &bus_io;

	spi->bits_per_word = 8;

	retval = spi_setup(spi);
	if (retval < 0) {
		input_err(true, &spi->dev, "Failed to set up SPI protocol driver\n");
		return retval;
	}

#if IS_ENABLED(CONFIG_SPI_MT65XX)
    /* new usage of MTK spi API */
    memcpy(&hw_if.bdata->spi_ctrl, &synap_spi_ctrdata, sizeof(struct mtk_chip_config));
    spi->controller_data = (void *)&hw_if.bdata->spi_ctrl;
#endif

	ovt_tcm_spi_device->dev.parent = &spi->dev;
	ovt_tcm_spi_device->dev.platform_data = &hw_if;

	retval = platform_device_add(ovt_tcm_spi_device);
	if (retval < 0) {
		input_err(true, &spi->dev, "Failed to add platform device\n");
		return retval;
	}

	return 0;
}

static int ovt_tcm_spi_remove(struct spi_device *spi)
{
	ovt_tcm_spi_device->dev.platform_data = NULL;

	platform_device_unregister(ovt_tcm_spi_device);

	return 0;
}

static const struct spi_device_id ovt_tcm_id_table[] = {
	{SPI_MODULE_NAME, 0},
	{},
};
MODULE_DEVICE_TABLE(spi, ovt_tcm_id_table);

#ifdef CONFIG_OF
static struct of_device_id ovt_tcm_of_match_table[] = {
	{
		.compatible = "ovt,tcm-spi",
	},
	{},
};
MODULE_DEVICE_TABLE(of, ovt_tcm_of_match_table);
#else
#define ovt_tcm_of_match_table NULL
#endif

static struct spi_driver ovt_tcm_spi_driver = {
	.driver = {
		.name = SPI_MODULE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = ovt_tcm_of_match_table,
	},
	.probe = ovt_tcm_spi_probe,
	.remove = ovt_tcm_spi_remove,
	.id_table = ovt_tcm_id_table,
};

int ovt_tcm_bus_init(void)
{
	return spi_register_driver(&ovt_tcm_spi_driver);
}
EXPORT_SYMBOL(ovt_tcm_bus_init);

void ovt_tcm_bus_exit(void)
{
	kfree(buf);

	kfree(xfer);

	spi_unregister_driver(&ovt_tcm_spi_driver);

	return;
}
EXPORT_SYMBOL(ovt_tcm_bus_exit);

MODULE_AUTHOR("Synaptics, Inc.");
MODULE_DESCRIPTION("Synaptics TCM SPI Bus Module");
MODULE_LICENSE("GPL v2");
