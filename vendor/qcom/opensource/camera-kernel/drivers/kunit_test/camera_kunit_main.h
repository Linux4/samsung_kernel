#ifndef _CAMERA_KUNIT_MAIN_H_
#define _CAMERA_KUNIT_MAIN_H_

#include <kunit/test.h>
#include <kunit/mock.h>

extern struct kunit_case cam_kunit_test_cases[];
extern int cam_kunit_init(void);
extern void cam_kunit_exit(void);
extern void cam_ois_mcu_test(struct kunit *test);

#endif /* _CAMERA_KUNIT_MAIN_H_ */