#include "stv01_usb_device.h"
#include "stv01_usb_command.h"
#include "stv01_usb_transport.h"
#include "stv01_usb_data.h"
#include <linux/utsname.h>

static char quirks[128];

/* Adjust device flags based on the "quirks=" module parameter */
static void usb_stor_adjust_quirks(struct usb_device *udev, unsigned long *fflags)
{
	char *p;
	u16 vid = le16_to_cpu(udev->descriptor.idVendor);
	u16 pid = le16_to_cpu(udev->descriptor.idProduct);
	unsigned f = 0;
	unsigned int mask = (US_FL_SANE_SENSE | US_FL_BAD_SENSE |
			US_FL_FIX_CAPACITY | US_FL_IGNORE_UAS |
			US_FL_CAPACITY_HEURISTICS | US_FL_IGNORE_DEVICE |
			US_FL_NOT_LOCKABLE | US_FL_MAX_SECTORS_64 |
			US_FL_CAPACITY_OK | US_FL_IGNORE_RESIDUE |
			US_FL_SINGLE_LUN | US_FL_NO_WP_DETECT |
			US_FL_NO_READ_DISC_INFO | US_FL_NO_READ_CAPACITY_16 |
			US_FL_INITIAL_READ10 | US_FL_WRITE_CACHE |
			US_FL_NO_ATA_1X | US_FL_NO_REPORT_OPCODES |
			US_FL_MAX_SECTORS_240);

	p = quirks;
	while (*p) {
		/* Each entry consists of VID:PID:flags */
		if (vid == simple_strtoul(p, &p, 16) &&
				*p == ':' &&
				pid == simple_strtoul(p+1, &p, 16) &&
				*p == ':')
			break;

		/* Move forward to the next entry */
		while (*p) {
			if (*p++ == ',')
				break;
		}
	}
	if (!*p)	/* No match */
		return;

	/* Collect the flags */
	while (*++p && *p != ',') {
		switch (TOLOWER(*p)) {
		case 'a':
			f |= US_FL_SANE_SENSE;
			break;
		case 'b':
			f |= US_FL_BAD_SENSE;
			break;
		case 'c':
			f |= US_FL_FIX_CAPACITY;
			break;
		case 'd':
			f |= US_FL_NO_READ_DISC_INFO;
			break;
		case 'e':
			f |= US_FL_NO_READ_CAPACITY_16;
			break;
		case 'f':
			f |= US_FL_NO_REPORT_OPCODES;
			break;
		case 'g':
			f |= US_FL_MAX_SECTORS_240;
			break;
		case 'h':
			f |= US_FL_CAPACITY_HEURISTICS;
			break;
		case 'i':
			f |= US_FL_IGNORE_DEVICE;
			break;
		case 'l':
			f |= US_FL_NOT_LOCKABLE;
			break;
		case 'm':
			f |= US_FL_MAX_SECTORS_64;
			break;
		case 'n':
			f |= US_FL_INITIAL_READ10;
			break;
		case 'o':
			f |= US_FL_CAPACITY_OK;
			break;
		case 'p':
			f |= US_FL_WRITE_CACHE;
			break;
		case 'r':
			f |= US_FL_IGNORE_RESIDUE;
			break;
		case 's':
			f |= US_FL_SINGLE_LUN;
			break;
		case 't':
			f |= US_FL_NO_ATA_1X;
			break;
		case 'u':
			f |= US_FL_IGNORE_UAS;
			break;
		case 'w':
			f |= US_FL_NO_WP_DETECT;
			break;
		/* Ignore unrecognized flag characters */
		}
	}
	*fflags = (*fflags & ~mask) | f;
}

int usb_stor_Bulk_reset(struct stv01_usb_data_s *us)
{
	return usb_stor_reset_common(us, US_BULK_RESET_REQUEST, 
				 USB_TYPE_CLASS | USB_RECIP_INTERFACE,
				 0, us->ifnum, NULL, 0);
}

/* Get the entries and the string descriptors */
static int get_device_info( struct stv01_usb_data_s *us, const struct usb_device_id *id)
{
    us->subclass = USB_SC_SCSI;
    us->protocol = USB_PR_BULK;
    
	us->fflags = id->driver_info;
	usb_stor_adjust_quirks(us->pusb_dev, &us->fflags);

	return 0;
}

/* Get the protocol settings */
static void get_device_protocol(struct stv01_usb_data_s *us)
{
	us->protocol_name = "Transparent SCSI";
	us->protocol_command = usb_stor_invoke_transport;
}

static void get_device_transport(struct stv01_usb_data_s *us)
{
	us->transport_name = "Bulk";
	us->transport_command = usb_stor_Bulk_transport;
	us->transport_reset = usb_stor_Bulk_reset;

}

static stv01_usb_device_method_t s_device_info_manager = 
{
    .get_info = get_device_info,
    .get_protocol = get_device_protocol,
    .get_transport = get_device_transport
};

EXPORT_PTR_V(stv01_usb_device_method_t, dim, &s_device_info_manager)

