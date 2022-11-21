/*
 * Copyright (c) 2017 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/sched.h>

/*
 * EMS Tune
 */
struct mllist_head {
	struct list_head node_list;
};

struct mllist_node {
	int			prio;
	struct list_head	prio_list;
	struct list_head	node_list;
};

struct emstune_mode_request {
	struct mllist_node node;
	bool active;
	struct delayed_work work; /* for emstune_update_request_timeout */
	char *func;
	unsigned int line;
};

/*
 * Sysbusy
 */
enum sysbusy_state {
	SYSBUSY_STATE0 = 0,
	SYSBUSY_STATE1,
	SYSBUSY_STATE2,
	SYSBUSY_STATE3,
	NUM_OF_SYSBUSY_STATE,
};

#define SYSBUSY_CHECK_BOOST	(0)
#define SYSBUSY_STATE_CHANGE	(1)

/*
 * EMStune
 */
#define EMSTUNE_GAME_MODE		(3)

#define BITS_OF_BOOST_TYPE		(3)
#define EMSTUNE_BOOST_TYPE_EXTREME	(1 << (BITS_OF_BOOST_TYPE - 1)) /* Last bit in boost level rage */
#define EMSTUNE_BOOST_TYPE_MASK		((1 << BITS_OF_BOOST_TYPE) - 1)
#define EMSTUNE_BOOST_TYPE(type)	(type & EMSTUNE_BOOST_TYPE_MASK)

#define BEGIN_OF_MODE_TYPE		BITS_OF_BOOST_TYPE
#define EMSTUNE_MODE_TYPE_GAME		(1 << (BEGIN_OF_MODE_TYPE + EMSTUNE_GAME_MODE))
#define EMSTUNE_MODE_TYPE_MASK		((-1) << BEGIN_OF_MODE_TYPE)

#define EMSTUNE_MODE_TYPE(type)		((type & EMSTUNE_MODE_TYPE_MASK) >> BEGIN_OF_MODE_TYPE)

#if IS_ENABLED(CONFIG_SCHED_EMS)
extern void stt_acquire_gmc_flag(int step, int cnt);
extern void stt_release_gmc_flag(void);
extern int emstune_get_cur_mode(void);
extern int emstune_get_cur_level(void);
extern void emstune_update_request(struct emstune_mode_request *req, s32 new_value);
extern void emstune_update_request_timeout(struct emstune_mode_request *req, s32 new_value,
					unsigned long timeout_us);

#define emstune_add_request(req)	do {				\
	__emstune_add_request(req, (char *)__func__, __LINE__);	\
} while(0);
extern void __emstune_add_request(struct emstune_mode_request *req, char *func, unsigned int line);
extern void emstune_remove_request(struct emstune_mode_request *req);

extern void emstune_boost(struct emstune_mode_request *req, int enable);
extern void emstune_boost_timeout(struct emstune_mode_request *req, unsigned long timeout_us);

extern int emstune_register_notifier(struct notifier_block *nb);
extern int emstune_unregister_notifier(struct notifier_block *nb);

extern int emstune_get_set_type(void *_set);

extern int ecs_request_register(char *name, const struct cpumask *mask);
extern int ecs_request_unregister(char *name);
extern int ecs_request(char *name, const struct cpumask *mask);

extern int sysbusy_register_notifier(struct notifier_block *nb);
extern int sysbusy_unregister_notifier(struct notifier_block *nb);
#else
static inline void stt_acquire_gmc_boost(int step, int cnt) { }
static inline void stt_release_gmc_boost(void) { }
static inline int emstune_get_cur_mode(void) { return -1; }
static inline int emstune_get_cur_level(void) { return -1; }
static inline void emstune_update_request(struct emstune_mode_request *req, s32 new_value) { }
static inline void emstune_update_request_timeout(struct emstune_mode_request *req, s32 new_value,
					unsigned long timeout_us) { }

#define emstune_add_request(req)	do { } while(0);
static inline void __emstune_add_request(struct emstune_mode_request *req, char *func, unsigned int line) { }
static inline void emstune_remove_request(struct emstune_mode_request *req) { }

static inline void emstune_boost(struct emstune_mode_request *req, int enable) { }
static inline void emstune_boost_timeout(struct emstune_mode_request *req, unsigned long timeout_us) { }

static inline int emstune_register_notifier(struct notifier_block *nb) { return 0; }
static inline int emstune_unregister_notifier(struct notifier_block *nb) { return 0; }

static inline int emstune_get_set_type(void *_set) { return -EINVAL; }

static inline int ecs_request_register(char *name, const struct cpumask *mask) { return 0; }
static inline int ecs_request_unregister(char *name) { return 0; }
static inline int ecs_request(char *name, const struct cpumask *mask) { return 0; }

static inline int sysbusy_register_notifier(struct notifier_block *nb) { return 0; };
static inline int sysbusy_unregister_notifier(struct notifier_block *nb) { return 0; };
#endif
