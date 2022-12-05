/*******************************************************************************
 *
 *	Linked list.
 *
 *	File:	list.h
 *  Author:	Fotis Panagiotopoulos
 *  Date:	13/09/2022
 *
 *
 ******************************************************************************/

#ifndef LIST_H_
#define LIST_H_

/* List. */
typedef struct {
	void * head;
	void * tail;
} List_t;

/* List item. */
typedef struct {
	void * next;
	/* Item members. */
} List_Item_t;


/*
 *	Initializes a list.
 *
 *	Parameters:
 *		list		List handle.
 */
void List_init(List_t * list);

/*
 *	Clears a list.
 *
 *	Parameters:
 *		list		List handle.
 */
#define List_clear(list)			List_init(list)

/*
 *	Gets the size of the list.
 *
 *	Parameters:
 *		list		List handle.
 *
 *	Returns the list size.
 */
unsigned List_size(List_t * list);

/*
 *	Adds an item at the end of the list.
 *
 *	Parameters:
 *		list		List handle.
 *		item		The item to add.
 */
void List_add(List_t * list, void * item);

/*
 *	Adds an item at the start of the list.
 *
 *	Parameters:
 *		list		List handle.
 *		item		The item to add.
 */
void List_addFirst(List_t * list, void * item);

/*
 *	Adds an item at the end of the list.
 *
 *	Parameters:
 *		list		List handle.
 *		item		The item to add.
 */
#define List_addLast(list, item)	List_add((list), (item))

/*
 *	Removes an item from the list.
 *
 *	Parameters:
 *		list		List handle.
 *		item		The item to remove.
 */
void List_remove(List_t * list, void * item);

/*
 *	Removes the first item of the list.
 *
 *	Parameters:
 *		list		List handle.
 *
 *	Returns the removed item, or NULL if the list is empty.
 */
void * List_removeFirst(List_t * list);

/*
 *	Removes the last item of the list.
 *
 *	Parameters:
 *		list		List handle.
 *
 *	Returns the removed item, or NULL if the list is empty.
 */
void * List_removeLast(List_t * list);

/*
 *	Gets the first item of the list.
 *
 *	Parameters:
 *		list		List handle.
 *
 *	Returns the first item, or NULL if the list is empty.
 */
void * List_getFirst(List_t * list);

/*
 *	Gets the last item of the list.
 *
 *	Parameters:
 *		list		List handle.
 *
 *	Returns the last item, or NULL if the list is empty.
 */
void * List_getLast(List_t * list);

/*
 *	Gets the item at the specified position.
 *
 *	Parameters:
 *		list		List handle.
 *		at			The index of the item.
 *
 *	Returns the requested item, or NULL if the index is out of bounds.
 */
void * List_getAt(List_t * list, unsigned at);

/*
 *	Gets the next list item (iterator).
 *
 *	Parameters:
 *		list		List handle.
 *		prev		The previous item (may be NULL).
 *
 *	Returns the next item, or NULL if the end of the list has been reached.
 */
void * List_getNext(List_t * list, void * prev);


#endif

