#include <kunit/test.h>
#include <kunit/mock.h>
/* #include <linux/example/example_driver.h> */

static void nfc_nfc_logger_test(struct test *test)
{
	/*
	struct example_driver_drvdata *p = test->priv;

	EXPECT_EQ(test, -EINVAL,  example_driver_set_intensity(p, 99999));
	EXPECT_FALSE(test, example_driver_set_enable(p, true));
	*/
	return;
}

static void nfc_nfc_logger_test2(struct test *test)
{
	/*
	struct example_driver_drvdata *p = test->priv;

	EXPECT_FALSE(test, example_driver_set_enable(p, false));
	EXPECT_EQ(test, -EINVAL,  example_set_frequency(p, EXAMPLE_FREQ_MAX));
	*/
	return;
}


static int nfc_nfc_logger_test_init(struct test *test)
{
	/*
	test->priv = a_struct_pointer;
	if (!test->priv)
		return -ENOMEM;
	*/

	return 0;
}

static void nfc_nfc_logger_test_exit(struct test *test)
{
	return;
}

static struct test_case nfc_nfc_logger_test_cases[] = {
	TEST_CASE(nfc_nfc_logger_test),
	TEST_CASE(nfc_nfc_logger_test2),
	{},
};

static struct test_module nfc_nfc_logger_test_module = {
	.name = "nfc-nfc-logger-test",
	.init = nfc_nfc_logger_test_init,
	.exit = nfc_nfc_logger_test_exit,
	.test_cases = nfc_nfc_logger_test_cases,
};
module_test(nfc_nfc_logger_test_module);
