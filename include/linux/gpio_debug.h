/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef GPIO_DEBUG_H__
#define GPIO_DEBUG_H__

#include <linux/errno.h>

#ifdef CONFIG_GPIO_DEBUG
struct gpio_debug_event_def {
	char *name;
	char *description;
};

void gpio_debug_event_enter(int base, int id);
void gpio_debug_event_exit(int base, int id);
int gpio_debug_event_list_register(struct gpio_debug_event_def *events, int event_count);
void gpio_debug_event_list_unregister(int base, int event_count);
int gpio_debug_event_enabled(int base, int id);
#else
#define gpio_debug_event_enter(_base, _id)
#define gpio_debug_event_exit(_base, _id)
#define gpio_debug_event_list_register(_events, _event_count) (-ENOSYS)
#define gpio_debug_event_list_unregister(_base, _event_count)
#define gpio_debug_event_enabled(_base, _id) (0)
#endif

#endif
