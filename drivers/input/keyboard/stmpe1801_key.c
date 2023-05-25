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

#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif

#include <linux/input/stmpe1801_key.h>

#define DD_VER	"2.4"

#define DBG_PRN	// define for debugging printk
#define STMPE1801_TRIGGER_TYPE_FALLING // define for INT trigger
#define STMPE1801_TMR_INTERVAL	10L


/*
 * Definitions & globals.
 */
#define STMPE1801_MAXGPIO				18
#define STMPE1801_KEY_FIFO_LENGTH		10

#define STMPE1801_REG_SYS_CHIPID		0x00
#define STMPE1801_REG_SYS_CTRL			0x02

#define STMPE1801_REG_INT_CTRL_LOW		0x04
#define STMPE1801_REG_INT_MASK_LOW		0x06
#define STMPE1801_REG_INT_STA_LOW		0x08

#define STMPE1801_REG_INT_GPIO_MASK_LOW	0x0A
#define STMPE1801_REG_INT_GPIO_STA_LOW	0x0D

#define STMPE1801_REG_GPIO_SET_yyy		0x10
#define STMPE1801_REG_GPIO_CLR_yyy		0x13
#define STMPE1801_REG_GPIO_MP_yyy		0x16
#define STMPE1801_REG_GPIO_SET_DIR_yyy	0x19
#define STMPE1801_REG_GPIO_RE_yyy		0x1C
#define STMPE1801_REG_GPIO_FE_yyy		0x1F
#define STMPE1801_REG_GPIO_PULL_UP_yyy	0x22

#define STMPE1801_REG_KPC_ROW			0x30
#define STMPE1801_REG_KPC_COL_LOW		0x31
#define STMPE1801_REG_KPC_CTRL_LOW		0x33
#define STMPE1801_REG_KPC_CTRL_MID		0x34
#define STMPE1801_REG_KPC_CTRL_HIGH		0x35
#define STMPE1801_REG_KPC_CMD			0x36
#define STMPE1801_REG_KPC_DATA_BYTE0	0x3A


#define STMPE1801_KPC_ROW_SHIFT			3 // can be 3

#define STMPE1801_KPC_DATA_UP			(0x80)
#define STMPE1801_KPC_DATA_COL			(0x78)
#define STMPE1801_KPC_DATA_ROW			(0x07)
#define STMPE1801_KPC_DATA_NOKEY		(0x0f)

union stmpe1801_kpc_data {
	uint64_t						value;
	uint8_t 						array[8];
};

struct stmpe1801_dev {
	int								dev_irq;
	struct i2c_client				*client;
	struct input_dev				*input_dev;
	struct delayed_work				worker;
	struct mutex					dev_lock;
	atomic_t						dev_enabled;
	atomic_t						dev_init;
	int								dev_resume_state;
	struct device					*sec_keypad;

	u8								regs_sys_int_init[4];
	u8								regs_kpc_init[8];
	u8								regs_pullup_init[3];

	struct matrix_keymap_data		*keymap_data;
	unsigned short					*keycode;
	int								keycode_entries;

	bool							probe_done;

	struct stmpe1801_devicetree_data	*dtdata;
	int								irq_gpio;
	int								sda_gpio;
	int								scl_gpio;
	struct pinctrl *pinctrl;
};

struct stmpe1801_devicetree_data {
	int gpio_int;
	int gpio_tkey_led_en;
	int block_type;
	int debounce;
	int scan_cnt;
	int repeat;
	int num_row;
	int num_column;
	const char *project;
	struct regulator *vdd_vreg;
};

static unsigned char gExpanderKeyState = 0;

extern struct class *sec_class;

static int __stmpe1801_get_keycode_entries(u32 mask_row, u32 mask_col)
{
	int col, row;
	int i, j, comp;

	row = mask_row & 0xff;
	col = mask_col & 0x3ff;

	for (i = 8; i > 0; i--) {
		comp = 1 << (i - 1);
		printk(" row: %d, comp: %d\n", row, comp);
		if (row & comp) break;
	}

	for (j = 10; j > 0; j--) {
		comp = 1 << (j - 1);
		printk(" col: %d, comp: %d\n", col, comp);
		if (col & comp) break;
	}
#ifdef DBG_PRN
	printk(" row: %d, col: %d\n", i, j);
#endif

	return (MATRIX_SCAN_CODE(i - 1, j - 1, STMPE1801_KPC_ROW_SHIFT)+1);
}


static int __i2c_block_read(struct i2c_client *client, u8 regaddr, u8 length, u8 *values)
{
	int ret;
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

	ret = i2c_transfer(client->adapter, msgs, 2);

	if (ret != 2) {
		dev_err(&client->dev, "failed to read reg %#x: %d\n", regaddr, ret);
		ret = -EIO;
	}

	return ret;
}

static int __i2c_block_write(struct i2c_client *client, u8 regaddr, u8 length, const u8 *values)
{
	int ret;
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

	ret = i2c_transfer(client->adapter, msgs, 1);

	if (ret != 1) {
		dev_err(&client->dev, "failed to write reg %#x: %d\n", regaddr, ret);
		ret = -EIO;
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

static void stmpe1801_regs_init(struct stmpe1801_dev *stmpe1801)
{

#ifdef DBG_PRN
	printk("dev lock @ %s\n", __func__);
#endif
	mutex_lock(&stmpe1801->dev_lock);

	// 0: SYS_CTRL
	stmpe1801->regs_sys_int_init[0] = stmpe1801->dtdata->debounce;
	// 1: INT_CTRL
	stmpe1801->regs_sys_int_init[1] =
		STMPE1801_INT_CTRL_ACTIVE_LOW
#ifdef STMPE1801_TRIGGER_TYPE_FALLING
		|STMPE1801_INT_CTRL_TRIG_EDGE
#else
		|STMPE1801_INT_CTRL_TRIG_LEVEL
#endif
		|STMPE1801_INT_CTRL_GLOBAL_ON;
	// 2: INT_EN_MASK
	stmpe1801->regs_sys_int_init[2] = 0;
	if (stmpe1801->dtdata->block_type & STMPE1801_BLOCK_GPIO)
		stmpe1801->regs_sys_int_init[2] |= STMPE1801_INT_MASK_GPIO;
	if (stmpe1801->dtdata->block_type & STMPE1801_BLOCK_KEY)
		stmpe1801->regs_sys_int_init[2] |= STMPE1801_INT_MASK_KEY|STMPE1801_INT_MASK_KEY_FIFO;

	if ((stmpe1801->dtdata->block_type & STMPE1801_BLOCK_KEY) && (stmpe1801->keymap_data != NULL)) {
		// 0 : KPC_ROW
		unsigned int i;
		i = (((1 << stmpe1801->dtdata->num_column) - 1) << 8)| ((1 << stmpe1801->dtdata->num_row) - 1);
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
		stmpe1801->regs_kpc_init[3] = (i << STMPE1801_SCAN_CNT_SHIFT) & STMPE1801_SCAN_CNT_MASK;
		// 4 : KPC_CTRL_MID
		stmpe1801->regs_kpc_init[4] = 0x62; // default : 0x62
		// 5 : KPC_CTRL_HIGH
		stmpe1801->regs_kpc_init[5] = (0x40 & ~STMPE1801_SCAN_FREQ_MASK) | STMPE1801_SCAN_FREQ_60HZ;
	}

	if (stmpe1801->dtdata->block_type & STMPE1801_BLOCK_GPIO
		/*&& (stmpe1801->pdata != NULL)*/) {
		// NOP
	}

#ifdef DBG_PRN
	printk("dev unlock @ %s\n", __func__);
#endif
	mutex_unlock(&stmpe1801->dev_lock);

	return;
}

static int stmpe1801_hw_init(struct stmpe1801_dev *stmpe1801)
{
	int ret;

	//Check ID
	ret = __i2c_reg_read(stmpe1801->client, STMPE1801_REG_SYS_CHIPID);
#ifdef DBG_PRN
	printk("Chip ID = %x %x\n" ,
		ret, __i2c_reg_read(stmpe1801->client, STMPE1801_REG_SYS_CHIPID+1));
#endif
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
		ret = __i2c_block_write(stmpe1801->client, STMPE1801_REG_KPC_ROW, 6, stmpe1801->regs_kpc_init);
		if (ret < 0) goto err_hw_init;
		ret = __i2c_block_write(stmpe1801->client, STMPE1801_REG_GPIO_PULL_UP_yyy, 3, stmpe1801->regs_pullup_init);
		if (ret < 0) goto err_hw_init;
#ifdef DBG_PRN
		printk("Keypad setting done.\n");
#endif
	}

	if (stmpe1801->dtdata->block_type & STMPE1801_BLOCK_GPIO) {
		// NOP
	}


err_hw_init:

	return ret;
}

static int stmpe1801_dev_power_off(struct stmpe1801_dev *stmpe1801)
{
//	int ret = -EINVAL;

#ifdef DBG_PRN
	printk(" %s\n", __func__);
#endif

	if (atomic_read(&stmpe1801->dev_init) > 0) {
		atomic_set(&stmpe1801->dev_init, 0);
	}

#ifdef DBG_PRN
	printk("return ok @ %s\n", __func__);
#endif

	return 0;
}


static int stmpe1801_dev_power_on(struct stmpe1801_dev *stmpe1801)
{
	int ret = -EINVAL;

#ifdef DBG_PRN
	printk(" %s\n", __func__);
#endif

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

static int stmpe1801_dev_kpc_disable(struct stmpe1801_dev *stmpe1801)
{
	int ret = -EINVAL;

	if (atomic_cmpxchg(&stmpe1801->dev_enabled, 1, 0)) {
#ifdef DBG_PRN
		printk("dev lock @ %s\n", __func__);
#endif
		mutex_lock(&stmpe1801->dev_lock);

		if (atomic_read(&stmpe1801->dev_init) > 0) {
			if (stmpe1801->dtdata->gpio_int >= 0)
				disable_irq_nosync(stmpe1801->dev_irq);
			else
				cancel_delayed_work(&stmpe1801->worker);
		}

		ret = stmpe1801_dev_power_off(stmpe1801);
		if (ret < 0) goto err_dis;

		// stop scanning
		ret = __i2c_set_bits(stmpe1801->client, STMPE1801_REG_KPC_CMD, 0, 0);
		if (ret < 0) goto err_dis;

#ifdef DBG_PRN
		printk("dev unlock @ %s\n", __func__);
#endif
		mutex_unlock(&stmpe1801->dev_lock);
	}

	return 0;

err_dis:
#ifdef DBG_PRN
	printk("err: dev unlock @ %s\n", __func__);
#endif
	mutex_unlock(&stmpe1801->dev_lock);

	return ret;
}


static int stmpe1801_dev_kpc_enable(struct stmpe1801_dev *stmpe1801)
{
	int ret = -EINVAL;

	if (!atomic_cmpxchg(&stmpe1801->dev_enabled, 0, 1)) {
#ifdef DBG_PRN
		printk("dev lock @ %s\n", __func__);
#endif
		mutex_lock(&stmpe1801->dev_lock);

		ret = stmpe1801_dev_power_on(stmpe1801);
		if (ret < 0) goto err_en;

		// start scanning
		ret = __i2c_set_bits(stmpe1801->client, STMPE1801_REG_KPC_CMD, 0, 1);
		if (ret < 0) goto err_en;

		if (atomic_read(&stmpe1801->dev_init) > 0) {
			if (stmpe1801->dtdata->gpio_int >= 0)
				enable_irq(stmpe1801->dev_irq);
			else
				schedule_delayed_work(&stmpe1801->worker, STMPE1801_TMR_INTERVAL);
		}

#ifdef DBG_PRN
		printk("dev unlock @ %s\n", __func__);
#endif
		mutex_unlock(&stmpe1801->dev_lock);
	}
	return 0;

err_en:
	atomic_set(&stmpe1801->dev_enabled, 0);
#ifdef DBG_PRN
	printk("err: dev unlock @ %s\n", __func__);
#endif
	mutex_unlock(&stmpe1801->dev_lock);

	return ret;
}

int stmpe1801_dev_kpc_input_open(struct input_dev *dev)
{
	struct stmpe1801_dev *stmpe1801 = input_get_drvdata(dev);

	return stmpe1801_dev_kpc_enable(stmpe1801);
}

void stmpe1801_dev_kpc_input_close(struct input_dev *dev)
{
	struct stmpe1801_dev *stmpe1801 = input_get_drvdata(dev);

	stmpe1801_dev_kpc_disable(stmpe1801);

	return;
}

static void stmpe1801_dev_int_proc(struct stmpe1801_dev *stmpe1801)
{
	int ret, i, j;
	int data, col, row, code;
	bool up;
	union stmpe1801_kpc_data key_values;

	mutex_lock(&stmpe1801->dev_lock);

	// disable chip global int
	__i2c_set_bits(stmpe1801->client, STMPE1801_REG_INT_CTRL_LOW, 0, 0);

	// get int src
	ret = __i2c_reg_read(stmpe1801->client, STMPE1801_REG_INT_STA_LOW);
	if (ret < 0) goto out_proc;

	if (stmpe1801->dtdata->block_type & STMPE1801_BLOCK_KEY) {	// KEYPAD mode
		if((ret & (STMPE1801_INT_MASK_KEY|STMPE1801_INT_MASK_KEY_FIFO)) > 0) { // INT
	for (i = 0; i < STMPE1801_KEY_FIFO_LENGTH; i++) {
		ret = __i2c_block_read(stmpe1801->client, STMPE1801_REG_KPC_DATA_BYTE0, 5, key_values.array);
		if (ret) {
			if (key_values.value != 0xffff8f8f8LL) {
				for (j = 0; j < 3; j++) {
					data = key_values.array[j];
					col = (data & STMPE1801_KPC_DATA_COL) >> 3;
					row = data & STMPE1801_KPC_DATA_ROW;
					code = MATRIX_SCAN_CODE(row, col, STMPE1801_KPC_ROW_SHIFT);
					up = data & STMPE1801_KPC_DATA_UP ? 1 : 0;

					if (col == STMPE1801_KPC_DATA_NOKEY)
						continue;

					gExpanderKeyState = !up; /* 1 : Release , 0 : Press */

					printk("[STMPE] R:C(%d:%d) flag(%d) code(%x)\n", row, col, up, stmpe1801->keycode[code]);

					input_event(stmpe1801->input_dev, EV_MSC, MSC_SCAN, code);
					input_report_key(stmpe1801->input_dev, stmpe1801->keycode[code], !up);
					input_sync(stmpe1801->input_dev);
				}
			}
		}
	}
#ifdef DBG_PRN
			printk("%s -- dev_int_proc --\n",__func__);
#endif
		}
}

out_proc:
	// enable chip global int
	__i2c_set_bits(stmpe1801->client, STMPE1801_REG_INT_CTRL_LOW, 0, 1);

	mutex_unlock(&stmpe1801->dev_lock);
}

static irqreturn_t stmpe1801_dev_isr(int irq, void *dev_id)
{
	struct stmpe1801_dev *stmpe1801 = (struct stmpe1801_dev *)dev_id;

	stmpe1801_dev_int_proc(stmpe1801);

	return IRQ_HANDLED;
}

static void stmpe1801_dev_worker(struct work_struct *work)
{
	struct stmpe1801_dev *stmpe1801 = container_of((struct delayed_work *)work, struct stmpe1801_dev, worker);

	stmpe1801_dev_int_proc(stmpe1801);

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
	if(device_data->dtdata->gpio_int < 0){
		dev_err(dev, "unable to get gpio_int\n");
		return device_data->dtdata->gpio_int;
	}

	ret = of_property_read_u32(np, "stmpe,block_type", &temp);
	if(ret){
		dev_err(dev, "unable to get block_type\n");
		return ret;
	}
	device_data->dtdata->block_type = temp;

	ret = of_property_read_u32(np, "stmpe,debounce", &temp);
	if(ret){
		dev_err(dev, "unable to get debounce\n");
		return ret;
	}
	device_data->dtdata->debounce = temp;

	ret = of_property_read_u32(np, "stmpe,scan_cnt", &temp);
	if(ret){
		dev_err(dev, "unable to get scan_cnt\n");
		return ret;
	}
	device_data->dtdata->scan_cnt = temp;

	ret = of_property_read_u32(np, "stmpe,repeat", &temp);
	if(ret){
		dev_err(dev, "unable to get repeat\n");
		return ret;
	}
	device_data->dtdata->repeat = temp;

	device_data->dtdata->gpio_tkey_led_en = of_get_named_gpio(np, "stmpe,led_en-gpio", 0);
	if(device_data->dtdata->gpio_tkey_led_en < 0){
		dev_err(dev, "unable to get gpio_tkey_led_en...ignoring\n");
	}

	ret = of_property_read_string(np, "stmpe,keypad-project", &device_data->dtdata->project);
	if(ret){
		dev_err(dev, "unable to get project\n");
		return ret;
	}

	ret = of_property_read_u32(np, "keypad,num-rows", &temp);
	if(ret){
		dev_err(dev, "unable to get num-rows\n");
		return ret;
	}
	device_data->dtdata->num_row = temp;

	ret = of_property_read_u32(np, "keypad,num-columns", &temp);
	if(ret){
		dev_err(dev, "unable to get num-columns\n");
		return ret;
	}
	device_data->dtdata->num_column = temp;

	map = of_get_property(np, "linux,keymap", &keymap_len);

	if (!map) {
		dev_err(dev, "Keymap not specified\n");
		return -EINVAL;
	}

	keymap_data = devm_kzalloc(dev,
					sizeof(*keymap_data), GFP_KERNEL);
	if (!keymap_data) {
		dev_err(dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	keymap_data->keymap_size = keymap_len / sizeof(u32);

	keymap = devm_kzalloc(dev,
		sizeof(uint32_t) * keymap_data->keymap_size, GFP_KERNEL);
	if (!keymap) {
		dev_err(dev, "could not allocate memory for keymap\n");
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
	keymap_data->keymap = keymap;
	device_data->keymap_data = keymap_data;


	dev_info(dev, "%s: gpio_int:%d, gpio_led_en:%d\n",
			__func__, device_data->dtdata->gpio_int, device_data->dtdata->gpio_tkey_led_en);


	return 0;
}
#else
static int abov_parse_dt(struct device *dev,
			struct abov_touchkey_devicetree_data *dtdata)
{
	return -ENODEV;
}
#endif

int stmpe1801_dev_regulator_on(struct device *dev,
			struct stmpe1801_dev *device_data)
{
	int ret = 0;

	device_data->dtdata->vdd_vreg = regulator_get(dev, "stmpe,vdd");

	ret = regulator_enable(device_data->dtdata->vdd_vreg);
	if(ret)
	{
		dev_err(dev, "regulator_enable error, ignoring\n");
		return 0;
	}

	if (IS_ERR(device_data->dtdata->vdd_vreg)){
		device_data->dtdata->vdd_vreg = NULL;
		dev_err(dev, "vdd_vreg get error, ignoring\n");
	} else
		regulator_set_voltage(device_data->dtdata->vdd_vreg, 1800000, 1800000);

	return ret;
}

static ssize_t key_led_onoff(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t size)
{
	int data;
	struct stmpe1801_dev *device_data = dev_get_drvdata(dev);

	sscanf(buf, "%d\n", &data);

	if (data) {
		gpio_direction_output(device_data->dtdata->gpio_tkey_led_en, 1);
		printk(KERN_ERR "[KEY] key_led_on\n");
		}
	else {
		gpio_direction_output(device_data->dtdata->gpio_tkey_led_en, 0);
		printk(KERN_ERR "[KEY] key_led_off\n");
		}

	return size;
}

static DEVICE_ATTR(brightness, S_IRUGO | S_IWUSR | S_IWGRP,
		NULL, key_led_onoff);

static struct attribute *key_attributes[] = {
	&dev_attr_brightness.attr,
	NULL,
};

static struct attribute_group key_attr_group = {
	.attrs = key_attributes,
};

static int stmpe1801_dev_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct input_dev *input_dev;
	struct stmpe1801_dev *device_data;
	int ret = 0;
	pr_err("%s++\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev,
			"i2c_check_functionality fail\n");
		return -EIO;
	}

	device_data = kzalloc(sizeof(struct stmpe1801_dev), GFP_KERNEL);
	if (!device_data) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}

	input_dev = input_allocate_device();
	if (!input_dev) {
		dev_err(&client->dev,
			"Failed to allocate memory for input device\n");
		ret = -ENOMEM;
		goto err_input_alloc;
	}

	device_data->client = client;
	device_data->input_dev = input_dev;

	atomic_set(&device_data->dev_enabled, 0);
	atomic_set(&device_data->dev_init, 0);

	mutex_init(&device_data->dev_lock);


	device_data->client = client;

	if (client->dev.of_node) {

		device_data->dtdata = devm_kzalloc(&client->dev,
			sizeof(struct stmpe1801_devicetree_data), GFP_KERNEL);
		if (!device_data->dtdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			ret = -ENOMEM;
			goto err_config;
		}
		ret = stmpe1801_parse_dt(&client->dev, device_data);
		if (ret) {
			dev_err(&client->dev, "Failed to use device tree\n");
			ret = -EIO;
			goto err_config;
		}

	} else {
		dev_err(&client->dev, "No use device tree\n");
		device_data->dtdata = client->dev.platform_data;
	}

	if (device_data->dtdata == NULL) {
		pr_err("failed to get platform data\n");
		goto err_config;
	}
	/* Get pinctrl if target uses pinctrl */
	device_data->pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR(device_data->pinctrl)) {
		if (PTR_ERR(device_data->pinctrl) == -EPROBE_DEFER)
			goto err_config;

		pr_err("%s: Target does not use pinctrl\n", __func__);
		device_data->pinctrl = NULL;
	}

	stmpe1801_regs_init(device_data);


	device_data->keycode_entries = __stmpe1801_get_keycode_entries(((1 << device_data->dtdata->num_row) - 1), ((1 << device_data->dtdata->num_column)-1));
#ifdef DBG_PRN
	printk("%s keycode entries (%d)\n",__func__, device_data->keycode_entries);
	printk("%s keymap size (%d)\n",__func__, device_data->keymap_data->keymap_size);
#endif
	device_data->keycode = kzalloc(device_data->keycode_entries * sizeof(unsigned short), GFP_KERNEL);
	if (device_data == NULL) {
		dev_err(&client->dev, "kzalloc memory error\n");
		goto err_keycode;
	}
#ifdef DBG_PRN
	printk("%s keycode addr (%p)\n", __func__, device_data->keycode);
#endif

	i2c_set_clientdata(client, device_data);

	ret = stmpe1801_dev_regulator_on(&client->dev, device_data);
	if (ret < 0)
	{
		dev_err(&client->dev, "stmpe1801_dev_regulator_on error... ignoring\n");
		ret = 0;
	}

	ret = stmpe1801_dev_power_on(device_data);
	if (ret < 0)
	{
		dev_err(&client->dev, "stmpe1801_dev_power_on error\n");
		goto err_power;
	}
	ret = stmpe1801_dev_power_off(device_data);
	if (ret < 0)
	{
		dev_err(&client->dev, "stmpe1801_dev_power_off error\n");
		goto err_power;
	}

	device_data->input_dev->dev.parent = &client->dev;
	device_data->input_dev->name = device_data->dtdata->project;
	device_data->input_dev->id.bustype = BUS_I2C;
	device_data->input_dev->flush = NULL;
	device_data->input_dev->event = NULL;
	device_data->input_dev->open = stmpe1801_dev_kpc_input_open;
	device_data->input_dev->close = stmpe1801_dev_kpc_input_close;


	input_set_drvdata(device_data->input_dev, device_data);
	input_set_capability(device_data->input_dev, EV_MSC, MSC_SCAN);
    __set_bit(EV_KEY, device_data->input_dev->evbit);
	if (device_data->dtdata->repeat)
		__set_bit(EV_REP, device_data->input_dev->evbit);

	device_data->input_dev->keycode = device_data->keycode;
	device_data->input_dev->keycodesize = sizeof(unsigned short);
	device_data->input_dev->keycodemax = device_data->keycode_entries;

	matrix_keypad_build_keymap(device_data->keymap_data, NULL,
		device_data->dtdata->num_row, device_data->dtdata->num_column, device_data->keycode, device_data->input_dev);

// request irq
	if (device_data->dtdata->gpio_int >= 0) {
		device_data->dev_irq = gpio_to_irq(device_data->dtdata->gpio_int);
#ifdef DBG_PRN
		printk("%s INT mode (%d)\n", __func__, device_data->dev_irq);
#endif
		ret = request_threaded_irq(device_data->dev_irq, NULL, stmpe1801_dev_isr,
#ifdef STMPE1801_TRIGGER_TYPE_FALLING
			IRQF_TRIGGER_FALLING|IRQF_ONESHOT,
#else
			IRQF_TRIGGER_LOW|IRQF_ONESHOT,
#endif
			client->name, device_data);

		if (ret < 0) {
			dev_err(&client->dev, "stmpe1801_dev_power_off error\n");
			goto interrupt_err;
		}
		disable_irq_nosync(device_data->dev_irq);
	}
	else {
#ifdef DBG_PRN
		printk("%s poll mode\n", __func__);
#endif
		INIT_DELAYED_WORK(&device_data->worker, stmpe1801_dev_worker);
		schedule_delayed_work(&device_data->worker, STMPE1801_TMR_INTERVAL);
	}


	ret = input_register_device(device_data->input_dev);
	if (ret) {
		dev_err(&client->dev, "stmpe1801_dev_power_off error\n");
		goto input_err;
	}


	device_data->sec_keypad = device_create(sec_class, NULL, 0, device_data, "sec_keypad");
	if (IS_ERR(device_data->sec_keypad))
		dev_err(&client->dev, "Failed to create sec_key device\n");

	ret = sysfs_create_group(&device_data->sec_keypad->kobj, &key_attr_group);
	if (ret) {
		dev_err(&client->dev, "Failed to create the test sysfs: %d\n",
			ret);
	}

	return ret;

input_err:
interrupt_err:
err_power:
	kfree(device_data->keycode);

err_keycode:
err_config:
	input_free_device(device_data->input_dev);
	mutex_destroy(&device_data->dev_lock);

err_input_alloc:
	kfree(device_data);

#ifdef DBG_PRN
	printk("Error at stmpe1801_dev_probe\n");
#endif

	return ret;
}

static int stmpe1801_dev_remove(struct i2c_client *client)
{
	struct stmpe1801_dev *device_data = i2c_get_clientdata(client);

	device_init_wakeup(&client->dev, 0);

	input_unregister_device(device_data->input_dev);
	input_free_device(device_data->input_dev);

	regulator_disable(device_data->dtdata->vdd_vreg);

	if (device_data->dtdata->gpio_int >= 0)
		free_irq(device_data->dev_irq, device_data);

	kfree(device_data->keycode);
	kfree(device_data);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int stmpe1801_dev_suspend(struct device *dev)
{
	struct stmpe1801_dev *device_data = dev_get_drvdata(dev);

#ifdef DBG_PRN
	printk(" %s\n", __func__);
#endif

	if (device_may_wakeup(dev)) {
		if (device_data->dtdata->gpio_int >= 0)
			enable_irq_wake(device_data->dev_irq);
	} else {
		mutex_lock(&device_data->input_dev->mutex);

		if (device_data->input_dev->users)
			stmpe1801_dev_kpc_disable(device_data);

		mutex_unlock(&device_data->input_dev->mutex);
	}

	return 0;
}

static int stmpe1801_dev_resume(struct device *dev)
{
	struct stmpe1801_dev *device_data = dev_get_drvdata(dev);

#ifdef DBG_PRN
	printk(" %s\n", __func__);
#endif

	if (device_may_wakeup(dev)) {
		if (device_data->dtdata->gpio_int >= 0)
			disable_irq_wake(device_data->dev_irq);
	} else {
		mutex_lock(&device_data->input_dev->mutex);

		if (device_data->input_dev->users)
			stmpe1801_dev_kpc_enable(device_data);

		mutex_unlock(&device_data->input_dev->mutex);
	}

	return 0;
}
#endif


static SIMPLE_DEV_PM_OPS(stmpe1801_dev_pm_ops, stmpe1801_dev_suspend, stmpe1801_dev_resume);


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
#define abov_match_table NULL
#endif
static struct i2c_driver stmpe1801_dev_driver = {
	.driver = {
		.name = STMPE1801_DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = stmpe1801_match_table,
		.pm = &stmpe1801_dev_pm_ops,
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
module_init(stmpe1801_dev_init);
module_exit(stmpe1801_dev_exit);

MODULE_DEVICE_TABLE(i2c, stmpe1801_dev_id);

MODULE_DESCRIPTION("STMPE1801 Key Driver");
MODULE_AUTHOR("Gyusung SHIM <gyusung.shim@st.com>");
MODULE_LICENSE("GPL");