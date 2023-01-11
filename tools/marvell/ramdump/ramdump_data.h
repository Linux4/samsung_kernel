//
// ramdump_data.h
// struct ramdump_state definition comes from arch/arm/mach-pxa/ramdump.c
// struct pt_regs definition comes from arch/arm/include/asm/ptrace.h
//

#ifndef _RAMDUMP_DATA_H_
#define _RAMDUMP_DATA_H_

/*
 * -----------------------------------------------------------------------
 *			aarch32 definitions
 * ------------------------------------------------------------------------
 */
struct pt_regs {
        long uregs[18];
};

#define ARM_cpsr        uregs[16]
#define ARM_pc          uregs[15]
#define ARM_lr          uregs[14]
#define ARM_sp          uregs[13]
#define ARM_ip          uregs[12]
#define ARM_fp          uregs[11]
#define ARM_r10         uregs[10]
#define ARM_r9          uregs[9]
#define ARM_r8          uregs[8]
#define ARM_r7          uregs[7]
#define ARM_r6          uregs[6]
#define ARM_r5          uregs[5]
#define ARM_r4          uregs[4]
#define ARM_r3          uregs[3]
#define ARM_r2          uregs[2]
#define ARM_r1          uregs[1]
#define ARM_r0          uregs[0]
#define ARM_ORIG_r0     uregs[17]



/* CPU mode registers */
struct mode_regs {
	unsigned spsr;
	unsigned sp;
	unsigned lr;
};
struct usr_regs {
	unsigned sp;
	unsigned lr;
};
struct fiq_regs {
	unsigned spsr;
	unsigned r8;
	unsigned r9;
	unsigned r10;
	unsigned r11;
	unsigned r12;
	unsigned sp;
	unsigned lr;
};

/* CP15 registers */
struct cp15_regs {
	unsigned id;		/* CPU ID */
	unsigned cr;		/* Control */
	unsigned aux_cr;	/* Auxiliary Control */
	unsigned ttb;		/* TTB; PJ4: ttb0 */
	unsigned da_ctrl;	/* Domain Access Control */
	unsigned cpar;		/* Co-processor access control */
	unsigned fsr;		/* PJ4: DFSR */
	unsigned far_;		/* PJ4: DFAR */  /* was "far": MSVC claims it's a reserved obsolete keyword */
	unsigned procid;	/* Process ID; PJ4: Context ID */
};

/* CP14 registers */
struct cp14_regs {
	unsigned ccnt;
	unsigned pmnc;
};

/* CP7 registers (L2C/BIU errors)*/
struct cp7_regs {
	unsigned errlog;
	unsigned erradrl;
	unsigned erradru;
};

/* CP6 registers (Interrupt Controller) */
struct cp6_regs {
	unsigned icip;
	unsigned icmr;
	unsigned iclr;
	unsigned icfp;
	unsigned icpr;
	unsigned ichp;
	unsigned icip2;
	unsigned icmr2;
	unsigned iclr2;
	unsigned icfp2;
	unsigned icpr2;
	unsigned icip3;
	unsigned icmr3;
	unsigned iclr3;
	unsigned icfp3;
	unsigned icpr3;
};

/* PJ4 specific cp15 regs. DONT EXTEND, ADD NEW STRUCTURE TO ramdump_state */
struct cp15_regs_pj4 {
	unsigned seccfg;	/* Secure Configuration */
	unsigned secdbg;	/* Secure Debug Enable */
	unsigned nsecac;	/* Non-secure Access Control */
	unsigned ttb1;		/* TTB1; TTB0 is cp15_regs.ttb */
	unsigned ttbc;		/* TTB Control */
	unsigned ifsr;		/* Instruction FSR; Data FSR is cp15_regs.fsr */
	unsigned ifar;		/* Instruction FAR; Data FAR is cp15_regs.far */
	unsigned auxdfsr;	/* Auxiliary DFSR */
	unsigned auxifsr;	/* Auxiliary IFSR */
	unsigned pa;		/* PA: physical address after translation */
	unsigned prremap;	/* Primary Region Remap */
	unsigned nmremap;	/* Normal Memory Remap */
	unsigned istat;		/* Interrupt Status */
	unsigned fcsepid;	/* FCSE PID */
	unsigned urwpid;	/* User Read/Write Thread and Process ID */
	unsigned uropid;	/* User Read/Only Thread and Process ID */
	unsigned privpid;	/* Priviledged Only Thread and Process ID */
	unsigned auxdmc0;	/* Auxiliary Debug Modes Control 0 */
	unsigned auxdmc1;	/* Auxiliary Debug Modes Control 1 */
	unsigned auxfmc;	/* Auxiliary Functional Modes Control */
	unsigned idext;		/* CPU ID code extension */
};

struct l2c_pj4_regs {
	unsigned l2errcnt;	/* L2 Cache Error Counter */
	unsigned l2errth;	/* L2 Cache Error Threshold */
	unsigned l2errcapt;	/* L2 Cache Error Capture */
};

/* PJ4 performance monitor */
struct pfm_pj4_regs {
	unsigned ctrl;
	unsigned ceset;
	unsigned ceclr;
	unsigned ovf;
	unsigned softinc;
	unsigned csel;
	unsigned ccnt;
	unsigned evsel;
	unsigned pmcnt;
	unsigned uen;
	unsigned ieset;
	unsigned ieclr;
};

/* AP Clock Control registers */
struct acc_regs {
	unsigned accr;
	unsigned acsr;
	unsigned aicsr;
	unsigned d0cken_a;
	unsigned d0cken_b;
	unsigned accr1;
	unsigned d0cken_c;
	unsigned cfgreg0;	/* MG1 Arbitration Control */
};

/* Main RAMDUMP data structure */
struct ramdump_state {
	unsigned reserved; /* was rdc_va; RDC header virtual addres */
	unsigned rdc_pa;	/* RDC header physical addres */
	char text[100];
	unsigned err;
	struct pt_regs regs;	/* saved context */
	struct thread_info *thread;
	struct mode_regs svc;
	struct usr_regs usr;
	struct mode_regs abt;
	struct mode_regs und;
	struct mode_regs irq;
	struct fiq_regs fiq;
	struct cp15_regs cp15;
	/* Up to this point same structure for XSC and PJ4 */
	union {
		struct {			/* 21 total */
			struct cp14_regs cp14;	/* 2 */
			struct cp6_regs cp6;	/* 16 */
			struct cp7_regs cp7;	/* 3 */
		}; /* XSC */
		struct {			/* 21 total */
			struct cp15_regs_pj4 cp15pj4;
		}; /* PJ4 */
	};
	struct acc_regs acc;
	struct l2c_pj4_regs l2cpj4;
	struct pfm_pj4_regs pfmpj4;
};


/*
 * -----------------------------------------------------------------------
 *			aarch64 definitions
 * -----------------------------------------------------------------------
 */

/* See arch/arm64/include/uapi/asm/ptrace.h */
struct user_pt_regs_64 {
	__u64		regs[31];
	__u64		sp;
	__u64		pc;
	__u64		pstate;
};

/* See arch/arm64/include/asm/ptrace.h */
struct pt_regs_64 {
	union {
		struct user_pt_regs_64 user_regs;
		struct {
			u64 regs[31];
			u64 sp;
			u64 pc;
			u64 pstate;
		};
	};
	u64 orig_x0;
	u64 syscallno;
};

/* EL1 bank SPRSs */
struct spr_regs_64 {
	u32	midr;
	u32	revidr;
	u32 current_el;
	u32	sctlr;
	u32	actlr;
	u32	cpacr;
	u32	isr;
	u64	tcr;
	u64	ttbr0;
	u64	ttbr1;
	u64	mair;
	u64	tpidr;
	u64	vbar;
	u32	esr;
	u32	reserved1;
	u64	far_;
};
struct soc_regs_64 {
	unsigned	reserved1;
};

struct ramdump_state_64 {
	unsigned reserved; /* was rdc_va; RDC header virtual addres */
	unsigned rdc_pa;	/* RDC header physical addres */
	char text[100];
	unsigned err;
	struct pt_regs_64 regs;	/* saved context */
	u64 /*struct thread_info * */ thread;

	unsigned				spr_size;
	struct spr_regs_64		spr;
	unsigned				soc_size;
	struct soc_regs_64 soc;
};
#endif /* _RAMDUMP_DATA_H_ */
