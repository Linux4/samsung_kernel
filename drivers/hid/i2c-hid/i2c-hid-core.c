/*
 * HID over I2C protocol implementation
 *
 * Copyright (c) 2012 Benjamin Tissoires <benjamin.tissoires@gmail.com>
 * Copyright (c) 2012 Ecole Nationale de l'Aviation Civile, France
 * Copyright (c) 2012 Red Hat, Inc
 *
 * This code is partly based on "USB HID support for Linux":
 *
 *  Copyright (c) 1999 Andreas Gal
 *  Copyright (c) 2000-2005 Vojtech Pavlik <vojtech@suse.cz>
 *  Copyright (c) 2005 Michael Haboustak <mike-@cinci.rr.com> for Concept2, Inc
 *  Copyright (c) 2007-2008 Oliver Neukum
 *  Copyright (c) 2006-2010 Jiri Kosina
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/pm.h>
#include <linux/device.h>
#include <linux/wait.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/hid.h>
#include <linux/mutex.h>
#include <linux/acpi.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>
#include <linux/version.h>
#include <linux/platform_data/i2c-hid.h>

#include "../hid-ids.h"
#include "i2c-hid.h"

//+P86801AA1-1797, caoxin2.wt, add, 2023.05.19, KB bringup
#include <linux/gpio.h>
#include <linux/err.h>
#include <linux/firmware.h>
#include <linux/hardware_info.h>
//-P86801AA1-1797, caoxin2.wt, add, 2023.05.19, KB bringup

/* quirks to control the device */
#define I2C_HID_QUIRK_SET_PWR_WAKEUP_DEV	BIT(0)
#define I2C_HID_QUIRK_NO_IRQ_AFTER_RESET	BIT(1)
#define I2C_HID_QUIRK_BOGUS_IRQ			BIT(4)
#define I2C_HID_QUIRK_RESET_ON_RESUME		BIT(5)
#define I2C_HID_QUIRK_BAD_INPUT_SIZE		BIT(6)
#define I2C_HID_QUIRK_NO_WAKEUP_AFTER_RESET	BIT(7)

//+P86801AA1-1797, caoxin2.wt, modify, 2023.07.18, KB bringup
/* flags */
#define I2C_HID_STARTED		0
#define I2C_HID_RESET_PENDING	1
#define I2C_HID_READ_PENDING	2
#define I2C_HID_KBFW_UPDATE	3
#define I2C_HID_PROBE	4

#define I2C_HID_PWR_ON		0x00
#define I2C_HID_PWR_SLEEP	0x01

/*ap mcu firmware update*/
#define ONE_WRITY_LEN_MIN	128
#define CMD_HEAD_LEN		10
#define PAGE			1024

/*feature report ID*/
#define GET_ERROR_TYPE		0x03
#define GET_KB_MCU_VERSION	0x04
#define GET_PCBA_SN		0x07
#define GET_ENTIRE_SN		0x09
#define KB_CONTROL_COMMAND	0x0a
#define GET_MCU_ERROR		0x0a
#define IAP_FLAH_PROGRAM	0x0b
#define KB_TEST_VER		0x0e

//input report ID
#define KB_KEYS			0x01
#define KB_SPECIAL_KEYS	0x02
#define EVENT_INPUT		0x03
#define KB_RESPONSE		0x08

/* crc */
#define CRC32_InitalValue	0xFFFFFFFF
#define CRC32_Polynomial	0x04C11DB7
#define CRC32_XOROUT		0x00000000

#define PWR_SET		0

#define HID_KB_FW_UPDATE_BUFSIZE	300

const unsigned int crc32_MPEG_2_table[256] = {
    0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b, 0x1a864db2, 0x1e475005,
    0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61, 0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd,
    0x4c11db70, 0x48d0c6c7, 0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
    0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3, 0x709f7b7a, 0x745e66cd,
    0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039, 0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5,
    0xbe2b5b58, 0xbaea46ef, 0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
    0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb, 0xceb42022, 0xca753d95,
    0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1, 0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d,
    0x34867077, 0x30476dc0, 0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
    0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4, 0x0808d07d, 0x0cc9cdca,
    0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde, 0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02,
    0x5e9f46bf, 0x5a5e5b08, 0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
    0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc, 0xb6238b25, 0xb2e29692,
    0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6, 0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a,
    0xe0b41de7, 0xe4750050, 0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
    0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34, 0xdc3abded, 0xd8fba05a,
    0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637, 0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb,
    0x4f040d56, 0x4bc510e1, 0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
    0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5, 0x3f9b762c, 0x3b5a6b9b,
    0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff, 0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623,
    0xf12f560e, 0xf5ee4bb9, 0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
    0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd, 0xcda1f604, 0xc960ebb3,
    0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7, 0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b,
    0x9b3660c6, 0x9ff77d71, 0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
    0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2, 0x470cdd2b, 0x43cdc09c,
    0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8, 0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24,
    0x119b4be9, 0x155a565e, 0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
    0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a, 0x2d15ebe3, 0x29d4f654,
    0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0, 0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c,
    0xe3a1cbc1, 0xe760d676, 0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
    0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662, 0x933eb0bb, 0x97ffad0c,
    0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668, 0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4,
};

extern char Mcu_name[HARDWARE_MAX_ITEM_LONGTH];
extern u8 pad_mcu_cur_ver[HARDWARE_MAX_ITEM_LONGTH];
extern u8 kb_mcu_cur_ver[HARDWARE_MAX_ITEM_LONGTH];

//kb update
u8 PCBA_SN[13] = {0};
u8 kb_mcu_test_ver[64] = {0};
u8 ENTIRE_SN[65] = {0};
unsigned int data_address,data_len;
const unsigned char *kbfw_data = NULL;
static int KB_FW_FLAGS = 0;
bool probe_status = false;
bool kbd_connect = false;
EXPORT_SYMBOL_GPL(kbd_connect);
bool connect_event = false;

//-P86801AA1-1797, caoxin2.wt, modify, 2023.07.18, KB bringup

//firmware
const struct firmware *fw_entry = NULL;
const struct firmware *kbfw_entry = NULL;

/* debug option */
static bool debug;
module_param(debug, bool, 0444);
MODULE_PARM_DESC(debug, "print a lot of debug information");

#define i2c_hid_dbg(ihid, fmt, arg...)					  \
do {									  \
	if (debug)							  \
		dev_printk(KERN_DEBUG, &(ihid)->client->dev, fmt, ##arg); \
} while (0)

struct i2c_hid_desc {
	__le16 wHIDDescLength;
	__le16 bcdVersion;
	__le16 wReportDescLength;
	__le16 wReportDescRegister;
	__le16 wInputRegister;
	__le16 wMaxInputLength;
	__le16 wOutputRegister;
	__le16 wMaxOutputLength;
	__le16 wCommandRegister;
	__le16 wDataRegister;
	__le16 wVendorID;
	__le16 wProductID;
	__le16 wVersionID;
	__le32 reserved;
} __packed;

struct i2c_hid_cmd {
	unsigned int registerIndex;
	__u8 opcode;
	unsigned int length;
	bool wait;
};

union command {
	u8 data[0];
	struct cmd {
		__le16 reg;
		__u8 reportTypeID;
		__u8 opcode;
	} __packed c;
};

#define I2C_HID_CMD(opcode_) \
	.opcode = opcode_, .length = 4, \
	.registerIndex = offsetof(struct i2c_hid_desc, wCommandRegister)

/* fetch HID descriptor */
static const struct i2c_hid_cmd hid_descr_cmd = { .length = 2 };
/* fetch report descriptors */
static const struct i2c_hid_cmd hid_report_descr_cmd = {
		.registerIndex = offsetof(struct i2c_hid_desc,
			wReportDescRegister),
		.opcode = 0x00,
		.length = 2 };
/* commands */
static const struct i2c_hid_cmd hid_reset_cmd =		{ I2C_HID_CMD(0x01),
							  .wait = true };
static const struct i2c_hid_cmd hid_get_report_cmd =	{ I2C_HID_CMD(0x02) };
static const struct i2c_hid_cmd hid_set_report_cmd =	{ I2C_HID_CMD(0x03) };
static const struct i2c_hid_cmd hid_set_power_cmd =	{ I2C_HID_CMD(0x08) };
static const struct i2c_hid_cmd hid_no_cmd =		{ .length = 0 };

/*
 * These definitions are not used here, but are defined by the spec.
 * Keeping them here for documentation purposes.
 *
 * static const struct i2c_hid_cmd hid_get_idle_cmd = { I2C_HID_CMD(0x04) };
 * static const struct i2c_hid_cmd hid_set_idle_cmd = { I2C_HID_CMD(0x05) };
 * static const struct i2c_hid_cmd hid_get_protocol_cmd = { I2C_HID_CMD(0x06) };
 * static const struct i2c_hid_cmd hid_set_protocol_cmd = { I2C_HID_CMD(0x07) };
 */

struct fw_gpio{
	u32 hid_fw_gpio;
	u32 hid_fw_gpio_flags;
};

static struct fw_gpio kb_fw_gpio;

/* The main device structure */
struct i2c_hid {
	struct i2c_client	*client;	/* i2c client */
	struct hid_device	*hid;	/* pointer to corresponding HID dev */
	union {
		__u8 hdesc_buffer[sizeof(struct i2c_hid_desc)];
		struct i2c_hid_desc hdesc;	/* the HID Descriptor */
	};
	__le16			wHIDDescRegister; /* location of the i2c
						   * register of the HID
						   * descriptor. */
	unsigned int		bufsize;	/* i2c buffer size */
	u8			*inbuf;		/* Input buffer */
	u8			*fwbuf;		/* kb response buffer */
	u8			*rawbuf;	/* Raw Input buffer */
	u8			*cmdbuf;	/* Command buffer */
	u8			*argsbuf;	/* Command arguments buffer */
	unsigned long		flags;		/* device flags */
	unsigned long		quirks;		/* Various quirks */

	wait_queue_head_t	wait;		/* For waiting the interrupt */

	struct i2c_hid_platform_data pdata;
	int kb_fw_irq;
	bool			irq_wake_enabled;
	struct mutex		reset_lock;
	struct wakeup_source *pogopin_wakelock;

	unsigned long		sleep_delay;
	//+P86801AA1-1797, caoxin2.wt, add, 2023.07.18, KB bringup
	u32 kpdmcu_fw_mcu_version;
	u16 kb_fw_current_ver;
	u32 kb_fw_new_ver;
	struct work_struct inreport_parse_work;
	struct work_struct kbfw_update_work;
	struct workqueue_struct *hid_queuework;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_reset_active;
	struct pinctrl_state *pins_reset_suspend;
	struct pinctrl_state *pins_sleep_active;
	struct pinctrl_state *pins_sleep_suspend;
	//-P86801AA1-1797, caoxin2.wt, add, 2023.07.18, KB bringup
};

static const struct i2c_hid_quirks {
	__u16 idVendor;
	__u16 idProduct;
	__u32 quirks;
} i2c_hid_quirks[] = {
	{ USB_VENDOR_ID_WEIDA, HID_ANY_ID,
		I2C_HID_QUIRK_SET_PWR_WAKEUP_DEV },
	{ I2C_VENDOR_ID_HANTICK, I2C_PRODUCT_ID_HANTICK_5288,
		I2C_HID_QUIRK_NO_IRQ_AFTER_RESET },
	{ I2C_VENDOR_ID_ITE, I2C_DEVICE_ID_ITE_VOYO_WINPAD_A15,
		I2C_HID_QUIRK_NO_IRQ_AFTER_RESET },
	{ I2C_VENDOR_ID_RAYDIUM, I2C_PRODUCT_ID_RAYDIUM_3118,
		I2C_HID_QUIRK_NO_IRQ_AFTER_RESET },
	{ USB_VENDOR_ID_ALPS_JP, HID_ANY_ID,
		 I2C_HID_QUIRK_RESET_ON_RESUME },
	{ I2C_VENDOR_ID_SYNAPTICS, I2C_PRODUCT_ID_SYNAPTICS_SYNA2393,
		 I2C_HID_QUIRK_RESET_ON_RESUME },
	{ USB_VENDOR_ID_ITE, I2C_DEVICE_ID_ITE_LENOVO_LEGION_Y720,
		I2C_HID_QUIRK_BAD_INPUT_SIZE },
	/*
	 * Sending the wakeup after reset actually break ELAN touchscreen controller
	 */
	{ USB_VENDOR_ID_ELAN, HID_ANY_ID,
		 I2C_HID_QUIRK_NO_WAKEUP_AFTER_RESET |
		 I2C_HID_QUIRK_BOGUS_IRQ },
	{ 0, 0 }
};

/*
 * i2c_hid_lookup_quirk: return any quirks associated with a I2C HID device
 * @idVendor: the 16-bit vendor ID
 * @idProduct: the 16-bit product ID
 *
 * Returns: a u32 quirks value.
 */
static u32 i2c_hid_lookup_quirk(const u16 idVendor, const u16 idProduct)
{
	u32 quirks = 0;
	int n;

	for (n = 0; i2c_hid_quirks[n].idVendor; n++)
		if (i2c_hid_quirks[n].idVendor == idVendor &&
		    (i2c_hid_quirks[n].idProduct == (__u16)HID_ANY_ID ||
		     i2c_hid_quirks[n].idProduct == idProduct))
			quirks = i2c_hid_quirks[n].quirks;

	return quirks;
}

static void i2c_hid_free_buffers(struct i2c_hid *ihid)
{
	kfree(ihid->inbuf);
	kfree(ihid->fwbuf);
	kfree(ihid->rawbuf);
	kfree(ihid->argsbuf);
	kfree(ihid->cmdbuf);
	ihid->inbuf = NULL;
	ihid->fwbuf = NULL;
	ihid->rawbuf = NULL;
	ihid->cmdbuf = NULL;
	ihid->argsbuf = NULL;
	ihid->bufsize = 0;
}

static int i2c_hid_alloc_buffers(struct i2c_hid *ihid, size_t report_size)
{
	/* the worst case is computed from the set_report command with a
	 * reportID > 15 and the maximum report length */
	int args_len = sizeof(__u8) + /* ReportID */
		       sizeof(__u8) + /* optional ReportID byte */
		       sizeof(__u16) + /* data register */
		       sizeof(__u16) + /* size of the report */
		       report_size; /* report */

	ihid->inbuf = kzalloc(report_size, GFP_KERNEL);
	ihid->fwbuf = kzalloc(report_size, GFP_KERNEL);
	ihid->rawbuf = kzalloc(report_size, GFP_KERNEL);
	ihid->argsbuf = kzalloc(args_len, GFP_KERNEL);
	ihid->cmdbuf = kzalloc(sizeof(union command) + args_len, GFP_KERNEL);

	if (!ihid->inbuf || !ihid->fwbuf || !ihid->rawbuf || !ihid->argsbuf || !ihid->cmdbuf) {
		i2c_hid_free_buffers(ihid);
		return -ENOMEM;
	}

	ihid->bufsize = report_size;

	return 0;
}

static int __i2c_hid_command(struct i2c_client *client,
		const struct i2c_hid_cmd *command, u8 reportID,
		u8 reportType, u8 *args, int args_len,
		unsigned char *buf_recv, int data_len)
{
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	union command *cmd = (union command *)ihid->cmdbuf;
	int ret;
	struct i2c_msg msg[2];
	int msg_num = 1;

	int length = command->length;
	bool wait = command->wait;
	unsigned int registerIndex = command->registerIndex;
	//+P86801AA1-1797,caoxin2.wt,add,2023/05/25,KB add hardware_info
	unsigned char PIDVID[4] = {0x04,0xe8,0xa0,0x35};
	unsigned char PIDVID_read[4];
	//-P86801AA1-1797,caoxin2.wt,add,2023/05/25,KB add hardware_info

	/* special case for hid_descr_cmd */
	if (command == &hid_descr_cmd) {
		cmd->c.reg = ihid->wHIDDescRegister;
	} else {
		cmd->data[0] = ihid->hdesc_buffer[registerIndex];
		cmd->data[1] = ihid->hdesc_buffer[registerIndex + 1];
	}

	if (length > 2) {
		cmd->c.opcode = command->opcode;
		cmd->c.reportTypeID = reportID | reportType << 4;
	}

	memcpy(cmd->data + length, args, args_len);
	length += args_len;

	i2c_hid_dbg(ihid, "%s: cmd=%*ph\n", __func__, length, cmd->data);

	msg[0].addr = client->addr;
	msg[0].flags = client->flags & I2C_M_TEN;
	msg[0].len = length;
	msg[0].buf = cmd->data;
	if (data_len > 0) {
		msg[1].addr = client->addr;
		msg[1].flags = client->flags & I2C_M_TEN;
		msg[1].flags |= I2C_M_RD;
		msg[1].len = data_len;
		msg[1].buf = buf_recv;
		msg_num = 2;
		set_bit(I2C_HID_READ_PENDING, &ihid->flags);
	}

	if (wait)
		set_bit(I2C_HID_RESET_PENDING, &ihid->flags);

	ret = i2c_transfer(client->adapter, msg, msg_num);
	i2c_hid_dbg(ihid, "%s: recv = %*ph\n", __func__, data_len, buf_recv);
	if(command == &hid_descr_cmd){
	//+P86801AA1-1797,caoxin2.wt,add,2023/05/25,KB add hardware_info
		PIDVID_read[0] = buf_recv[21];
		PIDVID_read[1] = buf_recv[20];
		PIDVID_read[2] = buf_recv[23];
		PIDVID_read[3] = buf_recv[22];
		dev_err(&client->dev, "%s: strncmp = %d\n", __func__, strncmp(PIDVID_read, PIDVID,4));
		if (!strncmp(PIDVID_read, PIDVID, 4)) {
			snprintf(Mcu_name,HARDWARE_MAX_ITEM_LONGTH,"CS32F03");
			dev_err(&client->dev, "%s: Mcu_name :%s\n", __func__, Mcu_name);
		} else {
			snprintf(Mcu_name,HARDWARE_MAX_ITEM_LONGTH,"NULL");
			dev_err(&client->dev, "%s: Mcu_name :%s\n", __func__, Mcu_name);
			return -ENODEV;
		}

	//-P86801AA1-1797,caoxin2.wt,add,2023/05/25,KB add hardware_info
	}

	if (data_len > 0)
		clear_bit(I2C_HID_READ_PENDING, &ihid->flags);

	if (ret != msg_num)
		return ret < 0 ? ret : -EIO;

	ret = 0;

	if (wait && (ihid->quirks & I2C_HID_QUIRK_NO_IRQ_AFTER_RESET)) {
		msleep(100);
	} else if (wait) {
		i2c_hid_dbg(ihid, "%s: waiting...\n", __func__);
		if (!wait_event_timeout(ihid->wait,
				!test_bit(I2C_HID_RESET_PENDING, &ihid->flags),
				msecs_to_jiffies(5000)))
			ret = -ENODATA;
		i2c_hid_dbg(ihid, "%s: finished.\n", __func__);
	}

	return ret;
}

static int i2c_hid_command(struct i2c_client *client,
		const struct i2c_hid_cmd *command,
		unsigned char *buf_recv, int data_len)
{
	return __i2c_hid_command(client, command, 0, 0, NULL, 0,
				buf_recv, data_len);
}

static int i2c_hid_get_report(struct i2c_client *client, u8 reportType,
		u8 reportID, unsigned char *buf_recv, int data_len)
{
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	u8 args[3];
	int ret;
	int args_len = 0;
	u16 readRegister = le16_to_cpu(ihid->hdesc.wDataRegister);

	i2c_hid_dbg(ihid, "%s\n", __func__);

	if (reportID >= 0x0F) {
		args[args_len++] = reportID;
		reportID = 0x0F;
	}

	args[args_len++] = readRegister & 0xFF;
	args[args_len++] = readRegister >> 8;

	ret = __i2c_hid_command(client, &hid_get_report_cmd, reportID,
		reportType, args, args_len, buf_recv, data_len);
	if (ret) {
		dev_err(&client->dev,
			"failed to retrieve report from device.\n");
		return ret;
	}

	return 0;
}

/**
 * i2c_hid_set_or_send_report: forward an incoming report to the device
 * @client: the i2c_client of the device
 * @reportType: 0x03 for HID_FEATURE_REPORT ; 0x02 for HID_OUTPUT_REPORT
 * @reportID: the report ID
 * @buf: the actual data to transfer, without the report ID
 * @len: size of buf
 * @use_data: true: use SET_REPORT HID command, false: send plain OUTPUT report
 */
static int i2c_hid_set_or_send_report(struct i2c_client *client, u8 reportType,
		u8 reportID, unsigned char *buf, size_t data_len, bool use_data)
{
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	u8 *args = ihid->argsbuf;
	const struct i2c_hid_cmd *hidcmd;
	int ret;
	u16 dataRegister = le16_to_cpu(ihid->hdesc.wDataRegister);
	u16 outputRegister = le16_to_cpu(ihid->hdesc.wOutputRegister);
	u16 maxOutputLength = le16_to_cpu(ihid->hdesc.wMaxOutputLength);
	u16 size;
	int args_len;
	int index = 0;

	i2c_hid_dbg(ihid, "%s\n", __func__);

	//+P86801AA1-1797, caoxin2.wt, add, 2023.07.18, KB bringup
	if (data_len > ihid->bufsize) {
		return -EINVAL;
	}

	//-P86801AA1-1797, caoxin2.wt, add, 2023.07.18, KB bringup
	size =		2			/* size */ +
			(reportID ? 1 : 0)	/* reportID */ +
			data_len		/* buf */;
	args_len =	(reportID >= 0x0F ? 1 : 0) /* optional third byte */ +
			2			/* dataRegister */ +
			size			/* args */;

	if (!use_data && maxOutputLength == 0)
		return -ENOSYS;

	if (reportID >= 0x0F) {
		args[index++] = reportID;
		reportID = 0x0F;
	}

	/*
	 * use the data register for feature reports or if the device does not
	 * support the output register
	 */
	if (use_data) {
		args[index++] = dataRegister & 0xFF;
		args[index++] = dataRegister >> 8;
		hidcmd = &hid_set_report_cmd;
	} else {
		args[index++] = outputRegister & 0xFF;
		args[index++] = outputRegister >> 8;
		hidcmd = &hid_no_cmd;
	}

	args[index++] = size & 0xFF;
	args[index++] = size >> 8;

	if (reportID)
		args[index++] = reportID;

	memcpy(&args[index], buf, data_len);

	ret = __i2c_hid_command(client, hidcmd, reportID,
		reportType, args, args_len, NULL, 0);
	if (ret) {
		dev_err(&client->dev, "failed to set a report to device.\n");
		return ret;
	}

	return data_len;
}

static int i2c_hid_set_power(struct i2c_client *client, int power_state)
{
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	int ret;

	i2c_hid_dbg(ihid, "%s\n", __func__);

	if (!PWR_SET)
		return 0;
	/*
	 * Some devices require to send a command to wakeup before power on.
	 * The call will get a return value (EREMOTEIO) but device will be
	 * triggered and activated. After that, it goes like a normal device.
	 */
	if (power_state == I2C_HID_PWR_ON &&
	    ihid->quirks & I2C_HID_QUIRK_SET_PWR_WAKEUP_DEV) {
		ret = i2c_hid_command(client, &hid_set_power_cmd, NULL, 0);

		/* Device was already activated */
		if (!ret)
			goto set_pwr_exit;
	}

	ret = __i2c_hid_command(client, &hid_set_power_cmd, power_state,
		0, NULL, 0, NULL, 0);

	if (ret)
		dev_err(&client->dev, "failed to change power setting.\n");

set_pwr_exit:

	/*
	 * The HID over I2C specification states that if a DEVICE needs time
	 * after the PWR_ON request, it should utilise CLOCK stretching.
	 * However, it has been observered that the Windows driver provides a
	 * 1ms sleep between the PWR_ON and RESET requests.
	 * According to Goodix Windows even waits 60 ms after (other?)
	 * PWR_ON requests. Testing has confirmed that several devices
	 * will not work properly without a delay after a PWR_ON request.
	 */
	if (!ret && power_state == I2C_HID_PWR_ON)
		msleep(60);

	return ret;
}

static int i2c_hid_hwreset(struct i2c_client *client)
{
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	int ret;

	i2c_hid_dbg(ihid, "%s\n", __func__);

	/*
	 * This prevents sending feature reports while the device is
	 * being reset. Otherwise we may lose the reset complete
	 * interrupt.
	 */
	mutex_lock(&ihid->reset_lock);

	ret = i2c_hid_set_power(client, I2C_HID_PWR_ON);
	if (ret)
		goto out_unlock;

	i2c_hid_dbg(ihid, "resetting...\n");

	ret = i2c_hid_command(client, &hid_reset_cmd, NULL, 0);
	if (ret) {
		dev_err(&client->dev, "failed to reset device.\n");
		i2c_hid_set_power(client, I2C_HID_PWR_SLEEP);
		goto out_unlock;
	} else {
		dev_err(&client->dev, "reset device success.\n");
	}

	/* At least some SIS devices need this after reset */
	if (!(ihid->quirks & I2C_HID_QUIRK_NO_WAKEUP_AFTER_RESET))
		ret = i2c_hid_set_power(client, I2C_HID_PWR_ON);

out_unlock:
	mutex_unlock(&ihid->reset_lock);
	return ret;
}

//+P86801AA1-1797, caoxin2.wt, add, 2023.07.17, KB bringup
static int kb_pinctrl_select_sleep_active(struct i2c_hid *ihid)
{
	int ret = 0;
	struct i2c_client *client = ihid->client;
	if (ihid->pinctrl && ihid->pins_sleep_active) {
		ret = pinctrl_select_state(ihid->pinctrl, ihid->pins_sleep_active);
		if (ret < 0) {
			dev_err(&client->dev,"Set MCU sleep active pin state error:%d", ret);
		}
	}
	return ret;
}

static int kb_pinctrl_select_sleep_suspend(struct i2c_hid *ihid)
{
	int ret = 0;
	struct i2c_client *client = ihid->client;
	if (ihid->pinctrl && ihid->pins_sleep_suspend) {
		ret = pinctrl_select_state(ihid->pinctrl, ihid->pins_sleep_suspend);
		if (ret < 0) {
			dev_err(&client->dev,"Set MCU sleep suspend pin state error:%d", ret);
		}
	}
	return ret;
}

static void pad_mcu_suspend(struct i2c_hid *ihid)
{
	int ret;
	ret = kb_pinctrl_select_sleep_active(ihid);
	if (ret < 0)
		dev_err(&ihid->client->dev, "%s:Mcu suspend fail, ret = %d\n", __func__, ret);
	else
		dev_err(&ihid->client->dev, "%s:Mcu suspend success\n", __func__);
}

static void pad_mcu_resume(struct i2c_hid *ihid)
{
	int ret;
	ret = kb_pinctrl_select_sleep_suspend(ihid);
	if (ret < 0)
		dev_err(&ihid->client->dev, "%s:Mcu resume fail, ret = %d\n", __func__, ret);
	else
		dev_err(&ihid->client->dev, "%s:Mcu resume success\n", __func__);
}

static int i2c_hid_connect(struct i2c_hid *ihid)
{
	int ret;

	if (IS_ERR_OR_NULL(ihid->hid) && !ihid->hid->driver){
		dev_err(&ihid->client->dev, "%s:hid dev error!\n", __func__);
		return -ENODEV;
	}

	if (!kbd_connect){
		ret = hid_hw_start(ihid->hid, HID_CONNECT_HIDINPUT);
		if (ret) {
			dev_err(&ihid->client->dev, "%s:keyboard detected,register input dev fail\n", __func__);
			return ret;
		} else {
			kbd_connect = true;
			pad_mcu_resume(ihid);
			dev_err(&ihid->client->dev, "%s:keyboard detected,register input dev success\n", __func__);
		}
	} else {
		dev_err(&ihid->client->dev, "%s:keyboard detected again\n", __func__);
	}

	return 0;
}

static int i2c_hid_disconnect(struct i2c_hid *ihid)
{
	if (IS_ERR_OR_NULL(ihid->hid)){
		dev_err(&ihid->client->dev, "%s:hid dev error!\n", __func__);
		return -ENODEV;
	}

	if (kbd_connect){
		hid_hw_stop(ihid->hid);
		kbd_connect = false;
		pad_mcu_suspend(ihid);
		if (test_bit(I2C_HID_KBFW_UPDATE,&ihid->flags)) {
			clear_bit(I2C_HID_KBFW_UPDATE,&ihid->flags);
		}
		dev_err(&ihid->client->dev, "%s:keyboard detected,unregister input dev success\n", __func__);
	} else {
		dev_err(&ihid->client->dev, "%s:keyboard disconnect again\n", __func__);
	}

	return 0;
}
//-P86801AA1-1797, caoxin2.wt, add, 2023.07.17, KB bringup

static int i2c_hid_get_report_length(struct hid_report *report)
{
	return ((report->size - 1) >> 3) + 1 +
		report->device->report_enum[report->type].numbered + 2;
}

/*
 * Traverse the supplied list of reports and find the longest
 */
static void i2c_hid_find_max_report(struct hid_device *hid, unsigned int type,
		unsigned int *max)
{
	struct hid_report *report;
	unsigned int size;

	/* We should not rely on wMaxInputLength, as some devices may set it to
	 * a wrong length. */
	list_for_each_entry(report, &hid->report_enum[type].report_list, list) {
		size = i2c_hid_get_report_length(report);
		if (*max < size)
			*max = size;
	}
}

static int i2c_hid_get_raw_report(struct hid_device *hid,
		unsigned char report_number, __u8 *buf, size_t count,
		unsigned char report_type)
{
	struct i2c_client *client = hid->driver_data;
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	size_t ret_count, ask_count;
	int ret;

	if (report_type == HID_OUTPUT_REPORT)
		return -EINVAL;

	/*
	 * In case of unnumbered reports the response from the device will
	 * not have the report ID that the upper layers expect, so we need
	 * to stash it the buffer ourselves and adjust the data size.
	 */
	if (!report_number) {
		buf[0] = 0;
		buf++;
		count--;
	}

	/* +2 bytes to include the size of the reply in the query buffer */
	ask_count = min(count + 2, (size_t)ihid->bufsize);

	ret = i2c_hid_get_report(client,
			report_type == HID_FEATURE_REPORT ? 0x03 : 0x01,
			report_number, ihid->rawbuf, ask_count);

	if (ret < 0)
		return ret;

	ret_count = ihid->rawbuf[0] | (ihid->rawbuf[1] << 8);

	if (ret_count <= 2)
		return 0;

	ret_count = min(ret_count, ask_count);

	/* The query buffer contains the size, dropping it in the reply */
	count = min(count, ret_count - 2);
	memcpy(buf, ihid->rawbuf + 2, count);

	if (!report_number)
		count++;

	return count;
}

static int i2c_hid_output_raw_report(struct hid_device *hid, __u8 *buf,
		size_t count, unsigned char report_type, bool use_data)
{
	struct i2c_client *client = hid->driver_data;
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	int report_id = buf[0];
	int ret;

	if (report_type == HID_INPUT_REPORT)
		return -EINVAL;

	mutex_lock(&ihid->reset_lock);

	/*
	 * Note that both numbered and unnumbered reports passed here
	 * are supposed to have report ID stored in the 1st byte of the
	 * buffer, so we strip it off unconditionally before passing payload
	 * to i2c_hid_set_or_send_report which takes care of encoding
	 * everything properly.
	 */
	ret = i2c_hid_set_or_send_report(client,
				report_type == HID_FEATURE_REPORT ? 0x03 : 0x02,
				report_id, buf + 1, count - 1, use_data);

	if (ret >= 0)
		ret++; /* add report_id to the number of transferred bytes */

	mutex_unlock(&ihid->reset_lock);

	return ret;
}

static int i2c_hid_output_report(struct hid_device *hid, __u8 *buf,
		size_t count)
{
	return i2c_hid_output_raw_report(hid, buf, count, HID_OUTPUT_REPORT,
			false);
}

static int i2c_hid_raw_request(struct hid_device *hid, unsigned char reportnum,
			       __u8 *buf, size_t len, unsigned char rtype,
			       int reqtype)
{
	switch (reqtype) {
	case HID_REQ_GET_REPORT:
		return i2c_hid_get_raw_report(hid, reportnum, buf, len, rtype);
	case HID_REQ_SET_REPORT:
		if (buf[0] != reportnum)
			return -EINVAL;
		return i2c_hid_output_raw_report(hid, buf, len, rtype, true);
	default:
		return -EIO;
	}
}

//+P86801AA1-11506, caoxin2.wt, add, 2023.08.18, ADD NODES
static int get_PCBA_SN(struct i2c_hid *ihid)
{
	int ret;
	__u8* buf = (__u8*)kzalloc(14, GFP_KERNEL);
	ret = ihid->hid->ll_driver->raw_request(ihid->hid, GET_PCBA_SN, buf, 14, HID_FEATURE_REPORT, HID_REQ_GET_REPORT);
	if (ret < 0){
		dev_err(&ihid->client->dev,"%s:raw_request fail,ret=%d\n",__func__, ret);
		dev_err(&ihid->client->dev, "get_PCBA_SN fail\n", __func__);
		goto out;
	}
	memcpy(PCBA_SN, buf + 1, 13);
	i2c_hid_dbg(ihid, "%s:PCBA_SN: %*ph\n", __func__, 13, PCBA_SN);
	ret = 0;
out:
	kfree(buf);
	return ret;
}

static int get_ENTIRE_SN(struct i2c_hid *ihid)
{
	int ret;
	__u8* buf = (__u8*)kzalloc(66, GFP_KERNEL);
	ret = ihid->hid->ll_driver->raw_request(ihid->hid, GET_ENTIRE_SN, buf, 66, HID_FEATURE_REPORT, HID_REQ_GET_REPORT);
	if (ret < 0){
		dev_err(&ihid->client->dev,"%s:raw_request fail,ret=%d\n",__func__, ret);
		dev_err(&ihid->client->dev, "get_ENTIRE_SN fail\n", __func__);
		goto out;
	}
	memcpy(ENTIRE_SN, buf + 1, 65);
	i2c_hid_dbg(ihid, "%s:ENTIRE_SN: %*ph\n", __func__, 65, ENTIRE_SN);
	ret = 0;
out:
	kfree(buf);
	return ret;
}

static int get_kb_mcu_ver(struct i2c_hid *ihid)
{
	int ret;
	__u8* buf = (__u8*)kzalloc(65, GFP_KERNEL);
	ret = ihid->hid->ll_driver->raw_request(ihid->hid, KB_TEST_VER, buf, 65, HID_FEATURE_REPORT, HID_REQ_GET_REPORT);
	if (ret < 0){
		dev_err(&ihid->client->dev,"%s:raw_request fail,ret=%d\n",__func__, ret);
		dev_err(&ihid->client->dev, "get KB_TEST_VER fail\n", __func__);
		goto out;
	}
	memcpy(kb_mcu_test_ver, buf + 1, 64);
	i2c_hid_dbg(ihid, "%s:kb_mcu_test_ver: %*ph\n", __func__, 64, kb_mcu_test_ver);
	ret = 0;
out:
	kfree(buf);
	return ret;
}

static ssize_t PCBA_SN_show(struct class *class, struct class_attribute *attr, char *buf)
{
	ssize_t len = 0;

	len += snprintf(buf+len, PAGE_SIZE, "%*ph\n", 13, PCBA_SN);
	return len;
}

static ssize_t ENTIRE_SN_show(struct class *class, struct class_attribute *attr, char *buf)
{
	ssize_t len = 0;
	int i;

	for(i = 0; i < 65; i++){
		len += snprintf(buf+len, PAGE_SIZE, "%02X ", ENTIRE_SN[i]);
	}
	len += snprintf(buf+len, PAGE_SIZE, "\n");

	return len;
}

static ssize_t kb_mcu_ver_show(struct class *class, struct class_attribute *attr, char *buf)
{
	ssize_t len = 0;
	int i;

	for(i = 0; i < 64; i++){
		len += snprintf(buf+len, PAGE_SIZE, "%02X ", kb_mcu_test_ver[i]);
	}
	len += snprintf(buf+len, PAGE_SIZE, "\n");

	return len;
}

static ssize_t pad_mcu_ver_show(struct class *class, struct class_attribute *attr, char *buf)
{
	ssize_t len = 0;

	len += snprintf(buf+len, PAGE_SIZE, "%s\n", pad_mcu_cur_ver);
	return len;
}

static ssize_t kb_cur_ver_show(struct class *class, struct class_attribute *attr, char *buf)
{
	ssize_t len = 0;

	len += snprintf(buf+len, PAGE_SIZE, "%s\n", kb_mcu_cur_ver);
	return len;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0))
static struct class_attribute class_attr_kb_mcu_ver = __ATTR(kb_mcu_ver, 0664, kb_mcu_ver_show, NULL);
static struct class_attribute class_attr_kb_cur_ver = __ATTR(kb_cur_ver, 0664, kb_cur_ver_show, NULL);
static struct class_attribute class_attr_pad_mcu_ver = __ATTR(pad_mcu_ver, 0664, pad_mcu_ver_show, NULL);
static struct class_attribute class_attr_PCBA_SN = __ATTR(PCBA_SN, 0664, PCBA_SN_show, NULL);
static struct class_attribute class_attr_ENTIRE_SN = __ATTR(ENTIRE_SN, 0664, ENTIRE_SN_show, NULL);

static struct attribute *pad_keyboard_class_attrs[] = {
	&class_attr_kb_mcu_ver.attr,
	&class_attr_kb_cur_ver.attr,
	&class_attr_pad_mcu_ver.attr,
	&class_attr_PCBA_SN.attr,
	&class_attr_ENTIRE_SN.attr,
	NULL,
};
ATTRIBUTE_GROUPS(pad_keyboard_class);

#else
static struct class_attribute pad_keyboard_class_attributes[] = {
	__ATTR(pad_mcu_ver, 0664, pad_mcu_ver_show, NULL),
	__ATTR(kb_mcu_ver, 0664, kb_mcu_ver_show, NULL),
	__ATTR(kb_cur_ver, 0664, kb_cur_ver_show, NULL),
	__ATTR(PCBA_SN, 0664, PCBA_SN_show, NULL);
	__ATTR(ENTIRE_SN, 0664, ENTIRE_SN_show, NULL);
	__ATTR_NULL,
}
#endif

static struct class pad_keyboard_class = {
	.name = "pad_keyboard",
	.owner = THIS_MODULE,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0)
	.class_groups = pad_keyboard_class_groups,
#else
	.class_attrs = pad_keyboard_class_attributes,
#endif
};

int pad_keyboard_sysfs_init(struct i2c_hid *ihid)
{
	int ret = 0;

	ret = class_register(&pad_keyboard_class);
	if (!ret) {
		dev_err(&ihid->client->dev, "pad_keyboard_class register\n", __func__);
	}

	return ret;
}

void pad_keyboard_sysfs_deinit(struct i2c_hid *ihid)
{
	class_unregister(&pad_keyboard_class);
	dev_err(&ihid->client->dev, "pad_keyboard_class unregister\n", __func__);
}
//-P86801AA1-11506, caoxin2.wt, add, 2023.08.18, ADD NODES

static int i2c_hid_parse(struct hid_device *hid)
{
	struct i2c_client *client = hid->driver_data;
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	struct i2c_hid_desc *hdesc = &ihid->hdesc;
	unsigned int rsize;
	char *rdesc;
	int ret;
	int tries = 3;
	char *use_override;

	i2c_hid_dbg(ihid, "entering %s\n", __func__);

	rsize = le16_to_cpu(hdesc->wReportDescLength);
	if (!rsize || rsize > HID_MAX_DESCRIPTOR_SIZE) {
		dbg_hid("weird size of report descriptor (%u)\n", rsize);
		return -EINVAL;
	}

	do {
		ret = i2c_hid_hwreset(client);
		if (ret)
			msleep(1000);
	} while (tries-- > 0 && ret);

	if (ret)
		return ret;

	use_override = i2c_hid_get_dmi_hid_report_desc_override(client->name,
								&rsize);

	if (use_override) {
		rdesc = use_override;
		i2c_hid_dbg(ihid, "Using a HID report descriptor override\n");
	} else {
		rdesc = kzalloc(rsize, GFP_KERNEL);

		if (!rdesc) {
			dbg_hid("couldn't allocate rdesc memory\n");
			return -ENOMEM;
		}

		i2c_hid_dbg(ihid, "asking HID report descriptor\n");

		ret = i2c_hid_command(client, &hid_report_descr_cmd,
				      rdesc, rsize);
		if (ret) {
			hid_err(hid, "reading report descriptor failed\n");
			kfree(rdesc);
			return -EIO;
		}
	}

	i2c_hid_dbg(ihid, "Report Descriptor: %*ph\n", rsize, rdesc);

	ret = hid_parse_report(hid, rdesc, rsize);
	if (!use_override)
		kfree(rdesc);

	if (ret) {
		dbg_hid("parsing report descriptor failed\n");
		return ret;
	}

	return 0;
}

static int i2c_hid_start(struct hid_device *hid)
{
	struct i2c_client *client = hid->driver_data;
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	int ret;
	unsigned int bufsize = HID_KB_FW_UPDATE_BUFSIZE;

	i2c_hid_find_max_report(hid, HID_INPUT_REPORT, &bufsize);
	i2c_hid_find_max_report(hid, HID_OUTPUT_REPORT, &bufsize);
	i2c_hid_find_max_report(hid, HID_FEATURE_REPORT, &bufsize);

	if (bufsize > ihid->bufsize) {
		disable_irq(client->irq);
		i2c_hid_free_buffers(ihid);

		ret = i2c_hid_alloc_buffers(ihid, bufsize);
		enable_irq(client->irq);

		if (ret)
			return ret;
	}

	return 0;
}

static void i2c_hid_stop(struct hid_device *hid)
{
	hid->claimed = 0;
}

static int i2c_hid_open(struct hid_device *hid)
{
	struct i2c_client *client = hid->driver_data;
	struct i2c_hid *ihid = i2c_get_clientdata(client);

	set_bit(I2C_HID_STARTED, &ihid->flags);
	return 0;
}

static void i2c_hid_close(struct hid_device *hid)
{
	struct i2c_client *client = hid->driver_data;
	struct i2c_hid *ihid = i2c_get_clientdata(client);

	clear_bit(I2C_HID_STARTED, &ihid->flags);
}

struct hid_ll_driver i2c_hid_ll_driver = {
	.parse = i2c_hid_parse,
	.start = i2c_hid_start,
	.stop = i2c_hid_stop,
	.open = i2c_hid_open,
	.close = i2c_hid_close,
	.output_report = i2c_hid_output_report,
	.raw_request = i2c_hid_raw_request,
};
EXPORT_SYMBOL_GPL(i2c_hid_ll_driver);

//+P86801AA1-1797, caoxin2.wt, modify, 2023.07.18, KB bringup
static int request_kb_firmware(struct i2c_hid *ihid)
{
	int ret;
	char *fw_path = "MH2113C_kb.bin";

	ret = request_firmware(&kbfw_entry, fw_path, &ihid->client->dev);
	if (ret) {
		dev_err(&ihid->client->dev, "request_kb_firmware fail\n");
		return ret;
	} else {
		dev_err(&ihid->client->dev, "request_kb_firmware success\n");
	}

	//fw_data_count:get from input report
	kbfw_data = kbfw_entry->data;
	return ret;
}

static int fetch_kb_fw_version(struct i2c_hid *ihid)
{
	//get report
	int ret;
	__u8 *buf;
	u32 recv;

	ret = 0;
	buf = (__u8*)kzalloc(4, GFP_KERNEL);
	ret = i2c_hid_raw_request(ihid->hid, GET_KB_MCU_VERSION,buf,4,HID_FEATURE_REPORT,HID_REQ_GET_REPORT);
	i2c_hid_dbg(ihid, "%s:recv=%*ph\n",__func__, 4, buf);
	if (ret < 0){
		dev_err(&ihid->client->dev, "%s:request fail,ret=%d\n",__func__, ret);
		goto out;
	}
	recv = (buf[1] & 0xFF) | ((buf[2] << 8) & 0xFF00) | ((buf[3] << 16) & 0xFF0000);
	if (!(recv ^ 0xFFFFFF)){
		dev_err(&ihid->client->dev, "%s:No kb fw verison!\n",__func__);
		ret = -ENODEV;
		goto out;
	} else {
		ihid->kb_fw_current_ver = (recv >> 8) & 0xFFFF;
		dev_err(&ihid->client->dev, "%s:kb_fw_current_ver = %d\n",__func__, ihid->kb_fw_current_ver);
		snprintf(kb_mcu_cur_ver, HARDWARE_MAX_ITEM_LONGTH, "%02X %02X", ((ihid->kb_fw_current_ver >> 8) & 0xFF), (ihid->kb_fw_current_ver & 0xFF));
	}

out:
	kfree(buf);
	buf = NULL;
	return ret;
}

static int kb_fw_IAP_upgrade(struct i2c_hid *ihid)
{
	//set report
	int ret;
	__u8 data[] = {0x0a,0x05,0x00,0x00,0x00,0x00};

	ret = i2c_hid_raw_request(ihid->hid,KB_CONTROL_COMMAND,data,6,HID_FEATURE_REPORT,HID_REQ_SET_REPORT);
	if (ret < 0)
		dev_err(&ihid->client->dev, "%s:raw_request fail,ret=%d\n",__func__, ret);

	return ret;
}

static u32 IAP_flash_program(struct i2c_hid *ihid,unsigned int data_address, unsigned int fw_data_count, const unsigned char *firmware_data)
{
	size_t len;
	int checksum = 0;
	int i,ret,write_len;
	u8* send_rx = (u8*)kzalloc(ONE_WRITY_LEN_MIN + 3, GFP_KERNEL);

	len = ONE_WRITY_LEN_MIN + 3;

	//send fw
	send_rx[0] = 0x0b;
	send_rx[1] = (fw_data_count < ONE_WRITY_LEN_MIN) ? fw_data_count : ONE_WRITY_LEN_MIN;

	write_len = send_rx[1];
	for(i = 0; i < write_len;i++) {
		send_rx[i + 2] = firmware_data[data_address + i];
		checksum += send_rx[i + 2];
	}

	if(fw_data_count < ONE_WRITY_LEN_MIN) {
		for(i = 0; i < ONE_WRITY_LEN_MIN - fw_data_count;i++) {
			send_rx[ONE_WRITY_LEN_MIN + 1 - i] = 0xff;
		}
	}

	send_rx[ONE_WRITY_LEN_MIN + 2] = checksum & 0x000000FF;

	ret = i2c_hid_raw_request(ihid->hid,IAP_FLAH_PROGRAM,send_rx,len,HID_FEATURE_REPORT,HID_REQ_SET_REPORT);
	if (ret < 0){
		dev_err(&ihid->client->dev, "%s:raw_request fail,ret=%d\n",__func__, ret);
		goto out;
	}

	ret = 0;

out:
	kfree(send_rx);
	send_rx = NULL;
	return ret;
}

static int IAP_update_exit(struct i2c_hid *ihid)
{
	int ret;
	__u8 data[] = {0x0a,0x06,0x00,0x00,0x00,0x00};

	ret = i2c_hid_raw_request(ihid->hid,KB_CONTROL_COMMAND,data,6,HID_FEATURE_REPORT,HID_REQ_SET_REPORT);
	if (ret < 0){
		dev_err(&ihid->client->dev, "%s:raw_request fail,ret=%d\n",__func__, ret);
	}
	return ret;

}

static int IAP_jump(struct i2c_hid *ihid)
{
	int ret;
	__u8 data[] = {0x0a,0x07,0x01,0x00,0x00,0x00};

	ret = i2c_hid_raw_request(ihid->hid,KB_CONTROL_COMMAND,data,6,HID_FEATURE_REPORT,HID_REQ_SET_REPORT);
	if (ret < 0){
		dev_err(&ihid->client->dev, "%s:raw_request fail,ret=%d\n",__func__, ret);
	}
	return ret;
}

static int kb_fw_update(struct i2c_hid *ihid)
{
	int ret = 0;
	i2c_hid_dbg(ihid, "%s, enter! KB_FW_FLAGS = %d\n", __func__, KB_FW_FLAGS);

	if (test_bit(I2C_HID_KBFW_UPDATE,&ihid->flags)){
		switch(KB_FW_FLAGS){
			case 1:
				ret = fetch_kb_fw_version(ihid);
				if (ret < 0) {
					dev_err(&ihid->client->dev, "%s:fetch_kb_fw_version fail\n", __func__);
					clear_bit(I2C_HID_KBFW_UPDATE,&ihid->flags);
					break;
				}

				ihid->kb_fw_new_ver = (kbfw_data[0] & 0xFF) | ((kbfw_data[1] << 8) & 0xFF00) | ((kbfw_data[2] << 16) & 0xFF0000) | ((kbfw_data[3] << 8) & 0xFF000000);
				if ((ihid->kb_fw_current_ver < 4) && (ihid->kb_fw_current_ver >= 2)) {
					dev_err(&ihid->client->dev, "%s: Old keyboard, the fw version is %d. Do not upgrade the firmware.\n", __func__, ihid->kb_fw_current_ver);
					clear_bit(I2C_HID_KBFW_UPDATE,&ihid->flags);
					break;
				} else if ((ihid->kb_fw_current_ver < 2) &&(ihid->kb_fw_new_ver >= 4)) {
					dev_err(&ihid->client->dev, "%s: New firmware V%02X cannot be upgraded to old keyboards\n", __func__, ihid->kb_fw_new_ver);
					clear_bit(I2C_HID_KBFW_UPDATE,&ihid->flags);
					break;
				}
				if ((ihid->kb_fw_new_ver > ihid->kb_fw_current_ver) || (ihid->kb_fw_current_ver > 0xAA00)) {
					dev_err(&ihid->client->dev, "%s,start kb fw upgrade\n", __func__);
					ret = kb_fw_IAP_upgrade(ihid);
					if (ret < 0){
						clear_bit(I2C_HID_KBFW_UPDATE,&ihid->flags);
					}
				} else {
					dev_err(&ihid->client->dev, "%s,No kb mcu firmware upgrade required,ihid->kb_fw_new_ver = %d <= ihid->kb_fw_current_ver = %d\n",
							__func__, ihid->kb_fw_new_ver, ihid->kb_fw_current_ver);
					clear_bit(I2C_HID_KBFW_UPDATE,&ihid->flags);
				}
				break;
			case 2:
				ret = IAP_flash_program(ihid,data_address,data_len,kbfw_data);
				if (ret < 0) {
					dev_err(&ihid->client->dev, "%s, IAP_flash_program fail\n", __func__);
					dev_err(&ihid->client->dev, "%s, KB_FW_FLAGS = %d, data_len = %d, data_address = %d\n", __func__, KB_FW_FLAGS, data_len, data_address);
					clear_bit(I2C_HID_KBFW_UPDATE,&ihid->flags);
				}
				break;
			case 3:
				ret = IAP_update_exit(ihid);
				if (ret < 0){
					dev_err(&ihid->client->dev, "%s, IAP_update_exit fail\n", __func__);
					clear_bit(I2C_HID_KBFW_UPDATE,&ihid->flags);
				}
				break;
			case 4:
				ret = IAP_jump(ihid);
				if (ret < 0){
					dev_err(&ihid->client->dev, "%s, IAP_jump fail\n", __func__);
					clear_bit(I2C_HID_KBFW_UPDATE,&ihid->flags);
				}
				break;
			default:
				ret = -EINVAL;
				clear_bit(I2C_HID_KBFW_UPDATE,&ihid->flags);
				break;
		}
	}
	return ret;
}

static void kb_response_nack_parse(struct i2c_hid *ihid)
{
	int nak_reason;

	nak_reason = ihid->fwbuf[4];
	switch(nak_reason){
		case 0x1:
			dev_err(&ihid->client->dev, "%s,Flash program data checksum err\n", __func__);
			break;
		case 0x2:
			dev_err(&ihid->client->dev, "%s,Execution status error\n", __func__);
			break;
		case 0x3:
			dev_err(&ihid->client->dev, "%s,kb fw crc check err\n", __func__);
			break;
		case 0x4:
			dev_err(&ihid->client->dev, "%s,Unsupported devices\n", __func__);
			break;
		case 0x5:
			dev_err(&ihid->client->dev, "%s,kb disconnect\n", __func__);
			break;
		case 0x6:
			dev_err(&ihid->client->dev, "%s,Unsupported report ID\n", __func__);
			break;
		case 0x7:
			dev_err(&ihid->client->dev, "%s,Unsupported cmd type\n", __func__);
			break;
		case 0x8:
			dev_err(&ihid->client->dev, "%s,Device busy\n", __func__);
			break;
		case 0x9:
			dev_err(&ihid->client->dev, "%s,One wire NACK\n", __func__);
			break;
		case 0xa:
			dev_err(&ihid->client->dev, "%s,Other err\n", __func__);
			break;
		default:
			break;
	}
}

static int input_kb_response_parse(struct i2c_hid *ihid)
{
	int ret, kb_ack;
	u8 *input_buf;

	ret = 0;
	input_buf = ihid->fwbuf;
	kb_ack = input_buf[2];

	i2c_hid_dbg(ihid, "%s,kb response:Processing status %d\n", __func__, input_buf[1]);

	if (ihid->pogopin_wakelock) {
		__pm_stay_awake(ihid->pogopin_wakelock);
	}

	switch(input_buf[3]){
		case 0x5a:
			if (kb_ack){
				dev_err(&ihid->client->dev, "%s,kb fw upgrade:enter\n", __func__);
				data_address = input_buf[4] | ((input_buf[5] << 8) & 0x0000FF00);
				data_address |= (input_buf[6] << 16) & 0x00FF0000;
				data_address |= (input_buf[7] << 24) & 0xFF000000;
				data_len = input_buf[8] | ((input_buf[9] << 8) & 0x0000FF00);
				dev_err(&ihid->client->dev, "%s, KB_FW_FLAGS = %d, data_len = %d, data_address = %d\n", __func__, KB_FW_FLAGS, data_len, data_address);

				KB_FW_FLAGS = 2;
				ret = kb_fw_update(ihid);
				if (ret < 0)
					goto upgrade_err;
			} else {
				dev_err(&ihid->client->dev, "%s,kb IAP upgrade NAK\n", __func__);
				goto upgrade_nak;
			}
			break;
		case 0x6a:
			if (kb_ack){
				dev_err(&ihid->client->dev, "%s,IAP UPDATE exit \n", __func__);
				if (!input_buf[4]) {
					dev_err(&ihid->client->dev, "%s,kb fw update success \n", __func__);
					KB_FW_FLAGS = 4;
					ret = kb_fw_update(ihid);
					if (ret < 0)
						goto upgrade_err;
				} else
					dev_err(&ihid->client->dev, "%s,kb fw update fail \n", __func__);
					goto upgrade_end;
			} else {
				dev_err(&ihid->client->dev, "%s,IAP UPDATE exit NAK\n", __func__);
				goto upgrade_nak;
			}
			break;
		case 0x7a:
			if (kb_ack){
				dev_err(&ihid->client->dev, "%s,jump ACK\n", __func__);
				goto upgrade_end;
			} else {
				dev_err(&ihid->client->dev, "%s,jump NAK\n", __func__);
				goto upgrade_nak;
			}
		case 0x0b:
			if (kb_ack){
				data_address = input_buf[4] | ((input_buf[5] << 8) & 0x0000FF00);
				data_address |= (input_buf[6] << 16) & 0x00FF0000;
				data_address |= (input_buf[7] << 24) & 0xFF000000;
				data_len = input_buf[8] | ((input_buf[9] << 8) & 0x0000FF00);
				i2c_hid_dbg(ihid, "%s,0x0b,data_len = %d, data_address = %d\n", __func__, data_len, data_address);

				if (((data_len == 0) && (data_address == 0))) {
					dev_err(&ihid->client->dev, "%s,IAP_flash_program ended\n", __func__);
					KB_FW_FLAGS = 3;
					ret = kb_fw_update(ihid);
					if (ret < 0)
						goto upgrade_err;
				} else {
					KB_FW_FLAGS = 2;
					ret = kb_fw_update(ihid);
					if (ret < 0)
						goto upgrade_err;
				}
			} else {
				dev_err(&ihid->client->dev, "%s,IAP flash program NAK\n", __func__);
				dev_err(&ihid->client->dev, "%s, 0x0b,data_len = %d, data_address = %d\n", __func__, data_len, data_address);
				goto upgrade_nak;
			}
			break;
		default:
			dev_err(&ihid->client->dev, "%s,Unsupported report %d\n", __func__, input_buf[0]);
			break;
	}
	return 0;

upgrade_nak:
	kb_response_nack_parse(ihid);

upgrade_err:
	if (ret < 0)
		dev_err(&ihid->client->dev, "%s:kb fw update fail, ret = %d, KB_FW_FLAGS = %d\n", __func__, ret, KB_FW_FLAGS);

upgrade_end:
	KB_FW_FLAGS = 0;
	clear_bit(I2C_HID_KBFW_UPDATE,&ihid->flags);
	if (ihid->pogopin_wakelock) {
		__pm_relax(ihid->pogopin_wakelock);
	}
	return ret;
}

static int get_error_log(struct i2c_hid *ihid, u8 err_len)
{
	int i,ret;
	ssize_t len = 0;

	char* log = (char*)kzalloc(400, GFP_KERNEL);
	__u8* buf = (__u8*)kzalloc(err_len + 1, GFP_KERNEL);
	ret = i2c_hid_raw_request(ihid->hid, GET_MCU_ERROR, buf, err_len + 1, HID_FEATURE_REPORT, HID_REQ_GET_REPORT);
	if (ret < 0){
		dev_err(&ihid->client->dev, "%s:raw_request fail,ret=%d\n",__func__, ret);
		goto out;
	}
	for (i=0; i<err_len+1; i++)
	{
		len += snprintf(log+len, PAGE_SIZE, "%02X ", buf[i]);
	}
	dev_err(&ihid->client->dev, "%s:ERROR_LOG: %s\n", __func__, log);
	ret = 0;
out:
	kfree(buf);
	kfree(log);
	return ret;
}

static int get_error_type(struct i2c_hid *ihid)
{
	int ret;
	__u8* buf = (__u8*)kzalloc(5, GFP_KERNEL);
	ret = i2c_hid_raw_request(ihid->hid, GET_ERROR_TYPE, buf, 5, HID_FEATURE_REPORT, HID_REQ_GET_REPORT);
	if (ret < 0){
		dev_err(&ihid->client->dev, "%s:raw_request fail,ret=%d\n",__func__, ret);
		goto out;
	}
	dev_err(&ihid->client->dev, "%s:ERROR_TYPE: %*ph\n", __func__, 5, buf);
	ret = 0;
out:
	kfree(buf);
	return ret;
}

static int input_event_report_parse(struct i2c_hid *ihid)
{
	int ret;
	u8 *input_buf;

	ret = 0;
	input_buf = ihid->inbuf;
	i2c_hid_dbg(ihid, "%s:input_buf[3] = %d\n", __func__, input_buf[3]);
	switch(input_buf[3]){
		case 0x04:
			dev_err(&ihid->client->dev, "%s:kbd online\n", __func__);
			connect_event = true;

			if (!wait_event_timeout(ihid->wait,
					test_bit(I2C_HID_PROBE, &ihid->flags),
					msecs_to_jiffies(10000)))
				ret = -ENODEV;

			ret = i2c_hid_connect(ihid);
			if (ret) {
				dev_err(&ihid->client->dev, "%s:i2c hid connect fail\n", __func__);
				break;
			}
			msleep(30);
			dev_err(&ihid->client->dev, "%s:kbd connect end\n", __func__);
			get_kb_mcu_ver(ihid);
			get_ENTIRE_SN(ihid);
			get_PCBA_SN(ihid);

			ret = request_kb_firmware(ihid);
			if (!ret) {
				KB_FW_FLAGS = 1;
				set_bit(I2C_HID_KBFW_UPDATE,&ihid->flags);
				kb_fw_update(ihid);
			}

			break;
		case 0x08:
			dev_err(&ihid->client->dev, "%s:kbd disconnect, communication error\n", __func__);
			connect_event = false;

			ret = i2c_hid_disconnect(ihid);
			if (ret) {
				dev_err(&ihid->client->dev, "%s:i2c hid disconnect fail\n", __func__);
				break;
			}
			memset(PCBA_SN, 0, sizeof(PCBA_SN));
			memset(ENTIRE_SN, 0, sizeof(ENTIRE_SN));
			memset(kb_mcu_test_ver, 0, sizeof(kb_mcu_test_ver));
			snprintf(kb_mcu_cur_ver, HARDWARE_MAX_ITEM_LONGTH, "NULL");
			break;
		case 0x10:
			dev_err(&ihid->client->dev, "%s:kbd disconnect, Heartbeat Timeout\n", __func__);
			connect_event = false;

			ret = i2c_hid_disconnect(ihid);
			if (ret) {
				dev_err(&ihid->client->dev, "%s:i2c hid disconnect fail\n", __func__);
				break;
			}
			memset(PCBA_SN, 0, sizeof(PCBA_SN));
			memset(ENTIRE_SN, 0, sizeof(ENTIRE_SN));
			memset(kb_mcu_test_ver, 0, sizeof(kb_mcu_test_ver));
			snprintf(kb_mcu_cur_ver, HARDWARE_MAX_ITEM_LONGTH, "NULL");
			break;
		case 0x20:
			dev_err(&ihid->client->dev, "%s:HID sleep or wake-up\n", __func__);
			break;
		case 0x40:
			dev_err(&ihid->client->dev, "%s:HID device error\n", __func__);
			ret = get_error_log(ihid, input_buf[4]);
			break;
		case 0x80:
			dev_err(&ihid->client->dev, "%s:KB device error\n", __func__);
			ret = get_error_type(ihid);
			if (ret < 0)
				dev_err(&ihid->client->dev, "%s:GET_ERROR_TYPE fail\n", __func__);
			break;
		default :
			dev_err(&ihid->client->dev, "%s:Unknown Input Report %d\n", __func__, input_buf[2]);
			break;
	}

	return ret;
}

static int input_report_parse(struct i2c_hid *ihid)
{
	int ret;
	u8 reportID;

	ret = 0;
	reportID = ihid->inbuf[2];

	i2c_hid_dbg(ihid, "%s: report %*ph\n", __func__, 11, ihid->inbuf);
	if (reportID == EVENT_INPUT) {
		ret = input_event_report_parse(ihid);
	} else
		dev_err(&ihid->client->dev, "%s:Unknown Input Report, reportID %d\n", __func__, reportID);

	return ret;
}

static void input_report_parse_func(struct work_struct *work)
{
	struct i2c_hid *hid_wq = container_of(work, struct i2c_hid, inreport_parse_work);
	input_report_parse(hid_wq);
}

static void i2c_hid_get_input(struct i2c_hid *ihid)
{
	int ret;
	u32 ret_size;

	int size = le16_to_cpu(ihid->hdesc.wMaxInputLength);

	i2c_hid_dbg(ihid, "%s:enter\n", __func__);

	if (size > ihid->bufsize)
		size = ihid->bufsize;

	ret = i2c_master_recv(ihid->client, ihid->inbuf, size);
	if (ret != size) {
		if (ret < 0)
			return;

		dev_err(&ihid->client->dev, "%s: got %d data instead of %d\n",
			__func__, ret, size);
		return;
	}

	ret_size = ihid->inbuf[0] | ihid->inbuf[1] << 8;
	i2c_hid_dbg(ihid, "%s:input: %*ph\n", __func__, ret_size, ihid->inbuf);

	if (!ret_size) {
		/* host or device initiated RESET completed */
		if (test_and_clear_bit(I2C_HID_RESET_PENDING, &ihid->flags))
			wake_up(&ihid->wait);
		return;
	}

	if (ihid->quirks & I2C_HID_QUIRK_BOGUS_IRQ && ret_size == 0xffff) {
		dev_warn_once(&ihid->client->dev, "%s: IRQ triggered but "
			      "there's no data\n", __func__);
		return;
	}

	if ((ret_size > size) || (ret_size < 2)) {
		if (ihid->quirks & I2C_HID_QUIRK_BAD_INPUT_SIZE) {
			ihid->inbuf[0] = size & 0xff;
			ihid->inbuf[1] = size >> 8;
			ret_size = size;
		} else {
			dev_err(&ihid->client->dev, "%s: incomplete report (%d/%d)\n",
				__func__, size, ret_size);
			return;
		}
	}

	if (KB_KEYS != ihid->inbuf[2] && KB_SPECIAL_KEYS != ihid->inbuf[2]) {
		i2c_hid_dbg(ihid, "%s: queue_work,inreport_parse_work , enter!\n", __func__);
		queue_work(ihid->hid_queuework, &ihid->inreport_parse_work);
	}

	if (test_bit(I2C_HID_STARTED, &ihid->flags))
		hid_input_report(ihid->hid, HID_INPUT_REPORT, ihid->inbuf + 2,
				ret_size - 2, 1);

	return;
}

static irqreturn_t i2c_hid_irq(int irq, void *dev_id)
{
	struct i2c_hid *ihid = dev_id;

	i2c_hid_dbg(ihid, "%s:enter\n", __func__);

	if (test_bit(I2C_HID_READ_PENDING, &ihid->flags))
		return IRQ_HANDLED;

	i2c_hid_dbg(ihid, "%s:IRQ UNHANDLED\n", __func__);

	i2c_hid_get_input(ihid);

	return IRQ_HANDLED;
}
//-P86801AA1-1797, caoxin2.wt, modify, 2023.07.18, KB bringup

static int i2c_hid_init_irq(struct i2c_client *client)
{
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	unsigned long irqflags = 0;
	int ret;

	dev_dbg(&client->dev, "Requesting IRQ: %d\n", client->irq);

	if (!irq_get_trigger_type(client->irq))
		irqflags = IRQF_TRIGGER_LOW;

	ret = request_threaded_irq(client->irq, NULL, i2c_hid_irq,
				   irqflags | IRQF_ONESHOT, client->name, ihid);
	if (ret < 0) {
		dev_warn(&client->dev,
			"Could not register for %s interrupt, irq = %d,"
			" ret = %d\n",
			client->name, client->irq, ret);

		return ret;
	}

	return 0;
}

//+P86801AA1-1797, caoxin2.wt, add, 2023.07.31, KB bringup
static int kbfw_update(struct i2c_hid *ihid)
{
	int ret;
	u8 reportID;

	ret = 0;
	reportID = ihid->fwbuf[0];
	if (KB_RESPONSE == reportID)
		ret = input_kb_response_parse(ihid);
	return ret;
}

static void kbfw_update_func(struct work_struct *work)
{
	struct i2c_hid *fw_ud_hid = container_of(work, struct i2c_hid, kbfw_update_work);
	kbfw_update(fw_ud_hid);
}

static void i2c_hid_get_fw_update_input(struct i2c_hid *ihid)
{
	int ret;
	u32 ret_size = 10;
	int size = le16_to_cpu(ihid->hdesc.wMaxInputLength);

	i2c_hid_dbg(ihid, "%s:enter\n", __func__);

	if (!kbd_connect) {
		dev_err(&ihid->client->dev, "%s:Firmware upgrade interrupt triggered, but no keyboard access\n", __func__);
		return;
	}

	if (size > ihid->bufsize)
		size = ihid->bufsize;

	ret = i2c_hid_raw_request(ihid->hid, KB_RESPONSE,ihid->fwbuf,10,HID_FEATURE_REPORT,HID_REQ_GET_REPORT);
	if (ret < 0)
		dev_err(&ihid->client->dev, "%s:i2c_hid_raw_request fail\n");

	i2c_hid_dbg(ihid, "%s:input: %*ph\n", __func__, ret_size, ihid->fwbuf);

	if (ret_size > size) {
		if (ihid->quirks & I2C_HID_QUIRK_BAD_INPUT_SIZE) {
			ihid->fwbuf[0] = size & 0xff;
			ihid->fwbuf[1] = size >> 8;
			ret_size = size;
		} else {
			dev_err(&ihid->client->dev, "%s: incomplete report (%d/%d)\n",
				__func__, size, ret_size);
			return;
		}
	}

	queue_work(ihid->hid_queuework, &ihid->kbfw_update_work);

	return;
}

static irqreturn_t i2c_hid_fw_ud_irq(int irq, void *dev_id)
{
	struct i2c_hid *ihid = dev_id;

	if (test_bit(I2C_HID_READ_PENDING, &ihid->flags))
		return IRQ_HANDLED;

	i2c_hid_get_fw_update_input(ihid);

	return IRQ_HANDLED;
}

static int fw_irq_init(struct i2c_client *client)
{
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	int ret;

	dev_dbg(&client->dev, "Requesting fw update IRQ: %d\n", client->irq);

	if (!irq_get_trigger_type(ihid->kb_fw_irq))
		kb_fw_gpio.hid_fw_gpio_flags = IRQF_TRIGGER_LOW;

	ihid->kb_fw_irq = gpio_to_irq(kb_fw_gpio.hid_fw_gpio);

	ret = request_threaded_irq(ihid->kb_fw_irq, NULL, i2c_hid_fw_ud_irq,
				   kb_fw_gpio.hid_fw_gpio_flags | IRQF_ONESHOT, client->name, ihid);
	if (ret < 0) {
		dev_warn(&client->dev,
			"Could not register for kb update interrupt, irq = %d,"
			" ret = %d\n",
			ihid->kb_fw_irq, ret);

		return ret;
	}

	return 0;
}

static int i2c_hid_parse_irq(struct i2c_client *client)
{
	struct device *dev = &client->dev;

	kb_fw_gpio.hid_fw_gpio = of_get_named_gpio_flags(dev->of_node, "hid-fw-gpio",
			0, &kb_fw_gpio.hid_fw_gpio_flags);

	if (kb_fw_gpio.hid_fw_gpio < 0) {
		dev_err(&client->dev,"Unable to get hid_fw_gpio");
		return -EINVAL;
	}
	dev_err(&client->dev,"%s:get hid_fw_gpio success", __func__);

	return 0;
}
//-P86801AA1-1797, caoxin2.wt, add, 2023.07.31, KB bringup

static int kb_pinctrl_select_reset_suspend(struct i2c_hid *ihid);
static int kb_pinctrl_select_reset_active(struct i2c_hid *ihid);
static int padmcu_fw_update(struct i2c_hid *ihid, u16 current_ver);

static int i2c_hid_fetch_hid_descriptor(struct i2c_hid *ihid)
{
	struct i2c_client *client = ihid->client;
	struct i2c_hid_desc *hdesc = &ihid->hdesc;
	unsigned int dsize;
	int ret;

	/* i2c hid fetch using a fixed descriptor size (30 bytes) */
	if (i2c_hid_get_dmi_i2c_hid_desc_override(client->name)) {
		i2c_hid_dbg(ihid, "Using a HID descriptor override\n");
		ihid->hdesc =
			*i2c_hid_get_dmi_i2c_hid_desc_override(client->name);
	} else {
		dev_err(&client->dev, "Fetching the HID descriptor\n"); //P86801AA1-1797, caoxin2.wt, add, 2023.05.25, KB bringup
		ret = i2c_hid_command(client, &hid_descr_cmd,
				      ihid->hdesc_buffer,
				      sizeof(struct i2c_hid_desc));

		if (strncmp(Mcu_name, "NULL", sizeof("NULL")) == 0) {

			//mcu_update
			ret = padmcu_fw_update(ihid, 0);
			if (ret < 0) {
				dev_err(&client->dev, "Mcu NULL,fw update fail,ret = %d\n", ret);
				return ret;
			}

			dev_err(&client->dev, "Fetching the HID descriptor again\n");
			ret = i2c_hid_command(client, &hid_descr_cmd,
				      ihid->hdesc_buffer,
				      sizeof(struct i2c_hid_desc));
		}

		if (ret) {
			dev_err(&client->dev, "hid_descr_cmd failed\n");
			return -ENODEV;
		}
	}

	/* Validate the length of HID descriptor, the 4 first bytes:
	 * bytes 0-1 -> length
	 * bytes 2-3 -> bcdVersion (has to be 1.00) */
	/* check bcdVersion == 1.0 */
	if (le16_to_cpu(hdesc->bcdVersion) != 0x0100) {
		dev_err(&client->dev,
			"unexpected HID descriptor bcdVersion (0x%04hx)\n",
			le16_to_cpu(hdesc->bcdVersion));
		return -ENODEV;
	}

	/* Descriptor length should be 30 bytes as per the specification */
	dsize = le16_to_cpu(hdesc->wHIDDescLength);
	if (dsize != sizeof(struct i2c_hid_desc)) {
		dev_err(&client->dev, "weird size of HID descriptor (%u)\n",
			dsize);
		return -ENODEV;
	}
	i2c_hid_dbg(ihid, "HID Descriptor: %*ph\n", dsize, ihid->hdesc_buffer);
	return 0;
}

#ifdef CONFIG_ACPI
static const struct acpi_device_id i2c_hid_acpi_blacklist[] = {
	/*
	 * The CHPN0001 ACPI device, which is used to describe the Chipone
	 * ICN8505 controller, has a _CID of PNP0C50 but is not HID compatible.
	 */
	{"CHPN0001", 0 },
	{ },
};

static int i2c_hid_acpi_pdata(struct i2c_client *client,
		struct i2c_hid_platform_data *pdata)
{
	static guid_t i2c_hid_guid =
		GUID_INIT(0x3CDFF6F7, 0x4267, 0x4555,
			  0xAD, 0x05, 0xB3, 0x0A, 0x3D, 0x89, 0x38, 0xDE);
	union acpi_object *obj;
	struct acpi_device *adev;
	acpi_handle handle;

	handle = ACPI_HANDLE(&client->dev);
	if (!handle || acpi_bus_get_device(handle, &adev)) {
		dev_err(&client->dev, "Error could not get ACPI device\n");
		return -ENODEV;
	}

	if (acpi_match_device_ids(adev, i2c_hid_acpi_blacklist) == 0)
		return -ENODEV;

	obj = acpi_evaluate_dsm_typed(handle, &i2c_hid_guid, 1, 1, NULL,
				      ACPI_TYPE_INTEGER);
	if (!obj) {
		dev_err(&client->dev, "Error _DSM call to get HID descriptor address failed\n");
		return -ENODEV;
	}

	pdata->hid_descriptor_address = obj->integer.value;
	ACPI_FREE(obj);

	return 0;
}

static void i2c_hid_acpi_fix_up_power(struct device *dev)
{
	struct acpi_device *adev;

	adev = ACPI_COMPANION(dev);
	if (adev)
		acpi_device_fix_up_power(adev);
}

static const struct acpi_device_id i2c_hid_acpi_match[] = {
	{"ACPI0C50", 0 },
	{"PNP0C50", 0 },
	{ },
};
MODULE_DEVICE_TABLE(acpi, i2c_hid_acpi_match);
#else
static inline int i2c_hid_acpi_pdata(struct i2c_client *client,
		struct i2c_hid_platform_data *pdata)
{
	return -ENODEV;
}

static inline void i2c_hid_acpi_fix_up_power(struct device *dev) {}
#endif

#ifdef CONFIG_OF
static int i2c_hid_of_probe(struct i2c_client *client,
		struct i2c_hid_platform_data *pdata)
{
	struct device *dev = &client->dev;
	u32 val;
	int ret;

	ret = of_property_read_u32(dev->of_node, "hid-descr-addr", &val);
	if (ret) {
		dev_err(&client->dev, "HID register address not provided\n");
		return -ENODEV;
	}
	if (val >> 16) {
		dev_err(&client->dev, "Bad HID register address: 0x%08x\n",
			val);
		return -EINVAL;
	}
	pdata->hid_descriptor_address = val;

	return 0;
}

static const struct of_device_id i2c_hid_of_match[] = {
	{ .compatible = "hid-over-i2c" },
	{},
};
MODULE_DEVICE_TABLE(of, i2c_hid_of_match);
#else
static inline int i2c_hid_of_probe(struct i2c_client *client,
		struct i2c_hid_platform_data *pdata)
{
	return -ENODEV;
}
#endif

static void i2c_hid_fwnode_probe(struct i2c_client *client,
				 struct i2c_hid_platform_data *pdata)
{
	u32 val;

	if (!device_property_read_u32(&client->dev, "post-power-on-delay-ms",
				      &val))
		pdata->post_power_delay_ms = val;
}

//+P86801AA1-1797, caoxin2.wt, add, 2023.06.09, KB bringup
u32 crc_compute_crc32_mpge(u32 init_value, u8 *buf, size_t buf_len)
{
	size_t i;
	u32 CRC32_data = init_value;

	for (i = 0; i < buf_len; i++)
	{
		CRC32_data = crc32_MPEG_2_table[((CRC32_data >> 24) ^ buf[i]) & 0xFF] ^ (CRC32_data << 8);
	}
	return (u32)(CRC32_data ^ CRC32_XOROUT);
}

static int hid_fw_update_cmd_send_recv(struct i2c_client *client,
		u16 addr, u8 *send_buf, u32 send_buf_len,
		u8 *recv_buf, u32 recv_buf_len)
{
	int ret;
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	struct i2c_msg msg[2];
	int send_msg_num = 1;

	i2c_hid_dbg(ihid, "%s: cmd=%*ph\n", __func__, send_buf_len, send_buf);
	msg[0].addr = addr;
	msg[0].flags = client->flags & I2C_M_TEN;
	msg[0].len = send_buf_len;
	msg[0].buf = send_buf;
	if (recv_buf_len > 0) {
		msg[1].addr = addr;
		msg[1].flags = client->flags & I2C_M_TEN;
		msg[1].flags |= I2C_M_RD;
		msg[1].len = recv_buf_len;
		msg[1].buf = recv_buf;
		send_msg_num = 2;
		set_bit(I2C_HID_READ_PENDING, &ihid->flags);
	}

	ret = i2c_transfer(client->adapter, msg, send_msg_num);

	if (recv_buf_len > 0)
		clear_bit(I2C_HID_READ_PENDING, &ihid->flags);

	if (ret != send_msg_num) {
		dev_err(&client->dev, "%s: fail\n", __func__);
		return ret < 0 ? ret : -EIO;
	}

	return 0;
}

static int upgrade_cmd_send_and_recv(struct i2c_hid *ihid)
{
	struct i2c_client *client = ihid->client;
	u8 upgrade_cmd[5] = {0x01,0xAA,0x01,0xF0,0xF1};
	u8 recv_buf[5] = {0};
	u8 send_mask_cmd[1] = {0x01};
	int ret;

	ret = hid_fw_update_cmd_send_recv(client, client->addr, upgrade_cmd, 5, NULL, 0);
	if (ret) {
		dev_err(&client->dev, "send upgrade_cmd fail\n");
		return ret;
	}
	msleep(5);

	ret = hid_fw_update_cmd_send_recv(client, client->addr, send_mask_cmd, 1, recv_buf, 5);
	if (recv_buf[3] != 0x08) {
		dev_err(&client->dev, "enter boot fail, recv_buf[3] = %d\n",recv_buf[3]);
		return ret < 0 ? ret : -EIO;
	} else {
		dev_err(&client->dev, "enter boot success, recv_buf[3] = %d\n",recv_buf[3]);
	}

	return 0;
}

static int read_kpdmcu_fw_version(struct i2c_hid *ihid)
{
	struct i2c_client *client = ihid->client;
	u8 version_cmd[5] = {0x01,0xAA,0x01,0xF5,0xF6};
	u8 recv_buf[9] = {0};
	u8 send_mask_cmd[1] = {0x01};
	int ret;

	ret = hid_fw_update_cmd_send_recv(client, client->addr, version_cmd, 5, NULL, 0);
	if (ret) {
		dev_err(&client->dev, "send version_cmd fail\n");
		return ret;
	}
	msleep(5);
	ret = hid_fw_update_cmd_send_recv(client, client->addr, send_mask_cmd, 1, recv_buf, 9);
	if (recv_buf[3] != 0x08) {
		dev_err(&client->dev, "read firmware verison fail\n");
		return ret < 0 ? ret : -EIO;
	}
	ihid->kpdmcu_fw_mcu_version = (((recv_buf[7]<<24)&0xFF000000)|((recv_buf[6]<<16)&0xFF0000)|((recv_buf[5]<<8)&0xFF00)|(recv_buf[4]));
	dev_err(&client->dev, "fw version is %d\n", ihid->kpdmcu_fw_mcu_version);

	return 0;
}

static int erase_cmd_send_and_recv(struct i2c_hid *ihid, int erase_page)
{
	struct i2c_client *client = ihid->client;
	u8 erase_cmd[11] = {0x01,0xAA,0x07,0xF1,0x00,0x0C,0x00,0x08,0x02,0x00,0x0E}; //erase page
	u8 recv_buf[5] = {0};
	u8 send_mask_cmd[1] = {0x01};
	int ret,i;

	erase_cmd[8] = erase_page & 0x00ff;

	//checksum
	erase_cmd[10] = 0;
	for (i = 2; i < 10; i++) {
		erase_cmd[10] += (erase_cmd[i] & 0x000000ff);
	}

	ret = hid_fw_update_cmd_send_recv(client, client->addr, erase_cmd, 11, NULL, 0);
	msleep(erase_page * 8);
	ret = hid_fw_update_cmd_send_recv(client, client->addr, send_mask_cmd, 1, recv_buf, 5);
	if (recv_buf[3] != 0x08) {
		dev_err(&client->dev, "erase fail recv_buf[3] = %d\n",recv_buf[3]);
	} else {
		dev_err(&client->dev, "erase ok recv_buf[3] = %d\n",recv_buf[3]);
	}
	return 0;
}

static int program_cmd_send_and_recv(struct i2c_hid *ihid, unsigned int fw_data_num, const unsigned char *fw_data)
{
	struct i2c_client *client = ihid->client;
	u8 program_cmd[10] = {0x01,0xAA,0x87,0xF2,0x00,0x0C,0x00,0x08,0x80,0x00}; //write data
	u8 CRC_cmd[9] = {0x01,0xAA,0x05,0xF3,0x79,0x04,0xB2,0x5B,0x82}; //CRC check
	u8 recv_buf[5] = {0};
	u8 send_mask_cmd[1] = {0x01};
	int ret,i,page_num,data_num;
	int fw_offset = 0; //The length of data written in
	int write_len = 0;
	int data_n = 0;
	u32 crc = CRC32_InitalValue;
	u8* send_rx = (u8*)kzalloc(ONE_WRITY_LEN_MIN+11, GFP_KERNEL);
	u8* CRC_rx = (u8*)kzalloc(ONE_WRITY_LEN_MIN, GFP_KERNEL);

	if (fw_data_num % PAGE)
	{
		page_num = fw_data_num / PAGE + 1;
		data_num = page_num * PAGE;
	} else {
		data_num = fw_data_num;
	}

	while (data_num) {

		send_rx[ONE_WRITY_LEN_MIN+10] = 0;
		program_cmd[4] = (0x08000c00 + fw_offset) & 0x000000ff;
		program_cmd[5] = (((0x08000c00 + fw_offset) & 0x0000ff00) >> 8);
		program_cmd[6] = (((0x08000c00 + fw_offset) & 0x00ff0000) >> 16);
		program_cmd[7] = (((0x08000c00 + fw_offset) & 0xff000000) >> 24);

		//head 10
		for (i = 0; i < CMD_HEAD_LEN; i++) {
			send_rx[i] = program_cmd[i];
		}

		write_len = (fw_data_num < ONE_WRITY_LEN_MIN) ? fw_data_num : ONE_WRITY_LEN_MIN;
		for(i = 0; i < write_len;i++) {
			send_rx[CMD_HEAD_LEN + i] = fw_data[fw_offset + i];
			CRC_rx[i] = fw_data[fw_offset + i];
		}

		if(fw_data_num < ONE_WRITY_LEN_MIN) {
			for(i = 0; i < ONE_WRITY_LEN_MIN - fw_data_num;i++) {
				send_rx[ONE_WRITY_LEN_MIN + CMD_HEAD_LEN- i - 1] = 0xff;
				CRC_rx[ONE_WRITY_LEN_MIN - i - 1] = 0xff;
			}
		}

		//check_sum
		for (i = 2; i < ONE_WRITY_LEN_MIN + 10; i++) {
			send_rx[ONE_WRITY_LEN_MIN + 10] = (send_rx[ONE_WRITY_LEN_MIN + 10] + send_rx[i]) & 0x000000ff;
		}

		//CRC
		crc = crc_compute_crc32_mpge(crc, CRC_rx, ONE_WRITY_LEN_MIN);

		//update
		fw_offset += ONE_WRITY_LEN_MIN;
		fw_data_num -= write_len;
		data_num -= ONE_WRITY_LEN_MIN;
		data_n += 1;

		ret = hid_fw_update_cmd_send_recv(client, client->addr, send_rx, ONE_WRITY_LEN_MIN + 11, NULL, 0);
 		if(ret){
			dev_err(&client->dev,"download ,send program_cmd fail, data %d\n", data_n);
			kfree(send_rx);
			send_rx = NULL;
			return ret;
		}

		msleep(5);

		ret = hid_fw_update_cmd_send_recv(client, client->addr, send_mask_cmd, 1, recv_buf, 5);
 		if(ret || (recv_buf[3] != 0x08)) {
			dev_err(&client->dev,"read fail,recv_buf = %d\n", recv_buf[3]);
			kfree(send_rx);
			send_rx = NULL;
			return ret < 0;
		}
	}
	dev_err(&client->dev,"send upgrade data success\n");
	kfree(send_rx);
	kfree(CRC_rx);
	send_rx = NULL;
	CRC_rx = NULL;

	msleep(5);

	//CRC check fw
	CRC_cmd[8] = 0;
	CRC_cmd[4] = crc & 0x000000ff;
	CRC_cmd[5] = (crc & 0x0000ff00) >> 8;
	CRC_cmd[6] = (crc & 0x00ff0000) >> 16;
	CRC_cmd[7] = (crc & 0xff000000) >> 24;
	for (i =2; i < 8; i++)
	{
		CRC_cmd[8] += CRC_cmd[i];
	}
	CRC_cmd[8] &= 0x000000ff;
	dev_err(&client->dev, "CRC = %u\n",crc);

	ret = hid_fw_update_cmd_send_recv(client, client->addr, CRC_cmd, 9, NULL, 0);
	if (ret) {
		dev_err(&client->dev, "CRC,send CRC_cmd fail\n");
		return ret;
	} else {
		dev_err(&client->dev,"CRC,send CRC_cmd success\n");
	 }

	msleep(page_num * 8);

	ret = hid_fw_update_cmd_send_recv(client, client->addr, send_mask_cmd, 1, recv_buf, 5);
	if(ret || (recv_buf[3] != 0x08)) {
		dev_err(&client->dev,"read fail,recv_buf = %d\n", recv_buf[3]);
		return ret;
	}
	return 0;
}
//jump
static int jump_cmd_send_and_recv(struct i2c_hid *ihid)
{
	struct i2c_client *client = ihid->client;
	u8 jump_cmd[5] = {0x01,0xAA,0x01,0xF4,0xF5}; //jump
	int ret;

	ret = hid_fw_update_cmd_send_recv(client, client->addr, jump_cmd, 5, NULL, 0);
	if (ret < 0) {
		dev_err(&client->dev,"jump APP fail\n");
		return ret;
	}
	return 0;
}

//+P86801AA1-1797, caoxin2.wt, modify, 2023.07.07, KB bringup
static int padmcu_fw_update(struct i2c_hid *ihid, u16 current_ver)
{
	struct i2c_client *client = ihid->client;
	const unsigned char *firmware_data;
	char *fw_path = "CS32Fxx_pad.bin";
	unsigned int fw_data_count,data_count;
	u16 pad_mcu_new_ver;
	int ret = 0;
	int page_num;

	ret = request_firmware(&fw_entry, fw_path, &client->dev);
	if (ret != 0) {
		dev_err(&client->dev, "request_firmware fail\n");
		return ret;
	} else {
		dev_err(&client->dev, "request_firmware success\n");
	}

	fw_data_count = (u32)fw_entry->size;
	firmware_data = fw_entry->data;

	pad_mcu_new_ver = (firmware_data[1048] & 0xFF) | ((firmware_data[1049] << 8) & 0xFF00);
	i2c_hid_dbg(ihid, "current ver = %d , new ver = %d\n", current_ver, pad_mcu_new_ver);
	if ((pad_mcu_new_ver > current_ver) | (current_ver > 0xAA00)) {
		dev_err(&client->dev, "%s:Start padmcu firmware upgrade\n", __func__);
		kb_pinctrl_select_reset_suspend(ihid);
		msleep(200);
		kb_pinctrl_select_reset_active(ihid);
		msleep(50);
	} else {
		dev_err(&client->dev, "%s:No pad mcu firmware upgrade required, current ver %x >= new ver %x\n", __func__, current_ver, pad_mcu_new_ver);
		return ret;
	}

	//-P86801AA1-1797, caoxin2.wt, modify, 2023.07.07, KB bringup

	if (0 != fw_data_count % PAGE)
	{
		page_num = fw_data_count / PAGE + 1;
		data_count = page_num * PAGE;
	}

	ret = upgrade_cmd_send_and_recv(ihid);
	if (ret) {
		dev_err(&client->dev, "enter boot fail\n");
		return ret;
	}
	msleep(20);

	ret = read_kpdmcu_fw_version(ihid);
	if (ret) {
		dev_err(&client->dev, "read version fail\n");
		return ret;
	}
	ret = erase_cmd_send_and_recv(ihid,page_num);
	if (ret) {
		dev_err(&client->dev, "erase fail\n");
		return ret;
	}

	ret = program_cmd_send_and_recv(ihid,fw_data_count,firmware_data);
	if (ret) {
		dev_err(&client->dev, "write fail\n");
		return ret;
	}
	msleep(5);

	ret = jump_cmd_send_and_recv(ihid);
	if (ret) {
		dev_err(&client->dev, "jump version fail\n");
		return ret;
	}
	msleep(250);

	snprintf(pad_mcu_cur_ver, HARDWARE_MAX_ITEM_LONGTH, "%02X %02X", firmware_data[1049], firmware_data[1048]);

	return 0;
}
static int kb_pinctrl_init(struct i2c_hid *ihid)
{
	struct i2c_client *client = ihid->client;
	int ret = 0;

	dev_err(&client->dev,"%s:init pinctrl \n", __func__);
	ihid->pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR_OR_NULL(ihid->pinctrl)) {
		dev_err(&client->dev,"%s:Failed to get pinctrl, please check dts\n", __func__);
		ret = PTR_ERR(ihid->pinctrl);
		goto err_pinctrl_get;
	} else {
		dev_err(&client->dev,"%s:get pinctrl success\n", __func__);
	}

	dev_err(&client->dev,"%s:pinctrl_lookup_state \n", __func__);
	ihid->pins_reset_active = pinctrl_lookup_state(ihid->pinctrl, "mcu_reset_active");
	if (IS_ERR_OR_NULL(ihid->pins_reset_active)) {
		dev_err(&client->dev,"MCU reset pin state[active->low] not found");
		ret = PTR_ERR(ihid->pins_reset_active);
		goto err_pinctrl_lookup;
	}
	ihid->pins_reset_suspend = pinctrl_lookup_state(ihid->pinctrl, "mcu_reset_suspend");
	if (IS_ERR_OR_NULL(ihid->pins_reset_suspend)) {
		dev_err(&client->dev,"MCU reset pin state[suspend->high] not found");
		ret = PTR_ERR(ihid->pins_reset_suspend);
		goto err_pinctrl_lookup;
	}
	ihid->pins_sleep_active = pinctrl_lookup_state(ihid->pinctrl, "mcu_sleep_active");
	if (IS_ERR_OR_NULL(ihid->pins_sleep_active)) {
		dev_err(&client->dev,"MCU sleep pin state[active->low] not found");
		ret = PTR_ERR(ihid->pins_sleep_active);
		goto err_pinctrl_lookup;
	}
	ihid->pins_sleep_suspend = pinctrl_lookup_state(ihid->pinctrl, "mcu_sleep_suspend");
	if (IS_ERR_OR_NULL(ihid->pins_sleep_suspend)) {
		dev_err(&client->dev,"MCU sleep pin state[suspend->high] not found");
		ret = PTR_ERR(ihid->pins_sleep_suspend);
		goto err_pinctrl_lookup;
	}
	dev_err(&client->dev,"%s:init over \n", __func__);

	return 0;
	err_pinctrl_lookup:
		if (ihid->pinctrl) {
			devm_pinctrl_put(ihid->pinctrl);
		}
	err_pinctrl_get:
		ihid->pinctrl = NULL;
		ihid->pins_reset_active = NULL;
		ihid->pins_reset_suspend = NULL;
		ihid->pins_sleep_active = NULL;
		ihid->pins_sleep_suspend = NULL;
		return ret;
}

static int kb_pinctrl_select_reset_active(struct i2c_hid *ihid)
{
	int ret = 0;
	struct i2c_client *client = ihid->client;
	if (ihid->pinctrl && ihid->pins_reset_active) {
		ret = pinctrl_select_state(ihid->pinctrl, ihid->pins_reset_active);
		if (ret < 0) {
			dev_err(&client->dev,"Set MCU reset active pin state error:%d", ret);
		}
	}
	return ret;
}

static int kb_pinctrl_select_reset_suspend(struct i2c_hid *ihid)
{
	int ret = 0;
	struct i2c_client *client = ihid->client;
	if (ihid->pinctrl && ihid->pins_reset_suspend) {
		ret = pinctrl_select_state(ihid->pinctrl, ihid->pins_reset_suspend);
		if (ret < 0) {
			dev_err(&client->dev,"Set MCU reset suspend pin state error:%d", ret);
		}
	}
	return ret;
}

//-P86801AA1-1797, caoxin2.wt, add, 2023.06.09, KB bringup

static int i2c_hid_probe(struct i2c_client *client,
			 const struct i2c_device_id *dev_id)
{
	int ret;
	struct i2c_hid *ihid;
	struct hid_device *hid;
	__u16 hidRegister;
	struct i2c_hid_platform_data *platform_data = client->dev.platform_data;
	u16 pad_mcu_current_ver;
	char *wake_name = NULL;

	dbg_hid("HID probe called for i2c 0x%02x\n", client->addr);
	if (!client->irq) {
		dev_err(&client->dev,
			"HID over i2c has not been provided an Int IRQ\n");
		return -EINVAL;
	}

	if (client->irq < 0) {
		if (client->irq != -EPROBE_DEFER)
			dev_err(&client->dev,
				"HID over i2c doesn't have a valid IRQ\n");
		return client->irq;
	}

	ihid = devm_kzalloc(&client->dev, sizeof(*ihid), GFP_KERNEL);
	if (!ihid)
		return -ENOMEM;

	if (client->dev.of_node) {
		ret = i2c_hid_of_probe(client, &ihid->pdata);
		if (ret)
			return ret;

		ret = i2c_hid_parse_irq(client);
		if (ret)
			dev_err(&client->dev, "kb fw update gpio parse fail\n");
	} else if (!platform_data) {
		ret = i2c_hid_acpi_pdata(client, &ihid->pdata);
		if (ret)
			return ret;
	} else {
		ihid->pdata = *platform_data;
	}

	/* Parse platform agnostic common properties from ACPI / device tree */
	i2c_hid_fwnode_probe(client, &ihid->pdata);

	ihid->pdata.supplies[0].supply = "vdd";
	ihid->pdata.supplies[1].supply = "vddl";

	ret = devm_regulator_bulk_get(&client->dev,
				      ARRAY_SIZE(ihid->pdata.supplies),
				      ihid->pdata.supplies);
	if (ret)
		return ret;

	ret = regulator_bulk_enable(ARRAY_SIZE(ihid->pdata.supplies),
				    ihid->pdata.supplies);
	if (ret < 0)
		return ret;

	if (ihid->pdata.post_power_delay_ms)
		msleep(ihid->pdata.post_power_delay_ms);

	i2c_set_clientdata(client, ihid);

	ihid->client = client;
	//+P86801AA1-1797, caoxin2.wt, add, 2023.06.05, KB bringup
	kb_pinctrl_init(ihid);
	kb_pinctrl_select_reset_active(ihid);
	//-P86801AA1-1797, caoxin2.wt, add, 2023.06.05, KB bringup
	hidRegister = ihid->pdata.hid_descriptor_address;
	ihid->wHIDDescRegister = cpu_to_le16(hidRegister);

	init_waitqueue_head(&ihid->wait);
	mutex_init(&ihid->reset_lock);

	/* we need to allocate the command buffer without knowing the maximum
	 * size of the reports. Let's use HID_MIN_BUFFER_SIZE, then we do the
	 * real computation later. */
	ret = i2c_hid_alloc_buffers(ihid, HID_MIN_BUFFER_SIZE);
	if (ret < 0)
		goto err_regulator;

	i2c_hid_acpi_fix_up_power(&client->dev);

	device_init_wakeup(&client->dev,true);
	device_enable_async_suspend(&client->dev);

	/* Make sure there is something at this address */
	ret = i2c_smbus_read_byte(client);
	if (ret < 0) {
		dev_err(&client->dev, "nothing at this address: %d\n", ret);
		ret = -ENXIO;
		//goto err_regulator;
	}

	ret = i2c_hid_fetch_hid_descriptor(ihid);
	if (ret < 0)
		goto err_regulator;

	//+P86801AA1-1797, caoxin2.wt, add, 2023.07.20, KB bringup
	//work_queue
	ihid->hid_queuework = create_singlethread_workqueue("hid_wq");
	if (ihid->hid_queuework == NULL) {
		dev_err(&client->dev, "create i2c hid workqueue fail");
	}
	INIT_WORK(&ihid->inreport_parse_work, input_report_parse_func);
	INIT_WORK(&ihid->kbfw_update_work, kbfw_update_func);

	//mcu_update
	pad_mcu_current_ver = le16_to_cpu(ihid->hdesc.wVersionID);
	snprintf(pad_mcu_cur_ver, HARDWARE_MAX_ITEM_LONGTH, "%02X %02X", ((pad_mcu_current_ver >> 8) & 0xFF), (pad_mcu_current_ver & 0xFF));

	ret = padmcu_fw_update(ihid, pad_mcu_current_ver);
	if (fw_entry != NULL) {
		release_firmware(fw_entry);
		fw_entry = NULL;
		dev_err(&client->dev, "%s:release pad mcu firmware\n", __func__);
	}
	if (ret < 0)
		dev_err(&client->dev, "fw update fail,ret = %d\n", ret);
	else {
		dev_err(&client->dev, "pad mcu fw update success\n");
		ret = i2c_hid_fetch_hid_descriptor(ihid);
		if (ret < 0) {
			goto err_regulator;
		}
	}

	ret = i2c_hid_init_irq(client);
	if (ret < 0)
		goto err_regulator;

	ret = fw_irq_init(client);
	if (ret < 0)
		goto err_regulator;

	hid = hid_allocate_device();
	if (IS_ERR(hid)) {
		ret = PTR_ERR(hid);
		goto err_irq;
	}

	ihid->hid = hid;

	hid->driver_data = client;
	hid->ll_driver = &i2c_hid_ll_driver;
	hid->dev.parent = &client->dev;
	hid->bus = BUS_I2C;
	hid->version = le16_to_cpu(ihid->hdesc.bcdVersion);
	hid->vendor = le16_to_cpu(ihid->hdesc.wVendorID);
	hid->product = le16_to_cpu(ihid->hdesc.wProductID);

	snprintf(hid->name, sizeof(hid->name), "%s", "book cover keyboard(EF-DX211)");
	strlcpy(hid->phys, dev_name(&client->dev), sizeof(hid->phys));

	ihid->quirks = i2c_hid_lookup_quirk(hid->vendor, hid->product);

	ret = hid_add_device(hid);
	if (ret) {
		if (ret != -ENODEV)
			hid_err(client, "can't add hid device: %d\n", ret);
		goto err_mem_free;
	}

	pad_keyboard_sysfs_init(ihid);
	wake_name = devm_kasprintf(&hid->dev, GFP_KERNEL, "%s", "pogo fw wakelock");
	ihid->pogopin_wakelock = wakeup_source_register(NULL, wake_name);

	set_bit(I2C_HID_PROBE,&ihid->flags);

	i2c_hid_dbg(ihid, "%s:connect_event %d\n", __func__, connect_event);

	if (!kbd_connect) {
		pad_mcu_suspend(ihid);
	}

	//-P86801AA1-1797, caoxin2.wt, modify, 2023.07.20, KB bringup

	return 0;

err_mem_free:
	hid_destroy_device(hid);

err_irq:
	free_irq(client->irq, ihid);
	if (ihid->hid_queuework) {
		destroy_workqueue(ihid->hid_queuework);
	}

err_regulator:
	regulator_bulk_disable(ARRAY_SIZE(ihid->pdata.supplies),
			       ihid->pdata.supplies);
	i2c_hid_free_buffers(ihid);

	return ret;
}

static int i2c_hid_remove(struct i2c_client *client)
{
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	struct hid_device *hid;

	pad_keyboard_sysfs_deinit(ihid);

	if (kbfw_entry != NULL) {
		release_firmware(kbfw_entry);
		kbfw_entry = NULL;
		dev_err(&client->dev, "%s:release kb firmware\n", __func__);
	}

	hid = ihid->hid;
	hid_destroy_device(hid);

	free_irq(client->irq, ihid);

	if (ihid->bufsize)
		i2c_hid_free_buffers(ihid);

	if (ihid->hid_queuework) {
		destroy_workqueue(ihid->hid_queuework);
	}

	regulator_bulk_disable(ARRAY_SIZE(ihid->pdata.supplies),
			       ihid->pdata.supplies);

	return 0;
}

static void i2c_hid_shutdown(struct i2c_client *client)
{
	struct i2c_hid *ihid = i2c_get_clientdata(client);

	i2c_hid_set_power(client, I2C_HID_PWR_SLEEP);
	free_irq(client->irq, ihid);
}

#ifdef CONFIG_PM_SLEEP
static int i2c_hid_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	struct hid_device *hid = ihid->hid;
	int ret;
	int wake_status;

	if (hid->driver && hid->driver->suspend) {
		ret = hid->driver->suspend(hid, PMSG_SUSPEND);
		if (ret < 0)
			return ret;
	}

	/* Save some power */
	//+P86801AA1-1797, caoxin2.wt, add, 2023.06.09, KB bringup
	pad_mcu_suspend(ihid);
	//-P86801AA1-1797, caoxin2.wt, add, 2023.06.09, KB bringup
	i2c_hid_set_power(client, I2C_HID_PWR_SLEEP);

	//disable_irq(client->irq);

	if (device_may_wakeup(&client->dev)) {
		wake_status = enable_irq_wake(client->irq);
		if (!wake_status)
			ihid->irq_wake_enabled = true;
		else
			hid_warn(hid, "Failed to enable irq wake: %d\n",
				wake_status);
	} else {
		regulator_bulk_disable(ARRAY_SIZE(ihid->pdata.supplies),
				       ihid->pdata.supplies);
	}

	return 0;
}

static int i2c_hid_resume(struct device *dev)
{
	int ret;
	struct i2c_client *client = to_i2c_client(dev);
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	struct hid_device *hid = ihid->hid;
	int wake_status;

	if (!device_may_wakeup(&client->dev)) {
		ret = regulator_bulk_enable(ARRAY_SIZE(ihid->pdata.supplies),
					    ihid->pdata.supplies);
		if (ret)
			hid_warn(hid, "Failed to enable supplies: %d\n", ret);

		if (ihid->pdata.post_power_delay_ms)
			msleep(ihid->pdata.post_power_delay_ms);
	} else if (ihid->irq_wake_enabled) {
		wake_status = disable_irq_wake(client->irq);
		if (!wake_status)
			ihid->irq_wake_enabled = false;
		else
			hid_warn(hid, "Failed to disable irq wake: %d\n",
				wake_status);
	}

	if (kbd_connect) {
		pad_mcu_resume(ihid);
	}
	//enable_irq(client->irq);

	/* Instead of resetting device, simply powers the device on. This
	 * solves "incomplete reports" on Raydium devices 2386:3118 and
	 * 2386:4B33 and fixes various SIS touchscreens no longer sending
	 * data after a suspend/resume.
	 *
	 * However some ALPS touchpads generate IRQ storm without reset, so
	 * let's still reset them here.
	 */
	if (ihid->quirks & I2C_HID_QUIRK_RESET_ON_RESUME)
		ret = i2c_hid_hwreset(client);
	else
		ret = i2c_hid_set_power(client, I2C_HID_PWR_ON);

	if (ret)
		return ret;

	if (hid->driver && hid->driver->reset_resume) {
		ret = hid->driver->reset_resume(hid);
		return ret;
	}

	return 0;
}
#endif

static const struct dev_pm_ops i2c_hid_pm = {
	SET_SYSTEM_SLEEP_PM_OPS(i2c_hid_suspend, i2c_hid_resume)
};

static const struct i2c_device_id i2c_hid_id_table[] = {
	{ "hid", 0 },
	{ "hid-over-i2c", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, i2c_hid_id_table);


static struct i2c_driver i2c_hid_driver = {
	.driver = {
		.name	= "i2c_hid",
		.pm	= &i2c_hid_pm,
		.acpi_match_table = ACPI_PTR(i2c_hid_acpi_match),
		.of_match_table = of_match_ptr(i2c_hid_of_match),
	},

	.probe		= i2c_hid_probe,
	.remove		= i2c_hid_remove,
	.shutdown	= i2c_hid_shutdown,
	.id_table	= i2c_hid_id_table,
};

module_i2c_driver(i2c_hid_driver);

MODULE_DESCRIPTION("HID over I2C core driver");
MODULE_AUTHOR("Benjamin Tissoires <benjamin.tissoires@gmail.com>");
MODULE_LICENSE("GPL");
