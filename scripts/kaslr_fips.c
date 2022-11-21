#include <stdio.h>
#include <stdlib.h>	
#include <elf.h>

typedef unsigned long long uint64;

//SHIFT must be 16KB / 2MB align according to KALSR config 

#define SHIFT(idx)  ((uint64)(idx) * 0x4000) 

#ifndef R_AARCH64_RELATIVE
#define R_AARCH64_RELATIVE     1027
#endif
#ifndef R_AARCH64_ABS64
#define R_AARCH64_ABS64         257
#endif

/*
 * the va_to_file offset is the file offset of the .rodata section in the 
 * vmlinux. rela_va_to_file offset is the file offset of the .rela section
 * in the vmlinux file. These two offsets may or may not be the same.
 */
uint64 va_to_file;
uint64 rela_va_to_file; 
int patch_rela(char *, uint64, uint64, uint64, uint64, uint64, uint64, uint64, uint64);
int main(int argc, char *argv[]){
	uint64 rs_offset = 0, re_offset = 0, ds_offset = 0;
	int index; 	
	char *file = NULL; 
	int ret; 
	if (argc != 12)
	{
		printf ("\nUsage : \n");
		printf ("kaslr_fips  vmlinux_file reloc_start_addr reloc_end_addr dynsym_addr index first_crypto_rodata last_crypto_rodata first_fmp_rodata last_fmp_rodata va_to_file rela_va_to_file");
		printf ("\n");
		return -1;
	}
	
	file = argv[1];
	rs_offset = atol(argv[2]);
	re_offset = atol(argv[3]);
	ds_offset = atol(argv[4]); 
	index = atoi(argv[5]);
	va_to_file = atol(argv[10]);
	rela_va_to_file = atol(argv[11]); 

	if( !file || !rs_offset || !re_offset )
	{
		printf ("kaslr_fips  vmlinux_file reloc_start_addr reloc_end_addr\n");
		printf ("kaslr_fips  %s, %llx, %llx \n",file,rs_offset, re_offset);
		printf ("kaslr_fips index %d\n", index); 
		return -1;
	}
	
	printf("----- start patching %s with reloc_s %llx reloc_e %llx dynsym_s %llx index %d va_to_file %lx rela_va_to_file %lx----\n", file, rs_offset, re_offset, ds_offset, index, (unsigned long)va_to_file, (unsigned long)rela_va_to_file); 		
	ret = patch_rela(file, rs_offset, re_offset, ds_offset, SHIFT(index), atol(argv[6]), atol(argv[7]), atol(argv[8]), atol(argv[9]));
	if(ret) return ret; 
	printf("----- end patching %s -----\n", file);
	return 0;  	
}	

/*
 * rela_start: relocation section start  in the vmlinux
 * rela_end
 * dynsym_start
 */

int patch_rela(char *file, uint64 rela_start, uint64 rela_end, uint64 dynsym_start, uint64 offset, uint64 first_crypto_rodata, uint64 last_crypto_rodata, uint64 first_fmp_rodata, uint64 last_fmp_rodata){
	uint64 rela_va_offset = 0;
	rela_va_offset = rela_va_to_file;
	uint64 rs_offset = rela_start - rela_va_offset;
	uint64 re_offset = rela_end - rela_va_offset;
	uint64 ds_offset = dynsym_start - rela_va_offset;

	FILE *fp = NULL;
	
	fp = fopen(file, "r+");
	if (NULL == fp){
		printf ("Unable to open file : %s", file);
		return -1;
	}

	Elf64_Rela rela_entry;
	Elf64_Sym  sym_entry;
	uint64 addr = 0, value = 0;
	size_t read_size = 0;
	/* Hint in case of FIPS failures: 
	 * rs_offset should exactly match the offset of .rela section in the vmlinux file
	 */
	printf("rs_offset %llx re_offset %llx ds_offset %llx\n", rs_offset, re_offset, ds_offset);
	for (; rs_offset < re_offset; rs_offset += sizeof(Elf64_Rela)){
		//seek and read the rela entry
		if(0 != fseek(fp, rs_offset, SEEK_SET)){
			fclose(fp);
			return -1;
		}

		read_size = fread((void*) &rela_entry, sizeof(rela_entry), 1, fp);
		if(0 == read_size) continue;
		//printf("%llx, %llx\n", ELF64_R_TYPE(rela_entry.r_info), R_AARCH64_RELATIVE);
		addr = rela_entry.r_offset;
		if (0x0 == addr) continue;
		//printf("addr %llx\n", addr);
		if ( !((addr >= first_crypto_rodata && addr <= last_crypto_rodata) || 
		   (addr >= first_fmp_rodata && addr <= last_fmp_rodata)))
		  continue;

		if (ELF64_R_TYPE(rela_entry.r_info) ==  R_AARCH64_RELATIVE) {
			value = offset + rela_entry.r_addend;
			//printf("--------->writing value %llx offset %llx rela_entry.r_addend %llx\n", value, offset, rela_entry.r_addend);

		} 
#if 0
		/* This is un-necessary  */
		else if(ELF64_R_TYPE(rela_entry.r_info) ==  R_AARCH64_ABS64)  {
			uint64 sym_index = ELF64_R_SYM(rela_entry.r_info);
			uint64 sym_offset = ds_offset + sym_index * (sizeof(Elf64_Sym));

			//seek to the start of the symbol table entry
			if (0 !=fseek(fp, sym_offset, SEEK_SET)){
				fclose(fp);
				return -1;
			}
			read_size = fread((void*) &sym_entry, sizeof(sym_entry), 1, fp);
			if(0 == read_size) continue;
			value = sym_entry.st_value + rela_entry.r_addend + offset;
		} 
#endif
		else {
		  	printf("Error! Try to patch none supported type %llx\n", (uint64)ELF64_R_TYPE(rela_entry.r_info));
			continue;
		}
		//printf("---rela_entry.r_offset %llx, rela_entry.r_addend %llx, rela_entry.r_info %llx addr-va_to_file %llx, value %llx\n", (uint64) rela_entry.r_offset, (uint64)rela_entry.r_addend, (uint64)rela_entry.r_info, addr-va_to_file, value);
		if (0 != fseek(fp, addr - va_to_file, SEEK_SET)) {
			printf("could not seek\n");
			fclose(fp);
			return -1;
		}
		if (fwrite((const void *) &value, sizeof(uint64), 1, fp) != 1) {
			fclose(fp);
			printf("could not write\n");
			return -1;
		}
	}

	fclose(fp);
	return 0;
}

