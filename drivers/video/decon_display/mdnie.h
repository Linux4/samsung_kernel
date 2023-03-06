#ifndef __MDNIE_H__
#define __MDNIE_H__

#define END_SEQ	0xffff

#define MDNIE_LITE

#if defined(MDNIE_LITE)
typedef u8 mdnie_t;
#else
typedef u16 mdnie_t;
#endif

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
	GRAYSCALE,
	GRAYSCALE_NEGATIVE,
	ACCESSIBILITY_MAX
};

enum HBM {
	HBM_OFF,
	HBM_ON,
	HBM_MAX,
};

enum MDNIE_CMD {
#if defined(MDNIE_LITE)
	LEVEL1_KEY_UNLOCK,
	MDNIE_CMD1,
	MDNIE_CMD2,
	LEVEL1_KEY_LOCK,
	MDNIE_CMD_MAX,
#else
	MDNIE_CMD1,
	MDNIE_CMD2 = MDNIE_CMD1,
	MDNIE_CMD_MAX,
#endif
};

struct mdnie_command {
	mdnie_t *sequence;
	unsigned int size;
};

struct mdnie_table {
	char *name;
	struct mdnie_command tune[MDNIE_CMD_MAX];
};

#if defined(MDNIE_LITE)
#define MDNIE_SET(id)	\
{							\
	.name		= #id,				\
	.tune		= {				\
		{	.sequence = LEVEL1_UNLOCK, .size = ARRAY_SIZE(LEVEL1_UNLOCK)},	\
		{	.sequence = id##_1, .size = ARRAY_SIZE(id##_1)},		\
		{	.sequence = id##_2, .size = ARRAY_SIZE(id##_2)},		\
		{	.sequence = LEVEL1_LOCK, .size = ARRAY_SIZE(LEVEL1_LOCK)},	\
	}	\
}

struct mdnie_ops {
	int (*write)(void *data, const mdnie_t *seq, u32 len);
	int (*read)(void *data, u8 addr, mdnie_t *buf, u32 len);
};

typedef int (*mdnie_w)(void *devdata, const u8 *seq, u32 len);
typedef int (*mdnie_r)(void *devdata, u8 addr, u8 *buf, u32 len);
#else
#define MDNIE_SET(id)	\
{							\
	.name		= #id,				\
	.tune		= {				\
		{	.sequence = id, .size = ARRAY_SIZE(id)},	\
	}	\
}

struct mdnie_ops {
	int (*write)(unsigned int a, unsigned int v);
	int (*read)(unsigned int a);
};

typedef int (*mdnie_w)(unsigned int a, unsigned int v);
typedef int (*mdnie_r)(unsigned int a);
#endif

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
extern struct mdnie_table *mdnie_request_table(char *path, struct mdnie_table *s);

#endif /* __MDNIE_H__ */
