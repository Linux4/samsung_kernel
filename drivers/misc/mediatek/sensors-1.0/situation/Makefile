# In case the platform does NOT support this type of sensors

ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/hwmon/include

obj-y	+= situation.o
obj-$(CONFIG_MTK_INPKHUB)	+=	inpocket/
obj-$(CONFIG_MTK_STATHUB) 	+= 	stationary/
obj-$(CONFIG_MTK_WAKEHUB)	+=	wake_gesture/
obj-$(CONFIG_MTK_GLGHUB)	+=	glance_gesture/
obj-$(CONFIG_MTK_PICKUPHUB)	+=	pickup_gesture/
obj-$(CONFIG_MTK_ANSWER_CALL_HUB)		+=	answercall/
obj-$(CONFIG_MTK_DEVICE_ORIENTATION_HUB)	+=	device_orientation/
obj-$(CONFIG_MTK_MOTION_DETECT_HUB)			+=	motion_detect/
obj-$(CONFIG_MTK_TILTDETECTHUB)				+=	tilt_detector/
obj-$(CONFIG_MTK_FLAT_HUB)				+=	flat/
obj-$(CONFIG_MTK_SAR_HUB)				+=	sar/

#+S96818AA1-2216, liuling3.wt,ADD, 2023/06/05, add smart alert sensor
obj-$(CONFIG_N28_SMART_ALERT_HUB)       +=  smart_alert/
#-S96818AA1-2216, liuling3.wt,ADD, 2023/06/05, add smart alert sensor

#+S96818AA1-2213, liuling3.wt,ADD, 2023/06/15, add lift to wake sensor
obj-$(CONFIG_N28_LIFT_TO_WAKE)          +=  lift_to_wake/
#-S96818AA1-2213, liuling3.wt,ADD, 2023/06/15, add lift to wake sensor
