#ifndef __EDIAG_DRV_H__
#define __EDIAG_DRV_H__

#include <linux/fs.h>


#define DRV_MODULE_NAME         "ediag_drv"
#define PFX DRV_MODULE_NAME	": "


struct proxy_t;
struct _diag_dev_info;

void evst_cleanup_devs(void);

int evst_ioctl(struct inode *inode, struct file *filp, uint cmd, ulong arg);
long evst_unlocked_ioctl(struct file *filp, uint cmd, ulong arg);

int evst_pci_scan(void *priv);

unsigned long evst_get_total_dev(void);
unsigned long evst_get_total_br_dev(void);

int evst_vf_dis(struct _diag_dev_info * cur_dev);

void evst_cleanup_one_dev(struct _diag_dev_info *dev_info);

int  evst_vf_en(
	struct _diag_dev_info * dev,      /* PF device info */
	uint                    vfs_num   /* Number of VFs */
	);

void evst_send_sigio_all(void);

struct edrv_dev {
	struct fasync_struct * async_queue;
};


/* DPC handlers */
void um_handle_default_sb(struct proxy_t * proxy);
void um_linux_timer(struct proxy_t * proxy);
void um_handle_sb(struct proxy_t * proxy, int sb_idx);

/* Functions attaching the appropriate DPC handler */
void evst_attach_timer_work(struct proxy_t * proxy);
void evst_attach_def_sb_work(struct proxy_t * proxy);
void evst_attach_sb_work(struct proxy_t * proxy, int sb_idx);

/* Proxy array related */
struct proxy_t * evst_timer_proxy(struct proxy_t *base_proxy);
struct proxy_t * evst_def_sb_proxy(struct proxy_t *base_proxy);
struct proxy_t * evst_sb_proxy(struct proxy_t *base_proxy, int sb_id);

#define PROXY_NAME_LEN 30
void evst_set_proxy_name(struct proxy_t *p, const char *p_name);
const char * evst_get_proxy_name(struct proxy_t *p);

#endif /* __EDIAG_DRV_H__ */
