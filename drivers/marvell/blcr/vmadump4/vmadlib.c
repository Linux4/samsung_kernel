/*-------------------------------------------------------------------------
 *  vmadlib.c: vmadump library management program
 *
 *  Copyright (C) 1999-2001 by Erik Hendriks <erik@hendriks.cx>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: vmadlib.c,v 1.3 2004/12/07 22:20:35 phargrov Exp $
 *-----------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syscall.h>
#include <limits.h>

#include "vmadump.h"

void usage(char *arg0) {
    printf(
"Usage: %s -c\n"
"       %s -a [libs...] \n"
"       %s -d [libs...] \n"
"       %s -l\n"
"\n"
"       This program manages the VMAdump in-kernel library list.\n"
"       -h            Display this message and exit.\n"
"       -v            Display version information and exit.\n"
"       -c            Clear kernel library list.\n"
"       -a [libs...]  Add to the kernel library list.\n"
"       -d [libs...]  Delete from the kernel library list.\n"
"       -l            Print the contents of the kernel library list.\n",
        arg0, arg0, arg0, arg0);
}

enum { MODE_CLEAR, MODE_ADD, MODE_DEL, MODE_LIST };

#ifdef __NR_vmadump
static
void add_lib(char *libname) {
    if (syscall(__NR_vmadump, VMAD_LIB_ADD, libname) == -1) {
	perror(libname);
	exit(1);
    }
}

static
void remove_lib(char *libname) {
    if (syscall(__NR_vmadump, VMAD_LIB_DEL, libname) == -1) {
	perror(libname);
	/*exit(1);*/
    }
}
static
void clear_libs(void) {
    if (syscall(__NR_vmadump, VMAD_LIB_CLEAR) == -1) {
	perror("VMAD_LIB_CLEAR");
	exit(1);
    }
}
#else

static
void add_lib(char *libname) {
    if (bproc_libadd(libname)) {
	perror(libname);
	exit(1);
    }
}
static
void remove_lib(char *libname) {
    if (bproc_libdel(libname)) {
	perror(libname);
    }
}
static
void clear_libs(vois) {
    if (bproc_libclear()) {
	perror("VMAD_LIB_CLEAR");
	exit(1);
    }
}
#endif


static
void remove_trailing_newline(char *line) {
    int len;
    len = strlen(line);
    if (line[len-1] == '\n') line[len-1] = 0;
}

int main(int argc, char *argv[]) {
    int c,i;
    int mode = -1;
    int len;
    char buf[PATH_MAX];
    char *listbuf, *p;
    
    while((c=getopt(argc, argv, "hvclad")) != EOF) {
	switch(c) {
	case 'h': usage(argv[0]); exit(0);
	case 'v': printf("%s version %s\n", argv[0], PACKAGE_VERSION); exit(0);
	case 'c': mode = MODE_CLEAR; break;
	case 'l': mode = MODE_LIST; break;
	case 'a': mode = MODE_ADD; break;
	case 'd': mode = MODE_DEL; break;
	default: exit(1);
	}
    }

    switch(mode) {
    case MODE_CLEAR:
	if (argc - optind != 0) {
	    fprintf(stderr, "No library names allowed with -c\n");
	    exit(1);
	}
	clear_libs();
	break;
    case MODE_ADD:
	for (i=optind; i < argc; i++) {
	    if (strcmp(argv[i], "-") == 0) {
		while (fgets(buf, PATH_MAX, stdin)) {
		    remove_trailing_newline(buf);
		    add_lib(buf);
		}
	    } else
		add_lib(argv[i]);
	}
	break;
    case MODE_DEL:
	for (i=optind; i < argc; i++) {
	    if (strcmp(argv[i], "-") == 0) {
		while (fgets(buf, PATH_MAX, stdin)) {
		    remove_trailing_newline(buf);
		    remove_lib(buf);
		}
	    } else {
		remove_lib(argv[i]);
	    }
	}
	break;
    case MODE_LIST:
	len = syscall(__NR_vmadump, VMAD_LIB_SIZE);
	if (len == -1) {
	    perror("VMAD_LIB_SIZE");
	    exit(1);
	}
	listbuf = malloc(len);
	if (!listbuf) {
	    fprintf(stderr, "Out of memory.\n");
	    exit(1);
	}
	if (syscall(__NR_vmadump, VMAD_LIB_LIST, listbuf, len) == -1) {
	    perror("VMAD_LIB_LIST");
	    exit(1);
	}
	/* print out the null delimited list of libraries */
	for (p = listbuf; *p; p += strlen(p)+1)
	    printf("%s\n", p);
	break;
    default:
	usage(argv[0]);
    }
    exit(0);
}
/*
 * Local variables:
 * c-basic-offset: 4
 * End:
 */
