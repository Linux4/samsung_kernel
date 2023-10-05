// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include "dl/dsp-rule-reader.h"

int dsp_exp_import(struct dsp_exp_element *exp, struct dsp_dl_lib_file *file)
{
	int ret;

	ret = dsp_dl_lib_file_read((char *)exp, sizeof(*exp), file);
	if (ret == -1) {
		dsp_err("CHK_ERR\n");
		return -1;
	}

	return 0;
}

void dsp_exp_free(struct dsp_list_head *head)
{
	dsp_list_free(head, struct dsp_exp_element, list_node);
}

int dsp_bit_slice_import(struct dsp_bit_slice *bs, struct dsp_dl_lib_file *file)
{
	int ret;

	ret = dsp_dl_lib_file_read((char *)bs, sizeof(*bs), file);
	if (ret == -1) {
		dsp_err("CHK_ERR\n");
		return -1;
	}

	return 0;
}

void dsp_bit_slice_free(struct dsp_list_head *head)
{
	dsp_list_free(head, struct dsp_bit_slice, list_node);
}

int dsp_pos_import(struct dsp_pos *pos, struct dsp_dl_lib_file *file)
{
	int ret, idx;

	ret = dsp_dl_lib_file_read((char *)pos, sizeof(*pos), file);
	if (ret == -1) {
		dsp_err("CHK_ERR\n");
		return -1;
	}

	if (pos->type == BIT_SLICE_LIST) {
		int list_num = pos->bit_slice_list.num;

		dsp_list_head_init(&pos->bit_slice_list);

		for (idx = 0; idx < list_num; idx++) {
			struct dsp_bit_slice *bs =
				(struct dsp_bit_slice *)dsp_dl_malloc(
					sizeof(*bs),
					"Bit slice");

			dsp_bit_slice_import(bs, file);
			dsp_list_node_init(&bs->list_node);
			dsp_list_node_push_back(&pos->bit_slice_list,
				&bs->list_node);
		}
	}

	return 0;
}

void dsp_pos_free(struct dsp_list_head *head)
{
	struct dsp_list_node *cur, *next;

	cur = head->next;

	while (cur != NULL) {
		struct dsp_pos *pos =
			container_of(cur, struct dsp_pos, list_node);

		next = cur->next;

		if (pos->type == BIT_SLICE_LIST)
			dsp_bit_slice_free(&pos->bit_slice_list);

		dsp_dl_free(pos);
		cur = next;
	}
}

int dsp_reloc_rule_import(struct dsp_reloc_rule *rule,
	struct dsp_dl_lib_file *file)
{
	int ret;
	int list_num, idx;

	ret = dsp_dl_lib_file_read((char *)rule, sizeof(*rule), file);
	if (ret == -1) {
		dsp_err("CHK_ERR\n");
		return -1;
	}

	list_num = rule->exp.num;
	dsp_list_head_init(&rule->exp);

	for (idx = 0; idx < list_num; idx++) {
		struct dsp_exp_element *exp =
			(struct dsp_exp_element *)
			dsp_dl_malloc(
				sizeof(*exp),
				"Exp elem");

		ret = dsp_exp_import(exp, file);

		if (ret == -1) {
			dsp_err("CHK_ERR\n");
			return -1;
		}

		dsp_list_node_init(&exp->list_node);
		dsp_list_node_push_back(&rule->exp, &exp->list_node);
	}

	list_num = rule->pos_list.num;
	dsp_list_head_init(&rule->pos_list);

	for (idx = 0; idx < list_num; idx++) {
		struct dsp_pos *pos = (struct dsp_pos *)dsp_dl_malloc(
				sizeof(*pos), "Pos");

		ret = dsp_pos_import(pos, file);

		if (ret == -1) {
			dsp_err("CHK_ERR\n");
			return -1;
		}

		dsp_list_node_init(&pos->list_node);
		dsp_list_node_push_back(&rule->pos_list, &pos->list_node);
	}

	return 0;
}

void dsp_reloc_rule_free(struct dsp_reloc_rule *rule)
{
	dsp_exp_free(&rule->exp);
	dsp_pos_free(&rule->pos_list);
}

int dsp_reloc_rule_list_import(struct dsp_reloc_rule_list *list,
	struct dsp_dl_lib_file *file)
{
	int ret;
	char magic[4];
	int idx;

	dsp_dbg("Rule list import\n");

	dsp_dl_lib_file_reset(file);
	ret = dsp_dl_lib_file_read(magic, sizeof(char) * 4, file);
	if (ret == -1) {
		dsp_err("CHK_ERR\n");
		return -1;
	}

	if (magic[0] != 'R' || magic[1] != 'U' || magic[2] != 'L' ||
		magic[3] != 'E') {
		dsp_err("Magic number is incorrect\n");
		return -1;
	}

	ret = dsp_dl_lib_file_read((char *)&list->cnt, sizeof(int), file);
	if (ret == -1) {
		dsp_err("CHK_ERR\n");
		return -1;
	}

	if (list->cnt > RULE_MAX) {
		dsp_err("Rule list count(%d) is over RULE_MAX(%d)\n",
				list->cnt, RULE_MAX);
		return -1;
	}
	dsp_dbg("Rule list count %d\n", list->cnt);

	for (idx = 0; idx < list->cnt; idx++) {
		list->list[idx] = (struct dsp_reloc_rule *)dsp_dl_malloc(
				sizeof(*list->list[idx]),
				"Reloc rule");
		dsp_reloc_rule_import(list->list[idx], file);
	}

	return 0;
}

void dsp_reloc_rule_list_free(struct dsp_reloc_rule_list *list)
{
	int idx;

	for (idx = 0; idx < list->cnt; idx++) {
		struct dsp_reloc_rule *rule = list->list[idx];

		dsp_reloc_rule_free(rule);
		dsp_dl_free(rule);
	}
}
