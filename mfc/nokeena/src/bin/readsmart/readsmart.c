#include <errno.h>
#include <fcntl.h>
#include <glob.h>

#include <scsi/scsi.h>
#include <scsi/scsi_ioctl.h>
#include <scsi/sg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <ctype.h>
#include <stddef.h>  // for offsetof()
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "atacmds.h"

#define ARGUSED(x) ((void)(x))

char *dev_name = NULL;
int fd = -1;

int sg_io_cmnd_io(int dev_fd, struct scsi_cmnd_io * iop, int report,
                         int unknown)
{
    struct sg_io_hdr io_hdr;

    memset(&io_hdr, 0, sizeof(struct sg_io_hdr));
    io_hdr.interface_id = 'S';
    io_hdr.cmd_len = iop->cmnd_len;
    io_hdr.mx_sb_len = iop->max_sense_len;
    io_hdr.dxfer_len = iop->dxfer_len;
    io_hdr.dxferp = iop->dxferp;
    io_hdr.cmdp = iop->cmnd;
    io_hdr.sbp = iop->sensep;
    /* sg_io_hdr interface timeout has millisecond units. Timeout of 0
       defaults to 60 seconds. */
    io_hdr.timeout = ((0 == iop->timeout) ? 60 : iop->timeout) * 1000;
    switch (iop->dxfer_dir) {
        case DXFER_NONE:
            io_hdr.dxfer_direction = SG_DXFER_NONE;
            break;
        case DXFER_FROM_DEVICE:
            io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
            break;
        case DXFER_TO_DEVICE:
            io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
            break;
        default:
            printf("do_scsi_cmnd_io: bad dxfer_dir\n");
            return -EINVAL;
    }
    iop->resp_sense_len = 0;
    iop->scsi_status = 0;
    iop->resid = 0;
    if (ioctl(dev_fd, SG_IO, &io_hdr) < 0) {
	printf("SG_IO ioctl failed, errno=%d [%s]\n", errno, strerror(errno));
        return -errno;
    }
    iop->resid = io_hdr.resid;
    iop->scsi_status = io_hdr.status;

    return 0;
}

// Copies in to out, but removes leading and trailing whitespace.
static void trim(char * out, const char * in)
{
  // Find the first non-space character (maybe none).
  int first = -1;
  int i;
  for (i = 0; in[i]; i++)
    if (!isspace((int)in[i])) {
      first = i;
      break;
    }

  if (first == -1) {
    // There are no non-space characters.
    out[0] = '\0';
    return;
  }

  // Find the last non-space character.
  for (i = strlen(in)-1; i >= first && isspace((int)in[i]); i--)
    ;
  int last = i;

  strncpy(out, in+first, last-first+1);
  out[last-first+1] = '\0';
}

void ata_format_id_string(char * out, const unsigned char * in, int n)
{
  char tmp[65];
  n = n > 64 ? 64 : n;
  strncpy(tmp, (const char *)in, n);
  tmp[n] = '\0';
  trim(out, tmp);
}

static void print_drive_info(struct ata_identify_device * drive)
{
    char model[40+1], serial[20+1], firmware[8+1];
    int smart_supported, smart_enabled;

    ata_format_id_string(model, drive->model, sizeof(model)-1);
    ata_format_id_string(serial, drive->serial_no, sizeof(serial)-1);
    ata_format_id_string(firmware, drive->fw_rev, sizeof(firmware)-1);

    smart_supported = ataSmartSupport(drive);
    smart_enabled = ataIsSmartEnabled(drive);

    printf("Model : %s\n", model);
    printf("Serial Number: %s\n", serial);
    printf("FW Rev: %s\n", firmware);

    printf("SMART : ");
    if (smart_supported)
	printf("Supported, ");
    else
	printf("Not Supported, ");

    if (smart_enabled)
	printf("Enabled");
    else
	printf("Disabled");

    printf("\n");

    return ;
}

static uint64_t ata_get_attr_raw_value(struct ata_smart_attribute *attr)
{
    const char* byteorder = "543210";
    unsigned char b;
    uint64_t rawvalue = 0;
    int i;

    for (i = 0; byteorder[i]; i++) {
	switch (byteorder[i]) {
	  case '0': b = attr->raw[0];  break;
	  case '1': b = attr->raw[1];  break;
	  case '2': b = attr->raw[2];  break;
	  case '3': b = attr->raw[3];  break;
	  case '4': b = attr->raw[4];  break;
	  case '5': b = attr->raw[5];  break;
	  case 'r': b = attr->reserv;  break;
	  case 'v': b = attr->current; break;
	  case 'w': b = attr->worst;   break;
	  default : b = 0;            break;
	}
	rawvalue <<= 8; rawvalue |= b;
    }
    return rawvalue;
}

static void print_smart_attrs(struct ata_smart_values *data)
{
    struct ata_smart_attribute *attr = NULL;
    int i;

    printf("ID\tValue\n");
    for (i = 0; i < NUMBER_ATA_SMART_ATTRIBUTES; i++) {
	attr = &data->vendor_attributes[i];
	if ((int)attr->id)
	    printf("%d\t%ld\n", (int)attr->id, ata_get_attr_raw_value(attr));
    }
}

// >>>>>> End of general SCSI specific linux code
int main(int argc, char* argv[])
{
    int ret = 0;
    struct ata_identify_device drive;
    struct ata_smart_values smartval;
    struct ata_smart_thresholds_pvt smartthres;

    if (argc < 2) {
	printf("Usage: %s <device>\n", argv[0]);
	return -1;
    }

    dev_name = argv[1];

    fd = open(dev_name, O_RDWR|O_NONBLOCK);
    if (fd < 0) {
	ret = errno;
	printf("Unable to open %s : %d\n", dev_name, ret);
	return ret;
    }

    memset(&drive, 0, sizeof(drive));
    memset(&smartval, 0, sizeof(smartval));
    memset(&smartthres, 0, sizeof(smartthres));

    ret = ata_read_identity(fd, &drive, 1);
    if (ret) {
	printf("Read Identify failed on device %s\n", dev_name);
	return ret;
    }

    print_drive_info(&drive);

    ret = ataReadSmartValues(fd, &smartval);
    if (ret) {
	printf("Read SMART Values failed on device %s\n", dev_name);
	return ret;
    }

    ret = ataReadSmartThresholds (fd, &smartthres);
    if (ret) {
	printf("Read SMART THRESHOLD failed on device %s\n", dev_name);
	return ret;
    }

    print_smart_attrs(&smartval);

    close(fd);
    return 0;
}
