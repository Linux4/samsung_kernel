/*------------------------------------------------------------
(C) Copyright [2006-2008] Marvell International Ltd.
All Rights Reserved
------------------------------------------------------------*/

#define ARRAY_SIZE(array) (sizeof(array)/sizeof(array[0]))
#define seq_printf fprintf
#include "rdp.h"

typedef unsigned char u8;
typedef unsigned short u16;

#include "i2c-logger.h"

//...

int i2c_logger_str2err(const char *str)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(i2c_err); i++) {
		if (strcmp(str, i2c_err[i].str) == 0)
			return i2c_err[i].id;
	}
return ERR_UNKNOWN;
}

static char *err2str(int id)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(i2c_err); i++) {
		if (i2c_err[i].id == id)
			return i2c_err[i].str;
	}
return i2c_err[ARRAY_SIZE(i2c_err)-1].str;
}

static void i2c_logger_print(union i2c_log_msg *msg, FILE *m)
{
	char *err;
	struct mandatory_field *mandatory;
	struct optional_field1 *optional_1;
	struct optional_field2 *optional_2;
	struct type *type;

	type = &msg->type;
	switch (GET_TYPE(type->value)) {
	case MANDATORY_MSG:
		mandatory = &msg->mandatory;
		seq_printf(m, MANDATORY_FIELD_FORMAT,
				MANDATORY_FIELD_DATA(mandatory));
		if (GET_ERR(mandatory->type) != ERR_NO_ERROR) {
			err = err2str(GET_ERR(mandatory->type));
			seq_printf(m, "\t%s\n", err);
		}
		break;
	case OPTIONAL_MSG1:
		optional_1 = &msg->optional_1;
		seq_printf(m, OPTIONAL_MSG1_FIELD_FORMAT,
			OPTIONAL_MSG1_FIELD_DATA(optional_1));
		break;
	case OPTIONAL_MSG2:
		optional_2 = &msg->optional_2;
		seq_printf(m, OPTIONAL_MSG2_FIELD_FORMAT,
			OPTIONAL_MSG2_FIELD_DATA(optional_2));
		break;
	}
}

int i2clog_parser(const char* inName, FILE *fin, const char *name, struct rdc_dataitem *rdi, int nw)
{
	FILE *fbin = 0;
	FILE *ftxt = 0;
	int i;
	int ret = 1;

	union i2c_log_msg msg;

	char *binname = changeNameExt(inName, name);
	char *txtname = changeExt(binname, "txt");

	if (!binname || ((fbin = fopen(binname, "rb")) == NULL)) {
		fprintf(rdplog, "Failed to open input file %s\n", binname);
		goto bail;
	}

	if (!txtname || ((ftxt = fopen(txtname, "wt+")) == NULL)) {
		fprintf(rdplog, "Failed to open output file %s\n", txtname);
		goto bail;
	}

	for (i = 0; !feof(fbin); i++) {
		if (fread((void *)&msg, sizeof(msg), 1, fbin) != 1) {
			fprintf(rdplog, "Failed to read input record #%d: stop parsing\n", i);
			break;
		}
		i2c_logger_print(&msg, ftxt);
	}
	ret = 0;

bail:
	if (fbin)
		fclose(fbin);
	if (ftxt) {
		fclose(ftxt);
		if (ret)
			unlink(txtname);
	}
	return ret;
}
