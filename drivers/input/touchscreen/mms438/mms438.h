#ifndef _LINUX_MMS438_TOUCH_H_
#define _LINUX_MMS438_TOUCH_H_

#define MMS_DEV_NAME "mms438"

extern struct class *sec_class;
/* Flag */
#define MMS_HAS_TOUCH_KEY		1
#define FLASH_VERBOSE_DEBUG		1

#define MAX_SECTION_NUM			3
#define MAX_FINGER_NUM			2
#define MAX_WIDTH				30
#define MAX_PRESSURE			255
#define MAX_LOG_LENGTH			128
#define FINGER_EVENT_SZ			6
#define ESD_DETECT_COUNT		10

/*
 * ISC_XFER_LEN	- ISC unit transfer length.
 * Give number of 2 power n, where  n is between 2 and 10
 * i.e. 4, 8, 16 ,,, 1024
 */
#define ISC_XFER_LEN			1024
#define MMS_FLASH_PAGE_SZ		1024
#define ISC_BLOCK_NUM			(MMS_FLASH_PAGE_SZ / ISC_XFER_LEN)

/* Registers */
#define MMS_MODE_CONTROL		0x01
#define MMS_THRESHOLD			0x05
#define MMS_TX_NUM				0x0B
#define MMS_RX_NUM				0x0C
#define MMS_KEY_NUM				0x0D
#define MMS_REF_TRACK_LEVEL		0x0E
#define MMS_EVENT_PKT_SZ		0x0F
#define MMS_EVENT_PKT			0x10
#define MMS_OPERATION_MODE		0x30
#define MMS_UNIVERSAL_CMD		0xA0
#define MMS_UNIVERSAL_RESULT_SZ	0xAE
#define MMS_UNIVERSAL_RESULT	0xAF
#define MMS_CMD_ENTER_ISC		0x5F
#define MMS_BL_VERSION			0xE1
#define MMS_CORE_VERSION		0xE2
#define MMS_CONFIG_VERSION		0xE3
#define MMS_POWER_CONTROL		0xB0
#define MMS_VENDOR_ID			0xC0
#define MMS_HW_ID				0xC2

/* Universal commands */
#define MMS_CMD_SET_LOG_MODE	0x20
#define MMS_UNIV_ENTER_TEST		0x40
#define MMS_UNIV_CM_DELTA		0x41
#define MMS_UNIV_CM_ABS			0x43
#define MMS_UNIV_CM_JITTER		0x45
#define MMS_UNIV_GET_DELTA_KEY	0x4A
#define MMS_UNIV_GET_ABS_KEY	0x4B
#define MMS_UNIV_GET_JITTER_KEY	0x4C
#define MMS_UNIV_EXIT_TEST		0x4F
#define MMS_UNIV_INTENSITY		0x70
#define MMS_UNIV_INTENSITY_KEY	0x71
#define MMS_UNIV_REFERENCE		0x72

/* Event types */
#define MMS_TEST_MODE			0X0C
#define MMS_NOTIFY_EVENT		0x0E
#define MMS_ERROR_EVENT			0x0F
#define MMS_INPUT_EVENT			0x10
#define MMS_TOUCH_KEY_EVENT		0x40

#define TSP_CMD_STR_LEN			32
#define TSP_CMD_RESULT_STR_LEN	512
#define TSP_CMD_PARAM_NUM		8
#define TSP_CMD_FULL_VER_LEN	9

enum {
	SYS_TXNUM	= 3,
	SYS_RXNUM,
	SYS_CLEAR,
	SYS_ENABLE,
	SYS_DISABLE,
	SYS_INTERRUPT,
	SYS_RESET,
};

enum {
	GET_RX_NUM	= 1,
	GET_TX_NUM,
	GET_EVENT_DATA,
};

enum {
	LOG_TYPE_U08	= 2,
	LOG_TYPE_S08,
	LOG_TYPE_U16,
	LOG_TYPE_S16,
	LOG_TYPE_U32	= 8,
	LOG_TYPE_S32,
};

enum {
	ISC_ADDR				= 0xD5,
	ISC_CMD_READ_STATUS		= 0xD9,
	ISC_CMD_READ			= 0x4000,
	ISC_CMD_EXIT			= 0x8200,
	ISC_CMD_PAGE_ERASE		= 0xC000,
	ISC_CMD_ERASE			= 0xC100,
	ISC_PAGE_ERASE_DONE		= 0x10000,
	ISC_PAGE_ERASE_ENTER	= 0x20000,
};

enum {
	CMD_STATUS_WAITING = 0,
	CMD_STATUS_RUNNING,
	CMD_STATUS_OK,
	CMD_STATUS_FAIL,
	CMD_STATUS_NOT_APPLICABLE,
};

enum {
	FW_FROM_BUILT_IN = 0,
	FW_FROM_UMS,
};

struct mms_fac_data {
	struct device		*fac_tsp_dev;
	struct list_head	cmd_list_head;
	u8					cmd_state;
	char				cmd[TSP_CMD_STR_LEN];
	char				cmd_buff[TSP_CMD_STR_LEN];
	int					cmd_param[TSP_CMD_PARAM_NUM];
	char				cmd_result[TSP_CMD_RESULT_STR_LEN];
	struct mutex		cmd_lock;
	bool				cmd_is_running;
	u8					rx_num;
	u8					tx_num;
#if MMS_HAS_TOUCH_KEY
	struct device		*fac_key_dev;
	u8					key_num;
#endif
	s16 *reference;
	s16 *delta;
	s16 *abs;
	s16 *intensity;
	int max;
	int min;
};

struct mms_platform_data {
	unsigned int	max_x;
	unsigned int	max_y;
	int		gpio_int;
	int				vdd_regulator_volt_1_8v;
	int				vdd_regulator_volt_3_0v;
	const char		*inkernel_fw_name;

};

struct mms_info {
	struct mms_platform_data	*pdata;
	struct i2c_client			*client;
	struct input_dev			*input_dev;
	char						phys[32];
	const struct firmware		*fw;
	int							irq;
	bool						enabled;
	bool						*finger_state;
	bool			noise_mode;
	bool			ta_status;
#if MMS_HAS_TOUCH_KEY
	u8							key_num;
#endif

	struct mms_fac_data			*fac_data;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend		early_suspend;
#endif
};

struct mms_bin_hdr {
	char	tag[8];
	u16		core_version;
	u16		section_num;
	u16		contains_full_binary;
	u16		reserved0;
	u32		binary_offset;
	u32		binary_length;
	u32		extention_offset;
	u32		reserved1;
} __attribute__ ((packed));

struct mms_fw_img {
	u16	type;
	u16	version;
	u16	start_page;
	u16	end_page;
	u32	offset;
	u32	length;
} __attribute__ ((packed));

struct isc_packet {
	u8	cmd;
	u32	addr;
	u8	data[0];
} __attribute__ ((packed));

#endif /* _LINUX_MMS134S_TOUCH_H_ */
