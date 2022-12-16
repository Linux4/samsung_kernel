#ifndef MTK_OIS_MCU_H
#define MTK_OIS_MCU_H

#include <linux/i2c.h>
#include "lens_info.h"

#define OIS_CAL_EEPROM_START_ADDR    0x0170
#define OIS_CAL_EEPROM_XGG_ADDR      0x0010
#define OIS_CAL_EEPROM_YGG_ADDR      0x0014
#define OIS_CAL_EEPROM_XSR_ADDR      0x0038
#define OIS_CAL_EEPROM_YSR_ADDR      0x003A
#define OIS_CAL_EEPROM_GG_SIZE       4
#define OIS_CAL_EEPROM_SR_SIZE       2
#define AF_I2C_SLAVE_ADDR            0x18
#define OIS_AF_MAIN_NAME             AFDRV_DW9825AF_OIS_MCU

enum ois_af_param {
	OIS_SET_MODE = 1,
	OIS_SET_VDIS_COEF = 2,
};

/* check a enum camera_metadata_enum_samsung_android_lens_ois_mode
	in a file mtkcam/utils/metadata/client/mtk_metadata_tag.h */
enum ois_mode {
	OIS_MODE_STILL = 0,
	OIS_MODE_MOVIE,
	OIS_MODE_SINE_X,
	OIS_MODE_SINE_Y,
	OIS_MODE_ZOOM = 101,
	OIS_MODE_VDIS = 102,
	OIS_MODE_CENTERING = 103,
	OIS_CUSTOM_MODE_TEST = 1000,
};

struct ois_info {
	//gyro gain
	char wide_x_gg[5];
	char wide_y_gg[5];
	//coefficient
	char wide_x_coef[3];
	char wide_y_coef[3];
	//suppression_ratio
	char wide_x_sr[3];
	char wide_y_sr[3];

	//gyro orientation(SW)
	unsigned char wide_x_gp;
	unsigned char wide_y_gp;
	unsigned char gyro_orientation;

	//Gyro offset
	char wide_x_goffset[3];
	char wide_y_goffset[3];

	short x_hall_center;
	short y_hall_center;

	struct stAF_DrvList *cur_af_drv;
	spinlock_t *af_spinLock;
	int *af_opened;
	const struct file_operations *af_op;

	int af_pos_wide;
	int is_fw_updated;
};

int ois_mcu_probe(struct device *pdev, struct pinctrl *pinctrl, const struct file_operations *af_op);
int ois_mcu_set_i2c_client(struct i2c_client *i2c_client_in, spinlock_t *af_spinLock, int *af_opened);
int ois_mcu_set_camera_mode(int Disable);
int ois_mcu_set_vdis_coef(int inCoef);
int ois_mcu_power_on(void);
int ois_mcu_power_off(void);
int ois_mcu_init(void);
int ois_mcu_deinit(void);
int ois_mcu_center_shift(unsigned long position);
int ois_mcu_get_offset_from_efs(long *raw_data_x, long *raw_data_y);
extern void AF_PowerDown(void);
#endif
