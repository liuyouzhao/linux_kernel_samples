#include <linux/mutex.h>
#include <linux/usb.h>
#include <linux/usb_usual.h>
#include <linux/usb/quirks.h>

#include <scsi/scsi.h>
#include <scsi/scsi_eh.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_devinfo.h>

#include <linux/kthread.h>
#include <linux/mutex.h>

#include "stv01_usb_storage.h"
#include "stv01_usb_data.h"
#include "stv01_usb_device.h"
#include "stv01_usb_transport.h"
#include "stv01_scsi_host.h"
#include "stv01_urb.h"
#include "stv01_kthread.h"
#include "stv01_utils.h"


#define US_SUSPEND	0
#define US_RESUME	1

static unsigned int delay_use = 1;

/**************************************
laogong comments:
linux/usb.h usb_get_intfdata return void*
usb_interface to stv01_usb_data_s struct
***************************************/
void INTERVAL_IMPL __usb_storage_disconnect_impl(struct stv01_usb_data_s *us);
void INTERVAL_IMPL __usb_storage_report_bus_reset(struct stv01_usb_data_s *us);

static int usb_storage_suspend(struct usb_interface *iface, pm_message_t message)
{
    /* usb_get_intfdata method from linux/usb.h */
	struct stv01_usb_data_s *us = usb_get_intfdata(iface);

	/* Wait until no command is running */
	mutex_lock(&us->dev_mutex);

#ifdef CONFIG_PM
	if (us->suspend_resume_hook)
		(us->suspend_resume_hook)(us, US_SUSPEND);
#endif

	/* When runtime PM is working, we'll set a flag to indicate
	 * whether we should autoresume when a SCSI request arrives. */

	mutex_unlock(&us->dev_mutex);
	return 0;
}

static int usb_storage_resume(struct usb_interface *iface)
{
	struct stv01_usb_data_s *us = usb_get_intfdata(iface);

	mutex_lock(&us->dev_mutex);

#ifdef CONFIG_PM
	if (us->suspend_resume_hook)
		(us->suspend_resume_hook)(us, US_RESUME);
#endif

	mutex_unlock(&us->dev_mutex);
	return 0;
}

/* Dissociate from the USB device */
static void dissociate_dev(struct stv01_usb_data_s *us)
{
	/* Free the buffers */
	kfree(us->cr);
	usb_free_coherent(us->pusb_dev, US_IOBUF_SIZE, us->iobuf, us->iobuf_dma);

	/* Remove our private data from the interface */
	usb_set_intfdata(us->pusb_intf, NULL);
}

/* Release all our dynamic resources */
static void release_resources(struct stv01_usb_data_s *us)
{
	/* Tell the control thread to exit.  The SCSI host must
	 * already have been removed and the DISCONNECTING flag set
	 * so that we won't accept any more commands.
	 */
	utils_device_dbg(us, "-- sending exit command to thread\n");

    /**
     * Why complete here? the kthread will be wakeup and try to go over
     * then we call kthread_stop waiting util kthread actually gets finish.
     */
	complete(&us->cmnd_ready);

    /* here wait for kthread stop
     * kthread will set should_stop then waiting for the kthread over.
    */
	if (us->ctl_thread)
		kthread_stop(us->ctl_thread);

	/* Call the destructor routine, if it exists */
	if (us->extra_destructor) {
		utils_device_dbg(us, "-- calling extra_destructor()\n");
		us->extra_destructor(us->extra);
	}

	/* Free the extra data and the URB */
	kfree(us->extra);
	usb_free_urb(us->current_urb);
}


/* Second stage of disconnect processing: deallocate all resources */
static void release_everything(struct stv01_usb_data_s *us)
{
	release_resources(us);
	dissociate_dev(us);

	/* Drop our reference to the host; the SCSI core will free it
	 * (and "us" along with it) when the refcount becomes 0. */
	scsi_host_put(us_to_host(us));
}


/* Handle a USB mass-storage disconnect */
static void usb_storage_disconnect(struct usb_interface *intf)
{
	struct stv01_usb_data_s *us = usb_get_intfdata(intf);

    __usb_storage_disconnect_impl(us);
	//quiesce_and_remove_host(us);
	release_everything(us);
}

static int usb_storage_reset_resume(struct usb_interface *iface)
{
	struct stv01_usb_data_s *us = usb_get_intfdata(iface);

	/* Report the reset to the SCSI core */
	__usb_storage_report_bus_reset(us);

	/* FIXME: Notify the subdrivers that they need to reinitialize
	 * the device */
	return 0;
}

static int usb_storage_pre_reset(struct usb_interface *iface)
{
	struct stv01_usb_data_s *us = usb_get_intfdata(iface);

	/* Make sure no command runs during the reset */
	mutex_lock(&us->dev_mutex);
	return 0;
}

static int usb_storage_post_reset(struct usb_interface *iface)
{
	struct stv01_usb_data_s *us = usb_get_intfdata(iface);

	/* Report the reset to the SCSI core */
	__usb_storage_report_bus_reset(us);

	/* FIXME: Notify the subdrivers that they need to reinitialize
	 * the device */

	mutex_unlock(&us->dev_mutex);
	return 0;
}


/* Determine what the maximum LUN supported is */
static int usb_stor_Bulk_max_lun(struct stv01_usb_data_s *us)
{
	int result;

	/* issue the command */
	us->iobuf[0] = 0;
	result = usb_stor_control_msg(us, us->recv_ctrl_pipe,
				 US_BULK_GET_MAX_LUN, 
				 USB_DIR_IN | USB_TYPE_CLASS | 
				 USB_RECIP_INTERFACE,
				 0, us->ifnum, us->iobuf, 1, 10*HZ);

	utils_device_dbg(us, "GetMaxLUN command result is %d, data is %d\n",
		     result, us->iobuf[0]);

	/*
	 * If we have a successful request, return the result if valid. The
	 * CBW LUN field is 4 bits wide, so the value reported by the device
	 * should fit into that.
	 */
	if (result > 0) {
		if (us->iobuf[0] < 16) {
			return us->iobuf[0];
		} else {
			dev_info(&us->pusb_intf->dev,
				 "Max LUN %d is not valid, using 0 instead",
				 us->iobuf[0]);
		}
	}

	/*
	 * Some devices don't like GetMaxLUN.  They may STALL the control
	 * pipe, they may return a zero-length result, they may do nothing at
	 * all and timeout, or they may fail in even more bizarrely creative
	 * ways.  In these cases the best approach is to use the default
	 * value: only one LUN.
	 */
	return 0;
}

/* Delayed-work routine to carry out SCSI-device scanning */
static void usb_stor_scan_dwork(struct work_struct *work)
{
	struct stv01_usb_data_s *us = container_of(work, struct stv01_usb_data_s,
			scan_dwork.work);
	struct device *dev = &us->pusb_intf->dev;

	dev_dbg(dev, "starting scan\n");

	/* For bulk-only devices, determine the max LUN value */
	if (us->protocol == USB_PR_BULK &&
	    !(us->fflags & US_FL_SINGLE_LUN) &&
	    !(us->fflags & US_FL_SCM_MULT_TARG)) {
		mutex_lock(&us->dev_mutex);
		us->max_lun = usb_stor_Bulk_max_lun(us);
		/*
		 * Allow proper scanning of devices that present more than 8 LUNs
		 * While not affecting other devices that may need the previous behavior
		 */
		if (us->max_lun >= 8)
			us_to_host(us)->max_lun = us->max_lun+1;
		mutex_unlock(&us->dev_mutex);
	}
	scsi_scan_host(us_to_host(us));
	dev_dbg(dev, "scan complete\n");

	/* Should we unbind if no devices were detected? */

	usb_autopm_put_interface(us->pusb_intf);
	clear_bit(US_FLIDX_SCAN_PENDING, &us->dflags);
}

/* Associate our private data with the USB device */
static int associate_dev(struct stv01_usb_data_s *us, struct usb_interface *intf)
{
	/* Fill in the device-related fields */
	us->pusb_dev = interface_to_usbdev(intf);
	us->pusb_intf = intf;
	us->ifnum = intf->cur_altsetting->desc.bInterfaceNumber;
	utils_device_dbg(us, "Vendor: 0x%04x, Product: 0x%04x, Revision: 0x%04x\n",
		     le16_to_cpu(us->pusb_dev->descriptor.idVendor),
		     le16_to_cpu(us->pusb_dev->descriptor.idProduct),
		     le16_to_cpu(us->pusb_dev->descriptor.bcdDevice));
	utils_device_dbg(us, "Interface Subclass: 0x%02x, Protocol: 0x%02x\n",
		     intf->cur_altsetting->desc.bInterfaceSubClass,
		     intf->cur_altsetting->desc.bInterfaceProtocol);

	/* Store our private data in the interface */
	usb_set_intfdata(intf, us);

	/* Allocate the control/setup and DMA-mapped buffers */
	us->cr = kmalloc(sizeof(*us->cr), GFP_KERNEL);
	if (!us->cr)
		return -ENOMEM;

	us->iobuf = usb_alloc_coherent(us->pusb_dev, US_IOBUF_SIZE,
			GFP_KERNEL, &us->iobuf_dma);
	if (!us->iobuf) {
		utils_device_dbg(us, "I/O buffer allocation failed\n");
		return -ENOMEM;
	}
	return 0;
}

/* First part of general USB mass-storage probing */
static int usb_stor_probe1(     struct stv01_usb_data_s **pus,
		                        struct usb_interface *intf,
		                        const struct usb_device_id *id,
		                        struct scsi_host_template *sht)
{
	struct Scsi_Host *host;
	struct stv01_usb_data_s *us;
	int result;

	dev_info(&intf->dev, "STV01 USB Mass Storage device detected\n");
    
	/*
	 * Ask the SCSI layer to allocate a host structure, with extra
	 * space at the end for our private stv01_usb_data_s structure.
	 */
	host = scsi_host_alloc(sht, sizeof(*us));
	if (!host) {
		dev_warn(&intf->dev, "Unable to allocate the scsi host\n");
		return -ENOMEM;
	}
    printk(KERN_INFO " ===================1.1 \n");
    
	/*
	 * Allow 16-byte CDBs and thus > 2TB
	 */
	host->max_cmd_len = 16;

    /* Get sg tablesize from usb_interface, through device interface manager */
	host->sg_tablesize = IPTR(dim)->get_sg_tablesize(intf);

	*pus = us = host_to_us(host);

    printk(KERN_INFO "Check us->fflags[position-1]: %lx  dflags: %lx\n", us->fflags, us->dflags);

    /**
     * (1) Init mutex for kthread, for push_dev
     * The 2 threads are:
     * kthread started in stv01_kthread.c,
     * kernel called thread from USB-BUG
    */
	mutex_init(&(us->dev_mutex));

    /**
     * (2) init completion for 
     */
	init_completion(&us->cmnd_ready);
	init_completion(&(us->notify));

	init_waitqueue_head(&us->delay_wait);

	INIT_DELAYED_WORK(&us->scan_dwork, usb_stor_scan_dwork);

	/* Associate the stv01_usb_data_s structure with the USB device */
	result = associate_dev(us, intf);
	if (result)
		goto BadDevice;

	/* Get the entries and the descriptors */
    /* Get standard transport and protocol settings */
    /* Commented by laogong, inside get_info, fflag be set to id->driver_info = 8, */
    IPTR(dim)->get_info(us, id);
    IPTR(dim)->get_protocol(us);
	IPTR(dim)->get_transport(us);
    printk(KERN_INFO "Check us->fflags[position-2]: %lx  dflags: %lx\n", us->fflags, us->dflags);
    //utils_device_dbg(us, "Check us->fflags[position-2]: %x  dflags: %x\n", us->fflags, us->dflags);
	/* Give the caller a chance to fill in specialized transport
	 * or protocol settings.
	 */
	return 0;

BadDevice:
	utils_device_dbg(us, "storage_probe() failed\n");
	release_everything(us);
	return result;
}

/* Second part of general USB mass-storage probing */
int usb_stor_probe2(struct stv01_usb_data_s *us)
{
	int result;
	struct device *dev = &us->pusb_intf->dev;

	//utils_device_dbg(us, "Check us->fflags[position-3]: %x  dflags: %x\n", us->fflags, us->dflags);
	/* In the normal case there is only a single target */
	us_to_host(us)->max_id = 1;

    us_to_host(us)->no_scsi2_lun_in_cdb = 1;
	
    printk(KERN_INFO "Check us->fflags[position-3]: %lx  dflags: %lx\n", us->fflags, us->dflags);

	/* Find the endpoints and calculate pipe values */
	result = IPTR(dim)->get_pipes(us);
	if (result)
		goto BadDevice;

	/*
	 * If the device returns invalid data for the first READ(10)
	 * command, indicate the command should be retried.
	 */
	if (us->fflags & US_FL_INITIAL_READ10)
		set_bit(US_FLIDX_REDO_READ10, &us->dflags);

	/* Acquire all the other resources and add the host */
	result = stv01_usb_kthread_run(us);
	if (result)
		goto BadDevice;
	snprintf(us->scsi_name, sizeof(us->scsi_name), "usb-storage %s",
					dev_name(&us->pusb_intf->dev));

    utils_device_dbg(us, "us->scsi_name: %s\n", us->scsi_name);

	result = scsi_add_host(us_to_host(us), dev);
	if (result) {
		dev_warn(dev,
				"Unable to add the scsi host\n");
		goto BadDevice;
	}

	/* Submit the delayed_work for SCSI-device scanning */
	usb_autopm_get_interface_no_resume(us->pusb_intf);
	set_bit(US_FLIDX_SCAN_PENDING, &us->dflags);

	if (delay_use > 0)
		dev_dbg(dev, "waiting for device to settle before scanning\n");
	queue_delayed_work(system_freezable_wq, &us->scan_dwork,
			delay_use * HZ);
	return 0;

	/* We come here if there are any problems */
BadDevice:
	utils_device_dbg(us, "storage_probe() failed\n");
	release_everything(us);
	return result;
}

/* The main probe routine for standard devices */
static int storage_probe(struct usb_interface *intf,
			 const struct usb_device_id *id)
{
	struct stv01_usb_data_s *us;
	int result;

    printk(KERN_INFO " storage_probe \n");

	/* If uas is enabled and this device can do uas then ignore it. */
#if IS_ENABLED(CONFIG_USB_UAS) && 0
	if (uas_use_uas_driver(intf, id, NULL))
		return -ENXIO;
#endif

    printk(KERN_INFO " ===================1 \n");
    
	result = usb_stor_probe1(&us, intf, id, &usb_stor_host_template);
    printk(KERN_INFO " ===================2 \n");

	if (result)
		return result;

	/* No special transport or protocol settings in the main module */

	result = usb_stor_probe2(us);
	return result;
}

static stv01_usb_storage_method_t s_usb_storage_method = 
{
    .probe = storage_probe,
    .suspend = usb_storage_suspend,
    .resume = usb_storage_resume,
    .disconnect = usb_storage_disconnect,
    .reset_resume = usb_storage_reset_resume,
    .pre_reset = usb_storage_pre_reset,
    .post_reset = usb_storage_post_reset
};

EXPORT_PTR_V(stv01_usb_storage_method_t, usm, &s_usb_storage_method)


