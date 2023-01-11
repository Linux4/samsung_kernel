/*
 * Copyright 2013 Marvell Tech. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __DTS_PXA1928_PINFUNC_H
#define __DTS_PXA1928_PINFUNC_H

/* drive-strength: value, mask */
#define DS_SLOW0		pinctrl-single,drive-strength = <0x0000 0x1800>
#define DS_SLOW1		pinctrl-single,drive-strength = <0x0800 0x1800>
#define DS_MEDIUM		pinctrl-single,drive-strength = <0x1000 0x1800>
#define DS_FAST			pinctrl-single,drive-strength = <0x1800 0x1800>

/* input-schmitt: value, mask ; input-schmitt-enable: value, enable, disable, mask */
#define EDGE_NONE		pinctrl-single,input-schmitt = <0 0x30>;	\
				pinctrl-single,input-schmitt-enable = <0x40 0 0x40 0x40>
#define EDGE_RISE		pinctrl-single,input-schmitt = <0x10 0x30>;	\
				pinctrl-single,input-schmitt-enable = <0x0 0 0x40 0x40>
#define EDGE_FALL		pinctrl-single,input-schmitt = <0x20 0x30>;	\
				pinctrl-single,input-schmitt-enable = <0x0 0 0x40 0x40>
#define EDGE_BOTH		pinctrl-single,input-schmitt = <0x30 0x30>;	\
				pinctrl-single,input-schmitt-enable = <0x0 0 0x40 0x40>

/* bias-pullup/down: value, enable, disable, mask */
#define PULL_NONE		pinctrl-single,bias-pullup = <0x0000 0xc000 0 0xc000>;	\
				pinctrl-single,bias-pulldown = <0x0000 0xa000 0 0xa000>
#define PULL_UP			pinctrl-single,bias-pullup = <0xc000 0xc000 0 0xc000>;	\
				pinctrl-single,bias-pulldown = <0x8000 0xa000 0x8000 0xa000>
#define PULL_DOWN		pinctrl-single,bias-pullup = <0x8000 0xc000 0x8000 0xc000>; \
				pinctrl-single,bias-pulldown = <0xa000 0xa000 0 0xa000>
#define PULL_BOTH		pinctrl-single,bias-pullup = <0xc000 0xc000 0x8000 0xc000>; \
				pinctrl-single,bias-pulldown = <0xa000 0xa000 0x8000 0xa000>
#define PULL_FLOAT		pinctrl-single,bias-pullup = <0x8000 0x8000 0 0xc000>;	\
				pinctrl-single,bias-pulldown = <0x8000 0x8000 0 0xa000>
#define PULL_LPM		pinctrl-single,bias-pullup = <0 0 0 0x8000>;		\
				pinctrl-single,bias-pulldown = <0 0 0 0x8000>

/*
 * Output value  sleep_oe_n  sleep_data  pullup_en  pulldown_en  pull_sel  sleep_sel
 *                 (bit 7)    (bit 8)    (bit 14)    (bit 13)     (bit 15)  (bit 9/3)
 *
 * Active           X(0)       X(0)        X(0)        X(0)       X(0)		11
 * Drive 0          0          0           0           X(1)       X(1)		00
 * Drive 1          0          1           X(1)        0	  X(1)		00
 * Pull hi (1)      1          X(1)        1           0	  0		00
 * Pull lo (0)      1          X(0)        0           1	  0		00
 * Input (float)    1          X(0)        0           0	  0		00
 *
 * lpm-output: value, mask
 */
#define LPM_NONE		pinctrl-single,lpm-output = <0x0208 0x6388>
#define LPM_DRIVELOW		pinctrl-single,lpm-output = <0x0000 0x6388>
#define LPM_DRIVEHIGH		pinctrl-single,lpm-output = <0x0100 0x6388>
#define LPM_PULLLOW		pinctrl-single,lpm-output = <0x2080 0x6388>
#define LPM_PULLHIGH		pinctrl-single,lpm-output = <0x4080 0x6388>
#define LPM_FLOAT		pinctrl-single,lpm-output = <0x0080 0x6388>

#define MFP_DEFAULT		DS_MEDIUM;PULL_NONE;EDGE_NONE;LPM_NONE

#define MFP_PULL_UP		DS_MEDIUM;PULL_UP;EDGE_NONE;LPM_NONE
#define MFP_PULL_DOWN		DS_MEDIUM;PULL_DOWN;EDGE_NONE;LPM_NONE
#define MFP_PULL_FLOAT		DS_MEDIUM;PULL_FLOAT;EDGE_NONE;LPM_NONE

#define MFP_LPM_PULL_HIGH	DS_MEDIUM;PULL_LPM;EDGE_NONE;LPM_PULLHIGH
#define MFP_LPM_PULL_LOW	DS_MEDIUM;PULL_LPM;EDGE_NONE;LPM_PULLLOW
#define MFP_LPM_DRIVE_HIGH	DS_MEDIUM;PULL_NONE;EDGE_NONE;LPM_DRIVEHIGH
#define MFP_LPM_DRIVE_LOW	DS_MEDIUM;PULL_NONE;EDGE_NONE;LPM_DRIVELOW
#define MFP_LPM_FLOAT		DS_MEDIUM;PULL_NONE;EDGE_NONE;LPM_FLOAT
#define MFP_EDGE_BOTH_DETECT	DS_MEDIUM;PULL_NONE;EDGE_BOTH;LPM_NONE

/*
 * gpio-range: pin base = base_addr / 4, nr pins, alternate function
 */
#define GPIO_0_11			&range 68  12 0
#define GPIO_12_32			&range 82  21 0
#define GPIO_33_42			&range 16  10 1
#define GPIO_43_54			&range 103 12 0
#define GPIO_55_58			&range 4   4  1
#define GPIO_59_61			&range 8   3  5
#define GPIO_62_63			&range 11  2  4
#define GPIO_64				&range 13  1  5
#define GPIO_65_66			&range 14  2  1
#define GPIO_67_68			&range 80  2  1
#define GPIO_69_75			&range 58  7  1
#define GPIO_76				&range 57  1  1
#define GPIO_77_79			&range 65  3  1
#define GPIO_80				&range 41  1  2
#define GPIO_81				&range 40  1  2
#define GPIO_82				&range 39  1  2
#define GPIO_83				&range 38  1  2
#define GPIO_84				&range 37  1  2
#define GPIO_85				&range 36  1  2
#define GPIO_86				&range 35  1  2
#define GPIO_87				&range 34  1  2
#define GPIO_88_89			&range 55  2  2
#define GPIO_90				&range 48  1  2
#define GPIO_91				&range 33  1  2
#define GPIO_92				&range 32  1  2
#define GPIO_93				&range 31  1  2
#define GPIO_94				&range 30  1  2
#define GPIO_95				&range 29  1  2
#define GPIO_96				&range 28  1  2
#define GPIO_97				&range 27  1  2
#define GPIO_98				&range 26  1  2
#define GPIO_99				&range 43  1  2
#define GPIO_100			&range 46  1  2
#define GPIO_101			&range 42  1  2
#define GPIO_102			&range 51  1  2
#define GPIO_103			&range 44  1  2
#define GPIO_104			&range 49  1  2
#define GPIO_105			&range 45  1  2
#define GPIO_106			&range 47  1  2
#define GPIO_107			&range 50  1  2
#define GPIO_108_110			&range 52  3  2
#define GPIO_111_122			&range 115 12 0
#define GPIO_123			&range 138 1  1
#define GPIO_124_126			&range 141 3  1
#define GPIO_127			&range 139 1  1
#define GPIO_128_135			&range 127 8  0
#define GPIO_136_138			&range 1   3  1
#define GPIO_139			&range 140 1  0
#define GPIO_140			&range 137 1  1
#define GPIO_141_142			&range 135 2  1
#define GPIO_143_144			&range 144 2  1
/* for A0 discrete */
#define GPIO_145_167			&range 146 23 0
#define GPIO_168			&range 190 1  0
#define GPIO_169			&range 189 1  0
#define GPIO_170			&range 188 1  0
#define GPIO_171			&range 187 1  0
#define GPIO_172			&range 186 1  0
#define GPIO_173			&range 185 1  0
#define GPIO_174			&range 184 1  0
#define GPIO_175			&range 183 1  0
#define GPIO_176			&range 182 1  0
#define GPIO_177			&range 181 1  0
#define GPIO_178			&range 180 1  0
#define GPIO_179			&range 179 1  0
#define GPIO_180			&range 178 1  0
#define GPIO_181			&range 177 1  0
#define GPIO_182			&range 176 1  0
#define GPIO_183			&range 175 1  0
#define GPIO_184			&range 174 1  0
#define GPIO_185			&range 173 1  0
#define GPIO_186			&range 172 1  0
#define GPIO_187			&range 171 1  0
#define GPIO_188			&range 170 1  0
#define GPIO_189			&range 169 1  0
#define GPIO_190			&range 193 1  0
#define GPIO_191_192			&range 191 2  0
#define GPIO_193_194			&range 194 2  0
#define GPIO_195			&range 198 1  0
#define GPIO_196_197			&range 196 2  0

/* gpio: gpio offset, pin offset, nr pins */
#define PXA1928_GPIO_0_31		&pmx 0 68 12	\
				&pmx 12 82 20
#define PXA1928_GPIO_32_63		&pmx 0 102 1	\
				&pmx 1 16 10	\
				&pmx 11 103 12	\
				&pmx 23 4 4	\
				&pmx 27 8 3	\
				&pmx 30 11 2
#define PXA1928_GPIO_64_95		&pmx 0 13 1	\
				&pmx 1 14 2	\
				&pmx 3 80 2	\
				&pmx 5 58 7	\
				&pmx 12 57 1	\
				&pmx 13 65 3	\
				&pmx 16 41 1	\
				&pmx 17 40 1	\
				&pmx 18 39 1	\
				&pmx 19 38 1	\
				&pmx 20 37 1	\
				&pmx 21 36 1	\
				&pmx 22 35 1	\
				&pmx 23 34 1	\
				&pmx 24 55 2	\
				&pmx 26 48 1	\
				&pmx 27 33 1	\
				&pmx 28 32 1	\
				&pmx 29 31 1	\
				&pmx 30 30 1	\
				&pmx 31 29 1
#define PXA1928_GPIO_96_127	&pmx 0 28 1	\
				&pmx 1 27 1	\
				&pmx 2 26 1	\
				&pmx 3 43 1	\
				&pmx 4 46 1	\
				&pmx 5 42 1	\
				&pmx 6 51 1	\
				&pmx 7 44 1	\
				&pmx 8 49 1	\
				&pmx 9 45 1	\
				&pmx 10 47 1	\
				&pmx 11 50 1	\
				&pmx 12 52 3	\
				&pmx 15 115 12	\
				&pmx 27 138 1	\
				&pmx 28 141 3	\
				&pmx 31 139 1
#define PXA1928_GPIO_128_159	&pmx 0 127 8	\
				&pmx 8 1 3	\
				&pmx 11 140 1	\
				&pmx 12 137 1	\
				&pmx 13 135 2	\
				&pmx 15 144 17
#define PXA1928_GPIO_160_191	&pmx 0 161 8	\
				&pmx 8 190 1	\
				&pmx 9 189 1	\
				&pmx 10 188 1	\
				&pmx 11 187 1	\
				&pmx 12 186 1	\
				&pmx 13 185 1	\
				&pmx 14 184 1	\
				&pmx 15 183 1	\
				&pmx 16 182 1	\
				&pmx 17 181 1	\
				&pmx 18 180 1	\
				&pmx 19 179 1	\
				&pmx 20 178 1	\
				&pmx 21 177 1	\
				&pmx 22 176 1	\
				&pmx 23 175 1	\
				&pmx 24 174 1	\
				&pmx 25 173 1	\
				&pmx 26 172 1	\
				&pmx 27 171 1	\
				&pmx 28 170 1	\
				&pmx 29 169 1	\
				&pmx 30 193 1	\
				&pmx 31 191 1
#define PXA1928_GPIO_192_197	&pmx 0 192 1	\
				&pmx 1 194 2	\
				&pmx 3 198 1	\
				&pmx 4 196 2

/* pmx: pin base, nr pins & gpio function */
#define PXA1928_GPIO_PINMUX	GPIO_0_11 GPIO_12_32 GPIO_33_42 GPIO_43_54	\
				GPIO_55_58 GPIO_59_61 GPIO_62_63 GPIO_64	\
				GPIO_65_66 GPIO_67_68 GPIO_69_75 GPIO_76	\
				GPIO_77_79 GPIO_80 GPIO_81 GPIO_82 GPIO_83	\
				GPIO_84 GPIO_85 GPIO_86 GPIO_87 GPIO_88_89	\
				GPIO_90 GPIO_91 GPIO_92 GPIO_93 GPIO_94 GPIO_95	\
				GPIO_96 GPIO_97 GPIO_98 GPIO_99 GPIO_100	\
				GPIO_101 GPIO_102 GPIO_103 GPIO_104		\
				GPIO_105 GPIO_106 GPIO_107 GPIO_108_110		\
				GPIO_111_122 GPIO_123 GPIO_124_126 GPIO_127	\
				GPIO_128_135 GPIO_136_138 GPIO_139 GPIO_140	\
				GPIO_141_142 GPIO_143_144			\
				GPIO_145_167 GPIO_168 GPIO_169 GPIO_170		\
				GPIO_171 GPIO_172 GPIO_173 GPIO_174 GPIO_175	\
				GPIO_176 GPIO_177 GPIO_178 GPIO_179 GPIO_180	\
				GPIO_181 GPIO_182 GPIO_183 GPIO_184 GPIO_185	\
				GPIO_186 GPIO_187 GPIO_188 GPIO_189		\
				GPIO_190 GPIO_191_192 GPIO_193_194		\
				GPIO_195 GPIO_196_197

/* mfpr_offset */
#ifndef PXA1928_DISCRETE

#define P0			0x110
#define P1			0x114
#define P2			0x118
#define P3			0x11c
#define P4			0x120
#define P5			0x124
#define P6			0x128
#define P7			0x12c
#define P8			0x130
#define P9			0x134
#define P10			0x138
#define P11			0x13c
#define P12			0x148
#define P13			0x14c
#define P14			0x150
#define P15			0x154
#define P16			0x158
#define P17			0x15c
#define P18			0x160
#define P19			0x164
#define P20			0x168
#define P21			0x16c
#define P22			0x170
#define P23			0x174
#define P24			0x178
#define P25			0x17c
#define P26			0x180
#define P27			0x184
#define P28			0x188
#define P29			0x18c
#define P30			0x190
#define P31			0x194
#define P32			0x198
#define P43			0x19c
#define P44			0x1a0
#define P45			0x1a4
#define P46			0x1a8
#define P47			0x1ac
#define P48			0x1b0
#define P49			0x1b4
#define P50			0x1b8
#define P51			0x1bc
#define P52			0x1c0
#define P53			0x1c4
#define P54			0x1c8

#define P136			0x4
#define P137			0x8
#define P138			0xc

#define P140			0x224
#define P141			0x21c
#define P142			0x220
#define P143			0x240
#define P144			0x244

#else /* PXA1928_DISCRETE */

#define P145			0x248
#define P146			0x24c
#define P147			0x250
#define P148			0x254
#define P149			0x258
#define P150			0x25c
#define P151			0x260
#define P152			0x264
#define P153			0x268
#define P154			0x26c
#define P155			0x270
#define P156			0x274
#define P157			0x278
#define P158			0x27c
#define P159			0x280
#define P160			0x284
#define P161			0x288
#define P162			0x28c
#define P163			0x290
#define P164			0x294
#define P165			0x298
#define P166			0x29c
#define P167			0x2a0
#define P168			0x2f8
#define P169			0x2f4
#define P170			0x2f0
#define P171			0x2ec
#define P172			0x2e8
#define P173			0x2e4
#define P174			0x2e0
#define P175			0x2dc
#define P176			0x2d8
#define P177			0x2d4
#define P178			0x2d0
#define P179			0x2cc
#define P180			0x2c8
#define P181			0x2c4
#define P182			0x2c0
#define P183			0x2bc
#define P184			0x2b8
#define P185			0x2b4
#define P186			0x2b0
#define P187			0x2ac
#define P188			0x2a8
#define P189			0x2a4

#define P190			0x304
#define P191			0x2fc
#define P192			0x300

#define P193			0x308
#define P194			0x30c
#define P195			0x318
#define P196			0x310
#define P197			0x314

#endif /* PXA1928_DISCRETE */

#define P33			0x40
#define P34			0x44
#define P35			0x48
#define P36			0x4c
#define P37			0x50
#define P38			0x54
#define P39			0x58
#define P40			0x5c
#define P41			0x60
#define P42			0x64

#define P55			0x10
#define P56			0x14
#define P57			0x18
#define P58			0x1c
#define P59			0x20
#define P60			0x24
#define P61			0x28
#define P62			0x2c
#define P63			0x30
#define P64			0x34
#define P65			0x38
#define P66			0x3c
#define P67			0x140
#define P68			0x144
#define P69			0xe8
#define P70			0xec
#define P71			0xf0
#define P72			0xf4
#define P73			0xf8
#define P74			0xfc
#define P75			0x100
#define P76			0xe4
#define P77			0x104
#define P78			0x108
#define P79			0x10c
#define P80			0xa4
#define P81			0xa0
#define P82			0x9c
#define P83			0x98
#define P84			0x94
#define P85			0x90
#define P86			0x8c
#define P87			0x88
#define P88			0xdc
#define P89			0xe0
#define P90			0xc0
#define P91			0x84
#define P92			0x80
#define P93			0x7c
#define P94			0x78
#define P95			0x74
#define P96			0x70
#define P97			0x6c
#define P98			0x68
#define P99			0xac
#define P100			0xb8
#define P101			0xa8
#define P102			0xcc
#define P103			0xb0
#define P104			0xc4
#define P105			0xb4
#define P106			0xbc
#define P107			0xc8
#define P108			0xd0
#define P109			0xd4
#define P110			0xd8
#define P111			0x1cc
#define P112			0x1d0
#define P113			0x1d4
#define P114			0x1d8
#define P115			0x1dc
#define P116			0x1e0
#define P117			0x1e4
#define P118			0x1e8
#define P119			0x1ec
#define P120			0x1f0
#define P121			0x1f4
#define P122			0x1f8
#define P123			0x228
#define P124			0x234
#define P125			0x238
#define P126			0x23c
#define P127			0x22c
#define P128			0x1fc
#define P129			0x200
#define P130			0x204
#define P131			0x208
#define P132			0x20c
#define P133			0x210
#define P134			0x214
#define P135			0x218
#define P139			0x230

#define MFPR_OFFSET(n)		P##n

/* GPIO alternate function in MFPR */
#define GPIO_AF1(n)		((((n) >= 33 && (n) <= 42) || ((n) >= 55 && (n) <= 58)		\
				|| ((n) >= 65 && (n) <= 75) || ((n) >= 77 && (n) <= 79)		\
				|| ((n) >= 123 && (n) <= 127) || ((n) >= 136 && (n) <= 138)	\
				|| ((n) >= 140 && (n) <= 144)) ? 1 : 0)
#define GPIO_AF2(n)		(((n) >= 80 && (n) <= 110) ? 2 : 0)
#define GPIO_AF4(n)		(((n) == 62 || (n) == 63) ? 4 : 0)
#define GPIO_AF5(n)		((((n) == 64) || ((n) >= 59 && (n) <= 61)) ? 5 : 0)
#define MFPR_AF_GPIO(n)		(GPIO_AF1(n) ? 1 : ((GPIO_AF2(n) ? 2 : ((GPIO_AF4(n) ? 4 : ((GPIO_AF5(n) ? 5 : 0)))))))

#define GPIO(n)		MFPR_OFFSET(n) MFPR_AF_GPIO(n)

#endif /* __DTS_PXA1928_PINFUNC_H */
