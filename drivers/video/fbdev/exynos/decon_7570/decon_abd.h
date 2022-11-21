/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __DECON_ABD_H__
#define __DECON_ABD_H__

#define ABD_EVENT_LOG_MAX	50
#define ABD_LOG_MAX		10

struct abd_event_log {
	u64 stamp;
	const char *print;
};

struct abd_event {
	struct abd_event_log log[ABD_EVENT_LOG_MAX];
	atomic_t log_idx;
};

struct abd_log {
	u64 stamp;

	/* pin */
	unsigned int level;
	unsigned int state;
	unsigned int onoff;

	/* fence */
	unsigned int winid;
	struct sync_fence fence;

	/* bit error */
	unsigned int size;
	unsigned int value;
	char *print[32];	/* max: 32 bit */
};

struct abd_trace {
	const char *name;
	unsigned int count;
	unsigned int lcdon_flag;
	struct abd_log log[ABD_LOG_MAX];
};

struct abd_pin {
	const char *name;
	unsigned int irq;
	struct irq_desc *desc;
	int gpio;
	int level;
	int active_level;

	struct abd_trace p_first;
	struct abd_trace p_lcdon;
	struct abd_trace p_event;

	irq_handler_t	handler;
	void		*dev_id;
};

enum {
	ABD_PIN_PCD,
	ABD_PIN_DET,
	ABD_PIN_ERR,
	ABD_PIN_MAX
};

struct abd_protect {
	struct abd_pin pin[ABD_PIN_MAX];
	struct abd_event event;

	struct abd_trace f_first;
	struct abd_trace f_lcdon;
	struct abd_trace f_event;

	struct abd_trace u_first;
	struct abd_trace u_lcdon;
	struct abd_trace u_event;

	struct abd_trace b_first;
	struct abd_trace b_event;

	unsigned int irq_enable;
	struct notifier_block reboot_notifier;
	spinlock_t slock;
};

struct decon_device;
extern void decon_abd_enable(struct decon_device *decon, int enable);
extern int decon_abd_register(struct decon_device *decon);
extern void decon_abd_save_log_fto(struct abd_protect *abd, struct sync_fence *fence);
extern int decon_abd_register_pin_handler(int irq, irq_handler_t handler, void *dev_id);
extern void decon_abd_save_log_event(struct abd_protect *abd, const char *print);
extern void decon_abd_save_log_bit(struct abd_protect *abd, unsigned int size, unsigned int value, char **print);

#endif

