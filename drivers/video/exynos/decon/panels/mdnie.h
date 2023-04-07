#ifndef __MDNIE_H__
#define __MDNIE_H__

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
	DMB_MODE_MAX
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
	COLOR_BLIND_HBM,
	ACCESSIBILITY_MAX
};

enum HBM {
	HBM_OFF,
	HBM_ON,
	HBM_ON_TEXT,
	HBM_MAX
};

enum COLOR_OFFSET_FUNC {
	COLOR_OFFSET_FUNC_NONE,
	COLOR_OFFSET_FUNC_F1,
	COLOR_OFFSET_FUNC_F2,
	COLOR_OFFSET_FUNC_F3,
	COLOR_OFFSET_FUNC_F4,
	COLOR_OFFSET_FUNC_MAX
};

enum hmt_mode {
	HMT_MDNIE_OFF,
	HMT_MDNIE_ON,
	HMT_3000K = HMT_MDNIE_ON,
	HMT_4000K,
	HMT_6400K,
	HMT_7500K,
	HMT_MDNIE_MAX
};

enum NIGHT_MODE {
	NIGHT_MODE_OFF,
	NIGHT_MODE_ON,
	NIGHT_MODE_MAX
};

struct mdnie_seq_info {
	mdnie_t *cmd;
	unsigned int len;
	unsigned int sleep;
};

struct mdnie_table {
	char *name;
	unsigned int update_flag[5];
	struct mdnie_seq_info seq[5 + 1];
};

struct mdnie_scr_info {
	u32 index;
	u32 cr;
	u32 wr;
	u32 wg;
	u32 wb;
};

struct mdnie_trans_info {
	u32 index;
	u32 offset;
	u32 enable;
};

struct mdnie_night_info {
	u32 max_w;
	u32 max_h;
};

struct mdnie_tune {
	struct mdnie_table	*bypass_table;
	struct mdnie_table	*accessibility_table;
	struct mdnie_table	*hbm_table;
	struct mdnie_table	*hmt_table;
	struct mdnie_table	(*main_table)[MODE_MAX];
	struct mdnie_table	*dmb_table;
	struct mdnie_table	*night_table;

	struct mdnie_scr_info	*scr_info;
	struct mdnie_trans_info	*trans_info;
	struct mdnie_night_info	*night_info;
	unsigned char **coordinate_table;
	unsigned char **adjust_ldu_table;
	unsigned char *night_mode_table;
	int (*get_hbm_index)(int);
	int (*color_offset[])(int, int);
};

struct rgb_info {
	int r;
	int g;
	int b;
};

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

	struct mdnie_tune	*tune;

	enum SCENARIO		scenario;
	enum MODE		mode;
	enum BYPASS		bypass;
	enum HBM		hbm;
	enum hmt_mode		hmt_mode;
	enum NIGHT_MODE		night_mode;

	unsigned int		tuning;
	unsigned int		accessibility;
	unsigned int		color_correction;
	unsigned int		coordinate[2];

	char			path[50];

	void			*data;

	struct mdnie_ops	ops;

	struct notifier_block	fb_notif;

	struct rgb_info		wrgb_current;

	unsigned int disable_trans_dimming;
	unsigned int night_mode_level;

	struct mdnie_table table_buffer;
	mdnie_t sequence_buffer[256];
#if defined(CONFIG_EXYNOS_DECON_MDNIE)
	mdnie_t *lpd_store_data;
	unsigned int need_update;
#endif
};

#if defined(CONFIG_EXYNOS_DECON_MDNIE)
extern struct mdnie_info* decon_mdnie_register(void);
extern void decon_mdnie_start(struct mdnie_info *mdnie, u32 w, u32 h);
extern void decon_mdnie_stop(struct mdnie_info *mdnie);
extern void decon_mdnie_frame_update(struct mdnie_info *mdnie, u32 xres, u32 yres);
extern u32 decon_mdnie_input_read(void);
#elif defined(CONFIG_EXYNOS_DECON_MDNIE_LITE)
extern int mdnie_register(struct device *p, void *data, mdnie_w w, mdnie_r r, unsigned int *coordinate, struct mdnie_tune *tune);
#endif

#if defined(CONFIG_EXYNOS_DECON_DUAL_DISPLAY) && defined(CONFIG_EXYNOS_DECON_MDNIE_LITE)
extern int mdnie2_register(struct device *p, void *data, mdnie_w w, mdnie_r r);
#endif
extern ssize_t attr_store_for_each(struct class *cls, const char *name, const char *buf, size_t size);
extern struct class *get_mdnie_class(void);

#endif /* __MDNIE_H__ */
