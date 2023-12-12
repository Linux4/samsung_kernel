/*
 * GalaxyCore touchscreen driver
 *
 * Copyright (C) 2021 GalaxyCore Incorporated
 *
 * Copyright (C) 2021 Neo Chen <neo_chen@gcoreinc.com>
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
 */
#include "gcore_drv_common.h"
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/gpio.h>
#include <linux/firmware.h>
#include <linux/time.h>
#include <linux/rtc.h>
#include <linux/timer.h>

/*
 * Note.
 * The below compile option is used to enable the panel type used
 */
/* #define CONFIG_ENABLE_PANEL_TYPE_INX */
/* #define CONFIG_ENABLE_PANEL_TYPE_HKC */
#define CONFIG_ENABLE_PANEL_TYPE_PANDA
/* #define CONFIG_ENABLE_PANEL_TYPE_MDT */
/* #define CONFIG_ENABLE_PANEL_TYPE_HSD */


#define MP_TEST_PROC_FILENAME "ts03_selftest"

#define MP_TEST_OF_PREFIX                  "ts03,"
#define MP_TEST_OF_ITEM_TEST_INT_PIN       MP_TEST_OF_PREFIX"test-int-pin"
#define MP_TEST_OF_ITEM_TEST_CHIP_ID       MP_TEST_OF_PREFIX"test-chip-id"
#define MP_TEST_OF_TEST_CHIP_ID            MP_TEST_OF_PREFIX"chip-id"
#define MP_TEST_OF_ITEM_TEST_OPEN          MP_TEST_OF_PREFIX"test-open"
#define MP_TEST_OF_TEST_OPEN_CB            MP_TEST_OF_PREFIX"open-cb"
#define MP_TEST_OF_TEST_OPEN_MIN           MP_TEST_OF_PREFIX"open-min"
#define MP_TEST_OF_ITEM_TEST_SHORT         MP_TEST_OF_PREFIX"test-short"
#define MP_TEST_OF_TEST_SHORT_CB           MP_TEST_OF_PREFIX"short-cb"
#define MP_TEST_OF_TEST_SHORT_MIN          MP_TEST_OF_PREFIX"short-min"
#define MP_TEST_OF_ITEM_TEST_RAWDATA       MP_TEST_OF_PREFIX"test-rawdata"
#define MP_TEST_OF_TEST_RAWDATA_MAX        MP_TEST_OF_PREFIX"rawdata-max"
#define MP_TEST_OF_TEST_RAWDATA_RANGE      MP_TEST_OF_PREFIX"rawdata-range"


#define MP_TEST_DATA_DIR              "/data/vendor/fac_sources"
#define MP_TEST_LOG_FILEPATH          MP_TEST_DATA_DIR"/GCORE_MP_Log"

#define PROC_NODE

#define GC_PROC_TOUCHSCREEN_FOLDER             "touchscreen"
#define GC_PROC_CTP_OPENSHORT_TEST_FILE        "ctp_openshort_test"
struct proc_dir_entry *gc_proc_touchscreen_dir;
struct proc_dir_entry *gc_proc_ctp_openshort_test_file = NULL;
#define EMPTY_NUM  4

#if defined(CONFIG_ENABLE_PANEL_TYPE_INX)
	int empty_place[EMPTY_NUM] = { 0, 17, -1, -1 };
#elif defined(CONFIG_ENABLE_PANEL_TYPE_HKC)
	int empty_place[EMPTY_NUM] = { 0, 17, -1, -1 };
#elif defined(CONFIG_ENABLE_PANEL_TYPE_PANDA)
	int empty_place[EMPTY_NUM] = { 8, 9, -1, -1 };
#elif defined(CONFIG_ENABLE_PANEL_TYPE_MDT)
	int empty_place[EMPTY_NUM] = { 0, 8, 9, 17 };
#elif defined(CONFIG_ENABLE_PANEL_TYPE_HSD)
	int empty_place[EMPTY_NUM] = { 8, 9, -1, -1 };
#else
	int empty_place[EMPTY_NUM] = { 8, 9, -1, -1 };
#endif

#define MP_RAWDATA_AVG_TEST  0  /* rawdata avg test no use for now */
#define MP_RAWDATA_DEC_TEST  0  /* rawdata dec test no use for now */

enum MP_DATA_TYPE {
	OPEN_DATA,
	SHORT_DATA,
	RAW_DATA,
	RAWDATA_DECH,
	RAWDATA_DECL,
	NOISE_DATA,
};

DECLARE_COMPLETION(mp_test_complete);

struct gcore_mp_data {
	struct proc_dir_entry *mp_proc_entry;

	int test_int_pin;
	int int_pin_test_result;

	int test_chip_id;
	int chip_id_test_result;
	u8 chip_id[2];

	int mp_update_result;

	int test_open;
	int open_test_result;
	int open_cb;
	int open_min;
	u8 *open_data;
	u16 *open_node_val;

	int test_short;
	int short_test_result;
	int short_cb;
	int short_min;
	u8 *short_data;
	u16 *short_node_val;

	int test_rawdata;
	int rawdata_test_result;
	u8 *raw_data;
	u16 *rawdata_node_val;
	u16 rawdata_node_max;
	u16 rawdata_node_min;
	u16 *rawdata_dec_h;
	u16 rawdata_dech_max;
	u16 rawdata_dech_min;
	u16 *rawdata_dec_l;
	u16 rawdata_decl_max;
	u16 rawdata_decl_min;
	u16 *rawdata_avg_h;
	u16 *rawdata_avg_l;
	u32 rawdata_range[2];
	int rawdata_per;
	int rawdata_range_max;
	int rawdata_range_min;

	int test_noise;
	int noise_test_result;
	int peak_to_peak;
	u8 *noise_data;
	u16 *noise_node_val;

	int all_test_result;

	u8 mpbin_ver[4];

	struct gcore_dev *gdev;
};

struct gcore_mp_data *g_mp_data;
struct task_struct *mp_thread;

static int gcore_mp_test_fn_init(struct gcore_dev *gdev);
static void gcore_mp_test_fn_remove(struct gcore_dev *gdev);

static void dump_mp_data_to_seq_file(struct seq_file *m, const u16 *data, int rows, int cols);

struct gcore_exp_fn mp_test_fn = {
	.fn_type = GCORE_MP_TEST,
	.wait_int = false,
	.event_flag = false,
	.init = gcore_mp_test_fn_init,
	.remove = gcore_mp_test_fn_remove,
};

static void *gcore_seq_start(struct seq_file *m, loff_t *pos)
{
	return *pos < 1 ? (void *)1 : NULL;
}

static void *gcore_seq_next(struct seq_file *m, void *v, loff_t *pos)
{
	++*pos;
	return NULL;
}

static void gcore_seq_stop(struct seq_file *m, void *v)
{
	return;
}

static int gcore_seq_show(struct seq_file *m, void *v)
{
	struct gcore_mp_data *mp_data = (struct gcore_mp_data *)m->private;
	int rows = RAWDATA_ROW;
	int cols = RAWDATA_COLUMN;

	GTP_DEBUG("gcore seq show");

	if (mp_data == NULL) {
		GTP_ERROR("seq_file private data = NULL");
		return -EPERM;
	}

	if (mp_data->test_int_pin) {
		seq_printf(m, "Int-Pin Test %s!\n\n", mp_data->int_pin_test_result == 0 ? "PASS" : "FAIL");
	}

	if (mp_data->test_chip_id) {
		seq_printf(m, "Chip-id Test %s!\n\n", mp_data->chip_id_test_result == 0 ? "PASS" : "FAIL");
	}

	if (mp_data->test_open) {
		if (mp_data->open_test_result == 0) {
			seq_printf(m, "Open Test PASS!\n\n");
		} else {
			seq_printf(m, "Open Test FAIL!\n");
			dump_mp_data_to_seq_file(m, mp_data->open_node_val, rows, cols);
			seq_putc(m, '\n');
		}

	}

	if (mp_data->test_short) {
		if (mp_data->short_test_result == 0) {
			seq_printf(m, "Short Test PASS!\n\n");
		} else {
			seq_printf(m, "Short Test FAIL!\n");
			dump_mp_data_to_seq_file(m, mp_data->short_node_val, rows, cols);
			seq_putc(m, '\n');
		}

	}

	if (mp_data->test_rawdata) {
		if (mp_data->rawdata_test_result == 0) {
			seq_printf(m, "Rawrata Test PASD!\n\n");
		} else {
			seq_printf(m, "Rawrata Test FAIL!\n");
			dump_mp_data_to_seq_file(m, mp_data->rawdata_node_val, rows, cols);
			seq_putc(m, '\n');
		}

	}

	return 0;
}

const struct seq_operations gcore_mptest_seq_ops = {
	.start = gcore_seq_start,
	.next = gcore_seq_next,
	.stop = gcore_seq_stop,
	.show = gcore_seq_show,
};

int gcore_parse_mp_test_dt(struct gcore_mp_data *mp_data)
{
	struct device_node *np = mp_data->gdev->bus_device->dev.of_node;

	GTP_DEBUG("gcore parse mp test dt");

	mp_data->test_int_pin = of_property_read_bool(np, MP_TEST_OF_ITEM_TEST_INT_PIN);

	mp_data->test_chip_id = of_property_read_bool(np, MP_TEST_OF_ITEM_TEST_CHIP_ID);
	if (mp_data->test_chip_id) {
		of_property_read_u8_array(np, MP_TEST_OF_TEST_CHIP_ID, mp_data->chip_id,
					  sizeof(mp_data->chip_id));
	}

	mp_data->test_open = of_property_read_bool(np, MP_TEST_OF_ITEM_TEST_OPEN);
	if (mp_data->test_open) {
		of_property_read_u32(np, MP_TEST_OF_TEST_OPEN_CB, &mp_data->open_cb);
		of_property_read_u32(np, MP_TEST_OF_TEST_OPEN_MIN, &mp_data->open_min);
	}

	mp_data->test_short = of_property_read_bool(np, MP_TEST_OF_ITEM_TEST_SHORT);
	if (mp_data->test_short) {
		of_property_read_u32(np, MP_TEST_OF_TEST_SHORT_CB, &mp_data->short_cb);
		of_property_read_u32(np, MP_TEST_OF_TEST_SHORT_MIN, &mp_data->short_min);
	}

	mp_data->test_rawdata = of_property_read_bool(np, MP_TEST_OF_ITEM_TEST_RAWDATA);
	if (mp_data->test_rawdata) {
		of_property_read_u32_array(np, MP_TEST_OF_TEST_RAWDATA_RANGE, mp_data->rawdata_range,
					  2);
		of_property_read_u32(np, MP_TEST_OF_TEST_RAWDATA_MAX, &mp_data->rawdata_per);
		mp_data->rawdata_range_min = mp_data->rawdata_range[0];
		mp_data->rawdata_range_max = mp_data->rawdata_range[1];

		GTP_DEBUG("rawdata_range_min11 = %d",mp_data->rawdata_range[0]);
		GTP_DEBUG("rawdata_range_min11 = %d",mp_data->rawdata_range[1]);
		GTP_DEBUG("rawdata_range_max = %d",mp_data->rawdata_range_min);
		GTP_DEBUG("rawdata_range_min11 = %d",mp_data->rawdata_range_max);
		GTP_DEBUG("num = %ld",sizeof(mp_data->rawdata_range));
	}
	return 0;
}
#define MP_BIN_NAME_HKC "GC7202_mp_hkc.bin"
#define MP_BIN_NAME_TXD "GC7202_mp_txd.bin"
#define MP_TEST_INI_HKC   "gcore_mp_test_hkc.ini"
#define MP_TEST_INI_TXD   "gcore_mp_test_txd.ini"
//+S96818AA1-1936,wangtao14.wt,modify,2023/07/06,exchange for reading ini files
#if 0
static u8 *read_line(u8 *buf, int buf_len, struct file *fp)
{
	int ret;
	int i = 0;
	mm_segment_t fs;

	fs = get_fs();
	set_fs(KERNEL_DS);
	ret = vfs_read(fp, buf, buf_len, &(fp->f_pos));
	set_fs(fs);

	if (ret <= 0)
		return NULL;

	while (buf[i++] != '\n' && i < ret) ;

	if (i < ret) {
		fp->f_pos += i - ret;
	}

	if (i < buf_len) {
		buf[i] = 0;
	}
	return buf;
}
#endif
extern char *saved_command_line;
int gcore_parse_mp_test_ini(struct gcore_mp_data *mp_data)
{
	u8 *tmp = NULL;
	u8 *buff = NULL;
	int ret = 0;
	int chip_type = 0;
	const struct firmware *fw = NULL;
	struct device *dev = &mp_data->gdev->bus_device->dev;
    if (strstr(saved_command_line,"n28_gc7272_dsi_vdo_hdp_xinxian_hkc")){
        ret = request_firmware(&fw, MP_TEST_INI_HKC, dev);
    }else
	{
        ret = request_firmware(&fw, MP_TEST_INI_TXD, dev);
	}
	if (ret == 0) {
        if (strstr(saved_command_line,"n28_gc7272_dsi_vdo_hdp_xinxian_hkc")){
            GTP_DEBUG("firmware request(%s) success", MP_TEST_INI_HKC);
        }else
    	{
            GTP_DEBUG("firmware request(%s) success", MP_TEST_INI_TXD);
    	}
//+S96818AA1-1936,daijun1.wt,modify,2023/07/29,Modify GC7272 optimize TP openshort test code
		buff = kzalloc(fw->size + 1, GFP_KERNEL);
		GTP_DEBUG("firmware size is:%d", fw->size+1);
		if (!buff) {
			GTP_ERROR("buffer kzalloc fail");
			return -EPERM;
		}
		memcpy(buff, fw->data, fw->size);
		buff[fw->size] = '\0';
		GTP_DEBUG("buff is: %s", buff);
//-S96818AA1-1936,daijun1.wt,modify,2023/07/29,Modify GC7272 optimize TP openshort test code
		release_firmware(fw);
		/* test int pin */
		tmp = strstr(buff, "test-int-pin=");
		ret = sscanf(tmp, "test-int-pin=%d", &mp_data->test_int_pin);
		if (ret < 0)
			GTP_ERROR("%s read test-int-pin error.\n", __func__);
		GTP_DEBUG("test-int-pin:%s", mp_data->test_int_pin ? "y" : "n");

		/* test chip id */
		tmp = strstr(buff, "test-chip-id=");
		ret = sscanf(tmp, "test-chip-id=%d", &mp_data->test_chip_id);
		if (ret < 0)
			GTP_ERROR("%s read test chip id error.\n", __func__);
		GTP_DEBUG("test-chip-id:%s", mp_data->test_chip_id ? "y" : "n");

		tmp = strstr(buff, "chip-id=[");
		ret = sscanf(tmp, "chip-id=[%x]", &chip_type);
		if (ret < 0)
			GTP_ERROR("%s read chip id error.\n", __func__);
		GTP_DEBUG("chip-id:%x", chip_type);
		mp_data->chip_id[0] = (u8) (chip_type >> 8);
		mp_data->chip_id[1] = (u8) chip_type;

		/* test open */
		tmp = strstr(buff, "test-open=");
		ret = sscanf(tmp, "test-open=%d", &mp_data->test_open);
		if (ret < 0)
			GTP_ERROR("%s read test open error.\n", __func__);
		GTP_DEBUG("test-open:%s", mp_data->test_open ? "y" : "n");

		tmp = strstr(buff, "open_cb=");
		ret = sscanf(tmp, "open_cb=%d", &mp_data->open_cb);
		if (ret < 0)
			GTP_ERROR("%s read open cb error.\n", __func__);
		GTP_DEBUG("read open cb:%d", mp_data->open_cb);

		tmp = strstr(buff, "open_min=");
		ret = sscanf(tmp, "open_min=%d", &mp_data->open_min);
		if (ret < 0)
			GTP_ERROR("%s read open min error.\n", __func__);
		GTP_DEBUG("read open min:%d", mp_data->open_min);

		/* test short */
		tmp = strstr(buff, "test-short=");
		ret = sscanf(tmp, "test-short=%d", &mp_data->test_short);
		if (ret < 0)
			GTP_ERROR("%s read test short error.\n", __func__);
		GTP_DEBUG("test-short:%s", mp_data->test_short ? "y" : "n");

		tmp = strstr(buff, "short_cb=");
		ret = sscanf(tmp, "short_cb=%d", &mp_data->short_cb);
		if (ret < 0)
			GTP_ERROR("%s read short cb error.\n", __func__);
		GTP_DEBUG("read short cb:%d", mp_data->short_cb);

		tmp = strstr(buff, "short_min=");
		ret = sscanf(tmp, "short_min=%d", &mp_data->short_min);
		if (ret < 0)
			GTP_ERROR("%s read short min error.\n", __func__);
		GTP_DEBUG("read short min:%d", mp_data->short_min);

		/* test rawdata */
		tmp = strstr(buff, "test-rawdata=");
		ret = sscanf(tmp, "test-rawdata=%d", &mp_data->test_rawdata);
		if (ret < 0)
			GTP_ERROR("%s test-rawdata error.\n", __func__);
		GTP_DEBUG("test-rawdata:%s", mp_data->test_rawdata ? "y" : "n");

		tmp = strstr(buff, "rawdata_max=");
		ret = sscanf(tmp, "rawdata_max=%d", &mp_data->rawdata_per);
		if (ret < 0)
			GTP_ERROR("%s rawdata_max error.\n", __func__);
		GTP_DEBUG("read rawdata max:%d", mp_data->rawdata_per);

		tmp = strstr(buff, "rawdata_range=");
		ret = sscanf(tmp, "rawdata_range=[%d %d]", &mp_data->rawdata_range_min, &mp_data->rawdata_range_max);
		if (ret < 0)
			GTP_ERROR("%s rawdata range error.\n", __func__);
		GTP_DEBUG("read range min:%d max:%d", mp_data->rawdata_range_min, mp_data->rawdata_range_max);

		/* test noise */
		tmp = strstr(buff, "test-noise=");
		ret = sscanf(tmp, "test-noise=%d", &mp_data->test_noise);
		if (ret < 0)
			GTP_ERROR("%s test-noise error.\n", __func__);
		GTP_DEBUG("test-noise:%s", mp_data->test_noise ? "y" : "n");

		tmp = strstr(buff, "peak_to_peak=");
		ret = sscanf(tmp, "peak_to_peak=%d", &mp_data->peak_to_peak);
		if (ret < 0)
			GTP_ERROR("%s peak to peak error.\n", __func__);
		GTP_DEBUG("read peak to peak:%d", mp_data->peak_to_peak);

		tmp = strstr(buff, "empty_place=");
		if(NULL == tmp){
			GTP_DEBUG("empty_place set default:%d %d",empty_place[0],empty_place[1]);
		}else{
		 	int num1, num2;
			if (sscanf(tmp, "empty_place=%d,%d", &num1, &num2) == 2) {
				empty_place[0] = num1;
            	empty_place[1] = num2;
			}
			GTP_DEBUG("read empty_place:%d,%d", empty_place[0],empty_place[1]);
		}

		kfree(buff);
	} else {
        if (strstr(saved_command_line,"n28_gc7272_dsi_vdo_hdp_xinxian_hkc")){
            GTP_ERROR("firmware request(%s) fail,ret=%d", MP_TEST_INI_HKC, ret);
        }else
    	{
            GTP_ERROR("firmware request(%s) fail,ret=%d", MP_TEST_INI_TXD, ret);
    	}

}

	return 0;

#if 0
	struct file *f = NULL;
	u8 *buff = NULL;
	int buff_len = 30;
	int chip_type = 0;

	buff = kzalloc(buff_len, GFP_KERNEL);
	if (IS_ERR_OR_NULL(buff)) {
		GTP_ERROR("file mem alloc fail!");
		return -EPERM;
	}
if (strstr(saved_command_line,"n28_gc7272_dsi_vdo_hdp_xinxian_hkc")){
        f = filp_open(MP_TEST_INI_HKC, O_RDONLY, 644);
    }else
	{
		f = filp_open(MP_TEST_INI_TXD, O_RDONLY, 644);
	}
	if (IS_ERR_OR_NULL(f)) {
		GTP_ERROR("open mp test ini file fail!");
		return -EPERM;
	}

	/* [MP TEST] */
	buff = read_line(buff, buff_len, f);
	GTP_DEBUG("ini read line %s", buff);

	/* test int pin */
	buff = read_line(buff, buff_len, f);
	GTP_DEBUG("ini read line %s", buff);
	if (buff) {
		sscanf(buff, "test-int-pin=%d", &mp_data->test_int_pin);
		GTP_DEBUG("test-int-pin:%s", mp_data->test_int_pin ? "y" : "n");
	}

	buff = read_line(buff, buff_len, f);
	GTP_DEBUG("ini read line %s", buff);

	/* test chip id */
	buff = read_line(buff, buff_len, f);
	GTP_DEBUG("ini read line %s", buff);
	if (buff) {
		sscanf(buff, "test-chip-id=%d", &mp_data->test_chip_id);
		GTP_DEBUG("test-chip-id:%s", mp_data->test_chip_id ? "y" : "n");
	}

	buff = read_line(buff, buff_len, f);
	GTP_DEBUG("ini read line %s", buff);
	if (buff) {
		sscanf(buff, "chip-id=[%x]", &chip_type);
		GTP_DEBUG("chip-id:%x", chip_type);
	}
	mp_data->chip_id[0] = (u8) (chip_type >> 8);
	mp_data->chip_id[1] = (u8) chip_type;

	buff = read_line(buff, buff_len, f);
	GTP_DEBUG("ini read line %s", buff);

	buff = read_line(buff, buff_len, f);
	GTP_DEBUG("ini read line %s", buff);
	if (buff) {
		sscanf(buff, "test-open=%d", &mp_data->test_open);
		GTP_DEBUG("test-open:%s", mp_data->test_open ? "y" : "n");
	}

	buff = read_line(buff, buff_len, f);
	GTP_DEBUG("ini read line %s", buff);
	if (buff) {
		sscanf(buff, "open_cb=%d", &mp_data->open_cb);
		GTP_DEBUG("read open cb:%d", mp_data->open_cb);
	}

	buff = read_line(buff, buff_len, f);
	GTP_DEBUG("ini read line %s", buff);
	if (buff) {
		sscanf(buff, "open_min=%d", &mp_data->open_min);
		GTP_DEBUG("read open min:%d", mp_data->open_min);
	}

	buff = read_line(buff, buff_len, f);
	GTP_DEBUG("ini read line %s", buff);

	buff = read_line(buff, buff_len, f);
	GTP_DEBUG("ini read line %s", buff);
	if (buff) {
		sscanf(buff, "test-short=%d", &mp_data->test_short);
		GTP_DEBUG("test-short:%s", mp_data->test_short ? "y" : "n");
	}

	buff = read_line(buff, buff_len, f);
	GTP_DEBUG("ini read line %s", buff);
	if (buff) {
		sscanf(buff, "short_cb=%d", &mp_data->short_cb);
		GTP_DEBUG("read short cb:%d", mp_data->short_cb);
	}

	buff = read_line(buff, buff_len, f);
	GTP_DEBUG("ini read line %s", buff);
	if (buff) {
		sscanf(buff, "short_min=%d", &mp_data->short_min);
		GTP_DEBUG("read short min:%d", mp_data->short_min);
	}

	buff = read_line(buff, buff_len, f);
	GTP_DEBUG("ini read line %s", buff);

	buff = read_line(buff, buff_len, f);
	GTP_DEBUG("ini read line %s", buff);
	if (buff) {
		sscanf(buff, "test-rawdata=%d", &mp_data->test_rawdata);
		GTP_DEBUG("test-rawdata:%s", mp_data->test_rawdata ? "y" : "n");
	}

	buff = read_line(buff, buff_len, f);
	GTP_DEBUG("ini read line %s", buff);
	if (buff) {
		sscanf(buff, "rawdata_max=%d", &mp_data->rawdata_per);
		GTP_DEBUG("read rawdata per:%d", mp_data->rawdata_per);
	}

	buff = read_line(buff, buff_len, f);
	GTP_DEBUG("ini read line %s", buff);
	if (buff) {
		sscanf(buff, "rawdata_range=[%d %d]", &mp_data->rawdata_range_min, &mp_data->rawdata_range_max);
		GTP_DEBUG("read range min:%d max:%d", mp_data->rawdata_range_min, mp_data->rawdata_range_max);
	}

	buff = read_line(buff, buff_len, f);
	GTP_DEBUG("ini read line %s", buff);

	buff = read_line(buff, buff_len, f);
	GTP_DEBUG("ini read line %s", buff);
	if (buff) {
		sscanf(buff, "test-noise=%d", &mp_data->test_noise);
		GTP_DEBUG("test-noise:%s", mp_data->test_noise ? "y" : "n");
	}

	buff = read_line(buff, buff_len, f);
	GTP_DEBUG("ini read line %s", buff);
	if (buff) {
		sscanf(buff, "peak_to_peak=%d", &mp_data->peak_to_peak);
		GTP_DEBUG("read peak to peak:%d", mp_data->peak_to_peak);
	}

	if (f != NULL) {
		filp_close(f, NULL);
	}

	kfree(buff);

	return 0;
#endif
}
//-S96818AA1-1936,wangtao14.wt,modify,2023/07/06,exchange for reading ini files
int gcore_mp_test_int_pin(struct gcore_mp_data *mp_data)
{
	u8 read_buf[4] = { 0 };

	GTP_DEBUG("gcore mp test int pin");

	mutex_lock(&mp_data->gdev->transfer_lock);
	mp_data->gdev->irq_disable(mp_data->gdev);

	gcore_enter_idm_mode();

	usleep_range(1000, 1100);

	gcore_idm_read_reg(0xC0000098, read_buf, sizeof(read_buf));
	GTP_DEBUG("reg addr: 0xC0000098 read_buf: %x %x %x %x",
		  read_buf[0], read_buf[1], read_buf[2], read_buf[3]);

	usleep_range(1000, 1100);

	read_buf[0] &= 0x7F;
	read_buf[2] &= 0x7F;

	gcore_idm_write_reg(0xC0000098, read_buf, sizeof(read_buf));

	usleep_range(1000, 1100);

	if (gpio_get_value(mp_data->gdev->irq_gpio) == 1) {
		GTP_ERROR("INT pin state != LOW test fail!");
		goto fail1;
	}

	read_buf[2] |= BIT7;

	gcore_idm_write_reg(0xC0000098, read_buf, sizeof(read_buf));

	usleep_range(1000, 1100);

	if (gpio_get_value(mp_data->gdev->irq_gpio) == 0) {
		GTP_ERROR("INT pin state != HIGH test fail!");
		goto fail1;
	}

	gcore_exit_idm_mode();

	mp_data->gdev->irq_enable(mp_data->gdev);
	mutex_unlock(&mp_data->gdev->transfer_lock);

	return 0;

fail1:
	mp_data->gdev->irq_enable(mp_data->gdev);
	mutex_unlock(&mp_data->gdev->transfer_lock);
	return -EPERM;
}

int gcore_mp_test_chip_id(struct gcore_mp_data *mp_data)
{
	u8 buf[2] = { 0 };

	GTP_DEBUG("gcore mp test chip id");

	gcore_idm_read_id(buf, sizeof(buf));

	if ((buf[0] == mp_data->chip_id[0]) && (buf[1] == mp_data->chip_id[1])) {
		GTP_DEBUG("gcore mp test chip id match success!");
		return 0;
	} else {
		GTP_DEBUG("gcore mp test chip id match failed!");
		return -EPERM;
	}
}

int gcore_mp_test_item_open(struct gcore_mp_data *mp_data)
{
	u8 cb_high = (u8) (mp_data->open_cb >> 8);
	u8 cb_low = (u8) mp_data->open_cb;
	u8 cmd[] = { 0x40, 0xAA, 0x00, 0x32, 0x73, 0x71, cb_high, cb_low, 0x01 };
	struct gcore_dev *gdev = mp_data->gdev;
	int ret = 0;
	int i = 0;
	int j = 0;
	u8 *opendata = NULL;
	u16 node_data = 0;
	bool jump = 0;

	GTP_DEBUG("gcore mp test item open");

	gdev->fw_event = FW_READ_OPEN;
	gdev->firmware = mp_data->open_data;
#ifdef CONFIG_TOUCH_DRIVER_INTERFACE_SPI
	gdev->fw_xfer = 2048;
#else
	gdev->fw_xfer = RAW_DATA_SIZE;
#endif

	mutex_lock(&gdev->transfer_lock);

	mp_test_fn.wait_int = true;

	ret = gcore_bus_write(cmd, sizeof(cmd));
	if (ret) {
		GTP_ERROR("write mp test open cmd fail!");
		return -EPERM;
	}

	mutex_unlock(&gdev->transfer_lock);

	if (!wait_for_completion_interruptible_timeout(&mp_test_complete, 1 * HZ)) {
		GTP_ERROR("mp test item open timeout.");
		mp_test_fn.wait_int = false;
		return EPERM;
	}

	opendata = mp_data->open_data;

	for (i = 0; i < RAW_DATA_SIZE / 2; i++) {
		node_data = (opendata[1] << 8) | opendata[0];

		mp_data->open_node_val[i] = node_data;

		opendata += 2;
	}

	for (i = 0; i < RAW_DATA_SIZE / 2; i++) {
		node_data = mp_data->open_node_val[i];
/* GTP_DEBUG("i:%d open node_data:%d", i, node_data); */
		jump = 0;

		for (j = 0; j < EMPTY_NUM; j++) {
			if (i == empty_place[j]) {
				GTP_DEBUG("empty node %d need not judge.", i);
				jump = 1;
			}
		}

		if (!jump) {
			if (node_data < mp_data->open_min) {
				GTP_ERROR("i: %d node_data %d < open_min test fail!", i, node_data);

				return -EPERM;
			}
		}
	}

	return 0;
}

int gcore_mp_test_item_open_reply(u8 *buf, int len)
{
	int ret = 0;

	if (buf == NULL) {
		GTP_ERROR("receive buffer is null!");
		return -EPERM;
	}

	ret = gcore_bus_read(buf, len);
	if (ret) {
		GTP_ERROR("read mp test open data error.");
		return -EPERM;
	}

	mp_test_fn.wait_int = false;
	complete(&mp_test_complete);

	return 0;
}

void mp_wait_int_set_fail(void)
{
    mp_test_fn.wait_int = false;
}

int gcore_mp_test_item_short(struct gcore_mp_data *mp_data)
{
	u8 cb_high = (u8) (mp_data->short_cb >> 8);
	u8 cb_low = (u8) mp_data->short_cb;
	u8 cmd[] = { 0x40, 0xAA, 0x00, 0x32, 0x73, 0x71, cb_high, cb_low, 0x02 };
	struct gcore_dev *gdev = mp_data->gdev;
	int ret = 0;
	int i = 0;
	int j = 0;
	u8 *shortdata = NULL;
	u16 node_data = 0;
	bool jump = 0;

	GTP_DEBUG("gcore mp test item short");

	gdev->fw_event = FW_READ_SHORT;
	gdev->firmware = mp_data->short_data;
#ifdef CONFIG_TOUCH_DRIVER_INTERFACE_SPI
	gdev->fw_xfer = 2048;
#else
	gdev->fw_xfer = RAW_DATA_SIZE;
#endif

	mutex_lock(&gdev->transfer_lock);

	mp_test_fn.wait_int = true;

	ret = gcore_bus_write(cmd, sizeof(cmd));
	if (ret) {
		GTP_ERROR("write mp test open cmd fail!");
		return -EPERM;
	}

	mutex_unlock(&gdev->transfer_lock);

	if (!wait_for_completion_interruptible_timeout(&mp_test_complete, 1 * HZ)) {
		GTP_ERROR("mp test item short timeout.");
		mp_test_fn.wait_int = false;
		return EPERM;
	}

	shortdata = mp_data->short_data;

	for (i = 0; i < RAW_DATA_SIZE / 2; i++) {
		node_data = (shortdata[1] << 8) | shortdata[0];

		mp_data->short_node_val[i] = node_data;

		shortdata += 2;
	}

	for (i = 0; i < RAW_DATA_SIZE / 2; i++) {
		node_data = mp_data->short_node_val[i];
/* GTP_DEBUG("i:%d short node_data:%d", i, node_data); */
		jump = 0;

		for (j = 0; j < EMPTY_NUM; j++) {
			if (i == empty_place[j]) {
				GTP_DEBUG("empty node %d need not judge.", i);
				jump = 1;
			}
		}

		if (!jump) {
			if (node_data < mp_data->short_min) {
				GTP_ERROR("i: %d node_data %d < short_min test fail!", i, node_data);

				return -EPERM;
			}
		}
	}

	return 0;
}

int gcore_mp_test_item_rawdata(struct gcore_mp_data *mp_data)
{
	u8 cmd[] = { 0x40, 0xAA, 0x00, 0x32, 0x73, 0x71, 0x00, 0x00, 0x03 };
	struct gcore_dev *gdev = mp_data->gdev;
	u8 *rawdata = NULL;
	int ret = 0;
	int i = 0;
	int j = 0;
	u16 node_data = 0;
//	u16 *rndata = mp_data->rawdata_node_val;
//	u16 *dec_value = NULL;
//	int rows = RAWDATA_ROW;
//	int cols = RAWDATA_COLUMN;
	bool jump = 0;


	GTP_DEBUG("gcore mp test item rawdata");

	gdev->fw_event = FW_READ_RAWDATA;
	gdev->firmware = mp_data->raw_data;
#ifdef CONFIG_TOUCH_DRIVER_INTERFACE_SPI
	gdev->fw_xfer = 2048;
#else
	gdev->fw_xfer = RAW_DATA_SIZE;
#endif

	mutex_lock(&gdev->transfer_lock);

	mp_test_fn.wait_int = true;

	ret = gcore_bus_write(cmd, sizeof(cmd));
	if (ret) {
		GTP_ERROR("write mp test open cmd fail!");
		return -EPERM;
	}

	mutex_unlock(&gdev->transfer_lock);

	if (!wait_for_completion_interruptible_timeout(&mp_test_complete, 3 * HZ)) {
		GTP_ERROR("mp test item rawdata timeout.");
		mp_test_fn.wait_int = false;
		return EPERM;
	}

	rawdata = mp_data->raw_data;

	for (i = 0; i < RAW_DATA_SIZE / 2; i++) {
		node_data = (rawdata[1] << 8) | rawdata[0];

		mp_data->rawdata_node_val[i] = node_data;

		rawdata += 2;
	}
	/* cal rawdata node max and min */
	mp_data->rawdata_node_max = mp_data->rawdata_node_val[0];
	mp_data->rawdata_node_min = mp_data->rawdata_node_val[0];
	for (i = 0; i < RAW_DATA_SIZE / 2; i++) {
		if (mp_data->rawdata_node_val[i] > mp_data->rawdata_node_max) {
			mp_data->rawdata_node_max = mp_data->rawdata_node_val[i];
		}

		if (mp_data->rawdata_node_val[i] < mp_data->rawdata_node_min) {
			mp_data->rawdata_node_min = mp_data->rawdata_node_val[i];
		}
	}

#if MP_RAWDATA_DEC_TEST
	dec_value = mp_data->rawdata_dec_h;
	rndata = mp_data->rawdata_node_val;
	for (j = 0; j < rows; j++) {
		for (i = 0; i < cols; i++) {
			if (i == cols - 1) {
				dec_value[i] = abs(rndata[i - 1] - rndata[i]) * 100 / rndata[i];
			} else {
				dec_value[i] = abs(rndata[i + 1] - rndata[i]) * 100 / rndata[i];
			}

//			GTP_DEBUG("rawdata cal i:%d val:%d", i, dec_value);
		}

		rndata += cols;
		dec_value += cols;
	}
	/* cal rawdata node max and min */
	mp_data->rawdata_dech_max = mp_data->rawdata_dec_h[0];
	mp_data->rawdata_dech_min = mp_data->rawdata_dec_h[0];
	for (i = 0; i < RAW_DATA_SIZE / 2; i++) {
		if (mp_data->rawdata_dec_h[i] > mp_data->rawdata_dech_max) {
			mp_data->rawdata_dech_max = mp_data->rawdata_dec_h[i];
		}

		if (mp_data->rawdata_dec_h[i] < mp_data->rawdata_dech_min) {
			mp_data->rawdata_dech_min = mp_data->rawdata_dec_h[i];
		}
	}

	dec_value = mp_data->rawdata_dec_l;
	rndata = mp_data->rawdata_node_val;
	for (j = 0; j < cols; j++) {
		for (i = 0; i < rows; i++) {
			if (i == rows - 1) {
				dec_value[i*cols+j] = abs(rndata[i*cols+j-cols] - rndata[i*cols+j]) * 100 / rndata[i*cols+j];
			} else {
				dec_value[i*cols+j] = abs(rndata[i*cols+j+cols] - rndata[i*cols+j]) * 100 / rndata[i*cols+j];
			}

//			GTP_DEBUG("rawdata cal i:%d val:%d", i, dec_value);
		}

	}
	/* cal rawdata node max and min */
	mp_data->rawdata_decl_max = mp_data->rawdata_dec_l[0];
	mp_data->rawdata_decl_min = mp_data->rawdata_dec_l[0];
	for (i = 0; i < RAW_DATA_SIZE / 2; i++) {
		if (mp_data->rawdata_dec_l[i] > mp_data->rawdata_decl_max) {
			mp_data->rawdata_decl_max = mp_data->rawdata_dec_l[i];
		}

		if (mp_data->rawdata_dec_l[i] < mp_data->rawdata_decl_min) {
			mp_data->rawdata_decl_min = mp_data->rawdata_dec_l[i];
		}
	}
#endif

#if MP_RAWDATA_AVG_TEST
	dec_value = mp_data->rawdata_avg_h;
	rndata = mp_data->rawdata_node_val;
	for (j = 0; j < rows; j++) {
		int max = 0;
		int min = 0;
		int avg = 0;
		int sum = 0;
		for (i = 0; i < cols; i++) {
			if (rndata[i] > rndata[max]) {
				max = i;
			}

			if (rndata[i] < rndata[min]) {
				min = i;
			}
		}

//		GTP_DEBUG("avgh max:%d min:%d", max, min);

		if (max == min) {
			avg = rndata[max];
		} else {
			for (i = 0; i < cols; i++) {
				if (i != max && i != min) {
					sum += rndata[i];
				}
			}
			avg = sum/(cols-2);
		}

//		GTP_DEBUG("avgh sum:%d avg:%d", sum, avg);

		for (i = 0; i < cols; i++) {
			dec_value[i] = abs(avg - rndata[i]) * 100 / rndata[i];
		}

//			GTP_DEBUG("rawdata cal i:%d val:%d", i, dec_value);

		rndata += cols;
		dec_value += cols;
	}


	dec_value = mp_data->rawdata_avg_l;
	rndata = mp_data->rawdata_node_val;
	for (j = 0; j < cols; j++) {
		int max = j;
		int min = j;
		int avg = 0;
		int sum = 0;
		for (i = 0; i < rows; i++) {
			if (rndata[i*cols+j] > rndata[max]) {
				max = i*cols+j;
			}

			if (rndata[i*cols+j] < rndata[min]) {
				min = i*cols+j;
			}
		}

//		GTP_DEBUG("avgl max:%d min:%d", max/18, min/18);

		if (max == min) {
			avg = rndata[max];
		} else {
			for (i = 0; i < rows; i++) {
				if ((i*cols+j) != max && (i*cols+j) != min) {
					sum += rndata[i*cols+j];
				}
			}
			avg = sum/(rows-2);
		}

//		GTP_DEBUG("avgl sum:%d avg:%d", sum, avg);

		for (i = 0; i < rows; i++) {
			dec_value[i*cols+j] = abs(avg - rndata[i*cols+j]) * 100 / rndata[i*cols+j];
		}

//			GTP_DEBUG("rawdata cal i:%d val:%d", i, dec_value);
	}
#endif

	for (i = 0; i < RAW_DATA_SIZE / 2; i++) {

		jump = 0;
		for (j = 0; j < EMPTY_NUM; j++) {
			if (i == empty_place[j]) {
				GTP_DEBUG("empty node %d need not judge.", i);
				jump = 1;
			}
		}

		if (!jump) {
			node_data = mp_data->rawdata_node_val[i];
			if ((node_data > mp_data->rawdata_range_max) ||
					(node_data < mp_data->rawdata_range_min)) {
				GTP_ERROR("rawdata range %d data %d not in range test fail!", i, node_data);

				return -EPERM;
			}
#if MP_RAWDATA_DEC_TEST
			node_data = mp_data->rawdata_dec_h[i];
			if (node_data > mp_data->rawdata_per) {
				GTP_ERROR("dech %d %d n_data %d > rawdata_per test fail!",
					i, mp_data->rawdata_node_val[i], node_data);

				return -EPERM;
			}

			node_data = mp_data->rawdata_dec_l[i];
			if (node_data > mp_data->rawdata_per) {
				GTP_ERROR("decl %d %d n_data %d > rawdata_per test fail!",
					i, mp_data->rawdata_node_val[i], node_data);

				return -EPERM;
			}
#endif
#if MP_RAWDATA_AVG_TEST
			node_data = mp_data->rawdata_avg_h[i];
			if (node_data > mp_data->rawdata_per) {
				GTP_ERROR("avgh %d %d n_data %d > rawdata_per test fail!",
					i, mp_data->rawdata_node_val[i], node_data);

				return -EPERM;
			}

			node_data = mp_data->rawdata_avg_l[i];
			if (node_data > mp_data->rawdata_per) {
				GTP_ERROR("avgl %d %d n_data %d > rawdata_per test fail!",
					i, mp_data->rawdata_node_val[i], node_data);

				return -EPERM;
			}
#endif
		}
	}

	return 0;
}

int gcore_mp_test_item_noise(struct gcore_mp_data *mp_data)
{
	u8 cmd[] = { 0x40, 0xAA, 0x00, 0x32, 0x73, 0x71, 0x00, 0x00, 0x04 };
	struct gcore_dev *gdev = mp_data->gdev;
	u8 *noisedata = NULL;
	int ret = 0;
	int i = 0;
	int j = 0;
	u16 node_data = 0;
	bool jump = 0;

	GTP_DEBUG("gcore mp test item noise");

	gdev->fw_event = FW_READ_NOISE;
	gdev->firmware = mp_data->noise_data;
#ifdef CONFIG_TOUCH_DRIVER_INTERFACE_SPI
	gdev->fw_xfer = 2048;
#else
	gdev->fw_xfer = RAW_DATA_SIZE;
#endif

	mutex_lock(&gdev->transfer_lock);

	mp_test_fn.wait_int = true;

	ret = gcore_bus_write(cmd, sizeof(cmd));
	if (ret) {
		GTP_ERROR("write mp test open cmd fail!");
		return -EPERM;
	}

	mutex_unlock(&gdev->transfer_lock);

	if (!wait_for_completion_interruptible_timeout(&mp_test_complete, 3 * HZ)) {
		GTP_ERROR("mp test item noise timeout.");
		mp_test_fn.wait_int = false;
		return EPERM;
	}

	noisedata = mp_data->noise_data;

	for (i = 0; i < RAW_DATA_SIZE / 2; i++) {
		node_data = (noisedata[1] << 8) | noisedata[0];

		node_data = node_data / 2;

		mp_data->noise_node_val[i] = node_data;

		noisedata += 2;
	}

	for (i = 0; i < RAW_DATA_SIZE / 2; i++) {
		node_data = mp_data->noise_node_val[i];
		jump = 0;

		for (j = 0; j < EMPTY_NUM; j++) {
			if (i == empty_place[j]) {
				GTP_DEBUG("empty node %d need not judge.", i);
				jump = 1;
			}
		}

		if (!jump) {
			if (node_data > mp_data->peak_to_peak) {
				GTP_ERROR("i: %d node_data %d > peak_to_peak test fail!", \
										i, node_data);

				return -EPERM;
			}
		}
	}

	return 0;
}

static int mp_test_event_handler(void *p)
{
	struct gcore_dev *gdev = (struct gcore_dev *)p;
	struct sched_param param = {.sched_priority = 4 };

	sched_setscheduler(current, SCHED_RR, &param);
	do {
		set_current_state(TASK_INTERRUPTIBLE);

		wait_event_interruptible(gdev->wait, mp_test_fn.event_flag == true);
		mp_test_fn.event_flag = false;

		if (mutex_is_locked(&gdev->transfer_lock)) {
			GTP_DEBUG("fw event is locked, ignore");
			continue;
		}

		mutex_lock(&gdev->transfer_lock);

		GTP_DEBUG("mp_test_event_handler enter.");

		switch (gdev->fw_event) {
		case FW_READ_OPEN:
			gcore_mp_test_item_open_reply(gdev->firmware, gdev->fw_xfer);
			break;

		case FW_READ_SHORT:
			gcore_mp_test_item_open_reply(gdev->firmware, gdev->fw_xfer);
			break;

		case FW_READ_RAWDATA:
			gcore_mp_test_item_open_reply(gdev->firmware, gdev->fw_xfer);
			break;

		case FW_READ_NOISE:
			gcore_mp_test_item_open_reply(gdev->firmware, gdev->fw_xfer);
			break;

		default:
			break;
		}

		mutex_unlock(&gdev->transfer_lock);

	} while (!kthread_should_stop());

	return 0;
}

int gcore_alloc_mp_test_mem(struct gcore_mp_data *mp_data)
{
	if (mp_data->test_open) {
		if (mp_data->open_data == NULL) {
#ifdef CONFIG_TOUCH_DRIVER_INTERFACE_SPI
			mp_data->open_data = kzalloc(2048, GFP_KERNEL);
#else
			mp_data->open_data = kzalloc(RAW_DATA_SIZE, GFP_KERNEL);
#endif
			if (IS_ERR_OR_NULL(mp_data->open_data)) {
				GTP_ERROR("allocate mp test open_data mem fail!");
				return -ENOMEM;
			}
		}

		memset(mp_data->open_data, 0, RAW_DATA_SIZE);

		if (mp_data->open_node_val == NULL) {
			mp_data->open_node_val = (u16 *) kzalloc(RAW_DATA_SIZE, GFP_KERNEL);
			if (IS_ERR_OR_NULL(mp_data->open_node_val)) {
				GTP_ERROR("allocate mp test open_node_val mem fail!");
				return -ENOMEM;
			}
		}

		memset(mp_data->open_node_val, 0, RAW_DATA_SIZE);

	}

	if (mp_data->test_short) {
		if (mp_data->short_data == NULL) {
#ifdef CONFIG_TOUCH_DRIVER_INTERFACE_SPI
			mp_data->short_data = kzalloc(2048, GFP_KERNEL);
#else
			mp_data->short_data = kzalloc(RAW_DATA_SIZE, GFP_KERNEL);
#endif
			if (IS_ERR_OR_NULL(mp_data->short_data)) {
				GTP_ERROR("allocate mp test short_data mem fail!");
				return -ENOMEM;
			}
		}

		memset(mp_data->short_data, 0, RAW_DATA_SIZE);

		if (mp_data->short_node_val == NULL) {
			mp_data->short_node_val = (u16 *) kzalloc(RAW_DATA_SIZE, GFP_KERNEL);
			if (IS_ERR_OR_NULL(mp_data->short_node_val)) {
				GTP_ERROR("allocate mp test short_node_val mem fail!");
				return -ENOMEM;
			}
		}

		memset(mp_data->short_node_val, 0, RAW_DATA_SIZE);

	}

	if (mp_data->test_rawdata) {
		if (mp_data->raw_data == NULL) {
#ifdef CONFIG_TOUCH_DRIVER_INTERFACE_SPI
			mp_data->raw_data = kzalloc(2048, GFP_KERNEL);
#else
			mp_data->raw_data = kzalloc(RAW_DATA_SIZE, GFP_KERNEL);
#endif
			if (IS_ERR_OR_NULL(mp_data->raw_data)) {
				GTP_ERROR("allocate mp test raw_data mem fail!");
				return -ENOMEM;
			}
		}

		memset(mp_data->raw_data, 0, RAW_DATA_SIZE);

		if (mp_data->rawdata_node_val == NULL) {
			mp_data->rawdata_node_val = (u16 *) kzalloc(RAW_DATA_SIZE, GFP_KERNEL);
			if (IS_ERR_OR_NULL(mp_data->rawdata_node_val)) {
				GTP_ERROR("allocate mp test rawdata_node_val mem fail!");
				return -ENOMEM;
			}
		}

		memset(mp_data->rawdata_node_val, 0, RAW_DATA_SIZE);

#if MP_RAWDATA_DEC_TEST
		if (mp_data->rawdata_dec_h == NULL) {
			mp_data->rawdata_dec_h = (u16 *) kzalloc(RAW_DATA_SIZE, GFP_KERNEL);
			if (IS_ERR_OR_NULL(mp_data->rawdata_dec_h)) {
				GTP_ERROR("allocate mp test rawdata dec h data mem fail!");
				return -ENOMEM;
			}
		}

		memset(mp_data->rawdata_dec_h, 0, RAW_DATA_SIZE);

		if (mp_data->rawdata_dec_l == NULL) {
			mp_data->rawdata_dec_l = (u16 *) kzalloc(RAW_DATA_SIZE, GFP_KERNEL);
			if (IS_ERR_OR_NULL(mp_data->rawdata_dec_l)) {
				GTP_ERROR("allocate mp test rawdata dec l data mem fail!");
				return -ENOMEM;
			}
		}

		memset(mp_data->rawdata_dec_l, 0, RAW_DATA_SIZE);
#endif

#if MP_RAWDATA_AVG_TEST
		if (mp_data->rawdata_avg_h == NULL) {
			mp_data->rawdata_avg_h = (u16 *) kzalloc(RAW_DATA_SIZE, GFP_KERNEL);
			if (IS_ERR_OR_NULL(mp_data->rawdata_avg_h)) {
				GTP_ERROR("allocate mp test rawdata avg h data mem fail!");
				return -ENOMEM;
			}
		}

		memset(mp_data->rawdata_avg_h, 0, RAW_DATA_SIZE);

		if (mp_data->rawdata_avg_l == NULL) {
			mp_data->rawdata_avg_l = (u16 *) kzalloc(RAW_DATA_SIZE, GFP_KERNEL);
			if (IS_ERR_OR_NULL(mp_data->rawdata_avg_l)) {
				GTP_ERROR("allocate mp test rawdata avg l data mem fail!");
				return -ENOMEM;
			}
		}

		memset(mp_data->rawdata_avg_l, 0, RAW_DATA_SIZE);
#endif
	}

	if (mp_data->test_noise) {
		if (mp_data->noise_data == NULL) {
#ifdef CONFIG_TOUCH_DRIVER_INTERFACE_SPI
			mp_data->noise_data = kzalloc(2048, GFP_KERNEL);
#else
			mp_data->noise_data = kzalloc(RAW_DATA_SIZE, GFP_KERNEL);
#endif
			if (IS_ERR_OR_NULL(mp_data->noise_data)) {
				GTP_ERROR("allocate mp test noise_data mem fail!");
				return -ENOMEM;
			}
		}

		memset(mp_data->noise_data, 0, RAW_DATA_SIZE);

		if (mp_data->noise_node_val == NULL) {
			mp_data->noise_node_val = (u16 *) kzalloc(RAW_DATA_SIZE, GFP_KERNEL);
			if (IS_ERR_OR_NULL(mp_data->noise_node_val)) {
				GTP_ERROR("allocate mp test noise_node_val mem fail!");
				return -ENOMEM;
			}
		}

		memset(mp_data->noise_node_val, 0, RAW_DATA_SIZE);

	}

	return 0;
}

void gcore_free_mp_test_mem(struct gcore_mp_data *mp_data)
{
	if (mp_data->test_open) {

		kfree(mp_data->open_data);

		kfree(mp_data->open_node_val);
	}

	if (mp_data->test_short) {

		kfree(mp_data->short_data);

		kfree(mp_data->short_node_val);
	}
}

static int firstline = 1;
int dump_mp_data_row_to_buffer(char *buf, size_t size, const u16 *data,
			       int cols, const char *prefix, const char *suffix, char seperator)
{
	int c, j, hole, count = 0;

	if (prefix) {
		count += scnprintf(buf, size, "%s", prefix);
	}

		for (c = 0; c < cols; c++) {
		if (!firstline) {
			count += scnprintf(buf + count, size - count, "%4u%c ", data[c], seperator);
		}else{
			//GTP_DEBUG("this is first line.");
			for (j = 0; j < EMPTY_NUM; j++) {
				if (c == empty_place[j]) {
					//GTP_DEBUG("empty node %d is blank.", empty_place[j]);
					hole = 1;
					break;
				}else{
					//GTP_DEBUG("empty node is normal.");
					hole = 0;
				}
			}

			if(hole == 1){
				count += scnprintf(buf + count, size - count, " %c", seperator);
				GTP_DEBUG("empty node %d is blank.", empty_place[j]);
			}else if(hole == 0){
				count += scnprintf(buf + count, size - count, "%4u%c ", data[c], seperator);
			}

		}
	}

	if (suffix) {
		count += scnprintf(buf + count, size - count, "%s", suffix);
	}

	return count;

}

int dump_diff_data_row_to_buffer(char *buf, size_t size, const s16 *data,
			       int cols, const char *prefix, const char *suffix, char seperator)
{
	int c, count = 0;

	if (prefix) {
		count += scnprintf(buf, size, "%s", prefix);
	}

	for (c = 0; c < cols; c++) {
		count += scnprintf(buf + count, size - count, "%4d%c ", data[c], seperator);
	}

	if (suffix) {
		count += scnprintf(buf + count, size - count, "%s", suffix);
	}

	return count;

}

int dump_demo_data_row_to_buffer(char *buf, size_t size, const u8 *data,
				  int cols, const char *prefix, const char *suffix, char seperator)
{
   int c, count = 0;

   if (prefix) {
	   count += scnprintf(buf, size, "%s", prefix);
   }

   for (c = 0; c < cols; c++) {
	   count += scnprintf(buf + count, size - count, "%02x%c", data[c], seperator);
   }

   if (suffix) {
	   count += scnprintf(buf + count, size - count, "%s", suffix);
   }

   return count;

}

int dump_excel_head_row_to_buffer(char *buf, size_t size,
			int cols, const char *prefix, const char *suffix, char seperator)
{
	int c, count = 0;

	if (prefix) {
		count += scnprintf(buf, size, "%s", prefix);
	}

	for (c = 1; c <= cols; c++) {
		count += scnprintf(buf + count, size - count, "X%d%c ", c, seperator);
	}

	if (suffix) {
		count += scnprintf(buf + count, size - count, "%s", suffix);
	}

	return count;

}

static loff_t pos;
int dump_mp_data_to_csv_file(const char *filepath, int flags,
			const u16 *data, int rows, int cols, enum MP_DATA_TYPE types)
{
	struct file *file;
	int r = 0;
	int ret = 0;
	const u16 *n_val = data;
	char linebuf[256];
	char y_prefix[10];
	char topic[30];
	int len;

	file = filp_open(filepath, flags, 0666);
	if (IS_ERR(file)) {
		GTP_ERROR("Open file %s failed %ld", filepath, PTR_ERR(file));
		return -EPERM;
	}

	switch (types) {
		case RAW_DATA:
			GTP_DEBUG("dump Raw data to csv file");
			strncpy(topic, "Rawdata:, ", sizeof(topic));
			break;

		case RAWDATA_DECH:
			strncpy(topic, "rawdata_per_h:, ", sizeof(topic));
			break;

		case RAWDATA_DECL:
			strncpy(topic, "rawdata_per_l:, ", sizeof(topic));
			break;

		case NOISE_DATA:
			GTP_DEBUG("dump Noise data to csv file");
			strncpy(topic, "Noise:, ", sizeof(topic));
			break;

		case OPEN_DATA:
			GTP_DEBUG("dump Open data to csv file");
			strncpy(topic, "Open:, ", sizeof(topic));
			break;

		case SHORT_DATA:
			GTP_DEBUG("dump Short data to csv file");
			strncpy(topic, "Short:, ", sizeof(topic));
			break;

		default:
			strncpy(topic, "Rawdata:, ", sizeof(topic));
			break;
	}

	len = dump_excel_head_row_to_buffer(linebuf, sizeof(linebuf),
						cols, topic, "\n", ',');
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	ret = kernel_write(file, linebuf, len, &pos);
#else
	ret = kernel_write(file, linebuf, len, pos);
	pos += len;
#endif

	for (r = 0; r < rows; r++) {
		scnprintf(y_prefix, sizeof(y_prefix), "Y%d, ", r+1);
		len = dump_mp_data_row_to_buffer(linebuf, sizeof(linebuf),
						n_val, cols, y_prefix, "\n", ',');
		firstline = 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
		ret = kernel_write(file, linebuf, len, &pos);
#else
		ret = kernel_write(file, linebuf, len, pos);
		pos += len;
#endif
		if (ret != len) {
			GTP_ERROR("write to file %s failed %d", filepath, ret);
			goto fail1;
		}

		n_val += cols;
	}
	firstline = 1;


	switch (types) {
	case RAW_DATA:
		len = scnprintf(linebuf, sizeof(linebuf), "Max:, %4u, ,Min:, %4u, ,Threshold:, %4u, %4u, \n",
				g_mp_data->rawdata_node_max, g_mp_data->rawdata_node_min, g_mp_data->rawdata_range_min,
				g_mp_data->rawdata_range_max);
		break;

	case RAWDATA_DECH:
		len = scnprintf(linebuf, sizeof(linebuf),
				"Max:, %4u, ,Min:, %4u, ,Threshold:, %4u%%, \n",
				g_mp_data->rawdata_dech_max, g_mp_data->rawdata_dech_min,
				g_mp_data->rawdata_per);
		break;

	case RAWDATA_DECL:
		len = scnprintf(linebuf, sizeof(linebuf),
				"Max:, %4u, ,Min:, %4u, ,Threshold:, %4u%%, \n",
				g_mp_data->rawdata_decl_max, g_mp_data->rawdata_decl_min,
				g_mp_data->rawdata_per);
		break;

	case OPEN_DATA:
		len = scnprintf(linebuf, sizeof(linebuf), "Threshold:, %4u, \n", \
				g_mp_data->open_min);
		break;

	case SHORT_DATA:
		len = scnprintf(linebuf, sizeof(linebuf), "Threshold:, %4u, \n", \
				g_mp_data->short_min);
		break;

	case NOISE_DATA:
		len = scnprintf(linebuf, sizeof(linebuf), "Threshold:, %4u, \n", \
				g_mp_data->peak_to_peak);
		break;

	default:
		len = 0;
		break;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	ret = kernel_write(file, linebuf, len, &pos);
#else
	ret = kernel_write(file, linebuf, len, pos);
	pos += len;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	ret = kernel_write(file, "\n", 1, &pos);
#else
	ret = kernel_write(file, "\n", 1, pos);
	pos++;
#endif
	if (ret != 1) {
		GTP_ERROR("write newline to file %s failed %d", filepath, ret);
		goto fail1;
	}

fail1:
	filp_close(file, NULL);

	return ret;

}

int dump_mp_info_to_csv_file(const char *filepath, int flags)
{
	struct file *file;
	int ret = 0;

	static char linebuf[256];
	int len;
#if defined(CONFIG_ENABLE_CHIP_TYPE_GC7371)
	u8 icname[10] = "GC7371";
#elif defined(CONFIG_ENABLE_CHIP_TYPE_GC7271)
	u8 icname[10] = "GC7271";
#elif defined(CONFIG_ENABLE_CHIP_TYPE_GC7202)
	u8 icname[10] = "GC7202";
#elif defined(CONFIG_ENABLE_CHIP_TYPE_GC7372)
	u8 icname[10] = "GC7372";
#elif defined(CONFIG_ENABLE_CHIP_TYPE_GC7302)
	u8 icname[10] = "GC7302";
#elif defined(CONFIG_ENABLE_CHIP_TYPE_GC7202H_GC7272)
	u8 icname[10] = "GC7202H";
#endif
#if defined(CONFIG_TOUCH_DRIVER_INTERFACE_I2C)
	u8 intef[5] = "I2C";
	u8 speed[10];
#elif defined(CONFIG_TOUCH_DRIVER_INTERFACE_SPI)
	u8 intef[5] = "SPI";
	u8 speed[10];
#endif

	pos = 0;
	file = filp_open(filepath, flags, 0666);
	if (IS_ERR(file)) {
		GTP_ERROR("Open file %s failed %ld", filepath, PTR_ERR(file));
		return -EPERM;
	}

	len = scnprintf(linebuf, sizeof(linebuf), "INFO:, \n");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	ret = kernel_write(file, linebuf, len, &pos);
#else
	ret = kernel_write(file, linebuf, len, pos);
	pos += len;
#endif

	len = scnprintf(linebuf, sizeof(linebuf), "GTP Driver version:, V%s\n", TOUCH_DRIVER_RELEASE_VERISON);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	ret = kernel_write(file, linebuf, len, &pos);
#else
	ret = kernel_write(file, linebuf, len, pos);
	pos += len;
#endif

	len = scnprintf(linebuf, sizeof(linebuf), "MP bin version:, 0x%02x%02x.0x%02x%02x\n",
		g_mp_data->mpbin_ver[1], g_mp_data->mpbin_ver[0],g_mp_data->mpbin_ver[3],g_mp_data->mpbin_ver[2]);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	ret = kernel_write(file, linebuf, len, &pos);
#else
	ret = kernel_write(file, linebuf, len, pos);
	pos += len;
#endif

	len = scnprintf(linebuf, sizeof(linebuf), "IC name:, %s\n", icname);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	ret = kernel_write(file, linebuf, len, &pos);
#else
	ret = kernel_write(file, linebuf, len, pos);
	pos += len;
#endif

	len = scnprintf(linebuf, sizeof(linebuf), "Interface mode:, %s\n", intef);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	ret = kernel_write(file, linebuf, len, &pos);
#else
	ret = kernel_write(file, linebuf, len, pos);
	pos += len;
#endif

#if defined(CONFIG_TOUCH_DRIVER_INTERFACE_I2C)
		scnprintf(speed, sizeof(speed), "400K");
#elif defined(CONFIG_TOUCH_DRIVER_INTERFACE_SPI)
		scnprintf(speed, sizeof(speed), "%u", g_mp_data->gdev->bus_device->max_speed_hz);
#endif
	len = scnprintf(linebuf, sizeof(linebuf), "Interface speed:, %s\n", speed);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	ret = kernel_write(file, linebuf, len, &pos);
#else
	ret = kernel_write(file, linebuf, len, pos);
	pos += len;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	ret = kernel_write(file, "\n", 1, &pos);
#else
	ret = kernel_write(file, "\n", 1, pos);
	pos++;
#endif
	if (ret != 1) {
		GTP_ERROR("write newline to file %s failed %d", filepath, ret);
		goto fail1;
	}

	len = scnprintf(linebuf, sizeof(linebuf), "RESULT:, \n");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	ret = kernel_write(file, linebuf, len, &pos);
#else
	ret = kernel_write(file, linebuf, len, pos);
	pos += len;
#endif

	len = scnprintf(linebuf, sizeof(linebuf), "MP Test result:, %s\n", \
					g_mp_data->all_test_result ? "FAIL" : "PASS");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	ret = kernel_write(file, linebuf, len, &pos);
#else
	ret = kernel_write(file, linebuf, len, pos);
	pos += len;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	ret = kernel_write(file, "\n", 1, &pos);
#else
	ret = kernel_write(file, "\n", 1, pos);
	pos++;
#endif
	if (ret != 1) {
		GTP_ERROR("write newline to file %s failed %d", filepath, ret);
		goto fail1;
	}

	len = scnprintf(linebuf, sizeof(linebuf), "1-Chip Test Result:, %s\n", \
					g_mp_data->chip_id_test_result ? "FAIL" : "PASS");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	ret = kernel_write(file, linebuf, len, &pos);
#else
	ret = kernel_write(file, linebuf, len, pos);
	pos += len;
#endif

	len = scnprintf(linebuf, sizeof(linebuf), "2-MPbin Test Result:, %s\n", \
					g_mp_data->mp_update_result ? "FAIL" : "PASS");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	ret = kernel_write(file, linebuf, len, &pos);
#else
	ret = kernel_write(file, linebuf, len, pos);
	pos += len;
#endif

	len = scnprintf(linebuf, sizeof(linebuf), "3-Open Test Result:, %s, %s\n", \
					g_mp_data->open_test_result ? "FAIL" : "PASS", \
					(g_mp_data->open_test_result > 0) ? "TIMEOUT" : "");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	ret = kernel_write(file, linebuf, len, &pos);
#else
	ret = kernel_write(file, linebuf, len, pos);
	pos += len;
#endif

	len = scnprintf(linebuf, sizeof(linebuf), "4-Short Test Result:, %s, %s\n", \
					g_mp_data->short_test_result ? "FAIL" : "PASS", \
					(g_mp_data->short_test_result > 0) ? "TIMEOUT" : "");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	ret = kernel_write(file, linebuf, len, &pos);
#else
	ret = kernel_write(file, linebuf, len, pos);
	pos += len;
#endif

	len = scnprintf(linebuf, sizeof(linebuf), "5-Rawdata Test Result:, %s, %s\n", \
					g_mp_data->rawdata_test_result ? "FAIL" : "PASS", \
					(g_mp_data->rawdata_test_result > 0) ? "TIMEOUT" : "");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	ret = kernel_write(file, linebuf, len, &pos);
#else
	ret = kernel_write(file, linebuf, len, pos);
	pos += len;
#endif

	len = scnprintf(linebuf, sizeof(linebuf), "6-Noise Test Result:, %s, %s\n", \
					g_mp_data->noise_test_result ? "FAIL" : "PASS", \
					(g_mp_data->noise_test_result > 0) ? "TIMEOUT" : "");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	ret = kernel_write(file, linebuf, len, &pos);
#else
	ret = kernel_write(file, linebuf, len, pos);
	pos += len;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	ret = kernel_write(file, "\n", 1, &pos);
#else
	ret = kernel_write(file, "\n", 1, pos);
	pos++;
#endif
	if (ret != 1) {
		GTP_ERROR("write newline to file %s failed %d", filepath, ret);
		goto fail1;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	ret = kernel_write(file, "\n", 1, &pos);
#else
	ret = kernel_write(file, "\n", 1, pos);
	pos++;
#endif
	if (ret != 1) {
		GTP_ERROR("write newline to file %s failed %d", filepath, ret);
		goto fail1;
	}

fail1:
	filp_close(file, NULL);

	return ret;

}

int gcore_write_buff_to_file(struct file *file, char *buff, int len)
{
	int ret = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	ret = kernel_write(file, buff, len, &pos);
#else
	ret = kernel_write(file, buff, len, pos);
	pos += len;
#endif
	if (ret != len) {
		GTP_ERROR("write to file failed %d", ret);
		return -EPERM;
	}

	return 0;
}

int gcore_write_blank_line_to_file(struct file *file)
{
	int ret = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	ret = kernel_write(file, "\n", 1, &pos);
#else
	ret = kernel_write(file, "\n", 1, pos);
	pos++;
#endif
	if (ret != 1) {
		GTP_ERROR("write newline to file failed %d", ret);
		return -EPERM;
	}

	return 0;
}

int frame_count;
u8 demolog_path[200] = "/sdcard/demodata.csv";

int dump_demo_data_to_csv_file(const u8 *data, int rows, int cols)
{
	struct file *file;
	char linebuf[256];
	const u8 *n_val = data;
	int len;
	int r = 0;
	int ret = 0;

	GTP_DEBUG("dump demo data to csv file");

	frame_count++;

	file = filp_open(demolog_path, O_RDWR | O_CREAT, 0666);
    if (IS_ERR(file)) {
        GTP_ERROR("Open file %s failed %ld", demolog_path, PTR_ERR(file));
        return -EPERM;
    }

	len = scnprintf(linebuf, sizeof(linebuf), "Frame_%d:, \n", frame_count);
	if (gcore_write_buff_to_file(file, linebuf, len)) {
		goto fail;
	}

	for (r = 0; r < rows; r++) {
		len = dump_demo_data_row_to_buffer(linebuf, sizeof(linebuf),
						n_val, cols, NULL, "\n", ',');

		if (gcore_write_buff_to_file(file, linebuf, len)) {
			goto fail;
		}

		n_val += cols;
	}

	len = dump_demo_data_row_to_buffer(linebuf, sizeof(linebuf), n_val, 5, NULL, "\n", ',');
	if (gcore_write_buff_to_file(file, linebuf, len)) {
		goto fail;
	}

	if (gcore_write_blank_line_to_file(file)) {
		goto fail;
	}

fail:
	filp_close(file, NULL);

    return ret;

}

void dump_mp_data_to_seq_file(struct seq_file *m, const u16 *data, int rows, int cols)
{
	int r = 0;
	const u16 *n_vals = data;

	for (r = 0; r < rows; r++) {
		char linebuf[256];
		int len;

		len = dump_mp_data_row_to_buffer(linebuf, sizeof(linebuf), n_vals, cols, NULL, "\n", ',');
		seq_puts(m, linebuf);

		n_vals += cols;
	}
}

void dump_diff_data_to_seq_file(struct seq_file *m, const s16 *data, int rows, int cols)
{
	int r = 0;
	const s16 *n_vals = data;
	char linebuf[256];
	int len;
//	char y_prefix[10];
//	char topic[30];

	/*
	strncpy(topic, "Raw:", sizeof(topic));
	len = dump_excel_head_row_to_buffer(linebuf, sizeof(linebuf), cols, topic, "\n", ',');
	seq_puts(m, linebuf);
	*/

	for (r = 0; r < rows; r++) {
		/* scnprintf(y_prefix, sizeof(y_prefix), "Y%2d ", r+1); */
		len = dump_diff_data_row_to_buffer(linebuf, sizeof(linebuf), n_vals, cols, NULL, "\n", ',');
		seq_puts(m, linebuf);

		n_vals += cols;
	}
}

u8 g_filepath[100];

#define FILEPATH_MAX_LENGTH 200

static char *get_date_time_str(void)
{
	struct timespec now_time;
	struct rtc_time rtc_now_time;
	static char time_data_buf[128] = { 0 };

	getnstimeofday(&now_time);
	rtc_time_to_tm(now_time.tv_sec, &rtc_now_time);
	snprintf(time_data_buf, sizeof(time_data_buf), "%04d%02d%02d_%02d%02d%02d",
		(rtc_now_time.tm_year + 1900), rtc_now_time.tm_mon + 1,
		rtc_now_time.tm_mday, rtc_now_time.tm_hour, rtc_now_time.tm_min,
		rtc_now_time.tm_sec);

	return time_data_buf;
}

int gcore_save_mp_data_to_file(struct gcore_mp_data *mp_data)
{
	int rows = RAWDATA_ROW;
	int cols = RAWDATA_COLUMN;
	int ret = 0;
	u8 *filepath = NULL;
	char *rtctime = NULL;

	GTP_DEBUG("save mp data to file");

	filepath = kzalloc(FILEPATH_MAX_LENGTH, GFP_KERNEL);
	if (IS_ERR_OR_NULL(filepath)) {
		GTP_ERROR("alloc filepath mem fail!");
		return -EPERM;
	}

	rtctime = get_date_time_str();

	scnprintf(filepath, FILEPATH_MAX_LENGTH, "%s_%s_%s.csv",
					MP_TEST_LOG_FILEPATH, rtctime,
					mp_data->all_test_result ? "FAIL" : "PASS");

	GTP_DEBUG("dump mp data to csv file: %s row: %d	col: %d", filepath, rows, cols);

	ret = dump_mp_info_to_csv_file(filepath, O_RDWR | O_CREAT);
	if (ret < 0) {
		GTP_ERROR("dump mp info to file failed");
		goto fail;
	}

	if (mp_data->test_open) {
		ret = dump_mp_data_to_csv_file(filepath,
					       O_RDWR | O_CREAT, mp_data->open_node_val, rows,
					       cols, OPEN_DATA);
		if (ret < 0) {
			GTP_ERROR("dump mp open test data to file failed");
			goto fail;
		}
	}

	if (mp_data->test_short) {
		ret = dump_mp_data_to_csv_file(filepath,
					       O_RDWR | O_CREAT, mp_data->short_node_val, rows,
					       cols, SHORT_DATA);
		if (ret < 0) {
			GTP_ERROR("dump mp short test data to file failed");
			goto fail;
		}
	}

	if (mp_data->test_rawdata) {
		ret = dump_mp_data_to_csv_file(filepath,
					     O_RDWR | O_CREAT, mp_data->rawdata_node_val, rows,
					     cols, RAW_DATA);
		if (ret < 0) {
			GTP_ERROR("dump mp test rawdata to file failed");
			goto fail;
		}

#if MP_RAWDATA_DEC_TEST
		ret = dump_mp_data_to_csv_file(filepath,
					     O_RDWR | O_CREAT, mp_data->rawdata_dec_h, rows,
					     cols, RAWDATA_DECH);
		if (ret < 0) {
			GTP_ERROR("dump mp test rawdata devc h to file failed");
			goto fail;
		}

		scnprintf(filepath, FILEPATH_MAX_LENGTH, "%s_%lu_%s.csv",
					MP_TEST_LOG_FILEPATH, tv.tv_sec,
					mp_data->all_test_result ? "FAIL" : "PASS");
		ret = dump_mp_data_to_csv_file(filepath,
					     O_RDWR | O_CREAT | O_TRUNC, mp_data->rawdata_dec_l, rows,
					     cols, RAWDATA_DECL);
		if (ret < 0) {
			GTP_ERROR("dump mp test rawdata devc l to file failed");
			goto fail;
		}
#endif
#if MP_RAWDATA_AVG_TEST
		ret = dump_mp_data_to_csv_file(filepath,
					     O_RDWR | O_CREAT | O_TRUNC, mp_data->rawdata_avg_h, rows,
					     cols);
		if (ret < 0) {
			GTP_ERROR("dump mp test rawdata avg h to file failed");
			goto fail;
		}

		ret = dump_mp_data_to_csv_file(filepath,
					     O_RDWR | O_CREAT | O_TRUNC, mp_data->rawdata_avg_l, rows,
					     cols);
		if (ret < 0) {
			GTP_ERROR("dump mp test rawdata avg l to file failed");
			goto fail;
		}
#endif
	}

	if (mp_data->test_noise) {
		ret = dump_mp_data_to_csv_file(filepath,
						   O_RDWR | O_CREAT, mp_data->noise_node_val, rows,
						   cols, NOISE_DATA);
		if (ret < 0) {
			GTP_ERROR("dump mp noise test data to file failed");
			goto fail;
		}
	}

	kfree(filepath);

	return 0;

fail:
	kfree(filepath);
	return ret;
}

int save_demo_data_to_file(void)
{
	char *rtctime = NULL;
	struct file *file;

	pos = 0;
	frame_count = 0;

	rtctime = get_date_time_str();

	scnprintf(demolog_path, FILEPATH_MAX_LENGTH, "%s_%s.csv", "/sdcard/demodata", rtctime);

	GTP_DEBUG("dump demo data to csv file: %s", demolog_path);

	file = filp_open(demolog_path, O_RDWR | O_CREAT, 0666);
    if (IS_ERR(file)) {
        GTP_ERROR("Open file %s failed %ld", demolog_path, PTR_ERR(file));
        return -EPERM;
    }

	filp_close(file, NULL);

	return 0;
}


int gcore_mp_bin_update(void)
{
	u8 *fw_buf = NULL;
#ifdef CONFIG_UPDATE_FIRMWARE_BY_BIN_FILE
	const struct firmware *fw = NULL;

	fw_buf = g_mp_data->gdev->fw_mem;
	if (IS_ERR_OR_NULL(fw_buf)) {
		GTP_ERROR("fw buf mem is NULL");
		return -EPERM;
	}
if (strstr(saved_command_line,"n28_gc7272_dsi_vdo_hdp_xinxian_hkc")){
       if (request_firmware(&fw, MP_BIN_NAME_HKC, &g_mp_data->gdev->bus_device->dev)) {
		GTP_ERROR("request firmware fail");
		goto fail1;
	}
    }else
	{
		if (request_firmware(&fw, MP_BIN_NAME_TXD, &g_mp_data->gdev->bus_device->dev)) {
		GTP_ERROR("request firmware fail");
		goto fail1;
		}
	}

	memcpy(fw_buf, fw->data, fw->size);

	add_CRC(fw_buf);

	GTP_DEBUG("fw buf:%x %x %x %x %x %x", fw_buf[0], fw_buf[1], fw_buf[2],
		  fw_buf[3], fw_buf[4], fw_buf[5]);

	GTP_DEBUG("mp bin ver:%x %x %x %x", fw_buf[FW_VERSION_ADDR+1], fw_buf[FW_VERSION_ADDR], \
										fw_buf[FW_VERSION_ADDR+3], fw_buf[FW_VERSION_ADDR+2]);

	if (fw) {
		release_firmware(fw);
	}
#else
	fw_buf = gcore_mp_FW;
#endif

	g_mp_data->mpbin_ver[0] = fw_buf[FW_VERSION_ADDR];
	g_mp_data->mpbin_ver[1] = fw_buf[FW_VERSION_ADDR + 1];
	g_mp_data->mpbin_ver[2] = fw_buf[FW_VERSION_ADDR + 2];
	g_mp_data->mpbin_ver[3] = fw_buf[FW_VERSION_ADDR + 3];

	GTP_DEBUG("mp bin version:%02x %02x %02x %02x", g_mp_data->mpbin_ver[1],
		g_mp_data->mpbin_ver[0], g_mp_data->mpbin_ver[3], g_mp_data->mpbin_ver[2]);

#ifdef CONFIG_GCORE_AUTO_UPDATE_FW_HOSTDOWNLOAD
	if (gcore_auto_update_hostdownload_proc(fw_buf)) {
		GTP_ERROR("mp bin update hostdownload proc fail");
		goto fail1;
	}
#else
	if (gcore_update_hostdownload_idm2(fw_buf)) {
		GTP_ERROR("mp bin update hostdownload proc fail");
		goto fail1;
	}
#endif

	return 0;

fail1:

	return -EPERM;

}

int gcore_mp_test_all_result(struct gcore_mp_data *mp_data)
{
	if (mp_data->test_int_pin && mp_data->int_pin_test_result) {
		return -EPERM;
	}

	if (mp_data->test_chip_id && mp_data->chip_id_test_result) {
		return -EPERM;
	}

	if (mp_data->test_open && mp_data->open_test_result) {
		return -EPERM;
	}

	if (mp_data->test_short && mp_data->short_test_result) {
		return -EPERM;
	}

	if (mp_data->test_short && mp_data->short_test_result) {
		return -EPERM;
	}

	if (mp_data->test_rawdata && mp_data->rawdata_test_result) {
		return -EPERM;
	}

	if (mp_data->test_noise && mp_data->noise_test_result) {
		return -EPERM;
	}

	return 0;
}

int gcore_start_mp_test(void)
{
	struct gcore_mp_data *mp_data = g_mp_data;

	fn_data.gdev->ts_stat = TS_MPTEST;
	if (mutex_is_locked(&fn_data.gdev->ITOTEST_lock) ){
		GTP_DEBUG("mp test is locked, ignore");
		mutex_unlock(&fn_data.gdev->ITOTEST_lock);
		return -EPERM;
	}

	mutex_lock(&fn_data.gdev->ITOTEST_lock);

	GTP_DEBUG("gcore start mp test.");

#if GCORE_WDT_RECOVERY_ENABLE
	cancel_delayed_work_sync(&mp_data->gdev->wdt_work);
#endif

	gcore_parse_mp_test_ini(mp_data);

	gcore_alloc_mp_test_mem(mp_data);

	if (mp_data->test_int_pin) {
		mp_data->int_pin_test_result = gcore_mp_test_int_pin(mp_data);
		if (mp_data->int_pin_test_result) {
			GTP_DEBUG("Int pin test result fail!");
		}
	}

	usleep_range(1000, 1100);

	if (mp_data->test_chip_id) {
		mp_data->chip_id_test_result = gcore_mp_test_chip_id(mp_data);
		if (mp_data->chip_id_test_result) {
			GTP_DEBUG("Chip id test result fail!");
		}
	}

	usleep_range(1000, 1100);

	if (mp_data->test_open || mp_data->test_short || mp_data->test_rawdata \
						   || mp_data->test_noise) {
		GTP_DEBUG("mp test begin to updata mp bin");

		if (gcore_mp_bin_update()) {
			GTP_ERROR("gcore mp bin update fail!");
			mp_data->mp_update_result = -1;
			mp_data->open_test_result = -1;
			mp_data->short_test_result = -1;
			mp_data->rawdata_test_result = -1;
			mp_data->noise_test_result = -1;
		} else {
			msleep(100);

			mp_data->mp_update_result = 0;

			if (mp_data->test_open) {
				mp_data->open_test_result = gcore_mp_test_item_open(mp_data);
				if (mp_data->open_test_result) {
					GTP_DEBUG("Open test result fail!");
				}
			}

			msleep(100);

			if (mp_data->test_short) {
				mp_data->short_test_result = gcore_mp_test_item_short(mp_data);
				if (mp_data->short_test_result) {
					GTP_DEBUG("Short test result fail!");
				}
			}

			msleep(100);

			if (mp_data->test_rawdata) {
				mp_data->rawdata_test_result = gcore_mp_test_item_rawdata(mp_data);
				if (mp_data->rawdata_test_result) {
					GTP_DEBUG("Rawdata test result fail!");
				}
			}

			msleep(100);

			if (mp_data->test_noise) {
				mp_data->noise_test_result = gcore_mp_test_item_noise(mp_data);
				if (mp_data->noise_test_result) {
					GTP_DEBUG("Noise test result fail!");
				}
			}
		}

	}

	msleep(100);

	mp_data->all_test_result = gcore_mp_test_all_result(mp_data);

	GTP_DEBUG("start mp test result:%d", mp_data->all_test_result);

	gcore_save_mp_data_to_file(mp_data);

#ifdef CONFIG_GCORE_AUTO_UPDATE_FW_HOSTDOWNLOAD
	gcore_request_firmware_update_work(NULL);
#else
	gcore_upgrade_soft_reset();
#endif

#if GCORE_WDT_RECOVERY_ENABLE
	mod_delayed_work(mp_data->gdev->fwu_workqueue, &mp_data->gdev->wdt_work, \
					   msecs_to_jiffies(GCORE_WDT_TIMEOUT_PERIOD));
#endif

	fn_data.gdev->ts_stat = TS_NORMAL;
	mutex_unlock(&fn_data.gdev->ITOTEST_lock);

	return mp_data->all_test_result;

}

static int32_t gcore_mp_test_open(struct inode *inode, struct file *file)
{
	struct gcore_mp_data *mp_data = PDE_DATA(inode);

	if (mp_data == NULL) {
		GTP_ERROR("Open selftest proc with mp data = NULL");
		return -EPERM;
	}

	GTP_DEBUG("gcore mp test open");

#if GCORE_WDT_RECOVERY_ENABLE
	cancel_delayed_work_sync(&mp_data->gdev->wdt_work);
#endif

	gcore_parse_mp_test_dt(mp_data);

	gcore_alloc_mp_test_mem(mp_data);

	if (mp_data->test_int_pin) {
		mp_data->int_pin_test_result = gcore_mp_test_int_pin(mp_data);
	}

	usleep_range(1000, 1100);

	if (mp_data->test_chip_id) {
		mp_data->chip_id_test_result = gcore_mp_test_chip_id(mp_data);
	}

	usleep_range(1000, 1100);

	if (mp_data->test_open || mp_data->test_short || mp_data->test_rawdata) {
		GTP_DEBUG("mp test begin to updata mp bin");

		if (gcore_mp_bin_update()) {
			GTP_ERROR("gcore mp bin update fail!");
			mp_data->open_test_result = 1;
			mp_data->short_test_result = 1;
			mp_data->rawdata_test_result = 1;
		} else {
			usleep_range(1000, 1100);

			if (mp_data->test_open) {
				mp_data->open_test_result = gcore_mp_test_item_open(mp_data);
			}

			usleep_range(1000, 1100);

			if (mp_data->test_short) {
				mp_data->short_test_result = gcore_mp_test_item_short(mp_data);
			}

			usleep_range(1000, 1100);

			if (mp_data->test_rawdata) {
				mp_data->rawdata_test_result = gcore_mp_test_item_rawdata(mp_data); 
				if (mp_data->rawdata_test_result) {
					GTP_DEBUG("Rawdata test result fail!");
				}
			}
		}

	}

	gcore_save_mp_data_to_file(mp_data);

	if (seq_open(file, &gcore_mptest_seq_ops)) {
		GTP_ERROR("seq open fail!");
		return -EPERM;
	}

	((struct seq_file *)file->private_data)->private = mp_data;

#if GCORE_WDT_RECOVERY_ENABLE
	mod_delayed_work(mp_data->gdev->fwu_workqueue, &mp_data->gdev->wdt_work, \
					   msecs_to_jiffies(GCORE_WDT_TIMEOUT_PERIOD));
#endif

	return 0;
}

struct file_operations gcore_mp_fops = {
	.owner = THIS_MODULE,
	.open = gcore_mp_test_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

static void *gcore_seq_raw_start(struct seq_file *m, loff_t *pos)
{
	GTP_DEBUG("seq start");

	return pos;
}

static void *gcore_seq_raw_next(struct seq_file *m, void *v, loff_t *pos)
{
	(*pos)++;

	return pos;
}

static void gcore_seq_raw_stop(struct seq_file *m, void *v)
{
	GTP_DEBUG("seq stop");

	return;
}

static int gcore_seq_raw_show(struct seq_file *m, void *v)
{
	struct gcore_dev *gdev = (struct gcore_dev *)m->private;
	int rows = RAWDATA_ROW;
	int cols = RAWDATA_COLUMN;
	int i = 0;
	u8 *touch_data_p = gdev->touch_data;
	s16 node_data = 0;

	gdev->usr_read = true;

	GTP_DEBUG("adb read before sleep.");
	while (gdev->data_ready == false) {
		if (wait_event_interruptible(gdev->usr_wait, gdev->data_ready == true)) {
			gdev->usr_read = false;
			return -ERESTARTSYS;
		}
	}
	GTP_DEBUG("adb read after sleep.");

	for (i = 0; i < RAW_DATA_SIZE / 2; i++) {
		node_data = (touch_data_p[1] << 8) | touch_data_p[0];

		gdev->diffdata[i] = node_data;

		touch_data_p += 2;
	}
	dump_diff_data_to_seq_file(m, gdev->diffdata, rows, cols);

	seq_putc(m, '\n');

	gdev->usr_read = false;
	gdev->data_ready = false;

	return 0;
}

const struct seq_operations gcore_rawdata_seq_ops = {
	.start = gcore_seq_raw_start,
	.next = gcore_seq_raw_next,
	.stop = gcore_seq_raw_stop,
	.show = gcore_seq_raw_show,
};

static int32_t gcore_rawdata_open(struct inode *inode, struct file *file)
{
	struct gcore_dev *gdev = g_mp_data->gdev;

	if (IS_ERR_OR_NULL(gdev->diffdata)) {
		gdev->diffdata = (s16 *)kzalloc(RAW_DATA_SIZE, GFP_KERNEL);
		if (IS_ERR_OR_NULL(gdev->diffdata)) {
			GTP_ERROR("allocate diffdata mem fail!");
			return -ENOMEM;
		}
	}

	memset(gdev->diffdata, 0, RAW_DATA_SIZE);

	if (seq_open(file, &gcore_rawdata_seq_ops)) {
		GTP_ERROR("seq open fail!");
		return -EPERM;
	}

	((struct seq_file *)file->private_data)->private = gdev;

	return 0;
}

struct proc_dir_entry *rawdata_proc_entry;
#define RAWDATA_PROC_FILENAME "gcore_rawdata"

struct file_operations gcore_rawdata_fops = {
	.owner = THIS_MODULE,
	.open = gcore_rawdata_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

static int openshort_test_proc_show(struct seq_file *m, void *v)
{
    mdelay(1000);
    if(gcore_start_mp_test() < 0)
        seq_printf(m, "%s\n", "result=0");
    else
        seq_printf(m, "%s\n", "result=1");
    return 0;
}
static int openshort_test_open(struct inode *inode, struct file *file)
{
    return single_open(file, openshort_test_proc_show, NULL);
}

static const struct file_operations gc_proc_ctp_openshort_test_fops = {
    .open 	 = openshort_test_open,
    .read 	 = seq_read,
    .llseek	 = seq_lseek,
    .release = single_release,
};

/*proc init part*/
extern char *saved_command_line;
static int __init proc_node_init(void)
{
	printk(KERN_ERR "%s: GTP start  ---- proc_ctp_openshort_test_file:%d\n", __func__,__LINE__);
    if (!strstr(saved_command_line,"n28_gc7272_dsi_vdo_hdp_xinxian_hkc")&&(!strstr(saved_command_line,"n28_gc7272_dsi_vdo_hdp_txd_sharp"))){
		GTP_ERROR("strstr error");
        return  -1;
	}
    gc_proc_touchscreen_dir = proc_mkdir(GC_PROC_TOUCHSCREEN_FOLDER , NULL);
	GTP_DEBUG("mdkdir touchscreen dir");
    if (gc_proc_touchscreen_dir == NULL)  {
        printk(KERN_ERR "%s: proc_touchpanel_dir file create failed!\n", __func__);
    }else{
#ifdef PROC_NODE
		GTP_DEBUG("mdkdir touchscreen dir");
        gc_proc_ctp_openshort_test_file = proc_create(GC_PROC_CTP_OPENSHORT_TEST_FILE,
        (S_IWUSR | S_IRUGO), gc_proc_touchscreen_dir, &gc_proc_ctp_openshort_test_fops);
        if(gc_proc_ctp_openshort_test_file ==NULL){
            printk(KERN_ERR "%s: proc_ctp_openshort_test_file create failed!\n", __func__);
            remove_proc_entry(GC_PROC_TOUCHSCREEN_FOLDER, NULL);
        }else{
            printk("%s:  proc_create PROC_CTP_OPENSHORT_TEST_FILE success",__func__);
        }
#endif
    }
	printk(KERN_ERR "%s: GTP end  ---- proc_ctp_openshort_test_file:%d\n", __func__,__LINE__);
    return  0;
}


int gcore_rawdata_node_init(void)
{
	rawdata_proc_entry = proc_create_data(RAWDATA_PROC_FILENAME,
						  S_IRUGO, NULL, &gcore_rawdata_fops, NULL);
	if (rawdata_proc_entry == NULL) {
		GTP_ERROR("create rawdata proc entry failed");
		return -EPERM;
	}

	return 0;
}

int gcore_mp_test_fn_init(struct gcore_dev *gdev)
{
	struct gcore_mp_data *mp_data = NULL;

	GTP_DEBUG("gcore mp test fn init");

	mp_thread = kthread_run(mp_test_event_handler, gdev, "gcore_mptest");

	mp_data = kzalloc(sizeof(struct gcore_mp_data), GFP_KERNEL);
	if (IS_ERR_OR_NULL(mp_data)) {
		GTP_ERROR("allocate mp_data mem fail! failed");
		return -EPERM;
	}

	mp_data->gdev = gdev;

	g_mp_data = mp_data;

	mp_data->mp_proc_entry = proc_create_data(MP_TEST_PROC_FILENAME,
						  S_IRUGO, NULL, &gcore_mp_fops, mp_data);
	if (mp_data->mp_proc_entry == NULL) {
		GTP_ERROR("create mp test proc entry selftest failed");
		goto fail1;
	}

	gcore_rawdata_node_init();

	return 0;

fail1:
	kfree(mp_data);
	return -EPERM;

}

void gcore_mp_test_fn_remove(struct gcore_dev *gdev)
{
	GTP_DEBUG("gcore_mp_test_fn_remove start.");
	mp_test_fn.event_flag = true;
	wake_up_interruptible(&gdev->wait);
	kthread_stop(mp_thread);

	if (g_mp_data->mp_proc_entry) {
		remove_proc_entry(MP_TEST_PROC_FILENAME, NULL);
	}

	gcore_free_mp_test_mem(g_mp_data);

	kfree(g_mp_data);
	GTP_DEBUG("gcore_mp_test_fn_remove end.");

	return;
}
fs_initcall(proc_node_init);

#if 0

static int __init gcore_mp_test_init(void)
{
	GTP_DEBUG("gcore_fw_update_init.");

	gcore_new_function_register(&mp_test_fn);

	return 0;
}

static void __exit gcore_mp_test_exit(void)
{
	GTP_DEBUG("gcore_fw_update_exit.");

	gcore_new_function_unregister(&mp_test_fn);

	return;
}

module_init(gcore_mp_test_init);
module_exit(gcore_mp_test_exit);

MODULE_AUTHOR("GalaxyCore, Inc.");
MODULE_DESCRIPTION("GalaxyCore Touch MP Test Module");
MODULE_LICENSE("GPL");

#endif
