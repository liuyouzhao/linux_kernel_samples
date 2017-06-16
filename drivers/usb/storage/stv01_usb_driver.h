/*
 * Driver for USB Mass Storage compliant devices
 * Main Header File
 *
 * Current development and maintenance by:
 *   (c) 1999-2002 Matthew Dharm (mdharm-usb@one-eyed-alien.net)
 *
 * Initial work by:
 *   (c) 1999 Michael Gee (michael@linuxspecific.com)
 *
 * This driver is based on the 'USB Mass Storage Class' document. This
 * describes in detail the protocol used to communicate with such
 * devices.  Clearly, the designers had SCSI and ATAPI commands in
 * mind when they created this document.  The commands are all very
 * similar to commands in the SCSI-II and ATAPI specifications.
 *
 * It is important to note that in a number of cases this class
 * exhibits class-specific exemptions from the USB specification.
 * Notably the usage of NAK, STALL and ACK differs from the norm, in
 * that they are used to communicate wait, failed and OK on commands.
 *
 * Also, for certain devices, the interrupt endpoint is used to convey
 * status of a command.
 *
 * Please see http://www.one-eyed-alien.net/~mdharm/linux-usb for more
 * information about this driver.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _STV01_USB_DRIVER_H_
#define _STV01_USB_DRIVER_H_

/* General routines provided by the usb-storage standard core */
#ifdef CONFIG_PM


#else
#define usb_stor_suspend	NULL
#define usb_stor_resume		NULL
#define usb_stor_reset_resume	NULL
#endif


#define module_usb_stor_driver(__driver, __sht, __name) \
static int __init __driver##_init(void) \
{ \
    printk(KERN_INFO "registering STV01 drive\n"); \
	usb_stor_host_template_init(&(__sht), __name, THIS_MODULE); \
	return usb_register(&(__driver)); \
} \
module_init(__driver##_init); \
static void __exit __driver##_exit(void) \
{ \
	usb_deregister(&(__driver)); \
} \
module_exit(__driver##_exit)

#endif /// _STV01_USB_DRIVER_H_
