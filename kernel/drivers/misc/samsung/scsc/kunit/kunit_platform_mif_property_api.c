#define of_property_read_bool(args...) 0

static bool platform_mif_wlbt_property_read_bool(struct scsc_mif_abs *interface, const char *propname);
bool kunit_platform_mif_wlbt_property_read_bool(struct scsc_mif_abs *interface, const char *propname)
{
    return platform_mif_wlbt_property_read_bool(interface, propname);
} 

static int platform_mif_wlbt_property_read_u8(struct scsc_mif_abs *interface,
                                              const char *propname, u8 *out_value, size_t size);
int kunit_platform_mif_wlbt_property_read_u8(struct scsc_mif_abs *interface,
                                              const char *propname, u8 *out_value, size_t size)
{
    return platform_mif_wlbt_property_read_u8(interface, propname, out_value, size);
}

static int platform_mif_wlbt_property_read_u16(struct scsc_mif_abs *interface,
                                              const char *propname, u16 *out_value, size_t size);
int kunit_platform_mif_wlbt_property_read_u16(struct scsc_mif_abs *interface,
                                              const char *propname, u16 *out_value, size_t size)
{
    return platform_mif_wlbt_property_read_u16(interface, propname, out_value, size);
}

static int platform_mif_wlbt_property_read_u32(struct scsc_mif_abs *interface,
                                              const char *propname, u32 *out_value, size_t size);
int kunit_platform_mif_wlbt_property_read_u32(struct scsc_mif_abs *interface,
                                              const char *propname, u32 *out_value, size_t size)
{
    return platform_mif_wlbt_property_read_u32(interface, propname, out_value, size);
}

static int platform_mif_wlbt_property_read_string(struct scsc_mif_abs *interface,
                                              const char *propname, char **out_value, size_t size);
int kunit_platform_mif_wlbt_property_read_string(struct scsc_mif_abs *interface,
                                              const char *propname, char **out_value, size_t size)
{
    return platform_mif_wlbt_property_read_string(interface, propname, out_value, size);
}
#if defined(SCSC_SEP_VERSION)
static int platform_mif_wlbt_phandle_property_read_u32(struct scsc_mif_abs *interface,
		const char *phandle_name, const char *propname, u32 *out_value, size_t size);
int kunit_platform_mif_wlbt_phandle_property_read_u32(struct scsc_mif_abs *interface,
		const char *phandle_name, const char *propname, u32 *out_value, size_t size)
{
    return platform_mif_wlbt_phandle_property_read_u32(interface,phandle_name, propname, out_value, size);
}
#endif
