#include <linux/module.h>
#include "ecs-helper.h"

/* ecs_OPER_WIDTH
 * OPER:	read
		write
 * WIDTH:	bb: 8-bit address, 8-bit data
		wb: 16-bit address, 8-bit data
		db: 32-bit address, 8-bit data
 */

static inline int ecs_read_wb(struct i2c_client *client, u16 reg, u8 *val)
{
	int ret;
	struct i2c_msg msg[] = {
		{
			.addr	= client->addr,
			.flags	= 0,
			.len	= 2,
			.buf	= (u8 *)&reg,
		},
		{
			.addr	= client->addr,
			.flags	= I2C_M_RD,
			.len	= 1,
			.buf	= val,
		},
	};

	reg = swab16(reg);

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret < 0) {
		dev_err(&client->dev, "Failed reading register 0x%04x!\n", reg);
		return ret;
	}

	return 0;
}

int xic_read_wb(struct x_i2c *xic, u16 reg, u8 *val)
{
	return ecs_read_wb(xic->client, reg, val);
}
EXPORT_SYMBOL(xic_read_wb);

/* write a register */
static inline int ecs_write_wb(struct i2c_client *client, u16 reg, u8 val)
{
	struct i2c_msg msg;
	struct reg_tab_wb buf;
	int ret;

	reg = swab16(reg);

	buf.reg = reg;
	buf.val = val;

	msg.addr	= client->addr;
	msg.flags	= 0;
	msg.len		= 3;
	msg.buf		= (u8 *)&buf;

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0) {
		dev_err(&client->dev, "Failed writing register 0x%04x!\n", reg);
		return ret;
	}

	return 0;
}

int xic_write_wb(struct x_i2c *xic, u16 reg, u8 val)
{
	return ecs_write_wb(xic->client, reg, val);
}
EXPORT_SYMBOL(xic_write_wb);

int xic_write_array_wb(struct x_i2c *xic, const void *table, int count)
{
	struct i2c_client *c = xic->client;
	const struct reg_tab_wb *vals = table;
	int ret, i = 0;

	for (i = 0; i < count; i++) {
		ret = ecs_write_wb(c, vals->reg, vals->val);
		if (unlikely(ret < 0)) {
			dev_err(&c->dev, "I2C returns error code 0x%X\n", ret);
			break;
		}
		vals++;
	}
	return i;
}
EXPORT_SYMBOL(xic_write_array_wb);

int xic_write_burst_wb(struct x_i2c *xic, u16 start_addr, \
			const u8 *values, int cnt)
{
	struct i2c_client *c = xic->client;
	u8 buffer[2 + cnt];
	int ret;
	u8 *pdata = buffer;

	start_addr = swab16(start_addr);
	memcpy(pdata, &start_addr, 2);
	pdata += 2;
	memcpy(pdata, values, cnt);
	ret = i2c_master_send(c, buffer, 2 + cnt);
	if (ret < 0)
		return ret;
	return 0;
}
EXPORT_SYMBOL(xic_write_burst_wb);

int xic_detect_wb(struct x_i2c *xic)
{
	struct i2c_client *c = xic->client;
	int ret, i = 0;
	u8 val;

	for (i = 0; i < xic->ident_regs; i++) {
		ret = ecs_read_wb(c, xic->ident_addr[i], &val);
		if (unlikely(ret < 0)) {
			dev_err(&c->dev, "I2C returns error code 0x%X\n", ret);
			return -ENXIO;
		}
		if (xic->ident_data[i] != (val & xic->ident_mask[i]))
			return -ENODEV;
	}
	return 0;
}
EXPORT_SYMBOL(xic_detect_wb);

struct x_i2c xic_default_wb = {
	.read		= xic_read_wb,
	.write		= xic_write_wb,
	.write_array	= xic_write_array_wb,
	.write_burst	= xic_write_burst_wb,
	.detect		= xic_detect_wb,
};
