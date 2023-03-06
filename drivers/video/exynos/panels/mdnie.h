#ifndef __MDNIE_H__
#define __MDNIE_H__

#define MDNIE_LITE

typedef u8 mdnie_t;

enum MODE {
	DYNAMIC,
	STANDARD,
	NATURAL,
	MOVIE,
	AUTO,
	MODE_MAX
};

enum SCENARIO {
	UI_MODE,
	VIDEO_NORMAL_MODE,
	CAMERA_MODE = 4,
	NAVI_MODE,
	GALLERY_MODE,
	VT_MODE,
	BROWSER_MODE,
	EBOOK_MODE,
	EMAIL_MODE,
	HMT_8_MODE,
	HMT_16_MODE,
	SCENARIO_MAX,
	DMB_NORMAL_MODE = 20,
	DMB_MODE_MAX,
};

enum BYPASS {
	BYPASS_OFF,
	BYPASS_ON,
	BYPASS_MAX
};

enum ACCESSIBILITY {
	ACCESSIBILITY_OFF,
	NEGATIVE,
	COLOR_BLIND,
	SCREEN_CURTAIN,
	ACCESSIBILITY_MAX
};

enum HBM {
	HBM_OFF,
	HBM_ON,
	HBM_ON_TEXT,
	HBM_MAX,
};
#ifdef CONFIG_LCD_HMT
enum hmt_mode {
	HMT_MDNIE_OFF,
	HMT_MDNIE_ON,
	HMT_3000K = HMT_MDNIE_ON,
	HMT_4000K,
	HMT_6400K,
	HMT_7500K,
	HMT_MDNIE_MAX,
};
#endif

enum MDNIE_CMD {
	LEVEL_KEY_UNLOCK,
	MDNIE_CMD1,
	MDNIE_CMD2,
	LEVEL_KEY_LOCK,
	MDNIE_CMD_MAX,
};

struct mdnie_seq_info {
	mdnie_t *cmd;
	unsigned int len;
	unsigned int sleep;
};

struct mdnie_table {
	char *name;
	struct mdnie_seq_info seq[MDNIE_CMD_MAX];
};

#define MDNIE_SET(id)	\
{							\
	.name		= #id,				\
	.seq		= {				\
		{	.cmd = LEVEL_UNLOCK,	.len = ARRAY_SIZE(LEVEL_UNLOCK),	.sleep = 0,},	\
		{	.cmd = id##_1,		.len = ARRAY_SIZE(id##_1),		.sleep = 0,},	\
		{	.cmd = id##_2,		.len = ARRAY_SIZE(id##_2),		.sleep = 0,},	\
		{	.cmd = LEVEL_LOCK,	.len = ARRAY_SIZE(LEVEL_LOCK),		.sleep = 0,},	\
	}	\
}

struct mdnie_ops {
	int (*write)(void *data, struct mdnie_seq_info *seq, u32 len);
	int (*read)(void *data, u8 addr, mdnie_t *buf, u32 len);
};

typedef int (*mdnie_w)(void *devdata, struct mdnie_seq_info *seq, u32 len);
typedef int (*mdnie_r)(void *devdata, u8 addr, u8 *buf, u32 len);

struct mdnie_info {
	struct clk		*bus_clk;
	struct clk		*clk;

	struct device		*dev;
	struct mutex		dev_lock;
	struct mutex		lock;

	unsigned int		enable;

	enum SCENARIO		scenario;
	enum MODE		mode;
	enum BYPASS		bypass;
	enum HBM		hbm;
#ifdef CONFIG_LCD_HMT
	enum hmt_mode	hmt_mode;
#endif
	unsigned int		tuning;
	unsigned int		accessibility;
	unsigned int		color_correction;
	unsigned int		auto_brightness;

	char			path[50];

	void			*data;

	struct mdnie_ops	ops;

	struct notifier_block	fb_notif;

	unsigned int white_r;
	unsigned int white_g;
	unsigned int white_b;
	struct mdnie_table table_buffer;
	mdnie_t sequence_buffer[256];
};

extern int mdnie_calibration(int *r);
extern int mdnie_open_file(const char *path, char **fp);
extern int mdnie_register(struct device *p, void *data, mdnie_w w, mdnie_r r);
#ifdef CONFIG_EXYNOS_DECON_DUAL_DISPLAY
extern int mdnie2_register(struct device *p, void *data, mdnie_w w, mdnie_r r);
#endif
extern struct mdnie_table *mdnie_request_table(char *path, struct mdnie_table *s);

#endif /* __MDNIE_H__ */
