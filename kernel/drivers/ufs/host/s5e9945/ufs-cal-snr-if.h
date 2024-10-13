#ifndef _UFS_SAVE_AND_RESTORE_
#define _UFS_SAVE_AND_RESTORE_

#include "ufs-cal-snr-if.h"

typedef ufs_cal_errno (*cal_if_func) (struct ufs_cal_param *);

ufs_cal_errno ufs_cal_hce_enable(struct ufs_cal_param *p);
ufs_cal_errno ufs_cal_pre_pm(struct ufs_cal_param *p, IP_NAME ip_type);
ufs_cal_errno ufs_cal_post_pm(struct ufs_cal_param *p);
ufs_cal_errno ufs_cal_resume_hibern8(struct ufs_cal_param *p);
ufs_cal_errno ufs_save_register(struct ufs_cal_param *p);
ufs_cal_errno ufs_restore_register(struct ufs_cal_param *p);

ufs_cal_errno ufs_cal_snr_store(struct ufs_cal_param *p);
ufs_cal_errno ufs_cal_snr_show(struct ufs_cal_param *p);
ufs_cal_errno ufs_cal_snr_init(struct ufs_cal_param *p);
#endif
