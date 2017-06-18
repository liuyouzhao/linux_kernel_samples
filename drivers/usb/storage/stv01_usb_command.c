#include <scsi/scsi.h>
#include <scsi/scsi_eh.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_devinfo.h>

#include "stv01_usb_transport.h"

void usb_stor_transparent_scsi_command(struct scsi_cmnd *srb,
				       struct stv01_usb_data_s *us)
{
	/* send the command to the transport layer */
	usb_stor_invoke_transport(srb, us);
}
