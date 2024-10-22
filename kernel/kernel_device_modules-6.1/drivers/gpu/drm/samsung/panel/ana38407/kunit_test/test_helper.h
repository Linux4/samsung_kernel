#ifndef __TEST_HELPER_H__
#define __TEST_HELPER_H__

#include <kunit/test.h>
#include <kunit/mock.h>

#define MOCK_SET_DEFAULT_ACTION_INVOKE_REAL(func_name) \
	mock_set_default_action(mock_get_global_mock(), \
			(#func_name), func_name, INVOKE_REAL(test, func_name))

#endif /* __TEST_HELPER_H__ */
