#ifndef _STV01_USB_STORAGE_H_
#define _STV01_USB_STORAGE_H_

#include <linux/pm.h>
#include <linux/usb.h>

#include "stv01_utils.h"

/**
 * pm_message_t declared in linux/pm.h
 * usb_interface declared in linux/usb.h
*/
typedef struct stv01_usb_storage_method_s
{
    int (*probe)        (struct usb_interface *, const struct usb_device_id *);
    int (*suspend)      (struct usb_interface *, pm_message_t);
    int (*resume)       (struct usb_interface *);
    void (*disconnect)  (struct usb_interface *);
    int (*reset_resume) (struct usb_interface *);
    int (*pre_reset)    (struct usb_interface *);
    int (*post_reset)   (struct usb_interface *);

} stv01_usb_storage_method_t;

EXTERN_PTR(stv01_usb_storage_method_t, usm)

#endif /// _STV01_USB_STORAGE_H_
