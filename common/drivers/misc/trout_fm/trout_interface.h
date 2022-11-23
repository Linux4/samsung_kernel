#ifndef TROUT_INTERFACE_H__
#define TROUT_INTERFACE_H__

struct trout_interface {
	char *name;
	unsigned int (*init) (void);
	unsigned int (*read_reg) (u32 addr, u32 *val);
	unsigned int (*write_reg) (u32 addr, u32 val);
	unsigned int (*exit) (void);
};

extern int trout_shared_init(struct trout_interface **p);
extern int trout_onchip_init(struct trout_interface **p);

#endif
