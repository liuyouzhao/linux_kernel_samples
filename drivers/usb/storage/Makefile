obj-m += stv01_usb_driver.o
stv01_usb_driver-objs := stv01_usb_storage.o \
			 stv01_usb_storage_impl.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
