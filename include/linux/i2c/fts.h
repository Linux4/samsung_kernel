#ifndef _LINUX_FTS_I2C_H_
#define _LINUX_FTS_I2C_H_

#define FTS_SUPPORT_NOISE_PARAM

extern struct fts_callbacks *fts_charger_callbacks;
struct fts_callbacks {
	void (*inform_charger) (struct fts_callbacks *, int);
};

#ifdef FTS_SUPPORT_NOISE_PARAM
#define MAX_NOISE_PARAM 5
struct fts_noise_param {
	unsigned short pAddr[MAX_NOISE_PARAM];
	unsigned short pData[MAX_NOISE_PARAM];
};
#endif

struct fts_i2c_platform_data {
	bool factory_flatform;
	bool recovery_mode;
	bool support_hover;
	bool support_mshover;
	int max_x;
	int max_y;
	int max_width;
	unsigned char panel_revision;	/* to identify panel info */

	const char *firmware_name;
	const char *project_name;
	const char *model_name;
	const char *regulator_dvdd;
	const char *regulator_avdd;

	int (*power)(void *data, bool on);
	void (*register_cb)(void *);
	void (*enable_sync)(bool on);
	unsigned char (*get_ddi_type)(void);	/* to indentify ddi type */

	unsigned gpio;
	int irq_type;
};

#define SEC_TSP_FACTORY_TEST

// #define FTS_SUPPORT_TA_MODE // DE version don't need.

#ifdef SEC_TSP_FACTORY_TEST
extern struct class *sec_class;
#endif

extern unsigned int lcdtype;

void fts_charger_infom(bool en);
#endif
