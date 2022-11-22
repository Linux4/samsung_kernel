/*
 * Berkeley Lab Checkpoint/Restart (BLCR) for Linux is Copyright (c)
 * 2008, The Regents of the University of California, through Lawrence
 * Berkeley National Laboratory (subject to receipt of any required
 * approvals from the U.S. Dept. of Energy).  All rights reserved.
 *
 * Portions may be copyrighted by others, as may be noted in specific
 * copyright notices within specific files.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: module.c,v 1.2.32.2 2010/08/12 21:58:31 phargrov Exp $
 */

#include "blcr_config.h"

#ifdef CR_NEED_AUTOCONF_H
#include <linux/autoconf.h>
#endif
#if defined(CONFIG_SMP) && ! defined(__SMP__)
  #define __SMP__
#endif
#if defined(CONFIG_MODVERSIONS) && ! defined(MODVERSIONS)
  #define MODVERSIONS
#endif

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/kallsyms.h>

MODULE_AUTHOR("Lawrence Berkeley National Lab " PACKAGE_BUGREPORT);
MODULE_DESCRIPTION("Berkeley Lab Checkpoint/Restart symbol imports kernel module");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif

#include <linux/utsname.h>
#if HAVE_INIT_UTSNAME
  #define cr_utsname() init_utsname()
#elif HAVE_SYSTEM_UTSNAME
  #define cr_utsname() (&system_utsname)
#endif
#include <linux/version.h>
#ifndef UTS_RELEASE
 #if HAVE_LINUX_UTSRELEASE_H
  #include <linux/utsrelease.h>
 #elif HAVE_GENERATED_UTSRELEASE_H
  #include <generated/utsrelease.h>
 #endif
#endif
#if HAVE_LINUX_COMPILE_H
  #include <linux/compile.h>
#endif

static int __init blcr_imports_init(void)
{
	/* Check current kernel against UTS_RELEASE/VERSION from configure time */
	{
	    struct new_utsname *tmp = cr_utsname();
	    if (0 != strcmp(tmp->release, UTS_RELEASE)) {
		printk(KERN_ERR "Running kernel UTS_RELEASE (%s) does not match that used to build BLCR (" UTS_RELEASE ")\n", tmp->release);
		return -EINVAL;
	    }
#ifdef UTS_VERSION /* Often NOT available in configured kernel headers */
	    if (0 != strcmp(tmp->version, UTS_VERSION)) {
		printk(KERN_ERR "Running kernel UTS_VERSION (%s) does not match that used to build BLCR (" UTS_VERSION ")\n", tmp->version);
		return -EINVAL;
	    }
#endif
	}

	/* Check current kernel against System.map used at configure time */
	{
#if defined(CR_EXPORTED_KCODE_register_chrdev)
	    unsigned long offset1 = CR_EXPORTED_KCODE_register_chrdev - (unsigned long)&register_chrdev;
#elif defined(CR_EXPORTED_KCODE___register_chrdev)
	    unsigned long offset1 = CR_EXPORTED_KCODE___register_chrdev - (unsigned long)&__register_chrdev;
#else
	    #error "No register_chrdev symbol for validation of System.map"
#endif
	    unsigned long offset2 = CR_EXPORTED_KCODE_register_blkdev - (unsigned long)&register_blkdev;
	    if (
#if defined(CONFIG_RELOCATABLE) && defined(CONFIG_PHYSICAL_ALIGN)
	    /* Check that relocation offset is valid and constant */
	        ((offset1 & (CONFIG_PHYSICAL_ALIGN - 1)) || (offset1 != offset2))
#else
	    /* No relocation */
	        (offset1 || offset2)
#endif
	        ) {
		printk(KERN_ERR "Running kernel does not match the System.map used to build BLCR\n");
		return -EINVAL;
	    }

	    #include "import_check.c"
	}

	return 0;
}

static void __exit blcr_imports_cleanup(void) { ; }

module_init(blcr_imports_init);
module_exit(blcr_imports_cleanup);
