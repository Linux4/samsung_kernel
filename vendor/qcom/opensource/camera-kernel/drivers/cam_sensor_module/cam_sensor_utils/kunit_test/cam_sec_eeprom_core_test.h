#ifndef _CAM_SEC_EEPROM_CORE_TEST_H_
#define _CAM_SEC_EEPROM_CORE_TEST_H_

#include "cam_eeprom_dev.h"
#include "cam_eeprom_core.h"
#include "cam_sec_eeprom_core.h"

int eeprom_test_init(struct kunit *test);
void eeprom_test_exit(struct kunit *test);
void eeprom_update_rear_module_info_test(struct kunit *test);
void eeprom_update_rear2_module_info_test(struct kunit *test);
void eeprom_update_rear3_module_info_test(struct kunit *test);
void eeprom_update_rear4_module_info_test(struct kunit *test);
void eeprom_update_front_module_info_test(struct kunit *test);
void eeprom_rear_match_crc_test(struct kunit *test);
void eeprom_rear2_match_crc_test(struct kunit *test);
void eeprom_rear3_match_crc_test(struct kunit *test);
void eeprom_rear4_match_crc_test(struct kunit *test);
void eeprom_front_match_crc_test(struct kunit *test);
void eeprom_calc_calmap_size_test(struct kunit *test);
void eeprom_get_custom_info_test(struct kunit *test);
void eeprom_fill_config_info_test(struct kunit *test);
void eeprom_link_module_info_rear_test(struct kunit *test);
void eeprom_link_module_info_rear2_test(struct kunit *test);
void eeprom_link_module_info_rear3_test(struct kunit *test);
void eeprom_link_module_info_rear4_test(struct kunit *test);
void eeprom_link_module_info_front_test(struct kunit *test);
void eeprom_link_module_info_front2_test(struct kunit *test);
void eeprom_link_module_info_front3_test(struct kunit *test);

#endif /* _CAM_SEC_EEPROM_CORE_TEST_H_ */
