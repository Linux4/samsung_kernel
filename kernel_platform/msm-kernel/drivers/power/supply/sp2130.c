
/*
 * SP2130 battery charging driver
*/

#include "inc/sp2130.h"
#include "inc/sp2130_reg.h"
#include "inc/sp2130_iio.h"

bool sp2130_conf_en = false;
struct sp2130 *g_sc;

bool get_sp2130_device_config(void)
{
	bool load_en;
	load_en = sp2130_conf_en;

	return load_en;
}
EXPORT_SYMBOL(get_sp2130_device_config);

/************************************************************************/
static int __sp2130_read_byte(struct sp2130 *sc, u8 reg, u8 *data)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(sc->client, reg);
	if (ret < 0) {
		sc_err("i2c read fail: can't read from reg 0x%02X\n", reg);
		return ret;
	}

	*data = (u8) ret;

	return 0;
}

static int __sp2130_write_byte(struct sp2130 *sc, int reg, u8 val)
{
	s32 ret;

	ret = i2c_smbus_write_byte_data(sc->client, reg, val);
	if (ret < 0) {
		sc_err("i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
		       val, reg, ret);
		return ret;
	}
	return 0;
}

int sp2130_read_byte(struct sp2130 *sc, u8 reg, u8 *data)
{
	int ret;

	if (sc->skip_reads) {
		*data = 0;
		return 0;
	}

	mutex_lock(&sc->i2c_rw_lock);
	ret = __sp2130_read_byte(sc, reg, data);
	mutex_unlock(&sc->i2c_rw_lock);

	return ret;
}

static int sp2130_write_byte(struct sp2130 *sc, u8 reg, u8 data)
{
	int ret;

	if (sc->skip_writes)
		return 0;

	mutex_lock(&sc->i2c_rw_lock);
	ret = __sp2130_write_byte(sc, reg, data);
	mutex_unlock(&sc->i2c_rw_lock);

	return ret;
}

static int sp2130_update_bits(struct sp2130*sc, u8 reg,
				    u8 mask, u8 data)
{
	int ret;
	u8 tmp;

	if (sc->skip_reads || sc->skip_writes)
		return 0;

	mutex_lock(&sc->i2c_rw_lock);
	ret = __sp2130_read_byte(sc, reg, &tmp);
	if (ret) {
		sc_err("Failed: reg=%02X, ret=%d\n", reg, ret);
		goto out;
	}

	tmp &= ~mask;
	tmp |= data & mask;

	ret = __sp2130_write_byte(sc, reg, tmp);
	if (ret)
		sc_err("Failed: reg=%02X, ret=%d\n", reg, ret);

out:
	mutex_unlock(&sc->i2c_rw_lock);
	return ret;
}

/*********************************************************************/
int sp2130_disable_cp_ts_detect(struct sp2130 *sc)
{
	int ret;

	sc_err("sp2130_disable_cp_ts_detect\n");
	ret = sp2130_write_byte(sc, SP2130_REG_0E,
				0x07);

	return ret;
}

int sp2130_enable_charge(struct sp2130 *sc, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = SP2130_CHG_ENABLE;
	else
		val = SP2130_CHG_DISABLE;

	val <<= SP2130_CHG_EN_SHIFT;

	sc_err("sp2130 charger %s\n", enable == false ? "disable" : "enable");
	ret = sp2130_update_bits(sc, SP2130_REG_0E,
				SP2130_CHG_EN_MASK, val);

	return ret;
}

int sp2130_check_charge_enabled(struct sp2130 *sc, bool *enabled)
{
	int ret;
	u8 val;

	ret = sp2130_read_byte(sc, SP2130_REG_0E, &val);
	sc_info(">>>reg [0x0E] = 0x%02x\n", val);
	if (!ret)
		*enabled = !!(val & SP2130_CHG_EN_MASK);
	return ret;
}

int sp2130_enable_wdt(struct sp2130 *sc, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = SP2130_WATCHDOG_ENABLE;
	else
		val = SP2130_WATCHDOG_DISABLE;

	val <<= SP2130_WATCHDOG_DIS_SHIFT;

	ret = sp2130_update_bits(sc, SP2130_REG_0D,
				SP2130_WATCHDOG_DIS_MASK, val);

	return ret;
}

/*static int sp2130_set_wdt(struct sp2130 *sc, int ms)
{
	int ret;
	u8 val;

	if (ms == 500)
		val = SP2130_WATCHDOG_0P5S;
	else if (ms == 1000)
		val = SP2130_WATCHDOG_1S;
	else if (ms == 5000)
		val = SP2130_WATCHDOG_5S;
	else if (ms == 30000)
		val = SP2130_WATCHDOG_30S;
	else
		val = SP2130_WATCHDOG_30S;

	val <<= SP2130_WATCHDOG_SHIFT;

	ret = sp2130_update_bits(sc, SP2130_REG_0B,
				SP2130_WATCHDOG_MASK, val);
	return ret;
}*/

int sp2130_set_reg_reset(struct sp2130 *sc)
{
	int ret;
	u8 val = 1;

	val = SP2130_REG_RST_ENABLE;

	val <<= SP2130_REG_RST_DISABLE;

	ret = sp2130_update_bits(sc, SP2130_REG_0D,
				SP2130_REG_RST_MASK, val);
	return ret;
}

int sp2130_enable_batovp(struct sp2130 *sc, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = SP2130_BAT_OVP_ENABLE;
	else
		val = SP2130_BAT_OVP_DISABLE;

	val <<= SP2130_BAT_OVP_DIS_SHIFT;

	ret = sp2130_update_bits(sc, SP2130_REG_00,
				SP2130_BAT_OVP_DIS_MASK, val);
	return ret;
}

int sp2130_set_batovp_th(struct sp2130 *sc, int threshold)
{
	int ret;
	u8 val;

	if (threshold < SP2130_BAT_OVP_BASE)
		threshold = SP2130_BAT_OVP_BASE;

	val = (threshold - SP2130_BAT_OVP_BASE) *10/ SP2130_BAT_OVP_LSB;

	val <<= SP2130_BAT_OVP_SHIFT;

	ret = sp2130_update_bits(sc, SP2130_REG_00,
				SP2130_BAT_OVP_MASK, val);
	return ret;
}

int sp2130_enable_batovp_alarm(struct sp2130 *sc, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = SP2130_BAT_OVP_ALM_ENABLE;
	else
		val = SP2130_BAT_OVP_ALM_DISABLE;

	val <<= SP2130_BAT_OVP_ALM_DIS_SHIFT;

	ret = sp2130_update_bits(sc, SP2130_REG_01,
				SP2130_BAT_OVP_ALM_DIS_MASK, val);
	return ret;
}

int sp2130_set_batovp_alarm_th(struct sp2130 *sc, int threshold)
{
	int ret;
	u8 val;

	if (threshold < SP2130_BAT_OVP_ALM_BASE)
		threshold = SP2130_BAT_OVP_ALM_BASE;

	val = (threshold - SP2130_BAT_OVP_ALM_BASE) / SP2130_BAT_OVP_ALM_LSB;

	val <<= SP2130_BAT_OVP_ALM_SHIFT;

	ret = sp2130_update_bits(sc, SP2130_REG_01,
				SP2130_BAT_OVP_ALM_MASK, val);
	return ret;
}

int sp2130_enable_batocp(struct sp2130 *sc, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = SP2130_BAT_OCP_ENABLE;
	else
		val = SP2130_BAT_OCP_DISABLE;

	val <<= SP2130_BAT_OCP_DIS_SHIFT;

	ret = sp2130_update_bits(sc, SP2130_REG_02,
				SP2130_BAT_OCP_DIS_MASK, val);
	return ret;
}

int sp2130_set_batocp_th(struct sp2130 *sc, int threshold)
{
	int ret;
	u8 val;

	if (threshold < SP2130_BAT_OCP_BASE)
		threshold = SP2130_BAT_OCP_BASE;

	val = (threshold - SP2130_BAT_OCP_BASE) / SP2130_BAT_OCP_LSB;

	val <<= SP2130_BAT_OCP_SHIFT;

	ret = sp2130_update_bits(sc, SP2130_REG_02,
				SP2130_BAT_OCP_MASK, val);
	return ret;
}

int sp2130_enable_batocp_alarm(struct sp2130 *sc, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = SP2130_BAT_OCP_ALM_ENABLE;
	else
		val = SP2130_BAT_OCP_ALM_DISABLE;

	val <<= SP2130_BAT_OCP_ALM_DIS_SHIFT;

	ret = sp2130_update_bits(sc, SP2130_REG_03,
				SP2130_BAT_OCP_ALM_DIS_MASK, val);
	return ret;
}

int sp2130_set_batocp_alarm_th(struct sp2130 *sc, int threshold)
{
	int ret;
	u8 val;

	if (threshold < SP2130_BAT_OCP_ALM_BASE)
		threshold = SP2130_BAT_OCP_ALM_BASE;

	val = (threshold - SP2130_BAT_OCP_ALM_BASE) / SP2130_BAT_OCP_ALM_LSB;

	val <<= SP2130_BAT_OCP_ALM_SHIFT;

	ret = sp2130_update_bits(sc, SP2130_REG_03,
				SP2130_BAT_OCP_ALM_MASK, val);
	return ret;
}

int sp2130_set_busovp_th(struct sp2130 *sc, int threshold)
{
	int ret;
	u8 val;

	if (threshold < SP2130_BUS_OVP_BASE)
		threshold = SP2130_BUS_OVP_BASE;

	val = (threshold - SP2130_BUS_OVP_BASE) / SP2130_BUS_OVP_LSB;

	val <<= SP2130_BUS_OVP_SHIFT;

	ret = sp2130_update_bits(sc, SP2130_REG_07,
				SP2130_BUS_OVP_MASK, val);
	return ret;
}

int sp2130_enable_busovp_alarm(struct sp2130 *sc, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = SP2130_BUS_OVP_ALM_ENABLE;
	else
		val = SP2130_BUS_OVP_ALM_DISABLE;

	val <<= SP2130_BUS_OVP_ALM_DIS_SHIFT;

	ret = sp2130_update_bits(sc, SP2130_REG_08,
				SP2130_BUS_OVP_ALM_DIS_MASK, val);
	return ret;
}

int sp2130_set_busovp_alarm_th(struct sp2130 *sc, int threshold)
{
	int ret;
	u8 val;

	if (threshold < SP2130_BUS_OVP_ALM_BASE)
		threshold = SP2130_BUS_OVP_ALM_BASE;

	val = (threshold - SP2130_BUS_OVP_ALM_BASE) / SP2130_BUS_OVP_ALM_LSB;

	val <<= SP2130_BUS_OVP_ALM_SHIFT;

	ret = sp2130_update_bits(sc, SP2130_REG_08,
				SP2130_BUS_OVP_ALM_MASK, val);
	return ret;
}

int sp2130_enable_busocp(struct sp2130 *sc, bool enable)
{
	int ret=0;
	u8 val;

	if (enable)
		val = SP2130_BUS_OCP_ENABLE;
	else
		val = SP2130_BUS_OCP_DISABLE;

	val <<= SP2130_BUS_OCP_DIS_SHIFT;

	ret = sp2130_update_bits(sc, SP2130_REG_09,
				SP2130_BUS_OCP_DIS_MASK, val);

	return ret;
}

int sp2130_set_busocp_th(struct sp2130 *sc, int threshold)
{
	int ret;
	u8 val;

	if (threshold < SP2130_BUS_OCP_BASE)
		threshold = SP2130_BUS_OCP_BASE;

	val = (threshold - SP2130_BUS_OCP_BASE) / (SP2130_BUS_OCP_LSB);

	val <<= SP2130_BUS_OCP_SHIFT;

	ret = sp2130_update_bits(sc, SP2130_REG_09,
				SP2130_BUS_OCP_MASK, val);
	return ret;
}

int sp2130_enable_busocp_alarm(struct sp2130 *sc, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = SP2130_BUS_OCP_ALM_ENABLE;
	else
		val = SP2130_BUS_OCP_ALM_DISABLE;

	val <<= SP2130_BUS_OCP_ALM_DIS_SHIFT;

	ret = sp2130_update_bits(sc, SP2130_REG_0A,
				SP2130_BUS_OCP_ALM_DIS_MASK, val);
	return ret;
}

int sp2130_set_busocp_alarm_th(struct sp2130 *sc, int threshold)
{
	int ret;
	u8 val;

	if (threshold < SP2130_BUS_OCP_ALM_BASE)
		threshold = SP2130_BUS_OCP_ALM_BASE;

	val = (threshold - SP2130_BUS_OCP_ALM_BASE) / SP2130_BUS_OCP_ALM_LSB;

	val <<= SP2130_BUS_OCP_ALM_SHIFT;

	ret = sp2130_update_bits(sc, SP2130_REG_0A,
				SP2130_BUS_OCP_ALM_MASK, val);
	return ret;
}

int sp2130_enable_batucp_alarm(struct sp2130 *sc, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = SP2130_BAT_UCP_ALM_ENABLE;
	else
		val = SP2130_BAT_UCP_ALM_DISABLE;

	val <<= SP2130_BAT_UCP_ALM_DIS_SHIFT;

	ret = sp2130_update_bits(sc, SP2130_REG_04,
				SP2130_BAT_UCP_ALM_DIS_MASK, val);
	return ret;
}

int sp2130_set_batucp_alarm_th(struct sp2130 *sc, int threshold)
{
	int ret;
	u8 val;

	if (threshold < SP2130_BAT_UCP_ALM_BASE)
		threshold = SP2130_BAT_UCP_ALM_BASE;

	val = (threshold - SP2130_BAT_UCP_ALM_BASE) / SP2130_BAT_UCP_ALM_LSB;

	val <<= SP2130_BAT_UCP_ALM_SHIFT;

	ret = sp2130_update_bits(sc, SP2130_REG_04,
				SP2130_BAT_UCP_ALM_MASK, val);
	return ret;
}

int sp2130_set_reg_ctrl_i2c(struct sp2130 *sc)
{
	int ret;

	ret = sp2130_write_byte(sc, 0xF7, 0x00);
	ret = sp2130_write_byte(sc, 0xF7, 0x78);
	ret = sp2130_write_byte(sc, 0xF7, 0x87);
	ret = sp2130_write_byte(sc, 0xF7, 0xAA);
	ret = sp2130_write_byte(sc, 0xF7, 0x55);
	ret = sp2130_write_byte(sc, 0x8C, 0x20);

	return ret;
}

int sp2130_set_acovp_th(struct sp2130 *sc, int threshold)
{
	int ret=0;

	u8 val;

	if (threshold < SP2130_AC1_OVP_BASE)
		threshold = SP2130_AC1_OVP_BASE;

	if (threshold == SP2130_AC1_OVP_6P5V)
		val = 0x07;
	else
		val = (threshold - SP2130_AC1_OVP_BASE) /  SP2130_AC1_OVP_LSB;

	val <<= SP2130_AC1_OVP_SHIFT;

	ret = sp2130_update_bits(sc, SP2130_REG_05,
				SP2130_AC1_OVP_MASK, val);

	return ret;

}

int sp2130_enable_otg(struct sp2130 *sc, bool enable)
{
	int ret=0;
	u8 val;

	if (enable)
			val = SP2130_OTG_ENABLE;
		else
			val = SP2130_OTG_DISABLE;

	val <<= SP2130_EN_OTG_SHIFT;
	ret = sp2130_update_bits(sc, SP2130_REG_2F,
					SP2130_EN_OTG_MASK, val);
		return ret;
}

int sp2130_check_enable_otg(struct sp2130 *sc, bool *enable)
{
	int ret;
	u8 val;

	ret = sp2130_read_byte(sc, SP2130_REG_2F, &val);
	sc_info(">>>reg [0x2F] = 0x%02x\n", val);
	if (!ret)
		*enable = !!(val & SP2130_EN_OTG_MASK);
	return ret;
}

int sp2130_enable_acdrv1(struct sp2130 *sc, bool enable)
{
	int ret=0;
	u8 val;


	if (enable)
		val = SP2130_ACDRV1_ENABLE;
	else
		val = SP2130_ACDRV1_DISABLE;

	val <<= SP2130_EN_ACDRV1_SHIFT;

	ret = sp2130_update_bits(sc, SP2130_REG_30,
				SP2130_EN_ACDRV1_MASK, val);

	return ret;

}

int sp2130_check_enable_acdrv1(struct sp2130 *sc, bool *enable)
{
	int ret;
	u8 val;

	ret = sp2130_read_byte(sc, SP2130_REG_30, &val);
	sc_info(">>>reg [0x30] = 0x%02x\n", val);
	if (!ret)
		*enable = !!(val & SP2130_EN_ACDRV1_MASK);
	return ret;
}

int sp2130_enable_acdrv2(struct sp2130 *sc, bool enable)
{
	int ret=0;
	u8 val;

	if (enable)
		val = SP2130_ACDRV2_ENABLE;
	else
		val = SP2130_ACDRV2_DISABLE;

	val <<= SP2130_EN_ACDRV2_SHIFT;

	ret = sp2130_update_bits(sc, SP2130_REG_30,
				SP2130_EN_ACDRV2_MASK, val);

	return ret;
}

int sp2130_check_enable_acdrv2(struct sp2130 *sc, bool *enable)
{
	int ret;
	u8 val;

	ret = sp2130_read_byte(sc, SP2130_REG_30, &val);
	sc_info(">>>reg [0x30] = 0x%02x\n", val);
	if (!ret)
		*enable = !!(val & SP2130_EN_ACDRV2_MASK);
	return ret;
}

#if 0
int sp2130_set_vdrop_th(struct sp2130 *sc, int threshold)
{
	int ret=0;
	u8 val;

	if (threshold == 300)
		val = SP2130_VDROP_THRESHOLD_300MV;
	else
		val = SP2130_VDROP_THRESHOLD_400MV;

	val <<= SP2130_VDROP_THRESHOLD_SET_SHIFT;

	ret = sp2130_update_bits(sc, SP2130_REG_05,
				SP2130_VDROP_THRESHOLD_SET_MASK,
				val);
	return ret;
}

int sp2130_set_vdrop_deglitch(struct sp2130 *sc, int us)
{
	int ret=0;
	u8 val;

	if (us == 8)
		val = SP2130_VDROP_DEGLITCH_8US;
	else
		val = SP2130_VDROP_DEGLITCH_5MS;

	val <<= SP2130_VDROP_DEGLITCH_SET_SHIFT;

	ret = sp2130_update_bits(sc, SP2130_REG_05,
				SP2130_VDROP_DEGLITCH_SET_MASK,
				val);
	return ret;
}
#endif

/*
 * the input threshold is the raw value that would write to register directly.
 */
 #if 0
int sp2130_set_bus_therm_th(struct sp2130 *sc, u8 threshold)
{
	int ret = 0;
#if 0
	ret = sp2130_write_byte(sc, SP2130_REG_28, threshold);
#endif
	return ret;
}
#endif
/*
 * please be noted that the unit here is degC
 */
 #if 0
int sp2130_set_die_therm_th(struct sp2130 *sc, u8 threshold)
{
	int ret=0;
	u8 val;

	/*BE careful, LSB is here is 1/LSB, so we use multiply here*/
	val = (threshold - SP2130_TDIE_ALM_BASE) * 10/SP2130_TDIE_ALM_LSB;
	val <<= SP2130_TDIE_ALM_SHIFT;

	ret = sp2130_update_bits(sc, SP2130_REG_2A,
				SP2130_TDIE_ALM_MASK, val);

	return ret;
}
#endif
int sp2130_enable_adc(struct sp2130 *sc, bool enable)
{
	int ret;
	u8 val;

	if (enable)
		val = SP2130_ADC_ENABLE;
	else
		val = SP2130_ADC_DISABLE;

	val <<= SP2130_ADC_EN_SHIFT;

	ret = sp2130_update_bits(sc, SP2130_REG_15,
				SP2130_ADC_EN_MASK, val);
	return ret;
}

int sp2130_set_adc(bool val)
{
	int ret = -EINVAL;

	if (g_sc)
		ret = sp2130_enable_adc(g_sc, val);
	return ret;
}
EXPORT_SYMBOL(sp2130_set_adc);

int sp2130_set_adc_scanrate(struct sp2130 *sc, bool oneshot)
{
	int ret;
	u8 val;

	if (oneshot)
		val = SP2130_ADC_RATE_ONESHOT;
	else
		val = SP2130_ADC_RATE_CONTINOUS;

	val <<= SP2130_ADC_RATE_SHIFT;

	ret = sp2130_update_bits(sc, SP2130_REG_15,
				SP2130_ADC_RATE_MASK, val);
	return ret;
}

int sp2130_get_adc_data(struct sp2130 *sc, int channel,  int *result)
{
	int ret;
	u8 val_l, val_h;
	u16 val;

	if(channel >= ADC_MAX_NUM) return 0;

	ret = sp2130_read_byte(sc, ADC_REG_BASE + (channel << 1), &val_h);
	ret = sp2130_read_byte(sc, ADC_REG_BASE + (channel << 1) + 1, &val_l);

	if (ret < 0)
		return ret;
	val = (val_h << 8) | val_l;

	if(sc->is_sp2130)
	{
		if((channel == ADC_TSBUS) || (channel == ADC_TSBAT))
			val = val * 9766/100000;
		else if(channel == ADC_TDIE)
			val = val * 5/10;
	}

	*result = val;

	return 0;
}

int sp2130_set_adc_scan(struct sp2130 *sc, int channel, bool enable)
{
	int ret;
	u8 reg;
	u8 mask;
	u8 shift;
	u8 val;

	if (channel > ADC_MAX_NUM)
		return -EINVAL;

	if (channel == ADC_IBUS) {
		reg = SP2130_REG_15;
		shift = SP2130_IBUS_ADC_DIS_SHIFT;
		mask = SP2130_IBUS_ADC_DIS_MASK;
	} else if (channel == ADC_VBUS) {
		reg = SP2130_REG_15;
		shift = SP2130_IBUS_ADC_DIS_SHIFT;
		mask = SP2130_IBUS_ADC_DIS_MASK;
	} else {
		reg = SP2130_REG_16;
		shift = 9 - channel;
		mask = 1 << shift;
	}

	if (enable)
		val = 0 << shift;
	else
		val = 1 << shift;

	ret = sp2130_update_bits(sc, reg, mask, val);

	return ret;
}

#if 0
/*init mask*/
int sp2130_set_alarm_int_mask(struct sp2130 *sc, u8 mask)
{
	int ret;
	u8 val;

	ret = sp2130_read_byte(sc, SP2130_REG_0F, &val);
	if (ret)
		return ret;

	val |= mask;

	ret = sp2130_write_byte(sc, SP2130_REG_0F, val);

	return ret;
}
#endif
/*static int sp2130_clear_alarm_int_mask(struct sp2130 *sc, u8 mask)
{
	int ret;
	u8 val;

	ret = sp2130_read_byte(sc, SP2130_REG_0F, &val);
	if (ret)
		return ret;

	val &= ~mask;

	ret = sp2130_write_byte(sc, SP2130_REG_0F, val);

	return ret;
}*/

/*static int sp2130_set_fault_int_mask(struct sp2130 *sc, u8 mask)
{
	int ret;
	u8 val;

	ret = sp2130_read_byte(sc, SP2130_REG_12, &val);
	if (ret)
		return ret;

	val |= mask;

	ret = sp2130_write_byte(sc, SP2130_REG_12, val);

	return ret;
}*/

/*static int sp2130_clear_fault_int_mask(struct sp2130 *sc, u8 mask)
{
	int ret;
	u8 val;

	ret = sp2130_read_byte(sc, SP2130_REG_12, &val);
	if (ret)
		return ret;

	val &= ~mask;

	ret = sp2130_write_byte(sc, SP2130_REG_12, val);

	return ret;
}*/

#if 0
int sp2130_set_sense_resistor(struct sp2130 *sc, int r_mohm)
{
	int ret=0;

	u8 val;

	if (r_mohm == 2)
		val = SP2130_SET_IBAT_SNS_RES_2MHM;
	else if (r_mohm == 5)
		val = SP2130_SET_IBAT_SNS_RES_5MHM;
	else
		return -EINVAL;

	val <<= SP2130_SET_IBAT_SNS_RES_SHIFT;

	ret = sp2130_update_bits(sc, SP2130_REG_2B,
				SP2130_SET_IBAT_SNS_RES_MASK,
				val);

	return ret;
}
#endif

#if 0
int sp2130_enable_regulation(struct sp2130 *sc, bool enable)
{
	int ret=0;



	u8 val;

	if (enable)
		val = SP2130_EN_REGULATION_ENABLE;
	else
		val = SP2130_EN_REGULATION_DISABLE;

	val <<= SP2130_EN_REGULATION_SHIFT;

	ret = sp2130_update_bits(sc, SP2130_REG_2B,
				SP2130_EN_REGULATION_MASK,
				val);
	return ret;
}
#endif

int sp2130_set_ss_timeout(struct sp2130 *sc, int timeout)
{
	int ret =0;
	u8 val;

	switch (timeout) {
	case 0:
		val = SP2130_SS_TIMEOUT_DISABLE;
		break;
	case 12:
		val = SP2130_SS_TIMEOUT_12P5MS;
		break;
	case 25:
		val = SP2130_SS_TIMEOUT_25MS;
		break;
	case 50:
		val = SP2130_SS_TIMEOUT_50MS;
		break;
	case 100:
		val = SP2130_SS_TIMEOUT_100MS;
		break;
	case 400:
		val = SP2130_SS_TIMEOUT_400MS;
		break;
	case 1500:
		val = SP2130_SS_TIMEOUT_1500MS;
		break;
	case 100000:
		val = SP2130_SS_TIMEOUT_100000MS;
		break;
	default:
		val = SP2130_SS_TIMEOUT_DISABLE;
		break;
	}

	val <<= SP2130_SS_TIMEOUT_SET_SHIFT;;

	ret = sp2130_update_bits(sc, SP2130_REG_2E,
				SP2130_SS_TIMEOUT_SET_MASK,
				val);

	return ret;
}

/*there is no set_ibat reg*/
#if 0
int sp2130_set_ibat_reg_th(struct sp2130 *sc, int th_ma)
{
	int ret =0;
	u8 val;

	if (th_ma == 200)
		val = SP2130_IBAT_REG_200MA;
	else if (th_ma == 300)
		val = SP2130_IBAT_REG_300MA;
	else if (th_ma == 400)
		val = SP2130_IBAT_REG_400MA;
	else if (th_ma == 500)
		val = SP2130_IBAT_REG_500MA;
	else
		val = SP2130_IBAT_REG_500MA;

	val <<= SP2130_IBAT_REG_SHIFT;
	ret = sp2130_update_bits(sc, SP2130_REG_2C,
				SP2130_IBAT_REG_MASK,
				val);

	return ret;
}
#endif

/*there is no set_vbat reg*/
#if 0
int sp2130_set_vbat_reg_th(struct sp2130 *sc, int th_mv)
{
	int ret = 0;
	u8 val;

	if (th_mv == 50)
		val = SP2130_VBAT_REG_50MV;
	else if (th_mv == 100)
		val = SP2130_VBAT_REG_100MV;
	else if (th_mv == 150)
		val = SP2130_VBAT_REG_150MV;
	else
		val = SP2130_VBAT_REG_200MV;

	val <<= SP2130_VBAT_REG_SHIFT;

	ret = sp2130_update_bits(sc, SP2130_REG_2C,
				SP2130_VBAT_REG_MASK,
				val);

	return ret;
}
#endif

#if 0
static int sp2130_get_work_mode(struct sp2130 *sc, int *mode)
{
	int ret;
	u8 val;

	ret = sp2130_read_byte(sc, sp2130_REG_0C, &val);

	if (ret) {
		sc_err("Failed to read operation mode register\n");
		return ret;
	}

	val = (val & SP2130_MS_MASK) >> SP2130_MS_SHIFT;
	if (val == SP2130_MS_MASTER)
		*mode = SP2130_ROLE_MASTER;
	else if (val == SP2130_MS_SLAVE)
		*mode = SP2130_ROLE_SLAVE;
	else
		*mode = SP2130_ROLE_STDALONE;

	sc_info("work mode:%s\n", *mode == SP2130_ROLE_STDALONE ? "Standalone" :
			(*mode == SP2130_ROLE_SLAVE ? "Slave" : "Master"));
	return ret;
}
#endif

int sp2130_check_vbus_error_status(struct sp2130 *sc)
{
	int ret;
	u8 data;

	ret = sp2130_read_byte(sc, SP2130_REG_35, &data);
	if(ret == 0){
		sc_err("vbus error >>>>%02x\n", data);
		sc->vbus_error = data;
	}

	return ret;
}

int sp2130_detect_device(struct sp2130 *sc)
{
	int ret;
	u8 data;

	ret = sp2130_read_byte(sc, SP2130_REG_31, &data);
	if (ret == 0) {
		sc->part_no = (data & SP2130_DEV_ID_MASK);
		sc->part_no >>= SP2130_DEV_ID_SHIFT;
	}
	sc_err("sc-part_no >>>>%02x\n", sc->part_no);

	return ret;
}

static void sp2130_dump_reg(struct sp2130 *sc)
{
       int ret;
       u8 val;
       u8 addr;

       for (addr = 0x00; addr <= SP2130_MAX_SHOW_REG_ADDR; addr++) {
            ret = sp2130_read_byte(sc, addr, &val);
            if (!ret)
                pr_err("Reg[%02X] = 0x%02X\n", addr, val);
       }
}

void sp2130_check_alarm_status(struct sp2130 *sc)
{
	int ret1;
	int ret2;
	u8 flag = 0;
	u8 stat1 = 0;
	u8 stat2 = 0;


	mutex_lock(&sc->data_lock);
#if 0
	ret = sp2130_read_byte(sc, SP2130_REG_08, &flag);
	if (!ret && (flag & SP2130_IBUS_UCP_FALL_FLAG_MASK))
		sc_dbg("UCP_FLAG =0x%02X\n",
			!!(flag & SP2130_IBUS_UCP_FALL_FLAG_MASK));

	ret = sp2130_read_byte(sc, SP2130_REG_2D, &flag);
	if (!ret && (flag & SP2130_VDROP_OVP_FLAG_MASK))
		sc_dbg("VDROP_OVP_FLAG =0x%02X\n",
			!!(flag & SP2130_VDROP_OVP_FLAG_MASK));
#endif
	/*read to clear alarm flag*/
	ret1 = sp2130_read_byte(sc, SP2130_REG_10, &flag);
	if (!ret1 && flag)
		sc_dbg("INT_FLAG =0x%02X\n", flag);

	ret1 = sp2130_read_byte(sc, SP2130_REG_0F, &stat1);
	ret2 = sp2130_read_byte(sc, SP2130_REG_36, &stat2);

	if (!ret1 && !ret2 && stat1 != sc->prev_alarm && stat2 != sc->prev_alarm) {
		sc_dbg("INT_STAT = stat1 0X%02x, stat2 0X%02x\n", stat1, stat2);
		sc->prev_alarm = stat1;
		sc->bat_ovp_alarm = !!(stat1 & BAT_OVP_ALARM);
		sc->bat_ocp_alarm = !!(stat1 & BAT_OCP_ALARM);
		sc->bus_ovp_alarm = !!(stat1 & BUS_OVP_ALARM);
		sc->bus_ocp_alarm = !!(stat1 & BUS_OCP_ALARM);
		sc->batt_present  = !!(stat1 & VBAT_INSERT);
		sc->bat_ucp_alarm = !!(stat1 & BAT_UCP_ALARM);
		sc->vbus_present  = !!(stat2 & VBUS_INSERT);
	}


#if 0
	ret = sp2130_read_byte(sc, SP2130_REG_08, &stat);
	if (!ret && (stat & 0x50))
		sc_err("Reg[05]BUS_UCPOVP = 0x%02X\n", stat);

	ret = sp2130_read_byte(sc, SP2130_REG_0A, &stat);
	if (!ret && (stat & 0x02))
		sc_err("Reg[0A]CONV_OCP = 0x%02X\n", stat);
#endif
	mutex_unlock(&sc->data_lock);
}

void sp2130_check_fault_status(struct sp2130 *sc)
{
	int ret;
	//u8 flag = 0;
	u8 stat = 0;
	bool changed = false;

	mutex_lock(&sc->data_lock);

	ret = sp2130_read_byte(sc, SP2130_REG_12, &stat);
	if (!ret && stat)
		sc_err("FAULT_STAT = 0x%02X\n", stat);

	//ret = sp2130_read_byte(sc, SP2130_REG_13, &flag);
	//if (!ret && flag)
	//	sc_err("FAULT_FLAG = 0x%02X\n", flag);

	if (!ret && stat != sc->prev_fault) {
		changed = true;
		sc->prev_fault = stat;
		sc->bat_ovp_fault = !!(stat & BAT_OVP_FAULT);
		sc->bat_ocp_fault = !!(stat & BAT_OCP_FAULT);
		sc->bus_ovp_fault = !!(stat & BUS_OVP_FAULT);
		sc->bus_ocp_fault = !!(stat & BUS_OCP_FAULT);
		sc->bat_therm_fault = !!(stat & TS_BAT_FAULT);
		sc->bus_therm_fault = !!(stat & TS_BUS_FAULT);

		sc->bat_therm_alarm = !!(stat & TBUS_TBAT_ALARM);
		sc->bus_therm_alarm = !!(stat & TBUS_TBAT_ALARM);
	}

	mutex_unlock(&sc->data_lock);
}

/*static int sp2130_check_reg_status(struct sp2130 *sc)
{
	int ret;
	u8 val;

	ret = sp2130_read_byte(sc, sp2130_REG_2C, &val);
	if (!ret) {
		sc->vbat_reg = !!(val & SP2130_VBAT_REG_ACTIVE_STAT_MASK);
		sc->ibat_reg = !!(val & SP2130_IBAT_REG_ACTIVE_STAT_MASK);
	}


	return ret;
}*/

static ssize_t sp2130_show_registers(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sp2130 *sc = dev_get_drvdata(dev);
	u8 addr;
	u8 val;
	u8 tmpbuf[300];
	int len;
	int idx = 0;
	int ret;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "sp2130");
	for (addr = 0x0; addr <= 0x31; addr++) {
		ret = sp2130_read_byte(sc, addr, &val);
		if (ret == 0) {
			len = snprintf(tmpbuf, PAGE_SIZE - idx,
					"Reg[%.2X] = 0x%.2x\n", addr, val);
			memcpy(&buf[idx], tmpbuf, len);
			idx += len;
		}
	}

	return idx;
}

static ssize_t sp2130_store_register(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sp2130 *sc = dev_get_drvdata(dev);
	int ret;
	unsigned int reg;
	unsigned int val;

	ret = sscanf(buf, "%x %x", &reg, &val);
	if (ret == 2 && reg <= 0x31)
		sp2130_write_byte(sc, (unsigned char)reg, (unsigned char)val);

	return count;
}

static DEVICE_ATTR(registers, 0660, sp2130_show_registers, sp2130_store_register);

static void sp2130_create_device_node(struct device *dev)
{
	device_create_file(dev, &dev_attr_registers);
}

static int sp2130_init_adc(struct sp2130 *sc)
{
	sp2130_set_adc_scanrate(sc, false);
	sp2130_set_adc_scan(sc, ADC_IBUS, true);
	sp2130_set_adc_scan(sc, ADC_VBUS, true);
	sp2130_set_adc_scan(sc, ADC_VOUT, true);
	sp2130_set_adc_scan(sc, ADC_VBAT, true);
	sp2130_set_adc_scan(sc, ADC_IBAT, true);
	sp2130_set_adc_scan(sc, ADC_TSBUS, true);
	sp2130_set_adc_scan(sc, ADC_TSBAT, true);
	sp2130_set_adc_scan(sc, ADC_TDIE, true);
	sp2130_set_adc_scan(sc, ADC_VAC1, true);
	sp2130_set_adc_scan(sc, ADC_VAC2, true);

	sp2130_enable_adc(sc, true);
	sc->adc_status = 1;

	return 0;
}

static int sp2130_init_int_src(struct sp2130 *sc)
{
	int ret=0;
	/*TODO:be careful ts bus and ts bat alarm bit mask is in
	 *	fault mask register, so you need call
	 *	sp2130_set_fault_int_mask for tsbus and tsbat alarm
	 */
#if 0
	ret = sp2130_set_alarm_int_mask(sc, ADC_DONE
		/*			| BAT_UCP_ALARM */
					| BAT_OVP_ALARM);
	if (ret) {
		sc_err("failed to set alarm mask:%d\n", ret);
		return ret;
	}
#endif
#if 0
	ret = sp2130_set_fault_int_mask(sc, TS_BUS_FAULT);
	if (ret) {
		sc_err("failed to set fault mask:%d\n", ret);
		return ret;
	}
#endif
	return ret;
}

static int sp2130_init_protection(struct sp2130 *sc)
{
	int ret;

	ret = sp2130_enable_batovp(sc, !sc->cfg->bat_ovp_disable);
	sc_err("%s bat ovp %s\n",
		sc->cfg->bat_ovp_disable ? "disable" : "enable",
		!ret ? "successfullly" : "failed");

	ret = sp2130_enable_batocp(sc, !sc->cfg->bat_ocp_disable);
	sc_err("%s bat ocp %s\n",
		sc->cfg->bat_ocp_disable ? "disable" : "enable",
		!ret ? "successfullly" : "failed");

	ret = sp2130_enable_busocp(sc, !sc->cfg->bus_ocp_disable);
	sc_err("%s bus ocp %s\n",
		sc->cfg->bus_ocp_disable ? "disable" : "enable",
		!ret ? "successfullly" : "failed");

	ret = sp2130_set_batovp_th(sc, sc->cfg->bat_ovp_th);
	sc_err("set bat ovp th %d %s\n", sc->cfg->bat_ovp_th,
		!ret ? "successfully" : "failed");

	ret = sp2130_set_batocp_th(sc, sc->cfg->bat_ocp_th);
	sc_err("set bat ocp threshold %d %s\n", sc->cfg->bat_ocp_th,
		!ret ? "successfully" : "failed");

	ret = sp2130_set_batocp_alarm_th(sc, sc->cfg->bat_ocp_alm_th);
	sc_err("set bat ocp alarm threshold %d %s\n", sc->cfg->bat_ocp_alm_th,
		!ret ? "successfully" : "failed");

	ret = sp2130_set_busovp_th(sc, sc->cfg->bus_ovp_th);
	sc_err("set bus ovp threshold %d %s\n", sc->cfg->bus_ovp_th,
		!ret ? "successfully" : "failed");

	ret = sp2130_set_busovp_alarm_th(sc, sc->cfg->bus_ovp_alm_th);
	sc_err("set bus ovp alarm threshold %d %s\n", sc->cfg->bus_ovp_alm_th,
		!ret ? "successfully" : "failed");

	ret = sp2130_set_busocp_th(sc, sc->cfg->bus_ocp_th);
	sc_err("set bus ocp threshold %d %s\n", sc->cfg->bus_ocp_th,
		!ret ? "successfully" : "failed");

	ret = sp2130_set_busocp_alarm_th(sc, sc->cfg->bus_ocp_alm_th);
	sc_err("set bus ocp alarm th %d %s\n", sc->cfg->bus_ocp_alm_th,
		!ret ? "successfully" : "failed");

	ret = sp2130_set_reg_ctrl_i2c(sc);
	sc_err("ctlr i2c %s\n",!ret ? "successfully" : "failed");

	ret = sp2130_set_acovp_th(sc, sc->cfg->ac_ovp_th);
	sc_err("set ac ovp threshold %d %s\n", sc->cfg->ac_ovp_th,
		!ret ? "successfully" : "failed");
	return 0;
}

static int sp2130_init_regulation(struct sp2130 *sc)
{
	//sp2130_set_ibat_reg_th(sc, 300);
	//sp2130_set_vbat_reg_th(sc, 100);

	//sp2130_set_vdrop_deglitch(sc, 5000);
	//sp2130_set_vdrop_th(sc, 400);

	//sp2130_enable_regulation(sc, false);
#if 0
	if(sc->is_sp2130)
	{
		sp2130_write_byte(sc, SP2130_REG_2E, 0x08);
		sp2130_write_byte(sc, SP2130_REG_34, 0x01);
	}
#endif
	return 0;
}

static int sp2130_init_device(struct sp2130 *sc)
{
	sp2130_set_reg_reset(sc);
	sp2130_enable_wdt(sc, false);
	sp2130_disable_cp_ts_detect(sc);
	sp2130_set_batocp_alarm_th(sc, 12000);
	sp2130_set_batovp_alarm_th(sc, 5390);

	//sp2130_set_ss_timeout(sc, 10000);
	//sp2130_set_sense_resistor(sc, sc->cfg->sense_r_mohm);

	sp2130_init_protection(sc);
	sp2130_init_adc(sc);
	sp2130_init_int_src(sc);

	sp2130_init_regulation(sc);

	sp2130_write_byte(sc, 0x02, 0xBC);
	sp2130_write_byte(sc, 0x03, 0xBE);
	sp2130_write_byte(sc, 0x04, 0xA8);

	sp2130_write_byte(sc, SP2130_REG_35, 0x01);
	return 0;
}

int sp2130_set_present(struct sp2130 *sc, bool present)
{
	sc->usb_present = present;

	if (present)
		sp2130_init_device(sc);
	return 0;
}

/*
 * block read function
 */
static int sp2130_i2c_read_block(struct sp2130 *bq, u8 reg, u32 len, u8 *data)
{
	int ret;

    mutex_lock(&bq->i2c_rw_lock);
	ret = i2c_smbus_read_i2c_block_data(bq->client, reg, len, data);
	mutex_unlock(&bq->i2c_rw_lock);

	return ret;
}


/*
 * check stat and flag regs for IC status
 */
static void sp2130_check_status_flags(struct sp2130 *bq)
{
    int ret;
	// u8 stat = 0;
	// u8 temp = 0;
	u8 sf_reg[22] = {0};

	pr_err("---------%s---------\n", __func__);

	ret = sp2130_i2c_read_block(bq, SP2130_REG_05, 2, &sf_reg[0]);
	if (ret > 0)
	{
	    if (sf_reg[0] & SP2130_AC1_OVP_STAT_MASK)
			pr_err("REG_05:%x, AC1_OVP_STAT\n", sf_reg[0]);

		if (sf_reg[0] & SP2130_AC1_OVP_FLAG_MASK)
			pr_err("REG_05:%x, AC1_OVP_FLAG\n", sf_reg[0]);

		if (sf_reg[1] & SP2130_AC2_OVP_STAT_MASK)
			pr_err("REG_06:%x, AC2_OVP_STAT\n", sf_reg[1]);

		if (sf_reg[1] & SP2130_AC2_OVP_FLAG_MASK)
			pr_err("REG_06:%x, AC2_OVP_FLAG\n", sf_reg[1]);
	}

	ret = sp2130_i2c_read_block(bq, SP2130_REG_09, 11, &sf_reg[2]);

	if (ret > 0)
	{
	    if (sf_reg[2] & SP2130_IBUS_UCP_RISE_FLAG_MASK)
			pr_err("REG_09:%x, IBUS_UCP_RISE_FLAG\n", sf_reg[2]);

		if (sf_reg[3] & SP2130_IBUS_UCP_FALL_FLAG_MASK)
			pr_err("REG_0A:%x, IBUS_UCP_FALL_FLAG\n", sf_reg[3]);

		if (sf_reg[4] & SP2130_VOUT_OVP_STAT_MASK)
			pr_err("REG_0B:%x, VOUT_OVP_STAT\n", sf_reg[4]);
		if (sf_reg[4] & SP2130_VOUT_OVP_FLAG_MASK)
			pr_err("REG_0B:%x, VOUT_OVP_FLAG\n", sf_reg[4]);
		

		if (sf_reg[5] & SP2130_TSD_FLAG_MASK)
			pr_err("REG_0C:%x, TSD_FLAG\n", sf_reg[5]);
		if (sf_reg[5] & SP2130_TSD_STAT_MASK)
			pr_err("REG_0C:%x, TSD_STAT\n", sf_reg[5]);
		if (sf_reg[5] & SP2130_VBUS_ERRORLO_FLAG_MASK)
			pr_err("REG_0C:%x, VBUS_ERRLO_FLAG\n", sf_reg[5]);
		if (sf_reg[5] & SP2130_VBUS_ERRORHI_FLAG_MASK)
			pr_err("REG_0C:%x, VBUS_ERRHI_FLAG\n", sf_reg[5]);
        	if (sf_reg[5] & SP2130_SS_TIMEOUT_FLAG_MASK)
			pr_err("REG_0C:%x, SS_TIMEOUT_FLAG\n", sf_reg[5]);
		if (sf_reg[5] & SP2130_CONV_SWITCHING_STAT_MASK)
			pr_err("REG_0C:%x, CONV_ACTIVE_STAT\n", sf_reg[5]);
		if (sf_reg[5] & SP2130_PIN_DIAG_FALL_FLAG_MASK)
			pr_err("REG_0C:%x, PIN_DIAG_FAIL_FLAG\n", sf_reg[5]);

		if (sf_reg[6] & SP2130_WD_TIMEOUT_FLAG_MASK)
			pr_err("REG_0D:%x, WD_TIMEOUT_FLAG\n", sf_reg[6]);

		if (sf_reg[8] & SP2130_BAT_OVP_ALM_STAT_MASK)
			pr_err("REG_0F:%x, BAT_OVP_ALM_STAT\n", sf_reg[8]);
		if (sf_reg[8] & SP2130_BAT_OCP_ALM_STAT_MASK)
			pr_err("REG_0F:%x, BAT_OCP_ALM_STAT\n", sf_reg[8]);
		if (sf_reg[8] & SP2130_BUS_OVP_ALM_STAT_MASK)
			pr_err("REG_0F:%x, BUS_OVP_ALM_STAT\n", sf_reg[8]);
		if (sf_reg[8] & SP2130_BUS_OCP_ALM_STAT_MASK)
			pr_err("REG_0F:%x, BUS_OCP_ALM_STAT\n", sf_reg[8]);
		if (sf_reg[8] & SP2130_BAT_UCP_ALM_STAT_MASK)
			pr_err("REG_0F:%x, BAT_UCP_ALM_STAT\n", sf_reg[8]);
		if (sf_reg[8] & SP2130_VBAT_INSERT_STAT_MASK)
			pr_err("REG_0F:%x, VBAT_INSERT_STAT\n", sf_reg[8]);
		if (sf_reg[8] & SP2130_ADC_DONE_STAT_MASK)
			pr_err("REG_0F:%x, ADC_DONE_STAT\n", sf_reg[8]);

		if (sf_reg[9] & SP2130_BAT_OVP_ALM_FLAG_MASK)
			pr_err("REG_10:%x, BAT_OVP_ALM_FLAG\n", sf_reg[9]);
		if (sf_reg[9] & SP2130_BAT_OCP_ALM_FLAG_MASK)
			pr_err("REG_10:%x, BAT_OCP_ALM_FLAG\n", sf_reg[9]);
		if (sf_reg[9] & SP2130_BUS_OVP_ALM_FLAG_MASK)
			pr_err("REG_10:%x, BUS_OVP_ALM_FLAG\n", sf_reg[9]);
		if (sf_reg[9] & SP2130_BUS_OCP_ALM_FLAG_MASK)
			pr_err("REG_10:%x, BUS_OCP_ALM_FLAG\n", sf_reg[9]);
		if (sf_reg[9] & SP2130_BAT_UCP_ALM_FLAG_MASK)
			pr_err("REG_10:%x, BAT_UCP_ALM_FLAG\n", sf_reg[9]);
		if (sf_reg[9] & SP2130_VBAT_INSERT_FLAG_MASK)
			pr_err("REG_10:%x, VBAT_INSERT_FLAG\n", sf_reg[9]);
		if (sf_reg[9] & SP2130_ADC_DONE_FLAG_MASK)
			pr_err("REG_10:%x, ADC_DONE_FLAG\n", sf_reg[9]);

		if (sf_reg[11] & SP2130_BAT_OVP_FLT_STAT_MASK)
			pr_err("REG_12:%x, BAT_OVP_FLT_STAT\n", sf_reg[11]);
		if (sf_reg[11] & SP2130_BAT_OCP_FLT_STAT_MASK)
			pr_err("REG_12:%x, BAT_OCP_FLT_STAT\n", sf_reg[11]);
		if (sf_reg[11] & SP2130_BUS_OVP_FLT_STAT_MASK)
			pr_err("REG_12:%x, BUS_OVP_FLT_STAT\n", sf_reg[11]);
		if (sf_reg[11] & SP2130_BUS_OCP_FLT_STAT_MASK)
			pr_err("REG_12:%x, BUS_OCP_FLT_STAT\n", sf_reg[11]);
		if (sf_reg[11] & SP2130_BUS_RCP_FLT_STAT_MASK)
			pr_err("REG_12:%x, BUS_RCP_FLT_STAT\n", sf_reg[11]);
		if (sf_reg[11] & SP2130_TS_ALM_STAT_MASK)
			pr_err("REG_12:%x, TS_ALM_STAT\n", sf_reg[11]);
		if (sf_reg[11] & SP2130_TS_FLT_STAT_MASK)
			pr_err("REG_12:%x, TS_FLT_STAT\n", sf_reg[11]);
		if (sf_reg[11] & SP2130_TDIE_ALM_STAT_MASK)
			pr_err("REG_12:%x, TDIE_ALM_STAT\n", sf_reg[11]);

		if (sf_reg[12] & SP2130_BAT_OVP_FLT_FLAG_MASK)
			pr_err("REG_13:%x, BAT_OVP_FLT_FLAG\n", sf_reg[12]);
		if (sf_reg[12] & SP2130_BAT_OCP_FLT_FLAG_MASK)
			pr_err("REG_13:%x, BAT_OCP_FLT_FLAG\n", sf_reg[12]);
		if (sf_reg[12] & SP2130_BUS_OVP_FLT_FLAG_MASK)
			pr_err("REG_13:%x, BUS_OVP_FLT_FLAG\n", sf_reg[12]);
		if (sf_reg[12] & SP2130_BUS_OCP_FLT_FLAG_MASK)
			pr_err("REG_13:%x, BUS_OCP_FLT_FLAG\n", sf_reg[12]);
		if (sf_reg[12] & SP2130_BUS_RCP_FLT_FLAG_MASK)
			pr_err("REG_13:%x, BUS_RCP_FLT_FLAG\n", sf_reg[12]);
		if (sf_reg[12] & SP2130_TS_ALM_FLAG_MASK)
			pr_err("REG_13:%x, TS_ALM_FLAG\n", sf_reg[12]);
		if (sf_reg[12] & SP2130_TS_FLT_FLAG_MASK)
			pr_err("REG_13:%x, TS_FLT_FLAG\n", sf_reg[12]);
		if (sf_reg[12] & SP2130_TDIE_ALM_FLAG_MASK)
			pr_err("REG_13:%x, TDIE_ALM_FLAG\n", sf_reg[12]);
	}

	ret = sp2130_i2c_read_block(bq, SP2130_REG_2E, 10, &sf_reg[13]);
	if (ret > 0)
	{
		if (sf_reg[14] & SP2130_VAC1PRESENT_STAT_MASK)
		    pr_err("REG_2F:%x, VAC1PRESENT_STAT\n", sf_reg[14]);
		if (sf_reg[14] & SP2130_VAC1PRESENT_FLAG_MASK)
		    pr_err("REG_2F:%x, VAC1PRESENT_FLAG\n", sf_reg[14]);
		if (sf_reg[14] & SP2130_VAC2PRESENT_STAT_MASK)
		    pr_err("REG_2F:%x, VAC2PRESENT_STAT\n", sf_reg[14]);
		if (sf_reg[14] & SP2130_VAC2PRESENT_FLAG_MASK)
		    pr_err("REG_2F:%x, VAC2PRESENT_FLAG\n", sf_reg[14]);

		if (sf_reg[15] & SP2130_ACRB1_STAT_MASK)
		    pr_err("REG_30:%x, ACRB1_STAT\n", sf_reg[15]);
		if (sf_reg[15] & SP2130_ACRB1_FLAG_MASK)
		    pr_err("REG_30:%x, ACRB1_FLAG\n", sf_reg[15]);
		if (sf_reg[15] & SP2130_ACRB2_STAT_MASK)
		    pr_err("REG_30:%x, ACRB2_STAT\n", sf_reg[15]);
		if (sf_reg[15] & SP2130_ACRB2_FLAG_MASK)
		    pr_err("REG_30:%x, ACRB2_FLAG\n", sf_reg[15]);

        	if (sf_reg[17] & SP2130_PMID2VOUT_UVP_FLAG_MASK)
		    pr_err("REG_32:%x, PMID2VOUT_UVP_FLAG\n", sf_reg[17]);

		if (sf_reg[17] & SP2130_PMID2VOUT_OVP_FLAG_MASK)
		    pr_err("REG_32:%x, PMID2VOUT_OVP_FLAG\n", sf_reg[17]);

		if (sf_reg[19] & SP2130_POWER_NG_FLAG_MASK)
		    pr_err("REG_34:%x, POWER_NG_FLAG\n", sf_reg[19]);

		if (sf_reg[21] & SP2130_VBUS_PRESENT_STAT_MASK)
		    pr_err("REG_36:%x, VBUS_PRESENT_STAT\n", sf_reg[21]);
		if (sf_reg[21] & SP2130_VBUS_PRESENT_FLAG_MASK)
		    pr_err("REG_36:%x, VBUS_PRESENT_FLAG\n", sf_reg[21]);
	}

}


/*
 * interrupt does nothing, just info event chagne, other module could get info
 * through power supply interface
 */
static irqreturn_t sp2130_charger_interrupt(int irq, void *dev_id)
{
	struct sp2130 *sc = dev_id;
	// int i;
	// u8 stat = 0;

	sc_dbg("INT OCCURED\n");
	mutex_lock(&sc->irq_complete);
	sp2130_check_status_flags(sc);
	mutex_unlock(&sc->irq_complete);

	return IRQ_HANDLED;
}

static enum power_supply_property sp2130_charger_props[] = {
    POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
    POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
    POWER_SUPPLY_PROP_STATUS,
#if 0
    POWER_SUPPLY_PROP_CHARGING_ENABLED,

    POWER_SUPPLY_PROP_UPM_BUS_VOLTAGE,
    POWER_SUPPLY_PROP_UPM_BUS_CURRENT,
    POWER_SUPPLY_PROP_UPM_BAT_VOLTAGE,
    POWER_SUPPLY_PROP_UPM_BAT_CURRENT,
    POWER_SUPPLY_PROP_UPM_AC1_VOLTAGE,
    POWER_SUPPLY_PROP_UPM_AC2_VOLTAGE,
    POWER_SUPPLY_PROP_UPM_OUT_VOLTAGE,
    POWER_SUPPLY_PROP_UPM_DIE_TEMP,
#endif
};

static int sp2130_charger_get_property(struct power_supply *psy,
                enum power_supply_property psp,
                union power_supply_propval *val)
{
    struct sp2130 *sc = power_supply_get_drvdata(psy);
    int result;
    int ret = 0;
    bool charging_enabled;

    switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		ret = 0;
        break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = sp2130_get_adc_data(sc, ADC_VBAT, &result);
        if (!ret)
            val->intval =  result;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = sp2130_get_adc_data(sc, ADC_VBUS, &result);
        if (!ret)
            sc->vbus_volt = result;
        val->intval = sc->vbus_volt;
        break;
	case POWER_SUPPLY_PROP_STATUS:
        ret = sp2130_check_charge_enabled(sc, &charging_enabled);
        if (ret)
            val->intval =  POWER_SUPPLY_STATUS_UNKNOWN;
        else
            val->intval = charging_enabled ? POWER_SUPPLY_STATUS_CHARGING : POWER_SUPPLY_STATUS_NOT_CHARGING;
        break;

    default:
        return -EINVAL;
    }

	if (ret)
		pr_err("logc: psp:%d, ret:%d", psp, ret);

    return ret;
}

static int sp2130_charger_set_property(struct power_supply *psy,
                    enum power_supply_property prop,
                    const union power_supply_propval *val)
{
    struct sp2130 *sc = power_supply_get_drvdata(psy);

    switch (prop) {
    case POWER_SUPPLY_PROP_PRESENT:
        sp2130_set_present(sc, !!val->intval);
        break;
    default:
        return -EINVAL;
    }

    return 0;
}

static int sp2130_charger_is_writeable(struct power_supply *psy,
                    enum power_supply_property prop)
{
    int ret = 0;

    switch (prop) {
    default:
        ret = 0;
        break;
    }
    return ret;
}

static int sp2130_psy_register(struct sp2130 *sc)
{
    sc->psy_cfg.drv_data = sc;
    sc->psy_cfg.of_node = sc->dev->of_node;
	sc->psy_desc.name = "charger_standalone";

    sc->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
    sc->psy_desc.properties = sp2130_charger_props;
    sc->psy_desc.num_properties = ARRAY_SIZE(sp2130_charger_props);
    sc->psy_desc.get_property = sp2130_charger_get_property;
    sc->psy_desc.set_property = sp2130_charger_set_property;
    sc->psy_desc.property_is_writeable = sp2130_charger_is_writeable;

    sc->fc2_psy = devm_power_supply_register(sc->dev, 
            &sc->psy_desc, &sc->psy_cfg);
    if (IS_ERR(sc->fc2_psy)) {
        pr_err("failed to register fc2_psy\n");
        return PTR_ERR(sc->fc2_psy);
    }

    pr_err("%s power supply register successfully\n", sc->psy_desc.name);

    return 0;
}

static int sp2130_parse_dt(struct sp2130 *sc, struct device *dev)
{
	int ret;
	struct device_node *np = dev->of_node;

	sc->cfg = devm_kzalloc(dev, sizeof(struct sp2130_cfg),
					GFP_KERNEL);

	if (!sc->cfg)
		return -ENOMEM;

	sc->cfg->bat_ovp_disable = of_property_read_bool(np,
			"nvt,sp2130,bat-ovp-disable");
	sc->cfg->bat_ocp_disable = of_property_read_bool(np,
			"nvt,sp2130,bat-ocp-disable");
	sc->cfg->bus_ocp_disable = of_property_read_bool(np,
			"nvt,sp2130,bus-ocp-disable");

	ret = of_property_read_u32(np, "nvt,sp2130,bat-ovp-threshold",
			&sc->cfg->bat_ovp_th);
	if (ret) {
		sc_err("failed to read bat-ovp-threshold\n");
		return ret;
	}
	ret = of_property_read_u32(np, "nvt,sp2130,bat-ocp-threshold",
			&sc->cfg->bat_ocp_th);
	if (ret) {
		sc_err("failed to read bat-ocp-threshold\n");
		return ret;
	}
	ret = of_property_read_u32(np, "nvt,sp2130,bus-ovp-threshold",
			&sc->cfg->bus_ovp_th);
	if (ret) {
		sc_err("failed to read bus-ovp-threshold\n");
		return ret;
	}
	ret = of_property_read_u32(np, "nvt,sp2130,bus-ovp-alarm-threshold",
			&sc->cfg->bus_ovp_alm_th);
	if (ret) {
		sc_err("failed to read bus-ovp-alarm-threshold\n");
		return ret;
	}
	ret = of_property_read_u32(np, "nvt,sp2130,bus-ocp-threshold",
			&sc->cfg->bus_ocp_th);
	if (ret) {
		sc_err("failed to read bus-ocp-threshold\n");
		return ret;
	}
	ret = of_property_read_u32(np, "nvt,sp2130,bus-ocp-alarm-threshold",
					&sc->cfg->bus_ocp_alm_th);
	if (ret) {
		sc_err("failed to read bus-ocp-alarm-threshold\n");
		return ret;
	}
	ret = of_property_read_u32(np, "nvt,sp2130,ac-ovp-threshold",
			&sc->cfg->ac_ovp_th);
	if (ret) {
		sc_err("failed to read ac-ovp-threshold\n");
		return ret;
	}
#if 0
	ret = of_property_read_u32(np, "ti,sp2130,sense-resistor-mohm",
			&sc->cfg->sense_r_mohm);
	if (ret) {
		sc_err("failed to read sense-resistor-mohm\n");
		return ret;
	}
#endif
	return 0;
}

static void determine_initial_status(struct sp2130 *sc)
{
	if (sc->client->irq)
		sp2130_charger_interrupt(sc->client->irq, sc);
}

static struct of_device_id sp2130_charger_match_table[] = {
	{
		.compatible = "nvt,sp2130-standalone",
		.data = &sp2130_mode_data[SP2130_STDALONE],
	},
	{
		.compatible = "nvt,sp2130-master",
		.data = &sp2130_mode_data[SP2130_MASTER],
	},

	{
		.compatible = "nvt,sp2130-slave",
		.data = &sp2130_mode_data[SP2130_SLAVE],
	},
	{},
};
MODULE_DEVICE_TABLE(of, sp2130_charger_match_table);

static int sp2130_irq_init(struct sp2130 *sc)
{
	int irq;
  	int irq_gpio;
	int ret = 0;
	irq_gpio = of_get_named_gpio(sc->dev->of_node, "sp2130,irq-gpio", 0);
	dev_info(sc->dev, "%s irq_gpio = %d\n", __func__, irq_gpio);
    	if (!gpio_is_valid(irq_gpio)) {
        	pr_err("fail to valid gpio : %d\n", irq_gpio);
        	return -EINVAL;
    	}
	ret = gpio_request_one(irq_gpio, GPIOF_DIR_IN, "sp2130_irq");
	irq = gpio_to_irq(irq_gpio);
	sc->client->irq = irq;
	if (irq < 0) {
		dev_err(sc->dev, "%s irq mapping fail(%d)\n", __func__, irq);
		return ret;
	}
	dev_info(sc->dev, "%s irq = %d\n", __func__, irq);


	ret = devm_request_threaded_irq(sc->dev, irq, NULL,
					sp2130_charger_interrupt,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					"sp2130_irq", sc);
	if (ret) {
		dev_err(sc->dev, "failed to request irq %d\n", irq);
		return ret;
	}

	return ret;
}


static int sp2130_charger_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	struct sp2130 *sc;
	const struct of_device_id *match;
	struct device_node *node = client->dev.of_node;
	struct iio_dev *indio_dev;
	int ret;

	indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*sc));
	sc = iio_priv(indio_dev);
	sc->indio_dev = indio_dev;
	sc->dev = &client->dev;
	sc->client = client;

	mutex_init(&sc->i2c_rw_lock);
	mutex_init(&sc->data_lock);
	mutex_init(&sc->charging_disable_lock);
	mutex_init(&sc->irq_complete);

	sc->resume_completed = true;
	sc->irq_waiting = false;
	sc->is_sp2130 = true;

	ret = sp2130_detect_device(sc);

	if (ret) {
		sc_err("No sp2130 device found!\n");
		return -ENODEV;
	}
	sc_err("%s %d\n", __func__, __LINE__);

	i2c_set_clientdata(client, sc);
	sp2130_create_device_node(&(client->dev));

	match = of_match_node(sp2130_charger_match_table, node);
	if (match == NULL) {
		sc_err("device tree match not found!\n");
		return -ENODEV;
	}
/*
	sp2130_get_work_mode(sc, &sc->mode);

	if (sc->mode !=  *(int *)match->data) {
		sc_err("device operation mode mismatch with dts configuration\n");
		return -EINVAL;
	}
*/
	sc->mode =  *(int *)match->data;
	ret = sp2130_parse_dt(sc, &client->dev);
	//if (ret)
	//	return -EIO;

	sc_err("---debug: %s %d\n", __func__, __LINE__);
	ret = sp2130_init_device(sc);
	if (ret) {
		sc_err("Failed to init device\n");
	//	return ret;
	}

	ret = sp2130_init_iio_psy(sc);
	if (ret)
		return ret;
	sc_err("%s %d\n", __func__, __LINE__);
  
	ret = sp2130_irq_init(sc);
	if (ret)
		return ret;
	sc_err("%s %d\n", __func__, __LINE__);

	ret =  sp2130_psy_register(sc);
	sp2130_dump_reg(sc);

	if (ret)
		pr_err("failed to register psy. err: %d\n", ret);
		
	device_init_wakeup(sc->dev, 1);
	determine_initial_status(sc);
	sp2130_conf_en = true;
	g_sc = sc;
	sc_err("sp2130 probe successfully, Part Num:%d\n!", sc->part_no);

	return 0;
}

static inline bool is_device_suspended(struct sp2130 *sc)
{
	return !sc->resume_completed;
}

static int sp2130_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sp2130 *sc = i2c_get_clientdata(client);

	mutex_lock(&sc->irq_complete);
	sc->resume_completed = false;
	mutex_unlock(&sc->irq_complete);
	//sp2130_enable_adc(sc, false);
	sc->adc_status = 0;
	sc_err("Suspend successfully!");

	return 0;
}

static int sp2130_suspend_noirq(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sp2130 *sc = i2c_get_clientdata(client);

	if (sc->irq_waiting) {
		pr_err_ratelimited("Aborting suspend, an interrupt was detected while suspending\n");
		return -EBUSY;
	}
	return 0;
}

static int sp2130_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sp2130 *sc = i2c_get_clientdata(client);


	mutex_lock(&sc->irq_complete);
	sc->resume_completed = true;
#if 0
	if (sc->irq_waiting) {
		sc->irq_disabled = false;
		enable_irq(client->irq);
		mutex_unlock(&sc->irq_complete);
		sp2130_charger_interrupt(client->irq, sc);
	} else {
		mutex_unlock(&sc->irq_complete);
	}
#else
	mutex_unlock(&sc->irq_complete);
#endif

	sc_err("Resume successfully!");
	sp2130_enable_adc(sc, true);

	return 0;
}
static int sp2130_charger_remove(struct i2c_client *client)
{
	struct sp2130 *sc = i2c_get_clientdata(client);


	sp2130_enable_adc(sc, false);
	sc->adc_status = 0;
	mutex_destroy(&sc->charging_disable_lock);
	mutex_destroy(&sc->data_lock);
	mutex_destroy(&sc->i2c_rw_lock);
	mutex_destroy(&sc->irq_complete);

	return 0;
}

static void sp2130_charger_shutdown(struct i2c_client *client)
{
	struct sp2130 *sc = i2c_get_clientdata(client);

	sp2130_enable_adc(sc, false);
	sc->adc_status = 0;
}

static const struct dev_pm_ops sp2130_pm_ops = {
	.resume		= sp2130_resume,
	.suspend_noirq = sp2130_suspend_noirq,
	.suspend	= sp2130_suspend,
};

static const struct i2c_device_id sp2130_charger_id[] = {
	{"sp2130-standalone", SP2130_STDALONE},
	{},
};
MODULE_DEVICE_TABLE(i2c, sp2130_charger_id);

static struct i2c_driver sp2130_charger_driver = {
	.driver		= {
		.name	= "sp2130-charger",
		.owner	= THIS_MODULE,
		.of_match_table = sp2130_charger_match_table,
		.pm	= &sp2130_pm_ops,
	},
	.id_table	= sp2130_charger_id,

	.probe		= sp2130_charger_probe,
	.remove		= sp2130_charger_remove,
	.shutdown	= sp2130_charger_shutdown,
};

module_i2c_driver(sp2130_charger_driver);

MODULE_DESCRIPTION("NVT SP2130 Charge Pump Driver");
MODULE_LICENSE("GPL v2");
