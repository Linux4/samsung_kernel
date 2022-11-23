/*
 * Protect lru of task support.It's between normal lru and mlock,
 * that means we will reclaim protect lru pages as late as possible.
 *
 * Copyright (C) 2001-2021, Huawei Tech.Co., Ltd. All rights reserved.
 *
 * Authors:
 * Shaobo Feng <fengshaobo@huawei.com>
 * Xishi Qiu <qiuxishi@huawei.com>
 * Yisheng Xie <xieyisheng1@huawei.com>
 * jing Xia <jing.xia@unisoc.com>
 *
 * This program is free software; you can redistibute it and/or modify
 * it under the t erms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PROTECT_LRU_H
#define PROTECT_LRU_H

#ifdef CONFIG_PROTECT_LRU
#include <linux/sysctl.h>

extern unsigned long protect_lru_enable __read_mostly;
extern struct ctl_table protect_lru_table[];
extern unsigned long protect_reclaim_ratio;

extern void protect_lruvec_init(struct mem_cgroup *memcg, struct lruvec *lruvec);
extern void protect_lru_set_from_process(struct page *page);
extern void add_page_to_protect_lru_list(struct page *page, struct lruvec *lruvec,
					 bool lru_head);
extern void del_page_from_protect_lru_list(struct page *page,
					   struct lruvec *lruvec);
extern struct page *protect_lru_move_and_shrink(struct page *page);
extern void shrink_protect_lru(struct lruvec *lruvec, bool force);

extern const struct file_operations proc_protect_level_operations;
#else
static inline void add_page_to_protect_lru_list(struct page *page,
					 struct lruvec *lruvec,
					 bool lru_head){}
static inline void del_page_from_protect_lru_list(struct page *page,
					   struct lruvec *lruvec){}
#endif
#endif
