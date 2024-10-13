/*
 * common.h - common header for framework and plugins
 *
 * author : Yongjin Lee (yongjin0.lee@samsung.com)
 *	    Jeonghoon Jang (jnghn.jang@samsung.com)
 */

#ifndef __COMMON_H__
#define __COMMON_H__

#if !IS_ENABLED(CONFIG_EXYNOS_ESCAV2)
typedef unsigned long long int	u64;
typedef unsigned int		u32;
typedef unsigned short		u16;
typedef unsigned char		u8;
typedef signed int		s32;
typedef signed short		s16;
typedef signed char		s8;
typedef float			f32;
typedef unsigned char		bool;
#endif

/**
 * struct ipc_cmd_info - RX command buffer info for framework and plugins
 *
 * @rx_cmd:		pointer to the RX buffer entry to be dequeued.
 */
struct ipc_cmd_info {
	u32 rx_cmd;
#if !defined(CONFIG_SOC_S5E9935) && !defined(CONFIG_SOC_S5E9945)
	u32 rx_cmd_indr;
#endif
};

/**
 * struct dbg_log_info - log buffer entry format
 */
struct dbg_log_info {
	char str[8];
	u32 val;
	u32 log_level;
};

/**
 * struct build_info
 */
struct build_info {
	char build_version[25];
	char build_time[25];
};

/* PMIC_IF definition */
struct pmic_if_ops {
	int (*init)(void);
#if defined(CONFIG_EXYNOS_MFD_I3C)
	int (*tx)(u8 channel, unsigned int reg, unsigned int val);
	int (*rx)(u8 channel, unsigned int reg);
#elif defined(CONFIG_EXYNOS_MFD_SPMI)
	int (*tx)(u8 sid, u16 addr, unsigned int val);
	int (*rx)(u8 sid, u16 addr);
	int (*bulk_tx)(u8 sid, u16 addr, u8 *val, u8 byte_count);
	int (*bulk_rx)(u8 sid, u16 addr, u8 *val, u8 byte_count);
#endif
};

/**
 * struct esca_ops - framework callbacks to be provided to plugins
 *
 * @set_timer_event:	adds timer request which expires at every 'msec'.
 * @del_timer_event:	removes the existing timer request.
 * @insert_dbg_log:	inserts a log entry at predefined log buffer region.
 * @get_tx_dest:	does full check of tx queue and returns ptr to enqueue.
 * @enqueue_tx:		updated tx_front ptr of tx queue and generates mbox irq.
 */
struct esca_ops {
	s32 (*set_timer_event) (u32 plugin_id, u32 msec);
	void (*del_timer_event) (u32 plugin_id);
	void (*insert_dbg_log) (u32 plugin_id, struct dbg_log_info *log);
	u32 (*get_tx_dest) (u32 ch_num, u32 *index);
	void (*enqueue_tx) (u32 ch_num, u32 index);
	s32 (*secure_func) (void *plugin, u32 arg0, u32 arg1,
                        u32 arg2, u32 arg3);
	s32 (*external_plugin_func) (void *plugin, u32 pid,
			u32 *arg0, u32 *arg1, u32 *arg2);
	s32 (*pmic_if_init)(void);
#if defined(CONFIG_MFD_I3C)
	s32 (*pmic_if_read)(u8 channel, u32 addr);
	s32 (*pmic_if_write)(u8 channel, u32 addr, u32 data);
#elif defined(CONFIG_MFD_SPMI)
	s32 (*pmic_if_read)(u8 sid, u16 addr);
	s32 (*pmic_if_write)(u8 sid, u16 addr, u32 data);
	s32 (*pmic_if_bulk_read)(u8 sid, u16 addr, u8 *buf, u8 byte_count);
	s32 (*pmic_if_bulk_write)(u8 sid, u16 addr, u8 *buf, u8 byte_count);
#endif
	void (*udelay)(u32 udelay);
	void (*intr_enable)(u32 pid, u32 intr);
	void (*intr_disable)(u32 pid, u32 intr);
	void (*preempt_disable_irq_save)(u32 *flag);
	void (*preempt_enable_irq_restore)(u32 *flag);
	s32 (*set_pmic_if_ops)(struct pmic_if_ops *pmic_if_ops);
	u32 (*get_board_type)(void);
	u32 (*get_board_rev)(void);
	u32 (*get_chip_rev)(void);
	u32 (*get_systick_cvr)(void);
};

/**
 * struct plugin_ops - plugin callbacks to be provided to framework
 *
 * @ipc_handler:	handler to be executed when ipc for this plugin is arrived.
 * @irq_handler:	handler to be executed when hw irq for this plugin is arrived.
 * @timer_event_handler:handler to be executed when requested timer is expired.
 */
struct plugin_ops {
#ifdef CONFIG_ESCA_FW
	s32 (*ipc_handler) (struct ipc_cmd_info *cmd, u32 ch_num);
	s32 (*irq_handler) (u32 irq_num);
	s32 (*timer_event_handler) (u8 event_id);
	s32 (*extern_func) (u32 *arg0, u32 *arg1, u32 *arg2);
	struct build_info info;
	s32 (*worker) (struct work *work);
#else
	u32 ipc_handler;
	u32 irq_handler;
	u32 timer_event_handler;
	u32 extern_func;
	struct build_info info;
#endif
};

/**
 * struct timer_desc - A descriptor for timer request
 *
 * @period:		requested period that framework executes timer_event_handler.
 * @multiplier:
 */
struct timer_desc {
	u32 period;
	u32 multiplier;
};

/**
 * struct plugin - The basic plugin structure
 *
 * @id:			Predefined id for this plugin.
 * @base_addr:		Predefined base addr for this plugin. (entrypoint)
 * @esca_ops:		Framework callbacks.
 * @plugin_ops:		Plugin callbacks.
 * @timer:		Timer descriptor for this plugin.
 * @is_attached:	For dynamic plugin support.
 * @size:		The size of this plugin.
 */
struct plugin {
#ifdef CONFIG_ESCA_FW
	u32 id;
	void *base_addr;
	struct acpm_ops *acpm_ops;
	struct plugin_ops *plugin_ops;
	u32 secure_func_mask;
        u32 extern_func_mask;
	struct timer_desc timer;							//TODO: remove
	u8 is_attached;
	u32 size;									//TODO: remove
	u8 stay_attached;								//TODO: remove
	const char *fw_name;								//TODO: remove
	u32 *dbg;
	u8 core_affinity;
#else
	u32 id;
	u32 base_addr;
	u32 acpm_ops;
	u32 plugin_ops;
	u32 secure_func_mask;
        u32 extern_func_mask;
	struct timer_desc timer;
	u8 is_attached;
	u32 size;
	u8 stay_attached;
	u32 fw_name;
	u32 dbg;
	u8 core_affinity;
#endif
};

typedef void (*pfn_plugin_init) (struct plugin *p);

enum ret_type {
	RET_OK,
	RET_FAIL,
};

#if !IS_ENABLED(CONFIG_EXYNOS_ESCAV2)
#define NULL	0
#define FALSE ((bool) 0)
#define TRUE  ((bool) 1)

#define __raw_writel(data, base)		(*(volatile int*)(base) = (data))
#define __raw_readl(base)			(*(volatile int*)(base))

#define ARRAY_SIZE(a)			(sizeof(a) / sizeof((a)[0]))

#define EBUSY			100
#define EINVAL			101
#endif

#endif
