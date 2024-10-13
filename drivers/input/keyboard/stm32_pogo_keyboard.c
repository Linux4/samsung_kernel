#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/input.h>
#include <linux/input/matrix_keypad.h>
#include <linux/delay.h>
#include <linux/ctype.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/errno.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/wakelock.h>
#include <linux/muic/muic.h>
#include <linux/muic/muic_notifier.h>
#include <linux/firmware.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif
#ifdef CONFIG_SEC_SYSFS
#include <linux/sec_class.h>
#else
extern struct class *sec_class;
#endif

extern void fpga_rstn_control(void);
extern void fpga_rstn_high(void);
extern bool is_fw_dl_completed(void);

#define STM32_FOTA_BIN_PATH		"/data/data/com.sec.android.app.applinker/files"
#define STM32_FW_SIZE			(100 * 1024)

#define STM32_DRV_DESC			"stm32 i2c I/O expander"
#define STM32_DRV_NAME			"stm32kbd"
#define STM32_MAGIC_WORD		"STM32L4"

#define INPUT_VENDOR_ID_SAMSUNG		0x04E8
#define INPUT_PRODUCT_ID_POGO_KEYBOARD	0xA035

#define STM32_TMR_INTERVAL		10L
#define STM32_KPC_ROW_SHIFT		4 // can be 3
#define STM32_KPC_DATA_ROW		0xE0
#define STM32_KPC_DATA_ROW_SHIFT	5
#define STM32_KPC_DATA_COL		0x1E
#define STM32_KPC_DATA_COL_SHIFT	1
#define STM32_KPC_DATA_PRESS		0x01

#define STM32_MAX_EVENT_COUNT		100

#define INT_ENABLE		1
#define INT_DISABLE		0

enum STM32_MODE {
	MODE_APP = 1,
	MODE_DFU,
};

enum STM32_FW_PARAM {
	FW_UPDATE_FOTA = 0,
};

#define STM32_DEBUG_LEVEL_I2C_LOG	(1 << 0)

/*
 * Definition about App protocol task
 */
#define STM32_CMD_CHECK_VERSION			0x00
#define STM32_CMD_CHECK_CRC			0x01
#define STM32_CMD_ENTER_DFU_MODE		0x02
#define STM32_CMD_READ_EVENT_COUNT		0x03
#define STM32_CMD_READ_EVENT			0x04
#define STM32_CMD_ENTER_LOW_POWER_MODE		0x05
#define STM32_CMD_EXIT_LOW_POWER_MODE		0x06
#define STM32_CMD_GET_MODE			0x07

/*
 * Definition about DFU protocol task
 */
#define STM32_CMD_GET_PROTOCOL_VERSION		0x0B
#define STM32_CMD_GET_TARGET_FW_VERSION		0x0D
#define STM32_CMD_GET_TARGET_FW_CRC32		0x0F
#define STM32_CMD_GET_TARGET_BANK		0x10
#define STM32_CMD_GET_PAGE_INDEX		0x11
#define STM32_CMD_GET_PAGE_SIZE			0x12
#define STM32_CMD_START_FW_UPGRADE		0x13
#define STM32_CMD_RESUME_FW_UPGRADE		0x14
#define STM32_CMD_WRITE_PAGE			0x15
#define STM32_CMD_WRITE_LAST_PAGE		0x16
#define STM32_CMD_GO				0x17
#define STM32_CMD_ABORT				0x18

#define STM32_CFG_PACKET_SIZE			256

#define STM32_FW_APP_CFG_OFFSET			0x018C

static const u32 crctab[256] =
{
	0x00000000,
	0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b,
	0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f, 0x2f8ad6d6,
	0x2b4bcb61, 0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd,
	0x4c11db70, 0x48d0c6c7, 0x4593e01e, 0x4152fda9, 0x5f15adac,
	0x5bd4b01b, 0x569796c2, 0x52568b75, 0x6a1936c8, 0x6ed82b7f,
	0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3, 0x709f7b7a,
	0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
	0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5, 0xbe2b5b58,
	0xbaea46ef, 0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033,
	0xa4ad16ea, 0xa06c0b5d, 0xd4326d90, 0xd0f37027, 0xddb056fe,
	0xd9714b49, 0xc7361b4c, 0xc3f706fb, 0xceb42022, 0xca753d95,
	0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1, 0xe13ef6f4,
	0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d, 0x34867077, 0x30476dc0,
	0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5,
	0x2ac12072, 0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16,
	0x018aeb13, 0x054bf6a4, 0x0808d07d, 0x0cc9cdca, 0x7897ab07,
	0x7c56b6b0, 0x71159069, 0x75d48dde, 0x6b93dddb, 0x6f52c06c,
	0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08, 0x571d7dd1,
	0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
	0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b,
	0xbb60adfc, 0xb6238b25, 0xb2e29692, 0x8aad2b2f, 0x8e6c3698,
	0x832f1041, 0x87ee0df6, 0x99a95df3, 0x9d684044, 0x902b669d,
	0x94ea7b2a, 0xe0b41de7, 0xe4750050, 0xe9362689, 0xedf73b3e,
	0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2, 0xc6bcf05f,
	0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34,
	0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59, 0x608edb80,
	0x644fc637, 0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb,
	0x4f040d56, 0x4bc510e1, 0x46863638, 0x42472b8f, 0x5c007b8a,
	0x58c1663d, 0x558240e4, 0x51435d53, 0x251d3b9e, 0x21dc2629,
	0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5, 0x3f9b762c,
	0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
	0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623, 0xf12f560e,
	0xf5ee4bb9, 0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65,
	0xeba91bbc, 0xef68060b, 0xd727bbb6, 0xd3e6a601, 0xdea580d8,
	0xda649d6f, 0xc423cd6a, 0xc0e2d0dd, 0xcda1f604, 0xc960ebb3,
	0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7, 0xae3afba2,
	0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6, 0x9ff77d71,
	0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74,
	0x857130c3, 0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640,
	0x4e8ee645, 0x4a4ffbf2, 0x470cdd2b, 0x43cdc09c, 0x7b827d21,
	0x7f436096, 0x7200464f, 0x76c15bf8, 0x68860bfd, 0x6c47164a,
	0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e, 0x18197087,
	0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
	0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d,
	0x2056cd3a, 0x2d15ebe3, 0x29d4f654, 0xc5a92679, 0xc1683bce,
	0xcc2b1d17, 0xc8ea00a0, 0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb,
	0xdbee767c, 0xe3a1cbc1, 0xe760d676, 0xea23f0af, 0xeee2ed18,
	0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4, 0x89b8fd09,
	0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662,
	0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06, 0xa6322bdf,
	0xa2f33668, 0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
};

struct stm32_event_data {
	union {
		struct {
			u8 press:1;
			u8 col:4;
			u8 row:3;
		} __packed;
		u8 data[1];
	};
};

struct stm32_fw_version {
	u8 hw_rev;
	u8 model_id;
	u8 fw_minor_ver;
	u8 fw_major_ver;
} __packed;

struct stm32_fw_header {
	u8 magic_word[8];
	u32 status_boot_mode;
	u32 reserved;
	u8 cfg_ver[4];
	struct stm32_fw_version boot_app_ver;
	struct stm32_fw_version target_app_ver;
	u32 boot_bank_addr;
	u32 target_bank_addr;
	u32 fpga_fw_addr_offset;
	u32 fpga_fw_size;
	u32 crc;
} __packed;

struct stm32_dev {
	int					dev_irq;
	struct i2c_client			*client;
	struct input_dev			*input_dev;
	struct delayed_work			worker;
#ifdef GHOST_CHECK_WORKAROUND
	struct delayed_work			ghost_check_work;
#endif
	struct mutex				dev_lock;
	struct mutex				irq_lock;
	struct wake_lock			stm_wake_lock;
#ifdef CONFIG_PM_SLEEP
	bool					dev_resume_state;
	struct completion			resume_done;
	bool					irq_wake;
#endif
	struct device				*sec_pogo_keyboard;

	struct matrix_keymap_data		*keymap_data;
	unsigned short				*keycode;
	int					keycode_entries;

	struct stm32_devicetree_data		*dtdata;
	int					irq_gpio;
	int					sda_gpio;
	int					scl_gpio;
	struct pinctrl				*pinctrl;
	int					*key_state;
	int					connect_state;
	int					current_connect_state;
	struct notifier_block			keyboard_nb;
	struct delayed_work			keyboard_work;
	char					key_name[707];

	struct delayed_work			check_ic_work;

	struct firmware				*fw;
	struct stm32_fw_header			*fw_header;
	struct stm32_fw_version			ic_fw_ver;
	u32					crc_of_ic;
	u32					crc_of_bin;

	bool					ic_ready;

	int					debug_level;
};

struct stm32_devicetree_data {
	int gpio_int;
	int gpio_sda;
	int gpio_scl;
	int num_row;
	int num_column;
	struct regulator *vdd_vreg;
	const char *model_name;
};

static int stm32_fw_update(struct stm32_dev *stm32, const struct firmware *fw);

static int stm32_get_keycode_entries(struct stm32_dev *stm32,
		u32 mask_row, u32 mask_col)
{
	int col, row;
	int i, j, comp;

	row = mask_row & 0xff;
	col = mask_col & 0x3ff;

	for (i = 8; i > 0; i--) {
		comp = 1 << (i - 1);
		if (row & comp)
			break;
	}

	for (j = 10; j > 0; j--) {
		comp = 1 << (j - 1);
		if (col & comp)
			break;
	}
	input_info(true, &stm32->client->dev, "row: %d, col: %d\n", i, j);

	return (MATRIX_SCAN_CODE(i - 1, j - 1, STM32_KPC_ROW_SHIFT)+1);
}

static void stm32_release_all_key(struct stm32_dev *stm32)
{
	int i, j;

	if (!stm32->input_dev)
		return;

	for (i = 0; i < stm32->dtdata->num_row; i++) {
		if (!stm32->key_state[i])
			continue;

		for (j = 0; j < stm32->dtdata->num_column; j++) {
			if (stm32->key_state[i] & (1 << j)) {
				int code = MATRIX_SCAN_CODE(i, j, STM32_KPC_ROW_SHIFT);
				input_event(stm32->input_dev, EV_MSC, MSC_SCAN, code);
				input_report_key(stm32->input_dev,
						stm32->keycode[code], 0);
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
				input_info(true, &stm32->client->dev,
						"RA code(0x%X|%d) R:C(%d:%d)\n",
						stm32->keycode[code],
						stm32->keycode[code], i, j);
#else
				input_info(true, &stm32->client->dev,
						"RA (%d:%d)\n", i, j);
#endif
				stm32->key_state[i] &= ~(1 << j);
			}
		}
	}
	input_sync(stm32->input_dev);
}

static int stm32_i2c_block_read(struct i2c_client *client, u8 length, u8 *values)
{
	struct stm32_dev *device_data = i2c_get_clientdata(client);
	int ret;
	int retry = 3;
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = client->flags | I2C_M_RD,
			.len = length,
			.buf = values,
		},
	};

	while (retry--) {
		ret = i2c_transfer(client->adapter, msgs, 1);
		if (ret != 1) {
			input_err(true, &client->dev, "failed to read %d, length %d, cdone %d\n",
					ret, length, is_fw_dl_completed());
			stm32_release_all_key(device_data);
			fpga_rstn_control();
			ret = -EIO;
			usleep_range(10000, 10000);
			continue;
		}
		break;
	}

	return ret;
}

static int stm32_i2c_block_write(struct i2c_client *client, u16 length, u8 *values)
{
	struct stm32_dev *device_data = i2c_get_clientdata(client);
	int ret;
	int retry = 3;
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = client->flags,
			.len = length,
			.buf = values,
		},
	};

	while (retry--) {
		ret = i2c_transfer(client->adapter, msgs, 1);
		if (ret != 1) {
			input_err(true, &client->dev, "failed to write %d, length %d, cdone %d\n",
					ret, length, is_fw_dl_completed());
			stm32_release_all_key(device_data);
			fpga_rstn_control();
			ret = -EIO;
			usleep_range(10000, 10000);
			continue;
		}
		break;
	}

	return ret;
}

static int stm32_i2c_reg_write(struct i2c_client *client, u8 regaddr)
{
	struct stm32_dev *device_data = i2c_get_clientdata(client);
	int ret;
	
	ret = stm32_i2c_block_write(client, 1, &regaddr);

	if (device_data->debug_level & STM32_DEBUG_LEVEL_I2C_LOG)
		input_info(true, &client->dev, "%s: %02X %s\n", __func__, regaddr, ret < 0 ? "fail" : "");

	return ret;
}

static int stm32_i2c_reg_read(struct i2c_client *client, u8 regaddr, u8 length, u8 *values)
{
	int ret;

	ret = stm32_i2c_reg_write(client, regaddr);
	if (ret < 0)
		return ret;
	
	ret = stm32_i2c_block_read(client, length, values);

	return ret;
}

static int stm32_dev_regulator(struct stm32_dev *device_data, int onoff)
{
	struct device *dev = &device_data->client->dev;
	int ret = 0;

	if (IS_ERR_OR_NULL(device_data->dtdata->vdd_vreg))
		return ret;

	if (onoff) {
		if (!regulator_is_enabled(device_data->dtdata->vdd_vreg)) {
			ret = regulator_enable(device_data->dtdata->vdd_vreg);
			if (ret) {
				input_err(true, dev,
						"%s: Failed to enable vddo: %d\n",
						__func__, ret);
				return ret;
			}
		} else {
			input_err(true, dev, "%s: vdd is already enabled\n", __func__);
		}
	} else {
		if (regulator_is_enabled(device_data->dtdata->vdd_vreg)) {
			ret = regulator_disable(device_data->dtdata->vdd_vreg);
			if (ret) {
				input_err(true, dev,
						"%s: Failed to disable vddo: %d\n",
						__func__, ret);
				return ret;
			}
		} else {
			input_err(true, dev, "%s: vdd is already disabled\n", __func__);
		}
	}

	input_err(true, dev, "%s %s: vdd:%s\n", __func__, onoff ? "on" : "off",
			regulator_is_enabled(device_data->dtdata->vdd_vreg) ? "on" : "off");

	return ret;
}

static void stm32_enable_irq(struct stm32_dev *stm32, int enable)
{
	static int depth;
	
	mutex_lock(&stm32->irq_lock);
	if (gpio_is_valid(stm32->dtdata->gpio_int)) {
		if (enable) {
			if (depth) {
				--depth;
				enable_irq(stm32->dev_irq);
				input_info(true, &stm32->client->dev, "%s: enable irq\n", __func__);
			}
		} else {
			if (!depth) {
				++depth;
				disable_irq_nosync(stm32->dev_irq);
				input_info(true, &stm32->client->dev, "%s: disable irq\n", __func__);
			}
		}
	} else {
		if (enable)
			schedule_delayed_work(&stm32->worker, STM32_TMR_INTERVAL);
		else
			cancel_delayed_work(&stm32->worker);
	}
	mutex_unlock(&stm32->irq_lock);
}

static int stm32_read_crc(struct stm32_dev *stm32)
{
	int ret;
	u8 rbuf[4] = { 0 };

	ret = stm32_i2c_reg_read(stm32->client, STM32_CMD_CHECK_CRC, 4, rbuf);
	if (ret < 0)
		return ret;

	stm32->crc_of_ic = rbuf[3] << 24 | rbuf[2] << 16 | rbuf[1] << 8 | rbuf[0];

	input_info(true, &stm32->client->dev, "%s: [IC] BOOT CRC32 = 0x%08X\n", __func__, stm32->crc_of_ic);
	return 0;
}

static int stm32_read_version(struct stm32_dev *stm32)
{
	int ret;
	u8 rbuf[4] = { 0 };

	ret = stm32_i2c_reg_read(stm32->client, STM32_CMD_CHECK_VERSION, 4, rbuf);
	if (ret < 0)
		return ret;

	stm32->ic_fw_ver.hw_rev = rbuf[0];
	stm32->ic_fw_ver.model_id = rbuf[1];
	stm32->ic_fw_ver.fw_minor_ver = rbuf[2];
	stm32->ic_fw_ver.fw_major_ver = rbuf[3];

	input_info(true, &stm32->client->dev, "%s: [IC] version:%d.%d, model_id:%02d, hw_rev:%02d\n",
			__func__, stm32->ic_fw_ver.fw_major_ver, stm32->ic_fw_ver.fw_minor_ver,
			stm32->ic_fw_ver.model_id, stm32->ic_fw_ver.hw_rev);
	return 0;
}

u32 stm32_crc32(u8 *src, size_t len) {
	u8 *bp;
	u32 idx, crc = 0xFFFFFFFF;

	for (idx = 0; idx < len; idx++) {
		bp = src  + (idx ^ 0x3);
		crc = (crc << 8) ^ crctab[((crc >> 24) ^ *bp) & 0xFF];
	}
	return crc;
}

static int stm32_load_fw_from_fota(struct stm32_dev *stm32, bool do_fw_update)
{
	struct file *fp;
	mm_segment_t old_fs;
	long fw_size, nread;
	int error = 0;
	char fw_path[128] = { 0 };

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	snprintf(fw_path, sizeof(fw_path), "%s/%s.bin", STM32_FOTA_BIN_PATH, stm32->dtdata->model_name);

	input_info(true, &stm32->client->dev, "%s: path:%s\n", __func__, fw_path);

	fp = filp_open(fw_path, O_RDONLY, 0400);
	if (IS_ERR(fp)) {
		input_err(true, &stm32->client->dev, "%s: failed to open fw\n", __func__);
		error = -ENOENT;
		goto err_open;
	}

	fw_size = fp->f_path.dentry->d_inode->i_size;
	if (fw_size <= 0) {
		error = -ENOENT;
		goto err_get_size;
	}

	stm32->fw->size = (size_t)fw_size;
	input_info(true, &stm32->client->dev, "%s: size %ld Bytes\n", __func__, fw_size);

	nread = vfs_read(fp, (char __user *)stm32->fw->data, fw_size, &fp->f_pos);
	if (nread != fw_size) {
		input_err(true, &stm32->client->dev,
				"%s: failed to read firmware file, nread %ld Bytes\n", __func__, nread);
		error = -EIO;
		goto err_get_size;
	}

	memcpy(stm32->fw_header, &stm32->fw->data[STM32_FW_APP_CFG_OFFSET], sizeof(struct stm32_fw_header));
	stm32->crc_of_bin = stm32_crc32((u8 *)stm32->fw->data, stm32->fw->size);

	input_info(true, &stm32->client->dev,
			"%s: [BIN] %s, version:%d.%d, model_id:%d, hw_rev:%d, CRC:0x%X\n",
			__func__, stm32->fw_header->magic_word,
			stm32->fw_header->boot_app_ver.fw_major_ver,
			stm32->fw_header->boot_app_ver.fw_minor_ver,
			stm32->fw_header->boot_app_ver.model_id,
			stm32->fw_header->boot_app_ver.hw_rev, stm32->crc_of_bin);

	if (do_fw_update) {
		fpga_rstn_control();
		msleep(20);

		error = stm32_fw_update(stm32, stm32->fw);
	}

err_get_size:
	filp_close(fp, NULL);
err_open:
	set_fs(old_fs);
	return error;
}

static bool stm32_check_fw_update(struct stm32_dev *stm32)
{
	u16 fw_ver_bin, fw_ver_ic;

	if (stm32_read_version(stm32) < 0) {
		input_err(true, &stm32->client->dev, "%s: failed to read version\n", __func__);
		return false;
	}

	if (stm32_read_crc(stm32) < 0) {
		input_err(true, &stm32->client->dev, "%s: failed to read crc\n", __func__);
		return false;
	}

	if (stm32_load_fw_from_fota(stm32, false) < 0) {
		input_err(true, &stm32->client->dev, "%s: failed to read bin data\n", __func__);
		return false;
	}

	fw_ver_bin = stm32->fw_header->boot_app_ver.fw_major_ver << 8 |
			stm32->fw_header->boot_app_ver.fw_minor_ver;
	fw_ver_ic = stm32->ic_fw_ver.fw_major_ver << 8 | stm32->ic_fw_ver.fw_minor_ver;

	input_info(true, &stm32->client->dev, "%s: [BIN] V:%d.%d CRC:0x%X / [IC] V:%d.%d CRC:0x%X\n",
			__func__, stm32->fw_header->boot_app_ver.fw_major_ver,
			stm32->fw_header->boot_app_ver.fw_minor_ver, stm32->crc_of_bin,
			stm32->ic_fw_ver.fw_major_ver, stm32->ic_fw_ver.fw_minor_ver, stm32->crc_of_ic);

	if (strncmp(STM32_MAGIC_WORD, stm32->fw_header->magic_word, 7) != 0) {
		input_info(true, &stm32->client->dev, "%s: binary file is wrong : %s\n",
				__func__, stm32->fw_header->magic_word);
		return false;
	}

	if (stm32->fw_header->boot_app_ver.model_id != stm32->ic_fw_ver.model_id) {
		input_info(true, &stm32->client->dev,
				"%s: [BIN] %d / [IC] %d : diffrent model id. do not update\n",
				__func__, stm32->fw_header->boot_app_ver.model_id, stm32->ic_fw_ver.model_id);
		return false;
	}

	if (stm32->fw_header->boot_app_ver.hw_rev != stm32->ic_fw_ver.hw_rev) {
		input_err(true, &stm32->client->dev,
				"%s: [BIN] %d / [IC] %d : diffrent hw rev. do not update\n",
				__func__, stm32->fw_header->boot_app_ver.hw_rev, stm32->ic_fw_ver.hw_rev);
		return false;
	}

	if (fw_ver_bin > fw_ver_ic) {
		input_info(true, &stm32->client->dev, "%s: need to update fw\n", __func__);
		return true;
	} else if (fw_ver_bin == fw_ver_ic && stm32->crc_of_bin != stm32->crc_of_ic) {
		input_err(true, &stm32->client->dev, "%s: CRC mismatch! need to update fw\n", __func__);
		return true;
	} else {
		input_info(true, &stm32->client->dev, "%s: skip fw update\n", __func__);
		return false;
	}
}

static int stm32_dev_kpc_disable(struct stm32_dev *stm32)
{
	stm32_enable_irq(stm32, INT_DISABLE);
	return 0;
}

static void stm32_check_ic_work(struct work_struct *work)
{
	struct stm32_dev *stm32 = container_of((struct delayed_work *)work,
			struct stm32_dev, check_ic_work);
	int ret;
	u8 buff = 0;

	ret = stm32_read_version(stm32);
	if (ret < 0) {
		input_err(true, &stm32->client->dev, "%s: failed to read version, %d\n", __func__, ret);
		return;
	}

	ret = stm32_i2c_reg_read(stm32->client, STM32_CMD_GET_MODE, 1, &buff);
	if (ret < 0) {
		input_err(true, &stm32->client->dev, "%s: failed to read mode, %d\n", __func__, ret);
		return;
	}

	input_info(true, &stm32->client->dev, "%s: [MODE] %d\n", __func__, buff);
	if (buff == MODE_DFU) {
		input_info(true, &stm32->client->dev, "%s: [DFU MODE] abort to boot bank\n", __func__);
		ret = stm32_i2c_reg_write(stm32->client, STM32_CMD_ABORT);
		if (ret < 0) {
			input_err(true, &stm32->client->dev, "%s: failed to change mode, %d\n", __func__, ret);
			return;
		}
	}

	input_info(true, &stm32->client->dev, "%s: [APP MODE]\n", __func__);
}

static int stm32_dev_kpc_enable(struct stm32_dev *stm32)
{
	if (is_fw_dl_completed()) {
		fpga_rstn_control();
		schedule_delayed_work(&stm32->check_ic_work, msecs_to_jiffies(1 * MSEC_PER_SEC));
	} else {
		input_info(true, &stm32->client->dev, "%s: FPGA FW downloding is not done\n", __func__);
	}

	stm32_enable_irq(stm32, INT_ENABLE);

	return 0;
}

static void stm32_dev_int_proc(struct stm32_dev *stm32, bool force_read_buffer)
{
	int i, ret = 0;
	u8 key_values[STM32_MAX_EVENT_COUNT], event_count = STM32_MAX_EVENT_COUNT;

	mutex_lock(&stm32->dev_lock);

	if (!stm32->input_dev) {
		input_err(true, &stm32->client->dev, "%s: input dev is null\n", __func__);
		goto out_proc;
	}

#ifdef CONFIG_PM_SLEEP
	if (!stm32->dev_resume_state) {
		wake_lock_timeout(&stm32->stm_wake_lock, msecs_to_jiffies(3 * MSEC_PER_SEC));
		/* waiting for blsp block resuming, if not occurs i2c error */
		ret = wait_for_completion_interruptible_timeout(&stm32->resume_done,
				msecs_to_jiffies(5 * MSEC_PER_SEC));
		if (ret == 0) {
			input_err(true, &stm32->client->dev,
					"%s: LPM: pm resume is not handled [timeout]\n",
					__func__);
			goto out_proc;
		}
	}
#endif
	fpga_rstn_high();

	if (!force_read_buffer) {
		ret = stm32_i2c_reg_read(stm32->client, STM32_CMD_READ_EVENT_COUNT, 1, &event_count);
		if (ret < 0) {
			input_err(true, &stm32->client->dev,
					"%s: failed to get read event count, %d\n", __func__, ret);
			goto out_proc;
		}
	}

	if (!force_read_buffer && (!event_count || event_count > STM32_MAX_EVENT_COUNT)) {
		if (event_count)
			input_info(true, &stm32->client->dev, "%s: %d event\n", __func__, event_count);

		if (event_count == 0xFF)
			stm32_release_all_key(stm32);

		ret = stm32_read_version(stm32);
		if (ret < 0)
			input_err(true, &stm32->client->dev, "%s: failed to read version\n", __func__);

		goto out_proc;
	}

	ret = stm32_i2c_reg_read(stm32->client, STM32_CMD_READ_EVENT, event_count, key_values);
	if (ret < 0)
		goto out_proc;

	for (i = 0; i < event_count; i++) {
		struct stm32_event_data keydata;
		int code;

		keydata.data[0] = key_values[i];
		code = MATRIX_SCAN_CODE(keydata.row, keydata.col, STM32_KPC_ROW_SHIFT);

		if (keydata.row >= stm32->dtdata->num_row || keydata.col >= stm32->dtdata->num_column)
			continue;

#ifdef GHOST_CHECK_WORKAROUND
		cancel_delayed_work(&stm32->ghost_check_work);
#endif
		if (!keydata.press) {	/* Release */
			stm32->key_state[keydata.row] &= ~(1 << keydata.col);
		} else {		/* Press */
			stm32->key_state[keydata.row] |= (1 << keydata.col);
#ifdef GHOST_CHECK_WORKAROUND
			if (!force_read_buffer)
				schedule_delayed_work(&stm32->ghost_check_work,
						msecs_to_jiffies(400));
#endif
		}

#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
		input_info(true, &stm32->client->dev,
				"%s '%c' 0x%02X(%d) R:C(%d:%d) v%d.%d.%d.%d\n",
				keydata.press ? "P" : "R",
				stm32->key_name[stm32->keycode[code]], stm32->keycode[code],
				stm32->keycode[code], keydata.row, keydata.col,
				stm32->ic_fw_ver.fw_major_ver, stm32->ic_fw_ver.fw_minor_ver,
				stm32->ic_fw_ver.model_id, stm32->ic_fw_ver.hw_rev);
#else
		input_info(true, &stm32->client->dev,
				"%s (%d:%d) v%d.%d.%d.%d\n", keydata.press ? "P" : "R",
				keydata.row, keydata.col,
				stm32->ic_fw_ver.fw_major_ver, stm32->ic_fw_ver.fw_minor_ver,
				stm32->ic_fw_ver.model_id, stm32->ic_fw_ver.hw_rev);
#endif
		input_event(stm32->input_dev, EV_MSC, MSC_SCAN, code);
		input_report_key(stm32->input_dev, stm32->keycode[code], keydata.press);
		input_sync(stm32->input_dev);
	}
	input_dbg(false, &stm32->client->dev, "%s--\n", __func__);

out_proc:
	mutex_unlock(&stm32->dev_lock);
}

static irqreturn_t stm32_dev_isr(int irq, void *dev_id)
{
	struct stm32_dev *stm32 = (struct stm32_dev *)dev_id;

	if (!stm32->current_connect_state) {
		input_dbg(false, &stm32->client->dev, "%s: not connected\n", __func__);
		return IRQ_HANDLED;
	}

	stm32_dev_int_proc(stm32, false);

	return IRQ_HANDLED;
}

#ifdef GHOST_CHECK_WORKAROUND
static void stm32_ghost_check_worker(struct work_struct *work)
{
	struct stm32_dev *stm32 = container_of((struct delayed_work *)work,
			struct stm32_dev, ghost_check_work);
	int i, state = 0;

	if (!stm32->current_connect_state) {
		input_dbg(false, &stm32->client->dev, "%s: not connected\n", __func__);
		return;
	}

	for (i = 0; i < stm32->dtdata->num_row; i++) {
		state |= stm32->key_state[i];
	}

	input_info(true, &stm32->client->dev, "%s: %d\n", __func__, state);
	if (!!state)
		stm32_dev_int_proc(stm32, true);
}
#endif

static void stm32_dev_worker(struct work_struct *work)
{
	struct stm32_dev *stm32 = container_of((struct delayed_work *)work,
			struct stm32_dev, worker);

	stm32_dev_int_proc(stm32, false);

	schedule_delayed_work(&stm32->worker, STM32_TMR_INTERVAL);
}

#ifdef CONFIG_OF
static int stm32_parse_dt(struct device *dev,
		struct stm32_dev *device_data)
{
	struct device_node *np = dev->of_node;
	struct matrix_keymap_data *keymap_data;
	int ret, keymap_len, i;
	u32 *keymap, temp;
	const __be32 *map;
	bool flag;

	device_data->dtdata->gpio_int = of_get_named_gpio(np, "stm32,irq_gpio", 0);
	if (!gpio_is_valid(device_data->dtdata->gpio_int))
		input_err(true, dev, "unable to get gpio_int\n");

	device_data->dtdata->gpio_sda = of_get_named_gpio(np, "stm32,sda_gpio", 0);
	if (!gpio_is_valid(device_data->dtdata->gpio_sda)) {
		input_err(true, dev, "unable to get gpio_sda\n");
		return -EIO;
	}

	device_data->dtdata->gpio_scl = of_get_named_gpio(np, "stm32,scl_gpio", 0);
	if (!gpio_is_valid(device_data->dtdata->gpio_scl)) {
		input_err(true, dev, "unable to get gpio_scl\n");
		return -EIO;
	}

	flag = of_property_read_bool(np, "stm32_vddo-supply");
	if (flag) {
		device_data->dtdata->vdd_vreg = devm_regulator_get(&device_data->client->dev, "stm32_vddo");
		if (IS_ERR(device_data->dtdata->vdd_vreg)) {
			ret = PTR_ERR(device_data->dtdata->vdd_vreg);
			input_err(true, &device_data->client->dev, "%s: could not get vddo, rc = %ld\n",
					__func__, ret);
			device_data->dtdata->vdd_vreg = NULL;
		}
	} else {
		input_err(true, &device_data->client->dev, "%s: not use regulator\n", __func__);
	}

	ret = of_property_read_string(np, "stm32,model_name", &device_data->dtdata->model_name);
	if (ret) {
		input_err(true, dev, "unable to get model_name\n");
		return ret;
	}

	ret = of_property_read_u32(np, "keypad,num-rows", &temp);
	if (ret) {
		input_err(true, dev, "unable to get num-rows\n");
		return ret;
	}
	device_data->dtdata->num_row = temp;

	ret = of_property_read_u32(np, "keypad,num-columns", &temp);
	if (ret) {
		input_err(true, dev, "unable to get num-columns\n");
		return ret;
	}
	device_data->dtdata->num_column = temp;

	map = of_get_property(np, "linux,keymap", &keymap_len);

	if (!map) {
		input_err(true, dev, "Keymap not specified\n");
		return -EINVAL;
	}

	keymap_data = devm_kzalloc(dev, sizeof(*keymap_data), GFP_KERNEL);
	if (!keymap_data) {
		input_err(true, dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	keymap_data->keymap_size = (unsigned int)(keymap_len / sizeof(u32));

	keymap = devm_kzalloc(dev,
			sizeof(uint32_t) * keymap_data->keymap_size, GFP_KERNEL);
	if (!keymap) {
		input_err(true, dev, "could not allocate memory for keymap\n");
		return -ENOMEM;
	}

	for (i = 0; i < keymap_data->keymap_size; i++) {
		unsigned int key = be32_to_cpup(map + i);
		int keycode, row, col;

		row = (key >> 24) & 0xff;
		col = (key >> 16) & 0xff;
		keycode = key & 0xffff;
		keymap[i] = KEY(row, col, keycode);
	}

	/* short keymap for easy finding key */
	device_data->key_name[2] = '1';
	device_data->key_name[3] = '2';
	device_data->key_name[4] = '3';
	device_data->key_name[5] = '4';
	device_data->key_name[6] = '5';
	device_data->key_name[7] = '6';
	device_data->key_name[8] = '7';
	device_data->key_name[9] = '8';
	device_data->key_name[10] = '9';
	device_data->key_name[11] = '0';
	device_data->key_name[12] = '-';
	device_data->key_name[13] = '=';
	device_data->key_name[14] = '<';
	device_data->key_name[15] = 'T';
	device_data->key_name[16] = 'q';
	device_data->key_name[17] = 'w';
	device_data->key_name[18] = 'e';
	device_data->key_name[19] = 'r';
	device_data->key_name[20] = 't';
	device_data->key_name[21] = 'y';
	device_data->key_name[22] = 'u';
	device_data->key_name[23] = 'i';
	device_data->key_name[24] = 'o';
	device_data->key_name[25] = 'p';
	device_data->key_name[26] = '[';
	device_data->key_name[27] = ']';
	device_data->key_name[28] = 'E';
	device_data->key_name[29] = 'C';
	device_data->key_name[30] = 'a';
	device_data->key_name[31] = 's';
	device_data->key_name[32] = 'd';
	device_data->key_name[33] = 'f';
	device_data->key_name[34] = 'g';
	device_data->key_name[35] = 'h';
	device_data->key_name[36] = 'j';
	device_data->key_name[37] = 'k';
	device_data->key_name[38] = 'l';
	device_data->key_name[39] = ';';
	device_data->key_name[40] = '\'';
	device_data->key_name[41] = '`';
	device_data->key_name[42] = 'S';
	device_data->key_name[43] = '\\'; /* US : backslash , UK : pound*/
	device_data->key_name[44] = 'z';
	device_data->key_name[45] = 'x';
	device_data->key_name[46] = 'c';
	device_data->key_name[47] = 'v';
	device_data->key_name[48] = 'b';
	device_data->key_name[49] = 'n';
	device_data->key_name[50] = 'm';
	device_data->key_name[51] = ',';
	device_data->key_name[52] = '.';
	device_data->key_name[53] = '/';
	device_data->key_name[54] = 'S';
	device_data->key_name[56] = 'A';
	device_data->key_name[57] = ' ';
	device_data->key_name[58] = 'C';
	device_data->key_name[86] = '\\'; /* only UK : backslash */
	device_data->key_name[100] = 'A';
	device_data->key_name[103] = 'U';
	device_data->key_name[105] = 'L';
	device_data->key_name[106] = 'R';
	device_data->key_name[108] = 'D';
	device_data->key_name[122] = 'H';
	device_data->key_name[125] = '@';
	device_data->key_name[217] = '@';
	device_data->key_name[523] = '#';
	device_data->key_name[706] = 'I';

	keymap_data->keymap = keymap;
	device_data->keymap_data = keymap_data;
	input_info(true, dev, "%s: scl: %d, sda: %d, int:%d, row:%d, col:%d, keymap size:%d,  model_name:%s\n",
			__func__, device_data->dtdata->gpio_scl, device_data->dtdata->gpio_sda,
			device_data->dtdata->gpio_int,
			device_data->dtdata->num_row, device_data->dtdata->num_column,
			device_data->keymap_data->keymap_size, device_data->dtdata->model_name);
	return 0;
}
#else
static int stm32_parse_dt(struct device *dev,
		struct stm32_dev *device_data)
{
	return -ENODEV;
}
#endif

static int stm32_keypad_start(struct stm32_dev *data)
{
	int ret = 0;

	if (!data)
		return -ENODEV;

	ret = stm32_dev_regulator(data, 1);
	if (ret < 0) {
		input_err(true, &data->client->dev, "%s: regulator on error\n", __func__);
		goto out;
	}

	msleep(50);
	ret = stm32_dev_kpc_enable(data);
	if (ret < 0) {
		input_err(true, &data->client->dev, "%s: enable error\n", __func__);
		goto out;
	}

	input_info(true, &data->client->dev, "%s done\n", __func__);
	return 0;
out:
	input_err(true, &data->client->dev, "%s: failed. int:%d, sda:%d, scl:%d\n", __func__,
			gpio_get_value(data->dtdata->gpio_int),
			gpio_get_value(data->dtdata->gpio_sda),
			gpio_get_value(data->dtdata->gpio_scl));

	stm32_enable_irq(data, INT_DISABLE);

	if (stm32_dev_regulator(data, 0) < 0)
		input_err(true, &data->client->dev, "%s: regulator off error %d\n", __func__);

	input_err(true, &data->client->dev, "%s: schedule keyboard work\n", __func__);
	return ret;
}

static int stm32_keypad_stop(struct stm32_dev *data) {
	int ret = 0;

	if (!data)
		return -ENODEV;

	stm32_dev_kpc_disable(data);

	stm32_release_all_key(data);

	ret = stm32_dev_regulator(data, 0);
	if (ret < 0) {
		input_err(true, &data->client->dev, "%s: regulator off error\n", __func__);
		goto out;
	}

out:
	input_info(true, &data->client->dev, "%s: %s\n", __func__, ret < 0 ? "failed" : "done");
	return ret;
}

static int stm32_set_input_dev(struct stm32_dev *device_data)
{
	struct input_dev *input_dev;
	struct i2c_client *client = device_data->client;
	int ret = 0;

	if (device_data->input_dev) {
		input_err(true, &client->dev, "input dev already exist\n");
		return ret;
	}

	input_dev = input_allocate_device();
	if (!input_dev) {
		input_err(true, &client->dev,
				"Failed to allocate memory for input device\n");
		return -ENOMEM;
	}

	device_data->input_dev = input_dev;
	device_data->input_dev->dev.parent = &client->dev;
	device_data->input_dev->name = "Tab S4 Book Cover Keyboard";
	device_data->input_dev->id.bustype = BUS_I2C;
	device_data->input_dev->id.vendor = INPUT_VENDOR_ID_SAMSUNG;
	device_data->input_dev->id.product = INPUT_PRODUCT_ID_POGO_KEYBOARD;
	device_data->input_dev->flush = NULL;
	device_data->input_dev->event = NULL;

	input_set_drvdata(device_data->input_dev, device_data);
	input_set_capability(device_data->input_dev, EV_MSC, MSC_SCAN);
	set_bit(EV_KEY, device_data->input_dev->evbit);
	set_bit(KEY_SLEEP, device_data->input_dev->keybit);

	device_data->input_dev->keycode = device_data->keycode;
	device_data->input_dev->keycodesize = sizeof(unsigned short);
	device_data->input_dev->keycodemax = device_data->keycode_entries;

	matrix_keypad_build_keymap(device_data->keymap_data, NULL,
			device_data->dtdata->num_row, device_data->dtdata->num_column,
			device_data->keycode, device_data->input_dev);

	ret = input_register_device(device_data->input_dev);
	if (ret) {
		input_err(true, &client->dev,
				"Failed to register input device\n");
		input_free_device(device_data->input_dev);
		device_data->input_dev = NULL;
	}
	input_info(true, &client->dev, "%s done\n", __func__);
	return ret;
}

static void stm32_keyboard_connect_work(struct work_struct *work)
{
	struct stm32_dev *data = container_of((struct delayed_work *)work,
			struct stm32_dev, keyboard_work);
	int ret = 0;

	if (data->connect_state == data->current_connect_state) {
		input_err(true, &data->client->dev,
				"%s: already %sconnected\n",
				__func__, data->connect_state ? "" : "dis");
		return;
	}

	input_info(true, &data->client->dev, "%s: %d\n", __func__, data->connect_state);

	mutex_lock(&data->dev_lock);
#ifndef CONFIG_SEC_FACTORY
	if (!data->dev_resume_state)
		wake_lock_timeout(&data->stm_wake_lock, msecs_to_jiffies(3 * MSEC_PER_SEC));
#endif
#ifdef GHOST_CHECK_WORKAROUND
	cancel_delayed_work(&data->ghost_check_work);
#endif
	cancel_delayed_work(&data->check_ic_work);

	if (data->connect_state) {
		ret = stm32_set_input_dev(data);
		if (ret)
			goto out;
		ret = stm32_keypad_start(data);
	} else {
		ret = stm32_keypad_stop(data);
	}

	if (data->input_dev) {
		if (ret >= 0) {
#ifndef CONFIG_SEC_FACTORY
			if (!data->connect_state) {
				input_report_key(data->input_dev, KEY_SLEEP, 1);
				input_sync(data->input_dev);
				input_report_key(data->input_dev, KEY_SLEEP, 0);
				input_sync(data->input_dev);
				input_info(true, &data->client->dev, "%s: make sleep\n", __func__);
			}
#endif
			data->current_connect_state = data->connect_state;
		}

		if (!data->current_connect_state) {
			usleep_range(1000, 1000);
			input_unregister_device(data->input_dev);
			data->input_dev = NULL;
			input_info(true, &data->client->dev,
					"%s: input dev unregistered\n", __func__);
		}
	}

out:
	mutex_unlock(&data->dev_lock);
}

static int stm32_keyboard_notifier(struct notifier_block *nb,
		unsigned long action, void *data)
{
	struct stm32_dev *devdata = container_of(nb, struct stm32_dev, keyboard_nb);
	int state = !!action;

	if (devdata->connect_state == state)
		goto out;

	input_info(true, &devdata->client->dev, "%s: current %d(%d) change to %d\n",
			__func__, devdata->current_connect_state, devdata->connect_state, state);
	cancel_delayed_work_sync(&devdata->keyboard_work);
	devdata->connect_state = state;
	schedule_delayed_work(&devdata->keyboard_work, msecs_to_jiffies(1));

out:
	return NOTIFY_DONE;
}

static ssize_t sysfs_key_onoff_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct stm32_dev *device_data = dev_get_drvdata(dev);
	int state = 0;
	int i;

	for (i = 0; i < device_data->dtdata->num_row; i++) {
		state |= device_data->key_state[i];
	}

	input_info(true, &device_data->client->dev,
			"%s: key state:%d\n", __func__, !!state);

	return snprintf(buf, 5, "%d\n", !!state);
}

static ssize_t keyboard_connected_store(struct device *dev,
		struct device_attribute *attr, const char *buf,
		size_t size)
{
	struct stm32_dev *data = dev_get_drvdata(dev);
	int onoff, err;

	if (strlen(buf) > 2) {
		input_err(true, &data->client->dev, "%s: cmd lenth is over %d\n",
				__func__, (int)strlen(buf));
		return -EINVAL;
	}

	err = kstrtoint(buf, 10, &onoff);
	if (err)
		return err;

	if (data->connect_state == !!onoff)
		return size;

	input_info(true, &data->client->dev, "%s: current %d change to %d\n",
			__func__, data->current_connect_state, data->connect_state);

	cancel_delayed_work_sync(&data->keyboard_work);
	data->connect_state = !!onoff;
	schedule_delayed_work(&data->keyboard_work, msecs_to_jiffies(1));
	return size;
}

static ssize_t keyboard_connected_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct stm32_dev *data = dev_get_drvdata(dev);

	input_info(true, dev, "%s: %d\n", __func__, data->current_connect_state);

	return snprintf(buf, 5, "%d\n", data->current_connect_state);
}

static ssize_t keyboard_get_fw_ver_ic(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct stm32_dev *stm32 = dev_get_drvdata(dev);
	char buff[256] = { 0 };
	int ret;

	fpga_rstn_control();
	msleep(20);

	ret = stm32_read_version(stm32);
	if (ret < 0)
		return snprintf(buf, 3, "NG");

	snprintf(buff, sizeof(buff), "%s_v%d.%d.%d.%d",
			stm32->dtdata->model_name,
			stm32->ic_fw_ver.fw_major_ver,
			stm32->ic_fw_ver.fw_minor_ver,
			stm32->ic_fw_ver.model_id,
			stm32->ic_fw_ver.hw_rev);

	input_info(true, &stm32->client->dev, "%s: %s\n", __func__, buff);
	return snprintf(buf, sizeof(buff), buff);
}

static ssize_t keyboard_get_fw_ver_bin(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct stm32_dev *stm32 = dev_get_drvdata(dev);
	char buff[256] = { 0 };

	if (stm32_load_fw_from_fota(stm32, false) < 0) {
		input_err(true, &stm32->client->dev, "%s: failed to read bin data\n", __func__);
		return snprintf(buf, sizeof(buff), "NG");
	}

	snprintf(buff, sizeof(buff), "%s_v%d.%d.%d.%d",
			stm32->dtdata->model_name,
			stm32->fw_header->boot_app_ver.fw_major_ver,
			stm32->fw_header->boot_app_ver.fw_minor_ver,
			stm32->fw_header->boot_app_ver.model_id,
			stm32->fw_header->boot_app_ver.hw_rev);

	input_info(true, &stm32->client->dev, "%s: %s\n", __func__, buff);
	return snprintf(buf, sizeof(buff), buff);
}

static ssize_t keyboard_get_crc(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct stm32_dev *stm32 = dev_get_drvdata(dev);
	char buff[256] = { 0 };
	int ret;

	fpga_rstn_control();
	msleep(20);

	ret = stm32_read_crc(stm32);
	if (ret < 0)
		return snprintf(buf, 3, "NG");

	snprintf(buff, sizeof(buff), "%08X", stm32->crc_of_ic);

	input_info(true, &stm32->client->dev, "%s: %s\n", __func__, buff);
	return snprintf(buf, sizeof(buff), buff);
}

static ssize_t keyboard_check_fw_update(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct stm32_dev *stm32 = dev_get_drvdata(dev);

	fpga_rstn_control();
	msleep(20);

	if (stm32_check_fw_update(stm32))
		return snprintf(buf, 3, "OK");
	else
		return snprintf(buf, 3, "NG");
}

static int stm32_fw_update(struct stm32_dev *stm32, const struct firmware *fw)
{
	int ret, i, packets_per_pages, page_index = 0, page_num, retry = 3;
	u8 buff[4] = { 0 };
	u16 page_size;
	struct stm32_fw_version target_fw;
	u32 target_crc, fw_crc;
	struct stm32_fw_header *fw_header;

	fw_header = (struct stm32_fw_header *)(fw->data + STM32_FW_APP_CFG_OFFSET);
	fw_crc = stm32_crc32((u8 *)fw->data, fw->size);

	input_info(true, &stm32->client->dev,
			"%s: [FW] %s, version:%d.%d, model_id:%d, hw_rev:%d, CRC:0x%X\n",
			__func__, fw_header->magic_word,
			fw_header->boot_app_ver.fw_major_ver,
			fw_header->boot_app_ver.fw_minor_ver,
			fw_header->boot_app_ver.model_id,
			fw_header->boot_app_ver.hw_rev, fw_crc);

	if (strncmp(STM32_MAGIC_WORD, fw_header->magic_word, 7) != 0) {
		input_info(true, &stm32->client->dev, "%s: firmware file is wrong : %s\n",
				__func__, fw_header->magic_word);
		return -ENOENT;
	}

	input_info(true, &stm32->client->dev, "%s: [DFU MODE] enter\n", __func__);
	ret = stm32_i2c_reg_write(stm32->client, STM32_CMD_ENTER_DFU_MODE);
	if (ret < 0)
		return ret;

	stm32_enable_irq(stm32, INT_DISABLE);

	msleep(60);

	ret = stm32_i2c_reg_read(stm32->client, STM32_CMD_GET_PROTOCOL_VERSION, 2, buff);
	if (ret < 0)
		goto out;
	input_info(true, &stm32->client->dev, "%s: protocol ver: 0x%02X%02X\n", __func__, buff[1], buff[0]);

	ret = stm32_i2c_reg_read(stm32->client, STM32_CMD_GET_TARGET_BANK, 1, buff);
	input_info(true, &stm32->client->dev, "%s: target bank: %02X\n", __func__, buff[0]);
	if (ret < 0 || buff[0] == 0xFF)
		goto out;

	ret = stm32_i2c_reg_read(stm32->client, STM32_CMD_GET_PAGE_SIZE, 2, buff);
	if (ret < 0)
		goto out;

	memcpy(&page_size, &buff[0], 2);
	packets_per_pages = page_size / STM32_CFG_PACKET_SIZE;
	page_num = fw->size / page_size;
	input_info(true, &stm32->client->dev,
			"%s: fw size: %d, page size: %d, page num:%d, packet size: %d, packets per pages: %d\n",
			__func__, fw->size, page_size, page_num, STM32_CFG_PACKET_SIZE, packets_per_pages);

	/* disable write protection of target bank & erase the target bank */
	input_info(true, &stm32->client->dev, "%s: start fw update\n", __func__);
	ret = stm32_i2c_reg_write(stm32->client, STM32_CMD_START_FW_UPGRADE);
	if (ret < 0)
		goto out;

	ret = stm32_i2c_block_read(stm32->client, 1, buff);
	if (ret < 0)
		goto out;
	input_info(true, &stm32->client->dev, "%s: bank %02X is erased\n", __func__, buff[0]);

	while (page_index <= page_num) {
		u8 *pn_data = (u8 *)&fw->data[page_index * page_size];
		u32 page_crc;
		u16 page_index_cmp;

		if (page_index < page_num) {
			/* write pages */
			ret = stm32_i2c_reg_write(stm32->client, STM32_CMD_WRITE_PAGE);
			if (ret < 0)
				goto out;
		} else {
			/* write last page */
			ret = stm32_i2c_reg_write(stm32->client, STM32_CMD_WRITE_LAST_PAGE);
			if (ret < 0)
				goto out;

			page_size = fw->size % page_size;
			packets_per_pages = page_size / STM32_CFG_PACKET_SIZE;

			memset(buff, 0, 4);
			memcpy(&buff[0], &page_size, 2);
			input_dbg(false, &stm32->client->dev, "%s: last page size: %X\n",
					__func__, page_size);

			ret = stm32_i2c_block_write(stm32->client, 2, buff);
			if (ret < 0)
				goto out;
		}

		input_dbg(false, &stm32->client->dev, "%s: write page %d, size %d\n",
				__func__, page_index, page_size);

		for (i = 0; i < packets_per_pages; i++) {
			/* write packets */
			ret = stm32_i2c_block_write(stm32->client, STM32_CFG_PACKET_SIZE,
					&pn_data[i * STM32_CFG_PACKET_SIZE]);
			if (ret < 0)
				goto out;
		}

		if (page_index == page_num) {
			u16 last_packet_size = page_size % STM32_CFG_PACKET_SIZE;

			/* write last packet */
			input_dbg(false, &stm32->client->dev, "%s: write page %d's last packet %d, size %d\n",
					__func__, page_index, packets_per_pages, last_packet_size);
			ret = stm32_i2c_block_write(stm32->client, last_packet_size,
					&pn_data[packets_per_pages * STM32_CFG_PACKET_SIZE]);
			if (ret < 0)
				goto out;
		}

		page_crc = stm32_crc32(pn_data, page_size);
		memcpy(&buff[0], &page_crc, 4);
		input_dbg(false, &stm32->client->dev, "%s: page %d CRC:0x%08X\n",
				__func__, page_index, page_crc, page_crc);
		/* write page crc */
		ret = stm32_i2c_block_write(stm32->client, 4, buff);
		if (ret < 0)
			goto out;

		/* read page index */
		ret = stm32_i2c_block_read(stm32->client, 2, buff);
		if (ret < 0)
			goto out;
		memcpy(&page_index_cmp, buff, 2);
		page_index++;

		if (page_index == page_index_cmp) {
			retry = 3;
		} else {
			page_index--;
			input_err(true, &stm32->client->dev, "%s: page %d(%d) is not programmed, retry %d\n",
					__func__, page_index, page_index_cmp, 3 - retry);
			retry--;
		}

		if (retry < 0) {
			input_err(true, &stm32->client->dev, "%s: failed\n", __func__);
			goto out;
		}
	}

	input_info(true, &stm32->client->dev, "%s: writing is done\n", __func__);

	/* check fw version & crc of target bank */
	ret = stm32_i2c_reg_read(stm32->client, STM32_CMD_GET_TARGET_FW_VERSION, 4, buff);
	if (ret < 0)
		goto out;

	target_fw.hw_rev = buff[0];
	target_fw.model_id = buff[1];
	target_fw.fw_minor_ver = buff[2];
	target_fw.fw_major_ver = buff[3];

	input_info(true, &stm32->client->dev, "%s: [TARGET] version:%d.%d, model_id:%d\n",
			__func__, target_fw.fw_major_ver, target_fw.fw_minor_ver, target_fw.model_id);

	if (target_fw.fw_major_ver != fw_header->boot_app_ver.fw_major_ver ||
		target_fw.fw_minor_ver != fw_header->boot_app_ver.fw_minor_ver) {
		input_err(true, &stm32->client->dev, "%s: version mismatch!\n", __func__);
		goto out;
	}

	ret = stm32_i2c_reg_read(stm32->client, STM32_CMD_GET_TARGET_FW_CRC32, 4, buff);
	if (ret < 0)
		goto out;

	target_crc = buff[3] << 24 | buff[2] << 16 | buff[1] << 8 | buff[0];

	input_info(true, &stm32->client->dev, "%s: [TARGET] CRC:0x%08X\n", __func__, target_crc);
	if (target_crc != fw_crc) {
		input_err(true, &stm32->client->dev, "%s: CRC mismatch!\n", __func__);
		goto out;
	}

	/* leave the DFU mode with switching to target bank */
	ret = stm32_i2c_reg_write(stm32->client, STM32_CMD_GO);
	if (ret < 0)
		goto out;

	input_info(true, &stm32->client->dev, "%s: [APP MODE] Start with target bank\n", __func__);

	msleep(100);
	stm32_enable_irq(stm32, INT_ENABLE);
	return 0;

out:
	msleep(2000);

	/* leave the DFU mode without switching to target bank */
	ret = stm32_i2c_reg_write(stm32->client, STM32_CMD_ABORT);
	if (ret < 0)
		return ret;

	input_info(true, &stm32->client->dev, "%s: [APP MODE] Start with boot bank\n", __func__);

	msleep(100);
	stm32_enable_irq(stm32, INT_ENABLE);
	return -EIO;
}

static ssize_t keyboard_fw_update(struct device *dev,
		struct device_attribute *attr, const char *buf,
		size_t size)
{
	struct stm32_dev *stm32 = dev_get_drvdata(dev);
	int ret, param;

	if (!stm32->current_connect_state)
		return -ENODEV;

	ret = kstrtoint(buf, 10, &param);
	if (ret)
		return ret;

	switch (param) {
	case FW_UPDATE_FOTA:
		ret = stm32_load_fw_from_fota(stm32, true);
		if (ret < 0)
			return ret;
		else
			return size;
	default:
		return -EINVAL;
	}
}

static ssize_t keyboard_debug_level_store(struct device *dev,
		struct device_attribute *attr, const char *buf,
		size_t size)
{
	struct stm32_dev *stm32 = dev_get_drvdata(dev);
	int ret, level;

	ret = kstrtoint(buf, 10, &level);
	if (ret)
		return ret;

	stm32->debug_level = level;

	input_info(true, &stm32->client->dev, "%s: %d\n", __func__, stm32->debug_level);
	return size;
}

static ssize_t keyboard_debug_level_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct stm32_dev *stm32 = dev_get_drvdata(dev);

	input_info(true, &stm32->client->dev, "%s: %d\n", __func__, stm32->debug_level);

	return snprintf(buf, 5, "%d\n", stm32->debug_level);
}

static ssize_t keyboard_i2c_write(struct device *dev,
		struct device_attribute *attr, const char *buf,
		size_t size)
{
	struct stm32_dev *stm32 = dev_get_drvdata(dev);
	int ret, reg;

	ret = kstrtoint(buf, 0, &reg);
	if (ret)
		return ret;

	if (reg > 0xFF)
		return -EIO;

	ret = stm32_i2c_reg_write(stm32->client, reg);

	input_info(true, &stm32->client->dev, "%s: write 0x%02X, ret: %d\n", __func__, reg, ret);
	return size;
}

static DEVICE_ATTR(sec_key_pressed, 0444 , sysfs_key_onoff_show, NULL);
static DEVICE_ATTR(keyboard_connected, 0644, keyboard_connected_show, keyboard_connected_store);
static DEVICE_ATTR(get_fw_ver_bin, 0444, keyboard_get_fw_ver_bin, NULL);
static DEVICE_ATTR(get_fw_ver_ic, 0444, keyboard_get_fw_ver_ic, NULL);
static DEVICE_ATTR(get_crc, 0444, keyboard_get_crc, NULL);
static DEVICE_ATTR(check_fw_update, 0444, keyboard_check_fw_update, NULL);
static DEVICE_ATTR(fw_update, 0220, NULL, keyboard_fw_update);
static DEVICE_ATTR(debug, 0644, keyboard_debug_level_show, keyboard_debug_level_store);
static DEVICE_ATTR(write_cmd, 0200, NULL, keyboard_i2c_write);

static struct attribute *key_attributes[] = {
	&dev_attr_sec_key_pressed.attr,
	&dev_attr_keyboard_connected.attr,
	&dev_attr_get_fw_ver_bin.attr,
	&dev_attr_get_fw_ver_ic.attr,
	&dev_attr_get_crc.attr,
	&dev_attr_check_fw_update.attr,
	&dev_attr_fw_update.attr,
	&dev_attr_debug.attr,
	&dev_attr_write_cmd.attr,
	NULL,
};

static struct attribute_group key_attr_group = {
	.attrs = key_attributes,
};

static int stm32_dev_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct stm32_dev *device_data;
	int ret = 0;

	input_err(true, &client->dev, "%s++\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		input_err(true, &client->dev,
				"i2c_check_functionality fail\n");
		return -EIO;
	}

	device_data = kzalloc(sizeof(struct stm32_dev), GFP_KERNEL);
	if (!device_data) {
		input_err(true, &client->dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}

	device_data->client = client;

	mutex_init(&device_data->dev_lock);
	mutex_init(&device_data->irq_lock);
	wake_lock_init(&device_data->stm_wake_lock, WAKE_LOCK_SUSPEND, "stm_key wake lock");
#ifdef CONFIG_PM_SLEEP
	init_completion(&device_data->resume_done);
	device_data->dev_resume_state = true;
#endif
	device_data->client = client;

	if (client->dev.of_node) {
		device_data->dtdata = devm_kzalloc(&client->dev,
				sizeof(struct stm32_devicetree_data), GFP_KERNEL);
		if (!device_data->dtdata) {
			input_err(true, &client->dev, "Failed to allocate memory\n");
			ret = -ENOMEM;
			goto err_config;
		}
		ret = stm32_parse_dt(&client->dev, device_data);
		if (ret) {
			input_err(true, &client->dev, "Failed to use device tree\n");
			ret = -EIO;
			goto err_config;
		}

	} else {
		input_err(true, &client->dev, "No use device tree\n");
		device_data->dtdata = client->dev.platform_data;
		if (!device_data->dtdata) {
			input_err(true, &client->dev, "failed to get platform data\n");
			ret = -ENOENT;
			goto err_config;
		}
	}

	/* Get pinctrl if target uses pinctrl */
	device_data->pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR(device_data->pinctrl)) {
		if (PTR_ERR(device_data->pinctrl) == -EPROBE_DEFER) {
			ret = PTR_ERR(device_data->pinctrl);
			goto err_config;
		}

		input_err(true, &client->dev, "%s: Target does not use pinctrl\n", __func__);
		device_data->pinctrl = NULL;
	}

	device_data->keycode_entries = stm32_get_keycode_entries(device_data,
			((1 << device_data->dtdata->num_row) - 1),
			((1 << device_data->dtdata->num_column) - 1));
	input_info(true, &client->dev, "%s keycode entries (%d)\n",
			__func__, device_data->keycode_entries);
	input_info(true, &client->dev, "%s keymap size (%d)\n",
			__func__, device_data->keymap_data->keymap_size);

	device_data->key_state = kzalloc(device_data->dtdata->num_row * sizeof(int),
			GFP_KERNEL);
	if (!device_data->key_state) {
		input_err(true, &client->dev, "key_state kzalloc memory error\n");
		ret = -ENOMEM;
		goto err_keystate;
	}

	device_data->keycode = kzalloc(device_data->keycode_entries * sizeof(unsigned short),
			GFP_KERNEL);
	if (!device_data->keycode) {
		input_err(true, &client->dev, "keycode kzalloc memory error\n");
		ret = -ENOMEM;
		goto err_keycode;
	}
	input_dbg(true, &client->dev, "%s keycode addr (%p)\n", __func__, device_data->keycode);

	device_data->fw_header = kzalloc(sizeof(struct stm32_fw_header), GFP_KERNEL);
	if (!device_data->fw_header) {
		input_err(true, &client->dev, "fw header kzalloc memory error\n");
		ret = -ENOMEM;
		goto err_alloc_fw_header;
	}

	device_data->fw = kzalloc(sizeof(struct firmware), GFP_KERNEL);
	if (!device_data->fw) {
		ret = -ENOMEM;
		goto err_alloc_fw;
	}

	device_data->fw->data = kzalloc(STM32_FW_SIZE, GFP_KERNEL);
	if (!device_data->fw->data) {
		ret = -ENOMEM;
		goto err_alloc_fw_data;
	}

	i2c_set_clientdata(client, device_data);

#ifdef GHOST_CHECK_WORKAROUND
	INIT_DELAYED_WORK(&device_data->ghost_check_work, stm32_ghost_check_worker);
#endif
	if (gpio_is_valid(device_data->dtdata->gpio_int)) {
		device_data->dev_irq = gpio_to_irq(device_data->dtdata->gpio_int);
		input_info(true, &client->dev,
				"%s INT mode (%d)\n", __func__, device_data->dev_irq);

		ret = request_threaded_irq(device_data->dev_irq, NULL, stm32_dev_isr,
						IRQF_TRIGGER_LOW | IRQF_ONESHOT,
						client->name, device_data);
		if (ret < 0) {
			input_err(true, &client->dev, "stm32_dev_power_off error\n");
			goto interrupt_err;
		}
	} else {
		input_info(true, &client->dev, "%s poll mode\n", __func__);
		INIT_DELAYED_WORK(&device_data->worker, stm32_dev_worker);
	}
	stm32_enable_irq(device_data, INT_DISABLE);

#ifdef CONFIG_SEC_SYSFS
	device_data->sec_pogo_keyboard = sec_device_create(13, device_data, "sec_keypad");
#else
	device_data->sec_pogo_keyboard = device_create(sec_class, NULL,
			13, device_data, "sec_keypad");
#endif
	if (IS_ERR(device_data->sec_pogo_keyboard)) {
		input_err(true, &client->dev, "Failed to create sec_keypad device\n");
		ret = PTR_ERR(device_data->sec_pogo_keyboard);
		goto err_create_device;
	}

	ret = sysfs_create_group(&device_data->sec_pogo_keyboard->kobj, &key_attr_group);
	if (ret) {
		input_err(true, &client->dev, "Failed to create the test sysfs: %d\n", ret);
		goto err_create_group;
	}

	device_init_wakeup(&client->dev, 1);

	INIT_DELAYED_WORK(&device_data->check_ic_work, stm32_check_ic_work);
	INIT_DELAYED_WORK(&device_data->keyboard_work, stm32_keyboard_connect_work);
	keyboard_notifier_register(&device_data->keyboard_nb,
			stm32_keyboard_notifier, KEYBOARD_NOTIFY_DEV_TSP);
	input_info(true, &client->dev, "%s done\n", __func__);

	return ret;

	//sysfs_remove_group(&device_data->sec_pogo_keyboard->kobj, &key_attr_group);
err_create_group:
#ifdef CONFIG_SEC_SYSFS
	sec_device_destroy(13);
#else
	device_destroy(sec_class, 13);
#endif
err_create_device:
interrupt_err:
	kfree(device_data->fw->data);
err_alloc_fw_data:
	kfree(device_data->fw);
err_alloc_fw:
	kfree(device_data->fw_header);
err_alloc_fw_header:
	kfree(device_data->keycode);
err_keycode:
	kfree(device_data->key_state);
err_keystate:
err_config:
	mutex_destroy(&device_data->dev_lock);
	mutex_destroy(&device_data->irq_lock);
	wake_lock_destroy(&device_data->stm_wake_lock);

	kfree(device_data);

	input_err(true, &client->dev, "Error at stm32_dev_probe\n");

	return ret;
}

static int stm32_dev_remove(struct i2c_client *client)
{
	struct stm32_dev *device_data = i2c_get_clientdata(client);

#ifdef GHOST_CHECK_WORKAROUND
	cancel_delayed_work_sync(&device_data->ghost_check_work);
#endif
	cancel_delayed_work_sync(&device_data->check_ic_work);
	device_init_wakeup(&client->dev, 0);
	wake_lock_destroy(&device_data->stm_wake_lock);

	if (device_data->input_dev) {
		input_unregister_device(device_data->input_dev);
		device_data->input_dev = NULL;
	}

	stm32_dev_regulator(device_data, 0);

	if (gpio_is_valid(device_data->dtdata->gpio_int))
		free_irq(device_data->dev_irq, device_data);
	else
		cancel_delayed_work(&device_data->worker);

#ifdef CONFIG_SEC_SYSFS
	sec_device_destroy(13);
#else
	device_destroy(sec_class, 13);
#endif

	kfree(device_data->fw->data);
	kfree(device_data->fw);
	kfree(device_data->fw_header);
	kfree(device_data->keycode);
	kfree(device_data);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int stm32_dev_suspend(struct device *dev)
{
	struct stm32_dev *device_data = dev_get_drvdata(dev);

	input_dbg(false, &device_data->client->dev, "%s\n", __func__);

	device_data->dev_resume_state = false;
	reinit_completion(&device_data->resume_done);

	if (device_data->current_connect_state && device_may_wakeup(dev)) {
		if (gpio_is_valid(device_data->dtdata->gpio_int)) {
			enable_irq_wake(device_data->dev_irq);
			device_data->irq_wake = true;
			input_info(false, &device_data->client->dev,
					"%s enable irq wake\n", __func__);
		}
	}

	return 0;
}

static int stm32_dev_resume(struct device *dev)
{
	struct stm32_dev *device_data = dev_get_drvdata(dev);

	input_dbg(false, &device_data->client->dev, "%s\n", __func__);

	device_data->dev_resume_state = true;
	complete_all(&device_data->resume_done);

	if (device_data->irq_wake && device_may_wakeup(dev)) {
		if (gpio_is_valid(device_data->dtdata->gpio_int)) {
			disable_irq_wake(device_data->dev_irq);
			device_data->irq_wake = false;
			input_info(false, &device_data->client->dev,
					"%s disable irq wake\n", __func__);
		}
	}

	return 0;
}

static SIMPLE_DEV_PM_OPS(stm32_dev_pm_ops,
		stm32_dev_suspend, stm32_dev_resume);
#endif

static const struct i2c_device_id stm32_dev_id[] = {
	{ STM32_DRV_NAME, 0 },
	{ }
};

#ifdef CONFIG_OF
static struct of_device_id stm32_match_table[] = {
	{ .compatible = "stm,stm32kbd",},
	{ },
};
#else
#define stm32_match_table NULL
#endif

static struct i2c_driver stm32_dev_driver = {
	.driver = {
		.name = STM32_DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = stm32_match_table,
#ifdef CONFIG_PM_SLEEP
		.pm = &stm32_dev_pm_ops,
#endif
	},
	.probe = stm32_dev_probe,
	.remove = stm32_dev_remove,
	.id_table = stm32_dev_id,
};

static int __init stm32_dev_init(void)
{
	pr_err("%s++\n", __func__);

	return i2c_add_driver(&stm32_dev_driver);
}
static void __exit stm32_dev_exit(void)
{
	i2c_del_driver(&stm32_dev_driver);
}
//module_init(stm32_dev_init);
late_initcall(stm32_dev_init);
module_exit(stm32_dev_exit);

MODULE_DEVICE_TABLE(i2c, stm32_dev_id);

MODULE_DESCRIPTION("STM32 Key Driver");
MODULE_AUTHOR("Samsung");
MODULE_LICENSE("GPL");
