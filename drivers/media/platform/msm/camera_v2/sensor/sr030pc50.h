#ifndef __SR030PC50_H__
#define __SR030PC50_H__

#include "msm_sensor.h"

#undef DEBUG_LEVEL_HIGH
#undef CDBG

#define DEBUG_LEVEL_HIGH
#ifdef DEBUG_LEVEL_HIGH
#define CDBG(fmt, args...)	printk("[SR030PC50] %s : %d : " fmt "\n",   __FUNCTION__, __LINE__, ##args)
#endif


struct sr030pc50_userset {
    unsigned int metering;
    unsigned int exposure;
    unsigned int wb;
    unsigned int iso;
    unsigned int effect;
    unsigned int scenemode;
	unsigned int resolution;
};

struct sr030pc50_ctrl {
    struct sr030pc50_userset settings;
    unsigned int op_mode;
    unsigned int prev_mode;
    unsigned int streamon;
    unsigned int exif_iso;
    unsigned int exif_shutterspeed ;
};

int32_t sr030pc50_sensor_config(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp);
int32_t sr030pc50_sensor_native_control(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp);
int sr030pc50_sensor_match_id(struct msm_camera_i2c_client *sensor_i2c_client,
	struct msm_camera_slave_info *slave_info,
	const char *sensor_name);
#endif	//__SR030PC50_H__