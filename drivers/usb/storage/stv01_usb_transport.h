#ifndef _STV01_USB_TRANSPORT_H_
#define _STV01_USB_TRANSPORT_H_

#include <scsi/scsi.h>
#include <scsi/scsi_eh.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_devinfo.h>

struct stv01_usb_data_s;
/* struct scsi_cmnd transfer buffer access utilities */
enum xfer_buf_dir	{TO_XFER_BUF, FROM_XFER_BUF};

void usb_stor_invoke_transport(struct scsi_cmnd *srb, struct stv01_usb_data_s *us);
int usb_stor_Bulk_transport(struct scsi_cmnd *srb, struct stv01_usb_data_s *us);
int usb_stor_msg_common(struct stv01_usb_data_s *us, int timeout);
unsigned int usb_stor_access_xfer_buf(unsigned char *buffer,
	unsigned int buflen, struct scsi_cmnd *srb, struct scatterlist **sgptr,
	unsigned int *offset, enum xfer_buf_dir dir);
int usb_stor_clear_halt(struct stv01_usb_data_s *us, unsigned int pipe);
int usb_stor_control_msg(struct stv01_usb_data_s *us, unsigned int pipe,
		                         u8 request, u8 requesttype, u16 value, u16 index, 
		                         void *data, u16 size, int timeout);
int usb_stor_reset_common(struct stv01_usb_data_s *us,
		                    u8 request, u8 requesttype,
		                    u16 value, u16 index, void *data, u16 size);

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

static inline struct scsi_disk *scsi_disk(struct gendisk *disk)
{
        return container_of(disk->private_data, struct scsi_disk, driver);
}
#endif /// _STV01_USB_TRANSPORT_H_
