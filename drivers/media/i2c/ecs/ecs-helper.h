#ifndef _ECS_HELPER_H_
#define _ECS_HELPER_H_

#include <linux/i2c.h>
#include <linux/delay.h>
#include "ecs-core.h"

struct reg_tab_bb {
		u8 reg;
		u8 val;
} __packed;

struct reg_tab_wb {
		u16 reg;
		u8 val;
} __packed;

struct reg_tab_db {
		u32 reg;
		u8 val;
} __packed;

#define ECS_I2C_IDENT_LENGTH	4

struct x_i2c {
	struct i2c_client *client;
	u16	ident_addr[ECS_I2C_IDENT_LENGTH];
	u8	ident_data[ECS_I2C_IDENT_LENGTH];
	u8	ident_mask[ECS_I2C_IDENT_LENGTH];
	u8	ident_regs;
	u8	reset_mask;
	u16	reset_addr;
	u16	delay_us;
	u8	reset_flag;
	int (*read)(struct x_i2c *xic, u16 reg, u8 *val);
	int (*write)(struct x_i2c *xic, u16 reg, u8 val);
	int (*write_array)(struct x_i2c *xic, const void *table, int count);
	int (*write_burst)(struct x_i2c *xic,
				u16 start_addr, const u8 *values, int cnt);
	int (*reset)(struct x_i2c *xic);
	int (*detect)(struct x_i2c *xic);
};

int xic_read_wb(struct x_i2c *xic, u16 reg, u8 *val);
int xic_write_wb(struct x_i2c *xic, u16 reg, u8 val);
int xic_write_array_wb(struct x_i2c *xic, const void *table, int count);
int xic_write_burst_wb(struct x_i2c *xic, u16 start_addr,
			const u8 *values, int cnt);
int xic_detect_wb(struct x_i2c *xic);

int xic_read_bb(struct x_i2c *xic, u16 reg, u8 *val);
int xic_write_bb(struct x_i2c *xic, u16 reg, u8 val);
int xic_write_array_bb(struct x_i2c *xic, const void *table, int count);
int xic_write_burst_bb(struct x_i2c *xic, u16 start_addr,
			const u8 *values, int cnt);
int xic_detect_bb(struct x_i2c *xic);
#endif
