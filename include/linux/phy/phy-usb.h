/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * phy-usb.h -- specific phy header file for usb driver
 *
 */

#include <linux/phy/phy-mipi-dphy.h>

struct phy;

enum phy_mode {
	PHY_MODE_INVALID,
	PHY_MODE_USB_HOST,
	PHY_MODE_USB_HOST_LS,
	PHY_MODE_USB_HOST_FS,
	PHY_MODE_USB_HOST_HS,
	PHY_MODE_USB_HOST_SS,
	PHY_MODE_USB_DEVICE,
	PHY_MODE_USB_DEVICE_LS,
	PHY_MODE_USB_DEVICE_FS,
	PHY_MODE_USB_DEVICE_HS,
	PHY_MODE_USB_DEVICE_SS,
	PHY_MODE_USB_OTG,
	PHY_MODE_UFS_HS_A,
	PHY_MODE_UFS_HS_B,
	PHY_MODE_PCIE,
	PHY_MODE_ETHERNET,
	PHY_MODE_MIPI_DPHY,
	PHY_MODE_SATA,
	PHY_MODE_LVDS,
	PHY_MODE_DP
};

enum phy_media {
	PHY_MEDIA_DEFAULT,
	PHY_MEDIA_SR,
	PHY_MEDIA_DAC,
};

struct phy_configure_opts_dp {
	/**
	 * @link_rate:
	 *
	 * Link Rate, in Mb/s, of the main link.
	 *
	 * Allowed values: 1620, 2160, 2430, 2700, 3240, 4320, 5400, 8100 Mb/s
	 */
	unsigned int link_rate;

	/**
	 * @lanes:
	 *
	 * Number of active, consecutive, data lanes, starting from
	 * lane 0, used for the transmissions on main link.
	 *
	 * Allowed values: 1, 2, 4
	 */
	unsigned int lanes;

	/**
	 * @voltage:
	 *
	 * Voltage swing levels, as specified by DisplayPort specification,
	 * to be used by particular lanes. One value per lane.
	 * voltage[0] is for lane 0, voltage[1] is for lane 1, etc.
	 *
	 * Maximum value: 3
	 */
	unsigned int voltage[4];

	/**
	 * @pre:
	 *
	 * Pre-emphasis levels, as specified by DisplayPort specification, to be
	 * used by particular lanes. One value per lane.
	 *
	 * Maximum value: 3
	 */
	unsigned int pre[4];

	/**
	 * @ssc:
	 *
	 * Flag indicating, whether or not to enable spread-spectrum clocking.
	 *
	 */
	u8 ssc : 1;

	/**
	 * @set_rate:
	 *
	 * Flag indicating, whether or not reconfigure link rate and SSC to
	 * requested values.
	 *
	 */
	u8 set_rate : 1;

	/**
	 * @set_lanes:
	 *
	 * Flag indicating, whether or not reconfigure lane count to
	 * requested value.
	 *
	 */
	u8 set_lanes : 1;

	/**
	 * @set_voltages:
	 *
	 * Flag indicating, whether or not reconfigure voltage swing
	 * and pre-emphasis to requested values. Only lanes specified
	 * by "lanes" parameter will be affected.
	 *
	 */
	u8 set_voltages : 1;
};

union phy_configure_opts {
	struct phy_configure_opts_mipi_dphy	mipi_dphy;
	struct phy_configure_opts_dp		dp;
};

struct phy_ops {
	int	(*init)(struct phy *phy);
	int	(*exit)(struct phy *phy);
	int	(*power_on)(struct phy *phy);
	int	(*power_off)(struct phy *phy);
	int	(*set_mode)(struct phy *phy, enum phy_mode mode, int submode);
	int	(*set_media)(struct phy *phy, enum phy_media media);
	int	(*set_speed)(struct phy *phy, int speed);

	/**
	 * @configure:
	 *
	 * Optional.
	 *
	 * Used to change the PHY parameters. phy_init() must have
	 * been called on the phy.
	 *
	 * Returns: 0 if successful, an negative error code otherwise
	 */
	int	(*configure)(struct phy *phy, union phy_configure_opts *opts);

	/**
	 * @validate:
	 *
	 * Optional.
	 *
	 * Used to check that the current set of parameters can be
	 * handled by the phy. Implementations are free to tune the
	 * parameters passed as arguments if needed by some
	 * implementation detail or constraints. It must not change
	 * any actual configuration of the PHY, so calling it as many
	 * times as deemed fit by the consumer must have no side
	 * effect.
	 *
	 * Returns: 0 if the configuration can be applied, an negative
	 * error code otherwise
	 */
	int	(*validate)(struct phy *phy, enum phy_mode mode, int submode,
			    union phy_configure_opts *opts);
	int	(*reset)(struct phy *phy);
	int	(*calibrate)(struct phy *phy);
	void	(*release)(struct phy *phy);
	struct module *owner;
};

struct phy_attrs {
	u32			bus_width;
	u32			max_link_rate;
	enum phy_mode		mode;
};

struct phy {
	struct device		dev;
	int			id;
	const struct phy_ops	*ops;
	struct mutex		mutex;
	int			init_count;
	int			power_count;
	struct phy_attrs	attrs;
	struct regulator	*pwr;
};

int phy_set_mode_ext(struct phy *phy, enum phy_mode mode, int submode);
