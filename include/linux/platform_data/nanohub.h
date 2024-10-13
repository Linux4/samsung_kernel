#ifndef __LINUX_PLATFORM_DATA_NANOHUB_H
#define __LINUX_PLATFORM_DATA_NANOHUB_H

#include <linux/types.h>

struct nanohub_flash_bank {
	int bank;
	u32 address;
	size_t length;
};

#define MAX_FILE_LEN (32)

struct nanohub_platform_data {
	void *mailbox_client;
	int irq;
};

enum CHUB_STATE {
	CHUB_FW_ST_INVAL,
	CHUB_FW_ST_POWERON,
	CHUB_FW_ST_OFF,
	CHUB_FW_ST_ON,
};

/* Structure of notifier block */
struct contexthub_notifier_block {
	const char *subsystem;
	unsigned int start_off;
	unsigned int end_off;
	enum CHUB_STATE state;
	int (*notifier_call)(struct contexthub_notifier_block *);
};

int contexthub_notifier_register(struct contexthub_notifier_block *nb);
#endif /* __LINUX_PLATFORM_DATA_NANOHUB_H */
