#ifndef	__PXA3XX_BBT_H__
#define	__PXA3XX_BBT_H__

#define PXA_RELOC_HEADER	0x524e
#define PXA_BEGIN_SLOT		2

/* New BBM scheme */
#define BOOT_PRAT_MAX		10
#define PXA_NEW_BBM_HEADER	0x4D424254
#define PXA_PART_IDET_1		0x4D52564C
#define PXA_PART_IDET_2		0x204D5054
#define BBM_FULL_MASK		0xffffffff
#define BBM_HALF_MASK		0x0000ffff
#define BBT_TYPE_FACT		0x46616374
#define BBT_TYPE_RUNT		0x52756E74
#define PART_PHYS		0x50687973
#define PART_LOGI		0x4C6F6769
#define RP_UPWD			0x55505744
#define RP_DNWD			0x444E5755

enum bbm_type {
	BBM_NONE = 0,
	BBM_LEGACY,
	BBM_NEW,
};

enum bbm_order {
	ORDER_REVERSE = 0,
	ORDER_POSITIVE,
};

enum bbt_state {
	BBT_NOINIT = 0,
	BBT_INITED,
	BBT_FORCE_NOINIT,
};

struct reloc_item {
	unsigned short from;
	unsigned short to;
};

struct reloc_table {
	unsigned short header;
	unsigned short total;
};

struct pxa3xx_bbt {
	uint32_t ident;
	uint32_t ver;
	uint32_t type;
	uint32_t reserved_1;
	uint32_t reserved_2;
	uint32_t entry_num;
	uint32_t bbt_loc;
	uint32_t reserved_3;
	uint32_t reserved_4;
	uint32_t reserved_5;
	union {
		struct reloc_item *reloc;
		uint32_t *fact_bad;
	};
};

struct pxa3xx_partinfo {
	uint32_t	type;		/* Logi or Phys */
	uint32_t	usage;		/* Partition name */
	uint32_t	identifier;	/* for distinguish same name */
	uint32_t	attrs;		/* r/w and permisson control */
	uint64_t	start_addr;	/* addr of LSB of start block */
	uint64_t	end_addr;	/* addr of MSB of end block */
	uint64_t	rp_start;
	uint64_t	rp_size;
	uint32_t	rp_algo;
	uint32_t	rbbt_type;
	uint64_t	rbbt_start;
	uint64_t	rbbt_start_back;
	uint64_t	reserved;
};

struct pxa3xx_part {
	uint64_t		identifier;
	uint32_t		version;
	uint32_t		part_num;
	uint64_t		reserved;
};

struct pxa3xx_legacy_bbm {
	int			current_slot;
	int			max_reloc_entry;

	struct reloc_table	*table;
	struct reloc_item	*reloc;
};

struct pxa3xx_new_bbm {
	int			main_block;
	int			back_block;
	int			update_indicator;

	struct pxa3xx_part	*part;
	struct pxa3xx_bbt	*fbbt;
	struct pxa3xx_bbt	*rbbt;
	struct pxa3xx_partinfo	*partinfo;
	loff_t			*rbbt_offset;
	int			*max_reloc_entry;
};

struct pxa3xx_bbm {
	int	bbm_type;
	int	is_init;
	int	no_sync;
	void	*data_buf;
	char	*rel_dist;

	void    (*uninit)(struct mtd_info *mtd);
	loff_t	(*search)(struct mtd_info *mtd,	loff_t ofs);
	loff_t	(*reserved_sz)(struct mtd_info *mtd);
};

int pxa3xx_scan_bbt(struct mtd_info *mtd);
int pxa3xx_block_bad(struct mtd_info *mtd, loff_t ofs, int allowbbt);
int pxa3xx_update_bbt(struct mtd_info *mtd, loff_t offs);
int pxa3xx_block_markbad(struct mtd_info *mtd, loff_t ofs);
int pxa3xx_bbm_recovery(struct mtd_info *mtd, int bbm_type,
		struct reloc_item *item, int num, int reserve_last_page);
int page_search(struct mtd_info *mtd, int start_page, int end_page,
	int direction, unsigned int indicator, void *buf, unsigned int mask);
#endif
