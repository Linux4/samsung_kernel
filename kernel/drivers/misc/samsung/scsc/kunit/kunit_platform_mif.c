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
#define of_irq_to_resource(node, i, res) kunit_of_irq_to_resource(node, i, res)

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

struct kunit *get_test_in_platform_mif(void)
{
	return gtest;
}

u32 ptr_property;
void set_property(u32 value)
{
	ptr_property = value;
}

int kunit_of_irq_to_resource(struct device_node *node, int i, struct resource *irq_res)
{
	if (i == 0)
		irq_res->name = "MBOX_WLAN";
	else if (i == 1)
		irq_res->name = "MBOX_WPAN";
	else if (i == 2)
		irq_res->name = "ALIVE";
	else if (i == 3)
		irq_res->name = "WDOG";
	else if (i == 4)
		irq_res->name = "CFG_REQ";
	return i+1;
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
	}
	return rsc;

}
#if IS_ENABLED(CONFIG_EXYNOS_ITMON) || IS_ENABLED(CONFIG_EXYNOS_ITMON_V2)
static void wlbt_karam_dump(struct platform_mif *platform);
void (*fp_wlbt_karam_dump)(struct platform_mif *platform) = &wlbt_karam_dump;
#endif

static void platform_mif_destroy(struct scsc_mif_abs *interface);
void (*fp_platform_mif_destroy)(struct scsc_mif_abs *interface) = &platform_mif_destroy;

static char *platform_mif_get_uid(struct scsc_mif_abs *interface);
char *(*fp_platform_mif_get_uid)(struct scsc_mif_abs *interface) = &platform_mif_get_uid;

static void __iomem *platform_mif_map_region(unsigned long phys_addr, size_t size);
void __iomem *(*fp_platform_mif_map_region)(unsigned long phys_addr, size_t size) = &platform_mif_map_region;

static void platform_mif_unmap_region(void *vmem);
void (*fp_platform_mif_unmap_region)(void *vmem) = &platform_mif_unmap_region;

static int platform_load_pmu_fw(struct scsc_mif_abs *interface, u32 *ka_patch, size_t ka_patch_len);
int (*fp_platform_load_pmu_fw)(struct scsc_mif_abs *interface, u32 *ka_patch, size_t ka_patch_len) = &platform_load_pmu_fw;

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
