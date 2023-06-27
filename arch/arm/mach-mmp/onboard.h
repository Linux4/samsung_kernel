#ifndef ONBOARD_H_
#define ONBOARD_H_

void __init dkb_add_lcd_tpo(void);
void __init dkb_add_lcd_truly(void);
void __init dkb_add_lcd_sharp(void);
void __init yellowstone_add_lcd_mipi(void);
void __init eden_fpga_add_lcd_mipi(void);
void __init mmp3_add_tv_out(void);
void __init emeidkb_add_lcd_mipi(void);
void __init t7_add_lcd_mipi(void);
void __init emeidkb_add_tv_out(void);
void __init harrison_add_lcd_mipi(void);
void __init lt02_add_lcd_mipi(void);
#if defined(CONFIG_TOUCHSCREEN_MXT336S)
void __init input_touchscreen_init(void);
#endif
#if defined(CONFIG_TOUCHSCREEN_CYPRESS_TMA46X)
void __init board_tsp_init(void);
#endif


#endif /* ONBOARD_H_ */
