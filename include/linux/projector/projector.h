#ifndef	__PROJECTOR_H__
#define	__PROJECTOR_H__

#define USE_DPP3430

enum {
	IOCTL_PROJECTOR_SET_STATE = 1,
	IOCTL_PROJECTOR_GET_STATE,
	IOCTL_PROJECTOR_SET_ROTATION,
	IOCTL_PROJECTOR_GET_ROTATION,
	IOCTL_PROJECTOR_SET_BRIGHTNESS,
	IOCTL_PROJECTOR_GET_BRIGHTNESS,
	IOCTL_PROJECTOR_SET_FUNCTION,
	IOCTL_PROJECTOR_GET_FUNCTION
};

enum {
	PRJ_WRITE = 0,
	PRJ_READ,
  	PRJMSPD_WRITE,
  	PRJMSPD_READ,
  	PRJ_FWREAD,
  	PRJ_FWWRITE
};

enum {
	LCD_VIEW = 0,
	INTERNAL_PATTERN
};

enum {
	PRJ_OFF = 0,
	PRJ_ON_RGB_LCD,
	PRJ_ON_INTERNAL,
	RGB_LED_OFF,
	PRJ_MAX_STATE
};

enum {
	CHECKER = 0,
	WHITE,
	BLACK,
	LEDOFF,
	RED,
	GREEN,
	BLUE,
	BEAUTY,
	STRIPE,
	DIAGONAL,
	CURTAIN_ON = 10,
	CURTAIN_OFF,
// BEGIN PPNFAD a.jakubiak/m.wengierow
	CHECKER_16x9 = 20,
	SWITCH_TO_LCD = 60,
	SWITCH_TO_PATTERN = 61,
// END PPNFAD a.jakubiak/m.wengierow
};

/**
 * @private
 */
enum {
    SIOP_FLAG_FREEZE = 0,
    SIOP_FLAG_NO_FREEZE_NO_DUTY = 1,
    SIOP_FLAG_NO_FREEZE = 2
};

enum {
	PRJ_ROTATE_0 = 0,
	PRJ_ROTATE_90,
	PRJ_ROTATE_180,
	PRJ_ROTATE_270,
    PRJ_ROTATE_90_FULL,
    PRJ_ROTATE_270_FULL,
};

enum {
	ROTATION_LOCK = 0,
	CURTAIN_ENABLE,
	HIGH_AMBIENT_LIGHT
};

struct proj_val {
	int bRotationLock;
	int bCurtainEnable;
	int bHighAmbientLight;
};

struct projector_dpp3430_platform_data {
	unsigned	gpio_scl;
	unsigned	gpio_sda;
	unsigned	gpio_pi_int;
	unsigned	gpio_prj_on;
	unsigned	gpio_mp_on;
};

enum {
    MOTOR_CW = 0,
    MOTOR_CCW,
    MOTOR_DIRECTION,
    MOTOR_INRANGE,
    MOTOR_OUTOFRANGE,
    MOTOR_CONTINUE_CW = 5,
    MOTOR_CONTINUE_CCW,
    MOTOR_BREAK
};

enum {
	BRIGHT_HIGH = 1,    // 1.2W
	BRIGHT_MID,         // 0.9W
	BRIGHT_LOW,         // 
    BRIGHT_HIGH_A,      // 1.1W
    BRIGHT_HIGH_B,      // 1.0W 
	BRIGHT_MID_A,       // 0.8W
};

enum {
	LABB_OFF = 0x80,
	LABB_ON
};

enum {
	KEYSTONE_OFF = 0,
	KEYSTONE_ON
};


#ifdef CONFIG_HAS_EARLYSUSPEND
void projector_module_early_suspend(struct early_suspend *h);
void projector_module_late_resume(struct early_suspend *h);
#endif



int __devinit dpp3430_i2c_probe(struct i2c_client *client,	const struct i2c_device_id *id);
__devexit int dpp3430_i2c_remove(struct i2c_client *client);
int projector_module_open(struct inode *inode, struct file *file);
int projector_module_release(struct inode *inode, struct file *file);
int projector_module_ioctl(struct inode *inode, struct file *file,      unsigned int cmd, unsigned long arg);
int __init projector_module_init(void);
void __exit projector_module_exit(void);
#endif
