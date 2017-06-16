#ifndef _STV01_USB_DATA_H_
#define _STV01_USB_DATA_H_

#include <linux/usb.h>
#include <scsi/scsi_host.h>

struct us_unusual_dev;
/** we allocate one of these for every device that we remember
*/
typedef struct stv01_usb_data_s {
	/*
	 * The device we're working with
	 * It's important to note:
	 *    (o) you must hold dev_mutex to change pusb_dev
	 */
	struct mutex		dev_mutex;	 /* protect pusb_dev */
	struct usb_device	*pusb_dev;	 /* this usb_device */
	struct usb_interface	*pusb_intf;	 /* this interface */


	struct us_unusual_dev   *unusual_dev;	 /* device-filter entry     */


	unsigned long		fflags;		 /* fixed flags from filter */
	unsigned long		dflags;		 /* dynamic atomic bitflags */
	unsigned int		send_bulk_pipe;	 /* cached pipe values */
	unsigned int		recv_bulk_pipe;
	unsigned int		send_ctrl_pipe;
	unsigned int		recv_ctrl_pipe;
	unsigned int		recv_intr_pipe;

	/* information about the device */
	char			*transport_name;
	char			*protocol_name;
	__le32			bcs_signature;
	u8			subclass;
	u8			protocol;
	u8			max_lun;

	u8			ifnum;		 /* interface number   */
	u8			ep_bInterval;	 /* interrupt interval */

	/* function pointers for this device */
	/* transport function	   */
    int (*transport_command)    (struct scsi_cmnd *, struct stv01_usb_data_s *);

    /* transport device reset */
    int (*transport_reset)      (struct stv01_usb_data_s *);

    /* protocol handler	   */
    void (*protocol_command)    (struct scsi_cmnd *, struct stv01_usb_data_s *);

//  trans_cmnd		transport;	 /* transport function	   */
//	trans_reset		transport_reset; /* transport device reset */
//	proto_cmnd		proto_handler;	 /* protocol handler	   */


	/* SCSI interfaces */
	struct scsi_cmnd	*srb;		 /* current srb		*/
	unsigned int		tag;		 /* current dCBWTag	*/
	char			scsi_name[32];	 /* scsi_host name	*/

	/* control and bulk communications data */
	struct urb		*current_urb;	 /* USB requests	 */
	struct usb_ctrlrequest	*cr;		 /* control requests	 */
	struct usb_sg_request	current_sg;	 /* scatter-gather req.  */
	unsigned char		*iobuf;		 /* I/O buffer		 */
	dma_addr_t		iobuf_dma;	 /* buffer DMA addresses */
	struct task_struct	*ctl_thread;	 /* the control thread   */

	/* mutual exclusion and synchronization structures */
	struct completion	cmnd_ready;	 /* to sleep thread on	    */
	struct completion	notify;		 /* thread begin/end	    */
	wait_queue_head_t	delay_wait;	 /* wait during reset	    */
	struct delayed_work	scan_dwork;	 /* for async scanning      */

	/* subdriver information */
	void			*extra;		 /* Any extra data          */

    /* extra data destructor   */
	void (*extra_destructor)(void *);

#ifdef CONFIG_PM
    /* power management hook */
    void (*suspend_resume_hook)(struct stv01_usb_data_s *, int);
#endif

	/* hacks for READ CAPACITY bug handling */
	int			use_last_sector_hacks;
	int			last_sector_retries;
} stv01_usb_data_t;

/* Convert between stv01_usb_data_s and the corresponding Scsi_Host */
static inline struct Scsi_Host *us_to_host(struct stv01_usb_data_s *us) {
	return container_of((void *) us, struct Scsi_Host, hostdata);
}

static inline struct stv01_usb_data_s *host_to_us(struct Scsi_Host *host) {
	return (struct stv01_usb_data_s *) host->hostdata;
}

static inline const char* host_info(struct Scsi_Host *host)
{
	struct stv01_usb_data_s *us = host_to_us(host);
	return us->scsi_name;
}

static inline void utils_device_dbg(const struct stv01_usb_data_s *us, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);

	dev_vprintk_emit(LOGLEVEL_DEBUG, &us->pusb_dev->dev, fmt, args);

	va_end(args);
}
#endif /// _STV01_USB_DATA_H_

