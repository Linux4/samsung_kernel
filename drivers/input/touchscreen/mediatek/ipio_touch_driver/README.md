# Introduce

Ipio touch driver is implemented by ILI Technology Corp, which is mainly used on its new generation touch ICs, TDDI.

# Support ICs

The following lists which of TDDI IC types supported by the driver.

* ILI9881F
* ILI9881H
* ILI7807G

# Support platform

* Firefly-RK3288 (Rockchip)
* Helio X20 (MT6797)
* Qualcomm
* SPRD

The default in this driver works on Qualcomm platform. If you'd like to port other platforms with this driver, please modify the number shown on below to fit it:

**common.h**
```
/* A platform currently supported by driver */
#define PT_QCOM	1
#define PT_MTK	2
#define PT_SPRD	3
#define TP_PLATFORM PT_QCOM
```

# Interface
This driver supports SPI interface in the version of 1.0.3.0 above. If you'd like to change the interface between I2C and SPI:

```
/* A interface currently supported by driver */
#define I2C_INTERFACE 1
#define SPI_INTERFACE 2
#define INTERFACE SPI_INTERFACE
```

Note that SPI is only used on ILI9881H series.

# Functions

## Gesture

To enable the support of this feature, you can either open it by its node under /proc :

```
echo on > /proc/ilitek/gesture
echo off > /proc/ilitek/gesture
```

or change its variable from the file **config.c** :
```
core_config->isEnableGesture = false;
```

## Check battery status

Some specific cases with power charge may affect on our TDDI IC so that we need to protect it if the event is occurring.

In order to enable this function, the first you need to do is to enable its macro at **common.h**

```
/* Check battery's status in order to avoid some effects from charge. */
#define BATTERY_CHECK
```

Once the function has been built in, you must have to on it by echoing:

```
echo on > /proc/ilitek/check_battery
echo off > /procilitek/check_battery
```

Or you can run it by default by setting its flag as true.

```
ipd->isEnablePollCheckPower = true;
```

## DMA

If your platform needs to use DMA with I2C, you can open its macro from **common.h** :
```
#define ENABLE_DMA
```
Note, it is disabled as default.

## I2C R/W Segment

Some of platforms with i2c bus may have no ability to R/W data with large length at once, so we provide th way of segmental operation.

Before enable its macro, you first need to know how much length your platform could accept. In **core/i2c.c** where you can find its variable:

```
core_i2c->seg_len = 256; // length of segment
```

You can change its value as long as you know the length that your platform can work on.

After that, as mentioned before, you need to enable its macro, which defines at **common.h**, to actually let driver run its function.

```
/* Split the length written to or read from IC via I2C. */
#define I2C_SEGMENT
```

## FW upgrade at boot time
Apart from manual firmware upgrade, we also provide the way of upgrading firmware when system boots initially as long as the verions of firmware

in the IC is different the version that you're going to upgrade.

```
/* Be able to upgrade fw at boot stage */
#define BOOT_FW_UPGRADE
```

## Glove/Proximity/Phone cover

These features need to be opened by the node only.

```
echo enaglove > /proc/ilitek/ioctl  --> enale glove
echo disglove > /proc/ilitek/ioctl  --> disable glove
echo enaprox > /proc/ilitek/ioctl   --> enable proximity
echo disprox > /proc/ilitek/ioctl   --> disable proximity
echo enapcc > /proc/ilitek/ioctl    --> enable phone cover
echo dispcc > /proc/ilitek/ioctl    --> disable phone cover
```

# The metho of debug

To do so, you must firstlly ensure that the version of image is compiled for the debug instead of the user, otherwise it won't allow users to write commands through our device nodes.

## Debug message

It is important to see more details with debug messages if an error happends somehow. To look up them, you can set up debug level via a node called debug_level under /proc.

At the beginning, you should read its node by cat to see what the levels you can choose for.

```
cat /proc/ilitek/debug_level

DEBUG_NONE = 0
DEBUG_IRQ = 1
DEBUG_FINGER_REPORT = 2
DEBUG_FIRMWARE = 4
DEBUG_CONFIG = 8
DEBUG_I2C = 16
DEBUG_BATTERY = 32
DEBUG_MP_TEST = 64
DEBUG_IOCTL = 128
DEBUG_NETLINK = 256
DEBUG_ALL = -1
```

The default level is zero once you get the driver at the first time. Let's say that we want to check out what is going on when the driver is upgrading firmware.

```
echo 4 > /proc/ilitek/debug_level
```

The result will only print the debug message with the process of firmware. Furthermore, you can also add two or three numbers to see multiple debug levels.

```
echo 7 > /proc/ilitek/debug_level
```

In this case the debusg message prints the status of IRQ, FINGER_REPORT and FIRMWARE. Finally, you can definitly see all of them without any thoughts.

```
echo -1 > /proc/ilitek/debug_level
```

## I2C R/W

Sometime it is necessary to see IC's direct output by sending I2C commands in special cases.

We provide three functions to achieve it by echoing a specific device nodev which is created under /proc/ilitek/ioctl.

The command line defines as below :

```
# echo <function>,<length>,<data> > /proc/ilitek/ioctl
```

The comma must be added in each following command to let the driver recognize them.

In the function you can order specific actions to operate I2C such as Write (i2c_w), Read (i2c_r) and Write/Read (i2c_w_r). For instance, if you want to order write command, you can then type this as following:

```
# echo i2c_w,0x3,0x40,0x0,0x1
```

In this case the function is Write, the length is 3, and the i2c command is 0x40, 0x0 and 0x1. Note that it is important to add "0x" with every numbers that you want to cmmand except for the functions.

Additionly, we offer a useful feature to do Write/Read at once: Delay time. In this function it will do Write first and then delay a certain time before reading data through I2C. Note that the default of delay time is 1ms

```
# echo i2c_w_r, <length of write>, <length of read>, <delay time>, <data>
```

# File structure

```
├── common.h
├── core
│   ├── config.c
│   ├── config.h
│   ├── finger_report.c
│   ├── finger_report.h
│   ├── firmware.c
│   ├── firmware.h
│   ├── flash.c
│   ├── flash.h
│   ├── gesture.c
│   ├── gesture.h
│   ├── i2c.c
│   ├── i2c.h
│   ├── Makefile
│   ├── mp_test.c
│   ├── mp_test.h
│   ├── parser.c
│   ├── parser.h
│   ├── protocol.c
│   ├── protocol.h
│   ├── spi.c
│   └── spi.h
├── Makefile
├── platform.c
├── platform.h
├── README.md
└── userspace.c

```

* The concepts of this driver is suppose to hide details, and you should ignore the core functions and follow up the example code **platform.c**
to write your own platform c file or just modify it. 

* If you want to create more nodes on the device driver for user space, just refer to **userspace.c**.

* The directory **Core** includes our touch ic settings, functions and other features. If you'd like to write an application in user space, can just copy the directory to your workspace and call those functions by including header files.

# Porting

## Tell driver what IC types it should support to.

To void any undefined exeception, you should know what types of IC on the platform and what the current type of IC this driver supports for.

```
/* An Touch IC currently supported by driver */
#define CHIP_TYPE_ILI7807	0x7807
#define CHIP_TYPE_ILI9881	0x9881
#define TP_TOUCH_IC		CHIP_TYPE_ILI9881
```
In this case the driver now supports the type of ILI9881.

## DTS example code

```
&i2c1 {
		ts@41 {
			status = "okay";
        	compatible = "tchip,ilitek";
      		reg = <0x41>;
         	touch,irq-gpio = <&gpio8 GPIO_A7 IRQ_TYPE_EDGE_RISING>;
         	touch,reset-gpio = <&gpio8 GPIO_A6 GPIO_ACTIVE_LOW>;
    	};

};
```
In this case the slave address is 0x41, and the name of table calls **tchip,ilitek**. **touch,irq-gpio** and **touch,reset-gpio** represent INT pin and RESET pin separately.
