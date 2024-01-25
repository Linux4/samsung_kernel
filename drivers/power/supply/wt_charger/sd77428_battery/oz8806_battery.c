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
#include <asm/unaligned.h>
#include <linux/iio/iio.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>
#include <linux/iio/consumer.h>

//you must add these here for BMT
#include "parameter.h"
#include "table.h"

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
//from charger
static int oz8806_get_vbus_online(struct oz8806_data *data);
static int oz8806_get_charger_status(struct oz8806_data *data);
static int oz8806_get_charger_type(struct oz8806_data *data);
struct oz8806_data *the_oz8806 = NULL;
static int8_t adapter_status = O2_CHARGER_BATTERY;
static int32_t rsoc_pre = 0;
#ifdef EXT_THERMAL_READ
static uint8_t ext_thermal_read = 1;
#else
static uint8_t ext_thermal_read = 0;
#endif
//-1 means not ready
int32_t age_soh = 0;
int32_t cycle_soh = 0;
uint32_t poweron_flag = 0;

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
		bmt_dbg("DEBUG ON \n");
	}
	else if (val == 0)
	{
		config_data.debug = 0;
		bmt_dbg("DEBUG CLOSE \n");
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
	bmt_dbg("start reinit\n");
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

	bmt_dbg("batt_cycle_soh: %d ", cycle_soh);
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

	bmt_dbg("batt_age_soh: %d ", age_soh);
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

int32_t sd77428_read_word(struct oz8806_data *chip,uint8_t index,uint16_t *data)
{
	int32_t ret = 0;
	uint8_t rxbuf[32] = {0};
	uint8_t i = 0;
	uint8_t pec = 0;

	for(i=0;i<4;i++)
	{
		ret = sd77428_i2c_read_bytes(chip,index,rxbuf,3);
		if(0 == ret)
		{
#if(1 == PEC_ENABLE)
				pec = calculate_pec_byte(chip->myclient->addr << 1,pec);
				pec = calculate_pec_byte(index,pec);
				pec = calculate_pec_byte(chip->myclient->addr << 1|1,pec);
				if(rxbuf[2] != calculate_pec_bytes(pec,rxbuf,2))
				{
					pr_err("sd77428_read_word pec Error\n");
					usleep_range(10000,20000);
					pec = 0;
					continue;
				}
#endif
			*data = get_unaligned_be16(rxbuf);
			break;
		}
	}

	if(i>4)
	        pr_err("sd77428_read_word Error\n");

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
    //bmt_dbg("%x %x %x %x\n",buf[0],buf[1],buf[2],buf[3]);
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
	bmt_dbg("chip version 0x%08X\n",chip_ver);
	//if(condition)
	return 0;
}

static int sd77428_hw_init(struct oz8806_data *chip)
{
	if(sd77428_write_word(chip,0x12,0x6303) < 0) //disable MCU, 关闭MCU, 只保留硬件ADC功能
		goto error;

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
EXPORT_SYMBOL(sd77428_hw_init);

//----------------------------------------------------------------------------------------------------------------------//
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
	int ret = 0;

	//charger full from charger:(ret = 4:charge full)
	ret = oz8806_get_charger_status(the_oz8806);
	pr_err("check_charger_full ret=%d \n", ret);

	if(POWER_SUPPLY_STATUS_FULL == ret)
		return 1;
	else
		return 0;
}

static void charge_end_fun(void)
{
#define CHG_END_PERIOD_MS	(MSEC_PER_SEC * 120) //60 s = 1 minutes
#define FORCE_FULL_MS	(MSEC_PER_SEC * 120) //120s = 2 minutes
#define CHG_END_PURSUE_STEP    (MSEC_PER_SEC * 120) //120s

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
		bmt_dbg("charger is full, enter external charger finish\n");
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

	if((batt_info_ptr->fVolt >= (config_data.charge_cv_voltage - 50))&&(batt_info_ptr->fCurr >= DISCH_CURRENT_TH) &&
		(batt_info_ptr->fCurr < oz8806_eoc)&& (!gas_gauge_ptr->charge_end))
	{
		if (!start_jiffies)
			start_jiffies = jiffies;

		time_accumulation = jiffies_to_msecs(jiffies - start_jiffies);

		//time accumulation is over 5 minutes
		if (time_accumulation >= CHG_END_PERIOD_MS)
		{
			charger_finish	 = 1;
			bmt_dbg("enter external charger finish\n");
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
	bmt_dbg("%s, time_accumulation:%d, charger_finish:%d\n",
			__func__, time_accumulation, charger_finish);

	bmt_dbg("voltage:%d, cv:%d, fcurr:%d, BMT eoc:%d, gas_gauge_ptr->charge_end:%d\n",
			batt_info_ptr->fVolt, config_data.charge_cv_voltage,
			batt_info_ptr->fCurr, oz8806_eoc, gas_gauge_ptr->charge_end);

#ifdef ENABLE_10MIN_END_CHARGE_FUN
	if((batt_info_ptr->fRSOC == 99) &&(!start_record_flag) &&(batt_info_ptr->fCurr > oz8806_eoc))
	{
		start_record_jiffies = jiffies;
		start_record_flag = 1;
		bmt_dbg("start_record_flag: %d, at %d ms\n",start_record_flag, jiffies_to_msecs(jiffies));
	}
	if((batt_info_ptr->fRSOC != 99) ||(batt_info_ptr->fCurr < oz8806_eoc))
	{
		start_record_flag = 0;
	}

	if((start_record_flag) && (batt_info_ptr->fCurr > oz8806_eoc))
	{
		if(jiffies_to_msecs(jiffies - start_record_jiffies) > FORCE_FULL_MS)
		{
			bmt_dbg("start_record_flag: %d, at %d ms\n",start_record_flag, jiffies_to_msecs(jiffies));
			charger_finish	 = 1;
			start_record_flag = 0;
			bmt_dbg("enter charge timer finish\n");
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
					bmt_dbg("fRSOC_extern:%d, soc:%d \n",fRSOC_extern,batt_info_ptr->fRSOC);

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
				bmt_dbg("enter charger finsh update soc:%d\n",batt_info_ptr->fRSOC);
			}
			else
			{
				bmt_dbg("enter charger charge end\n");
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
	int32_t ret = 0;
	int8_t adapter_status_temp = O2_CHARGER_BATTERY;

	ret = oz8806_get_vbus_online(data);
	if(ret > 0)
	{
		ret = oz8806_get_charger_type(data);
		if(POWER_SUPPLY_TYPE_USB_DCP == ret || POWER_SUPPLY_TYPE_USB_CDP == ret || POWER_SUPPLY_TYPE_USB_HVDCP == ret || POWER_SUPPLY_TYPE_USB_HVDCP_3 == ret)
			adapter_status_temp = O2_CHARGER_AC;
		else if(POWER_SUPPLY_TYPE_USB == ret)
			adapter_status_temp = O2_CHARGER_USB;
		else
			adapter_status_temp = O2_CHARGER_BATTERY;
	}
	else
		adapter_status_temp = O2_CHARGER_BATTERY;

	adapter_status = adapter_status_temp;
	bmt_dbg("adapter_status:%d", adapter_status);
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
	if (!oz8806_suspend)
		bmu_polling_loop();

	oz8806_update_batt_info(data);

	charge_end_fun();

	if(adapter_status == O2_CHARGER_BATTERY)
		discharge_end_fun(data);

	mutex_unlock(&update_mutex);
	/**************mutex_unlock*********************/
	oz8806_wakeup_event(data);

	bmt_dbg("l=%d v=%d t=%d c=%d ch=%d\n",
			data->batt_info.batt_soc, data->batt_info.batt_voltage,
			data->batt_info.batt_temp, data->batt_info.batt_current, adapter_status);

#ifdef BMT_PROPERTIES
	power_supply_changed(data->bat);
#endif
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
		bmt_dbg("BMT PM_SUSPEND_PREPARE \n");
		cancel_delayed_work_sync(&data->work);
		system_charge_discharge_status(data);
		oz8806_suspend = 1;
		return NOTIFY_OK;
	case PM_POST_SUSPEND:
		bmt_dbg("BMT PM_POST_SUSPEND \n");
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
				bmt_dbg("drop current\n");
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
	int	ret, batt_id_uv = 0;
	int batt_id_temp;

	the_oz8806->batt_id_vol = iio_channel_get(the_oz8806->dev, "batt_id");
	if ((!the_oz8806->batt_id_vol)) {
		pr_err("get_battery_id fail\n");
		return -1;
	}

	ret = iio_read_channel_processed(the_oz8806->batt_id_vol, &batt_id_uv);
	if (ret < 0) {
		dev_err(the_oz8806->dev, "read batt_id_vol channel fail, ret=%d\n", ret);
		return -1;
	}

	batt_id_temp = batt_id_uv / 1000;
	pr_err("wtchg_get_battery_id: batt_id_uv=%d \n", batt_id_uv);

	if(batt_id_temp < 500)
		the_oz8806->batt_id = 1;//main
	else if ((batt_id_temp > 500) && (batt_id_temp < 700))
		the_oz8806->batt_id = 2;//second
	else
		the_oz8806->batt_id = 3;//third

	pr_err("wtchg_get_battery_id: batt_id=%d \n", the_oz8806->batt_id);

	if (the_oz8806->batt_id == 1)
		return BATTERY_MODEL_MAIN;
	else if(the_oz8806->batt_id == 2)
		return BATTERY_MODEL_SECOND;
	else if(the_oz8806->batt_id == 3)
		return BATTERY_MODEL_THIRD;
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
	bmt_dbg("get tbat from platform %d\n",data->batt_info.batt_temp);
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

	bmt_dbg("batt_fcc_data:%d, discharge_current_th:%d\n",
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

struct iio_channel ** oz8806_get_ext_channels(struct device *dev,
		 const char *const *channel_map, int size)
{
	int i, rc = 0;
	struct iio_channel **iio_ch_ext;

	iio_ch_ext = devm_kcalloc(dev, size, sizeof(*iio_ch_ext), GFP_KERNEL);
	if (!iio_ch_ext)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < size; i++) {
		iio_ch_ext[i] = devm_iio_channel_get(dev, channel_map[i]);

		if (IS_ERR(iio_ch_ext[i])) {
			rc = PTR_ERR(iio_ch_ext[i]);
			if (rc != -EPROBE_DEFER)
				dev_err(dev, "%s channel unavailable, %d\n",
						channel_map[i], rc);
			return ERR_PTR(rc);
		}
	}

	return iio_ch_ext;
}

static bool  oz8806_is_sgm41542_chan_valid(struct oz8806_data *data,
		enum sgm41542_iio_channels chan)
{
	int rc;
	struct iio_channel **iio_list;

	if (!data->sgm41542_ext_iio_chans) {
		iio_list = oz8806_get_ext_channels(data->dev,sgm41542_iio_chan_name,
		ARRAY_SIZE(sgm41542_iio_chan_name));
		if (IS_ERR(iio_list)) {
			rc = PTR_ERR(iio_list);
			if (rc != -EPROBE_DEFER) {
				dev_err(data->dev, "Failed to get channels, %d\n",
					rc);
				data->sgm41542_ext_iio_chans = NULL;
			}
			return false;
		}
		data->sgm41542_ext_iio_chans = iio_list;
	}

	return true;
}

static int oz8806_get_charger_status(struct oz8806_data *data)
{
	int ret, temp;
	struct iio_channel *iio_chan_list;

	if (!oz8806_is_sgm41542_chan_valid(data, CHG_STATUS)) {
		dev_err(data->dev,"read charger_status channel fail\n");
		return -ENODEV;
	}

	iio_chan_list = data->sgm41542_ext_iio_chans[CHG_STATUS];
	ret = iio_read_channel_processed(iio_chan_list, &temp);
	if (ret < 0) {
		dev_err(data->dev,
		"read charger_status channel fail, ret=%d\n", ret);
		return ret;
	}

	return temp;
}

static int oz8806_get_charger_type(struct oz8806_data *data)
{
	int ret, temp;
	struct iio_channel *iio_chan_list;

	if (!oz8806_is_sgm41542_chan_valid(data, CHG_TYPE)) {
		dev_err(data->dev,"read charger_type channel fail\n");
		return -ENODEV;
	}

	iio_chan_list = data->sgm41542_ext_iio_chans[CHG_TYPE];
	ret = iio_read_channel_processed(iio_chan_list, &temp);
	if (ret < 0) {
		dev_err(data->dev,
		"read charger_type channel fail, ret=%d\n", ret);
		return ret;
	}

	return temp;
}

static int oz8806_get_vbus_online(struct oz8806_data *data)
{
	int ret, temp;
	struct iio_channel *iio_chan_list;

	if (!oz8806_is_sgm41542_chan_valid(data, ONLINE)) {
		dev_err(data->dev,"read vbus_online channel fail\n");
		return -ENODEV;
	}

	iio_chan_list = data->sgm41542_ext_iio_chans[ONLINE];
	ret = iio_read_channel_processed(iio_chan_list, &temp);
	if (ret < 0) {
		dev_err(data->dev,
		"read vbus_online channel fail, ret=%d\n", ret);
		return ret;
	}

	return temp;
}

static bool oz8806_ext_iio_init(struct oz8806_data *data)
{
	int rc = 0;
	struct iio_channel **iio_list;

	if (IS_ERR(data->sgm41542_ext_iio_chans))
		return false;

	if (!data->wtchg_ext_iio_chans) {
		iio_list = oz8806_get_ext_channels(data->dev,
			sgm41542_iio_chan_name, ARRAY_SIZE(sgm41542_iio_chan_name));
	 if (IS_ERR(iio_list)) {
		 rc = PTR_ERR(iio_list);
		 if (rc != -EPROBE_DEFER) {
			 dev_err(data->dev, "Failed to get channels, rc=%d\n",
					 rc);
			 data->sgm41542_ext_iio_chans = NULL;
		 }
		 return false;
	 }
	 data->sgm41542_ext_iio_chans = iio_list;
	}

	return true;
}

#ifdef BMT_PROPERTIES
static int bmt_battery_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	int ret = 0;

	switch(psp) {
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int bmt_battery_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	int ret = 0;

	struct oz8806_data *data = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		val->intval = data->batt_info.cycle;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = data->batt_info.batt_soc;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval= POWER_SUPPLY_HEALTH_GOOD;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = bmu_init_done;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = data->batt_info.batt_voltage * 1000;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = data->batt_info.batt_current * 1000;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = data->batt_info.batt_temp * 10;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static enum power_supply_property bmt_battery_properties[] = {
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_TEMP,
};

static int oz8806_iio_read_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int *val1,
		int *val2, long mask)
{
	struct oz8806_data *data = iio_priv(indio_dev);
	int ret = 0;
	*val1 = 0;

	switch (chan->channel) {
	case PSY_IIO_VOLTAGE_NOW:
		*val1 = data->batt_info.batt_voltage * 1000;
		break;
	case PSY_IIO_CURRENT_NOW:
		*val1 = data->batt_info.batt_current * 1000;
		break;
	case PSY_IIO_TEMP:
		*val1 = data->batt_info.batt_temp * 10;
		break;
	case PSY_IIO_CAPACITY:
		*val1 = data->batt_info.batt_soc;
		break;
	default:
		dev_err(data->dev, "Unsupported oz8806 IIO chan %d\n", chan->channel);
		ret = -EINVAL;
		break;
	}

	if (ret < 0) {
		dev_err(data->dev, "Couldn't read IIO channel %d, rc = %d\n",
			chan->channel, ret);
		return ret;
	}

	return IIO_VAL_INT;
}

static int oz8806_iio_write_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int val1,
		int val2, long mask)
{
	struct oz8806_data *data = iio_priv(indio_dev);
	int ret = 0;

	switch (chan->channel) {
	default:
		dev_err(data->dev, "Unsupported write OZ8806 IIO chan %d, default return 8806\n", chan->channel);
		ret = 8806;
		break;
	}

	return ret;
}

static int oz8806_iio_of_xlate(struct iio_dev *indio_dev,
				const struct of_phandle_args *iiospec)
{
	struct oz8806_data *data = iio_priv(indio_dev);
	struct iio_chan_spec *iio_chan = data->iio_chan;
	int i;

	for (i = 0; i < ARRAY_SIZE(oz8806_iio_psy_channels);
					i++, iio_chan++)
		if (iio_chan->channel == iiospec->args[0])
			return i;

	return -EINVAL;
}

static const struct iio_info oz8806_iio_info = {
	.read_raw	= oz8806_iio_read_raw,
	.write_raw	= oz8806_iio_write_raw,
	.of_xlate	= oz8806_iio_of_xlate,
};

static int oz8806_init_iio_psy(struct oz8806_data *data)
{
	struct iio_dev *indio_dev = data->indio_dev;
	struct iio_chan_spec *chan;
	int num_iio_channels = ARRAY_SIZE(oz8806_iio_psy_channels);
	int ret, i;

	data->iio_chan = devm_kcalloc(data->dev, num_iio_channels,
				sizeof(*data->iio_chan), GFP_KERNEL);
	if (!data->iio_chan)
		return -ENOMEM;

	data->int_iio_chans = devm_kcalloc(data->dev,
				num_iio_channels,
				sizeof(*data->int_iio_chans),
				GFP_KERNEL);
	if (!data->int_iio_chans)
		return -ENOMEM;

	indio_dev->info = &oz8806_iio_info;
	indio_dev->dev.parent = data->dev;
	indio_dev->dev.of_node = data->dev->of_node;
	indio_dev->name = "oz8806_iio_dev";
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = data->iio_chan;
	indio_dev->num_channels = num_iio_channels;

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

	ret = devm_iio_device_register(data->dev, indio_dev);
	if (ret)
		dev_err(data->dev, "Failed to register QG IIO device, rc=%d\n", ret);

	dev_err(data->dev, "OZ8806 IIO device, rc=%d\n", ret);
	return ret;
}
#endif

static int32_t oz8806_probe(struct i2c_client *client,const struct i2c_device_id *id)
{
	int32_t ret = 0;
	struct oz8806_data* data = NULL;
#ifdef BMT_PROPERTIES
	struct power_supply_desc *psy_desc;
	struct power_supply_config psy_cfg = {0};
#endif
	struct iio_dev *indio_dev;

	bmt_dbg("start\n");
	indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*data));
	data = iio_priv(indio_dev);
	if (!data) {
		bmt_dbg("%s : data create fail!\n", __func__);
		return -ENOMEM;
	}
	data->indio_dev = indio_dev;
	data->dev = &client->dev;
	i2c_set_clientdata(client, data);
	data->myclient = client;
	data->myclient->addr = 0x18;
	mutex_init(&data->i2c_rw_lock);
	the_oz8806 = data;

	//sd77428_read_word(data,0x12,&poweron_flag);
	sd77428_unlock(data);
	sd77428_read_addr(data, 0x40004358,&poweron_flag);
	//detect ic first
	ret = sd77428_detect_ic(data);
	if(ret < 0)
	{
		pr_err("do not detect ic, exit\n");
		return -ENODEV;
	}
	//init the hardware adc register config
	sd77428_hw_init(data);
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

	ret = oz8806_init_iio_psy(data);
	if (ret < 0)
		bmt_dbg("Failed to initialize OZ8806 iio psy: %d\n", ret);

	oz8806_ext_iio_init(data);
#ifdef BMT_PROPERTIES
	psy_desc = devm_kzalloc(&client->dev, sizeof(*psy_desc), GFP_KERNEL);
	if (!psy_desc)
		return -ENOMEM;
	psy_cfg.drv_data = data;
	psy_desc->name = BMT_PROPERTIES_NAME;
	psy_desc->type = POWER_SUPPLY_TYPE_BATTERY;
	psy_desc->properties = bmt_battery_properties;
	psy_desc->num_properties = ARRAY_SIZE(bmt_battery_properties);
	psy_desc->get_property = bmt_battery_get_property;
	psy_desc->set_property = bmt_battery_set_property;
	data->bat = devm_power_supply_register(&client->dev,
					   psy_desc,
					   &psy_cfg);
	if (IS_ERR(data->bat)) {
		ret = PTR_ERR(data->bat);
		printk(KERN_ERR"failed to register battery: %d\n", ret);
		return ret;
	}
#endif
	ret = sysfs_create_group(&(client->dev.kobj), &oz8806_attribute_group);
	if(ret)
		pr_err("failed to creat attributes\n");

	//alternative suspend/resume method
	data->pm_nb.notifier_call = oz8806_suspend_notifier;
	register_pm_notifier(&data->pm_nb);
	schedule_delayed_work(&data->work,(3*HZ));
	fg_hw_init_done = 1;
	bmt_dbg("end\n");
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
//	{.compatible = "bmt,sd77428",},
	{.compatible = "cellwise,cw221X",},

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
		bmt_dbg("Success\n");

	return ret;
}

static void __exit oz8806_exit(void)
{
	i2c_del_driver(&oz8806_driver);
}

MODULE_DESCRIPTION("BMT Fuel Gauge Driver");
MODULE_LICENSE("GPL v2");

//subsys_initcall_sync(oz8806_init);
module_init(oz8806_init);
module_exit(oz8806_exit);
