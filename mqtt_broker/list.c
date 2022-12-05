/*******************************************************************************
 *
 *	Linked list.
 *
 *	File:	list.c
 *  Author:	Fotis Panagiotopoulos
 *  Date:	13/09/2022
 *
 *
 ******************************************************************************/

#include "list.h"
#include <assert.h>
#include <sys/types.h>


void List_init(List_t * list)
{
	DEBUGASSERT(list);

	list->head = NULL;
	list->tail = NULL;
}

unsigned List_size(List_t * list)
{
	DEBUGASSERT(list);

	unsigned size = 0;

	List_Item_t * it = list->head;
	while (it)
	{
		size++;
		it = it->next;
	}

	return size;
}

void List_add(List_t * list, void * item)
{
	DEBUGASSERT(list);
	DEBUGASSERT(item);

	if (list->head == NULL)
	{
		DEBUGASSERT(list->tail == NULL);

		list->head = item;
		list->tail = item;
		((List_Item_t*)item)->next = NULL;
	}
	else
	{
		DEBUGASSERT(list->tail != NULL);
		DEBUGASSERT(((List_Item_t*)list->tail)->next == NULL);

		((List_Item_t*)list->tail)->next = item;
		list->tail = item;
		((List_Item_t*)item)->next = NULL;
	}
}

void List_addFirst(List_t * list, void * item)
{
	DEBUGASSERT(list);
	DEBUGASSERT(item);

	if (list->head == NULL)
	{
		DEBUGASSERT(list->tail == NULL);

		list->head = item;
		list->tail = item;
		((List_Item_t*)item)->next = NULL;
	}
	else
	{
		((List_Item_t*)item)->next = list->head;
		list->head = item;
	}
}

void List_remove(List_t * list, void * item)
{
	DEBUGASSERT(list);
	DEBUGASSERT(item);

	if (list->head == item)
	{
		list->head = ((List_Item_t*)item)->next;

		if (list->tail == item)
		{
			DEBUGASSERT(((List_Item_t*)item)->next == NULL);
			list->tail = NULL;
		}

		((List_Item_t*)item)->next = NULL;

		return;
	}
	else
	{
		List_Item_t * it = list->head;
		List_Item_t * prev = NULL;
		while (it)
		{
			if (it == item)
			{
				DEBUGASSERT(prev);
				prev->next = ((List_Item_t*)item)->next;

				if (item == list->tail)
				{
					DEBUGASSERT(((List_Item_t*)item)->next == NULL);
					list->tail = prev;
				}

				((List_Item_t*)item)->next = NULL;

				return;
			}

			prev = it;
			it = it->next;
		}
	}

	DEBUGASSERT(0);
}

void * List_removeFirst(List_t * list)
{
	DEBUGASSERT(list);

	if (list->head == NULL)
	{
		DEBUGASSERT(list->tail == NULL);
		return NULL;
	}

	List_Item_t * item = list->head;
	List_remove(list, item);
	return item;
}

void * List_removeLast(List_t * list)
{
	DEBUGASSERT(list);

	if (list->tail == NULL)
	{
		DEBUGASSERT(list->head == NULL);
		return NULL;
	}

	List_Item_t * item = list->tail;
	List_remove(list, item);
	return item;
}

void * List_getFirst(List_t * list)
{
	DEBUGASSERT(list);
	return list->head;
}

void * List_getLast(List_t * list)
{
	DEBUGASSERT(list);
	return list->tail;
}

void * List_getAt(List_t * list, unsigned at)
{
	unsigned idx = 0;
	List_Item_t * it = list->head;
	while (it)
	{
		if (idx == at)
			return it;

		idx++;
		it = it->next;
	}

	return NULL;
}

void * List_getNext(List_t * list, void * prev)
{
	DEBUGASSERT(list);

	if ((List_Item_t*)prev == NULL)
		return list->head;
	else
		return ((List_Item_t*)prev)->next;
}

