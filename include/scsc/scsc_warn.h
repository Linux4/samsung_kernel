#ifndef _WLBT_WARN_H
#define _WLBT_WARN_H

#ifdef CONFIG_WLBT_WARN_ON
#define WLBT_WARN_ON(condition) ({				\
	int __ret_warn_on = !!(condition);          \
	unlikely(__ret_warn_on);                    \
})
#else
#define WLBT_WARN_ON(condition) WARN_ON(condition)
#endif

#ifdef CONFIG_WLBT_WARN_ON
#define WLBT_WARN(condition, format...) ({       \
	int __ret_warn_on = !!(condition);       \
	if (unlikely(__ret_warn_on)) {           \
		printk(format);                  \
		printk(KERN_WARNING "Kernel Warning in %s at line %d in %s API",   \
		       __FILE__, __LINE__, __func__);                              \
	}                                                                          \
	unlikely(__ret_warn_on);                                                   \
})
#else
#define WLBT_WARN(condition, format...) WARN(condition, format)
#endif

#endif // _WLBT_WARN_H
