/*------------------------------------------------------------
(C) Copyright [2006-2008] Marvell International Ltd.
All Rights Reserved
------------------------------------------------------------*/

#include "rdp.h"

#define PMLOG_MAX_EV_STRINGS 300
#define PMLOG_MAX_STRING_LEN 50
#define PMLOG_MAX_OPS 30
#define PMLOG_MAX_OBJSIZE 0x100

#define OP_NAME_LEN					16
#define DVFM_MAX_NAME				32

#define EVENT_DB_ROW_MAGIC          0x44424C4E
#define OP_TABLE_ROW_MAGIC          0x4F504C4E
#define DEV_TABLE_ROW_MAGIC         0x44564C4E
#define EVENT_DB_HEADER_MAGIC       0x45564442
#define OP_TABLE_HEADER_MAGIC       0x4F505442
#define DEV_TABLE_HEADER_MAGIC      0x44565442

static inline UINT32 htonl(UINT32 x)
{
    UINT32 __x = (x);
    return (UINT32)(
        (((UINT32)(__x) & (UINT32)0x000000ffUL) << 24) |
        (((UINT32)(__x) & (UINT32)0x0000ff00UL) <<  8) |
        (((UINT32)(__x) & (UINT32)0x00ff0000UL) >>  8) |
        (((UINT32)(__x) & (UINT32)0xff000000UL) >> 24));
}

static int buf_to_hex(UINT8 *buf, size_t buf_size, FILE *fout)
{
	int i = 0;
	static int k = 0;

	if (!buf || !fout || buf_size == 0)
		return 1;

	for (i = 0; i < (int)buf_size; i++) {
		if (!(k%16)) fprintf(fout, "\nPM%05x ", k);
		fprintf(fout, "%02x ", buf[i]);
		k++;
	}

	return 0;
}

static int pmlog_bin_to_hex(FILE *fbin, FILE *fhex)
{
	size_t result;
	int lSize, i = 0, ret = 1;
	UINT8 *buffer;

	if (!fbin || !fhex)
		return 1;

	// obtain file size:
	fseek (fbin , 0 , SEEK_END);
	lSize = ftell (fbin);
	rewind (fbin);

	// allocate memory to contain the whole file:
	buffer = (UINT8*) malloc (sizeof(UINT8)*lSize);
	if (buffer == NULL) {
		fprintf(rdplog, "pmlog: Memory error");
		return 1;
	}

	// copy the file into the buffer:
	result = fread (buffer, sizeof(UINT8), lSize, fbin);
	if ((int)result != lSize) {
		fprintf(rdplog, "pmlog: Reading error");
		goto bail;
	}

	buf_to_hex(buffer, lSize, fhex);
	ret = 0;
bail:
	if (buffer)
		free (buffer);

	return ret;
}

static void pmlog_free_table(char **table, unsigned rows, unsigned cols)
{
	unsigned i, j;
	char *str;
	for (i = 0; i < rows; i++)
		for (j = 0; j < cols; j++)
			if ((str = table[i*cols + j]) != NULL) {
				free(str);
				table[i*cols + j] = NULL;
			}
}

#define MAX_DATA_NUM 10
const char *pmlog_builtin_events[][MAX_DATA_NUM] = {
        {"D1 ENTRY", "PWRMODE", "CKENA", "CKENB", "CKENC",
                "MIPI", "CPUPWR"},
        {"D2 ENTRY", "PWRMODE", "CKENA", "CKENB", "CKENC",
                "OSCC", "MIPI", "CPUPWR"},
        {"CGM ENTRY", "PWRMODE", "CKENA", "CKENB", "CKENC",
                "MIPI", "CPUPWR"},
        {"C1 ENTRY", "CPUPWR"},
        {"D1 EXIT", "AD1D0SR"},
        {"D2 EXIT", "AD2D0SR"},
        {"CGM EXIT", "ACGD0SR"},
        {"C1 EXIT"},
        {"OP REQ", "OP"},
        {"OP EN", "OP", "DEV"},
        {"OP DIS", "OP", "DEV"},
        {"OP EN NAME", "OP", "DEV"},
        {"OP DIS NAME", "OP", "DEV"},
        {"OP EN NO CHANGE", "OP", "DEV"},
        {"OP DIS NO CHANGE", "OP", "DEV"},
        {"OP SET", "FREQ", "ACCR", "ACSR"},
        {"C2 ENTRY"},
        {"C2 EXIT", "ICHP"},
        {"INFO"},
        {"WAKEUP GPIO", "GWSR1", "GWSR2", "GWSR3",
                "GWSR4", "GWSR5", "GWSR6"},
        {"LPM ABORTED APPS", "MSEC"},
        {"LPM ABORTED COMM"}
};
const char *pmlog_default_events_file="pmlog_app_db.txt";
static int pmlog_extract_default_events(const char* inName, char **table, unsigned rows, unsigned cols)
{
	char *name = 0;
	FILE *ef = 0;
	int ret = 1;
	char line[100];
	char str[PMLOG_MAX_STRING_LEN];
	int level, nchar, i;
	unsigned row, col, index;

	/* Try to open the event file in the log directory */
	if (((name = changeNameExt(inName, pmlog_default_events_file)) == NULL)
		|| ((ef = fopen(name, "rt")) == NULL)) {
		if (name)
			free(name);
		if (((name = changeNameExt(rdpPath, pmlog_default_events_file)) == NULL)
			|| ((ef = fopen(name, "rt")) == NULL)) {
			fprintf(rdplog, "Failed to find pmlog default event file %s\n", pmlog_default_events_file);
			goto bail;
		}
	}

	fprintf(rdplog, "Using pmlog default event file %s\n", name);
	level = 0;
	nchar = -1;
	row = col = index = 0;
	while (fgets(line, sizeof(line), ef)) {
		int len = strlen(line);
		for (i = 0; i<len; i++)
			if (nchar >= 0) {
				switch (line[i]) {
				case '"':
					str[nchar]=0;
					if ((table[index]=(char *)malloc(strlen(str)+1)) == NULL)
						goto bail;
					strcpy(table[index], str);
					index++;
					col++;
					nchar = -1;
					break;
				default:
					str[nchar++]=line[i];
				}
			} else {
				switch (line[i]) {
				case '"':
					nchar = 0;
					break;
				case '{':
					level++;
					break;
				case '}':
					level--;
					row++;
					col=0;
					index=row*cols;
					break;
				}
			}
	}
	ret = 0;
bail:
	if (name)
		free(name);
	if (ef)
		fclose(ef);
	if (ret)
		pmlog_free_table(table, rows, cols);
	return ret;
}

int pmlog_use_builtin_events(char **table, unsigned rows, unsigned cols)
{
	unsigned brows = sizeof(pmlog_builtin_events)/sizeof(pmlog_builtin_events[0]);
	unsigned bcols = MAX_DATA_NUM;
	unsigned i, j;
	int ret = 1;

	if ((cols > bcols) || (rows > brows))
		return 1;
	fprintf(rdplog, "Using pmlog builtin default events\n");
	for (i=0;i<brows;i++)
		for(j=0;j<bcols;j++) {
			unsigned index = i*cols+j;
			const char *str = pmlog_builtin_events[i][j];
			if (str) {
				if ((table[index]=(char *)malloc(strlen(str)+1)) == NULL)
					goto bail;
				strcpy(table[index], str);
			}
		}
	ret = 0;
bail:
	if (ret)
		pmlog_free_table(table, rows, cols);

	return ret;
}

/* Check the string is not empty and contains only printable chars */
static int pmlog_check_str(const char *str)
{
	int len = strlen(str);
	int i;

	if (!len || (len > PMLOG_MAX_STRING_LEN))
		return 1;
	for (i=0;i<len;i++)
		if (!isprint(str[i])) return 1;
	return 0;
}

static int pmlog_extract_events(FILE *fin, char **table, unsigned addr, unsigned rows, unsigned cols)
{
	unsigned *ttable = 0;
	unsigned size = rows*cols;
	unsigned i, j;
	char str[PMLOG_MAX_STRING_LEN];
	size_t str_len = 0;
	int ret = 1;

	ttable = (unsigned *)malloc(size*sizeof(ttable[0]));
	if (!ttable)
		return 1;

	if (readObjectAtVa(fin, (void *)ttable, addr, size*sizeof(ttable[0])) != (int)(size*sizeof(ttable[0]))) {
		fprintf(rdplog, "Failed to read data at va=0x%.8x\n", addr);
		goto bail;
	}

	for (i = 0; i < rows; i++) {
		for (j = 0; j < cols; j++) {
			unsigned index = i*cols + j;
			addr = ttable[index];
			table[index] = NULL;
			if (addr) {
				if (readObjectAtVa(fin, (void *)str, addr, PMLOG_MAX_STRING_LEN) < 2) {
					fprintf(rdplog, "Failed to read data at va=0x%.8x\n", addr);
					goto bail;
				}
				str_len = strlen(str);
				if (pmlog_check_str(str)) {
					fprintf(rdplog, "Bad string at va=0x%.8x\n", addr);
					goto bail;
				}
				table[index] = (char *)malloc(str_len + 1);
				if (!table[index])
					goto bail;
				strcpy(table[index], str);
			}
		}
	}
	ret = 0;
bail:
	if (ttable)
		free(ttable);
	if (ret)
		pmlog_free_table(table, rows, cols);
	return ret;
}

static int pmlog_emit_events(FILE *fout, char **table, unsigned rows, unsigned cols)
{
	unsigned i, j, magic, zero;
	char *str;

	magic = htonl(EVENT_DB_HEADER_MAGIC);
	buf_to_hex((UINT8*)&magic, sizeof(magic), fout);
	magic = htonl(EVENT_DB_ROW_MAGIC);
	zero = 0;
	for (i = 0; i < rows; i++) {
		buf_to_hex((UINT8*)&magic, sizeof(magic), fout);
		for (j = 0; j < cols; j++) {
			str = table[i*cols + j];
			if (!str)
				buf_to_hex((UINT8*)&zero, sizeof(UINT8), fout);
			else
				buf_to_hex((UINT8*)str, strlen(str) + 1, fout);
		}
	}
	return 0;
}

static int pmlog_handle_events(const char* inName, FILE *fin, FILE *fout, unsigned addr, unsigned rows, unsigned cols)
{
	char **table = 0;
	unsigned size = rows*cols;
	int ret = 1;

	if (size > PMLOG_MAX_EV_STRINGS) {
		fprintf(rdplog, "pmlog: bad event string count %d\n", size);
		return 1;
	}
	table = (char **)malloc(size*sizeof(table[0]));
	if (!table)
		return 1;

	memset(table,0,size*sizeof(table[0]));

	/* On PXA1801 kernel RO is not included into the dump (overwritten by the dump agents code.
	The pm_logger_app_db table and the strings it contains pointers to is located in RO, therefore
	we have to check the contents and detect the case table is invalid, so we can use one of the default event DBs. */
	if (!pmlog_extract_events(fin, table, addr, rows, cols)
		|| !pmlog_extract_default_events(inName, table, rows, cols)
		|| !pmlog_use_builtin_events(table, rows, cols))
		pmlog_emit_events(fout, table, rows, cols);
	pmlog_free_table(table, rows, cols);
	free(table);
	return 0;
}

/* addr is the va of the first op name field; objsize allows us to find the next one etc. */
static int pmlog_extract_ops(FILE *fin, FILE *fout, unsigned addr, unsigned objsize, unsigned objnum)
{
	UINT8 str[PMLOG_MAX_STRING_LEN];
    unsigned magic;
	unsigned i;
	int ret = -1;

	if (objnum > PMLOG_MAX_OPS) {
		fprintf(rdplog, "pmlog: bad ops count %d\n", objnum);
		return 1;
	}

	magic = htonl(OP_TABLE_HEADER_MAGIC);
	buf_to_hex((UINT8*)&magic, sizeof(magic), fout);
	for (i = 0; i < objnum; i++) {
		if (readObjectAtVa(fin, (void *)str, addr, PMLOG_MAX_STRING_LEN) < 2) {
			fprintf(rdplog, "Failed to read data at va=0x%.8x\n", addr);
			goto bail;
		}

		if (strlen((char*)str) >= PMLOG_MAX_STRING_LEN)
					goto bail;

		magic = htonl(OP_TABLE_ROW_MAGIC);
		buf_to_hex((UINT8*)&magic, sizeof(magic), fout);
		buf_to_hex(str, OP_NAME_LEN, fout);
		addr += objsize;
	}
	ret = 0;
bail:
	return ret;
}

/* addr is the va of the first op name field; objsize allows us to find the next one etc. */
/* iterate the list as done in include/linux/list.h:list_for_each_entry */
/**
 * list_for_each_entry  -       iterate over list of given type
 * @pos:        the type * to use as a loop cursor.
 * @head:       the head for your list.
 * @member:     the name of the list_struct within the struct.
 *
#define list_for_each_entry(pos, head, member)                          \
        for (pos = list_entry((head)->next, typeof(*pos), member);      \
             prefetch(pos->member.next), &pos->member != (head);        \
             pos = list_entry(pos->member.next, typeof(*pos), member))
 **
 * list_entry - get the struct for this entry
 * @ptr:        the &struct list_head pointer.
 * @type:       the type of the struct this is embedded in.
 * @member:     the name of the list_struct within the struct.
 *
#define list_entry(ptr, type, member) \
        container_of(ptr, type, member)
*/
static int pmlog_extract_trace(FILE *fin, FILE *fout, unsigned addr, unsigned offsnext, unsigned offsname)
{
	UINT8 obj[PMLOG_MAX_OBJSIZE];
	UINT8 i;
	unsigned addr0;
	unsigned magic;
	int ret = -1;
	UINT8 *str;

	if ((offsnext > PMLOG_MAX_OBJSIZE) || (offsname > PMLOG_MAX_OBJSIZE)) {
		fprintf(rdplog, "pmlog: bad dvfm trace object offsets %d - %d\n", offsnext, offsname);
		return 1;
	}

	addr0 = addr;
	/* Fetch head->next at (addr), which will be the first object address */
	if (readObjectAtVa(fin, (void *)&addr, addr0, sizeof(unsigned)) < sizeof(unsigned)) {
			fprintf(rdplog, "Failed to read data at va=0x%.8x\n", addr0);
			goto bail;
		}
	magic = htonl(DEV_TABLE_HEADER_MAGIC);
	buf_to_hex((UINT8*)&magic, sizeof(magic), fout);
	for (i = 0; ; i++) {
		if (readObjectAtVa(fin, (void *)obj, addr, PMLOG_MAX_OBJSIZE) < PMLOG_MAX_OBJSIZE) {
			fprintf(rdplog, "Failed to read data at va=0x%.8x\n", addr);
			goto bail;
		}

		str = obj - offsnext + offsname;
		if (strlen((char*)str) >= PMLOG_MAX_STRING_LEN)
					goto bail;

		magic = htonl(DEV_TABLE_ROW_MAGIC);
		buf_to_hex((UINT8*)&magic, sizeof(magic), fout);
		buf_to_hex(&i, sizeof (UINT8), fout);
		buf_to_hex(str, DVFM_MAX_NAME, fout);
		addr = *(unsigned *)(obj);
		if (addr == addr0)
			break; /* end of list */
	}
	ret = 0;
bail:
	return ret;
}

int pmlog_parser(const char* inName, FILE *fin, const char *name, struct rdc_dataitem *rdi, int nw)
{
	FILE *fbin = 0;
	FILE *fhex = 0;
	int ret = 1;
	char *binname = changeNameExt(inName, name);
	char *hexname = changeExt(binname, "hex");

	// These functions extract the events, ops, and trace arrays from the dump
	if (!binname || ((fbin = fopen(binname, "rb")) == 0)) {
		fprintf(rdplog, "Failed to open input file %s\n", binname);
		goto bail;
	}

	if (!hexname || ((fhex = fopen(hexname, "wt+")) == 0)) {
		fprintf(rdplog, "Failed to open output file %s\n", hexname);
		goto bail;
	}

	pmlog_bin_to_hex(fbin, fhex);
	pmlog_handle_events(inName, fin, fhex, rdi->body.w[4], rdi->body.w[6], rdi->body.w[5]);
	pmlog_extract_ops(fin, fhex, rdi->body.w[7], rdi->body.w[8], rdi->body.w[9]);
	pmlog_extract_trace(fin, fhex, rdi->body.w[10], rdi->body.w[11], rdi->body.w[12]);

	ret = 0;
bail:
	if (fbin)
		fclose(fbin);
	if (fhex) {
		fclose(fhex);
		if (ret)
			unlink(hexname);
	}
	if (binname)
		free(binname);
	if (hexname)
		free(hexname);
	return ret;
}
