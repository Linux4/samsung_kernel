#ifndef _DRIVERS_THERMAL_PLATFORM_H
#define _DRIVERS_THERMAL_PLATFORM_H

struct coefficients {
	int frequency;
	int power;
};

#ifdef CONFIG_SOC_EXYNOS5422_REV_0
#define NR_A7_COEFFS 8
#define NR_A15_COEFFS 13
#define NR_GPU_COEFFS 7

struct coefficients a7_cpu_coeffs[NR_A7_COEFFS] = {
	{
		.power		= 82,
		.frequency	= 800,
	},
	{
		.power		= 99,
		.frequency	= 900,
	},
	{
		.power		= 118,
		.frequency	= 1000,
	},
	{
		.power		= 143,
		.frequency	= 1100,
	},
	{
		.power		= 169,
		.frequency	= 1200,
	},
	{
		.power		= 204,
		.frequency	= 1300,
	},
	{
		.power		= 250,
		.frequency	= 1400,
	},
	{
		.power		= 289,
		.frequency	= 1500,
	},
};

struct coefficients a15_cpu_coeffs[NR_A15_COEFFS] = {
	{
		.power		= 356,
		.frequency	= 800,
	},
	{
		.power		= 423,
		.frequency	= 900,
	},
	{
		.power		= 495,
		.frequency	= 1000,
	},
	{
		.power		= 572,
		.frequency	= 1100,
	},
	{
		.power		= 656,
		.frequency	= 1200,
	},
	{
		.power		= 746,
		.frequency	= 1300,
	},
	{
		.power		= 842,
		.frequency	= 1400,
	},
	{
		.power		= 944,
		.frequency	= 1500,
	},
	{
		.power		= 1077,
		.frequency	= 1600,
	},
	{
		.power		= 1221,
		.frequency	= 1700,
	},
	{
		.power		= 1377,
		.frequency	= 1800,
	},
	{
		.power		= 1638,
		.frequency	= 1900,
	},
	{
		.power		= 1888,
		.frequency	= 2000,
	},
};

struct coefficients gpu_coeffs[NR_GPU_COEFFS] = {
	{
		.power		= 365,
		.frequency	= 100,
	},
	{
		.power		= 645,
		.frequency	= 177,
	},
	{
		.power		= 1062,
		.frequency	= 266,
	},
	{
		.power		= 1569,
		.frequency	= 350,
	},
	{
		.power		= 2101,
		.frequency	= 420,
	},
	{
		.power		= 2597,
		.frequency	= 480,
	},
	{
		.power		= 3110,
		.frequency	= 533,
	},
};
#elif defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS5433)
#define NR_A7_COEFFS 11
#define NR_A15_COEFFS 13
#define NR_GPU_COEFFS 7

struct coefficients a7_cpu_coeffs[NR_A7_COEFFS] = {
	{
		.frequency	= 500,
	},
	{
		.frequency	= 600,
	},
	{
		.frequency	= 700,
	},
	{
		.frequency	= 800,
	},
	{
		.frequency	= 900,
	},
	{
		.frequency	= 1000,
	},
	{
		.frequency	= 1100,
	},
	{
		.frequency	= 1200,
	},
	{
		.frequency	= 1300,
	},
	{
		.frequency	= 1400,
	},
	{
		.frequency	= 1500,
	},
};

struct coefficients a15_cpu_coeffs[NR_A15_COEFFS] = {
	{
		.frequency	= 800,
	},
	{
		.frequency	= 900,
	},
	{
		.frequency	= 1000,
	},
	{
		.frequency	= 1100,
	},
	{
		.frequency	= 1200,
	},
	{
		.frequency	= 1300,
	},
	{
		.frequency	= 1400,
	},
	{
		.frequency	= 1500,
	},
	{
		.frequency	= 1600,
	},
	{
		.frequency	= 1700,
	},
	{
		.frequency	= 1800,
	},
	{
		.frequency	= 1900,
	},
	{
		.frequency	= 2000,
	},
};
#else
#error "[IPA]there is no platform table"
#endif
#endif /* _DRIVERS_THERMAL_PLATFORM_H */
