#include <kunit/test.h>
#include "../scsc_mif_abs.h"
#include "../mifqos.h"
#ifdef CONFIG_SOC_S5E8825
#include "../platform_mif_s5e8825.h"
#endif
#ifdef CONFIG_SOC_S5E8535
#include "../platform_mif_s5e8535.h"
#endif
#ifdef CONFIG_WLBT_REFACTORY
#include "../platform_mif.h"
#endif
#include <linux/platform_device.h>

static void test_all(struct kunit *test)
{
	struct platform_device pdev;
	struct device dev;
	struct scsc_mif_abs     *scscmif;
	struct mifqos           *qos;
	enum scsc_service_id    id = SCSC_SERVICE_ID_WLAN;

	pdev.dev = dev;
	set_test_in_platform_mif(test);
	set_property(0x99);

	scscmif = get_mif();

	qos = kunit_kzalloc(test, sizeof(*qos), GFP_KERNEL);
	mifqos_init(qos, scscmif);

	mifqos_set_affinity_cpu(qos, 0);

	mifqos_add_request(qos, id, 0);
	qos->qos_in_use[id] = true;

	mifqos_update_request(qos, id, 0);
	
	mifqos_deinit(qos);

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
		.name = "test_mifqos",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("seungjoo.ham@samsung.com>");
