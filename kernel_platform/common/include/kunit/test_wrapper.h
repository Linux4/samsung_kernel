/* SPDX-License-Identifier: GPL-2.0 */
/*
 * add macros and functions to support 4.19 Kunit
 *
 */

#include <kunit/kunit-stream.h>
#include <kunit/strerror.h>
#include <linux/notifier.h>
#include <kunit/try-catch.h>

#define KUNIT_T kunit
#define KUNIT_RESOURCE_T kunit_resource
#define KUNIT_CASE_T kunit_case
#define KUNIT_SUITE_T kunit_suite

struct test_initcall {
	struct list_head node;
	int (*init)(struct test_initcall *this, struct kunit *test);
#if IS_ENABLED(CONFIG_UML)
	void (*exit)(struct test_initcall *this);
#else
	void (*exit)(struct test_initcall *this, struct kunit *test);
#endif /* CONFIG_UML */
};

void test_install_initcall(struct test_initcall *initcall);

#define test_pure_initcall(fn) postcore_initcall(fn)

#define test_register_initcall(initcall) \
		static int register_test_initcall_##initcall(void) \
		{ \
			test_install_initcall(&initcall); \
			\
			return 0; \
		} \
		test_pure_initcall(register_test_initcall_##initcall)

static inline struct kunit_stream *kunit_expect_start_ftn(struct kunit *test,
						    const char *file,
						    const char *line)
{
	struct kunit_stream *stream = alloc_kunit_stream(test, GFP_KERNEL);

	kunit_stream_add(stream, "EXPECTATION FAILED at %s:%s\n\t", file, line);

	return stream;
}

void kunit_set_failure(struct kunit *test);
static inline void kunit_expect_end_ftn(struct kunit *test,
				   bool success,
				   struct kunit_stream *stream)
{
	if (!success) {
		kunit_set_failure(test);
		kunit_stream_commit(stream);
	} else
		kunit_stream_clear(stream);
}

#define KUNIT_EXPECT_START(test) \
		kunit_expect_start_ftn(test, __FILE__, __stringify(__LINE__))

#define KUNIT_EXPECT_END(test, success, stream) kunit_expect_end_ftn(test, success, stream)

#define KUNIT_EXPECT(test, success, message) do {			\
	struct kunit_stream *__stream = KUNIT_EXPECT_START(test);	\
								\
	kunit_stream_add(__stream, message);			\
	KUNIT_EXPECT_END(test, success, __stream);			\
} while (0)

/**
 * KUNIT_EXPECT_NOT_NULL() - Causes a test failure when @expression is NULL.
 * @test: The test context object.
 * @expression: an arbitrary pointer expression. The test fails when this
 * evaluates to NULL.
 *
 * Sets an expectation that @expression does not evaluate to NULL. Similar to
 * EXPECT_TRUE() but supposed to be used with pointer expressions.
 */
#define KUNIT_EXPECT_NOT_NULL(test, expression)				       \
		KUNIT_EXPECT(test, (expression),				       \
		       "Expected " #expression " is not NULL, but is NULL.")

/**
 * KUNIT_EXPECT_NULL() - Causes a test failure when @expression is not NULL.
 * @test: The test context object.
 * @expression: an arbitrary pointer expression. The test fails when this does
 * not evaluate to NULL.
 *
 * Sets an expectation that @expression evaluates to NULL. Similar to
 * EXPECT_FALSE() but supposed to be used with pointer expressions.
 */
#define KUNIT_EXPECT_NULL(test, expression)					       \
		KUNIT_EXPECT(test, !(expression),				       \
		       "Expected " #expression " is NULL, but is not NULL.")

/**
 * KUNIT_EXPECT_SUCCESS() - Causes a test failure if @expression does not evaluate
 * to 0.
 * @test: The test context object.
 * @expression: an arbitrary expression evaluating to an int error code. The
 * test fails when this does not evaluate to 0.
 *
 * Sets an expectation that @expression evaluates to 0. Implementation assumes
 * that error codes are represented as negative values and if expression
 * evaluates to a negative value failure message will contain a mnemonic
 * representation of the error code (for example, for -1 it will contain EPERM).
 */
#define KUNIT_EXPECT_SUCCESS(test, expression) do {				       \
	struct kunit_stream *__stream = KUNIT_EXPECT_START(test);		       \
	typeof(expression) __result = (expression);			       \
	char buf[64];							       \
									       \
	if (__result != 0)						       \
		kunit_stream_add(__stream,					       \
			      "Expected " #expression " is not error, "	       \
			      "but is: %s.",				       \
			      strerror_r(-__result, buf, sizeof(buf)));	       \
	KUNIT_EXPECT_END(test, __result == 0, __stream);			       \
} while (0)

/**
 * KUNIT_EXPECT_ERROR() - Causes a test failure when @expression does not evaluate to @errno.
 * @test: The test context object.
 * @expression: an arbitrary expression evaluating to an int error code. The
 * test fails when this does not evaluate to @errno.
 * @errno: expected error value, error values are expected to be negative.
 *
 * Sets an expectation that @expression evaluates to @errno, so as opposed to
 * EXPECT_SUCCESS it verifies that @expression evaluates to an error.
 */
#define KUNIT_EXPECT_ERROR(test, expression, errno) do {			       \
	struct kunit_stream *__stream = KUNIT_EXPECT_START(test);		       \
	typeof(expression) __result = (expression);			       \
	char buf1[64];							       \
	char buf2[64];							       \
									       \
	if (__result != errno)						       \
		kunit_stream_add(__stream,					       \
			      "Expected " #expression " is %s, but is: %s.",   \
			      strerror_r(-errno, buf1, sizeof(buf1)),	       \
			      strerror_r(-__result, buf2, sizeof(buf2)));	       \
	KUNIT_EXPECT_END(test, __result == errno, __stream);			       \
} while (0)

static inline void kunit_expect_binary_ftn(struct kunit *test,
				      long long left, const char *left_name,
				      long long right, const char *right_name,
				      bool compare_result,
				      const char *compare_name,
				      const char *file,
				      const char *line)
{
	struct kunit_stream *stream = kunit_expect_start_ftn(test, file, line);

	kunit_stream_add(stream,
		    "Expected %s %s %s, but\n",
		    left_name, compare_name, right_name);
	kunit_stream_add(stream, "\t\t%s == %lld\n", left_name, left);
	kunit_stream_add(stream, "\t\t%s == %lld", right_name, right);

	kunit_expect_end_ftn(test, compare_result, stream);
}

/*
 * A factory macro for defining the expectations for the basic comparisons
 * defined for the built in types.
 *
 * Unfortunately, there is no common type that all types can be promoted to for
 * which all the binary operators behave the same way as for the actual types
 * (for example, there is no type that long long and unsigned long long can
 * both be cast to where the comparison result is preserved for all values). So
 * the best we can do is do the comparison in the original types and then coerce
 * everything to long long for printing; this way, the comparison behaves
 * correctly and the printed out value usually makes sense without
 * interpretation, but can always be interpretted to figure out the actual
 * value.
 */
#define KUNIT_EXPECT_BINARY(test, left, condition, right) do {		       \
	typeof(left) __left = (left);					       \
	typeof(right) __right = (right);				       \
	kunit_expect_binary_ftn(test,					       \
			   (long long) __left, #left,			       \
			   (long long) __right, #right,			       \
			   __left condition __right, #condition,	       \
			   __FILE__, __stringify(__LINE__));		       \
} while (0)

static inline struct kunit_stream *kunit_assert_start_ftn(struct kunit *test,
						    const char *file,
						    const char *line)
{
	struct kunit_stream *stream = alloc_kunit_stream(test, GFP_KERNEL);

	kunit_stream_add(stream, "ASSERTION FAILED at %s:%s\n\t", file, line);

	return stream;
}

static inline void kunit_assert_end_ftn(struct kunit *test,
				   bool success,
				   struct kunit_stream *stream)
{
	if (!success) {
		kunit_set_failure(test);
		kunit_stream_commit(stream);
	} else {
		kunit_stream_clear(stream);
	}
}

#define KUNIT_ASSERT_START(test) \
		kunit_assert_start_ftn(test, __FILE__, __stringify(__LINE__))

#define KUNIT_ASSERT_END(test, success, stream) kunit_assert_end_ftn(test, success, stream)

#define KUNIT_ASSERT(test, success, message) do {				       \
	struct kunit_stream *__stream = KUNIT_ASSERT_START(test);		       \
									       \
	kunit_stream_add(__stream, message);				       \
	KUNIT_ASSERT_END(test, success, __stream);				       \
} while (0)

/**
 * KUNIT_ASSERT_NOT_NULL() - Asserts that @expression does not evaluate to NULL.
 * @test: The test context object.
 * @expression: an arbitrary pointer expression. The test fails when this
 * evaluates to NULL.
 *
 * Asserts that @expression does not evaluate to NULL, see EXPECT_NOT_NULL().
 */
#define KUNIT_ASSERT_NOT_NULL(test, expression)				       \
		KUNIT_ASSERT(test, (expression),				       \
		       "Expected " #expression " is not NULL, but is NULL.")

/**
 * KUNIT_ASSERT_NULL() - Asserts that @expression evaluates to NULL.
 * @test: The test context object.
 * @expression: an arbitrary pointer expression. The test fails when this does
 * not evaluate to NULL.
 *
 * Asserts that @expression evaluates to NULL, see EXPECT_NULL().
 */
#define KUNIT_ASSERT_NULL(test, expression)					       \
		KUNIT_ASSERT(test, !(expression),				       \
		       "Expected " #expression " is NULL, but is not NULL.")

/**
 * KUNIT_ASSERT_SUCCESS() - Asserts that @expression is 0.
 * @test: The test context object.
 * @expression: an arbitrary expression evaluating to an int error code.
 *
 * Asserts that @expression evaluates to 0. It's the same as EXPECT_SUCCESS.
 */
#define KUNIT_ASSERT_SUCCESS(test, expression) do {				       \
	struct kunit_stream *__stream = KUNIT_ASSERT_START(test);		       \
	typeof(expression) __result = (expression);			       \
	char buf[64];							       \
									       \
	if (__result != 0)						       \
		kunit_stream_add(__stream,					       \
			      "Asserted " #expression " is not error, "	       \
			      "but is: %s.",				       \
			      strerror_r(-__result, buf, sizeof(buf)));	       \
	KUNIT_ASSERT_END(test, __result == 0, __stream);			       \
} while (0)

/**
 * KUNIT_ASSERT_ERROR() - Causes a test failure when @expression does not evaluate to
 * @errno.
 * @test: The test context object.
 * @expression: an arbitrary expression evaluating to an int error code. The
 * test fails when this does not evaluate to @errno.
 * @errno: expected error value, error values are expected to be negative.
 *
 * Asserts that @expression evaluates to @errno, similar to EXPECT_ERROR.
 */
#define KUNIT_ASSERT_ERROR(test, expression, errno) do {			       \
	struct kunit_stream *__stream = KUNIT_ASSERT_START(test);		       \
	typeof(expression) __result = (expression);			       \
	char buf1[64];							       \
	char buf2[64];							       \
									       \
	if (__result != errno)						       \
		kunit_stream_add(__stream,					       \
			      "Expected " #expression " is %s, but is: %s.",   \
			      strerror_r(-errno, buf1, sizeof(buf1)),	       \
			      strerror_r(-__result, buf2, sizeof(buf2)));	       \
	KUNIT_ASSERT_END(test, __result == errno, __stream);			       \
} while (0)

static inline void kunit_assert_binary_ftn(struct kunit *test,
				      long long left, const char *left_name,
				      long long right, const char *right_name,
				      bool compare_result,
				      const char *compare_name,
				      const char *file,
				      const char *line)
{
	struct kunit_stream *stream = kunit_assert_start_ftn(test, file, line);

	kunit_stream_add(stream,
		    "Asserted %s %s %s, but\n",
		    left_name, compare_name, right_name);
	kunit_stream_add(stream, "\t\t%s == %lld\n", left_name, left);
	kunit_stream_add(stream, "\t\t%s == %lld", right_name, right);

	kunit_assert_end_ftn(test, compare_result, stream);
}

/*
 * A factory macro for defining the expectations for the basic comparisons
 * defined for the built in types.
 *
 * Unfortunately, there is no common type that all types can be promoted to for
 * which all the binary operators behave the same way as for the actual types
 * (for example, there is no type that long long and unsigned long long can
 * both be cast to where the comparison result is preserved for all values). So
 * the best we can do is do the comparison in the original types and then coerce
 * everything to long long for printing; this way, the comparison behaves
 * correctly and the printed out value usually makes sense without
 * interpretation, but can always be interpretted to figure out the actual
 * value.
 */
#define KUNIT_ASSERT_BINARY(test, left, condition, right) do {		       \
	typeof(left) __left = (left);					       \
	typeof(right) __right = (right);				       \
	kunit_assert_binary_ftn(test,					       \
			   (long long) __left, #left,			       \
			   (long long) __right, #right,			       \
			   __left condition __right, #condition,	       \
			   __FILE__, __stringify(__LINE__));		       \
} while (0)

/*
 * NOTE: In order to support the backward compatibility, macros used in kunit/alpha are transferred to new ones.
 */

// struct kunit
struct test {
	void *priv;

	/* private: internal use only. */
	const char *name; /* Read only after initialization! */
	char *log; /* Points at case log after initialization */
	struct kunit_try_catch try_catch;
	/*
	 * success starts as true, and may only be set to false during a
	 * test case; thus, it is safe to update this across multiple
	 * threads using WRITE_ONCE; however, as a consequence, it may only
	 * be read after the test case finishes once all threads associated
	 * with the test case have terminated.
	 */
	bool success; /* Read only after test_case finishes! */
	spinlock_t lock; /* Guards all mutable test state. */
	/*
	 * Because resources is a list that may be updated multiple times (with
	 * new resources) from any thread associated with a test case, we must
	 * protect it with some type of lock.
	 */
	struct list_head resources; /* Protected by lock. */
	struct list_head post_conditions;
};
// struct kunit_case
struct test_case {
	void (*run_case)(struct test *test);
	const char *name;

	/* private: internal use only. */
	bool success;
	char *log;
};

// struct kunit_suite
struct test_module {
	const char name[256];
	int (*init)(struct test *test);
	void (*exit)(struct test *test);
	struct test_case *test_cases;

	/* private: internal use only */
	struct dentry *debugfs;
	char *log;
};

#define SUCCEED(test, ...) KUNIT_SUCCEED((struct kunit *)test, ##__VA_ARGS__)
#define FAIL(test, ...) KUNIT_FAIL((struct kunit *)test, ##__VA_ARGS__)

#define EXPECT(test, ...) KUNIT_EXPECT((struct kunit *)test, ##__VA_ARGS__)
#define EXPECT_TRUE(test, ...) KUNIT_EXPECT_TRUE((struct kunit *)test, ##__VA_ARGS__)
#define EXPECT_FALSE(test, ...) KUNIT_EXPECT_FALSE((struct kunit *)test, ##__VA_ARGS__)

#define EXPECT_NOT_NULL(test, ...) KUNIT_EXPECT_NOT_NULL((struct kunit *)test, ##__VA_ARGS__)
#define EXPECT_NULL(test, ...) KUNIT_EXPECT_NULL((struct kunit *)test, ##__VA_ARGS__)
#define EXPECT_SUCCESS(test, ...) KUNIT_EXPECT_SUCCESS((struct kunit *)test, ##__VA_ARGS__)
#define EXPECT_ERROR(test, ...) KUNIT_EXPECT_ERROR((struct kunit *)test, ##__VA_ARGS__)
#define EXPECT_BINARY(test, ...) KUNIT_EXPECT_BINARY((struct kunit *)test, ##__VA_ARGS__)

#define EXPECT_EQ(test, left, right) KUNIT_EXPECT_EQ((struct kunit *)test, (left), (typeof(left)) (right))
#define EXPECT_NE(test, left, right) KUNIT_EXPECT_NE((struct kunit *)test, (left), (typeof(left)) (right))
#define EXPECT_LT(test, left, right) KUNIT_EXPECT_LT((struct kunit *)test, (left), (typeof(left)) (right))
#define EXPECT_LE(test, left, right) KUNIT_EXPECT_LE((struct kunit *)test, (left), (typeof(left)) (right))
#define EXPECT_GT(test, left, right) KUNIT_EXPECT_GT((struct kunit *)test, (left), (typeof(left)) (right))
#define EXPECT_GE(test, left, right) KUNIT_EXPECT_GE((struct kunit *)test, (left), (typeof(left)) (right))
#define EXPECT_STREQ(test, left, right) KUNIT_EXPECT_STREQ((struct kunit *)test, (left), (typeof(left)) (right))

#define EXPECT_PTR_EQ(test, left, right) KUNIT_EXPECT_PTR_EQ((struct kunit *)test, (left), (right))
#define EXPECT_PTR_NE(test, left, right) KUNIT_EXPECT_PTR_NE((struct kunit *)test, (left), (right))

#define EXPECT_NOT_ERR_OR_NULL(test, ...) KUNIT_EXPECT_NOT_ERR_OR_NULL((struct kunit *)test, ##__VA_ARGS__)

#define ASSERT_TRUE(test, ...) KUNIT_ASSERT_TRUE((struct kunit *)test, ##__VA_ARGS__)
#define ASSERT_FALSE(test, ...) KUNIT_ASSERT_FALSE((struct kunit *)test, ##__VA_ARGS__)

#define ASSERT_NOT_NULL(test, ...) KUNIT_ASSERT_NOT_NULL((struct kunit *)test, ##__VA_ARGS__)
#define ASSERT_NULL(test, ...) KUNIT_ASSERT_NULL((struct kunit *)test, ##__VA_ARGS__)
#define ASSERT_SUCCESS(test, ...) KUNIT_ASSERT_SUCCESS((struct kunit *)test, ##__VA_ARGS__)
#define ASSERT_ERROR(test, ...) KUNIT_ASSERT_ERROR((struct kunit *)test, ##__VA_ARGS__)
#define ASSERT_BINARY(test, ...) KUNIT_ASSERT_BINARY((struct kunit *)test, ##__VA_ARGS__)
#define ASSERT_FAILURE(test, ...) KUNIT_ASSERT_FAILURE((struct kunit *)test, ##__VA_ARGS__)

#define ASSERT_EQ(test, left, right) KUNIT_ASSERT_EQ((struct kunit *)test, (left), (typeof(left)) (right))
#define ASSERT_NE(test, left, right) KUNIT_ASSERT_NE((struct kunit *)test, (left), (typeof(left)) (right))
#define ASSERT_LT(test, left, right) KUNIT_ASSERT_LT((struct kunit *)test, (left), (typeof(left)) (right))
#define ASSERT_LE(test, left, right) KUNIT_ASSERT_LE((struct kunit *)test, (left), (typeof(left)) (right))
#define ASSERT_GT(test, left, right) KUNIT_ASSERT_GT((struct kunit *)test, (left), (typeof(left)) (right))
#define ASSERT_GE(test, left, right) KUNIT_ASSERT_GE((struct kunit *)test, (left), (typeof(left)) (right))
#define ASSERT_STREQ(test, left, right) KUNIT_ASSERT_STREQ((struct kunit *)test, (left), (typeof(left)) (right))

#define ASSERT_PTR_EQ(test, left, right) KUNIT_ASSERT_PTR_EQ((struct kunit *)test, (left), (right))
#define ASSERT_PTR_NE(test, left, right) KUNIT_ASSERT_PTR_NE((struct kunit *)test, (left), (right))

#define ASSERT_NOT_ERR_OR_NULL(test, ...) KUNIT_ASSERT_NOT_ERR_OR_NULL((struct kunit *)test, ##__VA_ARGS__)

#define ASSERT_SIGSEGV(test, ...) do {} while (0)

#define test_info kunit_info
#define test_warn kunit_warn
#define test_err kunit_err
#define test_printk kunit_printk

// Note: the following functions don't quite have a 1:1 equivalent.
// * test_alloc_resource
// * test_free_resource
#define test_kmalloc(test, ...) kunit_kmalloc((struct kunit *)test, ##__VA_ARGS__)
#define test_kzalloc(test, ...) kunit_kzalloc((struct kunit *)test, ##__VA_ARGS__)
#define test_cleanup kunit_cleanup

// KUNIT_CASE
#define TEST_CASE(test_name) { .run_case = test_name, .name = #test_name }

// kunit_test_suite
#define module_test(suite)	kunit_test_suites((struct kunit_suite *)&suite)
// END
