#include <kunit/test.h>
#include <kunit/mock.h>
#include <linux/sti/abc_common.h>
#include <linux/platform_device_mock.h>

#define ABC_TEST_UEVENT_MAX		4
#define ABC_TEST_STR_MAX		50
#define ABC_TEST_TIMESTAMP_KEYWORD	"TIMESTAMP="

extern struct device *sec_abc;

struct abc_test {
	struct test *test;
};

static char abc_test_uevent_str[ABC_TEST_UEVENT_MAX][ABC_TEST_STR_MAX] = {"", };

ssize_t store_abc_enabled(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count);

void abc_test_get_uevent_str(char *uevent_str[])
{
	int i;

	for (i = 0; i < ABC_TEST_UEVENT_MAX; i++) {
		if (uevent_str[i]) {
			if (i >= 2 && !strncmp(uevent_str[i], ABC_TEST_TIMESTAMP_KEYWORD, strlen(ABC_TEST_TIMESTAMP_KEYWORD))) {
				strlcpy(abc_test_uevent_str[i], ABC_TEST_TIMESTAMP_KEYWORD, sizeof(abc_test_uevent_str[i]));
			} else {
				strlcpy(abc_test_uevent_str[i], uevent_str[i], sizeof(abc_test_uevent_str[i]));
			}
		}
	}
}

void abc_test_init_uevent_str()
{
	int i;
	for (i = 0; i < ABC_TEST_UEVENT_MAX; i++) {
		memset(abc_test_uevent_str[i], 0, sizeof(abc_test_uevent_str[i]));
	}
}

static void abc_test_enable_disable(struct test *test)
{
	store_abc_enabled(sec_abc, NULL, "1",0);
	msleep(100);
	EXPECT_EQ(test, 1, sec_abc_get_enabled());

	store_abc_enabled(sec_abc, NULL, "2",0);
	msleep(100);
	EXPECT_EQ(test, 2, sec_abc_get_enabled());

	store_abc_enabled(sec_abc, NULL, "0",0);
	msleep(100);
	EXPECT_EQ(test, 0, sec_abc_get_enabled());
}

static void abc_test_send_normal_event_in_disabled(struct test *test)
{
	store_abc_enabled(sec_abc, NULL, "0",0);
	msleep(100);
	abc_test_init_uevent_str();
	sec_abc_send_event("MODULE=abc_test@ERROR=normal_fault");
	msleep(500);
	EXPECT_STREQ(test, "", &abc_test_uevent_str[0][0]);
}

static void abc_test_send_normal_event_in_enabled(struct test *test)
{
	store_abc_enabled(sec_abc, NULL, "1",0);
	msleep(100);
	abc_test_init_uevent_str();
	sec_abc_send_event("MODULE=abc_test@ERROR=normal_fault");
	msleep(500);
	EXPECT_STREQ(test, "MODULE=abc_test", &abc_test_uevent_str[0][0]);
	EXPECT_STREQ(test, "ERROR=normal_fault", &abc_test_uevent_str[1][0]);
	EXPECT_STREQ(test, "TIMESTAMP=", &abc_test_uevent_str[2][0]);
	store_abc_enabled(sec_abc, NULL, "0",0);
}

static void abc_test_send_single_conditional_event_in_common_driver_mode(struct test *test)
{
	store_abc_enabled(sec_abc, NULL, "2",0);
	msleep(100);
	abc_test_init_uevent_str();
	sec_abc_send_event("MODULE=gpu@ERROR=gpu_fault");
	msleep(500);
	EXPECT_STREQ(test, "MODULE=gpu", &abc_test_uevent_str[0][0]);
	EXPECT_STREQ(test, "ERROR=gpu_fault", &abc_test_uevent_str[1][0]);
	EXPECT_STREQ(test, "TIMESTAMP=", &abc_test_uevent_str[2][0]);
	store_abc_enabled(sec_abc, NULL, "0",0);
}

static void abc_test_send_single_conditional_event_in_abc_driver_mode(struct test *test)
{
	store_abc_enabled(sec_abc, NULL, "1",0);
	msleep(100);
	abc_test_init_uevent_str();
	sec_abc_send_event("MODULE=gpu@ERROR=gpu_fault");
	msleep(500);
	EXPECT_STREQ(test, "", &abc_test_uevent_str[0][0]);
	store_abc_enabled(sec_abc, NULL, "0",0);
}

static void abc_test_send_multi_conditional_event_in_abc_driver_mode(struct test *test)
{
	int i;
	struct abc_info *pinfo = dev_get_drvdata(sec_abc);
	struct abc_qdata *pgpu = pinfo->pdata->gpu_items;
	int loop_cnt = pgpu->threshold_cnt;

	store_abc_enabled(sec_abc, NULL, "1",0);
	msleep(100);
	abc_test_init_uevent_str();

	for(i = 0; i < loop_cnt; i++) {
		sec_abc_send_event("MODULE=gpu@ERROR=gpu_fault");
		msleep(2);
	}
	msleep(500);

	EXPECT_STREQ(test, "MODULE=gpu", &abc_test_uevent_str[0][0]);
	EXPECT_STREQ(test, "ERROR=gpu_fault", &abc_test_uevent_str[1][0]);
	EXPECT_STREQ(test, "TIMESTAMP=", &abc_test_uevent_str[2][0]);
	store_abc_enabled(sec_abc, NULL, "0",0);
}

static int abc_test_init(struct test *test)
{
	abc_test_init_uevent_str();
	return 0;
}

static struct test_case abc_test_cases[] = {
	TEST_CASE(abc_test_enable_disable),
	TEST_CASE(abc_test_send_normal_event_in_disabled),
	TEST_CASE(abc_test_send_normal_event_in_enabled),
	TEST_CASE(abc_test_send_single_conditional_event_in_common_driver_mode),
	TEST_CASE(abc_test_send_single_conditional_event_in_abc_driver_mode),
	TEST_CASE(abc_test_send_multi_conditional_event_in_abc_driver_mode),
	{},
};

static struct test_module sec_abc_test_module = {
	.name = "sec_abc_test",
	.test_cases = abc_test_cases,
	.init = abc_test_init,
};

module_test(sec_abc_test_module);
