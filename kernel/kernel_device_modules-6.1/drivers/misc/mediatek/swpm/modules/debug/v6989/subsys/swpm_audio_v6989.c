// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <swpm_audio_v6989.h>

/****************************************************************************
 *  Local Variables
 ****************************************************************************/
struct audio_swpm_data audio_data = {0};

/* share sram for swpm cpu data */
static struct audio_swpm_data *audio_swpm_data_ptr;

static void update_audio_scenario(void)
{
#ifdef AUDIO_KERNEL_EXIST
	audio_data = mt6989_aud_get_power_scenario();
#endif
	*audio_swpm_data_ptr = audio_data;
}

static int audio_swpm_event(struct notifier_block *nb,
			  unsigned long event, void *v)
{
	switch (event) {
	case SWPM_LOG_DATA_NOTIFY:
		update_audio_scenario();
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block audio_swpm_notifier = {
	.notifier_call = audio_swpm_event,
};

int swpm_audio_v6989_init(void)
{
	unsigned int offset;

	offset = swpm_set_and_get_cmd(0, 0, 0, AUDIO_CMD_TYPE);

	audio_swpm_data_ptr = (struct audio_swpm_data *)sspm_sbuf_get(offset);

	/* exception control for illegal sbuf request */
	if (!audio_swpm_data_ptr) {
		pr_notice("%s(), swpm audio share sram offset fail\n", __func__);
		return -1;
	}

	pr_notice("%s(), swpm audio init offset = 0x%x\n", __func__, offset);
	swpm_register_event_notifier(&audio_swpm_notifier);

	return 0;
}

void swpm_audio_v6989_exit(void)
{
	swpm_unregister_event_notifier(&audio_swpm_notifier);
}
