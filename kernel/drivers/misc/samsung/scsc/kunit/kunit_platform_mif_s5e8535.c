#include <kunit/test.h>

#define devm_ioremap_resource(def, res)		((void *)0)
#define of_property_read_u32(DEV, PROPERTY, VALUE)		kunit_of_property_read_u32(VALUE)
#define of_property_count_u32_elems(DEV, PROPERTY)		kunit_of_property_count_u32_elems(PROPERTY)
#define of_property_read_u32_array(DEV, PROPERTY, QOS, LEN)	kunit_of_property_read_u32_array(PROPERTY, QOS, LEN)
#define platform_get_resource(DEV, OFFSET, OFFSET2)	kunit_platform_get_resource(OFFSET, OFFSET2)
#define syscon_regmap_lookup_by_phandle(args...)	((void *)1)
#define devm_kzalloc(DEV, SIZE, OPTION)		kunit_kzalloc(gtest, SIZE, OPTION)
#define devm_kfree(DEV, PTR)			((void *)0)
#define devm_request_irq(args...)		((void *)1)
#define devm_free_irq(args...)			((void *)0)
#define writel(args...)				((void *)0)
#define readl(args...)				(0)
#define regmap_read(args...)			((void *)0)
#define regmap_write(args...)			((void *)0)
#define regmap_update_bits(args...)		(0)
#define irq_get_irqchip_state(args...)		(0)
#define disable_irq_nosync(args...)		((void *)0)
#define wait_for_completion_timeout(args...)	(0)
#define vmalloc_to_pfn(args...)			((void *)0)
#define NUM_QOS_ELEMENT				12

struct mock_info{
	int product_id;
	int revision;
};

struct mock_info exynos_soc_info = {100, 1000};

static struct kunit *gtest;
void set_test_in_platform_mif(struct kunit *test)
{
	gtest = test;
}

u32 ptr_property;
void set_property(u32 value)
{
	ptr_property = value;
}

int kunit_of_property_read_u32(u32 *value)
{
	*value = ptr_property;

	return 0;
}

int kunit_of_property_count_u32_elems(char *property)
{
	if (!strcmp("qos_table", property))
		return NUM_QOS_ELEMENT;

	return 0;
}

int kunit_of_property_read_u32_array(char *property, unsigned int *qos, int len)
{
	int i;

	if (!strcmp("qos_table", property))
	{
		for (i = 0; i <	len; i++)
			*qos++ = 1000 + i;
	}
	return 0;
}

struct resource *kunit_platform_get_resource(unsigned offset, unsigned offset2)
{
	struct resource *rsc;

	rsc = (struct resource *)kunit_kzalloc(gtest, sizeof(struct resource), GFP_KERNEL);

	if (offset == IORESOURCE_IRQ) {
		if (offset2 == 0)
			rsc->name = "MBOX_WLAN";
		else if (offset2 == 1)
			rsc->name = "MBOX_WPAN";
		else if (offset2 == 2)
			rsc->name = "ALIVE";
		else if (offset2 == 3)
			rsc->name = "WDOG";
		else if (offset2 == 4)
			rsc->name = "CFG_REQ";
	} else {
		;
	}
	return rsc;

}

static void wlbt_karam_dump(struct platform_mif *platform);
void (*fp_wlbt_karam_dump)(struct platform_mif *platform) = &wlbt_karam_dump;

static int platform_mif_wlbt_phandle_property_read_u32(struct scsc_mif_abs *interface,
                                const char *phandle_name, const char *propname, u32 *out_value, size_t size);
int (*fp_platform_mif_wlbt_phandle_property_read_u32)(struct scsc_mif_abs *interface,
                                const char *phandle_name, const char *propname, u32 *out_value, size_t size) = &platform_mif_wlbt_phandle_property_read_u32;

static bool platform_mif_wlbt_property_read_bool(struct scsc_mif_abs *interface, const char *propname);
bool (*fp_platform_mif_wlbt_property_read_bool)(struct scsc_mif_abs *interface, const char *propname) = &platform_mif_wlbt_property_read_bool;

static int platform_mif_wlbt_property_read_u8(struct scsc_mif_abs *interface,
                                              const char *propname, u8 *out_value, size_t size);
int (*fp_platform_mif_wlbt_property_read_u8)(struct scsc_mif_abs *interface,
                                              const char *propname, u8 *out_value, size_t size) = &platform_mif_wlbt_property_read_u8;

static int platform_mif_wlbt_property_read_u16(struct scsc_mif_abs *interface,
                                              const char *propname, u16 *out_value, size_t size);
int (*fp_platform_mif_wlbt_property_read_u16)(struct scsc_mif_abs *interface,
                                              const char *propname, u16 *out_value, size_t size) = &platform_mif_wlbt_property_read_u16;

static int platform_mif_wlbt_property_read_u32(struct scsc_mif_abs *interface,
                                              const char *propname, u32 *out_value, size_t size);
int (*fp_platform_mif_wlbt_property_read_u32)(struct scsc_mif_abs *interface, 
                                              const char *propname, u32 *out_value, size_t size) = &platform_mif_wlbt_property_read_u32;

static int platform_mif_wlbt_property_read_string(struct scsc_mif_abs *interface,
                                              const char *propname, char **out_value, size_t size);
int (*fp_platform_mif_wlbt_property_read_string)(struct scsc_mif_abs *interface,
                                              const char *propname, char **out_value, size_t size) = &platform_mif_wlbt_property_read_string;

static int platform_mif_set_affinity_cpu(struct scsc_mif_abs *interface, u8 cpu);
int (*fp_platform_mif_set_affinity_cpu)(struct scsc_mif_abs *interface, u8 cpu) = &platform_mif_set_affinity_cpu;

static int platform_mif_pm_qos_add_request(struct scsc_mif_abs *interface, struct scsc_mifqos_request *qos_req, enum scsc_qos_config config);
int (*fp_platform_mif_pm_qos_add_request)(struct scsc_mif_abs *interface, struct scsc_mifqos_request *qos_req, enum scsc_qos_config config) = &platform_mif_pm_qos_add_request;

static int platform_mif_pm_qos_update_request(struct scsc_mif_abs *interface, struct scsc_mifqos_request *qos_req, enum scsc_qos_config config);
int (*fp_platform_mif_pm_qos_update_request)(struct scsc_mif_abs *interface, struct scsc_mifqos_request *qos_req, enum scsc_qos_config config) = &platform_mif_pm_qos_update_request;

static int platform_mif_pm_qos_remove_request(struct scsc_mif_abs *interface, struct scsc_mifqos_request *qos_req);
int (*fp_platform_mif_pm_qos_remove_request)(struct scsc_mif_abs *interface, struct scsc_mifqos_request *qos_req) = &platform_mif_pm_qos_remove_request;

static void platform_recovery_disabled_reg(struct scsc_mif_abs *, bool (*)(void));
void (*fp_platform_recovery_disabled_reg)(struct scsc_mif_abs *interface, bool (*handler)(void)) = &platform_recovery_disabled_reg;

static void platform_recovery_disabled_unreg(struct scsc_mif_abs *);
void (*fp_platform_recovery_disabled_unreg)(struct scsc_mif_abs *interface) = &platform_recovery_disabled_unreg;

static void platform_mif_irq_default_handler(int irq, void *data);
void (*fp_platform_mif_irq_default_handler)(int irq, void *data) = &platform_mif_irq_default_handler;

static void platform_mif_irq_reset_request_default_handler(int irq, void *data);
void (*fp_platform_mif_irq_reset_request_default_handler)(int irq, void *data) = &platform_mif_irq_reset_request_default_handler;

static void platform_mif_unregister_irq(struct platform_mif *platform);
void (*fp_platform_mif_unregister_irq)(struct platform_mif *platform) = &platform_mif_unregister_irq;

static int platform_mif_register_irq(struct platform_mif *platform);
int (*fp_platform_mif_register_irq)(struct platform_mif *platform) = &platform_mif_register_irq;

static void platform_mif_destroy(struct scsc_mif_abs *interface);
void (*fp_platform_mif_destroy)(struct scsc_mif_abs *interface) = &platform_mif_destroy;

static char *platform_mif_get_uid(struct scsc_mif_abs *interface);
char *(*fp_platform_mif_get_uid)(struct scsc_mif_abs *interface) = &platform_mif_get_uid;

static int platform_mif_start_wait_for_cfg_ack_completion(struct scsc_mif_abs *interface);
int (*fp_platform_mif_start_wait_for_cfg_ack_completion)(struct scsc_mif_abs *interface) = &platform_mif_start_wait_for_cfg_ack_completion;

static int platform_mif_pmu_reset_release(struct scsc_mif_abs *interface);
int (*fp_platform_mif_pmu_reset_release)(struct scsc_mif_abs *interface) = &platform_mif_pmu_reset_release;

static int platform_mif_reset(struct scsc_mif_abs *, bool);
int (*fp_platform_mif_reset)(struct scsc_mif_abs *interface, bool reset) = &platform_mif_reset;

static void __iomem *platform_mif_map_region(unsigned long phys_addr, size_t size);
void __iomem *(*fp_platform_mif_map_region)(unsigned long phys_addr, size_t size) = &platform_mif_map_region;

static void platform_mif_unmap_region(void *vmem);
void (*fp_platform_mif_unmap_region)(void *vmem) = &platform_mif_unmap_region;

static void *platform_mif_map(struct scsc_mif_abs *interface, size_t *allocated);
void *(*fp_platform_mif_map)(struct scsc_mif_abs *interface, size_t *allocated) = &platform_mif_map;

static void platform_mif_unmap(struct scsc_mif_abs *interface, void *mem);
void (*fp_platform_mif_unmap)(struct scsc_mif_abs *interface, void *mem) = &platform_mif_unmap;

static u32 platform_mif_irq_bit_mask_status_get(struct scsc_mif_abs *interface, enum scsc_mif_abs_target target);
u32 (*fp_platform_mif_irq_bit_mask_status_get)(struct scsc_mif_abs *interface, enum scsc_mif_abs_target target) = &platform_mif_irq_bit_mask_status_get;

static u32 platform_mif_irq_get(struct scsc_mif_abs *interface, enum scsc_mif_abs_target target);
u32 (*fp_platform_mif_irq_get)(struct scsc_mif_abs *interface, enum scsc_mif_abs_target target) = &platform_mif_irq_get;

static void platform_mif_irq_bit_set(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target);
void (*fp_platform_mif_irq_bit_set)(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target) = &platform_mif_irq_bit_set;

static void platform_mif_irq_bit_clear(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target);
void (*fp_platform_mif_irq_bit_clear)(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target) = &platform_mif_irq_bit_clear;

static void platform_mif_irq_bit_mask(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target);
void (*fp_platform_mif_irq_bit_mask)(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target) = &platform_mif_irq_bit_mask;

static void platform_mif_irq_bit_unmask(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target);
void (*fp_platform_mif_irq_bit_unmask)(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target) = &platform_mif_irq_bit_unmask;

static void platform_mif_remap_set(struct scsc_mif_abs *interface, uintptr_t remap_addr, enum scsc_mif_abs_target target);
void (*fp_platform_mif_remap_set)(struct scsc_mif_abs *interface, uintptr_t remap_addr, enum scsc_mif_abs_target target) = &platform_mif_remap_set;

static u32 __platform_mif_irq_bit_mask_read(struct platform_mif *platform);
static u32 __platform_mif_irq_bit_mask_read_wpan(struct platform_mif *platform);
static void __platform_mif_irq_bit_mask_write(struct platform_mif *platform, u32 val);
static void __platform_mif_irq_bit_mask_write_wpan(struct platform_mif *platform, u32 val);

static void platform_mif_irq_reg_handler(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data), void *dev);
void (*fp_platform_mif_irq_reg_handler)(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data), void *dev) = &platform_mif_irq_reg_handler;

static void platform_mif_irq_unreg_handler(struct scsc_mif_abs *interface);
void (*fp_platform_mif_irq_unreg_handler)(struct scsc_mif_abs *interface) = &platform_mif_irq_unreg_handler;

static void platform_mif_irq_reg_handler_wpan(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data), void *dev);
void (*fp_platform_mif_irq_reg_handler_wpan)(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data), void *dev) = &platform_mif_irq_reg_handler_wpan;

static void platform_mif_irq_unreg_handler_wpan(struct scsc_mif_abs *interface);
void (*fp_platform_mif_irq_unreg_handler_wpan)(struct scsc_mif_abs *interface) = &platform_mif_irq_unreg_handler_wpan;

static void platform_mif_irq_reg_reset_request_handler(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data), void *dev);
void (*fp_platform_mif_irq_reg_reset_request_handler)(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data), void *dev) = &platform_mif_irq_reg_reset_request_handler;

static void platform_mif_irq_unreg_reset_request_handler(struct scsc_mif_abs *interface);
void (*fp_platform_mif_irq_unreg_reset_request_handler)(struct scsc_mif_abs *interface) = &platform_mif_irq_unreg_reset_request_handler;


static void platform_mif_suspend_reg_handler(struct scsc_mif_abs *interface,
		int (*suspend)(struct scsc_mif_abs *abs, void *data),
		void (*resume)(struct scsc_mif_abs *abs, void *data),
		void *data);

void (*fp_platform_mif_suspend_reg_handler)(struct scsc_mif_abs *interface,
		int (*suspend)(struct scsc_mif_abs *abs, void *data),
		void (*resume)(struct scsc_mif_abs *abs, void *data),
		void *data) = &platform_mif_suspend_reg_handler;

static void platform_mif_suspend_unreg_handler(struct scsc_mif_abs *interface);
void (*fp_platform_mif_suspend_unreg_handler)(struct scsc_mif_abs *interface) = &platform_mif_suspend_unreg_handler;

static u32 *platform_mif_get_mbox_ptr(struct scsc_mif_abs *interface, u32 mbox_index, enum scsc_mif_abs_target target);
u32 *(*fp_platform_mif_get_mbox_ptr)(struct scsc_mif_abs *interface, u32 mbox_index, enum scsc_mif_abs_target target) = &platform_mif_get_mbox_ptr;

static int platform_mif_get_mbox_pmu(struct scsc_mif_abs *interface);
int (*fp_platform_mif_get_mbox_pmu)(struct scsc_mif_abs *interface) = &platform_mif_get_mbox_pmu;

static int platform_mif_set_mbox_pmu(struct scsc_mif_abs *interface, u32 val);
int (*fp_platform_mif_set_mbox_pmu)(struct scsc_mif_abs *interface, u32 val) = &platform_mif_set_mbox_pmu;

static int platform_load_pmu_fw(struct scsc_mif_abs *interface, u32 *ka_patch, size_t ka_patch_len);
int (*fp_platform_load_pmu_fw)(struct scsc_mif_abs *interface, u32 *ka_patch, size_t ka_patch_len) = &platform_load_pmu_fw;

static void platform_mif_irq_reg_pmu_handler(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data), void *dev);
void (*fp_platform_mif_irq_reg_pmu_handler)(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data), void *dev) = &platform_mif_irq_reg_pmu_handler;

static int platform_mif_get_mifram_ref(struct scsc_mif_abs *interface, void *ptr, scsc_mifram_ref *ref);
int (*fp_platform_mif_get_mifram_ref)(struct scsc_mif_abs *interface, void *ptr, scsc_mifram_ref *ref) = &platform_mif_get_mifram_ref;

static void *platform_mif_get_mifram_ptr(struct scsc_mif_abs *interface, scsc_mifram_ref ref);
void *(*fp_platform_mif_get_mifram_ptr)(struct scsc_mif_abs *interface, scsc_mifram_ref ref) = &platform_mif_get_mifram_ptr;

static void *platform_mif_get_mifram_phy_ptr(struct scsc_mif_abs *interface, scsc_mifram_ref ref);
void *(*fp_platform_mif_get_mifram_phy_ptr)(struct scsc_mif_abs *interface, scsc_mifram_ref ref) = &platform_mif_get_mifram_phy_ptr;

static uintptr_t platform_mif_get_mif_pfn(struct scsc_mif_abs *interface);
uintptr_t (*fp_platform_mif_get_mif_pfn)(struct scsc_mif_abs *interface) = &platform_mif_get_mif_pfn;

static struct device *platform_mif_get_mif_device(struct scsc_mif_abs *interface);
struct device *(*fp_platform_mif_get_mif_device)(struct scsc_mif_abs *interface) = &platform_mif_get_mif_device;

static void platform_mif_irq_clear(void);
void (*fp_platform_mif_irq_clear)(void) = &platform_mif_irq_clear;

static int platform_mif_read_register(struct scsc_mif_abs *interface, u64 id, u32 *val);
int (*fp_platform_mif_read_register)(struct scsc_mif_abs *interface, u64 id, u32 *val) = &platform_mif_read_register;

static void platform_mif_cleanup(struct scsc_mif_abs *interface);
void (*fp_platform_mif_cleanup)(struct scsc_mif_abs *interface) = &platform_mif_cleanup;

static void platform_mif_restart(struct scsc_mif_abs *interface);
void (*fp_platform_mif_restart)(struct scsc_mif_abs *interface) = &platform_mif_restart;

static bool init_done;
void kunit_set_init_done(bool done)
{
	init_done = done;
}