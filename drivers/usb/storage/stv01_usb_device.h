#ifndef _STV01_USB_DEVICE_H_
#define _STV01_USB_DEVICE_H_

#include <linux/usb.h>
#include <linux/usb_usual.h>
#include <linux/usb/quirks.h>

/*
 * Unusual device list definitions 
 */
struct stv01_usb_data_s;

struct us_unusual_dev {
	const char* vendorName;
	const char* productName;
	__u8  useProtocol;
	__u8  useTransport;
	int (*initFunction)(struct stv01_usb_data_s *);
};

extern struct us_unusual_dev us_unusual_dev_list[19];
extern struct usb_device_id usb_storage_usb_ids[19];
extern struct us_unusual_dev for_dynamic_ids;

int get_device_info(struct stv01_usb_data_s *us, const struct usb_device_id *id,
		struct us_unusual_dev *unusual_dev);
#endif /// _STV01_USB_DEVICE_H_
