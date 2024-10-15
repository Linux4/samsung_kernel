/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * linux/include/linux/exynos-dsufreq.h
 *
 */

int dsufreq_register_notifier(struct notifier_block *nb);
int dsufreq_unregister_notifier(struct notifier_block *nb);

unsigned long *dsufreq_get_freq_table(int *table_size);

int dsufreq_qos_add_request(char *label, struct dev_pm_qos_request *req,
		   enum dev_pm_qos_req_type type, s32 value);

unsigned int dsufreq_get_cur_freq(void);
unsigned int dsufreq_get_max_freq(void);
unsigned int dsufreq_get_min_freq(void);
int exynos_dsufreq_target(unsigned int target_freq);
