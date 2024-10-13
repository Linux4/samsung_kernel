#ifndef __ESCA_DBG_H__
#define __ESCA_DBG_H__

extern u32 ESCA_TIMER_LAYER;

struct esca_debug_info {
	void __iomem *log_buff_rear;
	void __iomem *log_buff_front;
	void __iomem *log_buff_base;
	unsigned int log_buff_len;
	unsigned int log_buff_size;

	unsigned int num_dumps;
	void __iomem **sram_dumps;
	void __iomem **dram_dumps;
	unsigned int *dump_sizes;

	unsigned int debug_log_level;
	struct esca_ipc_info *ipc_info;
	struct delayed_work periodic_work;
	struct work_struct update_log_work;

	spinlock_t lock;
};

#if IS_ENABLED(CONFIG_EXYNOS_ESCAV2)
extern struct workqueue_struct *update_log_wq;
extern bool is_esca_stop_log;
extern bool esca_stop_log_req;

extern void esca_ramdump(void);
extern void esca_log_idx_update(void);
extern void esca_log_print(void);
extern void esca_dbg_init(struct platform_device *pdev);
extern void esca_ktime_sram_sync(void);
#else
static inline void esca_ramdump(void) {}
static inline void esca_log_idx_update(void) {}
static inline void esca_log_print(void) {}
static inline void esca_dbg_init(struct platform_device *pdev)
{
	return -1;
}
#endif
#endif		/* __ESCA_DBG_H__ */
