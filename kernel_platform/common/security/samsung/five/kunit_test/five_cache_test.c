#include <kunit/test.h>
#include "five_cache.h"
#include "five_porting.h"

static void five_get_cache_status_test(struct test *test)
{
	struct integrity_iint_cache *iint;
	enum five_file_integrity status;

	iint = test_kzalloc(test, sizeof(struct integrity_iint_cache),
			    GFP_NOFS);
	ASSERT_NOT_ERR_OR_NULL(test, iint);
	memset(iint, 0xab, sizeof(struct integrity_iint_cache));
	iint->inode = test_kzalloc(test, sizeof(struct inode), GFP_NOFS);
	ASSERT_NOT_ERR_OR_NULL(test, iint->inode);
	memset(iint->inode, 0xcd, sizeof(struct inode));
	iint->version = inode_query_iversion(iint->inode);
	iint->five_status = FIVE_FILE_DMVERITY;

	status = five_get_cache_status(iint);
	EXPECT_EQ(test, status, FIVE_FILE_DMVERITY);

	status = five_get_cache_status(NULL);
	EXPECT_EQ(test, status, FIVE_FILE_UNKNOWN);

	iint->version = 555;
	status = five_get_cache_status(iint);
	EXPECT_EQ(test, status, FIVE_FILE_UNKNOWN);
}

static void five_set_cache_status_test(struct test *test)
{
	struct integrity_iint_cache *iint;
	enum five_file_integrity status = FIVE_FILE_DMVERITY;

	iint = test_kzalloc(test, sizeof(struct integrity_iint_cache),
			    GFP_NOFS);
	ASSERT_NOT_ERR_OR_NULL(test, iint);
	memset(iint, 0xab, sizeof(struct integrity_iint_cache));
	iint->inode = test_kzalloc(test, sizeof(struct inode), GFP_NOFS);
	ASSERT_NOT_ERR_OR_NULL(test, iint->inode);
	memset(iint->inode, 0xcd, sizeof(struct inode));

	five_set_cache_status(iint, status);
	EXPECT_EQ(test, iint->five_status, FIVE_FILE_DMVERITY);
	EXPECT_TRUE(test, inode_eq_iversion(iint->inode, iint->version));

	five_set_cache_status(NULL, status);
}

static int security_five_test_init(struct test *test)
{
	return 0;
}

static void security_five_test_exit(struct test *test)
{
	return;
}

static struct test_case security_five_test_cases[] = {
	TEST_CASE(five_get_cache_status_test),
	TEST_CASE(five_set_cache_status_test),
	{},
};

static struct test_module security_five_test_module = {
	.name = "five-cache-test",
	.init = security_five_test_init,
	.exit = security_five_test_exit,
	.test_cases = security_five_test_cases,
};
module_test(security_five_test_module);
