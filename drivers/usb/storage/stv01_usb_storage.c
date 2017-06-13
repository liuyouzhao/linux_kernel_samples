#include "stv01_usb_storage.h"
#include "stv01_usb_data.h"
#include "stv01_utils.h"

#include <linux/mutex.h>

#define US_SUSPEND	0
#define US_RESUME	1

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

/* Handle a USB mass-storage disconnect */
static void usb_storage_disconnect(struct usb_interface *intf)
{
	struct stv01_usb_data_s *us = usb_get_intfdata(intf);

    __usb_storage_disconnect_impl(us);
	//quiesce_and_remove_host(us);
	//release_everything(us);
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

static stv01_usb_storage_method_t s_usb_storage_method = 
{
    .suspend = usb_storage_suspend,
    .resume = usb_storage_resume,
    .disconnect = usb_storage_disconnect,
    .reset_resume = usb_storage_reset_resume,
    .pre_reset = usb_storage_pre_reset,
    .post_reset = usb_storage_post_reset
};

EXPORT_PTR_V(stv01_usb_storage_method_t, usm, &s_usb_storage_method)


