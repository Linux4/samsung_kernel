#ifndef _SOCISP_BLOCK_H
#define _SOCISP_BLOCK_H

/*
 * This file declears all the data structures used in hardware layer of
 * SoC-ISP (SoC based ISP framework).
 */
/*
 * SoC-ISP hardware layer aims to provide helper functions to handle platform
 * interfacing code of a ISP driver. From SoC's perspective, a ISP IP Core is
 * just a hardware section, which requires power management unit to provide
 * power, clock managment unit to provide clock, and report it's working status
 * by interrupt. Although the ISP appears in the form of one IP Core, it can be
 * divide into several blocks, which performs a certain part of the image
 * processing. These blocks may have different clock/power source, and report
 * in different interrupt, so each block needs to manage its hardware resource
 * independently. That's what SoC-ISP hardware layer try to handle
 */

#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/string.h>
#include <linux/platform_device.h>

#define MAX_BLOCK_PER_SOC	20
#define MAX_CLOCK_PER_BLOCK	5

enum isp_resrc_type {
	ISP_RESRC_IO = 0,
	ISP_RESRC_MEM,
	ISP_RESRC_IRQ,
	ISP_RESRC_DMA,
	ISP_RESRC_BUS,
	ISP_RESRC_CLK,
	ISP_RESRC_END,
};

struct isp_block;

struct isp_block_ops {
	/* Called in H/W initialize */
	int	(*init)(struct isp_block *block);
	/* Called before H/W destroy */
	void	(*clean)(struct isp_block *block);
	/* Called upon H/W enable/disable, used to set clock rate, etc*/
	int	(*set_power)(struct isp_block *block, int level);
	int	(*set_clock)(struct isp_block *block, int rate);
	/* Called just after H/W enable */
	int	(*open)(struct isp_block *block);
	/* Called just before H/W disable */
	void	(*close)(struct isp_block *block);
};

/*
 * @dev_type:	the type of the H/W IP Core that contains this block
 * @dev_id:	the instance ID of the H/W IP Core that contains this block
 * @mod_type:	the type of this H/W block
 * @mod_id:	the instance ID of this H/W block
 */
struct block_id {
	u8	dev_type; /* What kind of device own this mod? */
	u8	dev_id; /* Which device instance owns it? */
	u8	mod_type; /* H/W mod type */
	u8	mod_id; /* H/W instance ID */
};
#define id_valid(id, mask)						\
	(((id).dev_type == ((id).dev_type & (mask).dev_type))		\
	&& ((id).dev_id == ((id).dev_id & (mask).dev_id))		\
	&& ((id).mod_type == ((id).mod_type & (mask).mod_type))		\
	&& ((id).mod_id == ((id).mod_id & (mask).mod_id)))

struct isp_res_req {
	enum isp_resrc_type	type;	/* Xlate from pdev::res::type*/
	int			id;
	/* Pointer to tell manager how to dispatch this resource to block */
	/* For ISP_RESRC_MEM, it's the block's offset from device base address*/
	void *priv;
};

/*
 * Data structure to describe each H/W block,
 * which can be a part of or the entire camera IP core
 * @block_id:	the identifier that is unique within the SoC
 * @name:	Null-terminated string to describe this block
 * @resrc_pool:	pointer to the resource pool, from which block gets resource
 * @req_list:	the resource list needed by this block, NULL-terminated
 * @reg_base:	the mapped IO register base address of this block
 * @irq_num:	interrupt number assigned to this block
 * @clock:	the clock list needed by this block
 * @pwrcnt:	the refcnt for this block been enabled
 * @mux_lock:	a lock to prevent multi-entry of enable/disable sequence
 * @plat_priv:	point at platform specific data
 * @ops:	driver specific sequence for H/W
 * @hdev:	the host device that acts as the v4l2_subdev deligate
 */
struct isp_block {
	struct block_id		id;
	struct device		*dev;
	char			*name;
	struct list_head	*resrc_pool;

	/* platform resource */
	struct isp_res_req	*req_list;
	void __iomem		*reg_base;
	u32			irq_num;
	struct clk		*clock[MAX_CLOCK_PER_BLOCK];
	int			clock_cnt;

	int		pwrcnt;
	struct mutex	mux_lock;
	void		*plat_priv;
	struct isp_block_ops	*ops;
};

/*
 * @dev:	the owner device of this resource
 * @type:	type code of the resource
 * @id:		resource instance ID of this type
 * @mask:	resource will be allocated to block with matched id
 * @hook:	hook to chain the resource into resource list
 * @priv:	private pointer to none-standard data types that will be
		passed to resource handler
 * @handle:	resource handle
 */
struct isp_resrc {
	const char		*name;
	struct device		*dev;
	int			type;
	int			id;
	struct block_id		mask;
	u32			base;
	u32			size;
	struct list_head	hook;
	void			*priv;
	union {
		void __iomem	*va;
		struct clk	*clk;
		irq_handler_t	*handle;
	};
	int			refcnt;
};

struct isp_resrc *isp_resrc_register(struct device *dev,
	struct resource *res, struct list_head *pool, const char *name,
	struct block_id mask, int res_id, void *handle, void *priv);
int isp_block_register(struct isp_block *block, struct list_head *pool);
void isp_block_unregister(struct isp_block *block);
int isp_block_init_default(struct isp_block *block);
int isp_block_tune_power(struct isp_block *block, unsigned int level);

void isp_block_pool_init(struct list_head *pool);
void isp_block_pool_clean(struct list_head *pool);

/* Helper functions for 8-bit IO */
static inline u8 isp_read8(struct isp_block *block, const u32 addr)
{
	return readb(block->reg_base + addr);
}

static inline void isp_write8(struct isp_block *block,
				const u32 addr, const u8 value)
{
	writeb(value, block->reg_base + addr);
}

static inline void isp_set8(struct isp_block *block,
				const u32 addr, const u8 mask)
{
	u8 val = readb(block->reg_base + addr);
	val |= mask;
	writeb(val, block->reg_base + addr);
}

static inline void isp_clr8(struct isp_block *block,
				const u32 addr, const u8 mask)
{
	u8 val = readb(block->reg_base + addr);
	val &= ~mask;
	writeb(val, block->reg_base + addr);
}

/* Helper functions for 16-bit IO */
static inline u16 isp_read16(struct isp_block *block, const u32 addr)
{
	return readw(block->reg_base + addr);
}

static inline void isp_write16(struct isp_block *block,
				const u32 addr, const u16 value)
{
	writew(value, block->reg_base + addr);
}

static inline void isp_set16(struct isp_block *block,
				const u32 addr, const u16 mask)
{
	u16 val = readw(block->reg_base + addr);
	val |= mask;
	writew(val, block->reg_base + addr);
}

static inline void isp_clr16(struct isp_block *block,
				const u32 addr, const u16 mask)
{
	u16 val = readw(block->reg_base + addr);
	val &= ~mask;
	writew(val, block->reg_base + addr);
}

/* Helper functions for 32-bit IO */
static inline u32 isp_read32(struct isp_block *block, const u32 addr)
{
	return readl(block->reg_base + addr);
}

static inline void isp_write32(struct isp_block *block,
				const u32 addr, const u32 value)
{
	writel(value, block->reg_base + addr);
}

static inline void isp_set32(struct isp_block *block,
				const u32 addr, const u32 mask)
{
	u32 val = readl(block->reg_base + addr);
	val |= mask;
	writel(val, block->reg_base + addr);
}

static inline void isp_clr32(struct isp_block *block,
				const u32 addr, const u32 mask)
{
	u32 val = readl(block->reg_base + addr);
	val &= ~mask;
	writel(val, block->reg_base + addr);
}

#define d_log(fmt, arg...)						\
	pr_debug("%s:ln%d: " fmt "\n", KBUILD_BASENAME, __LINE__,	\
							## arg);	\

#define d_inf(level, fmt, arg...)					\
	do {								\
		if (trace >= level)					\
			pr_debug("%s: " fmt "\n", KBUILD_BASENAME,	\
							## arg);	\
	} while (0)
#endif
