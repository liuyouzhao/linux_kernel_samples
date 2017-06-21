#ifndef _STV01_KTHREAD_H_
#define _STV01_KTHREAD_H_

struct stv01_usb_data_s;

int stv01_kernel_thread(void * __us);
int stv01_usb_kthread_run(struct stv01_usb_data_s *us);

#endif
