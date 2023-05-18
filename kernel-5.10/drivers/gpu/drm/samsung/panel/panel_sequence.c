#include <linux/kernel.h>
#include <linux/types.h>
#include "panel.h"
#include "panel_debug.h"
#include "panel_obj.h"
#include "panel_sequence.h"

bool is_valid_sequence(struct seqinfo *seq)
{
	if (!seq)
		return false;

	if (!IS_CMD_TYPE_SEQ(get_pnobj_cmd_type(&seq->base)))
		return false;

	if (seq->cmdtbl == NULL)
		return false;

	return true;
}

char *get_sequence_name(struct seqinfo *seq)
{
	return get_pnobj_name(&seq->base);
}

/**
 * create_sequence - create a struct seqinfo structure
 * @name: pointer to a string for the name of this sequence.
 * @size : number of commands.
 *
 * This is used to create a struct seqinfo pointer.
 *
 * Returns &struct seqinfo pointer on success, or NULL on error.
 *
 * Note, the pointer created here is to be destroyed when finished by
 * making a call to destroy_sequence().
 */
struct seqinfo *create_sequence(char *name, size_t size)
{
	struct seqinfo *seq;

	if (!name)
		return NULL;

	if (!size)
		return NULL;

	seq = kzalloc(sizeof(*seq), GFP_KERNEL);
	if (!seq)
		return NULL;

	seq->cmdtbl = kzalloc(size * sizeof(void *), GFP_KERNEL);
	if (!seq->cmdtbl)
		goto err;

	pnobj_init(&seq->base, CMD_TYPE_SEQ, name);
	seq->size = size;

	return seq;

err:
	kfree(seq);
	return NULL;
}
EXPORT_SYMBOL(create_sequence);

/**
 * destroy_sequence - destroys a struct seqinfo structure
 * @seq: pointer to the struct seqinfo that is to be destroyed
 *
 * Note, the pointer to be destroyed must have been created with a call
 * to create_sequence().
 */
void destroy_sequence(struct seqinfo *seq)
{
	if (!seq)
		return;

	free_pnobj_name(&seq->base);
	kfree(seq->cmdtbl);
	kfree(seq);
}
EXPORT_SYMBOL(destroy_sequence);

__visible_for_testing void add_edge(struct list_head *head, int vertex)
{
	struct command_node *node;

	node = kmalloc(sizeof(*node), GFP_KERNEL);
	node->vertex = vertex;
	list_add_tail(&node->list, head);
}

/* TODO: need depth limit */
__visible_for_testing void topo_sort(struct list_head adj_list[],
		int vertex, bool visited[], int stack[], int *top)
{
	struct command_node *pos;

	visited[vertex] = true;
	list_for_each_entry(pos, &adj_list[vertex], list) {
		if (!visited[pos->vertex])
			topo_sort(adj_list, pos->vertex, visited, stack, top);
	}
	stack[(*top)++] = vertex;
}

__visible_for_testing bool check_cycle(struct list_head adj_list[],
		int n, int stack[], int *top)
{
	int i, *order;

	order = kmalloc_array(n, sizeof(*order), GFP_KERNEL);
	for (i = 0; i < n; i++)
		order[stack[*top - i - 1]] = i;

	for (i = 0; i < n; i++) {
		struct command_node *pos;

		list_for_each_entry(pos, &adj_list[i], list) {
			if (order[i] > order[pos->vertex]) {
				kfree(order);
				return true;
			}
		}
	}

	kfree(order);
	return false;
}

__visible_for_testing bool is_cyclic_util(struct list_head *adj_list,
		int v, bool visited[], bool *recur_stack)
{
	struct command_node *pos;

	if (!visited[v]) {
		visited[v] = true;
		recur_stack[v] = true;

		list_for_each_entry(pos, &adj_list[v], list) {
			if (!visited[pos->vertex] &&
					is_cyclic_util(adj_list, pos->vertex, visited, recur_stack))
				return true;
			else if (recur_stack[pos->vertex])
				return true;
		}
	}
	recur_stack[v] = false;

	return false;
}

__visible_for_testing bool is_cyclic(struct list_head *adj_list, int n)
{
	bool *visited, *recur_stack;
	bool cycle = false;
	int i;

	visited = kzalloc(sizeof(*visited) * n, GFP_KERNEL);
	recur_stack = kzalloc(sizeof(*visited) * n, GFP_KERNEL);

	for (i = 0; i < n; i++) {
		if (!visited[i] &&
				is_cyclic_util(adj_list, i, visited, recur_stack)) {
			cycle = true;
			goto out;
		}
	}

out:
	kfree(visited);
	kfree(recur_stack);

	return cycle;
}

/* TODO: change to hash table search */
int get_index_of_sequence(struct list_head *seq_list,
		struct seqinfo *sequence)
{
	struct pnobj *pos;
	int index = 0;

	if (!sequence)
		return -1;

	list_for_each_entry(pos, seq_list, list) {
		if (pos == &sequence->base)
			return index;
		index++;
	}

	return -1;
}

int get_count_of_sequence(struct list_head *seq_list)
{
	int num_seq = 0;
	struct pnobj *pos;

	list_for_each_entry(pos, seq_list, list) {
		if (!is_valid_sequence(
					pnobj_container_of(pos, struct seqinfo)))
			continue;

		num_seq++;
	}

	return num_seq;
}

int sequence_sort(struct list_head *seq_list)
{
	struct list_head *adj_list;
	bool *visited;
	int *stack;
	int i, iseq = 0, index, top = 0, ret = 0;
	unsigned int num_seq;
	struct seqinfo *sequence;
	struct seqinfo **seq_array;
	struct pnobj *pos, *command;

	num_seq = get_count_of_sequence(seq_list);
	adj_list = kzalloc(sizeof(*adj_list) * num_seq, GFP_KERNEL);
	visited = kzalloc(sizeof(*visited) * num_seq, GFP_KERNEL);
	stack = kzalloc(sizeof(*stack) * num_seq, GFP_KERNEL);
	seq_array = kmalloc_array(num_seq, sizeof(struct seqinfo *), GFP_KERNEL);

	/* add_edge */
	list_for_each_entry(pos, seq_list, list) {
		INIT_LIST_HEAD(&adj_list[iseq]);

		sequence = pnobj_container_of(pos, struct seqinfo);
		if (!is_valid_sequence(sequence))
			continue;

		seq_array[iseq] = sequence;
		for (i = 0; i < sequence->size && sequence->cmdtbl[i]; i++) {
			command = sequence->cmdtbl[i];
			if (!IS_CMD_TYPE_SEQ(get_pnobj_cmd_type(command)))
				continue;

			index = get_index_of_sequence(seq_list,
					pnobj_container_of(command, struct seqinfo));
			if (index < 0) {
				panel_err("failed to get index_of_sequnece(%s)\n",
						get_pnobj_name(command));
				ret = -EINVAL;
				goto err;
			}

			add_edge(&adj_list[iseq], index);
		}
		iseq++;
	}

	if (is_cyclic(adj_list, num_seq)) {
		panel_err("cycle exists\n");
		ret = -EINVAL;
		goto err;
	}

	/* topological sorting */
	for (i = 0; i < num_seq; i++) {
		if (!visited[i])
			topo_sort(adj_list, i, visited, stack, &top);
	}

	if (check_cycle(adj_list, num_seq, stack, &top)) {
		panel_err("unsorted order detected\n");
		ret = -EINVAL;
		goto err;
	}

	/* make sorted sequence list */
	INIT_LIST_HEAD(seq_list);
	for (i = 0; i < num_seq; i++)
		list_add_tail(get_pnobj_list(&seq_array[stack[i]]->base),
				seq_list);

	/* free command_node */
	for (i = 0; i < num_seq; i++) {
		struct command_node *pos, *next;

		list_for_each_entry_safe(pos, next, &adj_list[i], list)
			kfree(pos);
	}

err:
	kfree(adj_list);
	kfree(visited);
	kfree(stack);
	kfree(seq_array);

	return ret;
}
