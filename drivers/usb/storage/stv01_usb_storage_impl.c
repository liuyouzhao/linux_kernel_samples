#include <linux/device.h>
#include <linux/kthread.h>
#include <linux/usb.h>

#include <scsi/scsi.h>
#include <scsi/scsi_eh.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_devinfo.h>

#include "stv01_utils.h"
#include "stv01_usb_data.h"
#include "stv01_scsi_api.h"

/* Dynamic bitflag definitions (us->dflags): used in set_bit() etc. */
#define US_FLIDX_URB_ACTIVE	0	/* current_urb is in use    */
#define US_FLIDX_SG_ACTIVE	1	/* current_sg is in use     */
#define US_FLIDX_ABORTING	2	/* abort is in progress     */
#define US_FLIDX_DISCONNECTING	3	/* disconnect in progress   */
#define US_FLIDX_RESETTING	4	/* device reset in progress */
#define US_FLIDX_TIMED_OUT	5	/* SCSI midlayer timed out  */
#define US_FLIDX_SCAN_PENDING	6	/* scanning not yet done    */
#define US_FLIDX_REDO_READ10	7	/* redo READ(10) command    */
#define US_FLIDX_READ10_WORKED	8	/* previous READ(10) succeeded */

/*
 * We provide a DMA-mapped I/O buffer for use with small USB transfers.
 * It turns out that CB[I] needs a 12-byte buffer and Bulk-only needs a
 * 31-byte buffer.  But Freecom needs a 64-byte buffer, so that's the
 * size we'll allocate.
 */
#define US_IOBUF_SIZE		64	/* Size of the DMA-mapped I/O buffer */
#define US_SENSE_SIZE		18	/* Size of the autosense data buffer */

/* First stage of disconnect processing: stop SCSI scanning,
 * remove the host, and stop accepting new commands
 */
void INTERVAL_IMPL __usb_storage_disconnect_impl(struct stv01_usb_data_s *us)
{
    /**
     * (1) Stop scsi scanning, scan_dwork defined in stv01_usb_data_s
     *     called INIT_DELAYED_WORK bind with usb_stor_scan_dwork
     */
	struct Scsi_Host *host = us_to_host(us);

	/* If the device is really gone, cut short reset delays */
	if (us->pusb_dev->state == USB_STATE_NOTATTACHED) {
		set_bit(US_FLIDX_DISCONNECTING, &us->dflags);
		wake_up(&us->delay_wait);
	}

	/* Prevent SCSI scanning (if it hasn't started yet)
	 * or wait for the SCSI-scanning routine to stop.
	 */
	cancel_delayed_work_sync(&us->scan_dwork);

	/* Balance autopm calls if scanning was cancelled */
	if (test_bit(US_FLIDX_SCAN_PENDING, &us->dflags))
		usb_autopm_put_interface_no_suspend(us->pusb_intf);

	/* Removing the host will perform an orderly shutdown: caches
	 * synchronized, disks spun down, etc.
	 */
	scsi_remove_host(host);

	/* Prevent any new commands from being accepted and cut short
	 * reset delays.
	 */
	scsi_lock(host);
	set_bit(US_FLIDX_DISCONNECTING, &us->dflags);
	scsi_unlock(host);
	wake_up(&us->delay_wait);
}

/* Report a driver-initiated bus reset to the SCSI layer.
 * Calling this for a SCSI-initiated reset is unnecessary but harmless.
 * The caller must not own the SCSI host lock. */
void INTERVAL_IMPL __usb_storage_report_bus_reset(struct stv01_usb_data_s *us)
{
	struct Scsi_Host *host = us_to_host(us);

	scsi_lock(host);
	scsi_report_bus_reset(host, 0);
	scsi_unlock(host);
}
