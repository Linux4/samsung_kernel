#include <kunit/test.h>

#define devm_ioremap_resource(def, res)		((void *)0)
#define of_property_read_u32(DEV, PROPERTY, VALUE)		kunit_of_property_read_u32(VALUE)

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

struct mock_info{
	int product_id;
	int revision;
};

struct mock_info exynos_soc_info = {100, 1000};

static struct kunit *gtest;
void set_test(struct kunit *test)
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

static void platform_recovery_disabled_reg(struct scsc_mif_abs *, bool (*)(void));
void kunit_platform_recovery_disabled_reg(struct scsc_mif_abs *interface, bool (*handler)(void))
{
	platform_recovery_disabled_reg(interface, handler);
}

static void platform_recovery_disabled_unreg(struct scsc_mif_abs *);
void kunit_platform_recovery_disabled_unreg(struct scsc_mif_abs *interface)
{
	platform_recovery_disabled_unreg(interface);
}

static void platform_mif_irq_default_handler(int irq, void *data);
void kunit_platform_mif_irq_default_handler(int irq, void *data)
{
	platform_mif_irq_default_handler(irq, data);
}

static void platform_mif_irq_reset_request_default_handler(int irq, void *data);
void kunit_platform_mif_irq_reset_request_default_handler(int irq, void *data)
{
	platform_mif_irq_reset_request_default_handler(irq, data);
}

static void platform_mif_unregister_irq(struct platform_mif *platform);
void kunit_platform_mif_unregister_irq(struct platform_mif *platform)
{
	platform_mif_unregister_irq(platform);
}

static int platform_mif_register_irq(struct platform_mif *platform);
int kunit_platform_mif_register_irq(struct platform_mif *platform)
{
	return platform_mif_register_irq(platform);
}

static void platform_mif_destroy(struct scsc_mif_abs *interface);
void kunit_platform_mif_destroy(struct scsc_mif_abs *interface)
{
	platform_mif_destroy(interface);
}

static char *platform_mif_get_uid(struct scsc_mif_abs *interface);
char *kunit_platform_mif_get_uid(struct scsc_mif_abs *interface)
{
	return platform_mif_get_uid(interface);
}

static int platform_mif_start_wait_for_cfg_ack_completion(struct scsc_mif_abs *interface);
int kunit_platform_mif_start_wait_for_cfg_ack_completion(struct scsc_mif_abs *interface)
{
	return platform_mif_start_wait_for_cfg_ack_completion(interface);
}

static int platform_mif_pmu_reset_release(struct scsc_mif_abs *interface);
int kunit_platform_mif_pmu_reset_release(struct scsc_mif_abs *interface)
{
	return platform_mif_pmu_reset_release(interface);
}

static int platform_mif_reset(struct scsc_mif_abs *, bool);
int kunit_platform_mif_reset(struct scsc_mif_abs *interface, bool reset)
{
	return platform_mif_reset(interface, reset);
}

static void __iomem *platform_mif_map_region(unsigned long phys_addr, size_t size);
void __iomem *kunit_platform_mif_map_region(unsigned long phys_addr, size_t size)
{
	return platform_mif_map_region(phys_addr, size);
}

static void platform_mif_unmap_region(void *vmem);
void kunit_platform_mif_unmap_region(void *vmem)
{
	platform_mif_unmap_region(vmem);
}

static void *platform_mif_map(struct scsc_mif_abs *interface, size_t *allocated);
void *kunit_platform_mif_map(struct scsc_mif_abs *interface, size_t *allocated)
{
	return platform_mif_map(interface, allocated);
}

static void platform_mif_unmap(struct scsc_mif_abs *interface, void *mem);
void kunit_platform_mif_unmap(struct scsc_mif_abs *interface, void *mem)
{
	platform_mif_unmap(interface, mem);
}

static u32 platform_mif_irq_bit_mask_status_get(struct scsc_mif_abs *interface, enum scsc_mif_abs_target target);
u32 kunit_platform_mif_irq_bit_mask_status_get(struct scsc_mif_abs *interface, enum scsc_mif_abs_target target)
{
	return platform_mif_irq_bit_mask_status_get(interface, target);
}

static u32 platform_mif_irq_get(struct scsc_mif_abs *interface, enum scsc_mif_abs_target target);
u32 kunit_platform_mif_irq_get(struct scsc_mif_abs *interface, enum scsc_mif_abs_target target)
{
	return platform_mif_irq_get(interface, target);
}

static void platform_mif_irq_bit_set(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target);
void kunit_platform_mif_irq_bit_set(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target)
{
	platform_mif_irq_bit_set(interface, bit_num, target);
}

static void platform_mif_irq_bit_clear(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target);
void kunit_platform_mif_irq_bit_clear(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target)
{
	platform_mif_irq_bit_clear(interface, bit_num, target);
}

static void platform_mif_irq_bit_mask(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target);
void kunit_platform_mif_irq_bit_mask(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target)
{
	platform_mif_irq_bit_mask(interface, bit_num, target);
}

static void platform_mif_irq_bit_unmask(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target);
void kunit_platform_mif_irq_bit_unmask(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target)
{
	platform_mif_irq_bit_unmask(interface, bit_num, target);
}

static void platform_mif_remap_set(struct scsc_mif_abs *interface, uintptr_t remap_addr, enum scsc_mif_abs_target target);
void kunit_platform_mif_remap_set(struct scsc_mif_abs *interface, uintptr_t remap_addr, enum scsc_mif_abs_target target)
{
	platform_mif_remap_set(interface, remap_addr, target);
}

static u32 __platform_mif_irq_bit_mask_read(struct platform_mif *platform);
static u32 __platform_mif_irq_bit_mask_read_wpan(struct platform_mif *platform);
static void __platform_mif_irq_bit_mask_write(struct platform_mif *platform, u32 val);
static void __platform_mif_irq_bit_mask_write_wpan(struct platform_mif *platform, u32 val);

static void platform_mif_irq_reg_handler(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data), void *dev);
void kunit_platform_mif_irq_reg_handler(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data), void *dev)
{
	platform_mif_irq_reg_handler(interface, handler, dev);
}

static void platform_mif_irq_unreg_handler(struct scsc_mif_abs *interface);
void kunit_platform_mif_irq_unreg_handler(struct scsc_mif_abs *interface)
{
	platform_mif_irq_unreg_handler(interface);
}

static void platform_mif_irq_reg_handler_wpan(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data), void *dev);
void kunit_platform_mif_irq_reg_handler_wpan(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data), void *dev)
{
	platform_mif_irq_reg_handler_wpan(interface, handler, dev);
}

static void platform_mif_irq_unreg_handler_wpan(struct scsc_mif_abs *interface);
void kunit_platform_mif_irq_unreg_handler_wpan(struct scsc_mif_abs *interface)
{
	platform_mif_irq_unreg_handler_wpan(interface);
}

static void platform_mif_irq_reg_reset_request_handler(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data), void *dev);
void kunit_platform_mif_irq_reg_reset_request_handler(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data), void *dev)
{
	platform_mif_irq_reg_reset_request_handler(interface, handler, dev);
}

static void platform_mif_irq_unreg_reset_request_handler(struct scsc_mif_abs *interface);
void kunit_platform_mif_irq_unreg_reset_request_handler(struct scsc_mif_abs *interface)
{
	platform_mif_irq_unreg_reset_request_handler(interface);
}


static void platform_mif_suspend_reg_handler(struct scsc_mif_abs *interface,
		int (*suspend)(struct scsc_mif_abs *abs, void *data),
		void (*resume)(struct scsc_mif_abs *abs, void *data),
		void *data);

void kunit_platform_mif_suspend_reg_handler(struct scsc_mif_abs *interface,
		int (*suspend)(struct scsc_mif_abs *abs, void *data),
		void (*resume)(struct scsc_mif_abs *abs, void *data),
		void *data)
{

	platform_mif_suspend_reg_handler(interface, suspend, resume, data);
}

static void platform_mif_suspend_unreg_handler(struct scsc_mif_abs *interface);
void kunit_platform_mif_suspend_unreg_handler(struct scsc_mif_abs *interface)
{
	platform_mif_suspend_unreg_handler(interface);
}

static u32 *platform_mif_get_mbox_ptr(struct scsc_mif_abs *interface, u32 mbox_index, enum scsc_mif_abs_target target);
u32 *kunit_platform_mif_get_mbox_ptr(struct scsc_mif_abs *interface, u32 mbox_index, enum scsc_mif_abs_target target)
{
	return platform_mif_get_mbox_ptr(interface, mbox_index, target);
}

static int platform_mif_get_mbox_pmu(struct scsc_mif_abs *interface);
int kunit_platform_mif_get_mbox_pmu(struct scsc_mif_abs *interface)
{
	return platform_mif_get_mbox_pmu(interface);
}

static int platform_mif_set_mbox_pmu(struct scsc_mif_abs *interface, u32 val);
int kunit_platform_mif_set_mbox_pmu(struct scsc_mif_abs *interface, u32 val)
{
	return platform_mif_set_mbox_pmu(interface, val);
}

static int platform_load_pmu_fw(struct scsc_mif_abs *interface, u32 *ka_patch, size_t ka_patch_len);
int kunit_platform_load_pmu_fw(struct scsc_mif_abs *interface, u32 *ka_patch, size_t ka_patch_len)
{
	return platform_load_pmu_fw(interface, ka_patch, ka_patch_len);
}

static void platform_mif_irq_reg_pmu_handler(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data), void *dev);
void kunit_platform_mif_irq_reg_pmu_handler(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data), void *dev)
{
	platform_mif_irq_reg_pmu_handler(interface, handler, dev);
}

static int platform_mif_get_mifram_ref(struct scsc_mif_abs *interface, void *ptr, scsc_mifram_ref *ref);
int kunit_platform_mif_get_mifram_ref(struct scsc_mif_abs *interface, void *ptr, scsc_mifram_ref *ref)
{
	return platform_mif_get_mifram_ref(interface, ptr, ref);
}

static void *platform_mif_get_mifram_ptr(struct scsc_mif_abs *interface, scsc_mifram_ref ref);
void *kunit_platform_mif_get_mifram_ptr(struct scsc_mif_abs *interface, scsc_mifram_ref ref)
{
	return platform_mif_get_mifram_ptr(interface, ref);
}

static void *platform_mif_get_mifram_phy_ptr(struct scsc_mif_abs *interface, scsc_mifram_ref ref);
void *kunit_platform_mif_get_mifram_phy_ptr(struct scsc_mif_abs *interface, scsc_mifram_ref ref)
{
	return platform_mif_get_mifram_phy_ptr(interface, ref);
}

static uintptr_t platform_mif_get_mif_pfn(struct scsc_mif_abs *interface);
uintptr_t kunit_platform_mif_get_mif_pfn(struct scsc_mif_abs *interface)
{
	return platform_mif_get_mif_pfn(interface);
}

static struct device *platform_mif_get_mif_device(struct scsc_mif_abs *interface);
struct device *kunit_platform_mif_get_mif_device(struct scsc_mif_abs *interface)
{
	return platform_mif_get_mif_device(interface);
}

static void platform_mif_irq_clear(void);
void kunit_platform_mif_irq_clear(void)
{
	platform_mif_irq_clear();
}

static int platform_mif_read_register(struct scsc_mif_abs *interface, u64 id, u32 *val);
int kunit_platform_mif_read_register(struct scsc_mif_abs *interface, u64 id, u32 *val)
{
	return platform_mif_read_register(interface, id, val);
}

static void platform_mif_cleanup(struct scsc_mif_abs *interface);
void kunit_platform_mif_cleanup(struct scsc_mif_abs *interface)
{
	platform_mif_cleanup(interface);
}

static void platform_mif_restart(struct scsc_mif_abs *interface);
void kunit_platform_mif_restart(struct scsc_mif_abs *interface)
{
	platform_mif_restart(interface);
}

static bool init_done;
void kunit_set_init_done(bool done)
{
	init_done = done;
}
