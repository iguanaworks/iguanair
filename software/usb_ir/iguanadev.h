/****************************************************************************
 ** iguanadev.h *************************************************************
 ****************************************************************************
 *
 * A couple functions used to interface with Iguanaworks USB devices.
 *
 * Copyright (C) 2006, Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distribute under the GPL version 2.
 * See COPYING for license details.
 */
#ifndef _IGUANA_DEV_
#define _IGUANA_DEV_

/* start a thread to handle a single device instance */
void startWorker(struct usbDevice *dev);
/* terminate and join with each child thread */
bool reapAllChildren(struct usbDeviceList *list);

extern bool readLabels;

#endif
