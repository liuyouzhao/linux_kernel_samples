#include <scsi/scsi.h>
#include <scsi/scsi_eh.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_devinfo.h>

#include <linux/usb.h>
#include <linux/usb_usual.h>
#include <linux/usb/quirks.h>

#include <linux/kthread.h>
#include <linux/mutex.h>

#include "stv01_utils.h"
#include "stv01_usb_data.h"
#include "stv01_scsi_api.h"
#include "stv01_usb_transport.h"
#include "stv01_usb_device.h"

static void usb_stor_set_xfer_buf(unsigned char *buffer,
	unsigned int buflen, struct scsi_cmnd *srb)
{
	unsigned int offset = 0;
	struct scatterlist *sg = NULL;

	buflen = min(buflen, scsi_bufflen(srb));
	buflen = usb_stor_access_xfer_buf(buffer, buflen, srb, &sg, &offset, TO_XFER_BUF);
	if (buflen < scsi_bufflen(srb))
		scsi_set_resid(srb, scsi_bufflen(srb) - buflen);
}

static void fill_inquiry_response(struct stv01_usb_data_s *us, unsigned char *data,
		unsigned int data_len)
{
	if (data_len < 36) /* You lose. */
		return;

	memset(data+8, ' ', 28);
	if (data[0]&0x20) { /* USB device currently not connected. Return
			      peripheral qualifier 001b ("...however, the
			      physical device is not currently connected
			      to this logical unit") and leave vendor and
			      product identification empty. ("If the target
			      does store some of the INQUIRY data on the
			      device, it may return zeros or ASCII spaces
			      (20h) in those fields until the data is
			      available from the device."). */
	} else {
		u16 bcdDevice = le16_to_cpu(us->pusb_dev->descriptor.bcdDevice);
		int n;

        utils_device_dbg(us, "Check if need my faking giving\n");
        if(us->unusual_dev == NULL || us->unusual_dev->vendorName == NULL)
        {
            utils_device_dbg(us, "Fake needed\n");
            memcpy(data+8, "steven", 6);
            memcpy(data+16, "stv01", 5);
        }
        else
        {
            utils_device_dbg(us, "No fake needed\n");
		    n = strlen(us->unusual_dev->vendorName);
		    memcpy(data+8, us->unusual_dev->vendorName, min(8, n));
		    n = strlen(us->unusual_dev->productName);
		    memcpy(data+16, us->unusual_dev->productName, min(16, n));
        }
		data[32] = 0x30 + ((bcdDevice>>12) & 0x0F);
		data[33] = 0x30 + ((bcdDevice>>8) & 0x0F);
		data[34] = 0x30 + ((bcdDevice>>4) & 0x0F);
		data[35] = 0x30 + ((bcdDevice) & 0x0F);
	}

	usb_stor_set_xfer_buf(data, data_len, us->srb);
}


int stv01_kernel_thread(void * __us)
{
	struct stv01_usb_data_s *us = (struct stv01_usb_data_s *)__us;
	struct Scsi_Host *host = us_to_host(us);

	for (;;) {
		utils_device_dbg(us, "*** thread sleeping\n");
		if (wait_for_completion_interruptible(&us->cmnd_ready))
			break;

		utils_device_dbg(us, "*** thread awakened\n");

		/* lock the device pointers */
		mutex_lock(&(us->dev_mutex));

		/* lock access to the state */
		scsi_lock(host);

		/* When we are called with no command pending, we're done */
		if (us->srb == NULL) {
			scsi_unlock(host);
			mutex_unlock(&us->dev_mutex);
			utils_device_dbg(us, "-- exiting\n");
			break;
		}

		/* has the command timed out *already* ? */
		if (test_bit(US_FLIDX_TIMED_OUT, &us->dflags)) {
			us->srb->result = DID_ABORT << 16;
			goto SkipForAbort;
		}

		scsi_unlock(host);

		/* reject the command if the direction indicator
		 * is UNKNOWN
		 */
		if (us->srb->sc_data_direction == DMA_BIDIRECTIONAL) {
			utils_device_dbg(us, "UNKNOWN data direction\n");
			us->srb->result = DID_ERROR << 16;
		}

		/* reject if target != 0 or if LUN is higher than
		 * the maximum known LUN
		 */
		else if (us->srb->device->id &&
				!(us->fflags & US_FL_SCM_MULT_TARG)) {
			utils_device_dbg(us, "Bad target number (%d:%llu)\n",
				     us->srb->device->id,
				     us->srb->device->lun);
			us->srb->result = DID_BAD_TARGET << 16;
		}

		else if (us->srb->device->lun > us->max_lun) {
			utils_device_dbg(us, "Bad LUN (%d:%llu)\n",
				     us->srb->device->id,
				     us->srb->device->lun);
			us->srb->result = DID_BAD_TARGET << 16;
		}

		/* Handle those devices which need us to fake
		 * their inquiry data */
		else if ((us->srb->cmnd[0] == INQUIRY) &&
			    (us->fflags & US_FL_FIX_INQUIRY)) {
			unsigned char data_ptr[36] = {
			    0x00, 0x80, 0x02, 0x02,
			    0x1F, 0x00, 0x00, 0x00};

			utils_device_dbg(us, "Faking INQUIRY command\n");
			fill_inquiry_response(us, data_ptr, 36);
			us->srb->result = SAM_STAT_GOOD;
		}

		/* we've got a command, let's do it! */
		else {
			us->protocol_command(us->srb, us);
			usb_mark_last_busy(us->pusb_dev);
		}

		/* lock access to the state */
		scsi_lock(host);

		/* indicate that the command is done */
		if (us->srb->result != DID_ABORT << 16) {
			utils_device_dbg(us, "scsi cmd done, result=0x%x\n",
				     us->srb->result);
			us->srb->scsi_done(us->srb);
		} else {
SkipForAbort:
			utils_device_dbg(us, "scsi command aborted\n");
		}

		/* If an abort request was received we need to signal that
		 * the abort has finished.  The proper test for this is
		 * the TIMED_OUT flag, not srb->result == DID_ABORT, because
		 * the timeout might have occurred after the command had
		 * already completed with a different result code. */
		if (test_bit(US_FLIDX_TIMED_OUT, &us->dflags)) {
			complete(&(us->notify));

			/* Allow USB transfers to resume */
			clear_bit(US_FLIDX_ABORTING, &us->dflags);
			clear_bit(US_FLIDX_TIMED_OUT, &us->dflags);
		}

		/* finished working on this command */
		us->srb = NULL;
		scsi_unlock(host);

		/* unlock the device pointers */
		mutex_unlock(&us->dev_mutex);
	} /* for (;;) */

	/* Wait until we are told to stop */
	for (;;) {
		set_current_state(TASK_INTERRUPTIBLE);
		if (kthread_should_stop())
			break;
		schedule();
	}
	__set_current_state(TASK_RUNNING);
	return 0;
}
