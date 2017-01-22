/**
  Copyright (c) 2008 - 2015 Quantenna Communications Inc
  All Rights Reserved

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

 **/

#ifndef _HAPR_LIST_H_
#define _HAPR_LIST_H_

#include "hapr_types.h"

struct hapr_list
{
	struct hapr_list *prev;
	struct hapr_list *next;
};

static __inline void __hapr_list_add(struct hapr_list *nw,
	struct hapr_list *prev,
	struct hapr_list *next)
{
	next->prev = nw;
	nw->next = next;
	nw->prev = prev;
	prev->next = nw;
}

static __inline void __hapr_list_remove(struct hapr_list *prev,
	struct hapr_list *next)
{
	next->prev = prev;
	prev->next = next;
}

static __inline void hapr_list_init(struct hapr_list *list)
{
	list->next = list;
	list->prev = list;
}

static __inline void hapr_list_add(struct hapr_list *head, struct hapr_list *nw)
{
	__hapr_list_add(nw, head, head->next);
}

static __inline void hapr_list_add_tail(struct hapr_list *head, struct hapr_list *nw)
{
	__hapr_list_add(nw, head->prev, head);
}

static __inline void hapr_list_remove(struct hapr_list *entry)
{
	__hapr_list_remove(entry->prev, entry->next);
	hapr_list_init(entry);
}

static __inline int hapr_list_empty(const struct hapr_list *head)
{
	return ((head->next == head) && (head->prev == head));
}

#define hapr_list_first(container_type, head, member) \
	(hapr_list_empty((head)) ? NULL : hapr_containerof((head)->next, container_type, member))

#define hapr_list_last(container_type, head, member) \
	(hapr_list_empty((head)) ? NULL : hapr_containerof((head)->prev, container_type, member))

#define hapr_list_for_each(container_type, iter, head, member) \
	for (iter = hapr_containerof((head)->next, container_type, member); \
		&iter->member != (head); \
		iter = hapr_containerof(iter->member.next, container_type, member))

#define hapr_list_for_each_safe(container_type, iter, tmp_iter, head, member) \
	for (iter = hapr_containerof((head)->next, container_type, member), \
		tmp_iter = hapr_containerof(iter->member.next, container_type, member); \
		&iter->member != (head); \
		iter = tmp_iter, \
		tmp_iter = hapr_containerof(tmp_iter->member.next, container_type, member))

#endif
