#ifndef _STV01_UTILS_H_
#define _STV01_UTILS_H_

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/module.h>

#include <linux/usb.h>
#include <linux/usb_usual.h>
#include <linux/usb/quirks.h>

#define EXPORT
#define INTERVAL_IMPL
#define TOOLS_API

#define EXPORT_PTR(type, id) \
                type *g_global_##id;

#define EXPORT_PTR_V(type, id, v) \
                type *g_global_##id = v;

#define EXTERN_PTR(type, id) \
                extern type *g_global_##id;

#define IMPORT_PTR(id) \
                g_global_##id

#define IPTR(id) IMPORT_PTR(id)

#if 0
static inline TOOLS_API void utils_device_dbg(const struct device *dev, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);

	dev_vprintk_emit(LOGLEVEL_DEBUG, dev, fmt, args);

	va_end(args);
}
#endif

/*
 USB Definitions
*/
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

#define USB_STOR_STRING_LEN 32

#define USB_STOR_XFER_GOOD	0	/* good transfer                 */
#define USB_STOR_XFER_SHORT	1	/* transferred less than expected */
#define USB_STOR_XFER_STALLED	2	/* endpoint stalled              */
#define USB_STOR_XFER_LONG	3	/* device tried to send too much */
#define USB_STOR_XFER_ERROR	4	/* transfer died in the middle   */
#define USB_STOR_TRANSPORT_GOOD	   0   /* Transport good, command good	   */
#define USB_STOR_TRANSPORT_FAILED  1   /* Transport good, command failed   */
#define USB_STOR_TRANSPORT_NO_SENSE 2  /* Command failed, no auto-sense    */
#define USB_STOR_TRANSPORT_ERROR   3   /* Transport bad (i.e. device dead) */

#define US_IOBUF_SIZE		64	/* Size of the DMA-mapped I/O buffer */
#define US_SENSE_SIZE		18	/* Size of the autosense data buffer */

#define US_CBI_ADSC		0

/* Vendor IDs */
#define VENDOR_ID_NOKIA		0x0421
#define VENDOR_ID_NIKON		0x04b0
#define VENDOR_ID_PENTAX	0x0a17
#define VENDOR_ID_MOTOROLA	0x22b8

/* Works only for digits and letters, but small and fast */
#define TOLOWER(x) ((x) | 0x20)

#define CB_RESET_CMD_SIZE	12

#endif /// _STV01_UTILS_H_
