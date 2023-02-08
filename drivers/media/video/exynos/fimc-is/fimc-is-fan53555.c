#include <linux/types.h>
#include <linux/input.h>
#include <linux/errno.h>
#include <linux/err.h>

#include "fimc-is-fan53555.h"

int fan53555_enable_vsel0(struct i2c_client *client, int on_off)
{
	int ret;

	ret = i2c_smbus_read_byte_data(client, REG_VSEL0);
	if(ret < 0){
		pr_err("%s: read error = %d , try again", __func__, ret);
		ret = i2c_smbus_read_byte_data(client, REG_VSEL0);
		if (ret < 0)
			pr_err("%s: read 2nd error = %d", __func__, ret);
	}

	ret &= (~VSEL0_BUCK_EN0);
	ret |= (on_off << VSEL0_BUCK_EN0_SHIFT);

	ret = i2c_smbus_write_byte_data(client, REG_VSEL0, (BYTE)ret);
	if (ret < 0){
		pr_err("%s: write error = %d , try again", __func__, ret);
		ret = i2c_smbus_write_byte_data(client, REG_VSEL0, (BYTE)ret);
		if (ret < 0)
			pr_err("%s: write 2nd error = %d", __func__, ret);
	}
	return ret;
}

int fan53555_set_vsel0_vout(struct i2c_client *client, int vout)
{
	int ret;

	ret = i2c_smbus_read_byte_data(client, REG_VSEL0);
	if(ret < 0){
		pr_err("%s: read error = %d , try again", __func__, ret);
		ret = i2c_smbus_read_byte_data(client, REG_VSEL0);
		if (ret < 0)
			pr_err("%s: read 2nd error = %d", __func__, ret);
	}

	ret &= (~VSEL0_NSEL0);
	ret |= vout;

	ret = i2c_smbus_write_byte_data(client, REG_VSEL0, (BYTE)ret);
	if (ret < 0){
		pr_err("%s: write error = %d , try again", __func__, ret);
		ret = i2c_smbus_write_byte_data(client, REG_VSEL0, (BYTE)ret);
		if (ret < 0)
			pr_err("%s: write 2nd error = %d", __func__, ret);
	}
	return ret;
}


