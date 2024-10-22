/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef __AUDIO_MBOX_H__
#define __AUDIO_MBOX_H__

#include <linux/soc/mediatek/mtk-mbox.h>

/* for mbox-num = 2 */
#define AUDIO_TOTAL_MBOX             2
#define AUDIO_MBOX0_CH_ID            0
#define AUDIO_MBOX1_CH_ID            1
#define AUDIO_TOTAL_SEND_PIN         1
#define AUDIO_TOTAL_RECV_PIN         1
#define AUDIO_MBOX_SEND_SLOT_SIZE    64
#define AUDIO_MBOX_RECV_SLOT_SIZE    64
#define AUDIO_MBOX_SEND_SLOT_OFFSET  0
#define AUDIO_MBOX_RECV_SLOT_OFFSET  0

/* for mbox-num = 1 */
#define AUDIO_MBOX_SLOT_SIZE_1CH         32
#define AUDIO_MBOX_SEND_SLOT_OFFSET_1CH  0
#define AUDIO_MBOX_RECV_SLOT_OFFSET_1CH  32

int audio_mbox_send(void *msg, unsigned int wait);
bool is_audio_mbox_init_done(void);

extern struct device_attribute dev_attr_audio_ipi_test;
extern int audio_mbox_pin_cb(unsigned int id, void *prdata, void *buf, unsigned int len);

extern int adsp_mem_device_probe(struct platform_device *pdev);

#endif  /* __AUDIO_MBOX_H__ */
