/*
 * Copyright (c) 2007-2013 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef USB_MANAGER_CLIENT_H_
#define USB_MANAGER_CLIENT_H_

extern iref_t usb_manager_iref;
extern struct usb_manager_rpc_client usb_manager;

extern struct usb_generic_descriptor *gen_descriptor;

#endif /* USB_MANAGER_CLIENT_H_ */