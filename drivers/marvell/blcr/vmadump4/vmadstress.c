/*-------------------------------------------------------------------------
 *  vmadtest.c: vmadump test program
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
 * $Id: vmadstress.c,v 1.3 2004/12/07 22:20:35 phargrov Exp $
 *-----------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include "vmadump.h"

#define GROWCHUNK 10240

#define DEFAULT_FILE "dump"
#if HAVE_BINFMT_VMADUMP
#define DUMPMODE 0777
#else
#define DUMPMODE 0666
#endif

#ifdef __NR_vmadump
int vmadump(int fd, int flags) {
    return syscall(__NR_vmadump, VMAD_DO_DUMP, fd, flags);
}

int vmaundump(int fd) {
    return syscall(__NR_vmadump, VMAD_DO_UNDUMP, fd);
}

#else
#include <sys/bproc.h>
int vmadump(int fd, int flags) {
    return bproc_dump(fd, flags);
}

int vmaundump(int fd) {
    return bproc_undump(fd);
}
#endif


void test_mm(void) {
    fprintf(stderr, "Testing basic memory ops like growing my stack\n"
	    "and malloc'ing new RAM.\n");
    while(1) {
	char *foo;
	foo = alloca(GROWCHUNK);
	memset(foo, 0, GROWCHUNK);
	fprintf(stderr, "."); fflush(stderr);
	
	foo = malloc(GROWCHUNK);
	memset(foo, 0, GROWCHUNK);
	fprintf(stderr, "+"); fflush(stderr);
	
	sleep(1);		/* lets not use all memory instantly, ok? */
    }
}

int do_dump(char *filename, int flags) {
    int fd, r;
    fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC, DUMPMODE);
    if (fd == -1) { perror("open"); exit(1); }
    r = vmadump(fd,flags);
    if (r == -1) {
	perror("VMAD_DO_DUMP");
	exit(1);
    }
    close(fd);
    return r;
}

void do_undump(char *filename) {
    int fd, r;
    fd = open(filename, O_RDONLY);
    if (fd == -1) { perror("open"); exit(1); }
    r = vmaundump(fd);
    /* not reached if no error */
    perror("VMAD_DO_DUMP");
    exit(1);
}

void usage(char *arg0) {
    printf(
"Usage: %s -d [-f file] [-e] [-l] [-o]\n"
"       %s -u [-f file]\n"
"\n"
"       This program is a simple test program for the Beowulf VMAdump\n"
"       system.  It will try to dump itself (with -d) or restore a dump\n"
"       with -u.\n"
"\n"
"       -f file    Dump to/from file instead of the default (%s)\n"
"\n"
"       The following options only affect dumping.  (-d)\n"
"       -e         Use VMAD_DUMP_EXEC flag.\n"
"       -l         Use VMAD_DUMP_LIBS flag.\n"
"       -o         Use VMAD_DUMP_OTHER flag.\n"
, arg0, arg0, DEFAULT_FILE);
}

int main(int argc, char *argv[]) {
    int c;
    int dump_flags = 0;
    char *filename = DEFAULT_FILE;
    
    while ((c=getopt(argc, argv, "hvf:elo")) != EOF) {
	switch (c) {
	case 'h':
	case 'v': usage(argv[0]); exit(0);
	case 'f': filename = optarg; break;
	case 'e': dump_flags |= VMAD_DUMP_EXEC; break;
	case 'l': dump_flags |= VMAD_DUMP_LIBS; break;
	case 'o': dump_flags |= VMAD_DUMP_OTHER; break;
	default: exit(1);
	}
    }

    while (1) {
	int r;
	printf("Doing vmadump(\"%s\", 0x%x)=", filename, dump_flags); fflush(stdout);
	r = do_dump(filename, dump_flags);
	if (r > 0) {
	    printf("%d\n", r);
	    printf("Doing vmaundump(\"%s\")\n", filename);
	    fflush(stdout);
	    do_undump(filename);
	    /* not reached */
	}
	close(3); /* hack to deal with the lack of close_on_exec */
    }
}

/*
 * Local variables:
 * c-basic-offset: 4
 * End:
 */
