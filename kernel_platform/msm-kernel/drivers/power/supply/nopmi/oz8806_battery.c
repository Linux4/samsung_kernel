/*****************************************************************************
* Copyright(c) BMT, 2021. All rights reserved.
*       
* BMT [oz8806] Source Code Reference Design
* File:[oz8806_battery.c]
*       
* This Source Code Reference Design for BMT [oz8806] access 
* ("Reference Design") is solely for the use of PRODUCT INTEGRATION REFERENCE ONLY, 
* and contains confidential and privileged information of BMT International 
* Limited. BMT shall have no liability to any PARTY FOR THE RELIABILITY, 
* SERVICEABILITY FOR THE RESULT OF PRODUCT INTEGRATION, or results from: (i) any 
* modification or attempted modification of the Reference Design by any party, or 
* (ii) the combination, operation or use of the Reference Design with non-BMT 
* Reference Design. Use of the Reference Design is at user's discretion to qualify 
* the final work result.
*****************************************************************************/
#define pr_fmt(fmt)	"[bmt-fg] %s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/power_supply.h>
#include <linux/i2c.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/suspend.h>
#include <asm/div64.h>
//you must add these here for BMT
#include "parameter.h"
#include "table.h"
#include <asm/unaligned.h>

#include "oz8806_iio.h"
#include <linux/iio/consumer.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>

/*****************************************************************************
* static variables/functions section 
****************************************************************************/
static int32_t fg_hw_init_done = 0;
static uint8_t 	bmu_init_done = 0;
static int32_t oz8806_suspend = 0;
static DEFINE_MUTEX(update_mutex);
static bmu_data_t 	*batt_info_ptr = NULL;
static gas_gauge_t *gas_gauge_ptr = NULL;
static uint8_t charger_finish = 0;
static unsigned long ic_wakeup_time = 0;
static int32_t oz8806_update_batt_info(struct oz8806_data *data);
static int32_t oz8806_update_batt_temp(struct oz8806_data *data);
static int32_t oz8806_write_byte(struct oz8806_data *data, uint8_t index, uint8_t dat);
static int32_t oz8806_read_byte(struct oz8806_data *data, uint8_t index, uint8_t *dat);
struct oz8806_data *the_oz8806 = NULL;
static int8_t adapter_status = O2_CHARGER_BATTERY;
static int32_t rsoc_pre = 0;
#ifdef EXT_THERMAL_READ
static uint8_t ext_thermal_read = 1;
#else
static uint8_t ext_thermal_read = 0;
#endif

static int32_t est_cv_flag = 0;

//-1 means not ready
int32_t age_soh = 0;
int32_t cycle_soh = 0;
static bool charge_full = false;

 int FV_battery_cycle = 0;
 EXPORT_SYMBOL_GPL(FV_battery_cycle);

static enum power_supply_property oz8806_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_FULL,
};

enum {
	PLUGIN_CHARGING_TO_FULL = 0,
	PLUGIN_FULL,
};

int32_t sd77428_wakeup_chip(void)
{
	int32_t  ret1 = 0,ret2 = 0;
	uint8_t i = 0;

	for(i = 0; i < 5; i++)
	{
		ret1 = sd77428_write_addr(the_oz8806,0x400042A8,0x6318);//unlock
		ret2 = sd77428_write_addr(the_oz8806,0x400042AC,0x0000);//wakeup chip
		if((ret1 < 0) || (ret2 < 0))
			bmt_dbg("sd77428 write failed,ret1:%d,ret2:%d,i:%d!\n",ret1,ret2,i);
		else
		{
			bmt_dbg("sd77428 wakeup chip successed!\n");
			break;
		}
	}
	if(i >= 5)
	{
		bmt_dbg("sd77428 wakeup chip failed!\n");
		goto error;
	}
	sd77428_write_addr(the_oz8806,0x40004200,0x6318);//unlock
	sd77428_write_addr(the_oz8806,0x40004248,0x0000);//close sleep event
	sd77428_write_addr(the_oz8806,0x40004304,0xff);//clear sleep wakeup flag

	return 0;
error:
	pr_err("sd77428_wakeup_chip error\n");
	return -1;
}

static int Get_ChargerProp_To_ChangeSoc(struct oz8806_data *data, union power_supply_propval *soc_val)
{
	int ret = 0;
	union power_supply_propval online_val;
	union power_supply_propval status_val;

	if(data->bbc_psy == NULL) {
		data->bbc_psy = power_supply_get_by_name("bbc");
		if (data->bbc_psy == NULL) {
			pr_err("%s : fail to get psy ac\n", __func__);
			return -ENODEV;
		}
	}

	ret = power_supply_get_property(data->bbc_psy, POWER_SUPPLY_PROP_ONLINE, &online_val);
	ret = power_supply_get_property(data->bbc_psy, POWER_SUPPLY_PROP_STATUS, &status_val);
	if (ret < 0) {
		pr_err("%s : set ac psy type prop fail ret:%d\n", __func__, ret);
		return ret;
	}
	if(!online_val.intval)
	{
		data->bbc_status = PLUGIN_CHARGING_TO_FULL;
	} else{
		if(status_val.intval != POWER_SUPPLY_STATUS_FULL)
			data->bbc_status = PLUGIN_CHARGING_TO_FULL;

		if((data->bbc_status == PLUGIN_CHARGING_TO_FULL)
			&& (status_val.intval == POWER_SUPPLY_STATUS_FULL)
			&& (soc_val->intval == 100))
			data->bbc_status = PLUGIN_FULL;

		if(data->bbc_status == PLUGIN_FULL)
			soc_val->intval = 100;
	}
	pr_err("%s soc_val=%d,bbc_status:%d\n", __func__, soc_val->intval, data->bbc_status);
	return ret;
}
/*-------------------------------------------------------------------------*/
/*****************************************************************************
* Description:
*		below function is linux power section
* Parameters:
*		description for each argument, new argument starts at new line
* Return:
*		what does this function returned?
*****************************************************************************/
/*-------------------------------------------------------------------------*/

static int32_t oz8806_battery_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	struct oz8806_data *data = (struct oz8806_data *)power_supply_get_drvdata(psy);
	int32_t ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:

		if (adapter_status == O2_CHARGER_BATTERY)
		{
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING; /*discharging*/
		}
		else if(adapter_status == O2_CHARGER_USB ||
		        adapter_status == O2_CHARGER_AC )
		{
			if (data->batt_info.batt_soc == 100)
				val->intval = POWER_SUPPLY_STATUS_FULL;
			else
				val->intval = POWER_SUPPLY_STATUS_CHARGING;	/*charging*/
		}
		else
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		break;

	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = bmu_init_done;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = data->batt_info.batt_voltage * 1000;
		break;

	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = oz8806_get_battry_current();
		val->intval = ret * 1000;
		break;

	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = data->batt_info.batt_soc;
		Get_ChargerProp_To_ChangeSoc(data, val);
		break;

	case POWER_SUPPLY_PROP_TEMP:
		val->intval = data->batt_info.batt_temp * 10;
		break;

	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = data->batt_info.batt_fcc_data * 1000;
		break;

	case POWER_SUPPLY_PROP_CHARGE_NOW:
		val->intval = data->batt_info.batt_capacity;
		break;

        case POWER_SUPPLY_PROP_CHARGE_COUNTER:
                val->intval = oz8806_get_remaincap() * 1000;
                break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = oz8806_driver_name;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		val->intval = data->batt_info.batt_fcc_data * 1000;
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		val->intval = data->batt_info.cycle;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static void oz8806_powersupply_init(struct oz8806_data *data)
{
	data->bat_cfg.drv_data 			= data;
	data->bat_cfg.of_node  			= data->myclient->dev.of_node;

	data->bat_desc.name 				= "bms";
	data->bat_desc.type 				= POWER_SUPPLY_TYPE_BATTERY;
	data->bat_desc.properties 		= oz8806_battery_props;
	data->bat_desc.num_properties = ARRAY_SIZE(oz8806_battery_props);
	data->bat_desc.get_property 	= oz8806_battery_get_property;
	data->bat_desc.no_thermal 		= 1;
	data->bat_desc.external_power_changed = NULL;

	data->bat = power_supply_register(&data->myclient->dev, &data->bat_desc,&data->bat_cfg);
	if (!data->bat) 
		pr_err("failed to register power_supply battery\n");
}

// chip id: 0x38
static ssize_t oz8806_debug_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	return sprintf(buf,"%d\n", config_data.debug);
}
/*****************************************************************************
 * Description:
 *		oz8806_debug_store
 * Parameters:
 *		write example: echo 1 > debug ---open debug
 *****************************************************************************/
static ssize_t oz8806_debug_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t _count)
{
	int32_t val;
	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	if(val == 1)
	{
		config_data.debug = 1;
		pr_info("DEBUG ON \n");
	}
	else if (val == 0)
	{
		config_data.debug = 0;
		pr_info("DEBUG CLOSE \n");
	}
	else
	{
		pr_err("invalid command\n");
		return -EINVAL;
	}

	return _count;
}

static ssize_t oz8806_bmu_init_done_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	return sprintf(buf,"%d\n", bmu_init_done);
}
static ssize_t oz8806_bmu_init_done_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t _count)
{
	int32_t val = 0;
	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	bmu_init_done = 0;
	pr_info("start reinit\n");
	cancel_delayed_work(&the_oz8806->work);

	bmu_reinit();
	
	schedule_delayed_work(&the_oz8806->work, 0);
	return _count;
}

static ssize_t batt_cycle_soh_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int32_t val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	cycle_soh = val;
	accmulate_data_update();

	pr_info("batt_cycle_soh: %d ", cycle_soh);
	return count;
}

static ssize_t batt_cycle_soh_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	return sprintf(buf, "%d\n", cycle_soh);
}

static ssize_t batt_age_soh_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int32_t val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	age_soh = val;

	pr_info("batt_age_soh: %d ", age_soh);
	return count;
}

static ssize_t batt_age_soh_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	return sprintf(buf, "%d\n", age_soh);
}

static DEVICE_ATTR(bmt_debug, S_IRUGO | (S_IWUSR|S_IWGRP), oz8806_debug_show, oz8806_debug_store);
static DEVICE_ATTR(bmu_init_done, S_IRUGO| (S_IWUSR|S_IWGRP), oz8806_bmu_init_done_show, oz8806_bmu_init_done_store);
static DEVICE_ATTR(cycle_soh, S_IRUGO| (S_IWUSR|S_IWGRP), batt_cycle_soh_show, batt_cycle_soh_store);
static DEVICE_ATTR(age_soh, S_IRUGO| (S_IWUSR|S_IWGRP), batt_age_soh_show, batt_age_soh_store);
static struct attribute *oz8806_attributes[] = {
	&dev_attr_bmt_debug.attr,
	&dev_attr_bmu_init_done.attr,
	&dev_attr_cycle_soh.attr,
	&dev_attr_age_soh.attr,
	NULL,
};

static struct attribute_group oz8806_attribute_group = {
	.attrs = oz8806_attributes,
};

static int32_t oz8806_read_byte(struct oz8806_data *data, uint8_t index, uint8_t *dat)
{
	int32_t ret;
	uint8_t i;
	struct i2c_client *client = data->myclient;

	for(i = 0; i < 4; i++){
		ret = i2c_smbus_read_byte_data(client, index);

		if(ret >= 0) break;
		else
			dev_err(&client->dev, "%s: err %d, %d times\n", __func__, ret, i);
	}
	if(i >= 4)
	{
		return ret;
	} 
	*dat = (uint8_t)ret;

	return ret;
}

static int32_t oz8806_write_byte(struct oz8806_data *data, uint8_t index, uint8_t dat)
{
	int32_t ret;
	uint8_t i;
	struct i2c_client *client = data->myclient;

	for(i = 0; i < 4; i++){
		ret = i2c_smbus_write_byte_data(client, index, dat);
		if(ret >= 0) break;
		else
			dev_err(&client->dev, "%s: err %d, %d times\n", __func__, ret, i);
	}
	if(i >= 4)
	{
		return ret;
	}

	return ret;
}

int32_t oz8806_update_bits(struct oz8806_data *data, uint8_t reg, uint8_t mask, uint8_t dat)
{
	int32_t ret;
	uint8_t tmp;

	ret = oz8806_read_byte(data, reg, &tmp);
	if (ret < 0)
		return ret;

	if ((tmp & mask) != dat)
	{
		tmp &= ~mask;
		tmp |= dat & mask;
		return oz8806_write_byte(data, reg, tmp);

	}
	else
		return 0;
}
EXPORT_SYMBOL(oz8806_update_bits);

#define I2C_PEC_POLY                        0x07
#define SD77428_BLOCK_OP                    0x0F

int32_t sd77428_i2c_read_bytes(struct oz8806_data* chip, uint8_t cmd, uint8_t *val, uint8_t bytes)
{
    int32_t ret = 0;
    uint8_t i   = 0;
    usleep_range(1000,2000);

    mutex_lock(&chip->i2c_rw_lock);
    for(i=0; i<4; i++)
    {
        ret = i2c_smbus_read_i2c_block_data(chip->myclient, cmd, bytes, val);
        if(ret >= 0)
            break;
        else
        {
            pr_err("cmd[0x%02X], err %d, %d times\n", cmd, ret, i);
            usleep_range(10000,20000);//delay 10ms,retry
        }
    }
    mutex_unlock(&chip->i2c_rw_lock);

    return (i >= 4) ? -1 : 0;
}

int32_t sd77428_i2c_write_bytes(struct oz8806_data* chip,uint8_t cmd,uint8_t *val, uint8_t bytes)
{
    int32_t ret = 0;
    uint8_t i   = 0;
    usleep_range(1000,2000);

    mutex_lock(&chip->i2c_rw_lock);
    for(i=0; i<4; i++)
    {
        ret = i2c_smbus_write_i2c_block_data(chip->myclient, cmd, bytes, val);
        if(ret >= 0) 
            break;
        else
        {
            pr_err("cmd[0x%02X], err %d, %d times\n", cmd, ret, i);
            usleep_range(10000,20000);//delay 10ms,retry
        }
    }
    mutex_unlock(&chip->i2c_rw_lock);
    
    return (i >= 4) ? -1 : 0;
}

uint8_t calculate_pec_byte(uint8_t data, uint8_t crcin)  
{
    //Calculate CRC-8 value; uses polynomial input
    uint8_t crc8  = data ^ crcin;
    uint8_t index = 0;
    for (index = 0; index < 8; index++) {
        if (crc8 & 0x80)
            crc8 = (crc8 << 1) ^ I2C_PEC_POLY;
        else
            crc8 = (crc8 << 1);
    }
    return crc8;
}

uint8_t calculate_pec_bytes(uint8_t init_pec,uint8_t* pdata,uint8_t len)
{
    uint8_t i   = 0;
    uint8_t pec = init_pec;
    for(i=0;i<len;i++)
        pec = calculate_pec_byte(pdata[i],pec);

    return pec;
}

int32_t sd77428_read_word(struct oz8806_data *chip, uint8_t cmd, uint16_t* pdst)
{
	int32_t  ret = 0;
	uint8_t  rxbuf[32] = {0};
	uint8_t  i = 0;
	uint8_t  pec = 0;

	for(i=0;i<4;i++)
	{
		ret = sd77428_i2c_read_bytes(chip,cmd,rxbuf,3);
        
		if(0 == ret)
		{
            		pec = calculate_pec_byte(chip->myclient->addr << 1,pec);
            		pec = calculate_pec_byte(cmd,pec);
            		pec = calculate_pec_byte(chip->myclient->addr << 1 | 1,pec);
            
            		if(rxbuf[2] != calculate_pec_bytes(pec,rxbuf,2))
			{
  //              dev_err(chip->dev,"PEC verification fail cmd %02x,retry %d\n",cmd,i);
                		usleep_range(10000,20000);
                		pec = 0;
                		continue;
		}
            *pdst = get_unaligned_be16(rxbuf);
            break;
        }
    }
    return ret;
} 
int32_t sd77428_write_word(struct oz8806_data *chip,uint8_t index,uint16_t data)
{
    uint8_t w_buf[8] = {0};
    uint8_t pec = 0;
    w_buf[0] = (uint8_t)((data & 0xFF00) >> 8);
    w_buf[1] = (uint8_t)(data & 0x00FF);

    pec = calculate_pec_byte(chip->myclient->addr << 1,0);
    pec = calculate_pec_byte(index,pec);
    pec = calculate_pec_byte(w_buf[0],pec);
    pec = calculate_pec_byte(w_buf[1],pec);
    w_buf[2] = pec;

    return sd77428_i2c_write_bytes(chip,index, w_buf, 3); 
}

int32_t sd77428_read_block(struct oz8806_data *chip, uint8_t *data_buf,uint32_t len)
{
    int32_t  ret = -1;
    uint8_t  r_buf[132] = {0};
   uint8_t  i = 0;
    uint8_t  pec = 0;
    uint8_t     index = SD77428_BLOCK_OP;
    uint32_t read_len = (len >> 2) << 2;

    ret = sd77428_i2c_read_bytes(chip,index,r_buf,read_len+2);
    if (ret < 0) 
        return ret;

    pec = calculate_pec_byte(chip->myclient->addr << 1,pec);
    pec = calculate_pec_byte(index,pec);
    pec = calculate_pec_byte(chip->myclient->addr << 1 | 1,pec);

    if(r_buf[read_len+1] == calculate_pec_bytes(pec,r_buf,read_len+1))
    {
        for(i=0;i<read_len;i++)
            data_buf[i] = r_buf[i+1];
    }
    else
    {
        pr_err("PEC Error\n");
        return -1;
    }

    return ret;
}

int32_t sd77428_write_block(struct oz8806_data *chip,uint8_t *data_buf,uint32_t len)
{
    uint8_t w_buf[132] = {0};
    uint8_t pec = 0;
    uint32_t i = 0;
    uint32_t write_len = (len >> 2) << 2;

    w_buf[0] = write_len;

    for(i=0;i<w_buf[0];i++)
        w_buf[i+1] = data_buf[i];

    pec = calculate_pec_byte(chip->myclient->addr << 1,0);
    pec = calculate_pec_byte(SD77428_BLOCK_OP,pec);
    pec = calculate_pec_bytes(pec,w_buf,write_len+1);
    w_buf[write_len+1] = pec;

    return sd77428_i2c_write_bytes(chip,SD77428_BLOCK_OP, w_buf, write_len+2);  
}

int32_t sd77428_read_addr(struct oz8806_data *chip, uint32_t addr, uint32_t* pdata)
{
    uint8_t buf[4] = {0};
    uint16_t h_addr = (uint16_t)((addr & 0xFFFF0000) >> 16);
    uint16_t l_addr = (uint16_t) (addr & 0x0000FFFF);

    if(sd77428_write_word(chip,0x03,0x0004) < 0) 
        goto error;

    if(sd77428_write_word(chip,0x01,l_addr) < 0) 
        goto error;

    if(sd77428_write_word(chip,0x02,h_addr) < 0) 
        goto error;

    if(sd77428_read_block(chip,buf,4) < 0) 
        goto error; 
    //pr_info("%x %x %x %x\n",buf[0],buf[1],buf[2],buf[3]);
    *pdata = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];  

    return 0;

error:
    pr_err("error\n");
    return -1;
}

int32_t sd77428_write_addr(struct oz8806_data *chip,uint32_t addr, uint32_t value)
{
    uint8_t buf[4] = {0};
    uint16_t h_addr = (uint16_t)((addr & 0xFFFF0000) >> 16);
    uint16_t l_addr = (uint16_t) (addr & 0x0000FFFF);

    buf[0] = (uint8_t)((value & 0xFF000000) >> 24);
    buf[1] = (uint8_t)((value & 0x00FF0000) >> 16);
    buf[2] = (uint8_t)((value & 0x0000FF00) >> 8);
    buf[3] = (uint8_t)((value & 0x000000FF) >> 0);

    if(sd77428_write_word(chip,0x03,0x0004) < 0) 
        goto error;

    if(sd77428_write_word(chip,0x01,l_addr) < 0) 
        goto error;

    if(sd77428_write_word(chip,0x02,h_addr) < 0) 
        goto error;

    if(sd77428_write_block(chip,buf,4) < 0) 
        goto error;

    return 0;

error:
    pr_err("error\n");
    return -1;  
}
//--------------------------------------------ADC数据获取----------------------------------------------------------------//
static int32_t sd77428_unlock(struct oz8806_data *chip)
{
	if(sd77428_write_word(chip,0x11,0x6318) < 0) 
		goto error;

	if(sd77428_write_word(chip,0x12,0x6303) < 0) 
		goto error;

	return 0;

error:
	pr_err("error\n");
	return -1;
}

static int32_t sd77428_detect_ic(struct oz8806_data* chip)
{
	uint32_t chip_ver = 0;
	int32_t  ret = 0;
	ret = sd77428_read_addr(chip, 0x40004080,&chip_ver);
	if(ret)
		return -1;
	pr_info("chip version 0x%08X\n",chip_ver);
	//if(condition)
	return 0;
}

static int sd77428_hw_init(struct oz8806_data *chip)
{
	int32_t ret = 0;
	if(sd77428_write_word(chip,0x12,0x6303) < 0) //disable MCU, 关闭MCU, 只保留硬件ADC功能
		goto error;
	sd77428_wakeup_chip();
	ret = sd77428_write_addr(chip,0x400042A8,0x6318);
	if(ret)
		goto error;
	else
	{
		ret = sd77428_write_addr(chip,0x400042AC,0x0000);
		if(ret)
		{
			bmt_dbg("sd77428 poweron wake up chip failed!\n");
			goto error;
		}
	}

	//clear flag
	sd77428_write_addr(chip,0x40001004,0x0000);
	sd77428_write_addr(chip,0x40001008,21);
	sd77428_write_addr(chip,0x40001010,0x0001);
	sd77428_write_addr(chip,0x40004200,0x6318);//unlcok
	sd77428_write_addr(chip,0x40004248,0x0000);
	sd77428_write_addr(chip,0x40004304,0xff);

	sd77428_write_addr(chip,0x40004160,0x00000EC1);//使能auto scan intmp extmp cell1 1s auto

	sd77428_write_addr(chip,0x400041A0,0x00000081);//使能使能CADC

	//sd77428_write_addr(chip,0x400041B8,0x00004000);//这里直接把库仑计的值写了一半进来

	// sd77428_write_addr(chip,0x40004164,0);//6uA
	// sd77428_get_cc(chip, &m_sbs_ccRegAct);

	return 0;

error:
	pr_err("sd77428_hw_init error\n");
	return -1;
}
//----------------------------------------------------------------------------------------------------------------------//
//ext charge set the cv voltage
void update_charge_cv(int32_t charge_cv)
{
    if (charge_cv > 4500 || charge_cv < 4000 )
    {
        pr_info("charge cv voltage %d is unsafe, not enable.\n", charge_cv);
    }
    else
    {
        mutex_lock(&update_mutex);
        config_data.charge_cv_voltage = charge_cv;
        mutex_unlock(&update_mutex);
        pr_info("update charge cv voltage %d\n", config_data.charge_cv_voltage);
    }
}
EXPORT_SYMBOL(update_charge_cv);

// update charge cv base on cycle cnt
static int32_t est_cycle_cv()
{
    int32_t est_cv_voltage = 4350;
    int32_t cycle_cnt = 0;

    if (bmu_init_done && batt_info_ptr)
    {
    	if(FV_battery_cycle)
        	cycle_cnt = FV_battery_cycle;
      	else
        	cycle_cnt = batt_info_ptr->cycle_count;
      	pr_info("cycle_cnt %d \n", cycle_cnt);
    }
    else
    {
      	pr_info("bmu_init_done %d, or batt_info_ptr is null\n", bmu_init_done);
        return -1;
    }

    if (cycle_cnt <= 299)
    {
        est_cv_voltage = 4350;
    }
    else if (cycle_cnt >= 300 && cycle_cnt <= 399)
    {
        est_cv_voltage = 4330;
    }
    else if (cycle_cnt >= 400 && cycle_cnt <= 699)
    {
        est_cv_voltage = 4310;
    }
    else if (cycle_cnt >= 700 && cycle_cnt <= 999)
    {
        est_cv_voltage = 4290;
    }
    else
    {
        est_cv_voltage = 4240;
    }
    config_data.charge_cv_voltage = est_cv_voltage;
    pr_info("cycle_cnt %d, estimate charge cv voltage %d\n", cycle_cnt, config_data.charge_cv_voltage);
    return 0;
}
static void discharge_end_fun(struct oz8806_data *data)
{
	//End discharge
	//this may jump 2%
	if (!gas_gauge_ptr || !batt_info_ptr)
		return;

	if((batt_info_ptr->fVolt >= 2500) && (batt_info_ptr->fVolt < (config_data.discharge_end_voltage - 100)))
	{
        	if(batt_info_ptr->fRSOC == 1)
        	{
            		batt_info_ptr->fRSOC = 0;
            		batt_info_ptr->sCaMAH = data->batt_info.batt_fcc_data / num_100 -1;
            		discharge_end_process();
        	}
        	else if(batt_info_ptr->fRSOC > 0){
            		batt_info_ptr->sCaMAH = batt_info_ptr->fRSOC * data->batt_info.batt_fcc_data / num_100 - 1;
            		batt_info_ptr->fRSOC--;
		}
    }
}

static int32_t check_charger_full(void)
{
	pr_err("charge_full =%d\n",charge_full);
	//charger full from platform
	
	return charge_full;
}

static void charge_end_fun(void)
{
#define CHG_END_PERIOD_MS	(MSEC_PER_SEC * 30) //60 s = 1 minutes
#define FORCE_FULL_MS	(MSEC_PER_SEC * 120) //120s = 2 minutes
#define CHG_END_PURSUE_STEP    (MSEC_PER_SEC * 30) //120s

	static uint32_t time_accumulation;
	static unsigned long start_jiffies;
	int32_t oz8806_eoc = 0;
	static unsigned long chgr_full_soc_pursue_start;
	static uint32_t chgr_full_soc_pursue_accumulation;

#ifdef ENABLE_10MIN_END_CHARGE_FUN
	static unsigned long start_record_jiffies;
	static uint8_t start_record_flag = 0;
#endif

	if (!gas_gauge_ptr || !batt_info_ptr)
		return;

	if (adapter_status != O2_CHARGER_BATTERY && check_charger_full())
	{
		charger_finish = 1;
		pr_info("charger is full, enter external charger finish\n");
		goto charger_full;
	}

	if(adapter_status == O2_CHARGER_USB)
		oz8806_eoc = 100;
	else if(adapter_status == O2_CHARGER_AC)
		oz8806_eoc = config_data.charge_end_current;

	if((adapter_status == O2_CHARGER_BATTERY) ||
		(batt_info_ptr->fCurr < DISCH_CURRENT_TH) || 
		(batt_info_ptr->fCurr >  oz8806_eoc))
	{
		charger_finish   = 0;
		time_accumulation = 0;
		start_jiffies = 0;
		chgr_full_soc_pursue_start = 0;
#ifdef ENABLE_10MIN_END_CHARGE_FUN
		start_record_jiffies = 0;
		start_record_flag = 0;
#endif
		return;
	}

	//if((batt_info_ptr->fRSOC > 89) && (batt_info_ptr->fVolt >= (config_data.charge_cv_voltage - 100))&&(batt_info_ptr->fCurr >= DISCH_CURRENT_TH) &&
		//(batt_info_ptr->fCurr < oz8806_eoc)&& (!gas_gauge_ptr->charge_end))
  	if((batt_info_ptr->fVolt >= (config_data.charge_cv_voltage - 100))&&(batt_info_ptr->fCurr >= DISCH_CURRENT_TH) &&
        	(batt_info_ptr->fCurr < oz8806_eoc)&& (!gas_gauge_ptr->charge_end))
	{
		if (!start_jiffies)
			start_jiffies = jiffies;

		time_accumulation = jiffies_to_msecs(jiffies - start_jiffies);

		//time accumulation is over 5 minutes
		if (time_accumulation >= CHG_END_PERIOD_MS)
		{
			charger_finish	 = 1;
			pr_info("enter external charger finish\n");
		}
		else
		{
			charger_finish	= 0;
		}
	}
	else
	{
		time_accumulation = 0;
		start_jiffies = 0;
		charger_finish = 0;
	}

charger_full:
	pr_info("%s, time_accumulation:%d, charger_finish:%d\n",
			__func__, time_accumulation, charger_finish);

	pr_info("voltage:%d, cv:%d, fcurr:%d, BMT eoc:%d, gas_gauge_ptr->charge_end:%d\n",
			batt_info_ptr->fVolt, config_data.charge_cv_voltage,
			batt_info_ptr->fCurr, oz8806_eoc, gas_gauge_ptr->charge_end);

#ifdef ENABLE_10MIN_END_CHARGE_FUN
	if((batt_info_ptr->fRSOC == 99) &&(!start_record_flag) &&(batt_info_ptr->fCurr > oz8806_eoc))
	{
		start_record_jiffies = jiffies;
		start_record_flag = 1;
		pr_info("start_record_flag: %d, at %d ms\n",start_record_flag, jiffies_to_msecs(jiffies));
	}
	if((batt_info_ptr->fRSOC != 99) ||(batt_info_ptr->fCurr < oz8806_eoc))
	{
		start_record_flag = 0;
	}

	if((start_record_flag) && (batt_info_ptr->fCurr > oz8806_eoc))
	{
		if(jiffies_to_msecs(jiffies - start_record_jiffies) > FORCE_FULL_MS)
		{
			pr_info("start_record_flag: %d, at %d ms\n",start_record_flag, jiffies_to_msecs(jiffies));
			charger_finish	 = 1;
			start_record_flag = 0;
			pr_info("enter charge timer finish\n");
		}
	}
#endif
	if(charger_finish)
	{
		if(!gas_gauge_ptr->charge_end)
		{
			if(batt_info_ptr->fRSOC < 100)
			{
				static int32_t fRSOC_extern = 0;
				if(batt_info_ptr->fRSOC <= rsoc_pre){
					pr_info("fRSOC_extern:%d, soc:%d \n",fRSOC_extern,batt_info_ptr->fRSOC);

					if (!chgr_full_soc_pursue_start || fRSOC_extern != batt_info_ptr->fRSOC) {
						chgr_full_soc_pursue_start = jiffies;
						fRSOC_extern = batt_info_ptr->fRSOC ;
					}

					chgr_full_soc_pursue_accumulation =
						jiffies_to_msecs(jiffies - chgr_full_soc_pursue_start);

					if (chgr_full_soc_pursue_accumulation >= CHG_END_PURSUE_STEP){

						chgr_full_soc_pursue_start = 0;
						batt_info_ptr->fRSOC++;
						batt_info_ptr->sCaMAH = batt_info_ptr->fRSOC * the_oz8806->batt_info.batt_fcc_data / num_100;
						batt_info_ptr->sCaMAH +=  the_oz8806->batt_info.batt_fcc_data / 200;  // + 0.5%
					}
				}

				if(batt_info_ptr->fRSOC >= 100) {
					batt_info_ptr->fRSOC = 100;
					batt_info_ptr->sCaMAH = batt_info_ptr->fRSOC * the_oz8806->batt_info.batt_fcc_data / num_100;
					batt_info_ptr->sCaMAH ++;
				}
				//update fRSOC_extern
				fRSOC_extern = batt_info_ptr->fRSOC ;
				pr_info("enter charger finsh update soc:%d\n",batt_info_ptr->fRSOC);
			}
			else
			{
				pr_info("enter charger charge end\n");
				gas_gauge_ptr->charge_end = 1;
				charge_end_process();

				charger_finish = 0;
				chgr_full_soc_pursue_start = 0;
			}
		}
		else {
			charger_finish = 0;
			chgr_full_soc_pursue_start = 0;
		}
	} else chgr_full_soc_pursue_start = 0;

}
static void oz8806_wakeup_event(struct oz8806_data *data)
{
	static int32_t ws_active = 0;

	if (adapter_status == O2_CHARGER_BATTERY && ws_active) {
		pm_relax(&data->myclient->dev);
		ws_active = 0;
	}

	if (adapter_status != O2_CHARGER_BATTERY && !ws_active) {
		pm_stay_awake(&data->myclient->dev);
		ws_active = 1;
	}
}
//this is very important code customer need change
//customer should change charge discharge status according to your system
static void system_charge_discharge_status(struct oz8806_data *data)
{
	int8_t adapter_status_temp = O2_CHARGER_BATTERY;
	int ret =0;
	union power_supply_propval ac_prop = {0};
	union power_supply_propval usb_prop = {0};
	if (!data->ac_psy)
		data->ac_psy = power_supply_get_by_name ("ac"); 

	if (!data->usb_psy)
		data->usb_psy= power_supply_get_by_name ("usb");
		
	if (data->usb_psy)
	{
		ret = power_supply_get_property(data->usb_psy,POWER_SUPPLY_PROP_ONLINE, &usb_prop);
		if(usb_prop.intval)
			adapter_status_temp = O2_CHARGER_USB;
	}

	if (data->ac_psy)
	{
                ret = power_supply_get_property(data->ac_psy,POWER_SUPPLY_PROP_ONLINE, &ac_prop);
                if(ac_prop.intval)
			adapter_status_temp = O2_CHARGER_AC;
	}
		
	adapter_status = adapter_status_temp;
	pr_err("adapter_status:%d", adapter_status);
}

static void oz8806_battery_func(struct oz8806_data *data)
{
	unsigned long time_since_last_update_ms = 0;
	static unsigned long cur_jiffies = 0;

	if(0 == cur_jiffies)
		cur_jiffies = jiffies;

	time_since_last_update_ms = jiffies_to_msecs(jiffies - cur_jiffies);
	cur_jiffies = jiffies;

	//get charging type: battery, AC, or USB
	system_charge_discharge_status(data);

	rsoc_pre = data->batt_info.batt_soc;

	//you must add this code here for BMT
	//Notice: don't nest mutex
#ifdef EXT_THERMAL_READ
	oz8806_update_batt_temp(data);
#endif
	/**************mutex_lock*********************/
	mutex_lock(&update_mutex);
  
      	if (!est_cv_flag)
        {
            if (est_cycle_cv() >= 0 )
            {
                est_cv_flag = 1;
            }
        }
  
	if (!oz8806_suspend)
		bmu_polling_loop();

	oz8806_update_batt_info(data);

	charge_end_fun();

	if(adapter_status == O2_CHARGER_BATTERY)
		discharge_end_fun(data);

	mutex_unlock(&update_mutex);
	/**************mutex_unlock*********************/
	oz8806_wakeup_event(data);
	pr_info("l=%d v=%d t=%d c=%d ch=%d\n",
			data->batt_info.batt_soc, data->batt_info.batt_voltage, 
			data->batt_info.batt_temp, data->batt_info.batt_current, adapter_status);

	power_supply_changed(data->bat);
	bmt_dbg("since last batt update = %lu ms\n", time_since_last_update_ms);
}

static void oz8806_battery_work(struct work_struct *work)
{
	struct oz8806_data *data = container_of(work, struct oz8806_data, work.work);

	oz8806_battery_func(data);

	bmt_dbg("interval:%d ms\n", data->interval);

	if (!bmu_init_done || data->batt_info.batt_voltage < 2500) 
		schedule_delayed_work(&data->work, msecs_to_jiffies(3000));
	else 
		schedule_delayed_work(&data->work, msecs_to_jiffies(data->interval));
}

static int32_t oz8806_suspend_notifier(struct notifier_block *nb,
				unsigned long event,
				void *dummy)
{
	struct oz8806_data *data = container_of(nb, struct oz8806_data, pm_nb);

	switch (event) {

	case PM_SUSPEND_PREPARE:
		pr_info("BMT PM_SUSPEND_PREPARE \n");
		cancel_delayed_work_sync(&data->work);
		system_charge_discharge_status(data);
		oz8806_suspend = 1;
		return NOTIFY_OK;
	case PM_POST_SUSPEND:
		pr_info("BMT PM_POST_SUSPEND \n");
		system_charge_discharge_status(data);
		mutex_lock(&update_mutex);
		// if AC charge can't wake up every 1 min,you must remove the if.
		if(adapter_status == O2_CHARGER_BATTERY)
		{
			bmu_wake_up_chip();
		}

		oz8806_update_batt_info(data);
		mutex_unlock(&update_mutex);

		schedule_delayed_work(&data->work, 0);
		//this code must be here,very carefull.
		if(adapter_status == O2_CHARGER_BATTERY)
		{
			if(data->batt_info.batt_current >= data->batt_info.discharge_current_th)
			{
				if (batt_info_ptr) batt_info_ptr->fCurr = -20;

				data->batt_info.batt_current = -20;
				pr_info("drop current\n");
			}
		}
		oz8806_suspend = 0;
		return NOTIFY_OK;

	default:
		return NOTIFY_DONE;
	}
}

int32_t oz8806_get_battery_model(void)
{
	int32_t val = 100;

	pr_info("batt id = %d mv\n", val);

	if (val >= 700 && val <= 1100)
		return BATTERY_MODEL_MAIN;
	else if(val >= 80 && val <= 250)
		return BATTERY_MODEL_SECOND;
	else
		return BATTERY_MODEL_MAIN;
}

static int32_t oz8806_init_batt_info(struct oz8806_data *data)
{
	data->batt_info.batt_soc = 50; 
	data->batt_info.batt_voltage = 3999;
	data->batt_info.batt_current = -300;
	data->batt_info.batt_temp = 27;
	data->batt_info.batt_capacity = 4000;
	data->batt_info.cycle = 0;

	data->batt_info.batt_fcc_data = config_data.design_capacity;
	data->batt_info.discharge_current_th = DISCH_CURRENT_TH;
	data->batt_info.charge_end = 0;

	return 0;
}
static int32_t oz8806_update_batt_info(struct oz8806_data *data)
{
	int32_t ret = 0;
	//Notice: before call this function, use mutex_lock(&update_mutex)
	//Notice: don't nest mutex
	if (!batt_info_ptr || !gas_gauge_ptr)
	{
		ret = -EINVAL;
		goto end;
	}

	data->batt_info.batt_soc = batt_info_ptr->fRSOC; 
	if (data->batt_info.batt_soc > 100)
		data->batt_info.batt_soc = 100;

	data->batt_info.batt_voltage = batt_info_ptr->fVolt;
	data->batt_info.batt_current = batt_info_ptr->fCurr;
	data->batt_info.batt_capacity = batt_info_ptr->sCaMAH;
	data->batt_info.cycle = batt_info_ptr->cycle_count;

	data->batt_info.charge_end = gas_gauge_ptr->charge_end;
	bmu_init_done = gas_gauge_ptr->bmu_init_ok;
	bmt_dbg("bmu_init_done %d bmu_init_ok %d\n",bmu_init_done,gas_gauge_ptr->bmu_init_ok);
#ifndef EXT_THERMAL_READ
	oz8806_update_batt_temp(data);
#endif
end:
	return ret;
}

extern signed int battery_get_bat_temperature(void);
static int32_t oz8806_update_batt_temp(struct oz8806_data *data)
{
	if (!batt_info_ptr || !gas_gauge_ptr)
		return -EINVAL;

#ifdef EXT_THERMAL_READ
	data->batt_info.batt_temp = battery_get_bat_temperature();
	if (batt_info_ptr->fCellTemp != data->batt_info.batt_temp)
		batt_info_ptr->fCellTemp = data->batt_info.batt_temp;
	pr_info("get tbat from platform %d\n",data->batt_info.batt_temp);
#else
	data->batt_info.batt_temp = batt_info_ptr->fCellTemp;
#endif
	return 0;
}

void oz8806_battery_update_data(void)
{
	bmt_dbg("enter oz8806_battery_update_data func\n");
	if(fg_hw_init_done && the_oz8806 && bmu_init_done)
		oz8806_battery_func(the_oz8806);
}
EXPORT_SYMBOL(oz8806_battery_update_data);

int32_t oz8806_get_cycle_count(void)
{
	int32_t ret = -1;

	mutex_lock(&update_mutex);

	if (!bmu_init_done)
		ret = 0;
	else if (batt_info_ptr)
		ret = batt_info_ptr->cycle_count;

	mutex_unlock(&update_mutex);

	return ret;
}
EXPORT_SYMBOL(oz8806_get_cycle_count);

int32_t oz8806_get_remaincap(void)
{
	int32_t ret = -1;

	mutex_lock(&update_mutex);

	if (batt_info_ptr)
		ret = batt_info_ptr->sCaMAH;

	mutex_unlock(&update_mutex);

	return ret;
}
EXPORT_SYMBOL(oz8806_get_remaincap);

int32_t oz8806_get_soc(void)
{
	int32_t ret = -1;

	mutex_lock(&update_mutex);

	if (!bmu_init_done)
		ret = -1;
	else if (batt_info_ptr)
		ret = batt_info_ptr->fRSOC;

	mutex_unlock(&update_mutex);

	return ret;
}
EXPORT_SYMBOL(oz8806_get_soc);

int32_t oz8806_get_battry_current(void)
{
	int32_t ret = -1;

	if (!bmu_init_done) return -1;

	mutex_lock(&update_mutex);

	if (batt_info_ptr)
		ret = afe_read_current(&batt_info_ptr->fCurr);

	if (ret < 0)
	{
		ret = -1;
		pr_err("current adc error\n");
	}

	if (batt_info_ptr && ret >= 0)
		ret = batt_info_ptr->fCurr;

	mutex_unlock(&update_mutex);

	return ret;
}
EXPORT_SYMBOL(oz8806_get_battry_current);

int32_t oz8806_get_battery_voltage(void)
{
	int32_t ret = -1;

	if (!bmu_init_done) return -1;

	mutex_lock(&update_mutex);

	if (batt_info_ptr)
		ret = afe_read_cell_volt(&batt_info_ptr->fVolt);

	if (ret < 0)
	{
		ret = -1;
		pr_err("voltage adc error\n");
	}

	if (batt_info_ptr && ret >= 0)
		ret = batt_info_ptr->fVolt;

	mutex_unlock(&update_mutex);

	return ret;
}
EXPORT_SYMBOL(oz8806_get_battery_voltage);

int32_t oz8806_get_battery_temp(void)
{
	int32_t ret = -1;
	
	if (!bmu_init_done) return -1;

	mutex_lock(&update_mutex);

	if (batt_info_ptr)
		ret = batt_info_ptr->fCellTemp;

	mutex_unlock(&update_mutex);

	return ret;
}
EXPORT_SYMBOL(oz8806_get_battery_temp);

struct i2c_client * oz8806_get_client(void)
{
	if (the_oz8806)
		return the_oz8806->myclient;
	else
	{
		pr_err("NULL pointer\n");
		return NULL;
	}
}
EXPORT_SYMBOL(oz8806_get_client);

int8_t get_adapter_status(void)
{
	return adapter_status;
}
EXPORT_SYMBOL(get_adapter_status);

void oz8806_set_batt_info_ptr(bmu_data_t  *batt_info)
{
	mutex_lock(&update_mutex);

	if (!batt_info)
	{
		pr_err("NULL pointer\n");
		mutex_unlock(&update_mutex);
		return;
	}

	batt_info_ptr = batt_info;

	mutex_unlock(&update_mutex);
}
EXPORT_SYMBOL(oz8806_set_batt_info_ptr);

void oz8806_set_gas_gauge(gas_gauge_t *gas_gauge)
{
	mutex_lock(&update_mutex);

	if (!gas_gauge)
	{
		pr_err("NULL pointer\n");
		mutex_unlock(&update_mutex);
		return;
	}

	gas_gauge_ptr = gas_gauge;

	if (gas_gauge_ptr->fcc_data != 0)
		the_oz8806->batt_info.batt_fcc_data = gas_gauge_ptr->fcc_data;

	if (gas_gauge_ptr->discharge_current_th != 0)
		the_oz8806->batt_info.discharge_current_th = gas_gauge_ptr->discharge_current_th;

	pr_info("batt_fcc_data:%d, discharge_current_th:%d\n", 
			the_oz8806->batt_info.batt_fcc_data, the_oz8806->batt_info.discharge_current_th);

	gas_gauge_ptr->ext_temp_measure = ext_thermal_read;

	mutex_unlock(&update_mutex);
}
EXPORT_SYMBOL(oz8806_set_gas_gauge);


unsigned long oz8806_get_system_boot_time(void)
{
	unsigned long long time_sec;
	unsigned long mod;
	time_sec = ktime_to_ns(ktime_get_boottime());
	mod = do_div(time_sec,1000000);
	bmt_dbg("system boottime: %lu ms\n", (unsigned long)time_sec);

	return time_sec;
}
EXPORT_SYMBOL(oz8806_get_system_boot_time);

unsigned long oz8806_get_boot_up_time(void)
{
	unsigned long long t;
	struct timespec64 time;
	ktime_get_ts64(&time);
	t = time.tv_sec * 1000 + time.tv_nsec / 1000000;
	bmt_dbg("boot up time: %lu ms\n", (unsigned long) t);

	return t;
}
EXPORT_SYMBOL(oz8806_get_boot_up_time);

unsigned long oz8806_get_power_on_time(void)
{
	uint32_t t = 0;
	bmt_dbg("jiffies %lu,ic_wakeup_time %lu\n",jiffies,ic_wakeup_time);
	t = jiffies_to_msecs(jiffies - ic_wakeup_time);

	bmt_dbg("IC wakeup time: %u ms\n", t);

	return (unsigned long)t;
}
EXPORT_SYMBOL(oz8806_get_power_on_time);

void oz8806_set_shutdown_mode(void)
{
	mutex_lock(&update_mutex);
	if (!bmu_init_done) {
		bmt_dbg("set shutdwon mode failed!\n");
	} else if (batt_info_ptr) {
		sd77428_write_addr(the_oz8806,0x40004324,0x6318);//unlock
		sd77428_write_addr(the_oz8806,0x40004090,0x0004);
		sd77428_write_addr(the_oz8806,0x40004094,0x0001);
		msleep(1);
		sd77428_write_addr(the_oz8806,0x40004320,1);//set shutdown mode
		bmt_dbg("set shutdwon mode successed!\n");
	}
	mutex_unlock(&update_mutex);

	return;
}
EXPORT_SYMBOL(oz8806_set_shutdown_mode);

void oz8806_set_charge_full(bool val)
{
	charge_full = 0;
}
EXPORT_SYMBOL(oz8806_set_charge_full);

typedef struct {
	uint32_t v_min;
	uint32_t v_max;
} batt_id_range;

static batt_id_range id_range[] = {
	{0,400},//fg sm5602--192
	{400,800},//fg oz8806--619
	{800,1000},//reserve
};

static int get_battery_id(struct i2c_client *client, struct oz8806_data *data)
{
	int rc, bat_id_uv = 0;
	int batt_id_volt = 0;
	uint8_t temp_batt_id = 0;
	uint8_t i = 0;

	data->bat_id_vol = iio_channel_get(&client->dev, "batt_id");
	if (IS_ERR_OR_NULL(data->bat_id_vol)) {
		pr_err("iio_channel_get batt_id fail\n");
		return -ENODEV;
	}
	pr_info("bat_id_vol: %d\n",data->bat_id_vol);
	rc = iio_read_channel_processed(data->bat_id_vol, &bat_id_uv);
	if (rc < 0) {
		pr_err("iio read batt_id fail\n");
		return rc;
	}
	batt_id_volt = bat_id_uv / 1000;

	for(;i < ARRAY_SIZE(id_range); ++i) {
		if((batt_id_volt > id_range[i].v_min) && (batt_id_volt < id_range[i].v_max)) {
			temp_batt_id = i + 1;
			break;
		}
	}
	data->battery_id = temp_batt_id;
	pr_info("batt_id_volt: %d, bat_id_uv: %d,battery_id: %d\n",batt_id_volt, bat_id_uv, data->battery_id);
	return 0;
}

static int oz8806_iio_read_raw(struct iio_dev *indio_dev,
			struct iio_chan_spec const *chan, int *val1,
			int *val2, long mask)
{
	struct oz8806_data *data = iio_priv(indio_dev);
	int rc = 0;

	*val1 = 0;

	switch (chan->channel) {
		case PSY_IIO_SHUTDOWN_DELAY:
			*val1 = 0;
			break;
		case PSY_IIO_RESISTANCE:
			*val1 = 0;
			break;
		case PSY_IIO_RESISTANCE_ID:
			*val1 = data->battery_id;
			break;
		case PSY_IIO_SOC_DECIMAL:
			*val1 = 0;
			break;
		case PSY_IIO_SOC_DECIMAL_RATE:
			*val1 = 0;
			break;
		case PSY_IIO_FASTCHARGE_MODE:
			*val1 = data->fast_mode;
			break;
		case PSY_IIO_BATTERY_TYPE:
			break;
		case PSY_IIO_SOH:
			*val1 = 100;
			break;
		default:
			pr_debug("Unsupported oz8806 IIO chan %d\n", chan->channel);
			rc = -EINVAL;
			break;
	}

	pr_err("Couldn't read IIO channel %d, val1 = %d\n",chan->channel, val1);

	if (rc < 0) {
		pr_err("Couldn't read IIO channel %d, rc = %d\n",
			chan->channel, rc);
		return rc;
	}

	return IIO_VAL_INT;
}

static int fg_set_fastcharge_mode(struct oz8806_data *data, bool enable)
{
	int ret = 0;

	data->fast_mode = enable;
	return ret;
}

static int oz8806_iio_write_raw(struct iio_dev *indio_dev,
			struct iio_chan_spec const *chan, int val1,
			int val2, long mask)
{
	struct oz8806_data *data = iio_priv(indio_dev);
	int rc = 0;

	switch (chan->channel) {
		case PSY_IIO_FASTCHARGE_MODE:
			fg_set_fastcharge_mode(data, !!val1);
			break;
		default:
			pr_err("Unsupported oz8806 IIO chan %d\n", chan->channel);
			rc = -EINVAL;
			break;
	}
	if (rc < 0)
		pr_err("Couldn't write IIO channel %d, rc = %d\n",
			chan->channel, rc);
	return rc;
}

static int oz8806_iio_of_xlate(struct iio_dev *indio_dev,const struct of_phandle_args *iiospec)
{
	struct oz8806_data *data = iio_priv(indio_dev);
	struct iio_chan_spec *iio_chan = data->iio_chan;
	int i =0;

	for (i=0; i < ARRAY_SIZE(oz8806_iio_psy_channels);i++, iio_chan++)
		if (iio_chan->channel == iiospec->args[0])
			return i;
	return -EINVAL;
}

static const struct iio_info oz8806_iio_info = {
	.read_raw	= oz8806_iio_read_raw,
	.write_raw	= oz8806_iio_write_raw,
	.of_xlate	= oz8806_iio_of_xlate,
};

static int oz8806_init_iio_psy(struct i2c_client *client, struct oz8806_data *data)
{
	struct iio_dev *indio_dev = data->indio_dev;
	struct iio_chan_spec *chan = NULL;
	int num_iio_channels = ARRAY_SIZE(oz8806_iio_psy_channels);
	int rc = 0, i = 0;

	pr_info("oz8806_init_iio_psy start\n");
	data->iio_chan = devm_kcalloc(&client->dev, num_iio_channels,
									sizeof(*data->iio_chan), GFP_KERNEL);
	if (!data->iio_chan)
		return -ENOMEM;
	data->int_iio_chans = devm_kcalloc(&client->dev,num_iio_channels,
									sizeof(*data->int_iio_chans),GFP_KERNEL);
	if (!data->int_iio_chans)
		return -ENOMEM;

	indio_dev->info = &oz8806_iio_info;
	indio_dev->dev.parent = &client->dev;
	indio_dev->dev.of_node = client->dev.of_node;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = data->iio_chan;
	indio_dev->num_channels = num_iio_channels;
	indio_dev->name = "oz8806";

	for (i = 0; i < num_iio_channels; i++) {
		data->int_iio_chans[i].indio_dev = indio_dev;
		chan = &data->iio_chan[i];
		data->int_iio_chans[i].channel = chan;
		chan->address = i;
		chan->channel = oz8806_iio_psy_channels[i].channel_num;
		chan->type = oz8806_iio_psy_channels[i].type;
		chan->datasheet_name =
		oz8806_iio_psy_channels[i].datasheet_name;
		chan->extend_name =
		oz8806_iio_psy_channels[i].datasheet_name;
		chan->info_mask_separate =
		oz8806_iio_psy_channels[i].info_mask;
	}

	rc = devm_iio_device_register(&client->dev, indio_dev);

	if (rc)
		pr_err("Failed to register SM5602 IIO device, rc=%d\n", rc);

	pr_info("battery IIO device, rc=%d\n", rc);

	return rc;
}

uint32_t poweron_flag = 0;
static int32_t oz8806_probe(struct i2c_client *client,const struct i2c_device_id *id)
{
	int32_t ret = 0;
	struct oz8806_data* data = NULL;
	struct iio_dev *indio_dev =NULL;

	pr_info("start\n");
	indio_dev = devm_iio_device_alloc(&client->dev, sizeof(struct oz8806_data));

	if (!indio_dev){
		pr_err("Failed to allocate memory\n");
		return -ENOMEM;
	}
	data = iio_priv(indio_dev);
	if (!data) 
	{
		dev_err(&client->dev, "Can't alloc BMT_data struct\n");
		return -ENOMEM;
	}
	data->indio_dev = indio_dev;
	// Init real i2c_client 
	i2c_set_clientdata(client, data);
	mutex_init(&data->i2c_rw_lock);
	data->myclient = client;
	data->myclient->addr = 0x18;
	the_oz8806 = data;
	sd77428_unlock(data);
	sd77428_read_addr(data,0x40004358,&poweron_flag);
	//detect ic first
	ret = sd77428_detect_ic(data);
	if(ret < 0)
	{
		pr_err("do not detect ic, exit\n");
		return -ENODEV;
	}
	//init the hardware adc register config
	sd77428_hw_init(data);
	ret = get_battery_id(client, the_oz8806);
	if(ret < 0)
	{
		pr_err("get battery id fail\n");
		return -EPROBE_DEFER;
	}
	ret = oz8806_init_iio_psy(client, the_oz8806);
	if(ret < 0)
	{
		pr_err("oz8806_init_iio_psy fail\n");
		return -EPROBE_DEFER;
	}
	//init batt info as soon as possible
	oz8806_init_batt_info(data);
	//init ic_wakeup_time first
	if (oz8806_get_boot_up_time() < 2000)
		ic_wakeup_time = jiffies + msecs_to_jiffies(2000 - oz8806_get_boot_up_time());
	else
		ic_wakeup_time = jiffies;

	bmulib_init();
	data->interval = BATTERY_WORK_INTERVAL * 1000;

	ret = device_init_wakeup(&client->dev, true);
	if (ret) 
		dev_err(&client->dev, "wakeup source init failed.\n");
	//update batt info
	INIT_DELAYED_WORK(&data->work, oz8806_battery_work);
	oz8806_powersupply_init(data);
	ret = sysfs_create_group(&(client->dev.kobj), &oz8806_attribute_group);
	if(ret)
		pr_err("failed to creat attributes\n");
		
	//alternative suspend/resume method
	data->pm_nb.notifier_call = oz8806_suspend_notifier;
	register_pm_notifier(&data->pm_nb);
	schedule_delayed_work(&data->work,0);
	fg_hw_init_done = 1; 
	pr_info("end\n");
	return 0;
}

static int32_t oz8806_remove(struct i2c_client *client)
{
	struct oz8806_data *data = i2c_get_clientdata(client);

	cancel_delayed_work(&data->work);

	sysfs_remove_group(&(data->myclient->dev.kobj), &oz8806_attribute_group);

	power_supply_unregister(data->bat);

	mutex_lock(&update_mutex);

	/*
	 * It should be noted here that during the shutdown process, the shutdown callback must be called,
	 * otherwise the chip may not sleep normally, resulting in abnormal capacity at the next startup.
	 *
	 * And be careful after the callback, if there is work or thread running,
	 * it may wake up the chip again, to avoid this situation.
	 */
	bmu_power_down_chip();
	fg_hw_init_done = 0;
	oz8806_update_batt_info(data);

	mutex_unlock(&update_mutex);
	bmulib_exit();

	return 0;
}

static void oz8806_shutdown(struct i2c_client *client)
{
	struct oz8806_data *data = i2c_get_clientdata(client);

	cancel_delayed_work(&data->work);

	sysfs_remove_group(&(data->myclient->dev.kobj), &oz8806_attribute_group);

	mutex_lock(&update_mutex);

	/*
	 * It should be noted here that during the shutdown process, the shutdown callback must be called,
	 * otherwise the chip may not sleep normally, resulting in abnormal capacity at the next startup.
	 *
	 * And be careful after the callback, if there is work or thread running,
	 * it may wake up the chip again, to avoid this situation.
	 */
	bmu_power_down_chip();
	fg_hw_init_done = 0;
	oz8806_update_batt_info(data);
	mutex_unlock(&update_mutex);
}
/*-------------------------------------------------------------------------*/

static const struct i2c_device_id oz8806_id[] = {
	{ MYDRIVER, 0 },						
	{ }
};
MODULE_DEVICE_TABLE(i2c, oz8806_id);

static const struct of_device_id oz8806_of_match[] = {
	{.compatible = "bmt,sd77428",},
	{},
};
MODULE_DEVICE_TABLE(of, oz8806_of_match);

static struct i2c_driver oz8806_driver = {
	.driver = {
		.name	= MYDRIVER,
		.of_match_table = oz8806_of_match,
	},
	.probe		= oz8806_probe,
	.remove		= oz8806_remove,
	.shutdown	= oz8806_shutdown,
	.id_table	= oz8806_id,
};

static int32_t __init oz8806_init(void)
{
	int32_t ret = 0;

	ret = i2c_add_driver(&oz8806_driver);

	if(ret != 0)
		pr_err("failed\n");
	else
		pr_info("Success\n");

	return ret;
}

static void __exit oz8806_exit(void)
{
	i2c_del_driver(&oz8806_driver);
}

MODULE_DESCRIPTION("BMT Fuel Gauge Driver");
MODULE_LICENSE("GPL");

//subsys_initcall_sync(oz8806_init);
module_init(oz8806_init);
module_exit(oz8806_exit);

MODULE_DESCRIPTION("FG Charger Driver");
MODULE_LICENSE("GPL v2");