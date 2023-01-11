#ifndef __IST30XX_H__
#define __IST30XX_H__


#define IST30XX_DEV_NAME		"ist3038"

#define IST30XX_USE_KEY			(1)

/* I2C Transfer msg number */
#define WRITE_CMD_MSG_LEN		(1)
#define READ_CMD_MSG_LEN		(2)

#define IST30XX_ADDR_LEN			(4)
#define IST30XX_DATA_LEN			(4)

#define IST30XX_MAX_MT_FINGERS	(10)
#define IST30XX_MAX_KEYS			(5)


enum ist3038_commands {
	CMD_ENTER_UPDATE			= 0x02,
	CMD_EXIT_UPDATE			= 0x03,
	CMD_UPDATE_SENSOR			= 0x04,
	CMD_UPDATE_CONFIG			= 0x05,
	CMD_ENTER_REG_ACCESS		= 0x07,
	CMD_EXIT_REG_ACCESS		= 0x08,
	CMD_SET_NOISE_MODE		= 0x0A,
	CMD_START_SCAN			= 0x0B,
	CMD_ENTER_FW_UPDATE		= 0x0C,
	CMD_RUN_DEVICE			= 0x0D,
	CMD_EXEC_MEM_CODE		= 0x0E,
	CMD_SET_TEST_MODE			= 0x0F,

	CMD_CALIBRATE				= 0x11,
	CMD_USE_IDLE				= 0x12,
	CMD_USE_DEBUG			= 0x13,
	CMD_ZVALUE_MODE			= 0x15,
	CMD_SAME_POSITION			= 0x16,
	CMD_CHECK_CALIB			= 0x1A,
	CMD_SET_TEMPER_MODE		= 0x1B,
	CMD_USE_CORRECT_CP		= 0x1C,
	CMD_SET_REPORT_RATE		= 0x1D,
	CMD_SET_IDLE_TIME			= 0x1E,

	CMD_GET_COORD			= 0x20,

	CMD_GET_CHIP_ID			= 0x30,
	CMD_GET_FW_VER			= 0x31,
	CMD_GET_CHECKSUM			= 0x32,
	CMD_GET_LCD_RESOLUTION		= 0x33,
	CMD_GET_TSP_CHNUM1		= 0x34,
	CMD_GET_PARAM_VER			= 0x35,
	CMD_GET_SUB_VER			= 0x36,
	CMD_GET_CALIB_RESULT		= 0x37,
	CMD_GET_TSP_SWAP_INFO		= 0x38,
	CMD_GET_KEY_INFO1			= 0x39,
	CMD_GET_KEY_INFO2			= 0x3A,
	CMD_GET_KEY_INFO3			= 0x3B,
	CMD_GET_TSP_CHNUM2		= 0x3C,
	CMD_GET_TSP_DIRECTION		= 0x3D,
	CMD_GET_TSP_VENDOR		= 0x3E,

	CMD_GET_TSP_PANNEL_TYPE	= 0x40,
	CMD_GET_CHECKSUM_ALL		= 0x41,

	CMD_SET_GESTURE_MODE		= 0x50,

	CMD_DEFAULT				= 0xFF,
};

#define CALIB_MSG_MASK			(0xF0000FFF)
#define CALIB_MSG_VALID			(0x80000CAB)

#define IST30XX_INTR_STATUS		(0x00000C00)
#define CHECK_INTR_STATUS(n)		(((n & IST30XX_INTR_STATUS) == IST30XX_INTR_STATUS) ? 1 : 0)

/*
 * CMD : CMD_GET_COORD
 *
 *   1st  [31:24]   [23:21]   [20:16]   [15:12]   [11:10]   [9:0]
 *        Checksum  KeyCnt    KeyStatus FingerCnt Rsvd.     FingerStatus
 *   2nd  [31:28]   [27:24]   [23:12]   [11:0]
 *        ID        Area      X         Y
 */
#define PARSE_FINGER_CNT(n)		((n >> 12) & 0xF)
#define PARSE_KEY_CNT(n)			((n >> 21) & 0x7)
#define PARSE_FINGER_STATUS(n)	(n & 0x3FF)
#define PARSE_KEY_STATUS(n)		((n >> 16) & 0x1F)

#define PRESSED_STATUS(s, id)		(((s >> id) & 0x01) ? true : false)

struct finger_info {
	u32 y:12;
	u32 x:12;
	u32 w:4;
	u32 id:4;
};

struct finger_status {
	u16	x;
	u16	y;
	u8	w;
	bool	status;
};

struct ist3038_platform_data {
	int			max_x;
	int			max_y;
	int			max_w;
	int			irq_flag;
	int			avdd_volt;
	struct regulator	*avdd_regulator;
};

struct ist3038_data {
	struct i2c_client				*client;
	struct input_dev				*input_dev;
	struct ist3038_platform_data	*pdata;
	struct finger_status			fingers_status[IST30XX_MAX_MT_FINGERS];
	bool	keys_status[IST30XX_MAX_KEYS];
	bool	power_on;
};

#endif /* __IST30XX_H__ */

