#include "stv01_utils.h"
#include "stv01_usb_data.h"

/*
 * Interpret the results of a URB transfer
 *
 * This function prints appropriate debugging messages, clears halts on
 * non-control endpoints, and translates the status to the corresponding
 * USB_STOR_XFER_xxx return code.
 */
int interpret_urb_result(struct stv01_usb_data_s *us, unsigned int pipe,
		unsigned int length, int result, unsigned int partial, int (*clear_halt)(struct stv01_usb_data_s *, unsigned int))
{
	utils_device_dbg(us, "Status code %d; transferred %u/%u\n",
		     result, partial, length);
	switch (result) {

	/* no error code; did we send all the data? */
	case 0:
		if (partial != length) {
			utils_device_dbg(us, "-- short transfer\n");
			return USB_STOR_XFER_SHORT;
		}

		utils_device_dbg(us, "-- transfer complete\n");
		return USB_STOR_XFER_GOOD;

	/* stalled */
	case -EPIPE:
		/* for control endpoints, (used by CB[I]) a stall indicates
		 * a failed command */
		if (usb_pipecontrol(pipe)) {
			utils_device_dbg(us, "-- stall on control pipe\n");
			return USB_STOR_XFER_STALLED;
		}

		/* for other sorts of endpoint, clear the stall */
		utils_device_dbg(us, "clearing endpoint halt for pipe 0x%x\n",
			     pipe);
		if (clear_halt(us, pipe) < 0)
			return USB_STOR_XFER_ERROR;
		return USB_STOR_XFER_STALLED;

	/* babble - the device tried to send more than we wanted to read */
	case -EOVERFLOW:
		utils_device_dbg(us, "-- babble\n");
		return USB_STOR_XFER_LONG;

	/* the transfer was cancelled by abort, disconnect, or timeout */
	case -ECONNRESET:
		utils_device_dbg(us, "-- transfer cancelled\n");
		return USB_STOR_XFER_ERROR;

	/* short scatter-gather read transfer */
	case -EREMOTEIO:
		utils_device_dbg(us, "-- short read transfer\n");
		return USB_STOR_XFER_SHORT;

	/* abort or disconnect in progress */
	case -EIO:
		utils_device_dbg(us, "-- abort or disconnect in progress\n");
		return USB_STOR_XFER_ERROR;

	/* the catch-all error case */
	default:
		utils_device_dbg(us, "-- unknown error\n");
		return USB_STOR_XFER_ERROR;
	}
}

void usb_stor_blocking_completion(struct urb *urb)
{
	struct completion *urb_done_ptr = urb->context;

	complete(urb_done_ptr);
}


