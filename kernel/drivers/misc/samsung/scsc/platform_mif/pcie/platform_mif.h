#include <linux/kfifo.h>
#include <scsc/scsc_wakelock.h>

enum wlbt_irqs {
	PLATFORM_MIF_MBOX,
	PLATFORM_MIF_MBOX_WPAN,
	PLATFORM_MIF_ALIVE,
	PLATFORM_MIF_WDOG,
	PLATFORM_MIF_CFG_REQ,
	PLATFORM_MIF_MBOX_PMU,
	/* must be last */
	PLATFORM_MIF_NUM_IRQS
};

struct platform_mif {
	struct scsc_mif_abs interface;
	struct scsc_mbox_s *mbox;
	struct platform_device *pdev;
	struct device *dev;
	int pmic_gpio;
	int wakeup_irq;
	int gpio_num;
	int clk_irq;
#ifdef CONFIG_SCSC_BB_REDWOOD
	int reset_gpio;
	int suspend_gpio;
#endif

	struct {
		int irq_num;
		int flags;
		atomic_t irq_disabled_cnt;
	} wlbt_irq[PLATFORM_MIF_NUM_IRQS];

	/* MIF registers preserved during suspend */
	struct {
		u32 irq_bit_mask;
		u32 irq_bit_mask_wpan;
	} mif_preserve;

	/* register MBOX memory space */
	void __iomem *base;
	void __iomem *base_wpan;
#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
	size_t mem_size_region2;
	void __iomem *mem_region2;
#endif
	/* Shared memory space - reserved memory */
	unsigned long mem_start;
	size_t mem_size;
	void __iomem *mem;

	/* Callback function and dev pointer mif_intr manager handler */
	void (*wlan_handler)(int irq, void *data);
	void (*wpan_handler)(int irq, void *data);
	void *irq_dev;
	void *irq_dev_wpan;
	/* spinlock to serialize driver access */
	spinlock_t mif_spinlock;
	void (*reset_request_handler)(int irq, void *data);
	void *irq_reset_request_dev;

#ifdef CONFIG_SCSC_QOS
	struct {
		/* For CPUCL-QoS */
		struct cpufreq_policy *cpucl0_policy;
		struct cpufreq_policy *cpucl1_policy;
		struct cpufreq_policy *cpucl2_policy;
		struct cpufreq_policy *cpucl3_policy;
		struct dvfs_rate_volt *int_domain_fv;
		struct dvfs_rate_volt *mif_domain_fv;
	} qos;
	bool qos_enabled;
#endif
	/* Suspend/resume handlers */
	int (*suspend_handler)(struct scsc_mif_abs *abs, void *data);
	void (*resume_handler)(struct scsc_mif_abs *abs, void *data);
	void *suspendresume_data;

#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
	dma_addr_t paddr;
	int (*platform_mif_set_mem_region2)(struct scsc_mif_abs *interface, void __iomem *_mem_region2,
					    size_t _mem_size_region2);
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	/* Callback function to check recovery status */
	bool (*recovery_disabled)(void);
#endif
	int *ka_patch_fw;
	size_t ka_patch_len;
	/* Callback function and dev pointer mif_intr manager handler */
	void (*pmu_handler)(int irq, void *data);
	void (*pmu_pcie_off_handler)(int irq, void *data);
	void (*pmu_error_handler)(int irq, void *data);
	void *irq_dev_pmu;

	uintptr_t remap_addr_wlan;
	uintptr_t remap_addr_wpan;

	struct pcie_mif *pcie;
	int pcie_ch_num;
	struct device_node *np; /* cache device node */

	int (*pcie_wakeup_cb)(void *first, void *second); /* claim_complete callback fn to call after turning PCIE on */
	void *pcie_wakeup_cb_data_service;
	void *pcie_wakeup_cb_data_dev;

	/* FSM */
	spinlock_t kfifo_lock;
	struct task_struct *t;
	wait_queue_head_t event_wait_queue;
	struct completion pcie_on;
	struct kfifo ev_fifo;
	int host_users;
	int fw_users;
	/* required variables for CB serialization */
	bool off_req;
	spinlock_t cb_sync;
	struct notifier_block	pm_nb;

	/* This mutex_lock is only used for exynos_acpm_write_reg function */
	struct mutex acpm_lock;

	/* wakelock to stop suspend during wake# IRQ handler and kthread processing it */
	struct wake_lock wakehash_wakelock;

	/* This variable is used to check if scan2mem dump is in progress */
	bool scan2mem_mode;
};
