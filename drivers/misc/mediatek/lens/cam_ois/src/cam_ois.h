#ifndef CAM_OIS_H
#define CAM_OIS_H

#include <linux/i2c.h>
#include "lens_info.h"

#define OIS_CAL_EEPROM_START_ADDR        0x0170
#define OIS_CAL_EEPROM_XGG_ADDR          0x0010
#define OIS_CAL_EEPROM_YGG_ADDR          0x0014
#define OIS_CAL_EEPROM_XSR_ADDR          0x0038
#define OIS_CAL_EEPROM_YSR_ADDR          0x003A
#define OIS_CAL_EEPROM_GG_SIZE           4
#define OIS_CAL_EEPROM_SR_SIZE           2
#define MAX_EFS_DATA_LENGTH              30
#define AF_I2C_SLAVE_ADDR                0x18


enum ois_af_param {
	OIS_SET_MODE = 1,
	OIS_SET_VDIS_COEF = 2,
};

/* check a enum camera_metadata_enum_samsung_android_lens_ois_mode
 * in a file mtkcam/utils/metadata/client/mtk_metadata_tag.h
 */
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
	char wide_z_goffset[3];

	char efs_gyro_cal[MAX_EFS_DATA_LENGTH];

	short x_hall_center;
	short y_hall_center;

	struct stAF_DrvList *cur_af_drv;
	spinlock_t *af_spinLock;
	int *af_opened;
	struct i2c_client *af_i2c_client;
	const struct file_operations *af_op;

	int af_pos_wide;
};

int cam_ois_prove_init(void);
int cam_ois_set_i2c_client(void);
int cam_ois_set_af_client(struct i2c_client *i2c_client_in, spinlock_t *af_spinLock, int *af_opened,
								const struct file_operations *af_op);
int cam_ois_set_mode(int Disable);
int cam_ois_set_vdis_coef(int inCoef);
int cam_ois_power_on(void);
int cam_ois_power_off(void);
int cam_ois_sysfs_power_onoff(bool onoff);
int cam_ois_init(void);
int cam_ois_deinit(void);
int cam_ois_center_shift(unsigned long position);
int cam_ois_get_offset_from_efs(long *raw_data_x, long *raw_data_y, long *raw_data_z);
int cam_ois_get_offset_from_buf(const char *buffer, int efs_size,
	long *raw_data_x, long *raw_data_y, long *raw_data_z);
int cam_ois_mcu_update(void);
int cam_ois_get_eeprom_from_sysfs(void);
int cam_ois_af_power_on(void);
int cam_ois_af_power_off(void);
int cam_ois_gyro_sensor_noise_check(long *stdev_data_x, long *stdev_data_y);
void cam_ois_get_hall_position(unsigned int *targetPosition, unsigned int *hallPosition);
extern void AF_PowerDown(void);
#endif
