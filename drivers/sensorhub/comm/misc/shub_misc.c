#include <linux/kernel.h>
#include "../../utility/shub_utility.h"

extern int register_misc_dev_batch(bool en);
extern int register_misc_dev_injection(bool en);
extern int register_misc_dev_scontext(bool en);

typedef int (*miscdev_init)(bool en);

miscdev_init misc_dev_init_funcs[] =
{
	register_misc_dev_batch,
	register_misc_dev_injection,
	register_misc_dev_scontext,
};

int initialize_shub_misc_dev()
{
	int i, ret, j;

	for(i = 0 ; i<ARRAY_LEN(misc_dev_init_funcs); i++) {
		ret = misc_dev_init_funcs[i](true);
		if(ret) {
			shub_errf("init[%d] failed. ret %d", i, ret);
			break;
		}
	}

	if(i < ARRAY_LEN(misc_dev_init_funcs)) {
		j = i;
		for(i = 0; i < j; i++)
			misc_dev_init_funcs[i](false);
	}

	return ret;
}

void remove_shub_misc_dev()
{
	int i;

	for(i = 0 ; i<ARRAY_LEN(misc_dev_init_funcs); i++) {
		misc_dev_init_funcs[i](false);
	}
}