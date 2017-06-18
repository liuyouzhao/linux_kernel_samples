#ifndef _STV01_USB_COMMAND_H_
#define _STV01_USB_COMMAND_H_

#include <scsi/scsi.h>
#include <scsi/scsi_eh.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_devinfo.h>

void usb_stor_transparent_scsi_command(struct scsi_cmnd *srb,
				       struct stv01_usb_data_s *us);
#endif /// _STV01_USB_COMMAND_H_
