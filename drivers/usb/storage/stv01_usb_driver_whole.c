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

#define USUAL_DEV(use_protocol, use_transport) \
{ \
	.useProtocol = use_protocol,	\
	.useTransport = use_transport,	\
}
static struct us_unusual_dev us_unusual_dev_list[] = {
    /* Control/Bulk transport for all SubClass values */
    USUAL_DEV(USB_SC_RBC, USB_PR_CB),
    USUAL_DEV(USB_SC_8020, USB_PR_CB),
    USUAL_DEV(USB_SC_QIC, USB_PR_CB),
    USUAL_DEV(USB_SC_UFI, USB_PR_CB),
    USUAL_DEV(USB_SC_8070, USB_PR_CB),
    USUAL_DEV(USB_SC_SCSI, USB_PR_CB),

    /* Control/Bulk/Interrupt transport for all SubClass values */
    USUAL_DEV(USB_SC_RBC, USB_PR_CBI),
    USUAL_DEV(USB_SC_8020, USB_PR_CBI),
    USUAL_DEV(USB_SC_QIC, USB_PR_CBI),
    USUAL_DEV(USB_SC_UFI, USB_PR_CBI),
    USUAL_DEV(USB_SC_8070, USB_PR_CBI),
    USUAL_DEV(USB_SC_SCSI, USB_PR_CBI),

    /* Bulk-only transport for all SubClass values */
    USUAL_DEV(USB_SC_RBC, USB_PR_BULK),
    USUAL_DEV(USB_SC_8020, USB_PR_BULK),
    USUAL_DEV(USB_SC_QIC, USB_PR_BULK),
    USUAL_DEV(USB_SC_UFI, USB_PR_BULK),
    USUAL_DEV(USB_SC_8070, USB_PR_BULK),
    USUAL_DEV(USB_SC_SCSI, USB_PR_BULK),
	{ }		/* Terminating entry */
};

#undef USUAL_DEV
#define USUAL_DEV(useProto, useTrans) \
{ USB_INTERFACE_INFO(USB_CLASS_MASS_STORAGE, useProto, useTrans) }

struct usb_device_id usb_storage_usb_ids[] = {
     /* Control/Bulk transport for all SubClass values */
    USUAL_DEV(USB_SC_RBC, USB_PR_CB),
    USUAL_DEV(USB_SC_8020, USB_PR_CB),
    USUAL_DEV(USB_SC_QIC, USB_PR_CB),
    USUAL_DEV(USB_SC_UFI, USB_PR_CB),
    USUAL_DEV(USB_SC_8070, USB_PR_CB),
    USUAL_DEV(USB_SC_SCSI, USB_PR_CB),

    /* Control/Bulk/Interrupt transport for all SubClass values */
    USUAL_DEV(USB_SC_RBC, USB_PR_CBI),
    USUAL_DEV(USB_SC_8020, USB_PR_CBI),
    USUAL_DEV(USB_SC_QIC, USB_PR_CBI),
    USUAL_DEV(USB_SC_UFI, USB_PR_CBI),
    USUAL_DEV(USB_SC_8070, USB_PR_CBI),
    USUAL_DEV(USB_SC_SCSI, USB_PR_CBI),

    /* Bulk-only transport for all SubClass values */
    USUAL_DEV(USB_SC_RBC, USB_PR_BULK),
    USUAL_DEV(USB_SC_8020, USB_PR_BULK),
    USUAL_DEV(USB_SC_QIC, USB_PR_BULK),
    USUAL_DEV(USB_SC_UFI, USB_PR_BULK),
    USUAL_DEV(USB_SC_8070, USB_PR_BULK),
    USUAL_DEV(USB_SC_SCSI, USB_PR_BULK),
	{ }		/* Terminating entry */
};

#undef USUAL_DEV

#define USUAL_DEV(use_protocol, use_transport) \
{ \
	.useProtocol = use_protocol,	\
	.useTransport = use_transport,	\
}
static struct us_unusual_dev for_dynamic_ids = USUAL_DEV(USB_SC_SCSI, USB_PR_BULK);

struct scsi_disk {
        struct scsi_driver *driver;     /* always &sd_template */
        struct scsi_device *device;
        struct device   dev;
        struct gendisk  *disk;
        atomic_t        openers;
        sector_t        capacity;       /* size in 512-byte sectors */
        u32             max_xfer_blocks;
        u32             max_ws_blocks;
        u32             max_unmap_blocks;
        u32             unmap_granularity;
        u32             unmap_alignment;
        u32             index;
        unsigned int    physical_block_size;
        unsigned int    max_medium_access_timeouts;
        unsigned int    medium_access_timed_out;
        u8              media_present;
        u8              write_prot;
        u8              protection_type;/* Data Integrity Field */
        u8              provisioning_mode;
        unsigned        ATO : 1;        /* state of disk ATO bit */
        unsigned        cache_override : 1; /* temp override of WCE,RCD */
        unsigned        WCE : 1;        /* state of disk WCE bit */
        unsigned        RCD : 1;        /* state of disk RCD bit, unused */
        unsigned        DPOFUA : 1;     /* state of disk DPOFUA bit */
        unsigned        first_scan : 1;
        unsigned        lbpme : 1;
        unsigned        lbprz : 1;
        unsigned        lbpu : 1;
        unsigned        lbpws : 1;
        unsigned        lbpws10 : 1;
        unsigned        lbpvpd : 1;
        unsigned        ws10 : 1;
        unsigned        ws16 : 1;
};

static char quirks[128];
module_param_string(quirks, quirks, sizeof(quirks), S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(quirks, "supplemental list of device IDs and their quirks");

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Hujia Liu <liuyouzhao@hotmail.com>");
MODULE_DESCRIPTION("USB STV01 Linux Course Practising Registration Driver");
#define DRV_NAME "stv01-driver"
#define USB_STORAGE "usb-storage: "
/*
 * The table of devices
 */
#define UNUSUAL_DEV(id_vendor, id_product, bcdDeviceMin, bcdDeviceMax, \
		    vendorName, productName, useProtocol, useTransport, \
		    initFunction, flags) \
{ USB_DEVICE_VER(id_vendor, id_product, bcdDeviceMin, bcdDeviceMax), \
  .driver_info = (flags) }

static struct usb_device_id stv01_usb_ids[] = {
    UNUSUAL_DEV( 0x125f, 0x317a, 0x0000, 0x9999, "steven", "stv01",
		USB_SC_SCSI, USB_PR_SDDR55, NULL,
		US_FL_FIX_INQUIRY),
	{ }		/* Terminating entry */
};
MODULE_DEVICE_TABLE(usb, stv01_usb_ids);

#undef UNUSUAL_DEV

/*
 * The flags table
 */
#define UNUSUAL_DEV(idVendor, idProduct, bcdDeviceMin, bcdDeviceMax, \
		    vendor_name, product_name, use_protocol, use_transport, \
		    init_function, Flags) \
{ \
	.vendorName = vendor_name,	\
	.productName = product_name,	\
	.useProtocol = use_protocol,	\
	.useTransport = use_transport,	\
	.initFunction = init_function,	\
}

#undef UNUSUAL_DEV
#define US_CBI_ADSC		0
#define USB_STOR_XFER_GOOD	0	/* good transfer                 */
#define USB_STOR_XFER_SHORT	1	/* transferred less than expected */
#define USB_STOR_XFER_STALLED	2	/* endpoint stalled              */
#define USB_STOR_XFER_LONG	3	/* device tried to send too much */
#define USB_STOR_XFER_ERROR	4	/* transfer died in the middle   */
#define USB_STOR_TRANSPORT_GOOD	   0   /* Transport good, command good	   */
#define USB_STOR_TRANSPORT_FAILED  1   /* Transport good, command failed   */
#define USB_STOR_TRANSPORT_NO_SENSE 2  /* Command failed, no auto-sense    */
#define USB_STOR_TRANSPORT_ERROR   3   /* Transport bad (i.e. device dead) */
#define US_CBI_ADSC		0

/* Vendor IDs */
#define VENDOR_ID_NOKIA		0x0421
#define VENDOR_ID_NIKON		0x04b0
#define VENDOR_ID_PENTAX	0x0a17
#define VENDOR_ID_MOTOROLA	0x22b8

/* Works only for digits and letters, but small and fast */
#define TOLOWER(x) ((x) | 0x20)

#define CB_RESET_CMD_SIZE	12

static unsigned int delay_use = 1;
unsigned char usb_stor_sense_invalidCDB[18] = {
	[0]	= 0x70,			    /* current error */
	[2]	= ILLEGAL_REQUEST,	    /* Illegal Request = 0x05 */
	[7]	= 0x0a,			    /* additional length */
	[12]	= 0x24			    /* Invalid Field in CDB */
};

/* struct scsi_cmnd transfer buffer access utilities */
enum xfer_buf_dir	{TO_XFER_BUF, FROM_XFER_BUF};

/**
Global vars
*/
static struct scsi_host_template stv01_host_template;

static int interpret_urb_result(struct us_data *us, unsigned int pipe,
		unsigned int length, int result, unsigned int partial);
		
static int usb_stor_control_msg(struct us_data *us, unsigned int pipe,
		                         u8 request, u8 requesttype, u16 value, u16 index, 
		                         void *data, u16 size, int timeout);
static int usb_stor_bulk_transfer_buf(struct us_data *us, unsigned int pipe,
	void *buf, unsigned int length, unsigned int *act_len);
static void usb_stor_invoke_transport(struct scsi_cmnd *srb, struct us_data *us);
static void usb_stor_ufi_command(struct scsi_cmnd *srb, struct us_data *us);
static int get_pipes(struct us_data *us);
static int usb_stor_acquire_resources(struct us_data *us);

static void usb_stor_dbg(const struct us_data *us, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);

	dev_vprintk_emit(LOGLEVEL_DEBUG, &us->pusb_dev->dev, fmt, args);

	va_end(args);
}

static unsigned int usb_stor_sg_tablesize(struct usb_interface *intf)
{
	struct usb_device *usb_dev = interface_to_usbdev(intf);

	if (usb_dev->bus->sg_tablesize) {
		return usb_dev->bus->sg_tablesize;
	}
	return SG_ALL;
}

static void us_set_lock_class(struct mutex *mutex,
		struct usb_interface *intf)
{
}

static void usb_stor_blocking_completion(struct urb *urb)
{
	struct completion *urb_done_ptr = urb->context;

	complete(urb_done_ptr);
}

static int usb_stor_msg_common(struct us_data *us, int timeout)
{
	struct completion urb_done;
	long timeleft;
	int status;

	/* don't submit URBs during abort processing */
	if (test_bit(US_FLIDX_ABORTING, &us->dflags))
		return -EIO;

	/* set up data structures for the wakeup system */
	init_completion(&urb_done);

	/* fill the common fields in the URB */
	us->current_urb->context = &urb_done;
	us->current_urb->transfer_flags = 0;

	/* we assume that if transfer_buffer isn't us->iobuf then it
	 * hasn't been mapped for DMA.  Yes, this is clunky, but it's
	 * easier than always having the caller tell us whether the
	 * transfer buffer has already been mapped. */
	if (us->current_urb->transfer_buffer == us->iobuf)
		us->current_urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
	us->current_urb->transfer_dma = us->iobuf_dma;

	/* submit the URB */
	status = usb_submit_urb(us->current_urb, GFP_NOIO);
	if (status) {
		/* something went wrong */
		return status;
	}

	/* since the URB has been submitted successfully, it's now okay
	 * to cancel it */
	set_bit(US_FLIDX_URB_ACTIVE, &us->dflags);

	/* did an abort occur during the submission? */
	if (test_bit(US_FLIDX_ABORTING, &us->dflags)) {

		/* cancel the URB, if it hasn't been cancelled already */
		if (test_and_clear_bit(US_FLIDX_URB_ACTIVE, &us->dflags)) {
			usb_stor_dbg(us, "-- cancelling URB\n");
			usb_unlink_urb(us->current_urb);
		}
	}
 
	/* wait for the completion of the URB */
	timeleft = wait_for_completion_interruptible_timeout(
			&urb_done, timeout ? : MAX_SCHEDULE_TIMEOUT);
 
	clear_bit(US_FLIDX_URB_ACTIVE, &us->dflags);

	if (timeleft <= 0) {
		usb_stor_dbg(us, "%s -- cancelling URB\n",
			     timeleft == 0 ? "Timeout" : "Signal");
		usb_kill_urb(us->current_urb);
	}

	/* return the URB status */
	return us->current_urb->status;
}

static int usb_stor_ctrl_transfer(struct us_data *us, unsigned int pipe,
		u8 request, u8 requesttype, u16 value, u16 index,
		void *data, u16 size)
{
	int result;

	usb_stor_dbg(us, "rq=%02x rqtype=%02x value=%04x index=%02x len=%u\n",
		     request, requesttype, value, index, size);

	/* fill in the devrequest structure */
	us->cr->bRequestType = requesttype;
	us->cr->bRequest = request;
	us->cr->wValue = cpu_to_le16(value);
	us->cr->wIndex = cpu_to_le16(index);
	us->cr->wLength = cpu_to_le16(size);

	/* fill and submit the URB */
	usb_fill_control_urb(us->current_urb, us->pusb_dev, pipe, 
			 (unsigned char*) us->cr, data, size, 
			 usb_stor_blocking_completion, NULL);
	result = usb_stor_msg_common(us, 0);

	return interpret_urb_result(us, pipe, size, result,
			us->current_urb->actual_length);
}

/*
 * Transfer a scatter-gather list via bulk transfer
 *
 * This function does basically the same thing as usb_stor_bulk_transfer_buf()
 * above, but it uses the usbcore scatter-gather library.
 */
static int usb_stor_bulk_transfer_sglist(struct us_data *us, unsigned int pipe,
		struct scatterlist *sg, int num_sg, unsigned int length,
		unsigned int *act_len)
{
	int result;

	/* don't submit s-g requests during abort processing */
	if (test_bit(US_FLIDX_ABORTING, &us->dflags))
		return USB_STOR_XFER_ERROR;

	/* initialize the scatter-gather request block */
	usb_stor_dbg(us, "xfer %u bytes, %d entries\n", length, num_sg);
	result = usb_sg_init(&us->current_sg, us->pusb_dev, pipe, 0,
			sg, num_sg, length, GFP_NOIO);
	if (result) {
		usb_stor_dbg(us, "usb_sg_init returned %d\n", result);
		return USB_STOR_XFER_ERROR;
	}

	/* since the block has been initialized successfully, it's now
	 * okay to cancel it */
	set_bit(US_FLIDX_SG_ACTIVE, &us->dflags);

	/* did an abort occur during the submission? */
	if (test_bit(US_FLIDX_ABORTING, &us->dflags)) {

		/* cancel the request, if it hasn't been cancelled already */
		if (test_and_clear_bit(US_FLIDX_SG_ACTIVE, &us->dflags)) {
			usb_stor_dbg(us, "-- cancelling sg request\n");
			usb_sg_cancel(&us->current_sg);
		}
	}

	/* wait for the completion of the transfer */
	usb_sg_wait(&us->current_sg);
	clear_bit(US_FLIDX_SG_ACTIVE, &us->dflags);

	result = us->current_sg.status;
	if (act_len)
		*act_len = us->current_sg.bytes;
	return interpret_urb_result(us, pipe, length, result,
			us->current_sg.bytes);
}

int usb_stor_bulk_srb(struct us_data* us, unsigned int pipe,
		      struct scsi_cmnd* srb)
{
	unsigned int partial = 0;
	int result = usb_stor_bulk_transfer_sglist(us, pipe, scsi_sglist(srb),
				      scsi_sg_count(srb), scsi_bufflen(srb),
				      &partial);

	scsi_set_resid(srb, scsi_bufflen(srb) - partial);
	return result;
}

static int usb_stor_intr_transfer(struct us_data *us, void *buf,
				  unsigned int length)
{
	int result;
	unsigned int pipe = us->recv_intr_pipe;
	unsigned int maxp;

	usb_stor_dbg(us, "xfer %u bytes\n", length);

	/* calculate the max packet size */
	maxp = usb_maxpacket(us->pusb_dev, pipe, usb_pipeout(pipe));
	if (maxp > length)
		maxp = length;

	/* fill and submit the URB */
	usb_fill_int_urb(us->current_urb, us->pusb_dev, pipe, buf,
			maxp, usb_stor_blocking_completion, NULL,
			us->ep_bInterval);
	result = usb_stor_msg_common(us, 0);

	return interpret_urb_result(us, pipe, length, result,
			us->current_urb->actual_length);
}

static int usb_stor_clear_halt(struct us_data *us, unsigned int pipe)
{
	int result;
	int endp = usb_pipeendpoint(pipe);

	if (usb_pipein (pipe))
		endp |= USB_DIR_IN;

	result = usb_stor_control_msg(us, us->send_ctrl_pipe,
		USB_REQ_CLEAR_FEATURE, USB_RECIP_ENDPOINT,
		USB_ENDPOINT_HALT, endp,
		NULL, 0, 3*HZ);

	if (result >= 0)
		usb_reset_endpoint(us->pusb_dev, endp);

	usb_stor_dbg(us, "result = %d\n", result);
	return result;
}

static int usb_stor_CB_transport(struct scsi_cmnd *srb, struct us_data *us)
{
	unsigned int transfer_length = scsi_bufflen(srb);
	unsigned int pipe = 0;
	int result;

	/* COMMAND STAGE */
	/* let's send the command via the control pipe */
	result = usb_stor_ctrl_transfer(us, us->send_ctrl_pipe,
				      US_CBI_ADSC, 
				      USB_TYPE_CLASS | USB_RECIP_INTERFACE, 0, 
				      us->ifnum, srb->cmnd, srb->cmd_len);

	/* check the return code for the command */
	usb_stor_dbg(us, "Call to usb_stor_ctrl_transfer() returned %d\n",
		     result);

	/* if we stalled the command, it means command failed */
	if (result == USB_STOR_XFER_STALLED) {
		return USB_STOR_TRANSPORT_FAILED;
	}

	/* Uh oh... serious problem here */
	if (result != USB_STOR_XFER_GOOD) {
		return USB_STOR_TRANSPORT_ERROR;
	}

	/* DATA STAGE */
	/* transfer the data payload for this command, if one exists*/
	if (transfer_length) {
		pipe = srb->sc_data_direction == DMA_FROM_DEVICE ? 
				us->recv_bulk_pipe : us->send_bulk_pipe;
		result = usb_stor_bulk_srb(us, pipe, srb);
		usb_stor_dbg(us, "CBI data stage result is 0x%x\n", result);

		/* if we stalled the data transfer it means command failed */
		if (result == USB_STOR_XFER_STALLED)
			return USB_STOR_TRANSPORT_FAILED;
		if (result > USB_STOR_XFER_STALLED)
			return USB_STOR_TRANSPORT_ERROR;
	}

	/* STATUS STAGE */

	/* NOTE: CB does not have a status stage.  Silly, I know.  So
	 * we have to catch this at a higher level.
	 */
	if (us->protocol != USB_PR_CBI)
		return USB_STOR_TRANSPORT_GOOD;

	result = usb_stor_intr_transfer(us, us->iobuf, 2);
	usb_stor_dbg(us, "Got interrupt data (0x%x, 0x%x)\n",
		     us->iobuf[0], us->iobuf[1]);
	if (result != USB_STOR_XFER_GOOD)
		return USB_STOR_TRANSPORT_ERROR;

	/* UFI gives us ASC and ASCQ, like a request sense
	 *
	 * REQUEST_SENSE and INQUIRY don't affect the sense data on UFI
	 * devices, so we ignore the information for those commands.  Note
	 * that this means we could be ignoring a real error on these
	 * commands, but that can't be helped.
	 */
	if (us->subclass == USB_SC_UFI) {
		if (srb->cmnd[0] == REQUEST_SENSE ||
		    srb->cmnd[0] == INQUIRY)
			return USB_STOR_TRANSPORT_GOOD;
		if (us->iobuf[0])
			goto Failed;
		return USB_STOR_TRANSPORT_GOOD;
	}

	/* If not UFI, we interpret the data as a result code 
	 * The first byte should always be a 0x0.
	 *
	 * Some bogus devices don't follow that rule.  They stuff the ASC
	 * into the first byte -- so if it's non-zero, call it a failure.
	 */
	if (us->iobuf[0]) {
		usb_stor_dbg(us, "CBI IRQ data showed reserved bType 0x%x\n",
			     us->iobuf[0]);
		goto Failed;

	}

	/* The second byte & 0x0F should be 0x0 for good, otherwise error */
	switch (us->iobuf[1] & 0x0F) {
		case 0x00: 
			return USB_STOR_TRANSPORT_GOOD;
		case 0x01: 
			goto Failed;
	}
	return USB_STOR_TRANSPORT_ERROR;

	/* the CBI spec requires that the bulk pipe must be cleared
	 * following any data-in/out command failure (section 2.4.3.1.3)
	 */
  Failed:
	if (pipe)
		usb_stor_clear_halt(us, pipe);
	return USB_STOR_TRANSPORT_FAILED;
}

static int usb_stor_reset_common(struct us_data *us,
		u8 request, u8 requesttype,
		u16 value, u16 index, void *data, u16 size)
{
	int result;
	int result2;

	if (test_bit(US_FLIDX_DISCONNECTING, &us->dflags)) {
		usb_stor_dbg(us, "No reset during disconnect\n");
		return -EIO;
	}

	result = usb_stor_control_msg(us, us->send_ctrl_pipe,
			request, requesttype, value, index, data, size,
			5*HZ);
	if (result < 0) {
		usb_stor_dbg(us, "Soft reset failed: %d\n", result);
		return result;
	}

	/* Give the device some time to recover from the reset,
	 * but don't delay disconnect processing. */
	wait_event_interruptible_timeout(us->delay_wait,
			test_bit(US_FLIDX_DISCONNECTING, &us->dflags),
			HZ*6);
	if (test_bit(US_FLIDX_DISCONNECTING, &us->dflags)) {
		usb_stor_dbg(us, "Reset interrupted by disconnect\n");
		return -EIO;
	}

	usb_stor_dbg(us, "Soft reset: clearing bulk-in endpoint halt\n");
	result = usb_stor_clear_halt(us, us->recv_bulk_pipe);

	usb_stor_dbg(us, "Soft reset: clearing bulk-out endpoint halt\n");
	result2 = usb_stor_clear_halt(us, us->send_bulk_pipe);

	/* return a result code based on the result of the clear-halts */
	if (result >= 0)
		result = result2;
	if (result < 0)
		usb_stor_dbg(us, "Soft reset failed\n");
	else
		usb_stor_dbg(us, "Soft reset done\n");
	return result;
}

static int usb_stor_CB_reset(struct us_data *us)
{
	memset(us->iobuf, 0xFF, CB_RESET_CMD_SIZE);
	us->iobuf[0] = SEND_DIAGNOSTIC;
	us->iobuf[1] = 4;
	return usb_stor_reset_common(us, US_CBI_ADSC, 
				 USB_TYPE_CLASS | USB_RECIP_INTERFACE,
				 0, us->ifnum, us->iobuf, CB_RESET_CMD_SIZE);
}

static unsigned int usb_stor_access_xfer_buf(unsigned char *buffer,
	unsigned int buflen, struct scsi_cmnd *srb, struct scatterlist **sgptr,
	unsigned int *offset, enum xfer_buf_dir dir)
{
	unsigned int cnt = 0;
	struct scatterlist *sg = *sgptr;
	struct sg_mapping_iter miter;
	unsigned int nents = scsi_sg_count(srb);

	if (sg)
		nents = sg_nents(sg);
	else
		sg = scsi_sglist(srb);

	sg_miter_start(&miter, sg, nents, dir == FROM_XFER_BUF ?
		SG_MITER_FROM_SG: SG_MITER_TO_SG);

	if (!sg_miter_skip(&miter, *offset))
		return cnt;

	while (sg_miter_next(&miter) && cnt < buflen) {
		unsigned int len = min_t(unsigned int, miter.length,
				buflen - cnt);

		if (dir == FROM_XFER_BUF)
			memcpy(buffer + cnt, miter.addr, len);
		else
			memcpy(miter.addr, buffer + cnt, len);

		if (*offset + len < miter.piter.sg->length) {
			*offset += len;
			*sgptr = miter.piter.sg;
		} else {
			*offset = 0;
			*sgptr = sg_next(miter.piter.sg);
		}
		cnt += len;
	}
	sg_miter_stop(&miter);

	return cnt;
}

static int usb_stor_Bulk_transport(struct scsi_cmnd *srb, struct us_data *us)
{
	struct bulk_cb_wrap *bcb = (struct bulk_cb_wrap *) us->iobuf;
	struct bulk_cs_wrap *bcs = (struct bulk_cs_wrap *) us->iobuf;
	unsigned int transfer_length = scsi_bufflen(srb);
	unsigned int residue;
	int result;
	int fake_sense = 0;
	unsigned int cswlen;
	unsigned int cbwlen = US_BULK_CB_WRAP_LEN;

	/* Take care of BULK32 devices; set extra byte to 0 */
	if (unlikely(us->fflags & US_FL_BULK32)) {
		cbwlen = 32;
		us->iobuf[31] = 0;
	}

	/* set up the command wrapper */
	bcb->Signature = cpu_to_le32(US_BULK_CB_SIGN);
	bcb->DataTransferLength = cpu_to_le32(transfer_length);
	bcb->Flags = srb->sc_data_direction == DMA_FROM_DEVICE ?
		US_BULK_FLAG_IN : 0;
	bcb->Tag = ++us->tag;
	bcb->Lun = srb->device->lun;
	if (us->fflags & US_FL_SCM_MULT_TARG)
		bcb->Lun |= srb->device->id << 4;
	bcb->Length = srb->cmd_len;

	/* copy the command payload */
	memset(bcb->CDB, 0, sizeof(bcb->CDB));
	memcpy(bcb->CDB, srb->cmnd, bcb->Length);

	/* send it to out endpoint */
	usb_stor_dbg(us, "Bulk Command S 0x%x T 0x%x L %d F %d Trg %d LUN %d CL %d\n",
		     le32_to_cpu(bcb->Signature), bcb->Tag,
		     le32_to_cpu(bcb->DataTransferLength), bcb->Flags,
		     (bcb->Lun >> 4), (bcb->Lun & 0x0F),
		     bcb->Length);
	result = usb_stor_bulk_transfer_buf(us, us->send_bulk_pipe,
				bcb, cbwlen, NULL);
	usb_stor_dbg(us, "Bulk command transfer result=%d\n", result);
	if (result != USB_STOR_XFER_GOOD)
		return USB_STOR_TRANSPORT_ERROR;

	/* DATA STAGE */
	/* send/receive data payload, if there is any */

	/* Some USB-IDE converter chips need a 100us delay between the
	 * command phase and the data phase.  Some devices need a little
	 * more than that, probably because of clock rate inaccuracies. */
	if (unlikely(us->fflags & US_FL_GO_SLOW))
		udelay(125);

	if (transfer_length) {
		unsigned int pipe = srb->sc_data_direction == DMA_FROM_DEVICE ? 
				us->recv_bulk_pipe : us->send_bulk_pipe;
		result = usb_stor_bulk_srb(us, pipe, srb);
		usb_stor_dbg(us, "Bulk data transfer result 0x%x\n", result);
		if (result == USB_STOR_XFER_ERROR)
			return USB_STOR_TRANSPORT_ERROR;

		/* If the device tried to send back more data than the
		 * amount requested, the spec requires us to transfer
		 * the CSW anyway.  Since there's no point retrying the
		 * the command, we'll return fake sense data indicating
		 * Illegal Request, Invalid Field in CDB.
		 */
		if (result == USB_STOR_XFER_LONG)
			fake_sense = 1;

		/*
		 * Sometimes a device will mistakenly skip the data phase
		 * and go directly to the status phase without sending a
		 * zero-length packet.  If we get a 13-byte response here,
		 * check whether it really is a CSW.
		 */
		if (result == USB_STOR_XFER_SHORT &&
				srb->sc_data_direction == DMA_FROM_DEVICE &&
				transfer_length - scsi_get_resid(srb) ==
					US_BULK_CS_WRAP_LEN) {
			struct scatterlist *sg = NULL;
			unsigned int offset = 0;

			if (usb_stor_access_xfer_buf((unsigned char *) bcs,
					US_BULK_CS_WRAP_LEN, srb, &sg,
					&offset, FROM_XFER_BUF) ==
						US_BULK_CS_WRAP_LEN &&
					bcs->Signature ==
						cpu_to_le32(US_BULK_CS_SIGN)) {
				usb_stor_dbg(us, "Device skipped data phase\n");
				scsi_set_resid(srb, transfer_length);
				goto skipped_data_phase;
			}
		}
	}

	/* See flow chart on pg 15 of the Bulk Only Transport spec for
	 * an explanation of how this code works.
	 */

	/* get CSW for device status */
	usb_stor_dbg(us, "Attempting to get CSW...\n");
	result = usb_stor_bulk_transfer_buf(us, us->recv_bulk_pipe,
				bcs, US_BULK_CS_WRAP_LEN, &cswlen);

	/* Some broken devices add unnecessary zero-length packets to the
	 * end of their data transfers.  Such packets show up as 0-length
	 * CSWs.  If we encounter such a thing, try to read the CSW again.
	 */
	if (result == USB_STOR_XFER_SHORT && cswlen == 0) {
		usb_stor_dbg(us, "Received 0-length CSW; retrying...\n");
		result = usb_stor_bulk_transfer_buf(us, us->recv_bulk_pipe,
				bcs, US_BULK_CS_WRAP_LEN, &cswlen);
	}

	/* did the attempt to read the CSW fail? */
	if (result == USB_STOR_XFER_STALLED) {

		/* get the status again */
		usb_stor_dbg(us, "Attempting to get CSW (2nd try)...\n");
		result = usb_stor_bulk_transfer_buf(us, us->recv_bulk_pipe,
				bcs, US_BULK_CS_WRAP_LEN, NULL);
	}

	/* if we still have a failure at this point, we're in trouble */
	usb_stor_dbg(us, "Bulk status result = %d\n", result);
	if (result != USB_STOR_XFER_GOOD)
		return USB_STOR_TRANSPORT_ERROR;

 skipped_data_phase:
	/* check bulk status */
	residue = le32_to_cpu(bcs->Residue);
	usb_stor_dbg(us, "Bulk Status S 0x%x T 0x%x R %u Stat 0x%x\n",
		     le32_to_cpu(bcs->Signature), bcs->Tag,
		     residue, bcs->Status);
	if (!(bcs->Tag == us->tag || (us->fflags & US_FL_BULK_IGNORE_TAG)) ||
		bcs->Status > US_BULK_STAT_PHASE) {
		usb_stor_dbg(us, "Bulk logical error\n");
		return USB_STOR_TRANSPORT_ERROR;
	}

	/* Some broken devices report odd signatures, so we do not check them
	 * for validity against the spec. We store the first one we see,
	 * and check subsequent transfers for validity against this signature.
	 */
	if (!us->bcs_signature) {
		us->bcs_signature = bcs->Signature;
		if (us->bcs_signature != cpu_to_le32(US_BULK_CS_SIGN))
			usb_stor_dbg(us, "Learnt BCS signature 0x%08X\n",
				     le32_to_cpu(us->bcs_signature));
	} else if (bcs->Signature != us->bcs_signature) {
		usb_stor_dbg(us, "Signature mismatch: got %08X, expecting %08X\n",
			     le32_to_cpu(bcs->Signature),
			     le32_to_cpu(us->bcs_signature));
		return USB_STOR_TRANSPORT_ERROR;
	}

	/* try to compute the actual residue, based on how much data
	 * was really transferred and what the device tells us */
	if (residue && !(us->fflags & US_FL_IGNORE_RESIDUE)) {

		/* Heuristically detect devices that generate bogus residues
		 * by seeing what happens with INQUIRY and READ CAPACITY
		 * commands.
		 */
		if (bcs->Status == US_BULK_STAT_OK &&
				scsi_get_resid(srb) == 0 &&
					((srb->cmnd[0] == INQUIRY &&
						transfer_length == 36) ||
					(srb->cmnd[0] == READ_CAPACITY &&
						transfer_length == 8))) {
			us->fflags |= US_FL_IGNORE_RESIDUE;

		} else {
			residue = min(residue, transfer_length);
			scsi_set_resid(srb, max(scsi_get_resid(srb),
			                                       (int) residue));
		}
	}

	/* based on the status code, we report good or bad */
	switch (bcs->Status) {
		case US_BULK_STAT_OK:
			/* device babbled -- return fake sense data */
			if (fake_sense) {
				memcpy(srb->sense_buffer, 
				       usb_stor_sense_invalidCDB, 
				       sizeof(usb_stor_sense_invalidCDB));
				return USB_STOR_TRANSPORT_NO_SENSE;
			}

			/* command good -- note that data could be short */
			return USB_STOR_TRANSPORT_GOOD;

		case US_BULK_STAT_FAIL:
			/* command failed */
			return USB_STOR_TRANSPORT_FAILED;

		case US_BULK_STAT_PHASE:
			/* phase error -- note that a transport reset will be
			 * invoked by the invoke_transport() function
			 */
			return USB_STOR_TRANSPORT_ERROR;
	}

	/* we should never get here, but if we do, we're in trouble */
	return USB_STOR_TRANSPORT_ERROR;
}

static int usb_stor_bulk_transfer_buf(struct us_data *us, unsigned int pipe,
	void *buf, unsigned int length, unsigned int *act_len)
{
	int result;

	usb_stor_dbg(us, "xfer %u bytes\n", length);

	/* fill and submit the URB */
	usb_fill_bulk_urb(us->current_urb, us->pusb_dev, pipe, buf, length,
		      usb_stor_blocking_completion, NULL);
	result = usb_stor_msg_common(us, 0);

	/* store the actual length of the data transferred */
	if (act_len)
		*act_len = us->current_urb->actual_length;
	return interpret_urb_result(us, pipe, length, result, 
			us->current_urb->actual_length);
}

int usb_stor_Bulk_reset(struct us_data *us)
{
	return usb_stor_reset_common(us, US_BULK_RESET_REQUEST, 
				 USB_TYPE_CLASS | USB_RECIP_INTERFACE,
				 0, us->ifnum, NULL, 0);
}

/* Get the transport settings */
static void get_transport(struct us_data *us)
{
	switch (us->protocol) {
	case USB_PR_CB:
		us->transport_name = "Control/Bulk";
		us->transport = usb_stor_CB_transport;
		us->transport_reset = usb_stor_CB_reset;
		us->max_lun = 7;
		break;

	case USB_PR_CBI:
		us->transport_name = "Control/Bulk/Interrupt";
		us->transport = usb_stor_CB_transport;
		us->transport_reset = usb_stor_CB_reset;
		us->max_lun = 7;
		break;

	case USB_PR_BULK:
		us->transport_name = "Bulk";
		us->transport = usb_stor_Bulk_transport;
		us->transport_reset = usb_stor_Bulk_reset;
		break;
	}
}

/* Associate our private data with the USB device */
static int associate_dev(struct us_data *us, struct usb_interface *intf)
{
	/* Fill in the device-related fields */
	us->pusb_dev = interface_to_usbdev(intf);
	us->pusb_intf = intf;
	us->ifnum = intf->cur_altsetting->desc.bInterfaceNumber;
	usb_stor_dbg(us, "Vendor: 0x%04x, Product: 0x%04x, Revision: 0x%04x\n",
		     le16_to_cpu(us->pusb_dev->descriptor.idVendor),
		     le16_to_cpu(us->pusb_dev->descriptor.idProduct),
		     le16_to_cpu(us->pusb_dev->descriptor.bcdDevice));
	usb_stor_dbg(us, "Interface Subclass: 0x%02x, Protocol: 0x%02x\n",
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
		usb_stor_dbg(us, "I/O buffer allocation failed\n");
		return -ENOMEM;
	}
	return 0;
}

/* Adjust device flags based on the "quirks=" module parameter */
static void usb_stor_adjust_quirks(struct usb_device *udev, unsigned long *fflags)
{
	char *p;
	u16 vid = le16_to_cpu(udev->descriptor.idVendor);
	u16 pid = le16_to_cpu(udev->descriptor.idProduct);
	unsigned f = 0;
	unsigned int mask = (US_FL_SANE_SENSE | US_FL_BAD_SENSE |
			US_FL_FIX_CAPACITY | US_FL_IGNORE_UAS |
			US_FL_CAPACITY_HEURISTICS | US_FL_IGNORE_DEVICE |
			US_FL_NOT_LOCKABLE | US_FL_MAX_SECTORS_64 |
			US_FL_CAPACITY_OK | US_FL_IGNORE_RESIDUE |
			US_FL_SINGLE_LUN | US_FL_NO_WP_DETECT |
			US_FL_NO_READ_DISC_INFO | US_FL_NO_READ_CAPACITY_16 |
			US_FL_INITIAL_READ10 | US_FL_WRITE_CACHE |
			US_FL_NO_ATA_1X | US_FL_NO_REPORT_OPCODES |
			US_FL_MAX_SECTORS_240);

	p = quirks;
	while (*p) {
		/* Each entry consists of VID:PID:flags */
		if (vid == simple_strtoul(p, &p, 16) &&
				*p == ':' &&
				pid == simple_strtoul(p+1, &p, 16) &&
				*p == ':')
			break;

		/* Move forward to the next entry */
		while (*p) {
			if (*p++ == ',')
				break;
		}
	}
	if (!*p)	/* No match */
		return;

	/* Collect the flags */
	while (*++p && *p != ',') {
		switch (TOLOWER(*p)) {
		case 'a':
			f |= US_FL_SANE_SENSE;
			break;
		case 'b':
			f |= US_FL_BAD_SENSE;
			break;
		case 'c':
			f |= US_FL_FIX_CAPACITY;
			break;
		case 'd':
			f |= US_FL_NO_READ_DISC_INFO;
			break;
		case 'e':
			f |= US_FL_NO_READ_CAPACITY_16;
			break;
		case 'f':
			f |= US_FL_NO_REPORT_OPCODES;
			break;
		case 'g':
			f |= US_FL_MAX_SECTORS_240;
			break;
		case 'h':
			f |= US_FL_CAPACITY_HEURISTICS;
			break;
		case 'i':
			f |= US_FL_IGNORE_DEVICE;
			break;
		case 'l':
			f |= US_FL_NOT_LOCKABLE;
			break;
		case 'm':
			f |= US_FL_MAX_SECTORS_64;
			break;
		case 'n':
			f |= US_FL_INITIAL_READ10;
			break;
		case 'o':
			f |= US_FL_CAPACITY_OK;
			break;
		case 'p':
			f |= US_FL_WRITE_CACHE;
			break;
		case 'r':
			f |= US_FL_IGNORE_RESIDUE;
			break;
		case 's':
			f |= US_FL_SINGLE_LUN;
			break;
		case 't':
			f |= US_FL_NO_ATA_1X;
			break;
		case 'u':
			f |= US_FL_IGNORE_UAS;
			break;
		case 'w':
			f |= US_FL_NO_WP_DETECT;
			break;
		/* Ignore unrecognized flag characters */
		}
	}
	*fflags = (*fflags & ~mask) | f;
}

/* Get the unusual_devs entries and the string descriptors */
static int get_device_info(struct us_data *us, const struct usb_device_id *id,
		struct us_unusual_dev *unusual_dev)
{
	struct usb_device *dev = us->pusb_dev;
	struct usb_interface_descriptor *idesc =
		&us->pusb_intf->cur_altsetting->desc;
	struct device *pdev = &us->pusb_intf->dev;

	/* Store the entries */
	us->unusual_dev = unusual_dev;
	us->subclass = (unusual_dev->useProtocol == USB_SC_DEVICE) ?
			idesc->bInterfaceSubClass :
			unusual_dev->useProtocol;
	us->protocol = (unusual_dev->useTransport == USB_PR_DEVICE) ?
			idesc->bInterfaceProtocol :
			unusual_dev->useTransport;
	us->fflags = id->driver_info;
	usb_stor_adjust_quirks(us->pusb_dev, &us->fflags);

	if (us->fflags & US_FL_IGNORE_DEVICE) {
		dev_info(pdev, "device ignored\n");
		return -ENODEV;
	}

	/*
	 * This flag is only needed when we're in high-speed, so let's
	 * disable it if we're in full-speed
	 */
	if (dev->speed != USB_SPEED_HIGH)
		us->fflags &= ~US_FL_GO_SLOW;

	if (us->fflags)
		dev_info(pdev, "Quirks match for vid %04x pid %04x: %lx\n",
				le16_to_cpu(dev->descriptor.idVendor),
				le16_to_cpu(dev->descriptor.idProduct),
				us->fflags);

	/* Log a message if a non-generic unusual_dev entry contains an
	 * unnecessary subclass or protocol override.  This may stimulate
	 * reports from users that will help us remove unneeded entries
	 * from the unusual_devs.h table.
	 */
	if (id->idVendor || id->idProduct) {
		static const char *msgs[3] = {
			"an unneeded SubClass entry",
			"an unneeded Protocol entry",
			"unneeded SubClass and Protocol entries"};
		struct usb_device_descriptor *ddesc = &dev->descriptor;
		int msg = -1;

		if (unusual_dev->useProtocol != USB_SC_DEVICE &&
			us->subclass == idesc->bInterfaceSubClass)
			msg += 1;
		if (unusual_dev->useTransport != USB_PR_DEVICE &&
			us->protocol == idesc->bInterfaceProtocol)
			msg += 2;
		if (msg >= 0 && !(us->fflags & US_FL_NEED_OVERRIDE))
			dev_notice(pdev, "This device "
					"(%04x,%04x,%04x S %02x P %02x)"
					" has %s in unusual_devs.h (kernel"
					" %s)\n"
					"   Please send a copy of this message to "
					"<linux-usb@vger.kernel.org> and "
					"<usb-storage@lists.one-eyed-alien.net>\n",
					le16_to_cpu(ddesc->idVendor),
					le16_to_cpu(ddesc->idProduct),
					le16_to_cpu(ddesc->bcdDevice),
					idesc->bInterfaceSubClass,
					idesc->bInterfaceProtocol,
					msgs[msg],
					utsname()->release);
	}

	return 0;
}

static int usb_stor_control_msg(struct us_data *us, unsigned int pipe,
		                         u8 request, u8 requesttype, u16 value, u16 index, 
		                         void *data, u16 size, int timeout)
{
	int status;

	usb_stor_dbg(us, "rq=%02x rqtype=%02x value=%04x index=%02x len=%u\n",
		     request, requesttype, value, index, size);

	/* fill in the devrequest structure */
	us->cr->bRequestType = requesttype;
	us->cr->bRequest = request;
	us->cr->wValue = cpu_to_le16(value);
	us->cr->wIndex = cpu_to_le16(index);
	us->cr->wLength = cpu_to_le16(size);

	/* fill and submit the URB */
	usb_fill_control_urb(us->current_urb, us->pusb_dev, pipe, 
			 (unsigned char*) us->cr, data, size, 
			 usb_stor_blocking_completion, NULL);
	status = usb_stor_msg_common(us, timeout);

	/* return the actual length of the data transferred if no error */
	if (status == 0)
		status = us->current_urb->actual_length;
	return status;
}

/* Determine what the maximum LUN supported is */
static int usb_stor_Bulk_max_lun(struct us_data *us)
{
	int result;

	/* issue the command */
	us->iobuf[0] = 0;
	result = usb_stor_control_msg(us, us->recv_ctrl_pipe,
				 US_BULK_GET_MAX_LUN, 
				 USB_DIR_IN | USB_TYPE_CLASS | 
				 USB_RECIP_INTERFACE,
				 0, us->ifnum, us->iobuf, 1, 10*HZ);

	usb_stor_dbg(us, "GetMaxLUN command result is %d, data is %d\n",
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
	struct us_data *us = container_of(work, struct us_data,
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

/*
 * Interpret the results of a URB transfer
 *
 * This function prints appropriate debugging messages, clears halts on
 * non-control endpoints, and translates the status to the corresponding
 * USB_STOR_XFER_xxx return code.
 */
static int interpret_urb_result(struct us_data *us, unsigned int pipe,
		unsigned int length, int result, unsigned int partial)
{
	usb_stor_dbg(us, "Status code %d; transferred %u/%u\n",
		     result, partial, length);
	switch (result) {

	/* no error code; did we send all the data? */
	case 0:
		if (partial != length) {
			usb_stor_dbg(us, "-- short transfer\n");
			return USB_STOR_XFER_SHORT;
		}

		usb_stor_dbg(us, "-- transfer complete\n");
		return USB_STOR_XFER_GOOD;

	/* stalled */
	case -EPIPE:
		/* for control endpoints, (used by CB[I]) a stall indicates
		 * a failed command */
		if (usb_pipecontrol(pipe)) {
			usb_stor_dbg(us, "-- stall on control pipe\n");
			return USB_STOR_XFER_STALLED;
		}

		/* for other sorts of endpoint, clear the stall */
		usb_stor_dbg(us, "clearing endpoint halt for pipe 0x%x\n",
			     pipe);
		if (usb_stor_clear_halt(us, pipe) < 0)
			return USB_STOR_XFER_ERROR;
		return USB_STOR_XFER_STALLED;

	/* babble - the device tried to send more than we wanted to read */
	case -EOVERFLOW:
		usb_stor_dbg(us, "-- babble\n");
		return USB_STOR_XFER_LONG;

	/* the transfer was cancelled by abort, disconnect, or timeout */
	case -ECONNRESET:
		usb_stor_dbg(us, "-- transfer cancelled\n");
		return USB_STOR_XFER_ERROR;

	/* short scatter-gather read transfer */
	case -EREMOTEIO:
		usb_stor_dbg(us, "-- short read transfer\n");
		return USB_STOR_XFER_SHORT;

	/* abort or disconnect in progress */
	case -EIO:
		usb_stor_dbg(us, "-- abort or disconnect in progress\n");
		return USB_STOR_XFER_ERROR;

	/* the catch-all error case */
	default:
		usb_stor_dbg(us, "-- unknown error\n");
		return USB_STOR_XFER_ERROR;
	}
}

static void usb_stor_transparent_scsi_command(struct scsi_cmnd *srb,
				       struct us_data *us)
{
	/* send the command to the transport layer */
	usb_stor_invoke_transport(srb, us);
}

static inline struct scsi_disk *scsi_disk(struct gendisk *disk)
{
        return container_of(disk->private_data, struct scsi_disk, driver);
}


/* There are so many devices that report the capacity incorrectly,
 * this routine was written to counteract some of the resulting
 * problems.
 */
static void last_sector_hacks(struct us_data *us, struct scsi_cmnd *srb)
{
	struct gendisk *disk;
	struct scsi_disk *sdkp;
	u32 sector;

	/* To Report "Medium Error: Record Not Found */
	static unsigned char record_not_found[18] = {
		[0]	= 0x70,			/* current error */
		[2]	= MEDIUM_ERROR,		/* = 0x03 */
		[7]	= 0x0a,			/* additional length */
		[12]	= 0x14			/* Record Not Found */
	};

	/* If last-sector problems can't occur, whether because the
	 * capacity was already decremented or because the device is
	 * known to report the correct capacity, then we don't need
	 * to do anything.
	 */
	if (!us->use_last_sector_hacks)
		return;

	/* Was this command a READ(10) or a WRITE(10)? */
	if (srb->cmnd[0] != READ_10 && srb->cmnd[0] != WRITE_10)
		goto done;

	/* Did this command access the last sector? */
	sector = (srb->cmnd[2] << 24) | (srb->cmnd[3] << 16) |
			(srb->cmnd[4] << 8) | (srb->cmnd[5]);
	disk = srb->request->rq_disk;
	if (!disk)
		goto done;
	sdkp = scsi_disk(disk);
	if (!sdkp)
		goto done;
	if (sector + 1 != sdkp->capacity)
		goto done;

	if (srb->result == SAM_STAT_GOOD && scsi_get_resid(srb) == 0) {

		/* The command succeeded.  We know this device doesn't
		 * have the last-sector bug, so stop checking it.
		 */
		us->use_last_sector_hacks = 0;

	} else {
		/* The command failed.  Allow up to 3 retries in case this
		 * is some normal sort of failure.  After that, assume the
		 * capacity is wrong and we're trying to access the sector
		 * beyond the end.  Replace the result code and sense data
		 * with values that will cause the SCSI core to fail the
		 * command immediately, instead of going into an infinite
		 * (or even just a very long) retry loop.
		 */
		if (++us->last_sector_retries < 3)
			return;
		srb->result = SAM_STAT_CHECK_CONDITION;
		memcpy(srb->sense_buffer, record_not_found,
				sizeof(record_not_found));
	}

 done:
	/* Don't reset the retry counter for TEST UNIT READY commands,
	 * because they get issued after device resets which might be
	 * caused by a failed last-sector access.
	 */
	if (srb->cmnd[0] != TEST_UNIT_READY)
		us->last_sector_retries = 0;
}

static int usb_stor_port_reset(struct us_data *us)
{
	int result;

	/*for these devices we must use the class specific method */
	if (us->pusb_dev->quirks & USB_QUIRK_RESET)
		return -EPERM;

	result = usb_lock_device_for_reset(us->pusb_dev, us->pusb_intf);
	if (result < 0)
		usb_stor_dbg(us, "unable to lock device for reset: %d\n",
			     result);
	else {
		/* Were we disconnected while waiting for the lock? */
		if (test_bit(US_FLIDX_DISCONNECTING, &us->dflags)) {
			result = -EIO;
			usb_stor_dbg(us, "No reset during disconnect\n");
		} else {
			result = usb_reset_device(us->pusb_dev);
			usb_stor_dbg(us, "usb_reset_device returns %d\n",
				     result);
		}
		usb_unlock_device(us->pusb_dev);
	}
	return result;
}

/* Report a driver-initiated device reset to the SCSI layer.
 * Calling this for a SCSI-initiated reset is unnecessary but harmless.
 * The caller must own the SCSI host lock. */
void usb_stor_report_device_reset(struct us_data *us)
{
	int i;
	struct Scsi_Host *host = us_to_host(us);

	scsi_report_device_reset(host, 0, 0);
	if (us->fflags & US_FL_SCM_MULT_TARG) {
		for (i = 1; i < host->max_id; ++i)
			scsi_report_device_reset(host, 0, i);
	}
}

/* Invoke the transport and basic error-handling/recovery methods
 *
 * This is used by the protocol layers to actually send the message to
 * the device and receive the response.
 */
static void usb_stor_invoke_transport(struct scsi_cmnd *srb, struct us_data *us)
{
	int need_auto_sense;
	int result;

	/* send the command to the transport layer */
	scsi_set_resid(srb, 0);
	result = us->transport(srb, us);

	/* if the command gets aborted by the higher layers, we need to
	 * short-circuit all other processing
	 */
	if (test_bit(US_FLIDX_TIMED_OUT, &us->dflags)) {
		usb_stor_dbg(us, "-- command was aborted\n");
		srb->result = DID_ABORT << 16;
		goto Handle_Errors;
	}

	/* if there is a transport error, reset and don't auto-sense */
	if (result == USB_STOR_TRANSPORT_ERROR) {
		usb_stor_dbg(us, "-- transport indicates error, resetting\n");
		srb->result = DID_ERROR << 16;
		goto Handle_Errors;
	}

	/* if the transport provided its own sense data, don't auto-sense */
	if (result == USB_STOR_TRANSPORT_NO_SENSE) {
		srb->result = SAM_STAT_CHECK_CONDITION;
		last_sector_hacks(us, srb);
		return;
	}

	srb->result = SAM_STAT_GOOD;

	/* Determine if we need to auto-sense
	 *
	 * I normally don't use a flag like this, but it's almost impossible
	 * to understand what's going on here if I don't.
	 */
	need_auto_sense = 0;

	/*
	 * If we're running the CB transport, which is incapable
	 * of determining status on its own, we will auto-sense
	 * unless the operation involved a data-in transfer.  Devices
	 * can signal most data-in errors by stalling the bulk-in pipe.
	 */
	if ((us->protocol == USB_PR_CB || us->protocol == USB_PR_DPCM_USB) &&
			srb->sc_data_direction != DMA_FROM_DEVICE) {
		usb_stor_dbg(us, "-- CB transport device requiring auto-sense\n");
		need_auto_sense = 1;
	}

	/*
	 * If we have a failure, we're going to do a REQUEST_SENSE 
	 * automatically.  Note that we differentiate between a command
	 * "failure" and an "error" in the transport mechanism.
	 */
	if (result == USB_STOR_TRANSPORT_FAILED) {
		usb_stor_dbg(us, "-- transport indicates command failure\n");
		need_auto_sense = 1;
	}

	/*
	 * Determine if this device is SAT by seeing if the
	 * command executed successfully.  Otherwise we'll have
	 * to wait for at least one CHECK_CONDITION to determine
	 * SANE_SENSE support
	 */
	if (unlikely((srb->cmnd[0] == ATA_16 || srb->cmnd[0] == ATA_12) &&
	    result == USB_STOR_TRANSPORT_GOOD &&
	    !(us->fflags & US_FL_SANE_SENSE) &&
	    !(us->fflags & US_FL_BAD_SENSE) &&
	    !(srb->cmnd[2] & 0x20))) {
		usb_stor_dbg(us, "-- SAT supported, increasing auto-sense\n");
		us->fflags |= US_FL_SANE_SENSE;
	}

	/*
	 * A short transfer on a command where we don't expect it
	 * is unusual, but it doesn't mean we need to auto-sense.
	 */
	if ((scsi_get_resid(srb) > 0) &&
	    !((srb->cmnd[0] == REQUEST_SENSE) ||
	      (srb->cmnd[0] == INQUIRY) ||
	      (srb->cmnd[0] == MODE_SENSE) ||
	      (srb->cmnd[0] == LOG_SENSE) ||
	      (srb->cmnd[0] == MODE_SENSE_10))) {
		usb_stor_dbg(us, "-- unexpectedly short transfer\n");
	}

	/* Now, if we need to do the auto-sense, let's do it */
	if (need_auto_sense) {
		int temp_result;
		struct scsi_eh_save ses;
		int sense_size = US_SENSE_SIZE;
		struct scsi_sense_hdr sshdr;
		const u8 *scdd;
		u8 fm_ili;

		/* device supports and needs bigger sense buffer */
		if (us->fflags & US_FL_SANE_SENSE)
			sense_size = ~0;
Retry_Sense:
		usb_stor_dbg(us, "Issuing auto-REQUEST_SENSE\n");

		scsi_eh_prep_cmnd(srb, &ses, NULL, 0, sense_size);

		/* FIXME: we must do the protocol translation here */
		if (us->subclass == USB_SC_RBC || us->subclass == USB_SC_SCSI ||
				us->subclass == USB_SC_CYP_ATACB)
			srb->cmd_len = 6;
		else
			srb->cmd_len = 12;

		/* issue the auto-sense command */
		scsi_set_resid(srb, 0);
		temp_result = us->transport(us->srb, us);

		/* let's clean up right away */
		scsi_eh_restore_cmnd(srb, &ses);

		if (test_bit(US_FLIDX_TIMED_OUT, &us->dflags)) {
			usb_stor_dbg(us, "-- auto-sense aborted\n");
			srb->result = DID_ABORT << 16;

			/* If SANE_SENSE caused this problem, disable it */
			if (sense_size != US_SENSE_SIZE) {
				us->fflags &= ~US_FL_SANE_SENSE;
				us->fflags |= US_FL_BAD_SENSE;
			}
			goto Handle_Errors;
		}

		/* Some devices claim to support larger sense but fail when
		 * trying to request it. When a transport failure happens
		 * using US_FS_SANE_SENSE, we always retry with a standard
		 * (small) sense request. This fixes some USB GSM modems
		 */
		if (temp_result == USB_STOR_TRANSPORT_FAILED &&
				sense_size != US_SENSE_SIZE) {
			usb_stor_dbg(us, "-- auto-sense failure, retry small sense\n");
			sense_size = US_SENSE_SIZE;
			us->fflags &= ~US_FL_SANE_SENSE;
			us->fflags |= US_FL_BAD_SENSE;
			goto Retry_Sense;
		}

		/* Other failures */
		if (temp_result != USB_STOR_TRANSPORT_GOOD) {
			usb_stor_dbg(us, "-- auto-sense failure\n");

			/* we skip the reset if this happens to be a
			 * multi-target device, since failure of an
			 * auto-sense is perfectly valid
			 */
			srb->result = DID_ERROR << 16;
			if (!(us->fflags & US_FL_SCM_MULT_TARG))
				goto Handle_Errors;
			return;
		}

		/* If the sense data returned is larger than 18-bytes then we
		 * assume this device supports requesting more in the future.
		 * The response code must be 70h through 73h inclusive.
		 */
		if (srb->sense_buffer[7] > (US_SENSE_SIZE - 8) &&
		    !(us->fflags & US_FL_SANE_SENSE) &&
		    !(us->fflags & US_FL_BAD_SENSE) &&
		    (srb->sense_buffer[0] & 0x7C) == 0x70) {
			usb_stor_dbg(us, "-- SANE_SENSE support enabled\n");
			us->fflags |= US_FL_SANE_SENSE;

			/* Indicate to the user that we truncated their sense
			 * because we didn't know it supported larger sense.
			 */
			usb_stor_dbg(us, "-- Sense data truncated to %i from %i\n",
				     US_SENSE_SIZE,
				     srb->sense_buffer[7] + 8);
			srb->sense_buffer[7] = (US_SENSE_SIZE - 8);
		}

		scsi_normalize_sense(srb->sense_buffer, SCSI_SENSE_BUFFERSIZE,
				     &sshdr);

		usb_stor_dbg(us, "-- Result from auto-sense is %d\n",
			     temp_result);
		usb_stor_dbg(us, "-- code: 0x%x, key: 0x%x, ASC: 0x%x, ASCQ: 0x%x\n",
			     sshdr.response_code, sshdr.sense_key,
			     sshdr.asc, sshdr.ascq);
#ifdef CONFIG_USB_STORAGE_DEBUG
		usb_stor_show_sense(us, sshdr.sense_key, sshdr.asc, sshdr.ascq);
#endif

		/* set the result so the higher layers expect this data */
		srb->result = SAM_STAT_CHECK_CONDITION;

		scdd = scsi_sense_desc_find(srb->sense_buffer,
					    SCSI_SENSE_BUFFERSIZE, 4);
		fm_ili = (scdd ? scdd[3] : srb->sense_buffer[2]) & 0xA0;

		/* We often get empty sense data.  This could indicate that
		 * everything worked or that there was an unspecified
		 * problem.  We have to decide which.
		 */
		if (sshdr.sense_key == 0 && sshdr.asc == 0 && sshdr.ascq == 0 &&
		    fm_ili == 0) {
			/* If things are really okay, then let's show that.
			 * Zero out the sense buffer so the higher layers
			 * won't realize we did an unsolicited auto-sense.
			 */
			if (result == USB_STOR_TRANSPORT_GOOD) {
				srb->result = SAM_STAT_GOOD;
				srb->sense_buffer[0] = 0x0;

			/* If there was a problem, report an unspecified
			 * hardware error to prevent the higher layers from
			 * entering an infinite retry loop.
			 */
			} else {
				srb->result = DID_ERROR << 16;
				if ((sshdr.response_code & 0x72) == 0x72)
					srb->sense_buffer[1] = HARDWARE_ERROR;
				else
					srb->sense_buffer[2] = HARDWARE_ERROR;
			}
		}
	}

	/*
	 * Some devices don't work or return incorrect data the first
	 * time they get a READ(10) command, or for the first READ(10)
	 * after a media change.  If the INITIAL_READ10 flag is set,
	 * keep track of whether READ(10) commands succeed.  If the
	 * previous one succeeded and this one failed, set the REDO_READ10
	 * flag to force a retry.
	 */
	if (unlikely((us->fflags & US_FL_INITIAL_READ10) &&
			srb->cmnd[0] == READ_10)) {
		if (srb->result == SAM_STAT_GOOD) {
			set_bit(US_FLIDX_READ10_WORKED, &us->dflags);
		} else if (test_bit(US_FLIDX_READ10_WORKED, &us->dflags)) {
			clear_bit(US_FLIDX_READ10_WORKED, &us->dflags);
			set_bit(US_FLIDX_REDO_READ10, &us->dflags);
		}

		/*
		 * Next, if the REDO_READ10 flag is set, return a result
		 * code that will cause the SCSI core to retry the READ(10)
		 * command immediately.
		 */
		if (test_bit(US_FLIDX_REDO_READ10, &us->dflags)) {
			clear_bit(US_FLIDX_REDO_READ10, &us->dflags);
			srb->result = DID_IMM_RETRY << 16;
			srb->sense_buffer[0] = 0;
		}
	}

	/* Did we transfer less than the minimum amount required? */
	if ((srb->result == SAM_STAT_GOOD || srb->sense_buffer[2] == 0) &&
			scsi_bufflen(srb) - scsi_get_resid(srb) < srb->underflow)
		srb->result = DID_ERROR << 16;

	last_sector_hacks(us, srb);
	return;

	/* Error and abort processing: try to resynchronize with the device
	 * by issuing a port reset.  If that fails, try a class-specific
	 * device reset. */
  Handle_Errors:

	/* Set the RESETTING bit, and clear the ABORTING bit so that
	 * the reset may proceed. */
	scsi_lock(us_to_host(us));
	set_bit(US_FLIDX_RESETTING, &us->dflags);
	clear_bit(US_FLIDX_ABORTING, &us->dflags);
	scsi_unlock(us_to_host(us));

	/* We must release the device lock because the pre_reset routine
	 * will want to acquire it. */
	mutex_unlock(&us->dev_mutex);
	result = usb_stor_port_reset(us);
	mutex_lock(&us->dev_mutex);

	if (result < 0) {
		scsi_lock(us_to_host(us));
		usb_stor_report_device_reset(us);
		scsi_unlock(us_to_host(us));
		us->transport_reset(us);
	}
	clear_bit(US_FLIDX_RESETTING, &us->dflags);
	last_sector_hacks(us, srb);
}

static void usb_stor_pad12_command(struct scsi_cmnd *srb, struct us_data *us)
{
	/*
	 * Pad the SCSI command with zeros out to 12 bytes.  If the
	 * command already is 12 bytes or longer, leave it alone.
	 *
	 * NOTE: This only works because a scsi_cmnd struct field contains
	 * a unsigned char cmnd[16], so we know we have storage available
	 */
	for (; srb->cmd_len < 12; srb->cmd_len++)
		srb->cmnd[srb->cmd_len] = 0;

	/* send the command to the transport layer */
	usb_stor_invoke_transport(srb, us);
}

/* Get the protocol settings */
static void get_protocol(struct us_data *us)
{
	switch (us->subclass) {
	case USB_SC_RBC:
		us->protocol_name = "Reduced Block Commands (RBC)";
		us->proto_handler = usb_stor_transparent_scsi_command;
		break;

	case USB_SC_8020:
		us->protocol_name = "8020i";
		us->proto_handler = usb_stor_pad12_command;
		us->max_lun = 0;
		break;

	case USB_SC_QIC:
		us->protocol_name = "QIC-157";
		us->proto_handler = usb_stor_pad12_command;
		us->max_lun = 0;
		break;

	case USB_SC_8070:
		us->protocol_name = "8070i";
		us->proto_handler = usb_stor_pad12_command;
		us->max_lun = 0;
		break;

	case USB_SC_SCSI:
		us->protocol_name = "Transparent SCSI";
		us->proto_handler = usb_stor_transparent_scsi_command;
		break;

	case USB_SC_UFI:
		us->protocol_name = "Uniform Floppy Interface (UFI)";
		us->proto_handler = usb_stor_ufi_command;
		break;
	}
}

static void usb_stor_ufi_command(struct scsi_cmnd *srb, struct us_data *us)
{
	/* fix some commands -- this is a form of mode translation
	 * UFI devices only accept 12 byte long commands
	 *
	 * NOTE: This only works because a scsi_cmnd struct field contains
	 * a unsigned char cmnd[16], so we know we have storage available
	 */

	/* Pad the ATAPI command with zeros */
	for (; srb->cmd_len < 12; srb->cmd_len++)
		srb->cmnd[srb->cmd_len] = 0;

	/* set command length to 12 bytes (this affects the transport layer) */
	srb->cmd_len = 12;

	/* XXX We should be constantly re-evaluating the need for these */

	/* determine the correct data length for these commands */
	switch (srb->cmnd[0]) {

		/* for INQUIRY, UFI devices only ever return 36 bytes */
	case INQUIRY:
		srb->cmnd[4] = 36;
		break;

		/* again, for MODE_SENSE_10, we get the minimum (8) */
	case MODE_SENSE_10:
		srb->cmnd[7] = 0;
		srb->cmnd[8] = 8;
		break;

		/* for REQUEST_SENSE, UFI devices only ever return 18 bytes */
	case REQUEST_SENSE:
		srb->cmnd[4] = 18;
		break;
	} /* end switch on cmnd[0] */

	/* send the command to the transport layer */
	usb_stor_invoke_transport(srb, us);
}

/* Release all our dynamic resources */
static void usb_stor_release_resources(struct us_data *us)
{
	/* Tell the control thread to exit.  The SCSI host must
	 * already have been removed and the DISCONNECTING flag set
	 * so that we won't accept any more commands.
	 */
	usb_stor_dbg(us, "-- sending exit command to thread\n");
	complete(&us->cmnd_ready);
	if (us->ctl_thread)
		kthread_stop(us->ctl_thread);

	/* Call the destructor routine, if it exists */
	if (us->extra_destructor) {
		usb_stor_dbg(us, "-- calling extra_destructor()\n");
		us->extra_destructor(us->extra);
	}

	/* Free the extra data and the URB */
	kfree(us->extra);
	usb_free_urb(us->current_urb);
}

/* Dissociate from the USB device */
static void dissociate_dev(struct us_data *us)
{
	/* Free the buffers */
	kfree(us->cr);
	usb_free_coherent(us->pusb_dev, US_IOBUF_SIZE, us->iobuf, us->iobuf_dma);

	/* Remove our private data from the interface */
	usb_set_intfdata(us->pusb_intf, NULL);
}

/* Second stage of disconnect processing: deallocate all resources */
static void release_everything(struct us_data *us)
{
	usb_stor_release_resources(us);
	dissociate_dev(us);

	/* Drop our reference to the host; the SCSI core will free it
	 * (and "us" along with it) when the refcount becomes 0. */
	scsi_host_put(us_to_host(us));
}

/* First part of general USB mass-storage probing */
static int usb_stor_probe1( struct us_data **pus,
		                        struct usb_interface *intf,
		                        const struct usb_device_id *id,
		                        struct us_unusual_dev *unusual_dev,
		                        struct scsi_host_template *sht)
{
	struct Scsi_Host *host;
	struct us_data *us;
	int result;

	dev_info(&intf->dev, "STV01 USB Mass Storage device detected\n");

	/*
	 * Ask the SCSI layer to allocate a host structure, with extra
	 * space at the end for our private us_data structure.
	 */
	host = scsi_host_alloc(sht, sizeof(*us));
	if (!host) {
		dev_warn(&intf->dev, "Unable to allocate the scsi host\n");
		return -ENOMEM;
	}

	/*
	 * Allow 16-byte CDBs and thus > 2TB
	 */
	host->max_cmd_len = 16;
	host->sg_tablesize = usb_stor_sg_tablesize(intf);
	*pus = us = host_to_us(host);
	mutex_init(&(us->dev_mutex));
	us_set_lock_class(&us->dev_mutex, intf);
	init_completion(&us->cmnd_ready);
	init_completion(&(us->notify));
	init_waitqueue_head(&us->delay_wait);
	INIT_DELAYED_WORK(&us->scan_dwork, usb_stor_scan_dwork);

	/* Associate the us_data structure with the USB device */
	result = associate_dev(us, intf);
	if (result)
		goto BadDevice;

	/* Get the unusual_devs entries and the descriptors */
	result = get_device_info(us, id, unusual_dev);
	if (result)
		goto BadDevice;

	/* Get standard transport and protocol settings */
	get_transport(us);
	get_protocol(us);

	/* Give the caller a chance to fill in specialized transport
	 * or protocol settings.
	 */
	return 0;

BadDevice:
	usb_stor_dbg(us, "storage_probe() failed\n");
	release_everything(us);
	return result;
}

/* Second part of general USB mass-storage probing */
int usb_stor_probe2(struct us_data *us)
{
	int result;
	struct device *dev = &us->pusb_intf->dev;

	/* Make sure the transport and protocol have both been set */
	if (!us->transport || !us->proto_handler) {
		result = -ENXIO;
		goto BadDevice;
	}
	usb_stor_dbg(us, "Transport: %s\n", us->transport_name);
	usb_stor_dbg(us, "Protocol: %s\n", us->protocol_name);

	if (us->fflags & US_FL_SCM_MULT_TARG) {
		/*
		 * SCM eUSCSI bridge devices can have different numbers
		 * of LUNs on different targets; allow all to be probed.
		 */
		us->max_lun = 7;
		/* The eUSCSI itself has ID 7, so avoid scanning that */
		us_to_host(us)->this_id = 7;
		/* max_id is 8 initially, so no need to set it here */
	} else {
		/* In the normal case there is only a single target */
		us_to_host(us)->max_id = 1;
		/*
		 * Like Windows, we won't store the LUN bits in CDB[1] for
		 * SCSI-2 devices using the Bulk-Only transport (even though
		 * this violates the SCSI spec).
		 */
		if (us->transport == usb_stor_Bulk_transport)
			us_to_host(us)->no_scsi2_lun_in_cdb = 1;
	}

	/* fix for single-lun devices */
	if (us->fflags & US_FL_SINGLE_LUN)
		us->max_lun = 0;

	/* Find the endpoints and calculate pipe values */
	result = get_pipes(us);
	if (result)
		goto BadDevice;

	/*
	 * If the device returns invalid data for the first READ(10)
	 * command, indicate the command should be retried.
	 */
	if (us->fflags & US_FL_INITIAL_READ10)
		set_bit(US_FLIDX_REDO_READ10, &us->dflags);

	/* Acquire all the other resources and add the host */
	result = usb_stor_acquire_resources(us);
	if (result)
		goto BadDevice;
	snprintf(us->scsi_name, sizeof(us->scsi_name), "usb-storage %s",
					dev_name(&us->pusb_intf->dev));

    usb_stor_dbg(us, "us->scsi_name: %s\n", us->scsi_name);

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
	usb_stor_dbg(us, "storage_probe() failed\n");
	release_everything(us);
	return result;
}

/* Get the pipe settings */
static int get_pipes(struct us_data *us)
{
	struct usb_host_interface *altsetting =
		us->pusb_intf->cur_altsetting;
	int i;
	struct usb_endpoint_descriptor *ep;
	struct usb_endpoint_descriptor *ep_in = NULL;
	struct usb_endpoint_descriptor *ep_out = NULL;
	struct usb_endpoint_descriptor *ep_int = NULL;

	/*
	 * Find the first endpoint of each type we need.
	 * We are expecting a minimum of 2 endpoints - in and out (bulk).
	 * An optional interrupt-in is OK (necessary for CBI protocol).
	 * We will ignore any others.
	 */
	for (i = 0; i < altsetting->desc.bNumEndpoints; i++) {
		ep = &altsetting->endpoint[i].desc;

		if (usb_endpoint_xfer_bulk(ep)) {
			if (usb_endpoint_dir_in(ep)) {
				if (!ep_in)
					ep_in = ep;
			} else {
				if (!ep_out)
					ep_out = ep;
			}
		}

		else if (usb_endpoint_is_int_in(ep)) {
			if (!ep_int)
				ep_int = ep;
		}
	}

	if (!ep_in || !ep_out || (us->protocol == USB_PR_CBI && !ep_int)) {
		usb_stor_dbg(us, "Endpoint sanity check failed! Rejecting dev.\n");
		return -EIO;
	}

	/* Calculate and store the pipe values */
	us->send_ctrl_pipe = usb_sndctrlpipe(us->pusb_dev, 0);
	us->recv_ctrl_pipe = usb_rcvctrlpipe(us->pusb_dev, 0);
	us->send_bulk_pipe = usb_sndbulkpipe(us->pusb_dev,
		usb_endpoint_num(ep_out));
	us->recv_bulk_pipe = usb_rcvbulkpipe(us->pusb_dev,
		usb_endpoint_num(ep_in));
	if (ep_int) {
		us->recv_intr_pipe = usb_rcvintpipe(us->pusb_dev,
			usb_endpoint_num(ep_int));
		us->ep_bInterval = ep_int->bInterval;
	}
	return 0;
}

static void usb_stor_set_xfer_buf(unsigned char *buffer,
	unsigned int buflen, struct scsi_cmnd *srb)
{
	unsigned int offset = 0;
	struct scatterlist *sg = NULL;

	buflen = min(buflen, scsi_bufflen(srb));
	buflen = usb_stor_access_xfer_buf(buffer, buflen, srb, &sg, &offset,
			TO_XFER_BUF);
	if (buflen < scsi_bufflen(srb))
		scsi_set_resid(srb, scsi_bufflen(srb) - buflen);
}

static void fill_inquiry_response(struct us_data *us, unsigned char *data,
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

        usb_stor_dbg(us, "Check if need my faking giving\n");
        if(us->unusual_dev == NULL || us->unusual_dev->vendorName == NULL)
        {
            usb_stor_dbg(us, "Fake needed\n");
            memcpy(data+8, "steven", 6);
            memcpy(data+16, "stv01", 5);
        }
        else
        {
            usb_stor_dbg(us, "No fake needed\n");
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

static int usb_stor_control_thread(void * __us)
{
	struct us_data *us = (struct us_data *)__us;
	struct Scsi_Host *host = us_to_host(us);

	for (;;) {
		usb_stor_dbg(us, "*** thread sleeping\n");
		if (wait_for_completion_interruptible(&us->cmnd_ready))
			break;

		usb_stor_dbg(us, "*** thread awakened\n");

		/* lock the device pointers */
		mutex_lock(&(us->dev_mutex));

		/* lock access to the state */
		scsi_lock(host);

		/* When we are called with no command pending, we're done */
		if (us->srb == NULL) {
			scsi_unlock(host);
			mutex_unlock(&us->dev_mutex);
			usb_stor_dbg(us, "-- exiting\n");
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
			usb_stor_dbg(us, "UNKNOWN data direction\n");
			us->srb->result = DID_ERROR << 16;
		}

		/* reject if target != 0 or if LUN is higher than
		 * the maximum known LUN
		 */
		else if (us->srb->device->id &&
				!(us->fflags & US_FL_SCM_MULT_TARG)) {
			usb_stor_dbg(us, "Bad target number (%d:%llu)\n",
				     us->srb->device->id,
				     us->srb->device->lun);
			us->srb->result = DID_BAD_TARGET << 16;
		}

		else if (us->srb->device->lun > us->max_lun) {
			usb_stor_dbg(us, "Bad LUN (%d:%llu)\n",
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

			usb_stor_dbg(us, "Faking INQUIRY command\n");
			fill_inquiry_response(us, data_ptr, 36);
			us->srb->result = SAM_STAT_GOOD;
		}

		/* we've got a command, let's do it! */
		else {
			us->proto_handler(us->srb, us);
			usb_mark_last_busy(us->pusb_dev);
		}

		/* lock access to the state */
		scsi_lock(host);

		/* indicate that the command is done */
		if (us->srb->result != DID_ABORT << 16) {
			usb_stor_dbg(us, "scsi cmd done, result=0x%x\n",
				     us->srb->result);
			us->srb->scsi_done(us->srb);
		} else {
SkipForAbort:
			usb_stor_dbg(us, "scsi command aborted\n");
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

/* Initialize all the dynamic resources we need */
static int usb_stor_acquire_resources(struct us_data *us)
{
	int p;
	struct task_struct *th;

	us->current_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!us->current_urb) {
		usb_stor_dbg(us, "URB allocation failed\n");
		return -ENOMEM;
	}

	/* Just before we start our control thread, initialize
	 * the device if it needs initialization */
	if (us->unusual_dev->initFunction) {
		p = us->unusual_dev->initFunction(us);
		if (p)
			return p;
	}

	/* Start up our control thread */
	th = kthread_run(usb_stor_control_thread, us, "usb-storage");
	if (IS_ERR(th)) {
		dev_warn(&us->pusb_intf->dev,
				"Unable to start control thread\n");
		return PTR_ERR(th);
	}
	us->ctl_thread = th;

	return 0;
}

/* Stop the current URB transfer */
static void usb_stor_stop_transport(struct us_data *us)
{
	/* If the state machine is blocked waiting for an URB,
	 * let's wake it up.  The test_and_clear_bit() call
	 * guarantees that if a URB has just been submitted,
	 * it won't be cancelled more than once. */
	if (test_and_clear_bit(US_FLIDX_URB_ACTIVE, &us->dflags)) {
		usb_stor_dbg(us, "-- cancelling URB\n");
		usb_unlink_urb(us->current_urb);
	}

	/* If we are waiting for a scatter-gather operation, cancel it. */
	if (test_and_clear_bit(US_FLIDX_SG_ACTIVE, &us->dflags)) {
		usb_stor_dbg(us, "-- cancelling sg request\n");
		usb_sg_cancel(&us->current_sg);
	}
}


void usb_stor_host_template_init(struct scsi_host_template *sht,
				 const char *name, struct module *owner)
{
	*sht = usb_stor_host_template;
	sht->name = name;
	sht->proc_name = name;
	sht->module = owner;
}

/* First stage of disconnect processing: stop SCSI scanning,
 * remove the host, and stop accepting new commands
 */
static void quiesce_and_remove_host(struct us_data *us)
{
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

/* Handle a USB mass-storage disconnect */
static void usb_stor_disconnect(struct usb_interface *intf)
{
	struct us_data *us = usb_get_intfdata(intf);

	quiesce_and_remove_host(us);
	release_everything(us);
}

static int usb_stor_suspend(struct usb_interface *iface, pm_message_t message)
{
	struct us_data *us = usb_get_intfdata(iface);

	/* Wait until no command is running */
	mutex_lock(&us->dev_mutex);

	if (us->suspend_resume_hook)
		(us->suspend_resume_hook)(us, US_SUSPEND);

	/* When runtime PM is working, we'll set a flag to indicate
	 * whether we should autoresume when a SCSI request arrives. */

	mutex_unlock(&us->dev_mutex);
	return 0;
}

static int usb_stor_resume(struct usb_interface *iface)
{
	struct us_data *us = usb_get_intfdata(iface);

	mutex_lock(&us->dev_mutex);

	if (us->suspend_resume_hook)
		(us->suspend_resume_hook)(us, US_RESUME);

	mutex_unlock(&us->dev_mutex);
	return 0;
}

/* Report a driver-initiated bus reset to the SCSI layer.
 * Calling this for a SCSI-initiated reset is unnecessary but harmless.
 * The caller must not own the SCSI host lock. */
static void usb_stor_report_bus_reset(struct us_data *us)
{
	struct Scsi_Host *host = us_to_host(us);

	scsi_lock(host);
	scsi_report_bus_reset(host, 0);
	scsi_unlock(host);
}

static int usb_stor_pre_reset(struct usb_interface *iface)
{
	struct us_data *us = usb_get_intfdata(iface);

	/* Make sure no command runs during the reset */
	mutex_lock(&us->dev_mutex);
	return 0;
}

static int usb_stor_reset_resume(struct usb_interface *iface)
{
	struct us_data *us = usb_get_intfdata(iface);

	/* Report the reset to the SCSI core */
	usb_stor_report_bus_reset(us);

	/* FIXME: Notify the subdrivers that they need to reinitialize
	 * the device */
	return 0;
}

static int usb_stor_post_reset(struct usb_interface *iface)
{
	struct us_data *us = usb_get_intfdata(iface);

	/* Report the reset to the SCSI core */
	usb_stor_report_bus_reset(us);

	/* FIXME: Notify the subdrivers that they need to reinitialize
	 * the device */

	mutex_unlock(&us->dev_mutex);
	return 0;
}

/* The main probe routine for standard devices */
static int storage_probe(struct usb_interface *intf,
			 const struct usb_device_id *id)
{
	struct us_unusual_dev *unusual_dev;
	struct us_data *us;
	int result;
	int size;

	/* If uas is enabled and this device can do uas then ignore it. */
#if IS_ENABLED(CONFIG_USB_UAS) && 0
	if (uas_use_uas_driver(intf, id, NULL))
		return -ENXIO;
#endif

	/*
	 * Call the general probe procedures.
	 *
	 * The unusual_dev_list array is parallel to the usb_storage_usb_ids
	 * table, so we use the index of the id entry to find the
	 * corresponding unusual_devs entry.
	 */

	size = ARRAY_SIZE(us_unusual_dev_list);
	if (id >= usb_storage_usb_ids && id < usb_storage_usb_ids + size) {
		unusual_dev = (id - usb_storage_usb_ids) + us_unusual_dev_list;
	} else {
		unusual_dev = &for_dynamic_ids;

		dev_dbg(&intf->dev, "Use Bulk-Only transport with the Transparent SCSI protocol for dynamic id: 0x%04x 0x%04x\n",
			id->idVendor, id->idProduct);
	}

	result = usb_stor_probe1(&us, intf, id, unusual_dev,
				 &usb_stor_host_template);
	if (result)
		return result;

	/* No special transport or protocol settings in the main module */

	result = usb_stor_probe2(us);
	return result;
}

static struct usb_driver stv01_driver = {
	.name =		DRV_NAME,
	.probe =	storage_probe,
	.disconnect =	usb_stor_disconnect,
	.suspend =	usb_stor_suspend,
	.resume =	usb_stor_resume,
	.reset_resume =	usb_stor_reset_resume,
	.pre_reset =	usb_stor_pre_reset,
	.post_reset =	usb_stor_post_reset,
	.id_table =	stv01_usb_ids,
	.supports_autosuspend = 1,
	.soft_unbind =	1
};

module_usb_stor_driver(stv01_driver, stv01_host_template, DRV_NAME);


/***********************************************************************
 * /proc/scsi/ functions
 ***********************************************************************/


static const char* host_info(struct Scsi_Host *host)
{
	struct us_data *us = host_to_us(host);
	return us->scsi_name;
}

static int write_info(struct Scsi_Host *host, char *buffer, int length)
{
	/* if someone is sending us data, just throw it away */
	return length;
}

static int show_info (struct seq_file *m, struct Scsi_Host *host)
{
	struct us_data *us = host_to_us(host);
	const char *string;

	/* print the controller name */
	seq_printf(m, "   Host scsi%d: usb-storage\n", host->host_no);

	/* print product, vendor, and serial number strings */
	if (us->pusb_dev->manufacturer)
		string = us->pusb_dev->manufacturer;
	else if (us->unusual_dev->vendorName)
		string = us->unusual_dev->vendorName;
	else
		string = "Unknown";
	seq_printf(m, "       Vendor: %s\n", string);
	if (us->pusb_dev->product)
		string = us->pusb_dev->product;
	else if (us->unusual_dev->productName)
		string = us->unusual_dev->productName;
	else
		string = "Unknown";
	seq_printf(m, "      Product: %s\n", string);
	if (us->pusb_dev->serial)
		string = us->pusb_dev->serial;
	else
		string = "None";
	seq_printf(m, "Serial Number: %s\n", string);

	/* show the protocol and transport */
	seq_printf(m, "     Protocol: %s\n", us->protocol_name);
	seq_printf(m, "    Transport: %s\n", us->transport_name);

	/* show the device flags */
	seq_printf(m, "       Quirks:");

#define US_FLAG(name, value) \
	if (us->fflags & value) seq_printf(m, " " #name);
US_DO_ALL_FLAGS
#undef US_FLAG
	seq_putc(m, '\n');
	return 0;
}

/***********************************************************************
 * Error handling functions
 ***********************************************************************/

/* Command timeout and abort */
static int command_abort(struct scsi_cmnd *srb)
{
	struct us_data *us = host_to_us(srb->device->host);

	usb_stor_dbg(us, "%s called\n", __func__);

	/* us->srb together with the TIMED_OUT, RESETTING, and ABORTING
	 * bits are protected by the host lock. */
	scsi_lock(us_to_host(us));

	/* Is this command still active? */
	if (us->srb != srb) {
		scsi_unlock(us_to_host(us));
		usb_stor_dbg(us, "-- nothing to abort\n");
		return FAILED;
	}

	/* Set the TIMED_OUT bit.  Also set the ABORTING bit, but only if
	 * a device reset isn't already in progress (to avoid interfering
	 * with the reset).  Note that we must retain the host lock while
	 * calling usb_stor_stop_transport(); otherwise it might interfere
	 * with an auto-reset that begins as soon as we release the lock. */
	set_bit(US_FLIDX_TIMED_OUT, &us->dflags);
	if (!test_bit(US_FLIDX_RESETTING, &us->dflags)) {
		set_bit(US_FLIDX_ABORTING, &us->dflags);
		usb_stor_stop_transport(us);
	}
	scsi_unlock(us_to_host(us));

	/* Wait for the aborted command to finish */
	wait_for_completion(&us->notify);
	return SUCCESS;
}

/* This invokes the transport reset mechanism to reset the state of the
 * device */
static int device_reset(struct scsi_cmnd *srb)
{
	struct us_data *us = host_to_us(srb->device->host);
	int result;

	usb_stor_dbg(us, "%s called\n", __func__);

	/* lock the device pointers and do the reset */
	mutex_lock(&(us->dev_mutex));
	result = us->transport_reset(us);
	mutex_unlock(&us->dev_mutex);

	return result < 0 ? FAILED : SUCCESS;
}

/* Simulate a SCSI bus reset by resetting the device's USB port. */
static int bus_reset(struct scsi_cmnd *srb)
{
	struct us_data *us = host_to_us(srb->device->host);
	int result;

	usb_stor_dbg(us, "%s called\n", __func__);

	result = usb_stor_port_reset(us);
	return result < 0 ? FAILED : SUCCESS;
}

static int slave_alloc (struct scsi_device *sdev)
{
	struct us_data *us = host_to_us(sdev->host);

	/*
	 * Set the INQUIRY transfer length to 36.  We don't use any of
	 * the extra data and many devices choke if asked for more or
	 * less than 36 bytes.
	 */
	sdev->inquiry_len = 36;

	/* USB has unusual DMA-alignment requirements: Although the
	 * starting address of each scatter-gather element doesn't matter,
	 * the length of each element except the last must be divisible
	 * by the Bulk maxpacket value.  There's currently no way to
	 * express this by block-layer constraints, so we'll cop out
	 * and simply require addresses to be aligned at 512-byte
	 * boundaries.  This is okay since most block I/O involves
	 * hardware sectors that are multiples of 512 bytes in length,
	 * and since host controllers up through USB 2.0 have maxpacket
	 * values no larger than 512.
	 *
	 * But it doesn't suffice for Wireless USB, where Bulk maxpacket
	 * values can be as large as 2048.  To make that work properly
	 * will require changes to the block layer.
	 */
	blk_queue_update_dma_alignment(sdev->request_queue, (512 - 1));

	/* Tell the SCSI layer if we know there is more than one LUN */
	if (us->protocol == USB_PR_BULK && us->max_lun > 0)
		sdev->sdev_bflags |= BLIST_FORCELUN;

	return 0;
}

static int slave_configure(struct scsi_device *sdev)
{
	struct us_data *us = host_to_us(sdev->host);

	/* Many devices have trouble transferring more than 32KB at a time,
	 * while others have trouble with more than 64K. At this time we
	 * are limiting both to 32K (64 sectores).
	 */
	if (us->fflags & (US_FL_MAX_SECTORS_64 | US_FL_MAX_SECTORS_MIN)) {
		unsigned int max_sectors = 64;

		if (us->fflags & US_FL_MAX_SECTORS_MIN)
			max_sectors = PAGE_CACHE_SIZE >> 9;
		if (queue_max_hw_sectors(sdev->request_queue) > max_sectors)
			blk_queue_max_hw_sectors(sdev->request_queue,
					      max_sectors);
	} else if (sdev->type == TYPE_TAPE) {
		/* Tapes need much higher max_sector limits, so just
		 * raise it to the maximum possible (4 GB / 512) and
		 * let the queue segment size sort out the real limit.
		 */
		blk_queue_max_hw_sectors(sdev->request_queue, 0x7FFFFF);
	}

	/* Some USB host controllers can't do DMA; they have to use PIO.
	 * They indicate this by setting their dma_mask to NULL.  For
	 * such controllers we need to make sure the block layer sets
	 * up bounce buffers in addressable memory.
	 */
	if (!us->pusb_dev->bus->controller->dma_mask)
		blk_queue_bounce_limit(sdev->request_queue, BLK_BOUNCE_HIGH);

	/* We can't put these settings in slave_alloc() because that gets
	 * called before the device type is known.  Consequently these
	 * settings can't be overridden via the scsi devinfo mechanism. */
	if (sdev->type == TYPE_DISK) {

		/* Some vendors seem to put the READ CAPACITY bug into
		 * all their devices -- primarily makers of cell phones
		 * and digital cameras.  Since these devices always use
		 * flash media and can be expected to have an even number
		 * of sectors, we will always enable the CAPACITY_HEURISTICS
		 * flag unless told otherwise. */
		switch (le16_to_cpu(us->pusb_dev->descriptor.idVendor)) {
		case VENDOR_ID_NOKIA:
		case VENDOR_ID_NIKON:
		case VENDOR_ID_PENTAX:
		case VENDOR_ID_MOTOROLA:
			if (!(us->fflags & (US_FL_FIX_CAPACITY |
					US_FL_CAPACITY_OK)))
				us->fflags |= US_FL_CAPACITY_HEURISTICS;
			break;
		}

		/* Disk-type devices use MODE SENSE(6) if the protocol
		 * (SubClass) is Transparent SCSI, otherwise they use
		 * MODE SENSE(10). */
		if (us->subclass != USB_SC_SCSI && us->subclass != USB_SC_CYP_ATACB)
			sdev->use_10_for_ms = 1;

		/* Many disks only accept MODE SENSE transfer lengths of
		 * 192 bytes (that's what Windows uses). */
		sdev->use_192_bytes_for_3f = 1;

		/* Some devices don't like MODE SENSE with page=0x3f,
		 * which is the command used for checking if a device
		 * is write-protected.  Now that we tell the sd driver
		 * to do a 192-byte transfer with this command the
		 * majority of devices work fine, but a few still can't
		 * handle it.  The sd driver will simply assume those
		 * devices are write-enabled. */
		if (us->fflags & US_FL_NO_WP_DETECT)
			sdev->skip_ms_page_3f = 1;

		/* A number of devices have problems with MODE SENSE for
		 * page x08, so we will skip it. */
		sdev->skip_ms_page_8 = 1;

		/* Some devices don't handle VPD pages correctly */
		sdev->skip_vpd_pages = 1;

		/* Do not attempt to use REPORT SUPPORTED OPERATION CODES */
		sdev->no_report_opcodes = 1;

		/* Do not attempt to use WRITE SAME */
		sdev->no_write_same = 1;

		/* Some disks return the total number of blocks in response
		 * to READ CAPACITY rather than the highest block number.
		 * If this device makes that mistake, tell the sd driver. */
		if (us->fflags & US_FL_FIX_CAPACITY)
			sdev->fix_capacity = 1;

		/* A few disks have two indistinguishable version, one of
		 * which reports the correct capacity and the other does not.
		 * The sd driver has to guess which is the case. */
		if (us->fflags & US_FL_CAPACITY_HEURISTICS)
			sdev->guess_capacity = 1;

		/* Some devices cannot handle READ_CAPACITY_16 */
		if (us->fflags & US_FL_NO_READ_CAPACITY_16)
			sdev->no_read_capacity_16 = 1;

		/*
		 * Many devices do not respond properly to READ_CAPACITY_16.
		 * Tell the SCSI layer to try READ_CAPACITY_10 first.
		 * However some USB 3.0 drive enclosures return capacity
		 * modulo 2TB. Those must use READ_CAPACITY_16
		 */
		if (!(us->fflags & US_FL_NEEDS_CAP16))
			sdev->try_rc_10_first = 1;

		/* assume SPC3 or latter devices support sense size > 18 */
		if (sdev->scsi_level > SCSI_SPC_2)
			us->fflags |= US_FL_SANE_SENSE;

		/* USB-IDE bridges tend to report SK = 0x04 (Non-recoverable
		 * Hardware Error) when any low-level error occurs,
		 * recoverable or not.  Setting this flag tells the SCSI
		 * midlayer to retry such commands, which frequently will
		 * succeed and fix the error.  The worst this can lead to
		 * is an occasional series of retries that will all fail. */
		sdev->retry_hwerror = 1;

		/* USB disks should allow restart.  Some drives spin down
		 * automatically, requiring a START-STOP UNIT command. */
		sdev->allow_restart = 1;

		/* Some USB cardreaders have trouble reading an sdcard's last
		 * sector in a larger then 1 sector read, since the performance
		 * impact is negligible we set this flag for all USB disks */
		sdev->last_sector_bug = 1;

		/* Enable last-sector hacks for single-target devices using
		 * the Bulk-only transport, unless we already know the
		 * capacity will be decremented or is correct. */
		if (!(us->fflags & (US_FL_FIX_CAPACITY | US_FL_CAPACITY_OK |
					US_FL_SCM_MULT_TARG)) &&
				us->protocol == USB_PR_BULK)
			us->use_last_sector_hacks = 1;

		/* Check if write cache default on flag is set or not */
		if (us->fflags & US_FL_WRITE_CACHE)
			sdev->wce_default_on = 1;

		/* A few buggy USB-ATA bridges don't understand FUA */
		if (us->fflags & US_FL_BROKEN_FUA)
			sdev->broken_fua = 1;

	} else {

		/* Non-disk-type devices don't need to blacklist any pages
		 * or to force 192-byte transfer lengths for MODE SENSE.
		 * But they do need to use MODE SENSE(10). */
		sdev->use_10_for_ms = 1;

		/* Some (fake) usb cdrom devices don't like READ_DISC_INFO */
		if (us->fflags & US_FL_NO_READ_DISC_INFO)
			sdev->no_read_disc_info = 1;
	}

	/* The CB and CBI transports have no way to pass LUN values
	 * other than the bits in the second byte of a CDB.  But those
	 * bits don't get set to the LUN value if the device reports
	 * scsi_level == 0 (UNKNOWN).  Hence such devices must necessarily
	 * be single-LUN.
	 */
	if ((us->protocol == USB_PR_CB || us->protocol == USB_PR_CBI) &&
			sdev->scsi_level == SCSI_UNKNOWN)
		us->max_lun = 0;

	/* Some devices choke when they receive a PREVENT-ALLOW MEDIUM
	 * REMOVAL command, so suppress those commands. */
	if (us->fflags & US_FL_NOT_LOCKABLE)
		sdev->lockable = 0;

	/* this is to satisfy the compiler, tho I don't think the 
	 * return code is ever checked anywhere. */
	return 0;
}

static int target_alloc(struct scsi_target *starget)
{
	struct us_data *us = host_to_us(dev_to_shost(starget->dev.parent));

	/*
	 * Some USB drives don't support REPORT LUNS, even though they
	 * report a SCSI revision level above 2.  Tell the SCSI layer
	 * not to issue that command; it will perform a normal sequential
	 * scan instead.
	 */
	starget->no_report_luns = 1;

	/*
	 * The UFI spec treats the Peripheral Qualifier bits in an
	 * INQUIRY result as reserved and requires devices to set them
	 * to 0.  However the SCSI spec requires these bits to be set
	 * to 3 to indicate when a LUN is not present.
	 *
	 * Let the scanning code know if this target merely sets
	 * Peripheral Device Type to 0x1f to indicate no LUN.
	 */
	if (us->subclass == USB_SC_UFI)
		starget->pdt_1f_for_no_lun = 1;

	return 0;
}

/**
Template
**/
/***********************************************************************
 * Sysfs interface
 ***********************************************************************/

/* Output routine for the sysfs max_sectors file */
static ssize_t max_sectors_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct scsi_device *sdev = to_scsi_device(dev);

	return sprintf(buf, "%u\n", queue_max_hw_sectors(sdev->request_queue));
}

/* Input routine for the sysfs max_sectors file */
static ssize_t max_sectors_store(struct device *dev, struct device_attribute *attr, const char *buf,
		size_t count)
{
	struct scsi_device *sdev = to_scsi_device(dev);
	unsigned short ms;

	if (sscanf(buf, "%hu", &ms) > 0) {
		blk_queue_max_hw_sectors(sdev->request_queue, ms);
		return count;
	}
	return -EINVAL;
}
static DEVICE_ATTR_RW(max_sectors);
static struct device_attribute *sysfs_device_attr_list[] = {
	&dev_attr_max_sectors,
	NULL,
};

/* queue a command */
/* This is always called with scsi_lock(host) held */
static int queuecommand_lck(struct scsi_cmnd *srb,
			void (*done)(struct scsi_cmnd *))
{
	struct us_data *us = host_to_us(srb->device->host);

	/* check for state-transition errors */
	if (us->srb != NULL) {
		printk(KERN_ERR USB_STORAGE "Error in %s: us->srb = %p\n",
			__func__, us->srb);
		return SCSI_MLQUEUE_HOST_BUSY;
	}

	/* fail the command if we are disconnecting */
	if (test_bit(US_FLIDX_DISCONNECTING, &us->dflags)) {
		usb_stor_dbg(us, "Fail command during disconnect\n");
		srb->result = DID_NO_CONNECT << 16;
		done(srb);
		return 0;
	}

	/* enqueue the command and wake up the control thread */
	srb->scsi_done = done;
	us->srb = srb;
	complete(&us->cmnd_ready);

	return 0;
}

static DEF_SCSI_QCMD(queuecommand)

struct scsi_host_template usb_stor_host_template = {
	/* basic userland interface stuff */
	.name =				"usb-storage",
	.proc_name =			"usb-storage",
	.show_info =			show_info,
	.write_info =			write_info,
	.info =				host_info,

	/* command interface -- queued only */
	.queuecommand =			queuecommand,

	/* error and abort handlers */
	.eh_abort_handler =		command_abort,
	.eh_device_reset_handler =	device_reset,
	.eh_bus_reset_handler =		bus_reset,

	/* queue commands only, only one command per LUN */
	.can_queue =			1,

	/* unknown initiator id */
	.this_id =			-1,

	.slave_alloc =			slave_alloc,
	.slave_configure =		slave_configure,
	.target_alloc =			target_alloc,

	/* lots of sg segments can be handled */
	.sg_tablesize =			SCSI_MAX_SG_CHAIN_SEGMENTS,

	/* limit the total size of a transfer to 120 KB */
	.max_sectors =                  240,

	/* merge commands... this seems to help performance, but
	 * periodically someone should test to see which setting is more
	 * optimal.
	 */
	.use_clustering =		1,

	/* emulated HBA */
	.emulated =			1,

	/* we do our own delay after a device or bus reset */
	.skip_settle_delay =		1,

	/* sysfs device attributes */
	.sdev_attrs =			sysfs_device_attr_list,

	/* module management */
	.module =			THIS_MODULE
};

