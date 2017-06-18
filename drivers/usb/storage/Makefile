obj-m += stv01_usb.o
stv01_usb-objs :=  stv01_usb_driver.o \
		   stv01_usb_storage.o \
	           stv01_usb_storage_impl.o \
                   stv01_usb_transport.o \
                   stv01_scsi_host.o \
                   stv01_kthread.o \
                   stv01_urb.o \
                   stv01_usb_device.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
