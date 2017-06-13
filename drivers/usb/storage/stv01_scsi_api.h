#ifndef _STV01_SCSI_API_H_
#define _STV01_SCSI_API_H_

#include <linux/spinlock.h>

#define scsi_unlock(host)	spin_unlock_irq(host->host_lock)
#define scsi_lock(host)		spin_lock_irq(host->host_lock)

#endif /// _STV01_SCSI_API_H_
