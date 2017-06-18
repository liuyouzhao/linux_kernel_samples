#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/usb.h>
#include <linux/usb_usual.h>
#include <linux/usb/quirks.h>

#include <linux/jiffies.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/utsname.h>
#include <linux/device.h>

#include <linux/kthread.h>
#include <linux/mutex.h>

#include <scsi/scsi.h>
#include <scsi/scsi_eh.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_devinfo.h>


#include "stv01_usb_driver.h"
#include "stv01_usb_device.h"
#include "stv01_usb_data.h"
#include "stv01_usb_storage.h"
#include "stv01_usb_transport.h"
#include "stv01_scsi_host.h"
#include "stv01_utils.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Hujia Liu <liuyouzhao@hotmail.com>");
MODULE_DESCRIPTION("USB STV01 Linux Course Practising Registration Driver");
#define DRV_NAME "stv01-driver"
#define USB_STORAGE "usb-storage: "

/*
 * The table of devices
#define USB_DEVICE_VER(vend, prod, lo, hi) \
.match_flags = USB_DEVICE_ID_MATCH_DEVICE_AND_VERSION, \
.idVendor = (vend), \
.idProduct = (prod), \
.bcdDevice_lo = (lo), \
.bcdDevice_hi = (hi)
 */
#undef USUAL_DEV
#define UNUSUAL_DEV(id_vendor, id_product, bcdDeviceMin, bcdDeviceMax, \
		    vendorName, productName, useProtocol, useTransport, \
		    initFunction, flags) \
{ USB_DEVICE_VER(id_vendor, id_product, bcdDeviceMin, bcdDeviceMax), \
  .driver_info = (flags) }

/*
usb_device_id attributes:
/// which fields to match against?
__u16           match_flags;
/// Used for product specific matches; range is inclusive
__u16           idVendor;
__u16           idProduct;
__u16           bcdDevice_lo;
__u16           bcdDevice_hi;
/// Used for device class matches
__u8            bDeviceClass;
__u8            bDeviceSubClass;
__u8            bDeviceProtocol;
/// Used for interface class matches
__u8            bInterfaceClass;
__u8            bInterfaceSubClass;
__u8            bInterfaceProtocol;
/// Used for vendor-specific interface matches
__u8            bInterfaceNumber;
/// not matched against
kernel_ulong_t  driver_info
        __attribute__((aligned(sizeof(kernel_ulong_t))));
*/
static struct usb_device_id stv01_usb_ids[] = {
    UNUSUAL_DEV( 0x125f, 0x317a, 0x0000, 0x9999, "steven", "stv01",
		USB_SC_SCSI, USB_PR_SDDR55, NULL,
		US_FL_FIX_INQUIRY),
	{ }		/* Terminating entry */
};
MODULE_DEVICE_TABLE(usb, stv01_usb_ids);
static struct scsi_host_template stv01_host_template;

void usb_stor_host_template_init(struct scsi_host_template *sht,
				 const char *name, struct module *owner)
{
	*sht = usb_stor_host_template;
	sht->name = name;
	sht->proc_name = name;
	sht->module = owner;
}

static int probe(struct usb_interface *intf,
	             const struct usb_device_id *id)
{
    printk(KERN_INFO "probe STV01 drive\n");
    if(IPTR(usm))
    {
        printk(KERN_INFO "usm is ok\n");
        return IPTR(usm)->probe(intf, id);
    }
    printk(KERN_INFO "probe failed, usm is NULL\n");
    return -1;
}

static int suspend(struct usb_interface *iface, pm_message_t message)
{
    return IPTR(usm)->suspend(iface, message);
}

static int resume(struct usb_interface *iface)
{
    return IPTR(usm)->resume(iface);
}

static void disconnect(struct usb_interface *intf)
{
    IPTR(usm)->disconnect(intf);
}

static int reset_resume(struct usb_interface *iface)
{
    return IPTR(usm)->reset_resume(iface);
}

static int pre_reset(struct usb_interface *iface)
{
    return IPTR(usm)->pre_reset(iface);
}

static int post_reset(struct usb_interface *iface)
{
    return IPTR(usm)->post_reset(iface);
}

/*
	.disconnect =	usb_stor_disconnect,
	.suspend =	usb_stor_suspend,
	.resume =	usb_stor_resume,
	.reset_resume =	usb_stor_reset_resume,
	.pre_reset =	usb_stor_pre_reset,
	.post_reset =	usb_stor_post_reset,
*/
static struct usb_driver stv01_driver = {
	.name           =		DRV_NAME,
	.probe          =	probe,
	.disconnect     =	disconnect,
	.suspend        =	suspend,
	.resume         =	resume,
	.reset_resume   =	reset_resume,
	.pre_reset      =	pre_reset,
    .post_reset     =	post_reset,
	.id_table       =	stv01_usb_ids,
	.supports_autosuspend = 1,
	.soft_unbind =	1
};

module_usb_stor_driver(stv01_driver, stv01_host_template, DRV_NAME);
