#ifndef _CAM_OIS_MCU_TEST_H_
#define _CAM_OIS_MCU_TEST_H_

#include "camera_kunit_main.h"

#define copy_from_user(to, from, size) copy_from_user_kunit(to, from, size)
#define copy_to_user(to, from, size) copy_to_user_kunit(to, from, size)

static inline int copy_from_user_kunit (void *to, const void __user *from, unsigned long n)
{
    return true;
}

static inline int copy_to_user_kunit (void *to, const void __user *from, unsigned long n)
{
    return true;
}

extern struct cam_ois_ctrl_t *g_o_ctrl;

extern int cam_ois_get_dev_handle(struct cam_ois_ctrl_t *o_ctrl, void *arg);
extern int cam_ois_slaveInfo_pkt_parser(struct cam_ois_ctrl_t *o_ctrl, uint32_t *cmd_buf, size_t len);
extern int32_t cam_sensor_update_power_settings(void *cmd_buf, uint32_t cmd_length, struct cam_sensor_power_ctrl_t *power_info, size_t cmd_buf_len);
extern void cam_ois_reset(void *ctrl);
extern int cam_ois_thread_destroy(struct cam_ois_ctrl_t *o_ctrl);
extern int cam_ois_mcu_release_dev_handle(struct cam_ois_ctrl_t *o_ctrl, void *arg);
extern int cam_ois_mcu_shutdown (struct cam_ois_ctrl_t *o_ctrl);
extern int cam_ois_mcu_stop_dev (struct cam_ois_ctrl_t *o_ctrl);


int target_validation(struct cam_ois_ctrl_t *o_ctrl);
int target_empty_check_status(struct cam_ois_ctrl_t *o_ctrl);
int target_option_update(struct cam_ois_ctrl_t *o_ctrl);
int target_read_hwver(struct cam_ois_ctrl_t *o_ctrl);
int target_read_vdrinfo(struct cam_ois_ctrl_t *o_ctrl);
int target_empty_check_clear(struct cam_ois_ctrl_t * o_ctrl);
int32_t cam_ois_fw_update(struct cam_ois_ctrl_t *o_ctrl, bool is_force_update);

#endif /* _CAM_OIS_MCU_TEST_H_ */