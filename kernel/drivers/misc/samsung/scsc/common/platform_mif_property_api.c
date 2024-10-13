#include "platform_mif_property_api.h"

#ifdef CONFIG_WLBT_KUNIT
#include "../kunit/kunit_platform_mif_property_api.c"
#endif

static bool platform_mif_wlbt_property_read_bool(struct scsc_mif_abs *interface, const char *propname)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Get property: %s\n", propname);

	return of_property_read_bool(platform->np, propname);
}

static int platform_mif_wlbt_property_read_u8(struct scsc_mif_abs *interface,
					      const char *propname, u8 *out_value, size_t size)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Get property: %s\n", propname);

	if (of_property_read_u8_array(platform->np, propname, out_value, size)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Property doesn't exist\n");
		return -EINVAL;
	}
	return 0;
}

static int platform_mif_wlbt_property_read_u16(struct scsc_mif_abs *interface,
					       const char *propname, u16 *out_value, size_t size)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Get property: %s\n", propname, size);

	if (of_property_read_u16_array(platform->np, propname, out_value, size)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Property doesn't exist\n");
		return -EINVAL;
	}
	return 0;
}

static int platform_mif_wlbt_property_read_u32(struct scsc_mif_abs *interface,
					       const char *propname, u32 *out_value, size_t size)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Get property: %s\n", propname);

	if (of_property_read_u32_array(platform->np, propname, out_value, size)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Property doesn't exist\n");
		return -EINVAL;
	}
	return 0;
}

static int platform_mif_wlbt_property_read_string(struct scsc_mif_abs *interface,
						  const char *propname, char **out_value, size_t size)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	int ret;

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Get property: %s\n", propname);


	ret = of_property_read_string_array(platform->np, propname, (const char **)out_value, size);
	if (ret <= 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Property doesn't exist (ret=%d)\n", ret);
		return ret;
	}
	return ret;
}

#if defined(SCSC_SEP_VERSION)
static int platform_mif_wlbt_phandle_property_read_u32(struct scsc_mif_abs *interface,
				const char *phandle_name, const char *propname, u32 *out_value, size_t size)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	struct device_node *np;
	int err = 0;

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Get phandle: %s\n", phandle_name);
	np = of_parse_phandle(platform->dev->of_node, phandle_name, 0);
	if (np) {
		SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Get property: %s\n", propname);
		if (of_property_read_u32(np, propname, out_value)) {
			SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Property doesn't exist\n");
			err = -EINVAL;
		}
	} else {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "phandle doesn't exist\n");
		err = -EINVAL;
	}

	return err;
}
#endif

void platform_mif_wlbt_property_read_init(struct scsc_mif_abs *interface)
{
	interface->wlbt_property_read_bool = platform_mif_wlbt_property_read_bool;
	interface->wlbt_property_read_u8 = platform_mif_wlbt_property_read_u8;
	interface->wlbt_property_read_u16 = platform_mif_wlbt_property_read_u16;
	interface->wlbt_property_read_u32 = platform_mif_wlbt_property_read_u32;
	interface->wlbt_property_read_string = platform_mif_wlbt_property_read_string;
#if defined(SCSC_SEP_VERSION)
	interface->wlbt_phandle_property_read_u32 = platform_mif_wlbt_phandle_property_read_u32;
#else
	interface->wlbt_phandle_property_read_u32 = NULL;
#endif
}
