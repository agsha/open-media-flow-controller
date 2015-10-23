#ifndef ATACMDS_H_
#define ATACMDS_H_

#include <sys/types.h>
#include <stdint.h>

// Macro to check expected size of struct at compile time using a
// dummy typedef.  On size mismatch, compiler reports a negative array
// size.  If you see an error message of this form, it means that the
// #pragma pack(1) pragma below is not having the desired effect on
// your compiler.
#define ASSERT_SIZEOF_STRUCT(s, n) \
  typedef char assert_sizeof_struct_##s[(sizeof(struct s) == (n)) ? 1 : -1]

// Add __attribute__((packed)) if compiler supports it
// because some gcc versions (at least ARM) lack support of #pragma pack()
#ifdef HAVE_ATTR_PACKED
#define ATTR_PACKED __attribute__((packed))
#else
#define ATTR_PACKED
#endif

#ifndef SAT_ATA_PASSTHROUGH_12
#define SAT_ATA_PASSTHROUGH_12 0xa1
#endif
#ifndef SAT_ATA_PASSTHROUGH_16
#define SAT_ATA_PASSTHROUGH_16 0x85
#endif

#define SCSI_TIMEOUT_DEFAULT    20  // should be longer than the spin up time
typedef unsigned char UINT8;
typedef signed char INT8;
typedef unsigned int UINT32;
typedef int INT32;

#define DXFER_NONE        0
#define DXFER_FROM_DEVICE 1
#define DXFER_TO_DEVICE   2

struct scsi_cmnd_io
{
    UINT8 * cmnd;       /* [in]: ptr to SCSI command block (cdb) */
    size_t  cmnd_len;   /* [in]: number of bytes in SCSI command */
    int dxfer_dir;      /* [in]: DXFER_NONE, DXFER_FROM_DEVICE, or
                                 DXFER_TO_DEVICE */
    UINT8 * dxferp;     /* [in]: ptr to outgoing or incoming data buffer */
    size_t dxfer_len;   /* [in]: bytes to be transferred to/from dxferp */
    UINT8 * sensep;     /* [in]: ptr to sense buffer, filled when
                                 CHECK CONDITION status occurs */
    size_t max_sense_len; /* [in]: max number of bytes to write to sensep */
    unsigned timeout;   /* [in]: seconds, 0-> default timeout (60 seconds?) */
    size_t resp_sense_len;  /* [out]: sense buffer length written */
    UINT8 scsi_status;  /* [out]: 0->ok, 2->CHECK CONDITION, etc ... */
    int resid;          /* [out]: Number of bytes requested to be transferred
                                  less actual number transferred (0 if not
                                   supported) */
};

typedef enum {
  // returns no data, just succeeds or fails
  ENABLE,
  DISABLE,
  AUTOSAVE,
  IMMEDIATE_OFFLINE,
  AUTO_OFFLINE,
  STATUS,       // just says if SMART is working or not
  STATUS_CHECK, // says if disk's SMART status is healthy, or failing
  // return 512 bytes of data:
  READ_VALUES,
  READ_THRESHOLDS,
  READ_LOG,
  IDENTIFY,
  PIDENTIFY,
  // returns 1 byte of data
  CHECK_POWER_MODE,
  // writes 512 bytes of data:
  WRITE_LOG
} smart_command_set;

// ATA Specification Command Register Values (Commands)
#define ATA_IDENTIFY_DEVICE             0xec
#define ATA_IDENTIFY_PACKET_DEVICE      0xa1
#define ATA_SMART_CMD                   0xb0
#define ATA_CHECK_POWER_MODE            0xe5
// 48-bit commands
#define ATA_READ_LOG_EXT                0x2F

// ATA Specification Feature Register Values (SMART Subcommands).
// Note that some are obsolete as of ATA-7.
#define ATA_SMART_READ_VALUES           0xd0
#define ATA_SMART_READ_THRESHOLDS       0xd1
#define ATA_SMART_AUTOSAVE              0xd2
#define ATA_SMART_SAVE                  0xd3
#define ATA_SMART_IMMEDIATE_OFFLINE     0xd4
#define ATA_SMART_READ_LOG_SECTOR       0xd5
#define ATA_SMART_WRITE_LOG_SECTOR      0xd6
#define ATA_SMART_WRITE_THRESHOLDS      0xd7
#define ATA_SMART_ENABLE                0xd8
#define ATA_SMART_DISABLE               0xd9
#define ATA_SMART_STATUS                0xda
// SFF 8035i Revision 2 Specification Feature Register Value (SMART
// Subcommand)
#define ATA_SMART_AUTO_OFFLINE          0xdb


#define SAT_ATA_PASSTHROUGH_12LEN 12
#define SAT_ATA_PASSTHROUGH_16LEN 16

#define DEF_SAT_ATA_PASSTHRU_SIZE 16
#define ATA_RETURN_DESCRIPTOR 9

// Maximum allowed number of SMART Attributes
#define NUMBER_ATA_SMART_ATTRIBUTES     30

// Needed parts of the ATA DRIVE IDENTIFY Structure. Those labeled
// word* are NOT used.
#pragma pack(1)
struct ata_identify_device {
  unsigned short words000_009[10];
  unsigned char  serial_no[20];
  unsigned short words020_022[3];
  unsigned char  fw_rev[8];
  unsigned char  model[40];
  unsigned short words047_079[33];
  unsigned short major_rev_num;
  unsigned short minor_rev_num;
  unsigned short command_set_1;
  unsigned short command_set_2;
  unsigned short command_set_extension;
  unsigned short cfs_enable_1;
  unsigned short word086;
  unsigned short csf_default;
  unsigned short words088_255[168];
} ATTR_PACKED;
#pragma pack()
ASSERT_SIZEOF_STRUCT(ata_identify_device, 512);

/* ata_smart_attribute is the vendor specific in SFF-8035 spec */ 
#pragma pack(1)
struct ata_smart_attribute {
  unsigned char id;
  // meaning of flag bits: see MACROS just below
  // WARNING: MISALIGNED!
  unsigned short flags; 
  unsigned char current;
  unsigned char worst;
  unsigned char raw[6];
  unsigned char reserv;
} ATTR_PACKED;
#pragma pack()
ASSERT_SIZEOF_STRUCT(ata_smart_attribute, 12);

/* ata_smart_values is format of the read drive Attribute command */
/* see Table 34 of T13/1321D Rev 1 spec (Device SMART data structure) for *some* info */
#pragma pack(1)
struct ata_smart_values {
  unsigned short int revnumber;
  struct ata_smart_attribute vendor_attributes [NUMBER_ATA_SMART_ATTRIBUTES];
  unsigned char offline_data_collection_status;
  unsigned char self_test_exec_status;  //IBM # segments for offline collection
  unsigned short int total_time_to_complete_off_line; // IBM different
  unsigned char vendor_specific_366; // Maxtor & IBM curent segment pointer
  unsigned char offline_data_collection_capability;
  unsigned short int smart_capability;
  unsigned char errorlog_capability;
  unsigned char vendor_specific_371;  // Maxtor, IBM: self-test failure checkpoint see below!
  unsigned char short_test_completion_time;
  unsigned char extend_test_completion_time;
  unsigned char conveyance_test_completion_time;
  unsigned char reserved_375_385[11];
  unsigned char vendor_specific_386_510[125]; // Maxtor bytes 508-509 Attribute/Threshold Revision #
  unsigned char chksum;
} ATTR_PACKED;
#pragma pack()
ASSERT_SIZEOF_STRUCT(ata_smart_values, 512);

/* Vendor attribute of SMART Threshold (compare to ata_smart_attribute above) */
#pragma pack(1)
struct ata_smart_threshold_entry {
  unsigned char id;
  unsigned char threshold;
  unsigned char reserved[10];
} ATTR_PACKED;
#pragma pack()
ASSERT_SIZEOF_STRUCT(ata_smart_threshold_entry, 12);

/* Format of Read SMART THreshold Command */
/* Compare to ata_smart_values above */
#pragma pack(1)
struct ata_smart_thresholds_pvt {
  unsigned short int revnumber;
  struct ata_smart_threshold_entry thres_entries[NUMBER_ATA_SMART_ATTRIBUTES];
  unsigned char reserved[149];
  unsigned char chksum;
} ATTR_PACKED;
#pragma pack()
ASSERT_SIZEOF_STRUCT(ata_smart_thresholds_pvt, 512);


int ataIsSmartEnabled(const struct ata_identify_device * drive);
int ataSmartSupport(const struct ata_identify_device * drive);

// Convenience function for formatting strings from struct ata_identify_device.
void ata_format_id_string(char * out, const unsigned char * in, int n);

struct ata_register {
    unsigned char m_val;
    int	    m_is_set;
};

struct ata_reg_alias_16 {
    struct ata_register m_lo, m_hi;
};

struct ata_reg_alias_48 {
    struct ata_register m_ll, m_lm, m_lh,
			m_hl, m_hm, m_hh;

};
/// ATA Input registers (for 28-bit commands)
struct ata_in_regs
{
  // ATA-6/7 register names  // ATA-3/4/5        // ATA-8
  struct ata_register features;     // features         // features
  struct ata_register sector_count; // sector count     // count
  struct ata_register lba_low;      // sector number    // ]
  struct ata_register lba_mid;      // cylinder low     // ] lba
  struct ata_register lba_high;     // cylinder high    // ]
  struct ata_register device;       // device/head      // device
  struct ata_register command;      // 
};

struct ata_out_regs_flags
{
 int error, sector_count, lba_low, lba_mid, lba_high, device, status;
 int is_set;
};

struct ata_in_regs_48bit
{
  struct ata_in_regs regs;
  struct ata_in_regs prev;  ///< "previous content"

  // 16-bit aliases for above pair.
  struct ata_reg_alias_16 features_16;
  struct ata_reg_alias_16 sector_count_16;
  struct ata_reg_alias_16 lba_low_16;
  struct ata_reg_alias_16 lba_mid_16;
  struct ata_reg_alias_16 lba_high_16;

  // 48-bit alias to all 8-bit LBA registers.
  struct ata_reg_alias_48 lba_48;

};
struct ata_cmd_in
{
  struct ata_in_regs_48bit in_regs;  ///< Input registers
  struct ata_out_regs_flags out_needed; //< True if output register value needed
  enum { no_data = 0, data_in, data_out } direction; ///< I/O direction
  void * buffer; ///< Pointer to data buffer
  unsigned size; ///< Size of buffer
};

int ata_read_identity(int fd, struct ata_identify_device * buf,int fix_swapped_id);
int ataReadSmartValues(int fd, struct ata_smart_values *data);
int ataReadSmartThresholds (int fd, struct ata_smart_thresholds_pvt *data);
int sg_io_cmnd_io(int dev_fd, struct scsi_cmnd_io * iop, int report,
                         int unknown);
#endif /* ATACMDS_H_ */
