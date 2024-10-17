#include <kunit/test.h>
#include "../mxman_if.h"
#include "../mxman.h"
#include "../srvman.h"

#define mxman_get_state(args...) (1)
#define mxman_get_last_panic_time(args...) (0xDEADBEEF)
#define mxman_get_panic_code(args...) (0xF)
#define mxman_show_last_panic(args...) ((void)0)
#define mxman_wait_for_completion_timeout(args...) (1)
#define mxman_fail(args...) ((void)0)
#define mxman_get_last_syserr_subsys(args...) (0xff)
#define mxman_force_panic(args...) (0)
#define mxman_lerna_send(args...) (0)
#define mxman_users_active(args...) (0)
#define mxman_set_syserr_recovery_in_progress(args...) ((void)0)
#define mxman_get_syserr_recovery_in_progress(args...) (true)


void kunit_test_mxman_if_get_state(struct kunit *test, struct mxman *mxman)
{
	int result;
	const int ans = 1;

	mxman->mxman_state = ans;
	result = mxman_if_get_state(mxman);

	KUNIT_EXPECT_EQ(test, result, ans);
}

void kunit_test_mxman_if_get_last_panic_time(struct kunit *test, struct mxman *mxman)
{
	int result;
	const unsigned int ans = 0xDEADBEEF;

	mxman->last_panic_time = ans;
	result = mxman_if_get_last_panic_time(mxman);

	KUNIT_EXPECT_EQ(test, result, ans);
}

void kunit_test_mxman_if_get_panic_code(struct kunit *test, struct mxman *mxman)
{
	int result;
	const int ans = 0xF;

	mxman->scsc_panic_code = ans;
	result = mxman_if_get_panic_code(mxman);

	KUNIT_EXPECT_EQ(test, result, ans);
}

void kunit_test_mxman_if_show_last_panic(struct mxman *mxman)
{
	mxman->scsc_panic_code = 0;
	mxman_if_show_last_panic(mxman);

	mxman->scsc_panic_code = 1;
	mxman_if_show_last_panic(mxman);
}

void kunit_test_mxman_wait_for_completion_timeout(struct kunit *test, struct mxman *mxman)
{
	int ret;

	init_completion(&mxman->recovery_completion);
	ret = mxman_if_wait_for_completion_timeout(mxman, 1);

	KUNIT_EXPECT_EQ(test, ret, 1);
}

void kunit_test_mxman_if_set_syserr_recovery_in_progress(struct kunit *test, struct mxman *mxman)
{
	bool ret;
	bool value = true;

	mxman_if_set_syserr_recovery_in_progress(mxman, value);
	ret = mxman_if_get_syserr_recovery_in_progress(mxman);

	KUNIT_EXPECT_EQ(test, ret, value);
}

// void kunit_test_mxman_if_set_last_syserr_recovery_time(struct kunit *test, struct mxman *mxman)
// {
// 	unsigned int ret;
// 	unsigned int value = 0xFF00;

// 	mxman_if_set_last_syserr_recovery_time(mxman, value);
// 	ret = mxman->last_syserr_recovery_time;

// 	KUNIT_EXPECT_EQ(test, ret, value);
// }

// void kunit_test_mxman_if_force_panic_fail_case_1(struct kunit *test, struct mxman *mxman)
// {
// 	int ret = mxman_if_force_panic(mxman, 0);

// 	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
// }

void kunit_test_mxman_if_force_panic_success_case_1(struct kunit *test, struct mxman *mxman)
{
	int sub = SCSC_SUBSYSTEM_WLAN_WPAN;
	int ret;

	mxman->mxman_state = MXMAN_STATE_STARTED_WLAN_WPAN;
	ret = mxman_if_force_panic(mxman, sub);

	KUNIT_EXPECT_EQ(test, ret, 0);
}

void kunit_test_mxman_if_force_panic_success_case_2(struct kunit *test, struct mxman *mxman)
{
	int sub = SCSC_SUBSYSTEM_WLAN;
	int ret;

	mxman->mxman_state = MXMAN_STATE_STARTED_WLAN;
	ret = mxman_if_force_panic(mxman, sub);

	KUNIT_EXPECT_EQ(test, ret, 0);
}

void kunit_test_mxman_if_force_panic_success_case_3(struct kunit *test, struct mxman *mxman)
{
	int sub = SCSC_SUBSYSTEM_WPAN;
	int ret;

	mxman->mxman_state = MXMAN_STATE_STARTED_WPAN;
	ret = mxman_if_force_panic(mxman, sub);

	KUNIT_EXPECT_EQ(test, ret, 0);
}

// void kunit_test_mxman_if_force_panic_fail_case_2(struct kunit *test, struct mxman *mxman)
// {
// 	int sub = SCSC_SUBSYSTEM_WLAN;
// 	int ret;

// 	mxman->mxman_state = MXMAN_STATE_STARTED_WPAN;
// 	ret = mxman_if_force_panic(mxman, sub);

// 	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
// }

// void kunit_test_mxman_if_force_panic_fail_case_3(struct kunit *test, struct mxman *mxman)
// {
// 	int sub = SCSC_SUBSYSTEM_WPAN;
// 	int ret;

// 	mxman->mxman_state = MXMAN_STATE_STARTED_WLAN;
// 	ret = mxman_if_force_panic(mxman, sub);

// 	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
// }

void kunit_test_mxman_if_force_panic(struct kunit *test, struct mxman *mxman)
{
	struct srvman *srvman;

	scsc_mx_alloc(mxman);
	srvman = scsc_mx_get_srvman(mxman->mx);
	mutex_init(&mxman->mxman_mutex);
	srvman_reset(mxman->mx);

	// kunit_test_mxman_if_force_panic_fail_case_1(test, mxman);

	srvman->error = false;
	srvman_init(srvman, mxman->mx);

	kunit_test_mxman_if_force_panic_success_case_1(test, mxman);
	kunit_test_mxman_if_force_panic_success_case_2(test, mxman);
	kunit_test_mxman_if_force_panic_success_case_3(test, mxman);
	// kunit_test_mxman_if_force_panic_fail_case_2(test, mxman);
	// kunit_test_mxman_if_force_panic_fail_case_3(test, mxman);

	mutex_destroy(&mxman->mxman_mutex);
}

void kunit_test_mxman_if_get_last_syserr_subsys(struct kunit *test, struct mxman *mxman)
{
	u16 ret;
	u16 ans = 0xff;

	mxman->last_syserr.subsys = ans;
	ret = mxman_if_get_last_syserr_subsys(mxman);

	KUNIT_EXPECT_EQ(test, ret, ans);
}

void kunit_test_mxman_if_lerna_send_success_case(struct kunit *test, struct mxman *mxman)
{

}

void kunit_test_mxman_if_lerna_send(struct kunit *test, struct mxman *mxman)
{
	int ret;
	char *dummy = "DUMMY";

	ret = mxman_if_lerna_send(mxman, (void *)dummy, sizeof(dummy));

	KUNIT_EXPECT_EQ(test, ret, 0);
}

void kunit_test_mxman_if_close(struct mxman *mxman)
{
	struct srvman *srvman = scsc_mx_get_srvman(mxman->mx);

	srvman->error = true;
	mxman->users = 0;
	mxman->users_wpan = 0;

	mxman_if_close(mxman, SCSC_SUBSYSTEM_WPAN); // fail case 1

	srvman->error = false;
	mxman->mxman_state = MXMAN_STATE_STARTED_WLAN;
	mxman_if_close(mxman, SCSC_SUBSYSTEM_WPAN); // fail case 2
	mxman_if_close(mxman, SCSC_SUBSYSTEM_WLAN); // success case 1

	mxman->mxman_state = MXMAN_STATE_STARTED_WPAN;
	mxman_if_close(mxman, SCSC_SUBSYSTEM_WLAN); // fail case 3
	mxman_if_close(mxman, SCSC_SUBSYSTEM_WPAN); // success case 2

	mxman->users = 0;
	mxman->users_wpan = 0;
	mxman->mxman_state = MXMAN_STATE_STARTED_WLAN_WPAN;
	mxman_if_close(mxman, SCSC_SUBSYSTEM_WLAN); // success case 3
	mxman_if_close(mxman, SCSC_SUBSYSTEM_WPAN); // success case 4

	mxman->mxman_state = MXMAN_STATE_FAILED_PMU;
	mxman_if_close(mxman, SCSC_SUBSYSTEM_WLAN);

	mxman->mxman_state = MXMAN_STATE_FAILED_WLAN;
	mxman_if_close(mxman, SCSC_SUBSYSTEM_WLAN);

	mxman->mxman_state = MXMAN_STATE_FAILED_WPAN;
	mxman_if_close(mxman, SCSC_SUBSYSTEM_WPAN);

	mxman->mxman_state = MXMAN_STATE_FAILED_WLAN_WPAN;
	mxman_if_close(mxman, SCSC_SUBSYSTEM_WLAN);

	mxman->mxman_state = MXMAN_STATE_FAILED_WLAN_WPAN;
	mxman_if_close(mxman, SCSC_SUBSYSTEM_WLAN);

	mxman->mxman_state = -1;
	mxman_if_close(mxman, SCSC_SUBSYSTEM_WLAN);
}

void kunit_test_mxman_if_users_active(struct kunit *test, struct mxman *mxman)
{
	int ret;

	ret = mxman_users_active(mxman);

	KUNIT_EXPECT_EQ(test, ret, 0);
}


void kunit_test_mxman_if_all(struct kunit *test)
{
	struct mxman *mxman = kunit_kzalloc(test, sizeof(struct mxman), GFP_KERNEL);

	kunit_test_mxman_if_get_state(test, mxman);
	kunit_test_mxman_if_get_last_panic_time(test, mxman);
	kunit_test_mxman_if_get_panic_code(test, mxman);
	kunit_test_mxman_if_show_last_panic(mxman);
	kunit_test_mxman_wait_for_completion_timeout(test, mxman);
	kunit_test_mxman_if_set_syserr_recovery_in_progress(test, mxman);
	// kunit_test_mxman_if_set_last_syserr_recovery_time(test, mxman);
	kunit_test_mxman_if_force_panic(test, mxman);
	kunit_test_mxman_if_get_last_syserr_subsys(test, mxman);
	// kunit_test_mxman_if_lerna_send(test, mxman);
	// kunit_test_mxman_if_close(mxman);
}

static int test_init(struct kunit *test)
{
	return 0;
}

static void test_exit(struct kunit *test)
{
}

static struct kunit_case test_cases[] = {
	KUNIT_CASE(kunit_test_mxman_if_all),
	KUNIT_CASE(kunit_test_mxman_if_users_active),
	{}
};

static struct kunit_suite test_suite[] = {
	{
		.name = "test_mxman_if",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("wan_hyuk.seo@samsung.com>");

