#include <kunit/test.h>

#include "../platform_mif.h"
#include <linux/platform_device.h>

#define freq_qos_update_request(args...) ((void)0)
#define devm_ioremap_resource(args...)		((void *)0)
#define syscon_regmap_lookup_by_phandle(args...)	((void *)1)
#define devm_kfree(args...)			((void *)0)
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
#define of_property_read_u32(DEV, PROPERTY, VALUE)		kunit_of_property_read_u32(VALUE)
#define of_property_count_u32_elems(DEV, PROPERTY)		kunit_of_property_count_u32_elems(PROPERTY)
#define of_property_read_u32_array(DEV, PROPERTY, QOS, LEN)	kunit_of_property_read_u32_array(PROPERTY, QOS, LEN)
#define platform_get_resource(DEV, OFFSET, OFFSET2)	kunit_platform_get_resource(OFFSET, OFFSET2)
#define devm_kzalloc(DEV, SIZE, OPTION)		kunit_kzalloc(gtest, SIZE, OPTION)
#define of_irq_to_resource(node, i, res) kunit_of_irq_to_resource(node, i, res)

#if defined(SCSC_SEP_VERSION)
extern int (*fp_platform_mif_wlbt_phandle_property_read_u32)(struct scsc_mif_abs *interface,
                                const char *phandle_name, const char *propname, u32 *out_value, size_t size);
#endif
extern int (*fp_platform_mif_set_affinity_cpu)(struct scsc_mif_abs *interface, u8 cpu);
extern int (*fp_platform_mif_pm_qos_add_request)(struct scsc_mif_abs *interface, struct scsc_mifqos_request *qos_req, enum scsc_qos_config config);
extern int (*fp_platform_mif_pm_qos_update_request)(struct scsc_mif_abs *interface, struct scsc_mifqos_request *qos_req, enum scsc_qos_config config);
extern int (*fp_platform_mif_pm_qos_remove_request)(struct scsc_mif_abs *interface, struct scsc_mifqos_request *qos_req);
extern void (*fp_platform_recovery_disabled_reg)(struct scsc_mif_abs *interface, bool (*handler)(void));
extern void (*fp_platform_recovery_disabled_unreg)(struct scsc_mif_abs *interface);
extern void (*fp_platform_mif_irq_default_handler)(int irq, void *data);
extern void (*fp_platform_mif_irq_reset_request_default_handler)(int irq, void *data);
extern void (*fp_platform_mif_unregister_irq)(struct platform_mif *platform);
extern void (*fp_platform_mif_destroy)(struct scsc_mif_abs *interface);
extern char *(*fp_platform_mif_get_uid)(struct scsc_mif_abs *interface);
extern int (*fp_platform_mif_start_wait_for_cfg_ack_completion)(struct scsc_mif_abs *interface);
extern int (*fp_platform_mif_pmu_reset_release)(struct scsc_mif_abs *interface);
extern int (*fp_platform_mif_reset)(struct scsc_mif_abs *interface, bool reset);
extern struct device *(*fp_platform_mif_get_mif_device)(struct scsc_mif_abs *interface);
extern void (*fp_platform_mif_irq_clear)(void);
extern int (*fp_platform_mif_read_register)(struct scsc_mif_abs *interface, u64 id, u32 *val);
extern void (*fp_platform_mif_cleanup)(struct scsc_mif_abs *interface);
extern void (*fp_platform_mif_restart)(struct scsc_mif_abs *interface);

static void test_all(struct kunit *test)
{
	struct scsc_mif_abs *scscmif;
	struct platform_device pdev;
	struct device dev;
	struct scsc_mifqos_request qos_req;
	struct platform_mif *platform;
	int allocated;
	int val;

	pdev.dev = dev;
	set_test_in_platform_mif(test);
	set_property(0x99);

	scscmif = platform_mif_create(&pdev);

	//set_property(0x00);
	//scscmif = platform_mif_create(&pdev);
	
	platform = container_of(scscmif, struct platform_mif, interface);

	//platform_mif_pm_qos_get_table(scscmif, SCSC_QOS_MIN);
	//platform_mif_pm_qos_get_table(scscmif, SCSC_QOS_MED);
	//platform_mif_pm_qos_get_table(scscmif, SCSC_QOS_MAX);
	//platform_mif_pm_qos_get_table(scscmif, SCSC_QOS_MAX+1);

	fp_platform_mif_set_affinity_cpu(scscmif, 0);


	fp_platform_mif_pm_qos_add_request(scscmif, &qos_req, 0);
	fp_platform_mif_pm_qos_update_request(scscmif, &qos_req, 0);	
	fp_platform_mif_pm_qos_remove_request(scscmif, &qos_req);

	platform->qos_enabled = false;
	
	fp_platform_mif_pm_qos_add_request(scscmif, &qos_req, 0);
	fp_platform_mif_pm_qos_update_request(scscmif, &qos_req, 0);
	fp_platform_mif_pm_qos_remove_request(scscmif, &qos_req);

 	platform_recovery_disabled_reg(scscmif, NULL);

	platform_recovery_disabled_unreg(scscmif);

	platform_mif_irq_default_handler(0, 0);

	platform_mif_irq_reset_request_default_handler(0, NULL);

	// platform_wlbt_karamdump(scscmif);

	platform_mif_isr(0, platform);

	platform_mif_isr_wpan(0, platform);

	platform_wdog_isr(0, platform);

	platform_cfg_req_irq_clean_pending(platform);

	platform_set_wlbt_regs(platform);

	platform_cfg_req_isr(0, platform);

	platform_mif_unregister_irq(platform);

	platform_mif_register_irq(platform);

	fp_platform_mif_destroy(scscmif);

	fp_platform_mif_get_uid(scscmif);

	fp_platform_mif_start_wait_for_cfg_ack_completion(scscmif);

	fp_platform_mif_pmu_reset_release(scscmif);

	kunit_set_init_done(true);

	fp_platform_mif_pmu_reset_release(scscmif);

	platform_mif_reset(scscmif, true);

	platform_mif_reset(scscmif, false);

	platform_mif_map_region(0x01, 0x01);
	platform_mif_map_region(0x01, MIFRAMMAN_MAXMEM+1);

	platform_mif_unmap_region(NULL);

	platform_mif_map(scscmif, &allocated);

	platform_mif_unmap(scscmif, NULL);

	platform_mif_irq_bit_mask_status_get(scscmif, 0);

	platform_mif_irq_get(scscmif, 0);

	platform_mif_irq_bit_set(scscmif, 0, 0);
	platform_mif_irq_bit_set(scscmif, 16, 0);

	platform_mif_irq_bit_clear(scscmif, 0, 0);
	platform_mif_irq_bit_clear(scscmif, 16, 0);

	platform_mif_irq_bit_mask(scscmif, 0, 0);
	platform_mif_irq_bit_mask(scscmif, 16, 0);

	platform_mif_irq_bit_unmask(scscmif, 0, 0);
	platform_mif_irq_bit_unmask(scscmif, 16, 0);

	platform_mif_remap_set(scscmif, 0, SCSC_MIF_ABS_TARGET_WLAN);
	platform_mif_remap_set(scscmif, 0, SCSC_MIF_ABS_TARGET_WPAN);
	platform_mif_remap_set(scscmif, 0, SCSC_MIF_ABS_TARGET_WPAN+1);

	platform_mif_irq_reg_handler(scscmif, NULL, NULL);

	platform_mif_irq_unreg_handler(scscmif);

	platform_mif_irq_reg_handler_wpan(scscmif, NULL, NULL);

	platform_mif_irq_unreg_handler_wpan(scscmif);

	platform_mif_irq_reg_reset_request_handler(scscmif, NULL, NULL);

	platform_mif_irq_unreg_reset_request_handler(scscmif);

	platform_mif_suspend_reg_handler(scscmif, NULL, NULL, NULL);

	platform_mif_suspend_unreg_handler(scscmif);

	platform_mif_get_mbox_ptr(scscmif, 0, SCSC_MIF_ABS_TARGET_WLAN);
	platform_mif_get_mbox_ptr(scscmif, 0, SCSC_MIF_ABS_TARGET_WPAN);

#if defined(SCSC_SEP_VERSION)
	fp_platform_mif_wlbt_phandle_property_read_u32(scscmif, "ABCD", "BEEF", &val, 0);
#endif

	platform_mif_get_mbox_pmu(scscmif);

	// platform_mif_set_mbox_pmu(scscmif, 0);

	// fp_platform_load_pmu_fw(scscmif, "ABCD", 4);
	// fp_platform_load_pmu_fw(scscmif, "ABCD", 4);

	platform_mif_irq_reg_pmu_handler(scscmif, NULL, NULL);

	platform_mif_get_mifram_ref(scscmif, NULL, NULL);
	platform_mif_get_mifram_ref(scscmif, "ABCDE", NULL);

	platform_mif_get_mifram_ptr(scscmif, NULL);

	platform_mif_get_mifram_phy_ptr(scscmif, NULL);
	val = platform->mem_start;
	platform->mem_start=0;
	platform_mif_get_mifram_phy_ptr(scscmif, NULL);
	platform->mem_start = val;

	u8 dst = 0;
	u16 size = 0;
	u32 value = 0;
	char* string = "";
	int vsize = 0;

	kunit_platform_mif_wlbt_property_read_bool(scscmif, "test");
	kunit_platform_mif_wlbt_property_read_u8(scscmif, "test", &dst, vsize);
	kunit_platform_mif_wlbt_property_read_u16(scscmif, "test", &size, vsize);
	kunit_platform_mif_wlbt_property_read_u32(scscmif, "test", &value, vsize);
	kunit_platform_mif_wlbt_property_read_string(scscmif, "test", &string, vsize);

	platform_mif_get_mif_pfn(scscmif);

	fp_platform_mif_get_mif_device(scscmif);

	fp_platform_mif_irq_clear();

	fp_platform_mif_read_register(scscmif, 0, NULL);

	fp_platform_mif_cleanup(scscmif);

	fp_platform_mif_restart(scscmif);

	platform_mif_destroy_platform(NULL, NULL);

	platform_mif_get_platform_dev(scscmif);

	platform_mif_suspend(scscmif);

	platform_mif_resume(scscmif);

	platform_mif_reg_write(platform, 0, 0);

	platform_mif_reg_read(platform, 0);

	platform_mif_reg_write_wpan(platform, 0, 0);

	platform_mif_reg_read_wpan(platform, 0);

	const struct kernel_param *kp = NULL;
	qos_get_param(string, kp);

	KUNIT_EXPECT_STREQ(test, "OK", "OK");
}

static int test_init(struct kunit *test)
{
	return 0;
}

static void test_exit(struct kunit *test)
{
}

static struct kunit_case test_cases[] = {
	KUNIT_CASE(test_all),
	{}
};

static struct kunit_suite test_suite[] = {
	{
		.name = "test_platform_mif",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};


kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("youngss.kim@samsung.com>");

