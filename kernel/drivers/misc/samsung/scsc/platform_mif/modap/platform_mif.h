
#include <linux/io.h>
#include <linux/regmap.h>

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

enum wlbt_boot_state {
	WLBT_BOOT_IN_RESET = 0,
	WLBT_BOOT_WAIT_CFG_REQ,
	WLBT_BOOT_ACK_CFG_REQ,
	WLBT_BOOT_CFG_DONE,
	WLBT_BOOT_CFG_ERROR
};

struct platform_mif {
	struct scsc_mif_abs    interface;
	struct scsc_mbox_s     *mbox;
	struct platform_device *pdev;

	struct device          *dev;

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
	void __iomem  *base;
	void __iomem  *base_wpan;
#if IS_ENABLED(CONFIG_WLBT_PMU2AP_MBOX)
	void __iomem  *base_pmu;
#endif
#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
	size_t        mem_size_region2;
	void __iomem  *mem_region2;
#endif
	/* pmu syscon regmap */
	struct regmap **regmap;

	/* Signalled when CFG_REQ IRQ handled */
	struct completion cfg_ack;

	/* State of CFG_REQ handler */
	enum wlbt_boot_state boot_state;

	/* Shared memory space - reserved memory */
	unsigned long mem_start;
	size_t        mem_size;
	void __iomem  *mem;

	/* Callback function and dev pointer mif_intr manager handler */
	void          (*wlan_handler)(int irq, void *data);
	void          (*wpan_handler)(int irq, void *data);
	void          *irq_dev;
	void          *irq_dev_wpan;
	/* spinlock to serialize driver access */
	spinlock_t    mif_spinlock;
	void          (*reset_request_handler)(int irq, void *data);
	void          *irq_reset_request_dev;

#ifdef CONFIG_SCSC_QOS
	bool qos_enabled;
#if IS_ENABLED(CONFIG_WLBT_QOS_CPUFREQ_POLICY)
	struct {
		/* For CPUCL-QoS */
		struct cpufreq_policy *cpucl0_policy;
		struct cpufreq_policy *cpucl1_policy;
#if defined(CONFIG_SOC_S5E8845)
		struct dvfs_rate_volt *mif_domain_fv;
#endif
	} qos;
#else
	/* QoS table */
	struct qos_table *qos;
#endif
#endif
	/* Suspend/resume handlers */
	int (*suspend_handler)(struct scsc_mif_abs *abs, void *data);
	void (*resume_handler)(struct scsc_mif_abs *abs, void *data);
	void *suspendresume_data;

#ifdef CONFIG_SCSC_WLBT_CFG_REQ_WQ
	struct work_struct cfgreq_wq;
	struct workqueue_struct *cfgreq_workq;
#endif
#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
	dma_addr_t paddr;
	int (*platform_mif_set_mem_region2)
		(struct scsc_mif_abs *interface,
		void __iomem *_mem_region2,
		size_t _mem_size_region2);
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	/* Callback function to check recovery status */
	bool (*recovery_disabled)(void);
#endif
	int *ka_patch_fw;
	size_t ka_patch_len;
	/* Callback function and dev pointer mif_intr manager handler */
	void          (*pmu_handler)(int irq, void *data);
	void          *irq_dev_pmu;

	uintptr_t remap_addr_wlan;
	uintptr_t remap_addr_wpan;

#if IS_ENABLED(CONFIG_EXYNOS_ITMON) || IS_ENABLED(CONFIG_EXYNOS_ITMON_V2)
        struct notifier_block itmon_nb;
#endif

#if IS_ENABLED(CONFIG_WLBT_PROPERTY_READ)
	struct device_node *np; /* cache device node */
#endif
#if IS_ENABLED(CONFIG_WLBT_EXYNOS_S2MPU)
	int dbus_baaw0_allowed_list_set, dbus_baaw1_allowed_list_set;
#endif
};
