#ifndef _CAM_OIS_DRV_H
#define _CAM_OIS_DRV_H

enum OIS_I2C_DEV_IDX {
	OIS_I2C_DEV_NONE  = -1,
	OIS_I2C_DEV_IDX_1 = 0,
	OIS_I2C_DEV_IDX_2,
	OIS_I2C_DEV_IDX_3,
	OIS_I2C_DEV_IDX_MAX
};

#define CAM_OIS_MAGIC 'o'
/* CAM_OIS write */
#define CAM_OISIOC_S_WRITE _IOW(CAM_OIS_MAGIC, 0, u32)
/* CAM_OIS read */
#define CAM_OISIOC_G_READ _IOWR(CAM_OIS_MAGIC, 5, u32)

int __init cam_ois_drv_init(void);
void __exit cam_ois_drv_exit(void);

struct device *get_cam_ois_dev(void);
struct i2c_client *get_ois_i2c_client(enum OIS_I2C_DEV_IDX ois_i2c_ch);
#endif
