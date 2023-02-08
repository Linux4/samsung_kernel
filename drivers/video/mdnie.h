#ifndef __MDNIE_H__
#define __MDNIE_H__

#define END_SEQ			0xffff

enum MODE {
	DYNAMIC,
	STANDARD,
	MOVIE,
	NATURAL,
	AUTO,
	MODE_MAX,
};

enum SCENARIO {
	UI_MODE,
	VIDEO_MODE,
	CAMERA_MODE = 4,
	NAVI_MODE,
	GALLERY_MODE,
	VT_MODE,
	BROWSER_MODE,
	EBOOK_MODE,
	EMAIL_MODE,
	SCENARIO_MAX,
	DMB_NORMAL_MODE = 20,
	DMB_MODE_MAX,
};

enum BYPASS {
	BYPASS_OFF,
	BYPASS_ON,
	BYPASS_MAX,
};

enum ACCESSIBILITY {
	ACCESSIBILITY_OFF,
	NEGATIVE,
	COLOR_BLIND,
	SCREEN_CURTAIN,
	GRAYSCALE,
	GRAYSCALE_NEGATIVE,
	ACCESSIBILITY_MAX,
};

enum HBM {
	HBM_OFF,
	HBM_ON,
	HBM_MAX,
};

enum OUTDOOR {
	OUTDOOR,
	OUTDOOR_TEXT,
	OUTDOOR_MAX,
};

struct mdnie_tuning_info {
	const char *name;
	unsigned short *sequence;
};

struct mdnie_info {
	struct clk		*bus_clk;
	struct clk		*clk;

	struct device		*dev;
	struct mutex		dev_lock;
	struct mutex		lock;

	unsigned int		enable;

	enum SCENARIO scenario;
	enum MODE mode;
	enum BYPASS bypass;
	enum HBM		hbm;

	unsigned int tuning;
	unsigned int accessibility;
	unsigned int color_correction;
	unsigned int auto_brightness;
	char path[50];

	struct notifier_block fb_notif;

};

extern struct mdnie_info *g_mdnie;

int s3c_mdnie_hw_init(void);
int s3c_mdnie_set_size(void);

void mdnie_s3cfb_resume(void);
void mdnie_s3cfb_suspend(void);

void mdnie_update(struct mdnie_info *mdnie, u8 force);

extern int mdnie_calibration(unsigned short x, unsigned short y, int *r);
extern int mdnie_request_firmware(const char *path, u16 **buf, const char *name);
extern int mdnie_open_file(const char *path, char **fp);

#endif /* __MDNIE_H__ */
