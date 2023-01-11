#include <linux/kernel.h>
#include <linux/device.h>
#include <media/mv_sc2_twsi_conf.h>
#include <media/mv_sc2.h>

static int twsi_owner;
static DEFINE_MUTEX(sc2_mutex);

int sc2_select_pins_state(int id, int pin_state, int sc2_mod)
{
	struct device *dev;
	struct pinctrl *pin = NULL;
	struct msc2_ccic_dev *ccic_dev = NULL;
	int ret = 0;

	ret = msc2_get_ccic_dev(&ccic_dev, id);
	if (ret)
		return -EINVAL;
	dev = &ccic_dev->pdev->dev;

	mutex_lock(&sc2_mutex);

	if (pin_state == SC2_PIN_ST_TWSI)
		pin = pinctrl_get_select(dev, "twsi");
	else if (pin_state == SC2_PIN_ST_SCCB)
		pin = pinctrl_get_select(dev, "sccb");
	else if (pin_state == SC2_PIN_ST_GPIO)
		pin = pinctrl_get_select(dev, PINCTRL_STATE_DEFAULT);

	ret = IS_ERR(pin);
	if (ret < 0) {
		dev_err(dev, "could not configure pins");
		goto out;
	}

	if (pin_state == SC2_PIN_ST_TWSI)
		twsi_owner = sc2_mod;
	else if (pin_state == SC2_PIN_ST_SCCB)
		twsi_owner = sc2_mod;
	else
		twsi_owner = 0;

out:
	mutex_unlock(&sc2_mutex);
	return ret;
}
