/****************************************************************************
 ** list.h ******************************************************************
 ****************************************************************************
 *
 * Declarations of a simple list implementation used in the tools.
 *
 * Copyright (C) 2007, IguanaWorks Incorporated (http://iguanaworks.net)
 * Author: Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distributed under the GPL version 2.
 * See LICENSE for license details.
 */
#ifndef _LIST_
#define _LIST_

typedef struct item
{
    struct item *prev, *next;
    struct list *list;
} itemHeader;

typedef struct list
{
    struct item *head, *tail;
    unsigned int count;
} listHeader;

/* zero out the list */
void initializeList(listHeader *list);

/* insert before pos, NULL pos means append */
void insertItem(listHeader *list, itemHeader *pos, itemHeader *item);

/* return a pointer to the firstItem */
const itemHeader* firstItem(listHeader *list);

/* actually remove the first item */
itemHeader* removeFirstItem(listHeader *list);

/* remove object at pos (uses internal list pointer) */
itemHeader* removeItem(itemHeader *pos);

typedef bool (*actionFunc)(itemHeader *item, void *userData);

/* to perform an action on every item in a list */
void forEach(listHeader *list, actionFunc action, void *userData);

#endif
