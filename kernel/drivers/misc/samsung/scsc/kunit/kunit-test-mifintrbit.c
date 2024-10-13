#include <kunit/test.h>
#include "../scsc_mif_abs.h"
#include "../mifintrbit.h"

extern void (*fp_mifintrbit_default_handler)(int irq, void *data);
extern void (*fp_print_bitmaps)(struct mifintrbit *intr);
extern void (*fp_mifiintrman_isr_wpan)(int irq, void *data);
extern void (*fp_mifiintrman_isr)(int irq, void *data);

static void test_all(struct kunit *test)
{
	struct mifintrbit *intr;

	intr = kunit_kzalloc(test, sizeof(*intr), GFP_KERNEL);

	mifintrbit_init(intr, get_mif(), SCSC_MIF_ABS_TARGET_WLAN);

	fp_mifintrbit_default_handler(0, intr);

	fp_print_bitmaps(intr);

	fp_mifiintrman_isr_wpan(0, intr);

	fp_mifiintrman_isr(0, intr);

	mifintrbit_deinit(intr, SCSC_MIF_ABS_TARGET_WLAN);

	mifintrbit_init(intr, get_mif(), SCSC_MIF_ABS_TARGET_WPAN);

	mifintrbit_deinit(intr, SCSC_MIF_ABS_TARGET_WPAN);

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
		.name = "test_mifintrbit",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("yongjin.lim@samsung.com>");

