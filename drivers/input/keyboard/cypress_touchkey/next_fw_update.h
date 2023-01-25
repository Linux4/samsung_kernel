
#include <linux/firmware.h>
#include <linux/i2c/cypress_touchkey_234.h>

/* NEXTCHIP FW UPDATE */

#define NEXT_FW_VER		0X12

#define NEXT_FW_NAME	"next_tkey.fw"

#define NEXT_DEVICEID_FLASH_IN			0x75
#define NEXT_DEVICEID_FLASH				0x70

#define NEXT_REG_TEST_ENTER				0xAC5B
#define NEXT_REG_MCLK_PROTECT			0x4087
#define NEXT_REG_MCLK_SET				0x408B
#define NEXT_REG_EFLASH_GPROTECT			0x40FB
#define NEXT_REG_ERASE_PROTECT			0x4097
#define NEXT_REG_ERASE_PROTECT_INFO		0x409D
#define NEXT_REG_ERASE				0x409B
#define NEXT_REG_CHKSUM_LOW			0x409E
#define NEXT_REG_CHKSUM_HIGH			0x409F
#define NEXT_MAX_NUM_WR	66
/* NEXTCHIP FW UPDATE */

int next_i2c_fw_update(struct i2c_client *client);

