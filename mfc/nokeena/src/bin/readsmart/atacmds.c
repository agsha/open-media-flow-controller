#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>

#include "atacmds.h"

#define SMART_CYL_LOW  0x4F
#define SMART_CYL_HI   0xC2

// swap two bytes.  Point to low address
void swap2(char *location);
void swap2(char *location){
  char tmp=*location;
  *location=*(location+1);
  *(location+1)=tmp;
  return;
}

static int ata_pass_through (int fd, struct ata_cmd_in *in)
{
    struct scsi_cmnd_io io_hdr;
    unsigned char cdb[SAT_ATA_PASSTHROUGH_16LEN];
    unsigned char sense[32];
    int extend = 0;
    int ck_cond = 0;    /* set to 1 to read register(s) back */
    int protocol = 3;   /* non-data */
    int t_dir = 1;      /* 0 -> to device, 1 -> from device */
    int byte_block = 1; /* 0 -> bytes, 1 -> 512 byte blocks */
    int t_length = 0;   /* 0 -> no data transferred */
    int passthru_size = DEF_SAT_ATA_PASSTHRU_SIZE;

    memset(cdb, 0, sizeof(cdb));

  // Set data direction
    // TODO: This works only for commands where sector_count holds count!
    switch (in->direction) {
      case no_data:
        break;
      case data_in:
        protocol = 4;  // PIO data-in
        t_length = 2;  // sector_count holds count
        break;
      case data_out:
        protocol = 5;  // PIO data-out
        t_length = 2;  // sector_count holds count
        t_dir = 0;     // to device
        break;
      default:
        return -EINVAL;
    }

    // Check condition if any output register needed
    if (in->out_needed.is_set)
        ck_cond = 1;


    cdb[0] = (SAT_ATA_PASSTHROUGH_12LEN == passthru_size) ?
             SAT_ATA_PASSTHROUGH_12 : SAT_ATA_PASSTHROUGH_16;

    cdb[1] = (protocol << 1) | extend;
    cdb[2] = (ck_cond << 5) | (t_dir << 3) |
             (byte_block << 2) | t_length;

    // ATA PASS-THROUGH (16)
    const struct ata_in_regs *lo = &in->in_regs.regs;
    const struct ata_in_regs *hi = &in->in_regs.prev;
    // Note: all 'in.in_regs.prev.*' are always zero for 28-bit commands
    cdb[ 3] = hi->features.m_val;
    cdb[ 4] = lo->features.m_val;
    cdb[ 5] = hi->sector_count.m_val;
    cdb[ 6] = lo->sector_count.m_val;
    cdb[ 7] = hi->lba_low.m_val;
    cdb[ 8] = lo->lba_low.m_val;
    cdb[ 9] = hi->lba_mid.m_val;
    cdb[10] = lo->lba_mid.m_val;
    cdb[11] = hi->lba_high.m_val;
    cdb[12] = lo->lba_high.m_val;
    cdb[13] = lo->device.m_val;
    cdb[14] = lo->command.m_val;

    memset(&io_hdr, 0, sizeof(io_hdr));
    if (0 == t_length) {
        io_hdr.dxfer_dir = DXFER_NONE;
        io_hdr.dxfer_len = 0;
    } else if (t_dir) {         /* from device */
        io_hdr.dxfer_dir = DXFER_FROM_DEVICE;
        io_hdr.dxfer_len = in->size;
        io_hdr.dxferp = (unsigned char *)in->buffer;
        memset(in->buffer, 0, in->size); // prefill with zeroes
    } else {                    /* to device */
        io_hdr.dxfer_dir = DXFER_TO_DEVICE;
        io_hdr.dxfer_len = in->size;
        io_hdr.dxferp = (unsigned char *)in->buffer;
    }
    io_hdr.cmnd = cdb;
    io_hdr.cmnd_len = passthru_size;
    io_hdr.sensep = sense;
    io_hdr.max_sense_len = sizeof(sense);
    io_hdr.timeout = SCSI_TIMEOUT_DEFAULT;

    int ret = sg_io_cmnd_io(fd, &io_hdr, 0, 1);
    return ret;
}

int smartcommandhandler(int fd, smart_command_set command, int select, char *data);
int smartcommandhandler(int fd, smart_command_set command, int select, char *data){
    struct ata_cmd_in in;

    memset(&in, 0, sizeof(struct ata_cmd_in));
    // Set common register values
    switch (command) {
      default: // SMART commands
        in.in_regs.regs.command.m_val = ATA_SMART_CMD;
        in.in_regs.regs.lba_high.m_val = SMART_CYL_HI;
	in.in_regs.regs.lba_mid.m_val = SMART_CYL_LOW;
        break;
      case IDENTIFY: case PIDENTIFY: case CHECK_POWER_MODE: // Non SMART commands
        break;
    }
    // Set specific values
    switch (command) {
      case IDENTIFY:
        in.in_regs.regs.command.m_val = ATA_IDENTIFY_DEVICE;
	in.buffer = data;
	in.in_regs.regs.sector_count.m_val = 1;
	in.direction = data_in;
	in.size = in.in_regs.regs.sector_count.m_val * 512;
        break;
      case READ_VALUES:
        in.in_regs.regs.features.m_val = ATA_SMART_READ_VALUES;
	in.buffer = data;
	in.in_regs.regs.sector_count.m_val = 1;
	in.direction = data_in;
	in.size = in.in_regs.regs.sector_count.m_val * 512;
        break;
      case READ_THRESHOLDS:
        in.in_regs.regs.features.m_val = ATA_SMART_READ_THRESHOLDS;
        in.in_regs.regs.lba_low.m_val = 1; // TODO: CORRECT ???
	in.buffer = data;
	in.in_regs.regs.sector_count.m_val = 1;
	in.direction = data_in;
	in.size = in.in_regs.regs.sector_count.m_val * 512;
        break;
      case ENABLE:
        in.in_regs.regs.features.m_val = ATA_SMART_ENABLE;
        in.in_regs.regs.lba_low.m_val = 1; // TODO: CORRECT ???
        break;
      case DISABLE:
        in.in_regs.regs.features.m_val = ATA_SMART_DISABLE;
        in.in_regs.regs.lba_low.m_val = 1;  // TODO: CORRECT ???
        break;
      case STATUS_CHECK:
        in.out_needed.lba_high = in.out_needed.lba_mid = 1; // Status returned here
	in.out_needed.is_set = 1;
      case STATUS:
        in.in_regs.regs.features.m_val = ATA_SMART_STATUS;
        break;
      default:
        printf("Unrecognized command in smartcommandhandler()\n");
        return -1;
    }

    return ata_pass_through(fd, &in);
}

int ata_read_identity(int fd, struct ata_identify_device * buf,int fix_swapped_id)
{
  if ((smartcommandhandler(fd, IDENTIFY, 0, (char *)buf))){
      return -1; 
  }

  unsigned i;
  if (fix_swapped_id) {
    // Swap ID strings
    for (i = 0; i < sizeof(buf->serial_no)-1; i += 2)
      swap2((char *)(buf->serial_no+i));
    for (i = 0; i < sizeof(buf->fw_rev)-1; i += 2)
      swap2((char *)(buf->fw_rev+i));
    for (i = 0; i < sizeof(buf->model)-1; i += 2)
      swap2((char *)(buf->model+i));
  }

  return 0;
}

// returns 1 if SMART supported, 0 if SMART unsupported, -1 if can't tell
int ataSmartSupport(const struct ata_identify_device * drive)
{
  unsigned short word82=drive->command_set_1;
  unsigned short word83=drive->command_set_2;

  // check if words 82/83 contain valid info
  if ((word83>>14) == 0x01)
    // return value of SMART support bit
    return word82 & 0x0001;

  // since we can're rely on word 82, we don't know if SMART supported
  return -1;
}

// returns 1 if SMART enabled, 0 if SMART disabled, -1 if can't tell
int ataIsSmartEnabled(const struct ata_identify_device * drive)
{
  unsigned short word85=drive->cfs_enable_1;
  unsigned short word87=drive->csf_default;

  // check if words 85/86/87 contain valid info
  if ((word87>>14) == 0x01)
    // return value of SMART enabled bit
    return word85 & 0x0001;

  // Since we can't rely word85, we don't know if SMART is enabled.
  return -1;
}

// Reads SMART attributes into *data
int ataReadSmartValues(int fd, struct ata_smart_values *data){

  if (smartcommandhandler(fd, READ_VALUES, 0, (char *)data)){
    printf("Error SMART Values Read failed\n");
    return -1;
  }

  return 0;
}


int ataReadSmartThresholds (int fd, struct ata_smart_thresholds_pvt *data){

  // get data from device
  if (smartcommandhandler(fd, READ_THRESHOLDS, 0, (char *)data)){
    printf("Error SMART Thresholds Read failed\n");
    return -1;
  }
  return 0;
}

int ataEnableSmart (int fd );
int ataEnableSmart (int fd ){
  if (smartcommandhandler(fd, ENABLE, 0, NULL)){
    printf("Error SMART Enable failed\n");
    return -1;
  }
  return 0;
}

int ataDisableSmart (int fd );
int ataDisableSmart (int fd ){

  if (smartcommandhandler(fd, DISABLE, 0, NULL)){
    printf("Error SMART Disable failed\n");
    return -1;
  }
  return 0;
}


int ataDoesSmartWork(int fd);
int ataDoesSmartWork(int fd){
  int retval=smartcommandhandler(fd, STATUS, 0, NULL);

  if (-1 == retval)
    return 0;

  return 1;
}
