#ifndef __QOS_STRUCT_DEF_H__
#define __QOS_STRUCT_DEF_H__

struct QOS_REG_T {
	const char	*reg_name;
	unsigned int	base_addr;
	unsigned int	mask_value;
	unsigned int	set_value;
};

#endif
