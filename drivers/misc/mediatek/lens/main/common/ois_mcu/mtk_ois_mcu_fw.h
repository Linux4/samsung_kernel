#ifndef MTK_OIS_MCU_FW_H
#define MTK_OIS_MCU_FW_H

#define OIS_MCU_PINCTRL_PIN_BOOT        1
#define OIS_MCU_PINCTRL_PINSTATE_LOW    0
#define OIS_MCU_PINCTRL_PINSTATE_HIGH   1

#define OIS_MCU_PINCTRL_PIN_BOOT_HIGH      "cam0_ois_mcu_boot_swclk_1"
#define OIS_MCU_PINCTRL_PIN_BOOT_LOW       "cam0_ois_mcu_boot_swclk_0"
#define OIS_MCU_PINCTRL_PIN_NRST_HIGH      "cam0_ois_mcu_rst_1"
#define OIS_MCU_PINCTRL_PIN_NRST_LOW       "cam0_ois_mcu_rst_0"
#define OIS_MCU_PINCTRL_PIN_LDO_HIGH       "cam0_ois_mcu_ldo_en_1"
#define OIS_MCU_PINCTRL_PIN_LDO_LOW        "cam0_ois_mcu_ldo_en_0"
#define OIS_MCU_PINCTRL_PIN_OIS_VDD_HIGH   "cam0_ois_vdd_ldo_en_1"
#define OIS_MCU_PINCTRL_PIN_OIS_VDD_LOW    "cam0_ois_vdd_ldo_en_0"

#define MCU_PINCTRL_STATE_LOW           0
#define MCU_PINCTRL_STATE_HIGH          1

#define MCU_READ_I2C_SLAVE_ADDR         (0xC4)
#define MCU_WRITE_I2C_SLAVE_ADDR        (0xC5)

#define OIS_MCU_FW_PATH                 "/vendor/etc/ois_mcu_stm32_fw.bin"

#define OIS_FW_STATUS_OFFSET            (0x00FC)
#define OIS_FW_STATUS_SIZE              (4)
#define OIS_HW_VERSION_SIZE             (3)
#define OIS_MCU_VERSION_SIZE            (4)
#define OIS_MCU_VDRINFO_SIZE            (4)
#define OIS_HW_VERSION_OFFSET           (0xAFF1)
#define OIS_FW_VERSION_OFFSET           (0xAFED)
#define OIS_MCU_VERSION_OFFSET          (0x80F8)
#define OIS_MCU_VDRINFO_OFFSET          (0x807C)

#define OIS_CAMERA_HW_COMP              "mediatek,camera_hw"
#define OIS_FW_UPDATE_PACKET_SIZE       (256)
#define OIS_VER_SIZE                    (8)

#define BOOT_I2C_INTER_PKT_FRONT_INTVL  (1)
#define BOOT_I2C_INTER_PKT_BACK_INTVL   (1)

#define	BOOT_I2C_SYNC_RETRY_COUNT       (3)
#define	BOOT_I2C_SYNC_RETRY_INTVL       (50)

#define	BOOT_NRST_PULSE_INTVL           (2) /* msec */
#define BOOT_I2C_STARTUP_DELAY          (50) /* msec */

/* Commands and Response */
#define BOOT_I2C_CMD_GET                (0x00)
#define BOOT_I2C_CMD_GET_VER            (0x01)
#define BOOT_I2C_CMD_GET_ID             (0x02)
#define BOOT_I2C_CMD_READ               (0x11)
#define BOOT_I2C_CMD_GO                 (0x21)
#define BOOT_I2C_CMD_WRITE              (0x31)
#define BOOT_I2C_CMD_ERASE              (0x44)
#define BOOT_I2C_CMD_WRITE_UNPROTECT    (0x73)
#define BOOT_I2C_CMD_READ_UNPROTECT     (0x92)
#define BOOT_I2C_CMD_SYNC               (0xFF)

#define BOOT_I2C_RESP_ACK               (0x79)
#define BOOT_I2C_RESP_NACK              (0x1F)
#define BOOT_I2C_RESP_BUSY              (0x76)

#define BOOT_I2C_CMD_LEN                (1)
#define BOOT_I2C_ADDRESS_LEN            (4)
#define BOOT_I2C_NUM_READ_LEN           (1)
#define BOOT_I2C_NUM_WRITE_LEN          (1)
#define BOOT_I2C_NUM_ERASE_LEN          (2)
#define BOOT_I2C_CHECKSUM_LEN           (1)

#define BOOT_I2C_MAX_WRITE_LEN          (256)  /* Protocol limitation */
#define BOOT_I2C_MAX_ERASE_PARAM_LEN    (4096) /* In case of erase parameter with 2048 pages */
#define BOOT_I2C_MAX_PAYLOAD_LEN        (BOOT_I2C_MAX_ERASE_PARAM_LEN) /* Larger one between write and erase., */

#define BOOT_I2C_REQ_CMD_LEN            (BOOT_I2C_CMD_LEN + BOOT_I2C_CHECKSUM_LEN)
#define BOOT_I2C_REQ_ADDRESS_LEN        (BOOT_I2C_ADDRESS_LEN + BOOT_I2C_CHECKSUM_LEN)
#define BOOT_I2C_READ_PARAM_LEN         (BOOT_I2C_NUM_READ_LEN + BOOT_I2C_CHECKSUM_LEN)
#define BOOT_I2C_WRITE_PARAM_LEN(len)   (BOOT_I2C_NUM_WRITE_LEN + len + BOOT_I2C_CHECKSUM_LEN)
#define BOOT_I2C_ERASE_PARAM_LEN(len)   (len + BOOT_I2C_CHECKSUM_LEN)

#define BOOT_I2C_RESP_GET_VER_LEN       (0x01) /* bootloader version(1) */
#define BOOT_I2C_RESP_GET_ID_LEN        (0x03) /* number of bytes - 1(1) + product ID(2) */

#define	FLASH_PROG_TIME                 (37) /* per page or sector */
#define	FLASH_FULL_ERASE_TIME           (40 * 32) /* 2K erase time(40ms) * 32 pages */
#define	FLASH_PAGE_ERASE_TIME           (36) /* per page or sector */

#define BOOT_I2C_CMD_TMOUT              (30)
#define	BOOT_I2C_WRITE_TMOUT            (FLASH_PROG_TIME)
#define	BOOT_I2C_FULL_ERASE_TMOUT       (FLASH_FULL_ERASE_TIME)
#define	BOOT_I2C_PAGE_ERASE_TMOUT(n)    (FLASH_PAGE_ERASE_TIME * n)
#define BOOT_I2C_WAIT_RESP_TMOUT        (30)
#define BOOT_I2C_WAIT_RESP_POLL_TMOUT   (500)
#define BOOT_I2C_WAIT_RESP_POLL_INTVL   (3)
#define BOOT_I2C_WAIT_RESP_POLL_RETRY   (BOOT_I2C_WAIT_RESP_POLL_TMOUT / BOOT_I2C_WAIT_RESP_POLL_INTVL)
#define BOOT_I2C_XMIT_TMOUT(count)      (5 + (1 * count))
#define BOOT_I2C_RECV_TMOUT(count)      BOOT_I2C_XMIT_TMOUT(count)

#define OIS_FW_VER_BIN_ADDR_START       (0x80FB)

#ifndef NTOHL
#define NTOHL(x)                        ((((x) & 0xFF000000U) >> 24) | \
										 (((x) & 0x00FF0000U) >>  8) | \
										 (((x) & 0x0000FF00U) <<  8) | \
										 (((x) & 0x000000FFU) << 24))
#endif
#ifndef HTONL
#define HTONL(x)                        NTOHL(x)
#endif

#ifndef NTOHS

#define NTOHS(x)                        (((x >> 8) & 0x00FF) | ((x << 8) & 0xFF00))
#endif
#ifndef HTONS
#define HTONS(x)                        NTOHS(x)
#endif

struct sysboot_erase_param_type {
	u32 page;
	u32 count;
};

struct sysboot_page_type {
	uint32_t size;
	uint32_t count;
};

struct sysboot_map_type {
	uint32_t flashbase;  /* flash memory starting address */
	uint32_t sysboot;    /* system memory starting address */
	uint32_t optionbyte; /* option byte starting address */
	struct sysboot_page_type *pages;
};

struct mcu_info {
	struct device *pdev;
	char *name;
	uint32_t reset_ctrl_gpio;
	uint32_t boot0_ctrl_gpio;
	uint32_t ver;
	uint32_t id;
	char module_ver[OIS_VER_SIZE + 1];
	char phone_ver[OIS_VER_SIZE + 1];
	char cal_ver[OIS_VER_SIZE + 1];
	int fw_force_update;
	struct pinctrl *pinctrl;
	struct pinctrl_state *mcu_boot_high;
	struct pinctrl_state *mcu_boot_low;
	struct pinctrl_state *mcu_nrst_high;
	struct pinctrl_state *mcu_nrst_low;
	struct pinctrl_state *mcu_vdd_high;
	struct pinctrl_state *mcu_vdd_low;
	struct pinctrl_state *ois_vdd_high;
	struct pinctrl_state *ois_vdd_low;
	bool power_on;
	int is_updated;
	bool no_module_ver;
	unsigned char prev_vdis_coef;
};

enum {
	/* BASE ERROR ------------------------------------------------------------- */
	BOOT_ERR_BASE                         = -999, /* -9xx */
	BOOT_ERR_INVALID_PROTOCOL_GET_INFO,
	BOOT_ERR_INVALID_PROTOCOL_SYNC,
	BOOT_ERR_INVALID_PROTOCOL_READ,
	BOOT_ERR_INVALID_PROTOCOL_WRITE,
	BOOT_ERR_INVALID_PROTOCOL_ERASE,
	BOOT_ERR_INVALID_PROTOCOL_GO,
	BOOT_ERR_INVALID_PROTOCOL_WRITE_UNPROTECT,
	BOOT_ERR_INVALID_PROTOCOL_READ_UNPROTECT,
	BOOT_ERR_INVALID_MAX_WRITE_BYTES,

	/* I2C ERROR -------------------------------------------------------------- */
	BOOT_ERR_I2C_BASE                     = -899, /* -8xx */
	BOOT_ERR_I2C_RESP_NACK,
	BOOT_ERR_I2C_RESP_UNKNOWN,
	BOOT_ERR_I2C_RESP_API_FAIL,
	BOOT_ERR_I2C_XMIT_API_FAIL,
	BOOT_ERR_I2C_RECV_API_FAIL,

	/* SPI ERROR -------------------------------------------------------------- */
	BOOT_ERR_SPI_BASE                     = -799, /* -7xx */

	/* UART ERROR ------------------------------------------------------------- */
	BOOT_ERR_UART_BASE                    = -699, /* -6xx */

	/* DEVICE ERROR ----------------------------------------------------------- */
	BOOT_ERR_DEVICE_MEMORY_MAP            = -599, /* -5xx */
	BOOT_ERR_DEVICE_PAGE_SIZE_NOT_FOUND,

	/* API ERROR (OFFSET) ----------------------------------------------------- */
	BOOT_ERR_API_GET                      = -1000,
	BOOT_ERR_API_GET_ID                   = -2000,
	BOOT_ERR_API_GET_VER                  = -3000,
	BOOT_ERR_API_SYNC                     = -4000,
	BOOT_ERR_API_READ                     = -5000,
	BOOT_ERR_API_WRITE                    = -6000,
	BOOT_ERR_API_ERASE                    = -7000,
	BOOT_ERR_API_GO                       = -8000,
	BOOT_ERR_API_WRITE_UNPROTECT          = -9000,
	BOOT_ERR_API_READ_UNPROTECT           = -10000,
	BOOT_ERR_API_SAVE_CONTENTS            = -11000,
	BOOT_ERR_API_RESTORE_CONTENTS         = -12000,
};

extern int ois_mcu_fw_update(struct mcu_info *mcu_info, struct i2c_client *client);
extern int ois_mcu_pinctrl_init(struct mcu_info *mcu_info, struct pinctrl *pinctrl);
extern int ois_mcu_power_up(struct mcu_info *mcu_info);
extern int ois_mcu_power_down(struct mcu_info *mcu_info);
extern void ois_mcu_print_fw_version(struct mcu_info *mcu_info, char *str);
int ois_mcu_wait_idle(struct i2c_client *client, int retries);

#endif
