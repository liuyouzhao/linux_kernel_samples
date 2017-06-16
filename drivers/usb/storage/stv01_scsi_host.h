#ifndef _STV01_SCSI_HOST_H_
#define _STV01_SCSI_HOST_H_

int usb_stor_port_reset(struct stv01_usb_data_s *us);
int command_abort(struct scsi_cmnd *srb);
int target_alloc(struct scsi_target *starget);
int device_reset(struct scsi_cmnd *srb);
int bus_reset(struct scsi_cmnd *srb);
int slave_alloc (struct scsi_device *sdev);
int slave_configure(struct scsi_device *sdev);

int write_info(struct Scsi_Host *host, char *buffer, int length);
int show_info (struct seq_file *m, struct Scsi_Host *host);

extern struct device_attribute *sysfs_device_attr_list[2];
extern struct scsi_host_template usb_stor_host_template;
#endif /// _STV01_USB_COMMAND_H_

