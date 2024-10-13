#ifndef _UFS_SAVE_AND_RESTORE_
#define _UFS_SAVE_AND_RESTORE_

ufs_cal_errno ufs_save_register(struct ufs_cal_param *p);
ufs_cal_errno ufs_restore_register(struct ufs_cal_param *p);

ufs_cal_errno ufs_cal_snr_store(struct ufs_cal_param *p);
ufs_cal_errno ufs_cal_snr_show(struct ufs_cal_param *p);
ufs_cal_errno ufs_cal_snr_init(struct ufs_cal_param *p);

#endif
