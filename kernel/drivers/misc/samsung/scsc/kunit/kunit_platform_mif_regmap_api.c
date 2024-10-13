#include <kunit/test.h>
extern struct kunit *get_test_in_platform_mif(void);
#define devm_kzalloc(DEV, SIZE, OPTION)		kunit_kzalloc(get_test_in_platform_mif(), SIZE, OPTION)
#define platform_get_resource(DEV, OFFSET, OFFSET2)	kunit_platform_get_resource(OFFSET, OFFSET2)
#define devm_kfree(DEV, PTR)			((void *)0)
#define devm_request_irq(args...)		((void *)1)
#define devm_free_irq(args...)			((void *)0)
#define syscon_regmap_lookup_by_phandle(args...)	((void *)1)