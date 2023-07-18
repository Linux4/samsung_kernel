// KONX RSA kernel module testing code
// Toru Ishihara
#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mman.h>
#include <linux/init.h>
#include <linux/raw.h>
#include <linux/tty.h>
#include <linux/capability.h>
#include <linux/ptrace.h>
#include <linux/device.h>
#include <linux/highmem.h>
#include <linux/crash_dump.h>
#include <linux/backing-dev.h>
#include <linux/bootmem.h>
#include <linux/splice.h>
#include <linux/export.h>
#include <linux/time.h>

#include "rsa.h"
#include "rsa_lib.h"

int rsatest_open(struct inode *inode, struct file *filp);
int rsatest_release(struct inode *inode, struct file *filp);
ssize_t rsatest_read(struct file *filp, char *buf, size_t count, loff_t *f_pos);
ssize_t rsatest_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos);
void rsatest_exit(void);
int rsatest_init(void);

/* Structure that declares the usual file */
/* access functions */
struct file_operations rsatest_fops = {
  .read = rsatest_read,
  .write = rsatest_write,
  .open = rsatest_open,
  .release = rsatest_release
};

/* Declaration of the init and exit functions */
//module_init(rsatest_init);
//module_exit(rsatest_exit);

/* Global variables of the driver */
/* Major number */
int rsatest_major = 60;
/* Buffer to store data */
unsigned char *rsatest_buffer;
int rsatest_size = 1024;
int rsatest_init(void) {
  int result;

  /* Registering device */
  result = register_chrdev(rsatest_major, "rsatest", &rsatest_fops);
  if (result < 0) {
    printk(
      "<1>memory: cannot obtain major number %d\n", rsatest_major);
    return result;
  }

  /* Allocating memory for the buffer */
  rsatest_buffer = kmalloc(rsatest_size, GFP_KERNEL);
  if (!rsatest_buffer) {
    result = -ENOMEM;
    goto fail;
  }
  memset(rsatest_buffer, 0, rsatest_size);
  printk("<1>Inserting rsatest module\n");
  return 0;

  fail:
    rsatest_exit();
    return result;
}
void rsatest_exit(void) {
  /* Freeing the major number */
  unregister_chrdev(rsatest_major, "rsatest");

  /* Freeing buffer memory */
  if (rsatest_buffer) {
    kfree(rsatest_buffer);
  }

  printk("<1>Removing rsatest module\n");

}
int rsatest_open(struct inode *inode, struct file *filp) {

  /* Success */
  return 0;
}
int rsatest_release(struct inode *inode, struct file *filp) {

  /* Success */
  return 0;
}

static void dump(unsigned char *buf, int len) {
    int i;

    printk("len=%d ", len);
    if (len > 32) {
        for(i=0;i<32;++i)
            printk("%02x ", (unsigned char)buf[i]);
        printk("....");
        for(i=0;i<32;++i)
            printk("%02x ", (unsigned char)buf[len-32+i]);
    } else if (len > 8) {
        for(i=0;i<8;++i)
            printk("%02x ", (unsigned char)buf[i]);
        printk("....");
        for(i=0;i<8;++i)
            printk("%02x ", (unsigned char)buf[len-8+i]);
    }
    printk("\n");
}

// KONX RSA kernel module testing code
int test_cnt = 0;
unsigned char KeyPair[1024] = {0};
int KeyPairLen = 0;
unsigned char PubKey[1024] = {0};
int PubKeyLen = 0;
unsigned char DEK[16] = {0};
char    test_buf[4096];
int     test_buf_len = 0;
char    enc[1024];
int     encLen;
char    dec[1024];
int     decLen;

static void test_series(int bits)
{
    int     len, i;
    struct timespec spec0, spec;

    {
        len = 1024;
        getnstimeofday(&spec0);
        rsa_genKeyPair(KeyPair, &len, bits);
        getnstimeofday(&spec);
        KeyPairLen = len;
        printk("KeyPair generation is done bit=%d size=%d time = %ld sec %ld nsec\n", bits, len,
        	timespec_sub(spec, spec0).tv_sec, timespec_sub(spec, spec0).tv_nsec);
        dump(KeyPair, len);
    }
    {
        len = 16;
        rsa_oaes_genKey(DEK, &len);
        printk("DEK generation is done size=%d\n", len);
        dump(DEK, len);
    }
    {
        len = 1024;
        rsa_getPublicKey(KeyPair, KeyPairLen, PubKey, &len);
        printk("getPublicKey is done size=%d\n", len);
        dump(PubKey, len);
        PubKeyLen = len;
    }
    {
        len = 1024;
        for(i=0;i<16;++i)
            test_buf[i] = 'a';
        dump(test_buf, 16);
        getnstimeofday(&spec0);
        rsa_encryptByPub(PubKey, PubKeyLen, test_buf, 16, enc, &len); 
        getnstimeofday(&spec);
        printk("rsa_encryptByPub is done size=%d time=%ld sec %ld nsec\n", len,
        	timespec_sub(spec, spec0).tv_sec, timespec_sub(spec, spec0).tv_nsec);
        dump(enc, len);
        encLen = len;
    }
    {
        len = 1024;
        //KeyPair[70] = 44;
        getnstimeofday(&spec0);
        rsa_decryptByPair(KeyPair, KeyPairLen, enc, encLen, dec, &len);
        getnstimeofday(&spec);
        //rsa_decryptByPair(PubKey, PubKeyLen, enc, encLen, dec, &len);
        printk("rsa_decryptByPri is done size=%d time=%ld sec %ld nsec\n", len,
			timespec_sub(spec, spec0).tv_sec, timespec_sub(spec, spec0).tv_nsec);
        dump(dec, len);
    }
    {
        len = 1024;
        for(i=0;i<32;++i)
            test_buf[i] = 'x';
        dump(test_buf, 32);
        memset(enc, 0, 1024);
        rsa_oaes_encrypt(DEK, test_buf, 32, enc, &len); 
        printk("rsa_oaes_encrypt is done size=%d\n", len);
        dump(enc, len);
        encLen = len;
    }
    {
        len = 1024;
        memset(dec, 0, 1024);
        DEK[8] = 44;
        rsa_oaes_decrypt(DEK, enc, encLen, dec, &len); 
        printk("rsa_oaes_decrypt is done size=%d\n", len);
        dump(dec, len);
    }
}

ssize_t rsatest_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
	test_series(256);
	test_series(512);
	test_series(1024);
	return 0;
}

ssize_t rsatest_write( struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
    printk("rsatest_write conunt=%d\n", count);
    if (count < sizeof(test_buf)) {
        memcpy(test_buf, buf, count);
        test_buf_len = count;
    }
    return count;
}
