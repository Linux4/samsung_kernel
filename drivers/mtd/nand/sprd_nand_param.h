
#ifndef __SPRD_NAND_PARAM_H__
#define __SPRD_NAND_PARAM_H__

#define NAND_MAX_ID_LEN 5

#define BW_08 0x00
#define BW_16 0x01

#define CYCLES_3 0x03
#define CYCLES_4 0x04
#define CYCLES_5 0x05

#define NUM_BLOCK(n) (n)

#define SZ_K_BLOCK(n) (n * 1024)
#define SZ_K_PAGE(n) (n * 1024)

#define SZ_B_BLOCK(n) (n)
#define SZ_B_PAGE(n) (n)

#define SZ_B_SPARE(n) (n)
#define SZ_B_OOB(n) (n)
#define SZ_B_SECTOR(n) (n)

#define ECC_BITS(n) (n)

#define SZ_ECC(n) (n)
#define POS_ECC(n) (n)
#define SZ_INFO(n) (n)
#define POS_INFO(n) (n)

#define CALC_ECC_SIZE(n) ((14 * (n) + 7) / 8)


struct sprd_nand_device {
	uint8_t idDevice;
	char* pName;
	char* pType;
};

struct sprd_nand_maker {
	uint8_t idMaker;
	char* pName;
	struct sprd_nand_device* pDevTab;
};

struct sprd_nand_oob {
	uint16_t nOOBSize; //16
	uint8_t nEccBits;
	uint16_t nEccPos;
	uint16_t nEccSize;  //4
	uint16_t nInfoPos;
	uint16_t nInfoSize;
};

struct sprd_nand_timing {
	uint8_t	ace_ns;	/* ALE, CLE end of delay timing, unit: ns */
	uint8_t	rwl_ns;	/* WE, RE, IO, pulse  timing, unit: ns */
	uint8_t	rwh_ns;	/* WE, RE, IO, high hold  timing, unit: ns */
};

struct sprd_nand_param {
	uint8_t id[NAND_MAX_ID_LEN];
	uint8_t idMaker;
	uint8_t idDevice;
	uint32_t nBlkSize; //256K
	uint32_t nBlkNum; //
	uint32_t nPageSize; //4K
	uint16_t nSecSize; //512
	uint16_t nSpareSize;
	uint8_t nBusWidth;
	uint8_t nCycles;
	struct sprd_nand_timing sTiming;
	struct sprd_nand_oob sOOB;
};

struct sprd_nand_device micron_device_table[] = {
	{0xBC,"MT29F4G16ABBEA","4Gb,x16,1.8V"},
	{0x00,NULL,NULL}
};

struct sprd_nand_device adata_device_table[] = {
	{0xBC,"APZZLA4GAZ-N22MAB","4Gb,x16,1.8V"},
	{0x00,NULL,NULL}
};

struct sprd_nand_device kington_device_table[] = {
	{0xBC,"KSLCCBL2GA2H2A","4Gb,x16,1.8V"},
	{0x00,NULL,NULL}
};

struct sprd_nand_device hynix_device_table[] = {
	{0xBC,"H9TA4GH2GDACPR","4Gb,x16,1.8V"},
	{0x00,NULL,NULL}
};

struct sprd_nand_maker maker_table[] = {
	{0x98,"Kington",kington_device_table},
	{0x2C,"Micron",micron_device_table},
	{0xC8,"Adata",adata_device_table},
	{0xAD,"Hynix",hynix_device_table},
	{0x00,NULL,NULL}
};

struct sprd_nand_param sprd_nand_param_table[] = {
	{//Micron
		{0x2C,0xBC,0x90,0x66,0x54},0x2C,0xBC, 
		SZ_K_BLOCK(256),NUM_BLOCK(2048), SZ_K_PAGE(4),SZ_B_SECTOR(512),SZ_B_SPARE(224),BW_16,CYCLES_5,
		{10,15,10},
		{SZ_B_OOB(28),ECC_BITS(8),POS_ECC(14),SZ_ECC(14),POS_INFO(14),SZ_INFO(0)}
	},
	{//Kington
		{0x98,0xBC,0x90,0x66,0x76},0x98,0xBC, 
		SZ_K_BLOCK(256),NUM_BLOCK(2048), SZ_K_PAGE(4),SZ_B_SECTOR(512),SZ_B_SPARE(224),BW_16,CYCLES_5,
		{10,25,15},
		{SZ_B_OOB(28),ECC_BITS(8),POS_ECC(14),SZ_ECC(14),POS_INFO(14),SZ_INFO(0)}
	},
	{//Hynix
		{0xAD,0xBC,0x90,0x55,0x56},0xAD,0xBC, 
		SZ_K_BLOCK(128),NUM_BLOCK(4096), SZ_K_PAGE(2),SZ_B_SECTOR(512),SZ_B_SPARE(64),BW_16,CYCLES_5,
		{10,25,15},
		{SZ_B_OOB(16),ECC_BITS(4),POS_ECC(9),SZ_ECC(7),POS_INFO(9),SZ_INFO(0)}
	},
	{//Adata
		{0xC8,0xBC,0x90,0x55,0x54},0xC8,0xBC, 
		SZ_K_BLOCK(128),NUM_BLOCK(4096), SZ_K_PAGE(2),SZ_B_SECTOR(512),SZ_B_SPARE(64),BW_16,CYCLES_5,
		{10,25,15},
		{SZ_B_OOB(16),ECC_BITS(4),POS_ECC(9),SZ_ECC(7),POS_INFO(9),SZ_INFO(0)}
	},
	{
		{0x00, 0x00, 0x00, 0x00, 0x00},0x00,0x00, 
		SZ_K_BLOCK(256),NUM_BLOCK(2048), SZ_K_PAGE(4),SZ_B_SECTOR(512),SZ_B_SPARE(128),0x01,0x05,
		{10,21,15},
		{SZ_B_OOB(16),ECC_BITS(8),POS_ECC(2),SZ_ECC(14),POS_INFO(0),SZ_INFO(0)}
	}
};

STATIC_FUNC struct sprd_nand_maker* __FindMaker(uint8_t idMaker) {
	struct sprd_nand_maker* pmaker = maker_table;

	while (pmaker->idMaker != 0) {
		if(pmaker->idMaker == idMaker) {
			return pmaker;
		}
		pmaker++;
	}
	return NULL;
}

STATIC_FUNC struct sprd_nand_device* __FindDevice(struct sprd_nand_maker* pmaker, uint8_t idDevice) {
	struct sprd_nand_device* pdevice = pmaker->pDevTab;

	while (pdevice->idDevice != 0) {
		if(pdevice->idDevice == idDevice) {
			return pdevice;
		}
		pdevice++;
	}
	return NULL;
}

STATIC_FUNC void __PrintNandInfo(struct sprd_nand_param* p) {
	struct sprd_nand_device* pdevice;
	struct sprd_nand_maker* pmaker;

	DPRINT("%s\n", __func__);

	pmaker = __FindMaker(p->idMaker);
	pdevice = __FindDevice(pmaker, p->idDevice);
	
	DPRINT("device is %s:%s\n", pmaker->pName, pdevice->pName);
	DPRINT("block size is %d\n", p->nBlkSize);
	DPRINT("page size is %d\n", p->nPageSize);
	DPRINT("spare size is %d\n", p->nSpareSize);
	DPRINT("eccbits is %d\n", p->sOOB.nEccBits);
}


STATIC_FUNC void __ParseNandParam(uint8_t* id) {
	struct sprd_nand_param* param = sprd_nand_param_table;

	while(param->idMaker != 0) {
		if ((param->id[0] == id[0]) && 
			(param->id[4] == id[1])) {
			
			__PrintNandInfo(param);
			return param;
		}
		param++;
	}
	
	return;
}

STATIC_FUNC int __SprdCheckNancParam(struct sprd_nand_param* param) {
	struct sprd_nand_device* pdevice;
	struct sprd_nand_maker* pmaker;
	struct sprd_nand_oob* poob = &param->sOOB;
	uint8_t sector_num = param->nPageSize / param->nSecSize;

	DPRINT("%s\n", __func__);

	if (param->idMaker != param->id[0]) {
		DPRINT("%s: id is not match!!!!!!!!!!!!!!!\n", __func__);
		SPRD_ASSERT(0);
	}
	if (param->idDevice != param->id[1]) {
		DPRINT("%s: id is not match!!!!!!!!!!!!!!!\n", __func__);
		SPRD_ASSERT(0);
	}
	if (poob->nOOBSize * sector_num > param->nSpareSize) {
		DPRINT("%s: OOB size is not match!!!!!!!!!!!!!!!\n", __func__);
		SPRD_ASSERT(0);
	}
	if (poob->nEccPos + poob->nEccSize > poob->nOOBSize) {
		DPRINT("%s: ECC size exceeds the oob!!!!!!!!!!!!!!!\n", __func__);
		SPRD_ASSERT(0);
	}
	if (CALC_ECC_SIZE(poob->nEccBits) != poob->nEccSize) {
		DPRINT("%s: ECC size is wrong!!!!!!!!!!!!!!!\n", __func__);
		SPRD_ASSERT(0);
	}
	if (poob->nInfoPos + poob->nInfoSize != poob->nEccPos) {
		DPRINT("%s: Info position is wrong!!!!!!!!!!!!!!!\n", __func__);
		SPRD_ASSERT(0);
	}	

	return 0;	
}

struct sprd_nand_param* SprdGetNandParam(uint8_t* id) {
	struct sprd_nand_param* param = sprd_nand_param_table;
	int i;

	DPRINT("%s\n", __func__);

	for (i = 0; i < 5; i++) {
		DPRINT("id[%d] is %x\n", i, id[i]);
	}

	//__ParseNandParam(id);

	while(param->idMaker != 0) {
		if ((param->id[0] == id[0]) && 
			(param->id[1] == id[1]) &&
			(param->id[2] == id[2]) &&
			(param->id[3] == id[3]) &&
			(param->id[4] == id[4])) {

			__PrintNandInfo(param);
			__SprdCheckNancParam(param);
			return param;
		}
		param++;
	}
	DPRINT("Nand params unconfigured, please check it. Halt on booting!!!\n");
	for(;;) /*relax*/;

	return NULL;
}
#endif
