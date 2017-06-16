#include "stv01_usb_device.h"
#include "stv01_usb_data.h"
#include "stv01_utils.h"
#include <linux/utsname.h>

#undef USUAL_DEV
#define USUAL_DEV(use_protocol, use_transport) \
{ \
	.useProtocol = use_protocol,	\
	.useTransport = use_transport,	\
}
struct us_unusual_dev us_unusual_dev_list[] = {
    /* Control/Bulk transport for all SubClass values */
    USUAL_DEV(USB_SC_RBC, USB_PR_CB),
    USUAL_DEV(USB_SC_8020, USB_PR_CB),
    USUAL_DEV(USB_SC_QIC, USB_PR_CB),
    USUAL_DEV(USB_SC_UFI, USB_PR_CB),
    USUAL_DEV(USB_SC_8070, USB_PR_CB),
    USUAL_DEV(USB_SC_SCSI, USB_PR_CB),

    /* Control/Bulk/Interrupt transport for all SubClass values */
    USUAL_DEV(USB_SC_RBC, USB_PR_CBI),
    USUAL_DEV(USB_SC_8020, USB_PR_CBI),
    USUAL_DEV(USB_SC_QIC, USB_PR_CBI),
    USUAL_DEV(USB_SC_UFI, USB_PR_CBI),
    USUAL_DEV(USB_SC_8070, USB_PR_CBI),
    USUAL_DEV(USB_SC_SCSI, USB_PR_CBI),

    /* Bulk-only transport for all SubClass values */
    USUAL_DEV(USB_SC_RBC, USB_PR_BULK),
    USUAL_DEV(USB_SC_8020, USB_PR_BULK),
    USUAL_DEV(USB_SC_QIC, USB_PR_BULK),
    USUAL_DEV(USB_SC_UFI, USB_PR_BULK),
    USUAL_DEV(USB_SC_8070, USB_PR_BULK),
    USUAL_DEV(USB_SC_SCSI, USB_PR_BULK),
	{ }		/* Terminating entry */
};

#undef USUAL_DEV
#define USUAL_DEV(useProto, useTrans) \
{ USB_INTERFACE_INFO(USB_CLASS_MASS_STORAGE, useProto, useTrans) }

struct usb_device_id usb_storage_usb_ids[] = {
     /* Control/Bulk transport for all SubClass values */
    USUAL_DEV(USB_SC_RBC, USB_PR_CB),
    USUAL_DEV(USB_SC_8020, USB_PR_CB),
    USUAL_DEV(USB_SC_QIC, USB_PR_CB),
    USUAL_DEV(USB_SC_UFI, USB_PR_CB),
    USUAL_DEV(USB_SC_8070, USB_PR_CB),
    USUAL_DEV(USB_SC_SCSI, USB_PR_CB),

    /* Control/Bulk/Interrupt transport for all SubClass values */
    USUAL_DEV(USB_SC_RBC, USB_PR_CBI),
    USUAL_DEV(USB_SC_8020, USB_PR_CBI),
    USUAL_DEV(USB_SC_QIC, USB_PR_CBI),
    USUAL_DEV(USB_SC_UFI, USB_PR_CBI),
    USUAL_DEV(USB_SC_8070, USB_PR_CBI),
    USUAL_DEV(USB_SC_SCSI, USB_PR_CBI),

    /* Bulk-only transport for all SubClass values */
    USUAL_DEV(USB_SC_RBC, USB_PR_BULK),
    USUAL_DEV(USB_SC_8020, USB_PR_BULK),
    USUAL_DEV(USB_SC_QIC, USB_PR_BULK),
    USUAL_DEV(USB_SC_UFI, USB_PR_BULK),
    USUAL_DEV(USB_SC_8070, USB_PR_BULK),
    USUAL_DEV(USB_SC_SCSI, USB_PR_BULK),
	{ }		/* Terminating entry */
};

#undef USUAL_DEV

#define USUAL_DEV(use_protocol, use_transport) \
{ \
	.useProtocol = use_protocol,	\
	.useTransport = use_transport,	\
}

struct us_unusual_dev for_dynamic_ids = USUAL_DEV(USB_SC_SCSI, USB_PR_BULK);

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

/* Get the unusual_devs entries and the string descriptors */
int get_device_info(struct stv01_usb_data_s *us, const struct usb_device_id *id,
		struct us_unusual_dev *unusual_dev)
{
	struct usb_device *dev = us->pusb_dev;
	struct usb_interface_descriptor *idesc =
		&us->pusb_intf->cur_altsetting->desc;
	struct device *pdev = &us->pusb_intf->dev;

	/* Store the entries */
	us->unusual_dev = unusual_dev;
	us->subclass = (unusual_dev->useProtocol == USB_SC_DEVICE) ?
			idesc->bInterfaceSubClass :
			unusual_dev->useProtocol;
	us->protocol = (unusual_dev->useTransport == USB_PR_DEVICE) ?
			idesc->bInterfaceProtocol :
			unusual_dev->useTransport;
	us->fflags = id->driver_info;
	usb_stor_adjust_quirks(us->pusb_dev, &us->fflags);

	if (us->fflags & US_FL_IGNORE_DEVICE) {
		dev_info(pdev, "device ignored\n");
		return -ENODEV;
	}

	/*
	 * This flag is only needed when we're in high-speed, so let's
	 * disable it if we're in full-speed
	 */
	if (dev->speed != USB_SPEED_HIGH)
		us->fflags &= ~US_FL_GO_SLOW;

	if (us->fflags)
		dev_info(pdev, "Quirks match for vid %04x pid %04x: %lx\n",
				le16_to_cpu(dev->descriptor.idVendor),
				le16_to_cpu(dev->descriptor.idProduct),
				us->fflags);

	/* Log a message if a non-generic unusual_dev entry contains an
	 * unnecessary subclass or protocol override.  This may stimulate
	 * reports from users that will help us remove unneeded entries
	 * from the unusual_devs.h table.
	 */
	if (id->idVendor || id->idProduct) {
		static const char *msgs[3] = {
			"an unneeded SubClass entry",
			"an unneeded Protocol entry",
			"unneeded SubClass and Protocol entries"};
		struct usb_device_descriptor *ddesc = &dev->descriptor;
		int msg = -1;

		if (unusual_dev->useProtocol != USB_SC_DEVICE &&
			us->subclass == idesc->bInterfaceSubClass)
			msg += 1;
		if (unusual_dev->useTransport != USB_PR_DEVICE &&
			us->protocol == idesc->bInterfaceProtocol)
			msg += 2;
		if (msg >= 0 && !(us->fflags & US_FL_NEED_OVERRIDE))
			dev_notice(pdev, "This device "
					"(%04x,%04x,%04x S %02x P %02x)"
					" has %s in unusual_devs.h (kernel"
					" %s)\n"
					"   Please send a copy of this message to "
					"<linux-usb@vger.kernel.org> and "
					"<usb-storage@lists.one-eyed-alien.net>\n",
					le16_to_cpu(ddesc->idVendor),
					le16_to_cpu(ddesc->idProduct),
					le16_to_cpu(ddesc->bcdDevice),
					idesc->bInterfaceSubClass,
					idesc->bInterfaceProtocol,
					msgs[msg],
					utsname()->release);
	}

	return 0;
}

