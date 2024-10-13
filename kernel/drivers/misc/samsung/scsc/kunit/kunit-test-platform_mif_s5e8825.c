#include <kunit/test.h>

#include "../platform_mif_s5e8825.h"
#include <linux/platform_device.h>

static void test_platform_mif(struct kunit *test)
{
	struct scsc_mif_abs *scscmif;
	struct platform_device pdev;
	struct device dev;
	struct platform_mif *platform;

	pdev.dev = dev;
	set_test(test);
	set_property(0x99);

	scscmif = platform_mif_create(&pdev);

	//set_property(0x00);
	//scscmif = platform_mif_create(&pdev);

	kunit_platform_recovery_disabled_reg(scscmif, NULL);

	kunit_platform_recovery_disabled_unreg(scscmif);

	kunit_platform_mif_irq_default_handler(0, 0);

	kunit_platform_mif_irq_reset_request_default_handler(0, NULL);

	platform = container_of(scscmif, struct platform_mif, interface);

	platform_mif_isr(0, platform);

	platform_mif_isr_wpan(0, platform);

	platform_wdog_isr(0, platform);

	platform_cfg_req_irq_clean_pending(platform);

	platform_set_wlbt_regs(platform);

	platform_cfg_req_isr(0, platform);

	kunit_platform_mif_unregister_irq(platform);

	kunit_platform_mif_register_irq(platform);

	kunit_platform_mif_destroy(scscmif);

	kunit_platform_mif_get_uid(scscmif);

	kunit_platform_mif_start_wait_for_cfg_ack_completion(scscmif);

	kunit_platform_mif_pmu_reset_release(scscmif);

	kunit_set_init_done(true);

	kunit_platform_mif_pmu_reset_release(scscmif);

	kunit_platform_mif_reset(scscmif, true);

	kunit_platform_mif_reset(scscmif, false);

	kunit_platform_mif_map_region(0x01, 0x01);

	kunit_platform_mif_unmap_region(NULL);

	kunit_platform_mif_map(scscmif, 0x00);

	kunit_platform_mif_unmap(scscmif, NULL);

	kunit_platform_mif_irq_bit_mask_status_get(scscmif, 0);

	kunit_platform_mif_irq_get(scscmif, 0);

	kunit_platform_mif_irq_bit_set(scscmif, 0, 0);

	kunit_platform_mif_irq_bit_clear(scscmif, 0, 0);

	kunit_platform_mif_irq_bit_mask(scscmif, 0, 0);

	kunit_platform_mif_irq_bit_unmask(scscmif, 0, 0);

	kunit_platform_mif_remap_set(scscmif, 0, 0);

	kunit_platform_mif_irq_reg_handler(scscmif, NULL, NULL);

	kunit_platform_mif_irq_unreg_handler(scscmif);

	kunit_platform_mif_irq_reg_handler_wpan(scscmif, NULL, NULL);

	kunit_platform_mif_irq_unreg_handler_wpan(scscmif);

	kunit_platform_mif_irq_reg_reset_request_handler(scscmif, NULL, NULL);

	kunit_platform_mif_irq_unreg_reset_request_handler(scscmif);

	kunit_platform_mif_suspend_reg_handler(scscmif, NULL, NULL, NULL);

	kunit_platform_mif_suspend_unreg_handler(scscmif);

	kunit_platform_mif_get_mbox_ptr(scscmif, 0, 0);

	kunit_platform_mif_get_mbox_pmu(scscmif);

	kunit_platform_mif_set_mbox_pmu(scscmif, 0);

	kunit_platform_load_pmu_fw(scscmif, "ABCD", 4);

	kunit_platform_mif_irq_reg_pmu_handler(scscmif, NULL, NULL);

	kunit_platform_mif_get_mifram_ref(scscmif, NULL, NULL);

	kunit_platform_mif_get_mifram_ptr(scscmif, NULL);

	kunit_platform_mif_get_mifram_phy_ptr(scscmif, NULL);

	kunit_platform_mif_get_mif_pfn(scscmif);

	kunit_platform_mif_get_mif_device(scscmif);

	kunit_platform_mif_irq_clear();

	kunit_platform_mif_read_register(scscmif, 0, NULL);

	kunit_platform_mif_cleanup(scscmif);

	kunit_platform_mif_restart(scscmif);

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
		.name = "test_platform_mif_s5e8825",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("yongjin.lim@samsung.com>");

