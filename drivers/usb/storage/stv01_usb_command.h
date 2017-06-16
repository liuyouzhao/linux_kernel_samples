#ifndef _STV01_USB_COMMAND_H_
#define _STV01_USB_COMMAND_H_

void usb_stor_ufi_command(struct scsi_cmnd *srb, struct stv01_usb_data_s *us);
void usb_stor_pad12_command(struct scsi_cmnd *srb, struct stv01_usb_data_s *us);

#endif /// _STV01_USB_COMMAND_H_
