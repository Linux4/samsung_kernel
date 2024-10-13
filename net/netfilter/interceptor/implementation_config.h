/**
   @copyright
   Copyright (c) 2011 - 2014, INSIDE Secure Oy. All rights reserved.
*/


#ifndef IMPLEMENTATION_CONFIG_H
#define IMPLEMENTATION_CONFIG_H

#define DEBUG_IMPLEMENTATION_HEADER "debug_implementation.h"
#define DEBUG_FILENAME "/dev/null"
#define DEBUG_FILTER_FILENAME "/data/data/com.ipsec.service/debug/ipsecdebug_filter.txt"

#ifdef __KERNEL__
#define STRING_HEADER "kernelspd_defs.h"
#else
#define INTTYPES_HEADER <inttypes.h>
#define STRING_HEADER <string.h>
#define SNPRINTF_HEADER <stdio.h>
#endif

#endif /* IMPLEMENTATION_CONFIG_H */
