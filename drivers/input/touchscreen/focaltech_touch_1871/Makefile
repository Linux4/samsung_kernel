# Makefile for the focaltech touchscreen drivers.


obj-$(CONFIG_TOUCHSCREEN_FTS_1871)	+=  focaltech_core.o
obj-$(CONFIG_TOUCHSCREEN_FTS_1871)	+=  focaltech_ex_fun.o
obj-$(CONFIG_TOUCHSCREEN_FTS_1871)	+=  focaltech_ex_mode.o
obj-$(CONFIG_TOUCHSCREEN_FTS_1871)	+=  focaltech_gesture.o
obj-$(CONFIG_TOUCHSCREEN_FTS_1871)	+=  focaltech_esdcheck.o
obj-$(CONFIG_TOUCHSCREEN_FTS_1871)   +=  focaltech_point_report_check.o
# Huaqin add for HS70-142 ito test by gaozhengwei at 2019/10/08 start
obj-$(CONFIG_TOUCHSCREEN_FTS_1871)   +=  focaltech_test/
# Huaqin add for HS70-142 ito test by gaozhengwei at 2019/10/08 end

# Huaqin add for HS70-195 add cmd for get IC and binary FW version by gaozhengwei at 2019/10/15 start
obj-$(CONFIG_TOUCHSCREEN_FTS_1871)   +=  focaltech_sec.o
# Huaqin add for HS70-195 add cmd for get IC and binary FW version by gaozhengwei at 2019/10/15 end

obj-$(CONFIG_TOUCHSCREEN_FTS_1871)   +=  focaltech_spi.o
obj-$(CONFIG_TOUCHSCREEN_FTS_1871)	+=  focaltech_flash.o