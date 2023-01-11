#ifndef _EEHANDLER_H_
#define _EEHANDLER_H_

//
// This files contains a collection of definitions from errorhandler/
// These definitions should never be changed, so merge maintenance cost is minimal
//

//
// inc/loadTable.h
//
#define LOAD_TABLE_OFFSET    0x01C0

typedef struct
{
    UINT32 imageBegin;       // image addresses are in HARBELL address space 0xD0??.????
    UINT32 imageEnd;         //                 for BOERNE use conversion to 0xBF??.????
    char   Signature[16];
    UINT32 sharedFreeBegin;
    UINT32 sharedFreeEnd;
    UINT32 ramRWbegin;
    UINT32 ramRWend;
    char   spare[24];        /* This area may be used for data transfer from loader to COMM */
}LoadTableType; /*total size 0x40=64bytes */

#define LOAD_TABLE_SIGNATURE_STR  "LOAD_TABLE_SIGN"  /*15 + zeroEnd */
#define INVALID_ADDRESS           0xBAD0DEAD

//
// inc/EE_Postmortem.h
//

#define PUB_ADDR_OF_PTR2POSTMORTEM_OFFS        /*ADDR_2*/(3*sizeof(void*))

#define POSTMORTEM_NUMOF_ENTRIES        (15+1) /* The last one is all zeros as end! */

#define POSTMORTEM_BUF_NAME_SIZE       (7+1) /* FIXED size including \0 */

typedef struct
{
    char      name[POSTMORTEM_BUF_NAME_SIZE];  //8 bytes    Name  for the buffer to be accessed or saved into file
    UINT32    bufAddr;       //4 bytes   pointer to the buffer to be accessed or saved into file
    UINT32    bufLen;        //4 bytes   length  of the buffer to be accessed or saved into file
} EE_PostmortemDesc_Entry;   //=16 bytes

//
// ap_cp_mmap.h
//
/*
 * PHY_OFFSET(cp_addr) is used to convert a CP DDR address to a physical one.
 * CP addresses may be in form of 0xdxxxxxxx or 0x4xxxxxxx).
 * The mask used to limit CP space to 16MB, which is too restrictive now.
 * Clearing [31:28] is enough to remove the CP specific address prefix.
 */
#define TAVOR_ADDR_SELECT_MASK            0xF0000000
#define PHY_OFFSET(addr) ((addr)&~TAVOR_ADDR_SELECT_MASK)
#define COMM_RO_ADDRESS 0xd0900000
#define COMM_LOW_ADDRESS 0xd0000000

#endif