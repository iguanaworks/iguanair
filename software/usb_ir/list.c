/****************************************************************************
 ** list.c ******************************************************************
 ****************************************************************************
 *
 * A general list implementation used by the tools.
 *
 * Copyright (C) 2007, IguanaWorks Incorporated (http://iguanaworks.net)
 * Author: Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distributed under the GPL version 2.
 * See LICENSE for license details.
 */
#include "iguanaIR.h"
#include "compat.h"

#include <stdlib.h>
#include <string.h>

#include "list.h"

void initializeList(listHeader *list)
{
    memset(list, 0, sizeof(listHeader));
}

void insertItem(listHeader *list, itemHeader *pos, itemHeader *item)
{
    itemHeader **prevPtr;
    item->next = pos;

    if (pos == NULL)
        prevPtr = &list->tail;
    else
        prevPtr = &pos->prev;

    item->prev = *prevPtr;
    if (item->prev == NULL)
        list->head = item;
    else
        item->prev->next = item;
    *prevPtr = item;
    list->count++;
    item->list = list;
}

const itemHeader* firstItem(listHeader *list)
{
    return list->head;
}

itemHeader* removeFirstItem(listHeader *list)
{
    return removeItem(list->head);
}

static void removeObject(listHeader *list, itemHeader *prev, itemHeader *next)
{
    if (prev == NULL)
        list->head = next;
    else
        prev->next = next;

    if (next == NULL)
        list->tail = prev;
    else
        next->prev = prev;

    list->count--;
}

itemHeader* removeItem(itemHeader *pos)
{
    if (pos != NULL)
    {
        removeObject(pos->list, pos->prev, pos->next);
        pos->list = NULL;
        pos->prev = NULL;
        pos->next = NULL;
    }

    return pos;
}

void forEach(listHeader *list, actionFunc action, void *userData)
{
    itemHeader *prev = NULL, *pos, *next;

    for(pos = list->head; pos; pos = next)
    {
        next = pos->next;
        if (! action(pos, userData))
            removeObject(list, prev, next);
        else
            prev = pos;
    }
}
