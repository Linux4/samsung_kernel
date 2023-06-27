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
 * $Id: vmadtest.c,v 1.3 2004/12/07 22:20:35 phargrov Exp $
 *-----------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include "vmadump.h"

#ifndef __NR_vmadump
#include <sys/bproc.h>
#endif

#define GROWCHUNK 10240

#define DEFAULT_FILE "dump"
#if HAVE_BINFMT_VMADUMP
#define DUMPMODE 0777
#else
#define DUMPMODE 0666
#endif

extern char **environ;

void sighandler(void) { printf("In sighandler!\n"); }

#ifdef __NR_vmadump
int vmadump(int fd, int flags) {
    return syscall(__NR_vmadump, VMAD_DO_DUMP, fd, flags);
}

int vmaundump(int fd) {
    return syscall(__NR_vmadump, VMAD_DO_UNDUMP, fd);
}

int vmaexecdump(int fd, struct vmadump_execdump_args *args) {
    return syscall(__NR_vmadump, VMAD_DO_EXECDUMP, args);
}
#else
int vmadump(int fd, int flags) {
    return bproc_dump(fd, flags);
}

int vmaundump(int fd) {
    return bproc_undump(fd);
}

int vmaexecdump(int fd, struct vmadump_execdump_args *args) {
    return bproc_execdump(fd, args->flags,args->arg0, args->argv, args->envp);
}
#endif

void test_mm(void) {
    int i;
    printf("Testing basic memory ops like growing my stack\n"
	    "and malloc'ing new RAM.\n");
    for (i=0; i < 100; i++) {
	char *foo;
	foo = alloca(GROWCHUNK);
	memset(foo, 0, GROWCHUNK);
	printf("."); fflush(stdout);
	
	foo = malloc(GROWCHUNK);
	memset(foo, 0, GROWCHUNK);
	printf("+"); fflush(stdout);
    }
    printf("\n");
}

void do_dump(char *filename, int flags) {
    int fd, r;
    printf("Dumping to: %s\n", filename);
    fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC, DUMPMODE);
    if (fd == -1) { perror("open"); exit(1); }
    r = vmadump(fd,flags);
    printf("vmadump(%d,0x%x)=%d\n", fd, flags, r);
    if (r == -1) {
	perror("VMAD_DO_DUMP");
	exit(1);
    }
    close(fd);
    if (r) {
	printf("dump complete - exiting.\n");
	exit(0);
    }

    /* r == 0... Which means that we were dumped and we're getting
     * here because we've been undumped. */
    printf("Whoo!  I've been undumped!  Kick ass!\n");
    printf("New pid: %d\n", (int) getpid());

    test_mm();
    exit(0);
}

void do_execdump(char *filename, int flags, char **argv) {
    int fd;
    struct vmadump_execdump_args args;

    printf("Exec-dumping to: %s\n", filename);
    fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC, DUMPMODE);
    if (fd == -1) { perror("open"); exit(1); }
    args.arg0 = argv[optind];
    args.argv = argv+optind;
    args.envp = environ;
    args.fd   = fd;
    vmaexecdump(fd,&args);
    perror("VMAD_DO_EXEC_DUMP");
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
"       -d         Dump to a file.\n"
"       -u         Undump from a file.\n"
"       -f file    Dump to/from file instead of the default (%s)\n"
"       -F         Perform a floating point operation before (un)dumping.\n"
"\n"
"       The following options only affect dumping.  (-d)\n"
"       -e         Use VMAD_DUMP_EXEC flag.\n"
"       -l         Use VMAD_DUMP_LIBS flag.\n"
"       -o         Use VMAD_DUMP_OTHER flag.\n"
, arg0, arg0, DEFAULT_FILE);
}

double c;

int main(int argc, char *argv[]) {
    int fd, r,c ;
    int dump=-1;
    int dump_flags = 0;
    int fpop = 0;
    char *filename = DEFAULT_FILE;
    
    while ((c=getopt(argc, argv, "hvf:Fduelox")) != EOF) {
	switch (c) {
	case 'h':
	case 'v': usage(argv[0]); exit(0);
	case 'f': filename = optarg; break;
	case 'd': dump = 0; break;
	case 'u': dump = 1; break;
	case 'x': dump = 2; break;
	case 'e': dump_flags |= VMAD_DUMP_EXEC; break;
	case 'l': dump_flags |= VMAD_DUMP_LIBS; break;
	case 'o': dump_flags |= VMAD_DUMP_OTHER; break;
	case 'F': fpop = 1; break;
	default: exit(1);
	}
    }

    if (fpop) {
	double a, b;
	a = 5.3; 
	b = getpid();
	c = a * b;
    }

    switch (dump) {
    case 0:
	/* Set a sighandler to test sighandler save/restore */
	signal(SIGUSR1, (void (*)(int)) sighandler);
	do_dump(filename, dump_flags);
	break;

    case 1:
	printf("Undumping from: %s\n", filename);
	fd = open(filename, O_RDONLY);
	if (fd == -1) { perror("open"); exit(1); }
	r = vmaundump(fd);
	printf("VMAD_DO_UNDUMP failed: err=%d (%s)\n",
	       errno, strerror(errno));
	break;
    case 2:
	do_execdump(filename, dump_flags, argv);
	break;
    default:
	usage(argv[0]);
	exit(0);
  }
  exit(1);
}

/*
 * Local variables:
 * c-basic-offset: 4
 * End:
 */
