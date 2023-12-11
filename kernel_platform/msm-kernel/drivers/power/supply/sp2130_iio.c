
#include "inc/sp2130.h"
#include "inc/sp2130_reg.h"
#include "inc/sp2130_iio.h"

static int sp2130_iio_write_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int val1,
		int val2, long mask)
{
	struct sp2130 *sc = iio_priv(indio_dev);
	int rc = 0;

	switch (chan->channel) {
	case PSY_IIO_CHARGING_ENABLED:
		sp2130_enable_charge(sc, val1);
		sp2130_check_charge_enabled(sc, &sc->charge_enabled);
		sc_info("POWER_SUPPLY_PROP_CHARGING_ENABLED: %s\n",
				val1 ? "enable" : "disable");
		break;
	case PSY_IIO_PRESENT:
		sp2130_set_present(sc, !!val1);
		break;
#if 0
	case PSY_IIO_SP2130_ENABLE_ADC:
		sp2130_enable_adc(sc, !!val1);
		sc->adc_status = !!val1;
		break;
	case PSY_IIO_SP2130_ACDRV1_ENABLED:
		sp2130_enable_acdrv1(sc, val1);
		sc_info("POWER_SUPPLY_PROP_ACDRV1_ENABLED: %s\n",
				val1 ? "enable" : "disable");
	case PSY_IIO_SP2130_ENABLE_OTG:
		sp2130_enable_otg(sc, val1);
		sc_info("POWER_SUPPLY_PROP_OTG_ENABLED: %s\n",
				val1 ? "enable" : "disable");
#endif
	default:
		pr_debug("Unsupported SP2130 IIO chan %d\n", chan->channel);
		rc = -EINVAL;
		break;
	}

	if (rc < 0)
		pr_err("Couldn't write IIO channel %d, rc = %d\n",
			chan->channel, rc);

	return rc;
}

static int sp2130_iio_read_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int *val1,
		int *val2, long mask)
{
	struct sp2130 *sc = iio_priv(indio_dev);
	int result = 0;
	int ret = 0;
	u8 reg_val;
	*val1 = 0;

	switch (chan->channel) {
	case PSY_IIO_CHARGING_ENABLED:
		sp2130_check_charge_enabled(sc, &sc->charge_enabled);
		*val1 = sc->charge_enabled;
		break;
	case PSY_IIO_STATUS:
		*val1 = 0;
		break;
	case PSY_IIO_PRESENT:
		*val1 = 1;
		break;
	case PSY_IIO_SP2130_BATTERY_PRESENT:
		ret = sp2130_read_byte(sc, SP2130_REG_0F, &reg_val);
		if (!ret)
			sc->batt_present  = !!(reg_val & VBAT_INSERT);
		*val1 = sc->batt_present;
		break;
	case PSY_IIO_SP2130_VBUS_PRESENT:
		ret = sp2130_read_byte(sc, SP2130_REG_36, &reg_val);
		if (!ret)
			sc->vbus_present  = !!(reg_val & VBUS_INSERT);
		*val1 = sc->vbus_present;
		break;
	case PSY_IIO_SP2130_BATTERY_VOLTAGE:
		ret = sp2130_get_adc_data(sc, ADC_VBAT, &result);
		if (!ret)
			sc->vbat_volt = result;
		*val1 = sc->vbat_volt;
		break;
	case PSY_IIO_SP2130_BATTERY_CURRENT:
		ret = sp2130_get_adc_data(sc, ADC_IBAT, &result);
		if (!ret)
			sc->ibat_curr = result;

		*val1 = sc->ibat_curr;
		break;
	case PSY_IIO_SP2130_BATTERY_TEMPERATURE:
		ret = sp2130_get_adc_data(sc, ADC_TSBAT, &result);
		if (!ret)
			sc->bat_temp = result;

		*val1 = sc->bat_temp;
		break;
	case PSY_IIO_SP2130_BUS_VOLTAGE:
		ret = sp2130_get_adc_data(sc, ADC_VBUS, &result);
		if (!ret)
			sc->vbus_volt = result;

		*val1 = sc->vbus_volt;
		break;
	case PSY_IIO_SP2130_BUS_CURRENT:
		ret = sp2130_get_adc_data(sc, ADC_IBUS, &result);
		if (!ret)
			sc->ibus_curr = result;

		*val1 = sc->ibus_curr;
		break;
	case PSY_IIO_SP2130_BUS_TEMPERATURE:
		ret = sp2130_get_adc_data(sc, ADC_TSBUS, &result);
		if (!ret)
			sc->bus_temp = result;

		*val1 = sc->bus_temp;
		break;
	case PSY_IIO_SP2130_DIE_TEMPERATURE:
		ret = sp2130_get_adc_data(sc, ADC_TDIE, &result);
		if (!ret)
			sc->die_temp = result;

		*val1= sc->die_temp;
		break;

	case PSY_IIO_SP2130_ALARM_STATUS:
		sp2130_check_alarm_status(sc);
		*val1 = ((sc->bat_ovp_alarm << BAT_OVP_ALARM_SHIFT)
			| (sc->bat_ocp_alarm << BAT_OCP_ALARM_SHIFT)
			| (sc->bat_ucp_alarm << BAT_UCP_ALARM_SHIFT)
			| (sc->bus_ovp_alarm << BUS_OVP_ALARM_SHIFT)
			| (sc->bus_ocp_alarm << BUS_OCP_ALARM_SHIFT)
			| (sc->bat_therm_alarm << BAT_THERM_ALARM_SHIFT)
			| (sc->bus_therm_alarm << BUS_THERM_ALARM_SHIFT)
			| (sc->die_therm_alarm << DIE_THERM_ALARM_SHIFT));
		break;
	case PSY_IIO_SP2130_FAULT_STATUS:
		sp2130_check_fault_status(sc);
		*val1 = ((sc->bat_ovp_fault << BAT_OVP_FAULT_SHIFT)
			| (sc->bat_ocp_fault << BAT_OCP_FAULT_SHIFT)
			| (sc->bus_ovp_fault << BUS_OVP_FAULT_SHIFT)
			| (sc->bus_ocp_fault << BUS_OCP_FAULT_SHIFT)
			| (sc->bat_therm_fault << BAT_THERM_FAULT_SHIFT)
			| (sc->bus_therm_fault << BUS_THERM_FAULT_SHIFT)
			| (sc->die_therm_fault << DIE_THERM_FAULT_SHIFT));
		break;
	case PSY_IIO_SP2130_VBUS_ERROR_STATUS:
		sp2130_check_vbus_error_status(sc);
		*val1 = sc->vbus_error;
		break;
#if 0
	case PSY_IIO_SP2130_ENABLE_ADC:
		*val1 = sc->adc_status;
		break;
	case PSY_IIO_SP2130_ACDRV1_ENABLED:
		sp2130_check_enable_acdrv1(sc, &sc->acdrv1_enable);
		*val1 = sc->acdrv1_enable;
		sc_info("check acdrv1 enable: %d\n", *val1);
	case PSY_IIO_SP2130_ENABLE_OTG:
		sp2130_check_enable_otg(sc, &sc->otg_enable);
		*val1 = sc->otg_enable;
		sc_info("check otg enable: %d\n", *val1);
#endif
	default:
		pr_debug("Unsupported QG IIO chan %d\n", chan->channel);
		ret = -EINVAL;
		break;
	}

	if (ret < 0) {
		pr_err("Couldn't read IIO channel %d, ret = %d\n",
			chan->channel, ret);
		return ret;
	}

	return IIO_VAL_INT;
}

static int sp2130_iio_of_xlate(struct iio_dev *indio_dev,
				const struct of_phandle_args *iiospec)
{
	struct sp2130 *chip = iio_priv(indio_dev);
	struct iio_chan_spec *iio_chan = chip->iio_chan;
	int i;

	for (i = 0; i < ARRAY_SIZE(sp2130_iio_psy_channels);
					i++, iio_chan++) {
		//pr_err("sp2130_iio_of_xlate: iio_chan->channel %d, iiospec->args[0]:%d\n",iio_chan->channel,iiospec->args[0]);
		if (iio_chan->channel == iiospec->args[0])
			return i;
	}
	return -EINVAL;
}

static const struct iio_info sc_iio_info = {
	.read_raw	= sp2130_iio_read_raw,
	.write_raw	= sp2130_iio_write_raw,
	.of_xlate	= sp2130_iio_of_xlate,
};

int sp2130_init_iio_psy(struct sp2130 *chip)
{
	struct iio_dev *indio_dev = chip->indio_dev;
	struct iio_chan_spec *chan;
	int num_iio_channels = ARRAY_SIZE(sp2130_iio_psy_channels);
	int rc, i;

	pr_err("SP2130 sp2130_init_iio_psy start\n");
	chip->iio_chan = devm_kcalloc(chip->dev, num_iio_channels,
				sizeof(*chip->iio_chan), GFP_KERNEL);
	if (!chip->iio_chan)
		return -ENOMEM;

	chip->int_iio_chans = devm_kcalloc(chip->dev,
				num_iio_channels,
				sizeof(*chip->int_iio_chans),
				GFP_KERNEL);
	if (!chip->int_iio_chans)
		return -ENOMEM;

	indio_dev->info = &sc_iio_info;
	indio_dev->dev.parent = chip->dev;
	indio_dev->dev.of_node = chip->dev->of_node;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = chip->iio_chan;
	indio_dev->num_channels = num_iio_channels;
#if 0
	if (chip->mode == SP2130_ROLE_MASTER) {
		indio_dev->name = "sp2130-master";
		for (i = 0; i < num_iio_channels; i++) {
			chip->int_iio_chans[i].indio_dev = indio_dev;
			chan = &chip->iio_chan[i];
			chip->int_iio_chans[i].channel = chan;
			chan->address = i;
			chan->channel = sp2130_iio_psy_channels[i].channel_num;
			chan->type = sp2130_iio_psy_channels[i].type;
			chan->datasheet_name =
				sp2130_iio_psy_channels[i].datasheet_name;
			chan->extend_name =
				sp2130_iio_psy_channels[i].datasheet_name;
			chan->info_mask_separate =
				sp2130_iio_psy_channels[i].info_mask;
		}
	} else if (chip->mode == SP2130_ROLE_SLAVE) {
		indio_dev->name = "sp2130-slave";
		for (i = 0; i < num_iio_channels; i++) {
			chip->int_iio_chans[i].indio_dev = indio_dev;
			chan = &chip->iio_chan[i];
			chip->int_iio_chans[i].channel = chan;
			chan->address = i;
			chan->channel = sp2130_slave_iio_psy_channels[i].channel_num;
			chan->type = sp2130_slave_iio_psy_channels[i].type;
			chan->datasheet_name =
				sp2130_slave_iio_psy_channels[i].datasheet_name;
			chan->extend_name =
				sp2130_slave_iio_psy_channels[i].datasheet_name;
			chan->info_mask_separate =
				sp2130_slave_iio_psy_channels[i].info_mask;
		}
	} else {
#endif
		indio_dev->name = "sp2130-standalone";
		for (i = 0; i < num_iio_channels; i++) {
			chip->int_iio_chans[i].indio_dev = indio_dev;
			chan = &chip->iio_chan[i];
			chip->int_iio_chans[i].channel = chan;
			chan->address = i;
			chan->channel = sp2130_iio_psy_channels[i].channel_num;
			chan->type = sp2130_iio_psy_channels[i].type;
			chan->datasheet_name =
				sp2130_iio_psy_channels[i].datasheet_name;
			chan->extend_name =
				sp2130_iio_psy_channels[i].datasheet_name;
			chan->info_mask_separate =
				sp2130_iio_psy_channels[i].info_mask;
		}
//	}

	rc = devm_iio_device_register(chip->dev, indio_dev);
	if (rc)
		pr_err("Failed to register SP2130 IIO device, rc=%d\n", rc);

	pr_err("SP2130 IIO device, rc=%d\n", rc);
	return rc;
}
