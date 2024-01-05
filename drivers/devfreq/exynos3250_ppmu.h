/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS3250 - PPMU support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __DEVFREQ_EXYNOS3250_PPMU_H
#define __DEVFREQ_EXYNOS3250_PPMU_H __FILE__


#define PPMU_PMNC		0x0
#define PPMU_CNTENS		0x10
#define PPMU_FLAG		0x50
#define PPMU_BEVT0SEL		0x1000

#define PPMU_CCNT		0x100
#define PPMU_PMCNT0		0x110
#define PPMU_PMCNT_OFFSET	0x10
#define PMCNT_OFFSET(x)		(PPMU_PMCNT0 + (PPMU_PMCNT_OFFSET * x))

/* For PPMU control */
#define PPMU_ENABLE		BIT(0)
#define PPMU_DISABLE		0x0

#define PPMU_ENABLE_COUNT0	BIT(0)
#define PPMU_ENABLE_COUNT1	BIT(1)
#define PPMU_ENABLE_COUNT2	BIT(2)
#define PPMU_ENABLE_COUNT3	BIT(3)
#define PPMU_ENABLE_CYCLE	BIT(31)

#define PPMU_CC_RESET		BIT(2)
#define PPMU_COUNT_RESET	BIT(1)

#define PPMU_CCNT_OVERFLOW	BIT(31)

#define PPMU_BEVTSEL_OFFSET	0x100
#define PPMU_BEVTSEL(x)		(PPMU_BEVT0SEL + (x * PPMU_BEVTSEL_OFFSET))

/* For Event Selection */
#define RD_DATA_COUNT		0x5
#define WR_DATA_COUNT		0x6
#define RDWR_DATA_COUNT		0x7

enum exynos3250_ppmu_sets {
	PPMU_SET_DDR,
	PPMU_SET_INT,
	PPMU_SET_CPU,
};
enum exynos3250_ppmu_counter {
	PPMU_PMNCNT0 = 0,
	PPMU_PMCCNT1,
	PPMU_PMNCNT2,
	PPMU_PMNCNT3,
	PPMU_PMNCNT_MAX,
};

enum exynos3250_ppmu_list {
	PPMU_DMC_0,
	PPMU_DMC_1,
	PPMU_RIGHT,
	PPMU_LEFT,
	PPMU_CPU,
	PPMU_END,
};

struct exynos3250_ppmu {
	void __iomem *hw_base;
	unsigned int ccnt;
	unsigned int event[PPMU_PMNCNT_MAX];
	unsigned long count[PPMU_PMNCNT_MAX];
	unsigned long long ns;
	bool ccnt_overflow;
	bool count_overflow[PPMU_PMNCNT_MAX];
};

struct exynos3250_ppmu_handle;

int exynos3250_ppmu_get_filter(enum exynos3250_ppmu_sets filter,
	enum exynos3250_ppmu_list *start, enum exynos3250_ppmu_list *end);
int exynos3250_ppmu_update(enum exynos3250_ppmu_sets filter);
struct exynos3250_ppmu_handle *exynos3250_ppmu_get(enum exynos3250_ppmu_sets filter);
void exynos3250_ppmu_put(struct exynos3250_ppmu_handle *handle);
int exynos3250_ppmu_get_busy(struct exynos3250_ppmu_handle *handle,
		enum exynos3250_ppmu_sets filter,
		unsigned int *ccnt, unsigned long *int_pmcnt);

#endif /* __DEVFREQ_EXYNOS3250_PPMU_H */
