#ifndef SKB_LLIST_H
#define SKB_LLIST_H
/*
 * modified from <linux/llist.h>
 *
 * Lock-less NULL terminated single linked list
 * can be used in multiple producers and one consumer case
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <asm/cmpxchg.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>	/* dev_kfree_skb_any */

struct skb_llist_head {
	struct sk_buff *first;
};

#define SKB_LLIST_HEAD_INIT(name)	{ NULL }
#define SKB_LLIST_HEAD(name) \
	struct skb_llist_head name = SKB_LLIST_HEAD_INIT(name)

/**
 * init_skb_llist_head - initialize lock-less list head
 * @head:	the head for your lock-less list
 */
static inline void init_skb_llist_head(struct skb_llist_head *list)
{
	list->first = NULL;
}

/**
 * skb_llist_empty - tests whether a lock-less list is empty
 * @head:	the list to test
 *
 * Not guaranteed to be accurate or up to date.  Just a quick way to
 * test whether the list is empty without deleting something from the
 * list.
 */
static inline bool skb_llist_empty(const struct skb_llist_head *head)
{
	return ACCESS_ONCE(head->first) == NULL;
}

static inline struct sk_buff *skb_llist_next(struct sk_buff *node)
{
	return node->next;
}

/**
 * skb_llist_add - add a new entry
 * @new:	new entry to be added
 * @head:	the head for your lock-less list
 *
 * Returns true if the list was empty prior to adding this entry.
 */
static inline bool skb_llist_add(struct sk_buff *new,
				struct skb_llist_head *head)
{

	struct sk_buff *first;

	do {
		new->next = first = ACCESS_ONCE(head->first);
	} while (cmpxchg(&head->first, first, new) != first);

	return !first;
}

/**
 * llist_del_all - delete all entries from lock-less list
 * @head:	the head of lock-less list to delete all entries
 *
 * If list is empty, return NULL, otherwise, delete all entries and
 * return the pointer to the first entry.  The order of entries
 * deleted is from the newest to the oldest added one.
 */
static inline struct sk_buff *skb_llist_del_all(struct skb_llist_head *head)
{
	return xchg(&head->first, NULL);
}

struct sk_buff *skb_llist_reverse_order(struct sk_buff *head);


struct skb_llist {
	struct skb_llist_head head;

	atomic_t len;
	/* can only be used by the consumer */
	struct sk_buff *detached;
};

static inline u32 skb_llist_len(struct skb_llist *list)
{
	return atomic_read(&list->len);
}

static inline void skb_llist_enqueue(struct skb_llist *list,
					struct sk_buff *skb)
{
	skb_llist_add(skb, &list->head);
	atomic_inc(&list->len);
}

static inline void skb_llist_queue_head(struct skb_llist *list,
					struct sk_buff *skb)
{
	skb->next = list->detached;
	list->detached = skb;
	atomic_inc(&list->len);
}

static inline struct sk_buff *skb_llist_peek(struct skb_llist *list)
{
	if (!list->detached) {
		list->detached = skb_llist_del_all(&list->head);
		list->detached = skb_llist_reverse_order(list->detached);
	}

	return list->detached;
}

static inline struct sk_buff *skb_llist_dequeue(struct skb_llist *list)
{
	struct sk_buff *skb = NULL;

	skb = skb_llist_peek(list);

	if (list->detached) {
		list->detached = list->detached->next;
		atomic_dec(&list->len);
	}

	return skb;
}

static inline void skb_llist_init(struct skb_llist *list)
{
	init_skb_llist_head(&list->head);
	list->detached = NULL;
	atomic_set(&list->len, 0);
}

static inline void skb_llist_clean(struct skb_llist *list)
{
	struct sk_buff *head = list->detached;

	atomic_set(&list->len, 0);
	list->detached = NULL;

	while (head) {
		struct sk_buff *tmp = head;
		head = head->next;
		dev_kfree_skb_any(tmp);
	}

	head = skb_llist_del_all(&list->head);

	while (head) {
		struct sk_buff *tmp = head;
		head = head->next;
		dev_kfree_skb_any(tmp);
	}
}

#endif /* LLIST_H */
