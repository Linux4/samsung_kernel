#include <kunit/test.h>
#include <linux/platform_device.h>

extern void (*fp_platform_mif_module_probe_registered_clients)(struct scsc_mif_abs *mif_abs);
extern int (*fp_platform_mif_module_probe)(struct platform_device *pdev);
extern int (*fp_platform_mif_module_remove)(struct platform_device *pdev);
extern int (*fp_platform_mif_module_suspend)(struct device *dev);
extern int (*fp_platform_mif_module_resume)(struct device *dev);


static void test_all(struct kunit *test)
{
	struct scsc_mif_abs *scscmif;
	struct platform_device pdev;
	struct device dev;

	pdev.dev = dev;
	set_test_in_platform_mif(test);
	set_property(0x99);

	//fp_platform_mif_module_probe_registered_clients(NULL);

	fp_platform_mif_module_probe(&pdev);

	fp_platform_mif_module_remove(&pdev);

	fp_platform_mif_module_suspend(&pdev);

	fp_platform_mif_module_resume(&pdev);

	scsc_mif_abs_unregister(NULL);

	scsc_mif_mmap_register(NULL);

	scsc_mif_mmap_unregister(NULL);

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
		.name = "test_platform_mif_module",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("yongjin.lim@samsung.com>");

