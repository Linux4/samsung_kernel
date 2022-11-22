#ifndef KMEMPAGERECORDER
#include <linux/types.h>
#define OBJECT_TABLE_SIZE      1543
#define BACKTRACE_SIZE 10
#define BACKTRACE_LEVEL 10

typedef enum {
	NODE_PAGE_RECORD,
	NODE_PAGE_MAX
} PAGE_RECORD_TYPE;

typedef enum {
	HASH_PAGE_NODE_KERNEL_BACKTRACE,
	HASH_PAGE_NODE_KERNEL_SYMBOL,
	HASH_PAGE_NODE_KERNEL_PAGE_ALLOC_BACKTRACE,
	HASH_PAGE_NODE_MAX
} PAGE_HASH_NODE_TYPE;

/* 	for BT	*/
typedef struct PageObjectEntry {
	size_t slot;
	struct PageObjectEntry *prev;
	struct PageObjectEntry *next;
	size_t numEntries;
	size_t reference;
	unsigned int size;
	void *object[0];
} PageObjectEntry, *PPageObjectEntry;

typedef struct {
	size_t count;
	PageObjectEntry *slots[OBJECT_TABLE_SIZE];
} PageObjectTable, *PPageObjectTable;

/* 	for page	*/
typedef struct PageHashEntry {
	void *page;
	unsigned int size;
	struct PageHashEntry *prev;
	struct PageHashEntry *next;
	PPageObjectEntry allocate_map_entry;
	PPageObjectEntry bt_entry;
	unsigned int flag;
} PageHashEntry, *PPageHashEntry;

typedef struct {
	size_t count;
	size_t table_size;
	PageHashEntry *page_hash_table[OBJECT_TABLE_SIZE];
} PageHashTable;

struct page_mapping {
	char *name;
	unsigned long address;
	unsigned int size;
};

typedef struct page_record_param {
	unsigned int *page;
	unsigned int address_type;
	unsigned int address;
	unsigned int length;
	unsigned long backtrace[BACKTRACE_SIZE];
	unsigned int backtrace_num;
	unsigned long kernel_symbol[BACKTRACE_SIZE];
	struct page_mapping mapping_record[BACKTRACE_SIZE];
	unsigned int size;
} page_record_t;

/*	rank object	*/
struct page_object_rank_entry {
	PageObjectEntry *entry;
	struct page_object_rank_entry *prev;
	struct page_object_rank_entry *next;
};

void page_debug_show_backtrace(void);
int record_page_record(void *page, unsigned int order);
int remove_page_record(void *page, unsigned int order);
void disable_page_alloc_tracer(void);
#endif
#ifndef KMEMPAGERECORDER
#define KMEMPAGERECORDER
#endif
