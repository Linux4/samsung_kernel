#include <sdp/common.h>
#include <sdp/cache_cleanup.h>

#if SDP_CACHE_CLEANUP_DEBUG
static void sdp_page_dump(unsigned char *buf, int len, const char* str)
{
    unsigned int i;
    char s[512];

    s[0] = 0;
    for(i=0;i<len && i<32;++i) {
        char tmp[8];
        sprintf(tmp, " %02x", buf[i]);
        strcat(s, tmp);
    }

    if (len > 32) {
        char tmp[8];
        sprintf(tmp, " ...");
        strcat(s, tmp);
    }

    printk("%s [%s len=%d]\n", s, str, len);
}
#else
static void sdp_page_dump(unsigned char *buf, int len, const char* str) {
    return;
}
#endif

void sdp_page_cleanup(struct page *page)
{
    if(page && page->mapping) {
        if(mapping_sensitive(page->mapping)) {
            void *d;

#if SDP_CACHE_CLEANUP_DEBUG
            printk("%s : deleting [%s] sensitive page.\n",
                    __func__, page->mapping->host->i_sb->s_type->name);
            //dump_stack();
#endif
            d = kmap_atomic(page);
            if(d) {
                sdp_page_dump((unsigned char *)d, PAGE_SIZE, "freeing");
                clear_page(d);
                sdp_page_dump((unsigned char *)d, PAGE_SIZE, "freed");
                kunmap_atomic(d);
            }
        }
    }
}