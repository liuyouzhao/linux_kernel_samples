#ifndef _STV01_URB_H_
#define _STV01_URB_H_

struct stv01_usb_data_s;
int interpret_urb_result(struct stv01_usb_data_s *us, 
                            unsigned int pipe, 
                            unsigned int length, 
                            int result, 
                            unsigned int partial, 
                            int (*clear_halt)(struct stv01_usb_data_s *, unsigned int));

void usb_stor_blocking_completion(struct urb *urb);
#endif
