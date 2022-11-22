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
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif
#ifdef CONFIG_SEC_SYSFS
#include <linux/sec_class.h>
#else
extern struct class *sec_class;
#endif

#define STMPE1801_DRV_DESC			"stmpe1801 i2c I/O expander"
#define STMPE1801_DRV_NAME			"stmpe1801_pogo"

#define INPUT_VENDOR_ID_SAMSUNG			0x04E8
#define INPUT_PRODUCT_ID_POGO_KEYBOARD		0xA035

#define STMPE1801_BLOCK_GPIO			0x01
#define STMPE1801_BLOCK_KEY			0x02

#define STMPE1801_DEBOUNCE_30US			0
#define STMPE1801_DEBOUNCE_90US			(1 << 1)
#define STMPE1801_DEBOUNCE_150US		(2 << 1)
#define STMPE1801_DEBOUNCE_210US		(3 << 1)

#define STMPE1801_SCAN_FREQ_MASK		0x03
#define STMPE1801_SCAN_FREQ_60HZ		0x00
#define STMPE1801_SCAN_FREQ_30HZ		0x01
#define STMPE1801_SCAN_FREQ_15HZ		0x02
#define STMPE1801_SCAN_FREQ_275HZ		0x03

#define STMPE1801_SCAN_CNT_MASK			0xF0
#define STMPE1801_SCAN_CNT_SHIFT		4

#define STMPE1801_INT_CTRL_ACTIVE_LOW		0x00
#define STMPE1801_INT_CTRL_ACTIVE_HIGH		0x04
#define STMPE1801_INT_CTRL_TRIG_LEVEL		0x00
#define STMPE1801_INT_CTRL_TRIG_EDGE		0x02
#define STMPE1801_INT_CTRL_GLOBAL_ON		0x01
#define STMPE1801_INT_CTRL_GLOBAL_OFF		0x00

#define STMPE1801_INT_MASK_GPIO			0x08
#define STMPE1801_INT_MASK_KEY			0x02
#define STMPE1801_INT_MASK_KEY_COMBI		0x10
#define STMPE1801_INT_MASK_KEY_FIFO		0x04

#define STMPE1801_INT_STA_WAKEUP		0x01
#define STMPE1801_INT_STA_KEY			0x02
#define STMPE1801_INT_STA_KEY_FIFO		0x04
#define STMPE1801_INT_STA_GPIO			0x08
#define STMPE1801_INT_STA_KEY_COMBI		0x10

/*#define STMPE1801_TRIGGER_TYPE_FALLING */
#define STMPE1801_TMR_INTERVAL	10L

/*
 * Definitions & globals.
 */
#define STMPE1801_MAXGPIO			18
#define STMPE1801_KEY_FIFO_LENGTH		10

#define STMPE1801_REG_SYS_CHIPID		0x00
#define STMPE1801_REG_SYS_CTRL			0x02

#define STMPE1801_REG_INT_CTRL_LOW		0x04
#define STMPE1801_REG_INT_MASK_LOW		0x06
#define STMPE1801_REG_INT_STA_LOW		0x08

#define STMPE1801_REG_INT_GPIO_MASK_LOW		0x0A
#define STMPE1801_REG_INT_GPIO_STA_LOW		0x0D

#define STMPE1801_REG_GPIO_SET_yyy		0x10
#define STMPE1801_REG_GPIO_CLR_yyy		0x13
#define STMPE1801_REG_GPIO_MP_yyy		0x16
#define STMPE1801_REG_GPIO_SET_DIR_yyy		0x19
#define STMPE1801_REG_GPIO_RE_yyy		0x1C
#define STMPE1801_REG_GPIO_FE_yyy		0x1F
#define STMPE1801_REG_GPIO_PULL_UP_yyy		0x22

#define STMPE1801_REG_KPC_ROW			0x30
#define STMPE1801_REG_KPC_COL_LOW		0x31
#define STMPE1801_REG_KPC_CTRL_LOW		0x33
#define STMPE1801_REG_KPC_CTRL_MID		0x34
#define STMPE1801_REG_KPC_CTRL_HIGH		0x35
#define STMPE1801_REG_KPC_CMD			0x36
#define STMPE1801_REG_KPC_DATA_BYTE0		0x3A


#define STMPE1801_KPC_ROW_SHIFT			4 // can be 3

#define STMPE1801_KPC_DATA_UP			(0x80)
#define STMPE1801_KPC_DATA_COL			(0x78)
#define STMPE1801_KPC_DATA_ROW			(0x07)
#define STMPE1801_KPC_DATA_NOKEY		(0x0f)

union stmpe1801_kpc_data {
	uint64_t	value;
	uint8_t		array[8];
};

struct stmpe1801_dev {
	int					dev_irq;
	struct i2c_client			*client;
	struct input_dev			*input_dev;
	struct delayed_work			worker;
	struct delayed_work			ghost_check_work;
	struct mutex				dev_lock;
	struct wake_lock			stmpe_wake_lock;
	atomic_t				dev_enabled;
	atomic_t				dev_init;
#ifdef CONFIG_PM_SLEEP
	bool					dev_resume_state;
	struct completion			resume_done;
	bool					irq_wake;
#endif
	struct device				*sec_pogo_keyboard;

	u8					regs_sys_int_init[4];
	u8					regs_kpc_init[8];
	u8					regs_pullup_init[3];

	struct matrix_keymap_data		*keymap_data;
	unsigned short				*keycode;
	int					keycode_entries;

	bool					probe_done;

	struct stmpe1801_devicetree_data	*dtdata;
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
};

struct stmpe1801_devicetree_data {
	int gpio_int;
	int gpio_sda;
	int gpio_scl;
	int block_type;
	int debounce;
	int scan_cnt;
	int repeat;
	int freq;
	int kpc_debounce;
	int num_row;
	int num_column;
	struct regulator *vdd_vreg;
};

static struct stmpe1801_dev *copy_device_data;

static int __stmpe1801_get_keycode_entries(struct stmpe1801_dev *stmpe1801,
		u32 mask_row, u32 mask_col)
{
	int col, row;
	int i, j, comp;

	row = mask_row & 0xff;
	col = mask_col & 0x3ff;

	for (i = 8; i > 0; i--) {
		comp = 1 << (i - 1);
		input_info(true, &stmpe1801->client->dev, "row: %X, comp: %X\n", row, comp);
		if (row & comp) break;
	}

	for (j = 10; j > 0; j--) {
		comp = 1 << (j - 1);
		input_info(true, &stmpe1801->client->dev, "col: %X, comp: %X\n", col, comp);
		if (col & comp) break;
	}
	input_info(true, &stmpe1801->client->dev, "row: %d, col: %d\n", i, j);

	return (MATRIX_SCAN_CODE(i - 1, j - 1, STMPE1801_KPC_ROW_SHIFT)+1);
}

static void stmpe1801_release_all_key(struct stmpe1801_dev *stmpe1801)
{
	int i, j;

	if (!stmpe1801->input_dev)
		return;

	for (i = 0; i < stmpe1801->dtdata->num_row; i++) {
		if (!stmpe1801->key_state[i])
			continue;

		for (j = 0; j < stmpe1801->dtdata->num_column; j++) {
			if (stmpe1801->key_state[i] & (1 << j)) {
				int code = MATRIX_SCAN_CODE(i, j, STMPE1801_KPC_ROW_SHIFT);
				input_event(stmpe1801->input_dev, EV_MSC, MSC_SCAN, code);
				input_report_key(stmpe1801->input_dev,
						stmpe1801->keycode[code], 0);
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
				input_info(true, &stmpe1801->client->dev,
						"RA code(0x%X|%d) R:C(%d:%d)\n",
						stmpe1801->keycode[code],
						stmpe1801->keycode[code], i, j);
#else
				input_info(true, &stmpe1801->client->dev,
						"RA (%d:%d)\n", i, j);
#endif
				stmpe1801->key_state[i] &= ~(1 << j);
			}
		}
	}
	input_sync(stmpe1801->input_dev);
}

static int __i2c_block_read(struct i2c_client *client, u8 regaddr, u8 length, u8 *values)
{
	int ret;
	int retry = 3;
	struct stmpe1801_dev *device_data = i2c_get_clientdata(client);
	struct i2c_msg	msgs[] = {
		{
			.addr = client->addr,
			.flags = client->flags,
			.len = 1,
			.buf = &regaddr,
		},
		{
			.addr = client->addr,
			.flags = client->flags | I2C_M_RD,
			.len = length,
			.buf = values,
		},
	};

	while (retry--) {
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret != 2) {
			input_err(true, &client->dev, "failed to read reg %#x: %d, try %d\n",
					regaddr, ret, 3 - retry);
			stmpe1801_release_all_key(device_data);
			ret = -EIO;
			usleep_range(10000, 10000);
			continue;
		}
		break;
	}

	return ret;
}

static int __i2c_block_write(struct i2c_client *client,
		u8 regaddr, u8 length, const u8 *values)
{
	int ret;
	int retry = 3;
	unsigned char msgbuf0[I2C_SMBUS_BLOCK_MAX+1];
	struct i2c_msg	msgs[] = {
		{
			.addr = client->addr,
			.flags = client->flags,
			.len = 1 + length,
			.buf = msgbuf0,
		},
	};

	if (length > I2C_SMBUS_BLOCK_MAX)
		return -E2BIG;

	memcpy(msgbuf0 + 1, values, length * sizeof(u8));
	msgbuf0[0] = regaddr;

	while (retry--) {
		ret = i2c_transfer(client->adapter, msgs, 1);
		if (ret != 1) {
			input_err(true, &client->dev,
					"failed to write reg %#x: %d\n", regaddr, ret);
			ret = -EIO;
			usleep_range(10000, 10000);
			continue;
		}
		break;
	}

	return ret;
}


static int __i2c_reg_read(struct i2c_client *client, u8 regaddr)
{
	int ret;
	u8 buf[4];

	ret = __i2c_block_read(client, regaddr, 1, buf);
	if (ret < 0)
		return ret;

	return buf[0];
}

static int __i2c_reg_write(struct i2c_client *client, u8 regaddr, u8 val)
{
	int ret;

	ret = __i2c_block_write(client, regaddr, 1, &val);
	if (ret < 0)
		return ret;

	return ret;
}

static int __i2c_set_bits(struct i2c_client *client, u8 regaddr, u8 offset, u8 val)
{
	int ret;
	u8 mask;

	ret = __i2c_reg_read(client, regaddr);
	if (ret < 0)
		return ret;

	mask = 1 << offset;

	if (val > 0) {
		ret |= mask;
	}
	else {
		ret &= ~mask;
	}

	return __i2c_reg_write(client, regaddr, ret);
}

static int stmpe1801_dev_pinctrl_configure(struct stmpe1801_dev *data, bool active)
{
	struct pinctrl_state *set_state;
	int retval;

	if (!data->pinctrl)
		return -ENODEV;

	input_dbg(true, &data->client->dev,
			"%s: %s\n", __func__, active ? "ACTIVE" : "SUSPEND");

	set_state = pinctrl_lookup_state(data->pinctrl,
			active ? "active_state" : "suspend_state");
	if (IS_ERR(set_state)) {
		input_err(true, &data->client->dev, "%s: cannot get active state\n", __func__);
		return -EINVAL;
	}

	retval = pinctrl_select_state(data->pinctrl, set_state);
	if (retval) {
		input_err(true, &data->client->dev, "%s: cannot set pinctrl %s state\n",
				__func__, active ? "active" : "suspend");
		return -EINVAL;
	}

	return 0;
}

static void stmpe1801_regs_init(struct stmpe1801_dev *stmpe1801)
{

	input_dbg(true, &stmpe1801->client->dev, "dev lock @ %s\n", __func__);
	mutex_lock(&stmpe1801->dev_lock);

	// 0: SYS_CTRL
	stmpe1801->regs_sys_int_init[0] = stmpe1801->dtdata->debounce;
	// 1: INT_CTRL
	stmpe1801->regs_sys_int_init[1] =
		STMPE1801_INT_CTRL_ACTIVE_LOW
#ifdef STMPE1801_TRIGGER_TYPE_FALLING
		|STMPE1801_INT_CTRL_TRIG_EDGE
#endif
		|STMPE1801_INT_CTRL_GLOBAL_ON;
	// 2: INT_EN_MASK
	stmpe1801->regs_sys_int_init[2] = 0;
	if (stmpe1801->dtdata->block_type & STMPE1801_BLOCK_GPIO)
		stmpe1801->regs_sys_int_init[2] |= STMPE1801_INT_MASK_GPIO;
	if (stmpe1801->dtdata->block_type & STMPE1801_BLOCK_KEY)
		stmpe1801->regs_sys_int_init[2] |= STMPE1801_INT_MASK_KEY |
				STMPE1801_INT_MASK_KEY_FIFO;

	if ((stmpe1801->dtdata->block_type & STMPE1801_BLOCK_KEY) &&
			(stmpe1801->keymap_data != NULL)) {
		// 0 : KPC_ROW
		unsigned int i;
		i = (((1 << stmpe1801->dtdata->num_column) - 1) << 8) |
				((1 << stmpe1801->dtdata->num_row) - 1);
		stmpe1801->regs_kpc_init[0] = i & 0xff;	// Row
		// 1 : KPC_COL_LOW
		stmpe1801->regs_kpc_init[1] = (i >> 8) & 0xff; // Col_low
		// 2 : KPC_COL_HIGH
		stmpe1801->regs_kpc_init[2] = (i >> 16) & 0x03; // Col_high
		// Pull-up in unused pin
		i = ~i;
		stmpe1801->regs_pullup_init[0] = i & 0xff;
		stmpe1801->regs_pullup_init[1] = (i >> 8) & 0xff;
		stmpe1801->regs_pullup_init[2] = (i >> 16) & 0x03;
		// 3 : KPC_CTRL_LOW
		i = stmpe1801->dtdata->scan_cnt & 0x0f;
		stmpe1801->regs_kpc_init[3] = (i << STMPE1801_SCAN_CNT_SHIFT) &
				STMPE1801_SCAN_CNT_MASK;
		// 4 : KPC_CTRL_MID
		if (stmpe1801->dtdata->kpc_debounce)
			stmpe1801->regs_kpc_init[4] =
					(stmpe1801->dtdata->kpc_debounce - 1) << 1;
		else
			stmpe1801->regs_kpc_init[4] = 0x62; // default : 0x62 (50ms)
		// 5 : KPC_CTRL_HIGH
		stmpe1801->regs_kpc_init[5] = (0x40 & ~STMPE1801_SCAN_FREQ_MASK) |
			(stmpe1801->dtdata->freq & STMPE1801_SCAN_FREQ_MASK);
	}

	input_dbg(true, &stmpe1801->client->dev, "dev unlock @ %s\n", __func__);
	mutex_unlock(&stmpe1801->dev_lock);

	return;
}

static int stmpe1801_hw_init(struct stmpe1801_dev *stmpe1801)
{
	int ret;

	//Check ID
	ret = __i2c_reg_read(stmpe1801->client, STMPE1801_REG_SYS_CHIPID);
	input_info(true, &stmpe1801->client->dev, "Chip ID = %x %x\n" ,
			ret, __i2c_reg_read(stmpe1801->client, STMPE1801_REG_SYS_CHIPID+1));
	if (ret != 0xc1)  goto err_hw_init;

	//soft reset & set debounce time
	ret = __i2c_reg_write(stmpe1801->client, STMPE1801_REG_SYS_CTRL,
			0x80 | stmpe1801->regs_sys_int_init[0]);
	if (ret < 0) goto err_hw_init;
	mdelay(20);	// waiting reset

	// INT CTRL - It is erased if it is not need.
	ret = __i2c_reg_write(stmpe1801->client, STMPE1801_REG_INT_CTRL_LOW,
			stmpe1801->regs_sys_int_init[1]);
	if (ret < 0) goto err_hw_init;


	// INT EN Mask INT_EN_MASK_LOW Register
	ret = __i2c_reg_write(stmpe1801->client, STMPE1801_REG_INT_MASK_LOW,
			stmpe1801->regs_sys_int_init[2]);
	if (ret < 0) goto err_hw_init;

	if (stmpe1801->dtdata->block_type & STMPE1801_BLOCK_KEY) {
		ret = __i2c_block_write(stmpe1801->client, STMPE1801_REG_KPC_ROW, 6,
				stmpe1801->regs_kpc_init);
		if (ret < 0) goto err_hw_init;
		ret = __i2c_block_write(stmpe1801->client, STMPE1801_REG_GPIO_PULL_UP_yyy, 3,
				stmpe1801->regs_pullup_init);
		if (ret < 0) goto err_hw_init;
		input_info(true, &stmpe1801->client->dev, "Keypad setting done\n");
	}

err_hw_init:
	return ret;
}

static int stmpe1801_dev_power_off(struct stmpe1801_dev *stmpe1801)
{
	input_info(true, &stmpe1801->client->dev, "%s\n", __func__);

	if (atomic_read(&stmpe1801->dev_init) > 0)
		atomic_set(&stmpe1801->dev_init, 0);

	return 0;
}

static int stmpe1801_dev_power_on(struct stmpe1801_dev *stmpe1801)
{
	int ret = -EINVAL;

	input_info(true, &stmpe1801->client->dev, "%s\n", __func__);

	if (!atomic_read(&stmpe1801->dev_init)) {
		ret = stmpe1801_hw_init(stmpe1801);
		if (ret < 0) {
			stmpe1801_dev_power_off(stmpe1801);
			return ret;
		}
		atomic_set(&stmpe1801->dev_init, 1);
	}

	return 0;
}

static int stmpe1801_dev_regulator(struct stmpe1801_dev *device_data, int onoff)
{
	struct device *dev = &device_data->client->dev;
	int ret = 0;

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

static int stmpe1801_dev_kpc_disable(struct stmpe1801_dev *stmpe1801)
{
	int ret = -EINVAL;

	if (atomic_cmpxchg(&stmpe1801->dev_enabled, 1, 0)) {
		if (atomic_read(&stmpe1801->dev_init) > 0) {
			if (gpio_is_valid(stmpe1801->dtdata->gpio_int)) {
				input_info(true, &stmpe1801->client->dev,
						"%s: disable irq\n", __func__);
				disable_irq_nosync(stmpe1801->dev_irq);
			} else {
				cancel_delayed_work(&stmpe1801->worker);
			}
		}

		ret = stmpe1801_dev_power_off(stmpe1801);
		if (ret < 0) goto err_dis;

		// stop scanning
		//ret = __i2c_set_bits(stmpe1801->client, STMPE1801_REG_KPC_CMD, 0, 0);
		//if (ret < 0) goto err_dis;
	}

	return 0;

err_dis:
	return ret;
}


static int stmpe1801_dev_kpc_enable(struct stmpe1801_dev *stmpe1801)
{
	int ret = -EINVAL;

	if (!atomic_cmpxchg(&stmpe1801->dev_enabled, 0, 1)) {
		ret = stmpe1801_dev_power_on(stmpe1801);
		if (ret < 0) goto err_en;

		// start scanning
		ret = __i2c_set_bits(stmpe1801->client, STMPE1801_REG_KPC_CMD, 0, 1);
		if (ret < 0) goto err_en;

		if (atomic_read(&stmpe1801->dev_init) > 0) {
			if (gpio_is_valid(stmpe1801->dtdata->gpio_int)) {
				input_info(true, &stmpe1801->client->dev,
						"%s: enable irq\n", __func__);
				enable_irq(stmpe1801->dev_irq);
			} else {
				schedule_delayed_work(&stmpe1801->worker,
						STMPE1801_TMR_INTERVAL);
			}
		}
	}
	return 0;

err_en:
	atomic_set(&stmpe1801->dev_enabled, 0);
	return ret;
}

/*
static int stmpe1801_dev_kpc_input_open(struct input_dev *dev)
{
	struct stmpe1801_dev *stmpe1801 = input_get_drvdata(dev);

	return stmpe1801_dev_kpc_enable(stmpe1801);
}

static void stmpe1801_dev_kpc_input_close(struct input_dev *dev)
{
	struct stmpe1801_dev *stmpe1801 = input_get_drvdata(dev);

	stmpe1801_dev_kpc_disable(stmpe1801);

	return;
}
*/

static void stmpe1801_dev_int_proc(struct stmpe1801_dev *stmpe1801, bool force_read_buffer)
{
	int ret = 0, i, j;
	int data, col, row, code;
	bool up;
	union stmpe1801_kpc_data key_values;

	mutex_lock(&stmpe1801->dev_lock);

	if (!stmpe1801->input_dev) {
		input_err(true, &stmpe1801->client->dev, "%s: input dev is null\n", __func__);
		goto out_proc;
	}

	wake_lock_timeout(&stmpe1801->stmpe_wake_lock, msecs_to_jiffies(3 * MSEC_PER_SEC));
#ifdef CONFIG_PM_SLEEP
	if (!stmpe1801->dev_resume_state) {
		/* waiting for blsp block resuming, if not occurs i2c error */
		ret = wait_for_completion_interruptible_timeout(&stmpe1801->resume_done,
				msecs_to_jiffies(5 * MSEC_PER_SEC));
		if (ret == 0) {
			input_err(true, &stmpe1801->client->dev,
					"%s: LPM: pm resume is not handled [timeout]\n",
					__func__);
			goto out_proc;
		}
	}
#endif
	// disable chip global int
	//__i2c_set_bits(stmpe1801->client, STMPE1801_REG_INT_CTRL_LOW, 0, 0);

	// get int src
	if (!force_read_buffer) {
		ret = __i2c_reg_read(stmpe1801->client, STMPE1801_REG_INT_STA_LOW);
		if (ret < 0)
			goto out_proc;
		input_dbg(false, &stmpe1801->client->dev,
				"%s Read STMPE1801_REG_INT_STA_LOW[%d]\n", __func__, ret);
	}

	if ((stmpe1801->dtdata->block_type & STMPE1801_BLOCK_KEY) &&
			(force_read_buffer ||
			(ret & (STMPE1801_INT_MASK_KEY | STMPE1801_INT_MASK_KEY_FIFO)))) {
		/* keypad INT */
		for (i = 0; i < STMPE1801_KEY_FIFO_LENGTH; i++) {
			ret = __i2c_block_read(stmpe1801->client,
					STMPE1801_REG_KPC_DATA_BYTE0, 5, key_values.array);
			if (ret <= 0)
				goto out_proc;

			if (key_values.value == 0xffff8f8f8LL)
				continue;

			for (j = 0; j < 3; j++) {
				data = key_values.array[j];
				col = (data & STMPE1801_KPC_DATA_COL) >> 3;
				row = data & STMPE1801_KPC_DATA_ROW;
				code = MATRIX_SCAN_CODE(row, col, STMPE1801_KPC_ROW_SHIFT);
				up = data & STMPE1801_KPC_DATA_UP ? 1 : 0;

				if (row >= stmpe1801->dtdata->num_row ||
						col >= stmpe1801->dtdata->num_column)
					continue;

				if (col == STMPE1801_KPC_DATA_NOKEY)
					continue;

				cancel_delayed_work(&stmpe1801->ghost_check_work);

				if (up) {	/* Release */
					stmpe1801->key_state[row] &= ~(1 << col);
				} else {	/* Press */
					stmpe1801->key_state[row] |= (1 << col);
					if (!force_read_buffer)
						schedule_delayed_work(&stmpe1801->ghost_check_work,
								msecs_to_jiffies(400));
				}
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
				input_info(true, &stmpe1801->client->dev,
						"%s '%c' code(0x%X|%d) R:C(%d:%d)\n",
						!up ? "P" : "R",
						stmpe1801->key_name[stmpe1801->keycode[code]],
						stmpe1801->keycode[code],
						stmpe1801->keycode[code], row, col);
#else
				input_info(true, &stmpe1801->client->dev,
						"%s (%d:%d)\n", !up ? "P" : "R", row, col);
#endif
				input_event(stmpe1801->input_dev, EV_MSC, MSC_SCAN, code);
				input_report_key(stmpe1801->input_dev,
						stmpe1801->keycode[code], !up);
				input_sync(stmpe1801->input_dev);
			}
		}
	}
	input_dbg(false, &stmpe1801->client->dev, "%s -- dev_int_proc --\n",__func__);

out_proc:
	// enable chip global int
	//__i2c_set_bits(stmpe1801->client, STMPE1801_REG_INT_CTRL_LOW, 0, 1);

	mutex_unlock(&stmpe1801->dev_lock);

}

static irqreturn_t stmpe1801_dev_isr(int irq, void *dev_id)
{
	struct stmpe1801_dev *stmpe1801 = (struct stmpe1801_dev *)dev_id;

	if (!stmpe1801->current_connect_state) {
		input_dbg(false, &stmpe1801->client->dev, "%s: not connected\n", __func__);
		return IRQ_HANDLED;
	}

	stmpe1801_dev_int_proc(stmpe1801, false);

	return IRQ_HANDLED;
}

static void stmpe1801_ghost_check_worker(struct work_struct *work)
{
	struct stmpe1801_dev *stmpe1801 = container_of((struct delayed_work *)work,
			struct stmpe1801_dev, ghost_check_work);
	int i, state = 0;

	if (!stmpe1801->current_connect_state) {
		input_dbg(false, &stmpe1801->client->dev, "%s: not connected\n", __func__);
		return;
	}

	for (i = 0; i < stmpe1801->dtdata->num_row; i++) {
		state |= stmpe1801->key_state[i];
	}

	input_info(true, &stmpe1801->client->dev, "%s: %d\n", __func__, state);
	if (!!state)
		stmpe1801_dev_int_proc(stmpe1801, true);
}

static void stmpe1801_dev_worker(struct work_struct *work)
{
	struct stmpe1801_dev *stmpe1801 = container_of((struct delayed_work *)work,
			struct stmpe1801_dev, worker);

	stmpe1801_dev_int_proc(stmpe1801, false);

	schedule_delayed_work(&stmpe1801->worker, STMPE1801_TMR_INTERVAL);
}

#ifdef CONFIG_OF
static int stmpe1801_parse_dt(struct device *dev,
		struct stmpe1801_dev *device_data)
{
	struct device_node *np = dev->of_node;
	struct matrix_keymap_data *keymap_data;
	int ret, keymap_len, i;
	u32 *keymap, temp;
	const __be32 *map;

	device_data->dtdata->gpio_int = of_get_named_gpio(np, "stmpe,irq_gpio", 0);
	if (!gpio_is_valid(device_data->dtdata->gpio_int)) {
		input_err(true, dev, "unable to get gpio_int\n");
		return device_data->dtdata->gpio_int;
	}

	device_data->dtdata->gpio_sda = of_get_named_gpio(np, "stmpe,sda_gpio", 0);
	device_data->dtdata->gpio_scl = of_get_named_gpio(np, "stmpe,scl_gpio", 0);

	ret = of_property_read_u32(np, "stmpe,block_type", &temp);
	if (ret) {
		input_err(true, dev, "unable to get block_type\n");
		return ret;
	}
	device_data->dtdata->block_type = temp;

	ret = of_property_read_u32(np, "stmpe,debounce", &temp);
	if (ret) {
		input_err(true, dev, "unable to get debounce\n");
		return ret;
	}
	device_data->dtdata->debounce = temp;

	ret = of_property_read_u32(np, "stmpe,scan_cnt", &temp);
	if (ret) {
		input_err(true, dev, "unable to get scan_cnt\n");
		return ret;
	}
	device_data->dtdata->scan_cnt = temp;

	ret = of_property_read_u32(np, "stmpe,repeat", &temp);
	if (ret) {
		input_err(true, dev, "unable to get repeat\n");
		return ret;
	}
	device_data->dtdata->repeat = temp;

	ret = of_property_read_u32(np, "stmpe,freq", &temp);
	if (ret)
		input_err(true, dev, "unable to get frequency\n");
	else
		device_data->dtdata->freq = temp;

	ret = of_property_read_u32(np, "stmpe,kpc_debounce", &temp);
	if (ret)
		input_err(true, dev, "unable to get kpc_debounce\n");
	else
		device_data->dtdata->kpc_debounce = temp;

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

	keymap_data->keymap_size = keymap_len / sizeof(u32);

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
	input_info(true, dev, "%s: scl: %d, sda: %d, int:%d, block type:%d, "
			"debounce:%d, kpc_debounce:%d, scan cnt:%d, repeat:%d, row:%d, "
			"col:%d, keymap size:%d\n",
			__func__, device_data->dtdata->gpio_scl, device_data->dtdata->gpio_sda,
			device_data->dtdata->gpio_int,
			device_data->dtdata->block_type, device_data->dtdata->debounce,
			device_data->dtdata->kpc_debounce,
			device_data->dtdata->scan_cnt, device_data->dtdata->repeat,
			device_data->dtdata->num_row, device_data->dtdata->num_column,
			device_data->keymap_data->keymap_size);


	return 0;
}
#else
static int stmpe1801_parse_dt(struct device *dev,
		struct stmpe1801_dev *device_data)
{
	return -ENODEV;
}
#endif

void stmpe1801_gpio_enable(bool en)
{
	struct stmpe1801_dev *data = copy_device_data;
	int ret;

	if (!data)
		return;

	input_info(true, &data->client->dev, "%s + en:%d\n",__func__, en);

	// NOVEL gpio 5 set for TDMB_FM_SEL
	ret = __i2c_reg_write(data->client, STMPE1801_REG_GPIO_SET_DIR_yyy, 0x20);
	if (ret < 0)
		input_err(true, &data->client->dev, "%s write fail %d\n",__func__, ret);

	if (en) {
		ret = __i2c_reg_write(data->client, STMPE1801_REG_GPIO_SET_yyy, 0x20);
		if (ret < 0)
			input_err(true, &data->client->dev, "%s write fail %d\n",__func__, ret);
	} else {
		ret = __i2c_reg_write(data->client, STMPE1801_REG_GPIO_CLR_yyy, 0x20);
		if (ret < 0)
			input_err(true, &data->client->dev, "%s write fail %d\n",__func__, ret);
	}

	ret =__i2c_reg_read(data->client, STMPE1801_REG_GPIO_MP_yyy);
	input_info(true, &data->client->dev, "%s - MP:%x\n",__func__, ret);
}
EXPORT_SYMBOL(stmpe1801_gpio_enable);

static ssize_t  sysfs_key_onoff_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct stmpe1801_dev *device_data = dev_get_drvdata(dev);
	int state = 0;
	int i;

	for (i = 0; i < device_data->dtdata->num_row; i++) {
		state |= device_data->key_state[i];
	}

	input_info(true, &device_data->client->dev,
			"%s: key state:%d\n", __func__, !!state);

	return snprintf(buf, 5, "%d\n", !!state);
}

static DEVICE_ATTR(sec_key_pressed, 0444 , sysfs_key_onoff_show, NULL);

static int stmpe1801_keypad_start(struct stmpe1801_dev *data) {
	int ret = 0;

	if (!data)
		return -ENODEV;

	ret = stmpe1801_dev_regulator(data, 1);
	if (ret < 0) {
		input_err(true, &data->client->dev, "%s: regulator on error\n", __func__);
		goto out;
	}

	ret = stmpe1801_dev_kpc_enable(data);
	if (ret < 0) {
		input_err(true, &data->client->dev, "%s: enable error\n", __func__);
		goto out;
	}

	stmpe1801_gpio_enable(0);

	input_info(true, &data->client->dev, "%s done\n", __func__);
	return 0;
out:
	input_err(true, &data->client->dev, "%s: failed. int:%d, sda:%d, scl:%d\n", __func__,
			gpio_get_value(data->dtdata->gpio_int),
			gpio_get_value(data->dtdata->gpio_sda),
			gpio_get_value(data->dtdata->gpio_scl));

	if (stmpe1801_dev_regulator(data, 0) < 0)
		input_err(true, &data->client->dev, "%s: regulator off error %d\n", __func__);

	return ret;
}

static int stmpe1801_keypad_stop(struct stmpe1801_dev *data) {
	int ret = 0;

	if (!data)
		return -ENODEV;

	stmpe1801_dev_kpc_disable(data);

	stmpe1801_release_all_key(data);

	ret = stmpe1801_dev_regulator(data, 0);
	if (ret < 0) {
		input_err(true, &data->client->dev, "%s: regulator off error\n", __func__);
		goto out;
	}

	input_info(true, &data->client->dev, "%s done\n", __func__);
out:
	return ret;
}

static int stmpe1801_set_input_dev(struct stmpe1801_dev *device_data)
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
	device_data->input_dev->name = "Tab S3 Book Cover Keyboard";
	device_data->input_dev->id.bustype = BUS_I2C;
	device_data->input_dev->id.vendor = INPUT_VENDOR_ID_SAMSUNG;
	device_data->input_dev->id.product = INPUT_PRODUCT_ID_POGO_KEYBOARD;
	device_data->input_dev->flush = NULL;
	device_data->input_dev->event = NULL;
	//device_data->input_dev->open = stmpe1801_dev_kpc_input_open;
	//device_data->input_dev->close = stmpe1801_dev_kpc_input_close;

	input_set_drvdata(device_data->input_dev, device_data);
	input_set_capability(device_data->input_dev, EV_MSC, MSC_SCAN);
	__set_bit(EV_KEY, device_data->input_dev->evbit);
	if (device_data->dtdata->repeat)
		__set_bit(EV_REP, device_data->input_dev->evbit);
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

static void stmpe1801_keyboard_connect_work(struct work_struct *work)
{
	struct stmpe1801_dev *data = container_of((struct delayed_work *)work,
			struct stmpe1801_dev, keyboard_work);
	int ret = 0;

	if (!data) {
		pr_err("%s: dev is null\n", __func__);
		return;
	}

	input_info(true, &data->client->dev, "%s: %d\n", __func__, data->connect_state);

	mutex_lock(&data->dev_lock);
#ifndef CONFIG_SEC_FACTORY
	wake_lock_timeout(&data->stmpe_wake_lock, msecs_to_jiffies(3 * MSEC_PER_SEC));
#endif
	cancel_delayed_work(&data->ghost_check_work);

	if (data->connect_state) {
		ret = stmpe1801_set_input_dev(data);
		if (ret)
			goto out;
		ret = stmpe1801_keypad_start(data);
	} else {
		ret = stmpe1801_keypad_stop(data);
	}

	if (data->input_dev) {
		if (ret >= 0) {
#ifndef CONFIG_SEC_FACTORY
			if (!data->connect_state) {
				input_report_key(data->input_dev, KEY_SLEEP, 1);
				input_sync(data->input_dev);
				input_report_key(data->input_dev, KEY_SLEEP, 0);
				input_sync(data->input_dev);
				input_info(true, &data->client->dev,
						"%s: 0x%2X\n", __func__, KEY_SLEEP);
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

static int stmpe1801_keyboard_notifier(struct notifier_block *nb,
		unsigned long action, void *data)
{
	struct stmpe1801_dev *devdata = container_of(nb, struct stmpe1801_dev, keyboard_nb);
	int state = !!action;

	if (!devdata) {
		pr_err("%s: dev is null\n", __func__);
		goto out;
	}

	if (devdata->current_connect_state == state)
		goto out;

	input_info(true, &devdata->client->dev, "%s: current %d change to %d\n",
			__func__, devdata->current_connect_state, state);
	devdata->connect_state = state;
	cancel_delayed_work(&devdata->keyboard_work);
	schedule_delayed_work(&devdata->keyboard_work, msecs_to_jiffies(1));

out:
	return NOTIFY_DONE;
}

static ssize_t keyboard_connected(struct device *dev,
		struct device_attribute *attr, const char *buf,
		size_t size)
{
	struct stmpe1801_dev *data = dev_get_drvdata(dev);
	int onoff, err;

	if (size > 2) {
		input_err(true, &data->client->dev, "%s: cmd lenth is over %d\n",
				__func__, (int)strlen(buf));
		return -EINVAL;
	}

	err = kstrtoint(buf, 10, &onoff);
	if (err)
		return err;

	if (!data)
		return size;

	if (data->current_connect_state == !!onoff)
		return size;

	data->connect_state = !!onoff;

	input_info(true, &data->client->dev, "%s: current %d change to %d\n",
			__func__, data->current_connect_state, data->connect_state);

	cancel_delayed_work(&data->keyboard_work);
	schedule_delayed_work(&data->keyboard_work, msecs_to_jiffies(1));
	return size;
}

static ssize_t keyboard_connected_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct stmpe1801_dev *data = dev_get_drvdata(dev);

	input_info(true, dev, "%s: %d\n", __func__, data->current_connect_state);

	return snprintf(buf, 5, "%d\n", data->current_connect_state);
}
static DEVICE_ATTR(keyboard_connected, S_IRUGO | S_IWUSR | S_IWGRP,
		keyboard_connected_show, keyboard_connected);

static struct attribute *key_attributes[] = {
	&dev_attr_sec_key_pressed.attr,
	&dev_attr_keyboard_connected.attr,
	NULL,
};

static struct attribute_group key_attr_group = {
	.attrs = key_attributes,
};

static int stmpe1801_dev_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct stmpe1801_dev *device_data;
	int ret = 0;

	input_err(true, &client->dev, "%s++\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		input_err(true, &client->dev,
				"i2c_check_functionality fail\n");
		return -EIO;
	}

	device_data = kzalloc(sizeof(struct stmpe1801_dev), GFP_KERNEL);
	if (!device_data) {
		input_err(true, &client->dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}

	copy_device_data = device_data;
	device_data->client = client;

	atomic_set(&device_data->dev_enabled, 0);
	atomic_set(&device_data->dev_init, 0);

	mutex_init(&device_data->dev_lock);
	wake_lock_init(&device_data->stmpe_wake_lock, WAKE_LOCK_SUSPEND, "stmpe_key wake lock");
#ifdef CONFIG_PM_SLEEP
	init_completion(&device_data->resume_done);
	device_data->dev_resume_state = true;
#endif
	device_data->client = client;

	if (client->dev.of_node) {
		device_data->dtdata = devm_kzalloc(&client->dev,
				sizeof(struct stmpe1801_devicetree_data), GFP_KERNEL);
		if (!device_data->dtdata) {
			input_err(true, &client->dev, "Failed to allocate memory\n");
			ret = -ENOMEM;
			goto err_config;
		}
		ret = stmpe1801_parse_dt(&client->dev, device_data);
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

	stmpe1801_dev_pinctrl_configure(device_data, true);

	device_data->dtdata->vdd_vreg = devm_regulator_get(&client->dev, "vddo");
	if (IS_ERR(device_data->dtdata->vdd_vreg)) {
		ret = PTR_ERR(device_data->dtdata->vdd_vreg);
		input_err(true, &client->dev, "%s: could not get vddo, rc = %ld\n",
				__func__, ret);
		device_data->dtdata->vdd_vreg = NULL;
		goto err_power;
	}

	stmpe1801_regs_init(device_data);

	device_data->keycode_entries = __stmpe1801_get_keycode_entries(device_data,
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

	i2c_set_clientdata(client, device_data);

	INIT_DELAYED_WORK(&device_data->ghost_check_work, stmpe1801_ghost_check_worker);
	// request irq
	if (gpio_is_valid(device_data->dtdata->gpio_int)) {
		device_data->dev_irq = gpio_to_irq(device_data->dtdata->gpio_int);
		input_info(true, &client->dev,
				"%s INT mode (%d)\n", __func__, device_data->dev_irq);

		ret = request_threaded_irq(device_data->dev_irq, NULL, stmpe1801_dev_isr,
#ifdef STMPE1801_TRIGGER_TYPE_FALLING
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
#else
				IRQF_TRIGGER_LOW | IRQF_ONESHOT,
#endif
				client->name, device_data);

		if (ret < 0) {
			input_err(true, &client->dev, "stmpe1801_dev_power_off error\n");
			goto interrupt_err;
		}
		disable_irq_nosync(device_data->dev_irq);
	} else {
		input_info(true, &client->dev, "%s poll mode\n", __func__);

		INIT_DELAYED_WORK(&device_data->worker, stmpe1801_dev_worker);
		schedule_delayed_work(&device_data->worker, STMPE1801_TMR_INTERVAL);
	}

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

	INIT_DELAYED_WORK(&device_data->keyboard_work, stmpe1801_keyboard_connect_work);
	keyboard_notifier_register(&device_data->keyboard_nb,
			stmpe1801_keyboard_notifier, KEYBOARD_NOTIFY_DEV_TSP);

	input_info(true, &client->dev, "%s done\n", __func__);

	return ret;

err_create_group:
#ifdef CONFIG_SEC_SYSFS
	sec_device_destroy(13);
#else
	device_destroy(sec_class, 13);
#endif
err_create_device:
interrupt_err:
	kfree(device_data->keycode);
err_keycode:
	kfree(device_data->key_state);
err_keystate:
err_power:
err_config:
	mutex_destroy(&device_data->dev_lock);
	wake_lock_destroy(&device_data->stmpe_wake_lock);

	copy_device_data = NULL;
	kfree(device_data);

	input_err(true, &client->dev, "Error at stmpe1801_dev_probe\n");

	return ret;
}

static int stmpe1801_dev_remove(struct i2c_client *client)
{
	struct stmpe1801_dev *device_data = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&device_data->ghost_check_work);
	device_init_wakeup(&client->dev, 0);
	wake_lock_destroy(&device_data->stmpe_wake_lock);

	if (device_data->input_dev) {
		input_unregister_device(device_data->input_dev);
		device_data->input_dev = NULL;
	}

	stmpe1801_dev_regulator(device_data, 0);

	if (gpio_is_valid(device_data->dtdata->gpio_int))
		free_irq(device_data->dev_irq, device_data);

#ifdef CONFIG_SEC_SYSFS
	sec_device_destroy(13);
#else
	device_destroy(sec_class, 13);
#endif

	kfree(device_data->keycode);
	kfree(device_data);
	copy_device_data = NULL;

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int stmpe1801_dev_suspend(struct device *dev)
{
	struct stmpe1801_dev *device_data = dev_get_drvdata(dev);

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

static int stmpe1801_dev_resume(struct device *dev)
{
	struct stmpe1801_dev *device_data = dev_get_drvdata(dev);

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

static SIMPLE_DEV_PM_OPS(stmpe1801_dev_pm_ops,
		stmpe1801_dev_suspend, stmpe1801_dev_resume);
#endif

static const struct i2c_device_id stmpe1801_dev_id[] = {
	{ STMPE1801_DRV_NAME, 0 },
	{ }
};

#ifdef CONFIG_OF
static struct of_device_id stmpe1801_match_table[] = {
	{ .compatible = "stmpe,stmpe1801bjr",},
	{ },
};
#else
#define stmpe1801_match_table NULL
#endif

static struct i2c_driver stmpe1801_dev_driver = {
	.driver = {
		.name = STMPE1801_DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = stmpe1801_match_table,
#ifdef CONFIG_PM_SLEEP
		.pm = &stmpe1801_dev_pm_ops,
#endif
	},
	.probe = stmpe1801_dev_probe,
	.remove = stmpe1801_dev_remove,
	.id_table = stmpe1801_dev_id,
};

static int __init stmpe1801_dev_init(void)
{
	pr_err("%s++\n", __func__);

	return i2c_add_driver(&stmpe1801_dev_driver);
}
static void __exit stmpe1801_dev_exit(void)
{
	i2c_del_driver(&stmpe1801_dev_driver);
}
late_initcall(stmpe1801_dev_init);
module_exit(stmpe1801_dev_exit);

MODULE_DEVICE_TABLE(i2c, stmpe1801_dev_id);

MODULE_DESCRIPTION("STMPE1801 Key Driver");
MODULE_AUTHOR("Gyusung SHIM <gyusung.shim@st.com>");
MODULE_LICENSE("GPL");
