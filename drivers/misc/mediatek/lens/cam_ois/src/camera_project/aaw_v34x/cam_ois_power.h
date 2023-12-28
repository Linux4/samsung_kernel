#ifndef CAM_OIS_POWER_H
#define CAM_OIS_POWER_H

struct mcu_info;

int cam_ois_pinctrl_init(struct mcu_info *mcu_info);
int cam_ois_mcu_power_up(struct mcu_info *mcu_info);
int cam_ois_mcu_power_down(struct mcu_info *mcu_info);
int cam_ois_sysfs_mcu_power_up(struct mcu_info *mcu_info);
int cam_ois_sysfs_mcu_power_down(struct mcu_info *mcu_info);
int cam_ois_af_power_up(struct mcu_info *mcu_info);
int cam_ois_af_power_down(struct mcu_info *mcu_info);

#endif
