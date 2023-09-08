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
#hs14 code for SR-AL6528A-01-779 by xiongxiaoliang at 2022/11/23 start
obj-$(CONFIG_MTK_WAKE_UP_MOTION)		+=	wakeup_motion/
#hs14 code for SR-AL6528A-01-779 by xiongxiaoliang at 2022/11/23 end
#hs14 code for SR-AL6528A-01-778 by xiongxiaoliang at 2022/11/26 start
obj-$(CONFIG_MTK_SMARTALERT_HUB)		+=	smart_alert/
#hs14 code for SR-AL6528A-01-778 by xiongxiaoliang at 2022/11/26 end
