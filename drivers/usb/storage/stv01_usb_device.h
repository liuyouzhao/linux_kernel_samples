#ifndef _STV01_USB_DEVICE_H_
#define _STV01_USB_DEVICE_H_

#include <linux/usb.h>
#include <linux/usb_usual.h>
#include <linux/usb/quirks.h>

#include "stv01_utils.h"

/*
 * Unusual device list definitions 
 */
struct stv01_usb_data_s;

/**
 * pm_message_t declared in linux/pm.h
 * usb_interface declared in linux/usb.h
*/
typedef struct stv01_usb_device_method_s
{
    int (*get_info) ( struct stv01_usb_data_s *us,
                      const struct usb_device_id *id );
    void (*get_protocol)(struct stv01_usb_data_s *us);
    void (*get_transport)(struct stv01_usb_data_s *us);

} stv01_usb_device_method_t;

EXTERN_PTR(stv01_usb_device_method_t, dim)

#endif /// _STV01_USB_DEVICE_H_
