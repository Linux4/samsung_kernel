/*
 * Copyright (c) [2009-2013] Marvell International Ltd. and its affiliates.
 * All rights reserved.
 * This software file (the "File") is owned and distributed by Marvell
 * International Ltd. and/or its affiliates ("Marvell") under the following
 * licensing terms.
 * If you received this File from Marvell, you may opt to use, redistribute
 * and/or modify this File in accordance with the terms and conditions of
 * the General Public License Version 2, June 1991 (the "GPL License"), a
 * copy of which is available along with the File in the license.txt file
 * or by writing to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 or on the worldwide web at
 * http://www.gnu.org/licenses/gpl.txt. THE FILE IS DISTRIBUTED AS-IS,
 * WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY
 * DISCLAIMED. The GPL License provides additional details about this
 * warranty disclaimer.
 */
#include <stdio.h>
#include <stdlib.h>
#include "tee_perf.h"

/* arg1: src name
   arg2: dst name
   arg3: first record name
 */
int main(int argc, char *argv[])
{
	FILE *src_fd = NULL;
	FILE *dst_fd = NULL;
	unsigned int len;
	unsigned int last_val = 0;
	unsigned int total_val = 0;
	tee_perf_desc head;
	tee_perf_record current;
	unsigned int found_header = 0;
	unsigned int i = 0;

	if (argc != 4) {
		printf("please use below command line:\n");
		printf("    ./perf_parser <src> <dst> <first_record_name>\n");
		return -1;
	}

	src_fd = fopen(argv[1], "rb")
	if (!src_fd) {
		printf("open file src failed\n");
		return -1;
	}

	dst_fd = fopen(argv[2], "wb")
	if (!dst_fd) {
		printf("open file dst failed\n");
		fclose(src_fd);
		return -1;
	}
	fread(&head, sizeof(tee_perf_desc), 1, src_fd);
	for (i = 0; i < head.total; i++) {
		if (found_header == 0) {
			len =
			    fread(&current, sizeof(tee_perf_record), 1, src_fd);
			if (len < 1)
				break;
			if (!memcmp(current.name, argv[3], 4)) {
				fprintf(dst_fd, "| %s-", argv[3]);
				total_val = 0;
				last_val = current.value;
				found_header = 1;
			} else {
				continue;
			}
		}

		len = fread(&current, sizeof(tee_perf_record), 1, src_fd);
		if (len < 1)
			break;
		if (memcmp(current.name, argv[3], 4)) {
			fprintf(dst_fd, "-%d-->%c%c%c%c-",
				current.value - last_val, current.name[0],
				current.name[1], current.name[2],
				current.name[3]);
			total_val += current.value - last_val;
			last_val = current.value;
		} else {
			fprintf(dst_fd, "->| total %d\n| %s-", total_val,
				argv[3]);
			last_val = current.value;
			total_val = 0;
		}
	}

	fclose(dst_fd);
	fclose(src_fd);

	return 0;
}
