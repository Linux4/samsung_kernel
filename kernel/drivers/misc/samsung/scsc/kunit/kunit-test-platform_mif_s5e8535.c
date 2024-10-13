#include <kunit/test.h>
#include "../platform_mif_s5e8535.h"
#include <linux/platform_device.h>
extern int (*fp_platform_mif_wlbt_phandle_property_read_u32)(struct scsc_mif_abs *interface,
                                const char *phandle_name, const char *propname, u32 *out_value, size_t size);
extern bool (*fp_platform_mif_wlbt_property_read_bool)(struct scsc_mif_abs *interface, const char *propname);
extern int (*fp_platform_mif_wlbt_property_read_u8)(struct scsc_mif_abs *interface,
                                              const char *propname, u8 *out_value, size_t size);
extern int (*fp_platform_mif_wlbt_property_read_u16)(struct scsc_mif_abs *interface,
                                              const char *propname, u16 *out_value, size_t size);
extern int (*fp_platform_mif_wlbt_property_read_u32)(struct scsc_mif_abs *interface, 
                                              const char *propname, u32 *out_value, size_t size);
extern int (*fp_platform_mif_wlbt_property_read_string)(struct scsc_mif_abs *interface,
                                              const char *propname, char **out_value, size_t size);
extern int (*fp_platform_mif_set_affinity_cpu)(struct scsc_mif_abs *interface, u8 cpu);
extern int (*fp_platform_mif_pm_qos_add_request)(struct scsc_mif_abs *interface, struct scsc_mifqos_request *qos_req, enum scsc_qos_config config);
extern int (*fp_platform_mif_pm_qos_update_request)(struct scsc_mif_abs *interface, struct scsc_mifqos_request *qos_req, enum scsc_qos_config config);
extern int (*fp_platform_mif_pm_qos_remove_request)(struct scsc_mif_abs *interface, struct scsc_mifqos_request *qos_req);
extern void (*fp_platform_recovery_disabled_reg)(struct scsc_mif_abs *interface, bool (*handler)(void));
extern void (*fp_platform_recovery_disabled_unreg)(struct scsc_mif_abs *interface);
extern void (*fp_platform_mif_irq_default_handler)(int irq, void *data);
extern void (*fp_platform_mif_irq_reset_request_default_handler)(int irq, void *data);
extern void (*fp_platform_mif_unregister_irq)(struct platform_mif *platform);
extern int  (*fp_platform_mif_register_irq)(struct platform_mif *platform);
extern void (*fp_platform_mif_destroy)(struct scsc_mif_abs *interface);
extern char *(*fp_platform_mif_get_uid)(struct scsc_mif_abs *interface);
extern int (*fp_platform_mif_start_wait_for_cfg_ack_completion)(struct scsc_mif_abs *interface);
extern int (*fp_platform_mif_pmu_reset_release)(struct scsc_mif_abs *interface);
extern int (*fp_platform_mif_reset)(struct scsc_mif_abs *interface, bool reset);
extern void __iomem *(*fp_platform_mif_map_region)(unsigned long phys_addr, size_t size);
extern void (*fp_platform_mif_unmap_region)(void *vmem);
extern void *(*fp_platform_mif_map)(struct scsc_mif_abs *interface, size_t *allocated);
extern void (*fp_platform_mif_unmap)(struct scsc_mif_abs *interface, void *mem);
extern u32 (*fp_platform_mif_irq_bit_mask_status_get)(struct scsc_mif_abs *interface, enum scsc_mif_abs_target target);
extern u32 (*fp_platform_mif_irq_get)(struct scsc_mif_abs *interface, enum scsc_mif_abs_target target);
extern void (*fp_platform_mif_irq_bit_set)(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target);
extern void (*fp_platform_mif_irq_bit_clear)(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target);
extern void (*fp_platform_mif_irq_bit_mask)(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target);
extern void (*fp_platform_mif_irq_bit_unmask)(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target);
extern void (*fp_platform_mif_remap_set)(struct scsc_mif_abs *interface, uintptr_t remap_addr, enum scsc_mif_abs_target target);
extern void (*fp_platform_mif_irq_reg_handler)(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data), void *dev);
extern void (*fp_platform_mif_irq_unreg_handler)(struct scsc_mif_abs *interface);
extern void (*fp_platform_mif_irq_reg_handler_wpan)(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data), void *dev);
extern void (*fp_platform_mif_irq_unreg_handler_wpan)(struct scsc_mif_abs *interface);
extern void (*fp_platform_mif_irq_reg_reset_request_handler)(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data), void *dev);
extern void (*fp_platform_mif_irq_unreg_reset_request_handler)(struct scsc_mif_abs *interface);
extern void (*fp_platform_mif_suspend_reg_handler)(struct scsc_mif_abs *interface,
		int (*suspend)(struct scsc_mif_abs *abs, void *data),
		void (*resume)(struct scsc_mif_abs *abs, void *data),
		void *data);
extern void (*fp_platform_mif_suspend_unreg_handler)(struct scsc_mif_abs *interface);
extern u32 *(*fp_platform_mif_get_mbox_ptr)(struct scsc_mif_abs *interface, u32 mbox_index, enum scsc_mif_abs_target target);
extern int (*fp_platform_mif_get_mbox_pmu)(struct scsc_mif_abs *interface);
extern int (*fp_platform_mif_set_mbox_pmu)(struct scsc_mif_abs *interface, u32 val);
extern int (*fp_platform_load_pmu_fw)(struct scsc_mif_abs *interface, u32 *ka_patch, size_t ka_patch_len);
extern void (*fp_platform_mif_irq_reg_pmu_handler)(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data), void *dev);
extern int (*fp_platform_mif_get_mifram_ref)(struct scsc_mif_abs *interface, void *ptr, scsc_mifram_ref *ref);
extern void *(*fp_platform_mif_get_mifram_ptr)(struct scsc_mif_abs *interface, scsc_mifram_ref ref);
extern void *(*fp_platform_mif_get_mifram_phy_ptr)(struct scsc_mif_abs *interface, scsc_mifram_ref ref);
extern uintptr_t (*fp_platform_mif_get_mif_pfn)(struct scsc_mif_abs *interface);
extern struct device *(*fp_platform_mif_get_mif_device)(struct scsc_mif_abs *interface);
extern void (*fp_platform_mif_irq_clear)(void);
extern int (*fp_platform_mif_read_register)(struct scsc_mif_abs *interface, u64 id, u32 *val);
extern void (*fp_platform_mif_cleanup)(struct scsc_mif_abs *interface);
extern void (*fp_platform_mif_restart)(struct scsc_mif_abs *interface);

static void test_platform_mif(struct kunit *test)
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
	platform->qos_enabled = false;
	fp_platform_mif_pm_qos_add_request(scscmif, &qos_req, 0);

	fp_platform_mif_pm_qos_update_request(scscmif, &qos_req, 0);
	platform->qos_enabled = true;
	fp_platform_mif_pm_qos_update_request(scscmif, &qos_req, 0);

	//fp_platform_mif_pm_qos_remove_request(scscmif, &qos_req);
	//platform->qos_enabled = false;
	//fp_platform_mif_pm_qos_remove_request(scscmif, &qos_req);

	fp_platform_recovery_disabled_reg(scscmif, NULL);

	fp_platform_recovery_disabled_unreg(scscmif);

	fp_platform_mif_irq_default_handler(0, 0);

	fp_platform_mif_irq_reset_request_default_handler(0, NULL);

	platform_wlbt_karamdump(scscmif);

	platform_mif_isr(0, platform);

	platform_mif_isr_wpan(0, platform);

	platform_wdog_isr(0, platform);

	platform_cfg_req_irq_clean_pending(platform);

	platform_set_wlbt_regs(platform);

	platform_cfg_req_isr(0, platform);

	fp_platform_mif_unregister_irq(platform);

	fp_platform_mif_register_irq(platform);

	fp_platform_mif_destroy(scscmif);

	fp_platform_mif_get_uid(scscmif);

	fp_platform_mif_start_wait_for_cfg_ack_completion(scscmif);

	fp_platform_mif_pmu_reset_release(scscmif);

	kunit_set_init_done(true);

	fp_platform_mif_pmu_reset_release(scscmif);

	fp_platform_mif_reset(scscmif, true);

	fp_platform_mif_reset(scscmif, false);

	fp_platform_mif_map_region(0x01, 0x01);
	fp_platform_mif_map_region(0x01, MIFRAMMAN_MAXMEM+1);

	fp_platform_mif_unmap_region(NULL);

	fp_platform_mif_map(scscmif, &allocated);

	fp_platform_mif_unmap(scscmif, NULL);

	fp_platform_mif_irq_bit_mask_status_get(scscmif, 0);

	fp_platform_mif_irq_get(scscmif, 0);

	fp_platform_mif_irq_bit_set(scscmif, 0, 0);
	fp_platform_mif_irq_bit_set(scscmif, 16, 0);

	fp_platform_mif_irq_bit_clear(scscmif, 0, 0);
	fp_platform_mif_irq_bit_clear(scscmif, 16, 0);

	fp_platform_mif_irq_bit_mask(scscmif, 0, 0);
	fp_platform_mif_irq_bit_mask(scscmif, 16, 0);

	fp_platform_mif_irq_bit_unmask(scscmif, 0, 0);
	fp_platform_mif_irq_bit_unmask(scscmif, 16, 0);

	fp_platform_mif_remap_set(scscmif, 0, SCSC_MIF_ABS_TARGET_WLAN);
	fp_platform_mif_remap_set(scscmif, 0, SCSC_MIF_ABS_TARGET_WPAN);
	fp_platform_mif_remap_set(scscmif, 0, SCSC_MIF_ABS_TARGET_WPAN+1);

	fp_platform_mif_irq_reg_handler(scscmif, NULL, NULL);

	fp_platform_mif_irq_unreg_handler(scscmif);

	fp_platform_mif_irq_reg_handler_wpan(scscmif, NULL, NULL);

	fp_platform_mif_irq_unreg_handler_wpan(scscmif);

	fp_platform_mif_irq_reg_reset_request_handler(scscmif, NULL, NULL);

	fp_platform_mif_irq_unreg_reset_request_handler(scscmif);

	fp_platform_mif_suspend_reg_handler(scscmif, NULL, NULL, NULL);

	fp_platform_mif_suspend_unreg_handler(scscmif);

	fp_platform_mif_get_mbox_ptr(scscmif, 0, SCSC_MIF_ABS_TARGET_WLAN);
	fp_platform_mif_get_mbox_ptr(scscmif, 0, SCSC_MIF_ABS_TARGET_WPAN);

	fp_platform_mif_wlbt_phandle_property_read_u32(scscmif, "ABCD", "BEEF", &val, 0);

	fp_platform_mif_get_mbox_pmu(scscmif);

	fp_platform_mif_set_mbox_pmu(scscmif, 0);

	fp_platform_load_pmu_fw(scscmif, "ABCD", 4);
	fp_platform_load_pmu_fw(scscmif, "ABCD", 4);

	fp_platform_mif_irq_reg_pmu_handler(scscmif, NULL, NULL);

	fp_platform_mif_get_mifram_ref(scscmif, NULL, NULL);
	fp_platform_mif_get_mifram_ref(scscmif, "ABCDE", NULL);

	fp_platform_mif_get_mifram_ptr(scscmif, NULL);

	fp_platform_mif_get_mifram_phy_ptr(scscmif, NULL);
	val = platform->mem_start;
	platform->mem_start=0;
	fp_platform_mif_get_mifram_phy_ptr(scscmif, NULL);
	platform->mem_start = val;

	u8 dst = 0;
	u16 size = 0;
	u32 value;
	char* string;
	int vsize = 0;

	fp_platform_mif_wlbt_property_read_bool(scscmif, "test");
	fp_platform_mif_wlbt_property_read_u8(scscmif, "test", &dst, vsize);
	fp_platform_mif_wlbt_property_read_u16(scscmif, "test", &size, vsize);
	fp_platform_mif_wlbt_property_read_u32(scscmif, "test", &value, vsize);
	fp_platform_mif_wlbt_property_read_string(scscmif, "test", &string, vsize);

	fp_platform_mif_get_mif_pfn(scscmif);

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
	KUNIT_CASE(test_platform_mif),
	{}
};

static struct kunit_suite test_suite[] = {
	{
		.name = "test_platform_mif_s5e8535",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("yongjin.lim@samsung.com>");

