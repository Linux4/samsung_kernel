#ifndef _ECS_CORE_H_
#define _ECS_CORE_H_

/* Essential Camera Sensor: Header file */
/* Essential Camera Sensor driver (A.K.A. ECS_driver or x_driver) is a abstract
 * way to describe camera sensors. It provides an option to create a camera
 * sensor driver in an easy way: simply list all the sensor setting, register
 * them in a reasonable way, and use the helper function to implement all
 * nessarry function of any popular sensor driver interface (like v4l2-subdev),
 * and then, you get a driver */

#include <linux/types.h>
#include <media/v4l2-common.h>

#include <media/soc_camera.h>

struct ecs_setting {
	char	*name;
	/* As for property, each setting is refered to by a id number */
	int	id;
	/* Sensor config table */
	const void	*cfg_tab;
	/* Config table size, in bytes */
	int	cfg_sz;
	/* A pointer to save the feed back information location. The information
	 * tells camera controller how to adapt to this sensor config */
	void	*info;
};

struct ecs_property {
	char	*name;
	/* As for sensor, each property is refered to by a id number */
	int	id;
	/* a hook to the list that links all properties of a sensor */
	struct ecs_setting *stn_tab;
	int	stn_num;
	/* settings for this property must in a register range of
	 * [reg_low, reg_high), if reg_low==reg_high, range check is ignored */
	int	reg_low;
	int	reg_high;
	/* cam we just ignore it, when change property to its current value? */
	int	speculate;
	int	value_now;
	int	value_type;
	/* Following handler will download register values into sensor,
	 * and return the number of downloaded values */
	int (*cfg_handler)(void *hw_ctx, const void *table, int size);
	int (*value_equal)(void *a, void *b);
};

struct ecs_state_cfg {
	int	prop_id;
	int	prop_val;
};

struct ecs_state {
	char	*name;
	/* As for sensor, each state is refered to by a id number */
	int	id;
	/* a list to link all settings for this state,
	 * list member is struct ecs_state_cfg*/
	struct ecs_state_cfg *cfg_tab;
	int	cfg_num;
};

enum {
	ECS_HWIF_NONE	= 0,
	ECS_HWIF_I2C,
	ECS_HWIF_END,
};

enum {
	ECS_IF_NONE	= 0,
	ECS_IF_GENERIC,
	ECS_IF_SUBDEV,
	ECS_IF_END,
};

struct ecs_sensor {
	char	*name;
	/* a list to link all properties, list member is struct ecs_prop */
	struct ecs_property	*prop_tab;
	int	prop_num;
	/* a list to link all states, list member is struct ecs_state */
	struct ecs_state	*state_tab;
	int	state_num;
	int	state_now;
	int	speculate;

	void	*hw_ctx;
	/* Following handler will download register values into sensor,
	 * and return the number of downloaded values */
	int	(*cfg_handler)(void *hw_ctx, const void *list, int size);
};

/* recommended content of format setting info*/
struct ecs_default_fmt_info {
	enum	v4l2_mbus_pixelcode code;
	__u32	field;
	__u32	clrspc;
	int	fourcc;
};

/* recommended content of resolution setting info*/
struct ecs_default_res_info {
	int h_act;	/* output width */
	int v_act;	/* output height */
	int h_blk;	/* line blank */
	int v_blk;	/* frame blank */
};

int ecs_sensor_reset(struct ecs_sensor *snsr);
int ecs_sensor_merge(struct ecs_sensor *orig, const struct ecs_sensor *plat);
int ecs_sensor_init(struct ecs_sensor *snsr);
int ecs_setting_override(struct ecs_sensor *snsr, int prop, int stn,
				void *cfg_tab, int cfg_sz);
int ecs_property_override(struct ecs_sensor *snsr, int prop_id,
			struct ecs_setting *stn_tab,
			int (*cfg_handler)(void *, const void *, int));
int ecs_get_info(struct ecs_sensor *snsr, int prop_id, void **feedback);

int ecs_set_state(struct ecs_sensor *snsr, int state);
int ecs_set_list(struct ecs_sensor *snsr,
					struct ecs_state_cfg *list, int len);

#define __DECLARE_SETTING(SENSOR, sensor, PROP, prop, VAL, val) \
	[SENSOR##_##PROP##_##VAL] = { \
		.name		= #VAL, \
		.id		= SENSOR##_##PROP##_##VAL, \
		.cfg_tab	= sensor##_##prop##_##val, \
		.cfg_sz		= ARRAY_SIZE(sensor##_##prop##_##val), \
		.info		= NULL, \
	}

#define __DECLARE_SETTING_VS_INFO(SENSOR, sensor, PROP, prop, VAL, val) \
	[SENSOR##_##PROP##_##VAL] = { \
		.name		= #VAL, \
		.id		= SENSOR##_##PROP##_##VAL, \
		.cfg_tab	= sensor##_##prop##_##val, \
		.cfg_sz		= ARRAY_SIZE(sensor##_##prop##_##val), \
		.info		= &sensor##_##prop##_##info##_table \
					[SENSOR##_##PROP##_##VAL], \
	}

#define __DECLARE_STATE(SENSOR, sensor, VAL, val) \
	[SENSOR##_##ST##_##VAL] = { \
		.name		= #VAL, \
		.id		= SENSOR##_ST_##VAL, \
		.cfg_tab	= sensor##_state_##val, \
		.cfg_num	= ARRAY_SIZE(sensor##_state_##val) ,\
	}

#define __DECLARE_PROPERTY(SENSOR, sensor, PPT, ppt, NAME, REGH, REGL, SPEC) \
	[SENSOR##_##PROP##_##PPT] = { \
		.name		= #NAME, \
		.id		= SENSOR##_PROP_##PPT, \
		.stn_tab	= sensor##_##ppt##_stn_table, \
		.stn_num	= SENSOR##_##PPT##_##END, \
		.reg_low	= REGL, \
		.reg_high	= REGH, \
		.speculate	= SPEC, \
		.value_now	= UNSET, \
		.cfg_handler	= NULL, \
	}

#define __DECLARE_VIRTUAL_PROPERTY(SENSOR, PPT, NAME, SPEC) \
	[SENSOR##_##PROP##_##PPT] = { \
		.name		= #NAME, \
		.id		= SENSOR##_PROP_##PPT, \
		.stn_tab	= NULL, \
		.stn_num	= SENSOR##_##PPT##_##END, \
		.speculate	= SPEC, \
		.value_now	= UNSET, \
		.cfg_handler	= NULL, \
	}

#define ECS_DEFAULT_SNSR_CFG_HANDLER	(&ecs_reg_array_dump)
#define ECS_DEFAULT_CTLR_CFG_HANDLER	NULL

/* trace_level: */
/* 0 - only actual errors */
/* 1 - ecs internal error included */
/* 2 - all ecs log */
/* 3 - all ecs log and sensor behavior, performance impactable */
extern int ecs_trace;
#define x_printk(level, printk_level, fmt, arg...) \
	do { \
		if (ecs_trace >= level) \
			printk(printk_level fmt, ##arg); \
	} while (0)
#define x_inf(level, fmt, arg...) \
	x_printk(level, KERN_NOTICE, "x: " fmt "\n", ##arg)
#define x_log(level, fmt, arg...) \
	x_printk(level, KERN_DEBUG, "x: " fmt "\n", ##arg)
#define xinf(level, fmt, arg...) x_printk(level, KERN_NOTICE, fmt, ##arg)
#define xlog(level, fmt, arg...) x_printk(level, KERN_DEBUG, fmt, ##arg)

#endif
