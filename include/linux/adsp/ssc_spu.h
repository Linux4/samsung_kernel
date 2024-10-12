#ifndef SSC_SPU_HEADER
#define SSC_SPU_HEADER

#ifdef CONFIG_SUPPORT_SSC_SPU
#define VER_COMPARE_CNT		2
#define SPU_VER_LEN		50
#define SLPI_VER_INFO		"/vendor/firmware_mnt/verinfo/slpi_ver.txt"
#define SLPI_SPU_VER_INFO	"/spu/sensorhub/slpi_ver.txt"
enum {
	SSC_SPU,
	SSC_ORI,
	SSC_ORI_AF_SPU_FAIL,
	SSC_CNT_MAX = SSC_ORI_AF_SPU_FAIL
};
#endif

#endif  /* SSC_SPU_HEADER */
