
#ifndef __STM32_POGO_I2C_H__
#define __STM32_POGO_I2C_H__

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/errno.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/wakelock.h>
#ifdef CONFIG_MUIC_SUPPORT_KEYBOARDDOCK
#include <linux/muic/muic.h>
#include <linux/muic/muic_notifier.h>
#endif
#include <linux/firmware.h>
#include <linux/platform_device.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif
#ifdef CONFIG_SEC_SYSFS
#include <linux/sec_class.h>
#elif CONFIG_DRV_SAMSUNG
#include <linux/sec_class.h>
#endif
#include <linux/fpga/pogo_i2c_notifier.h>

#define STM32_DRV_DESC				"stm32 i2c I/O expander"
#define STM32_DRV_NAME				"stm32_pogo_i2c"

#define STM32_PRINT_INFO_DELAY			msecs_to_jiffies(30 * MSEC_PER_SEC)

#define STM32_FOTA_BIN_PATH			"/data/data/com.sec.android.app.applinker/files"
#define STM32_FW_SIZE				(100 * 1024)
#define STM32_MAGIC_WORD			"STM32L0"

#define STM32_TC_CRC_OK				0x55AA /* by zinitix touch controller */

#define STM32_MAX_EVENT_SIZE			100

#define INT_ENABLE				0
#define INT_DISABLE_NOSYNC			1
#define INT_DISABLE_SYNC			2

#define STM32_DEBUG_LEVEL_I2C_LOG		(1 << 0)
#define STM32_DEBUG_LEVEL_IRQ_DATA_LOG		(1 << 1)

/*
 * Definition about MCU commands
 */
#define STM32_CMD_GET_PROTOCOL_VERSION		0x00
#define STM32_CMD_GET_MODE			0x01
#define STM32_CMD_CHECK_VERSION			0x02
#define STM32_CMD_CHECK_CRC			0x03
#define STM32_CMD_GET_TARGET_FW_VERSION		0x04
#define STM32_CMD_GET_TARGET_FW_CRC32		0x05
#define STM32_CMD_ENTER_DFU_MODE		0x06
#define STM32_CMD_GET_PAGE_INDEX		0x11
#define STM32_CMD_GET_PAGE_SIZE			0x12
#define STM32_CMD_START_FW_UPGRADE		0x13
#define STM32_CMD_WRITE_PAGE			0x14
#define STM32_CMD_WRITE_LAST_PAGE		0x15
#define STM32_CMD_GO				0x16
#define STM32_CMD_ABORT				0x17
#define STM32_CMD_GET_TC_FW_VERSION		0x18
#define STM32_CMD_GET_TC_FW_CRC16		0x19
#define STM32_CMD_GET_TC_RESOLUTION		0x1A

#define STM32_CFG_PACKET_SIZE			128

#define STM32_FW_APP_CFG_OFFSET			0x00C0

enum STM32_TC_FW_UPDATE_RESULT {
	TC_FW_RES_FAIL = 0x0F,
	TC_FW_RES_OK = 0xF0,
};

enum STM32_MODE {
	MODE_APP = 1,
	MODE_DFU,
};

enum STM32_FW_PARAM {
	FW_UPDATE_MCU = 0,
	FW_UPDATE_TC,
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

struct stm32_tc_fw_version {
	u16 major_ver;
	u16 minor_ver;
	u16 data_ver;
} __packed;

struct stm32_dev {
	int					dev_irq;
	struct i2c_client			*client;
	struct mutex				dev_lock;
	struct mutex				irq_lock;
	struct mutex				i2c_lock;
	struct mutex				reset_lock;
	struct wake_lock			stm_wake_lock;
#ifdef CONFIG_PM_SLEEP
	struct completion			resume_done;
	bool					irq_wake;
#endif
	struct device				*sec_pogo;

	struct stm32_devicetree_data		*dtdata;
	struct pinctrl				*pinctrl;
	int					connect_state;
	volatile int				current_connect_state;
#ifdef CONFIG_MUIC_SUPPORT_KEYBOARDDOCK
	struct notifier_block			keyboard_nb;
#endif
	struct delayed_work			keyboard_work;
	struct delayed_work			check_ic_work;
	struct delayed_work			print_info_work;

	struct firmware				*fw;
	struct stm32_fw_header			*fw_header;
	struct stm32_fw_version			ic_fw_ver;
	u32					crc_of_ic;
	u32					crc_of_bin;

	struct firmware				*tc_fw;
	u16					tc_crc;
	struct stm32_tc_fw_version		tc_fw_ver_of_ic;
	struct stm32_tc_fw_version		tc_fw_ver_of_bin;

	bool					ic_ready;

	int					debug_level;
	u32					fpga_download_count;
	u32					reset_count;

	volatile bool				hall_closed;
};

struct stm32_devicetree_data {
	int gpio_int;
	int gpio_sda;
	int gpio_scl;
	struct regulator *vdd_vreg;
	const char *model_name;
};

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

#ifdef CONFIG_POGO_FPGA
extern void fpga_rstn_control(void);
extern void fpga_rstn_high(void);
extern void fpga_rstn_low(void);
extern bool is_fw_dl_completed(void);
extern void fpga_fw_download(void);
extern bool is_fpga_wakeup(void);
#endif
#endif /* __STM32_POGO_I2C_H__*/
