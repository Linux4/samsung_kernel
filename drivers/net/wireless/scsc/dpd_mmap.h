/****************************************************************************
 *
 * Copyright (c) 2014 - 2022 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __DPD_MMAP_H__
#define __DPD_MMAP_H__

#define SLSI_WLAN_DPD_DRV_MSG_ID_WLAN_ON    1
#define SLSI_WLAN_DPD_DRV_MSG_ID_WLAN_OFF   2
#define SLSI_WLAN_DPD_DRV_MSG_ID_WLAN_INTR  3

int slsi_wlan_dpd_mmap_set_buffer(struct slsi_dev *sdev, void *buf, size_t sz);
void slsi_wlan_dpd_mmap_user_space_event(int msg_id);

int slsi_wlan_dpd_mmap_create(void);
int slsi_wlan_dpd_mmap_destroy(void);
#endif /* __DPD_MMAP_H__ */
