static bool platform_mif_wlbt_property_read_bool(struct scsc_mif_abs *interface, const char *propname);
bool (*fp_platform_mif_wlbt_property_read_bool)(struct scsc_mif_abs *interface, const char *propname) = &platform_mif_wlbt_property_read_bool;

static int platform_mif_wlbt_property_read_u8(struct scsc_mif_abs *interface,
                                              const char *propname, u8 *out_value, size_t size);
int (*fp_platform_mif_wlbt_property_read_u8)(struct scsc_mif_abs *interface,
                                              const char *propname, u8 *out_value, size_t size) = &platform_mif_wlbt_property_read_u8;

static int platform_mif_wlbt_property_read_u16(struct scsc_mif_abs *interface,
                                              const char *propname, u16 *out_value, size_t size);
int (*fp_platform_mif_wlbt_property_read_u16)(struct scsc_mif_abs *interface,
                                              const char *propname, u16 *out_value, size_t size) = &platform_mif_wlbt_property_read_u16;

static int platform_mif_wlbt_property_read_u32(struct scsc_mif_abs *interface,
                                              const char *propname, u32 *out_value, size_t size);
int (*fp_platform_mif_wlbt_property_read_u32)(struct scsc_mif_abs *interface,
                                              const char *propname, u32 *out_value, size_t size) = &platform_mif_wlbt_property_read_u32;

static int platform_mif_wlbt_property_read_string(struct scsc_mif_abs *interface,
                                              const char *propname, char **out_value, size_t size);
int (*fp_platform_mif_wlbt_property_read_string)(struct scsc_mif_abs *interface,
                                              const char *propname, char **out_value, size_t size) = &platform_mif_wlbt_property_read_string;
#if defined(SCSC_SEP_VERSION)
static int platform_mif_wlbt_phandle_property_read_u32(struct scsc_mif_abs *interface,
		const char *phandle_name, const char *propname, u32 *out_value, size_t size);
int (*fp_platform_mif_wlbt_phandle_property_read_u32)(struct scsc_mif_abs *interface,
		const char *phandle_name, const char *propname, u32 *out_value, size_t size)
	= &platform_mif_wlbt_phandle_property_read_u32;
#endif
