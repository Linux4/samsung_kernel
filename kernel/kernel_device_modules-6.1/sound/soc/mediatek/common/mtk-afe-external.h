/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2018 MediaTek Inc.
 */

#ifndef MTK_MEM_ALLOCATION_CONTROL_H_
#define MTK_MEM_ALLOCATION_CONTROL_H_
#include <linux/notifier.h>

enum {
	NOTIFIER_VOW_ALLOCATE_MEM = 1,
	NOTIFIER_ULTRASOUND_ALLOCATE_MEM,
	NOTIFIER_ADSP_3WAY_SEMAPHORE_GET,
	NOTIFIER_ADSP_3WAY_SEMAPHORE_RELEASE,
	NOTIFIER_SCP_3WAY_SEMAPHORE_GET,
	NOTIFIER_SCP_3WAY_SEMAPHORE_RELEASE,
	NOTIFIER_VP_AUDIO_START,
	NOTIFIER_VP_AUDIO_STOP,
	NOTIFIER_VP_AUDIO_TRIGGER,
	NOTIFIER_VP_AUDIO_TIMER,
	NOTIFIER_VOW_IPI_SEND,
	NOTIFIER_ULTRA_AFE_HW_FREE
};

/* sound soc vow ipi related */
enum vow_sound_soc_ipi_msgid_t {
	SOUND_SOC_IPIMSG_VOW_PCM_HWFREE = 100
};

enum {
	SOUND_SOC_VOW_IPI_BYPASS_ACK = 0,
	SOUND_SOC_VOW_IPI_NEED_ACK,
	SOUND_SOC_VOW_IPI_ACK_BACK
};

struct vow_sound_soc_ipi_send_info {
	unsigned int msg_id;
	unsigned int payload_len;
	unsigned int *payload;
	unsigned int need_ack;
};

int register_afe_allocate_mem_notifier(struct notifier_block *nb);
int unregister_afe_allocate_mem_notifier(struct notifier_block *nb);
int notify_allocate_mem(unsigned long module, void *v);

int register_3way_semaphore_notifier(struct notifier_block *nb);
int unregister_3way_semaphore_notifier(struct notifier_block *nb);
int notify_3way_semaphore_control(unsigned long module, void *v);

int register_vp_audio_notifier(struct notifier_block *nb);
int unregister_vp_audio_notifier(struct notifier_block *nb);
int notify_vb_audio_control(unsigned long module, void *v);

int register_vow_ipi_send_notifier(struct notifier_block *nb);
int unregister_vow_ipi_send_notifier(struct notifier_block *nb);
int notify_vow_ipi_send(unsigned long module, void *v);

// ultrasound register notify for AFE HW free condition
int register_ultra_afe_hw_free_notifier(struct notifier_block *nb);
int unregister_ultra_afe_hw_free_notifier(struct notifier_block *nb);
int notify_ultra_afe_hw_free(unsigned long module, void *v);

#endif /* MTK_MEM_ALLOCATION_CONTROL_H_ */
