/*  Himax Android Driver Sample Code for MTK kernel 4.4 platform

    Copyright (C) 2018 Himax Corporation.

    This software is licensed under the terms of the GNU General Public
    License version 2, as published by the Free Software Foundation, and
    may be copied, distributed, and modified under those terms.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

*/
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>

#include "himax_platform.h"
#include "himax_common.h"

/* MTK header */
#ifndef CONFIG_SPI_MT65XX
#include "mtk_spi.h"
#include "mtk_spi_hal.h"
#endif
//#include "mtk_gpio.h"
//#include "mach/gpio_const.h"
/*#include "mt_spi.h"
#include "mt_spi_hal.h"
#include "mt_gpio.h"
#include "mach/gpio_const.h"*/

int i2c_error_count = 0;
int irq_enable_count = 0;
#ifdef CONFIG_SPI_MT65XX
	u32 hx_spi_speed = 1*1000000;
#endif

#define PACKET_MAX_SZ (HX_SPI_MTK_MAX_WRITE_SZ + 2)

DEFINE_MUTEX(hx_wr_access);

MODULE_DEVICE_TABLE(of, himax_match_table);
struct of_device_id himax_match_table[] = {
	{.compatible = "mediatek,himax_cap_touch" },
	{},
};

static int himax_tpd_int_gpio = 5;
unsigned int himax_touch_irq = 0;
unsigned int himax_tpd_rst_gpio_number = -1;
unsigned int himax_tpd_int_gpio_number = -1;

u8 *gpDMABuf_va = NULL;
u8 *gpDMABuf_pa = NULL;
static uint8_t *gBuffer;

/* Custom set some config */
static int hx_panel_coords[4] = {0, 719, 0, 1519}; /* [1]=X resolution, [3]=Y resolution */
static int hx_display_coords[4] = {0, 719, 0, 1519};
static int report_type = PROTOCOL_TYPE_B;

struct i2c_client *i2c_client_point = NULL;

extern struct himax_ic_data *ic_data;
extern struct himax_ts_data *private_ts;

extern int himax_chip_common_init(void);
extern void himax_chip_common_deinit(void);

extern void himax_ts_work(struct himax_ts_data *ts);
extern enum hrtimer_restart himax_ts_timer_func(struct hrtimer *timer);

/******* SPI-start *******/
struct mutex             	hx_spi_lock;

static struct spi_device	*hx_spi;
#ifndef CONFIG_SPI_MT65XX
static struct mt_chip_conf 	hx_spi_mcc;
#endif
#if 0//Himax Test
struct mtk_chip_config 		hx_spi_mcc;
#endif
static int 				hx_irq;
//static u8 			*hx_spi_buffer;  /* only used for SPI transfer internal */
/******* SPI-end *******/

#ifndef CONFIG_SPI_MT65XX
struct mt_chip_conf spi_ctrdata = {
	.setuptime = 10,
	.holdtime = 10,
	.high_time = 50, /* 1MHz */
	.low_time = 50,
	.cs_idletime = 10,
	.ulthgh_thrsh = 0,

	.cpol = SPI_CPOL_0,
	.cpha = SPI_CPHA_0,

	.rx_mlsb = SPI_MSB,
	.tx_mlsb = SPI_MSB,

	.tx_endian = SPI_LENDIAN,
	.rx_endian = SPI_LENDIAN,

	.com_mod = FIFO_TRANSFER,
	/* .com_mod = DMA_TRANSFER, */

	.pause = 0,
	.finish_intr = 1,
	.deassert = 0,
	.ulthigh = 0,
	.tckdly = 0,
};
#endif


#if defined(HX_PLATFOME_DEFINE_KEY)
/*In MT6797 need to set 1 into use-tpd-button in dts kernel-3.18\arch\arm64\boot\dts\amt6797_evb_m.dts*/
/*key_range : [keyindex][key_data] {..{x,y}..}*/
static int key_range[3][2] = {{90,883 }, { 230,883}, {370,883} };
#endif

int himax_dev_set(struct himax_ts_data *ts)
{
	//ts->input_dev = tpd->dev;
	ts->input_dev = input_allocate_device();
	if(!ts->input_dev)
		return MEM_ALLOC_FAIL;

        ts->input_dev->name = "hxmax-touchscreen";
	I("%s: input_allocate_device Success!\n", __func__) ;

	return NO_ERR;
}
int himax_input_register_device(struct input_dev *input_dev)
{
	return NO_ERR;
}

#if defined(HX_PLATFOME_DEFINE_KEY)
void himax_platform_key(void)
{
	int idx = 0;

	if (tpd_dts_data.use_tpd_button) {
		for (idx = 0; idx < tpd_dts_data.tpd_key_num; idx++) {
			input_set_capability(tpd->dev, EV_KEY, tpd_dts_data.tpd_key_local[idx]);
			I("[%d]key:%d\n", idx, tpd_dts_data.tpd_key_local[idx]);
		}
	}
}
/* report coordinates to system and system will transfer it into Key */
static void himax_vk_parser(struct himax_i2c_platform_data *pdata, int key_num)
{
	int i = 0;
	struct himax_virtual_key *vk;
	uint8_t key_index = 0;
	vk = kzalloc(key_num * (sizeof *vk), GFP_KERNEL);

	for (key_index = 0; key_index < key_num ; key_index++) {
		/* index: def in our driver */
		vk[key_index].index = key_index + 1;
		/* key size */
		vk[key_index].x_range_min = key_range[key_index][0], vk[key_index].x_range_max = key_range[key_index][0];
		vk[key_index].y_range_min = key_range[key_index][1], vk[key_index].y_range_max = key_range[key_index][1];
	}

	pdata->virtual_key = vk;

	for (i = 0 ; i < key_num; i++) {
		I(" vk[%d] idx:%d x_min:%d, y_max:%d\n", i, pdata->virtual_key[i].index, pdata->virtual_key[i].x_range_min, pdata->virtual_key[i].y_range_max);
	}
}
#else
void himax_vk_parser(struct device_node *dt,
						struct himax_i2c_platform_data *pdata)
{
	u32 data = 0;
	uint8_t cnt = 0, i = 0;
	uint32_t coords[4] = {0};
	struct device_node *node, *pp = NULL;
	struct himax_virtual_key *vk;
	node = of_parse_phandle(dt, "virtualkey", 0);

	if (node == NULL) {
		I(" DT-No vk info in DT\n");
		return;
	} else {
		while ((pp = of_get_next_child(node, pp)))
			cnt++;

		if (!cnt)
			return;

		vk = kzalloc(cnt * (sizeof *vk), GFP_KERNEL);
		pp = NULL;

		while ((pp = of_get_next_child(node, pp))) {
			if (of_property_read_u32(pp, "idx", &data) == 0)
				vk[i].index = data;

			if (of_property_read_u32_array(pp, "range", coords, 4) == 0) {
				vk[i].x_range_min = coords[0], vk[i].x_range_max = coords[1];
				vk[i].y_range_min = coords[2], vk[i].y_range_max = coords[3];
			} else {
				I(" range faile\n");
			}

			i++;
		}

		pdata->virtual_key = vk;

		for (i = 0; i < cnt; i++)
			I(" vk[%d] idx:%d x_min:%d, y_max:%d\n", i, pdata->virtual_key[i].index,
			  pdata->virtual_key[i].x_range_min, pdata->virtual_key[i].y_range_max);
	}
}
#endif
int himax_parse_dt(struct himax_ts_data *ts, struct himax_i2c_platform_data *pdata)
{
	struct device_node *dt = NULL;

	dt = ts->dev->of_node;
	I("%s: Entering!\n", __func__);

	/*if (dt) {
		const struct of_device_id *match;
			match = of_match_device(of_match_ptr(himax_match_table), ts->dev);

		if (!match) {
			TPD_DMESG("[Himax]Error: No device match found\n");
			return -ENODEV;
		}
	}*/

	/*  pdata->gpio_reset = of_get_named_gpio(dev->of_node, "rst-gpio", 0);
	    pdata->gpio_irq = of_get_named_gpio(dev->of_node, "int-gpio", 0);
	    I("pdata->gpio_reset: %d\n", pdata->gpio_reset );
	    I("pdata->gpio_irq: %d\n", pdata->gpio_irq ); */
	/* himax_tpd_rst_gpio_number = of_get_named_gpio(dt, "rst-gpio", 0); */
	/* himax_tpd_int_gpio_number = of_get_named_gpio(dt, "int-gpio", 0); */
	/* It will be a non-zero and non-one value for MTK PINCTRL API */
	himax_tpd_rst_gpio_number = GTP_RST_PORT;
	himax_tpd_int_gpio_number = GTP_INT_PORT;
	pdata->gpio_reset	= himax_tpd_rst_gpio_number;
	pdata->gpio_irq		= himax_tpd_int_gpio_number;
	I("%s: int : %2.2x\n", __func__, pdata->gpio_irq);
	I("%s: rst : %2.2x\n", __func__, pdata->gpio_reset);
#if defined(HX_PLATFOME_DEFINE_KEY)
	/* now 3 keys */
	himax_vk_parser(pdata, 3);
#else
	himax_vk_parser(dt, pdata);
#endif
	/* Set device tree data */
	/* Set panel coordinates */
	pdata->abs_x_min = hx_panel_coords[0], pdata->abs_x_max = hx_panel_coords[1];
	pdata->abs_y_min = hx_panel_coords[2], pdata->abs_y_max = hx_panel_coords[3];
	I(" %s:panel-coords = %d, %d, %d, %d\n", __func__, pdata->abs_x_min,
	  pdata->abs_x_max, pdata->abs_y_min, pdata->abs_y_max);
	/* Set display coordinates */
	pdata->screenWidth  = hx_display_coords[1];
	pdata->screenHeight = hx_display_coords[3];
	I(" %s:display-coords = (%d, %d)\n", __func__, pdata->screenWidth,
	  pdata->screenHeight);
	/* report type */
	pdata->protocol_type = report_type;
	return 0;
}

static ssize_t himax_spi_sync(struct spi_message *message)
{
	int status;

	status = spi_sync(hx_spi, message);

	if (status == 0) {
		status = message->status;
		if (status == 0)
			status = message->actual_length;
	}
	return status;
}

static int himax_spi_read(uint8_t *command, uint8_t command_len, uint8_t *data, uint32_t length, uint8_t toRetry)
{
	struct spi_message message;
	struct spi_transfer xfer[2];
	int retry = 0;
	int error = -1;

	spi_message_init(&message);
	memset(xfer, 0, sizeof(xfer));

	xfer[0].tx_buf = command;
	xfer[0].len = command_len;
	spi_message_add_tail(&xfer[0], &message);

	xfer[1].tx_buf = data;
	xfer[1].rx_buf = data;
	xfer[1].len = length;
	spi_message_add_tail(&xfer[1], &message);

	for (retry = 0; retry < toRetry; retry++) {
		error = spi_sync(hx_spi, &message);
		if (error) {
			E("SPI read error: %d\n", error);
		} else{
			break;
		}
	}
	if (retry == toRetry) {
		E("%s: SPI read error retry over %d\n",
			__func__, toRetry);
		return -EIO;
	}

	return 0;
}

static int himax_spi_write(uint8_t *buf, uint32_t length)
{

	struct spi_transfer	t = {
			.tx_buf		= buf,
			.len		= length,
	};
	struct spi_message	m;
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	return himax_spi_sync(&m);

}

int himax_bus_read(uint8_t command, uint8_t *data, uint8_t length, uint8_t toRetry)
{
	int result = 0;
	uint8_t spi_format_buf[3];

	mutex_lock(&hx_spi_lock);
	spi_format_buf[0] = 0xF3;
	spi_format_buf[1] = command;
	spi_format_buf[2] = 0x00;

	result = himax_spi_read(&spi_format_buf[0], 3, data, length, 10);
	mutex_unlock(&hx_spi_lock);

	return result;

	return 0;
}

int himax_bus_write(uint8_t command, uint8_t *data, uint8_t length, uint8_t toRetry)
{
	int i = 0;
	int result = 0;

	uint8_t *spi_format_buf = gBuffer;

	mutex_lock(&hx_spi_lock);
	spi_format_buf[0] = 0xF2;
	spi_format_buf[1] = command;

	for (i = 0; i < length; i++)
		spi_format_buf[i + 2] = data[i];

	result = himax_spi_write(spi_format_buf, length + 2);
	mutex_unlock(&hx_spi_lock);
	return result;
}

int himax_bus_write_command(uint8_t command, uint8_t toRetry)
{
	return himax_bus_write(command, NULL, 0, toRetry);
}

int himax_bus_master_write(uint8_t *data, uint8_t length, uint8_t toRetry)
{
	uint8_t buf[length];

	struct spi_transfer	t = {
		.tx_buf	= buf,
		.len	= length,
	};
	struct spi_message	m;
	int result = 0;

	mutex_lock(&(private_ts->spi_lock));
	memcpy(buf, data, length);

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	result = himax_spi_sync(&m);
	mutex_unlock(&(private_ts->spi_lock));

	return result;
}


uint8_t himax_int_gpio_read(int pinnum)
{
	return  gpio_get_value(himax_tpd_int_gpio);
}

void himax_int_enable(int enable)
{
	int irqnum = 0;
	irqnum = hx_irq;
	I("%s: Entering!\n", __func__);

	if (enable == 1 && irq_enable_count == 0) {
		enable_irq(irqnum);
		irq_enable_count++;
		private_ts->irq_enabled = 1;
	} else if (enable == 0 && irq_enable_count == 1) {
		disable_irq_nosync(irqnum);
		irq_enable_count--;
		private_ts->irq_enabled = 0;
	}

	I("irq_enable_count = %d\n", irq_enable_count);
}

#ifdef HX_RST_PIN_FUNC
void himax_rst_gpio_set(int pinnum, uint8_t value)
{
	if (value)
		tpd_gpio_output(himax_tpd_rst_gpio_number, 1);
	else
		tpd_gpio_output(himax_tpd_rst_gpio_number, 0);
}
#endif
/*  int himax_match_device(struct device *dev)
    {
	if (dev->of_node) {
		const struct of_device_id *match;

		match = of_match_device(of_match_ptr(himax_match_table), dev);
		if (!match) {
			TPD_DMESG("Error: No device match found\n");
			return -ENODEV;
		}
	}
	return 0;
    }*/
int himax_gpio_power_config(struct himax_i2c_platform_data *pdata)
{
/*	int error = 0;
	error = regulator_enable(tpd->reg);

	if (error != 0)
		TPD_DMESG("Failed to enable reg-vgp6: %d\n", error);
*/
	msleep(100);
#ifdef HX_RST_PIN_FUNC
	tpd_gpio_output(himax_tpd_rst_gpio_number, 1);
	msleep(20);
	tpd_gpio_output(himax_tpd_rst_gpio_number, 0);
	msleep(20);
	tpd_gpio_output(himax_tpd_rst_gpio_number, 1);
#endif
	TPD_DMESG("mtk_tpd: himax reset over \n");
	/* set INT mode */
	tpd_gpio_as_int(himax_tpd_int_gpio_number);
	return 0;
}

static void himax_ts_isr_func(struct himax_ts_data *ts)
{
	himax_ts_work(ts);
}

irqreturn_t himax_ts_thread(int irq, void *ptr)
{

	himax_ts_isr_func((struct himax_ts_data *)ptr);

	return IRQ_HANDLED;
}

static void himax_ts_work_func(struct work_struct *work)
{
	struct himax_ts_data *ts = container_of(work, struct himax_ts_data, work);
	himax_ts_work(ts);
}

int himax_int_register_trigger(void)
{
	int ret = NO_ERR;
	struct himax_ts_data *ts = private_ts;

	if (ic_data->HX_INT_IS_EDGE) {
		ret = request_threaded_irq(hx_irq, NULL, himax_ts_thread,
									IRQF_TRIGGER_FALLING | IRQF_ONESHOT, HIMAX_common_NAME, ts);
	} else {
		ret = request_threaded_irq(hx_irq, NULL, himax_ts_thread,
									IRQF_TRIGGER_LOW | IRQF_ONESHOT, HIMAX_common_NAME, ts);
	}

	return ret;
}

int himax_int_en_set(void)
{
	int ret = NO_ERR;
	ret = himax_int_register_trigger();
	return ret;
}

int himax_ts_register_interrupt(void)
{
	struct himax_ts_data *ts = private_ts;
	struct device_node *node = NULL;
	int ret = 0;

	u32 ints[2] = {0, 0};
	node = of_find_matching_node(node, touch_of_match);

	if (node) {
		ret = of_property_read_u32_array(node, "debounce", ints, ARRAY_SIZE(ints));
		if(ret != 0){
                    E("failed to read debounce data.\n");
		}
		gpio_set_debounce(ints[0], ints[1]);
		himax_touch_irq = irq_of_parse_and_map(node, 0);
		I("himax_touch_irq=%ud \n", himax_touch_irq);
		hx_irq = himax_touch_irq;
	} else {
		I("[%s] tpd request_irq can not find touch eint device node!\n", __func__);
	}

	ts->irq_enabled = 0;
	ts->use_irq = 0;

	/* Work functon */
	if (hx_irq) {/*INT mode*/
		ts->use_irq = 1;
		ret = himax_int_register_trigger();

		if (ret == 0) {
			ts->irq_enabled = 1;
			irq_enable_count = 1;
			I("%s: irq enabled at qpio: %d\n", __func__, hx_irq);
#ifdef HX_SMART_WAKEUP
			irq_set_irq_wake(hx_irq, 1);
#endif
		} else {
			ts->use_irq = 0;
			E("%s: request_irq failed\n", __func__);
		}
	} else {
		I("%s: hx_irq is empty, use polling mode.\n", __func__);
	}

	if (!ts->use_irq) {/*if use polling mode need to disable HX_ESD_RECOVERY function*/
		ts->himax_wq = create_singlethread_workqueue("himax_touch");
		INIT_WORK(&ts->work, himax_ts_work_func);
		hrtimer_init(&ts->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		ts->timer.function = himax_ts_timer_func;
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
		I("%s: polling mode enabled\n", __func__);
	}

	return ret;
}

struct pinctrl *himax_spi_pinctrl;
struct pinctrl_state *spi_all;

static int himax_common_probe_spi(struct spi_device *spi)
{
	struct himax_ts_data *ts;
	int ret = 0;
	/* Allocate driver data */
	I("%s:IN!\n", __func__);

/*
	himax_spi_pinctrl = devm_pinctrl_get(&(spi->dev));
	if (IS_ERR(himax_spi_pinctrl)) {
		ret = PTR_ERR(himax_spi_pinctrl);
		E("Cannot find touch spi pinctrl!\n");
	}
	spi_all = pinctrl_lookup_state(himax_spi_pinctrl, "state_spi_all");

	if (IS_ERR(spi_all)) {
		ret = PTR_ERR(spi_all);
		E("Cannot find state_spi_all %d!\n", ret);
	} else {
		ret = pinctrl_select_state(himax_spi_pinctrl, spi_all);
		I("%s: state_spi_all select ret = %d\n", __func__, ret);
	}
*/
	gBuffer = kzalloc(sizeof(uint8_t) * PACKET_MAX_SZ, GFP_KERNEL);
	if (gBuffer == NULL) {
		E("%s: allocate gBuffer failed\n", __func__);
		ret = -ENOMEM;
		goto err_alloc_gbuffer_failed;
	}

	ts = kzalloc(sizeof(struct himax_ts_data), GFP_KERNEL);
	if (ts == NULL) {
		E("%s: allocate himax_ts_data failed\n", __func__);
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}

	//spin_lock_init(&hx_spi_lock);

	/* Initialize the driver data */
	hx_spi = spi;

	//himax_hw_init();

	/* setup SPI parameters */
	/* CPOL=CPHA=0, speed 1MHz */
	if (hx_spi->master->flags & SPI_MASTER_HALF_DUPLEX) {
        I("Full duplex not supported by master\n");
        ret = -EIO;
        goto err_spi_setup;
    }
	hx_spi->mode            = SPI_MODE_3;
	hx_spi->bits_per_word   = 8;
#ifndef CONFIG_SPI_MT65XX
	memcpy(&hx_spi_mcc, &spi_ctrdata, sizeof(struct mt_chip_conf));
	hx_spi->controller_data = (void *)&hx_spi_mcc;
	I("%s %d,Old SPI,need to spi_setup()\n", __func__, __LINE__);
	spi_setup(hx_spi);
#endif

#if 0//Himax Test
	memcpy(&hx_spi_mcc, &spi_ctrdata, sizeof(struct mtk_chip_config));
	hx_spi->controller_data = (void *)&hx_spi_mcc;
	I("%s %d,New SPI, NOT need to spi_setup()\n", __func__, __LINE__);
#endif

	hx_irq = 0;
	spi_set_drvdata(spi, ts);
	mutex_init(&hx_spi_lock);

	/* allocate buffer for SPI transfer */
	/*hx_spi_buffer = kzalloc(bufsiz, GFP_KERNEL);
	if (hx_spi_buffer == NULL) {
		status = -ENOMEM;
		goto err_check_functionality_failed;
	}*/

	//hx_spi = spi;
	ts->dev = &spi->dev;
	private_ts = ts;

	ret = himax_chip_common_init();

	return ret;
//err_check_functionality_failed:
err_spi_setup:
	kfree(ts);
err_alloc_data_failed:
	kfree(gBuffer);
err_alloc_gbuffer_failed:
	return ret;
}

int himax_common_remove_spi(struct spi_device *spi)
{
	int ret = 0;
	hx_spi = NULL;
	spi_set_drvdata(spi, NULL);
	himax_chip_common_deinit();

	return ret;
}

static void himax_common_suspend(struct device *dev)
{
	struct himax_ts_data *ts = private_ts;

	I("%s: enter \n", __func__);
	himax_chip_common_suspend(ts);
	I("%s: END \n", __func__);
	return ;
}
static void himax_common_resume(struct device *dev)
{
#if defined(LCD_TP_RESUME_WORK)
	I("%s: TP resume from LCD, return... \n", __func__);
	return;
#else
	struct himax_ts_data *ts = private_ts;

	I("%s: enter \n", __func__);
	himax_chip_common_resume(ts);
	I("%s: END \n", __func__);
	return ;
#endif
}

#if defined(LCD_TP_RESUME_WORK)
void himax_resume_workqueue(struct work_struct *work)
{
	I("%s: enter \n", __func__);
//	struct himax_ts_data *ts = private_ts;
	himax_chip_common_resume(private_ts);
	I("%s: end \n", __func__);
	return;
}

 void himax_lcd_resume_func(void)
{
	I("%s: enter \n", __func__);
	schedule_work(&private_ts->resume_work_queue);
	I("%s: end \n", __func__);
	return;;
}

#else
 void himax_lcd_resume_func(void)
{
	I("%s: TP resume from notifier,return... \n", __func__);
	return;;
}

#endif

// void himax_lcd_resume_func(void);

EXPORT_SYMBOL(himax_lcd_resume_func);

#if defined(CONFIG_FB)
int fb_notifier_callback(struct notifier_block *self,
							unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;
	struct himax_ts_data *ts =
	    container_of(self, struct himax_ts_data, fb_notif);
	I(" %s\n", __func__);

	if (evdata && evdata->data && event == FB_EVENT_BLANK && ts && (hx_spi)) {
		blank = evdata->data;

		switch (*blank) {
		case FB_BLANK_UNBLANK:
				himax_common_resume(ts->dev);
			break;

		case FB_BLANK_POWERDOWN:
		case FB_BLANK_HSYNC_SUSPEND:
		case FB_BLANK_VSYNC_SUSPEND:
		case FB_BLANK_NORMAL:
				himax_common_suspend(ts->dev);
			break;
		}
	}

	return 0;
}
#endif

static const struct i2c_device_id himax_common_ts_id[] = {
	{HIMAX_common_NAME, 0 },
	{}
};


struct spi_device_id hx_spi_id_table[]  = {
	{"hxmax_generic", 0},
	{},
};//{"himax-spi", 1};
struct spi_driver himax_common_driver = {
	.driver = {
		.name = HIMAX_common_NAME,
                .bus = &spi_bus_type,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = himax_match_table,
#endif
	},
	.probe = himax_common_probe_spi,
	.remove = himax_common_remove_spi,
	.id_table = hx_spi_id_table,
};

static int himax_common_local_init(void) {
	int retval;

	I("[Himax] Himax_ts SPI Touchscreen Driver local init\n");

/*	tpd->reg = regulator_get(tpd->tpd_dev, "vtouch");
/	retval = regulator_set_voltage(tpd->reg, 2800000, 2800000);

	if (retval != 0) {
		E("Failed to set voltage 2V8: %d\n", retval);
	}

*/	

	//spi_register_board_info(spi_board_devs, 1);
	retval = spi_register_driver(&himax_common_driver);
	if (retval < 0) {
		E("unable to add SPI driver.\n");
		E("Himax touch screen driver init failed.\n");
		return -EFAULT;
	}
	I("[Himax] Himax touch screen driver init Success.\n");

#if defined(HX_PLATFOME_DEFINE_KEY)

	if (tpd_dts_data.use_tpd_button) {
		I("tpd_dts_data.use_tpd_button %d\n", tpd_dts_data.use_tpd_button);
		tpd_button_setting(tpd_dts_data.tpd_key_num,
							tpd_dts_data.tpd_key_local,
							tpd_dts_data.tpd_key_dim_local);
	}

#endif
	I("end %s, %d\n", __FUNCTION__, __LINE__);
	tpd_type_cap = 1;
	return 0;
}

static struct tpd_driver_t tpd_device_driver = {
	.tpd_device_name = HIMAX_common_NAME,
	.tpd_local_init = himax_common_local_init,
	.suspend = himax_common_suspend,
	.resume = himax_common_resume,
#ifdef TPD_HAVE_BUTTON
	.tpd_have_button = 1,
#else
	.tpd_have_button = 0,
#endif
};

static int (*kp_tpd_scan_device)(void);

void himax_trigger_platform_probe(void)
{
	kp_tpd_scan_device = (void *)kallsyms_lookup_name("tpd_scan_device");
	if (kp_tpd_scan_device)
		kp_tpd_scan_device();
	else
		I("failed to get tpd_scan_device!\n");
}
EXPORT_SYMBOL(himax_trigger_platform_probe);

static int __init himax_common_init(void)
{
	I("%s: Enter.\n", __func__);

	D("Himax check double loading\n");
	if (g_mmi_refcnt++ > 0) {
	  I("Himax driver has been loaded! ignoring....\n");
	  return 0;
	}
	tpd_get_dts_info();

	if (tpd_driver_add(&tpd_device_driver) < 0)
		I("Failed to add Driver!\n");

	I("%s: Exit..\n", __func__);

	return 0;
}

static void __exit himax_common_exit(void)
{
	spi_unregister_driver(&himax_common_driver);
}

module_init(himax_common_init);
module_exit(himax_common_exit);

MODULE_DESCRIPTION("Himax_common driver");
MODULE_LICENSE("GPL");

