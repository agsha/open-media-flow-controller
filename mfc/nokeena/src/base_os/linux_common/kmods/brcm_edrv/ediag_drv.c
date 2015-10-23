#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/device.h>  /* for dev_info() */
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/bitops.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <asm/byteorder.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/workqueue.h>
#include <asm/io.h>
#if (LINUX_VERSION_CODE >= 0x20622)
        #include <generated/autoconf.h>
#else
        #include <linux/autoconf.h>
#endif

#include "ediag_drv.h"
#include "ediag_compat.h"
#include "debug.h"
#include "ediag_proxy.h"


#define DRV_MODULE_VERSION	"0.0.1"
#define DRV_MODULE_RELDATE	"$DateTime: 2007/12/04 10:18:53 $"

#define DIAG_CHAR_DEV_NAME   "diag577xx"

/********************************** Globals ***********************************/
u16          dbg_trace_level_mparam = LV_FATAL;
extern u8    dbg_trace_level;
extern u32   dbg_code_path;
int          use_inta;

module_param(dbg_trace_level_mparam, ushort, 0);
MODULE_PARM_DESC(dbg_code_path, " Trace code path (CP_ALL by default)");

module_param(dbg_code_path, uint, 0);
MODULE_PARM_DESC(dbg_trace_level_mparam, " Trace level (LV_FATAL by default)");

module_param(use_inta, uint, 0);
MODULE_PARM_DESC(use_inta, " Use INT#x instead of MSI-X (FALSE by default)");



/******************************************* Static stuff *******************************/
/* Maximum supported concurrent lediag instances */
#define MAX_NUM_EDIAG_INST       16

static unsigned int diag_major = 0;
static struct file *lediag_filp[MAX_NUM_EDIAG_INST];
static int lediag_num;
struct semaphore lediag_filp_sema;

/****************************** Functions *******************************/

#if defined(CONFIG_COMPAT) && (LINUX_VERSION_CODE < 0x2060c)
int evst_compat_ioctl(unsigned int fd, unsigned int cmd,
            unsigned long arg, struct file *filp);
#else
long evst_compat_ioctl (struct file * filp, unsigned cmd, unsigned long arg);
#endif

int evst_ioctl32_register(void);
void evst_ioctl32_unregister(void);

/* MM initialization function */
void mm_init(void);
void mm_cleanup(void);
void os_if_cleanup(void);
void os_if_init(void);

static int evst_inc_mod_usage(void)
{
    int rc = 0;

#if (LINUX_VERSION_CODE >= 0x20600)
    if (!try_module_get(THIS_MODULE)) 
    {  
        rc = -1;
    }
#else
    MOD_INC_USE_COUNT;
#endif
    return rc;
}

void
evst_dec_mod_usage(
    void
    )
{
#if (LINUX_VERSION_CODE >= 0x20600)
    module_put(THIS_MODULE);
#else
    MOD_DEC_USE_COUNT;
#endif
}

static int evst_fasync(int fd, struct file *filep, int mode)
{
	struct edrv_dev *pdev = filep->private_data;

	return fasync_helper(fd, filep, mode, &pdev->async_queue);
}

static int evst_push_filp(struct file *filp)
{
	int i;

	down(&lediag_filp_sema);
	for (i = 0; i < MAX_NUM_EDIAG_INST; i++) {
		if (!lediag_filp[i]) {
			lediag_filp[i] = filp;
			lediag_num++;
			up(&lediag_filp_sema);
			return 0;
		}
	}
	up(&lediag_filp_sema);

	printk(KERN_ERR"Too many lediag instances. Must not be more than %d\n",
	       MAX_NUM_EDIAG_INST);
	return 1;
}

static void evst_pop_filp(struct file *filp)
{
	int i;

	down(&lediag_filp_sema);
	for (i = 0; i < MAX_NUM_EDIAG_INST; i++) {
		if (lediag_filp[i] == filp) {
			lediag_filp[i] = NULL;
			lediag_num--;
			up(&lediag_filp_sema);
			return;
		}
	}
	up(&lediag_filp_sema);
}

void evst_send_sigio_all(void)
{
	int i, cnt;
	
	down(&lediag_filp_sema);
	for (i = 0, cnt = 0; i < MAX_NUM_EDIAG_INST; i++) {
		if (lediag_filp[i]) {
			struct edrv_dev * edev = ediag_filp_priv(lediag_filp[i]);

			/* Signal to the application */
			if (edev->async_queue)
				ediag_kill_fasync_io(&edev->async_queue);

			/* Break if we signaled all known lediag instances */
			if (++cnt >= lediag_num) {
				up(&lediag_filp_sema);
				return;
			}
		}
	}
	up(&lediag_filp_sema);
}

/* linux device file ops registration */
static int evst_open(struct inode *inode, struct file *filp)
{	
	struct edrv_dev * pdev = NULL;
	int rc = evst_pci_scan(filp);
	if ( rc )
		return rc;

	/* Push filp to the list. Needed for DbgBreak */
	if (evst_push_filp(filp))
		return -EINVAL;

	pdev = kmalloc(sizeof(struct edrv_dev), GFP_KERNEL);
	if (! pdev) {
		printk(KERN_ERR"Failed to allocate private data\n");
		return -ENOMEM;
	}

	memset(pdev, 0, sizeof(struct edrv_dev));
	filp->private_data = pdev;

	/* Increase module usage count */
	evst_inc_mod_usage();

	return 0;
}


static int evst_close(struct inode *inode, struct file *filp)
{
	/* Disable DbgBreak for this lediag instance */
	evst_pop_filp(filp);

	/* Release dispatch tables of BRCM devices */
	evst_cleanup_devs();

	/* Remove the filp from async queue */
	evst_fasync(-1, filp, 0);

	/* Free edrv_dev */
	if (filp->private_data) {
		kfree(filp->private_data);
		filp->private_data = NULL;
	}

	/* Decrease module usage count */
	evst_dec_mod_usage();
        return 0;
}

static int evst_mmap (struct file *filp, struct vm_area_struct *vma)
{
	unsigned long size = vma->vm_end - vma->vm_start;

#if (LINUX_VERSION_CODE < 0x2060c)
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;

	DbgMessage3(NULL, VERBOSE, "Mapping at: start=0x%x, offset=0x%x size=%d\n",
		    vma->vm_start, offset, size);

	if (remap_page_range(vma, vma->vm_start, offset, size,
		    vma->vm_page_prot))
#else
	DbgMessage3(NULL, VERBOSE, "Mapping at: start=0x%x, offset=0x%x size=%d\n",
		    vma->vm_start, vma->vm_pgoff, size);
	if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, size,
		    vma->vm_page_prot))
#endif
	    return -EAGAIN;

	return 0;
}

/* #ifndef CONFIG_COMPAT
#error Compat not configured
#endif */

struct file_operations evst_fops = {
#if (LINUX_VERSION_CODE >= 0x20622)
	unlocked_ioctl:	evst_unlocked_ioctl,
#else
	ioctl:		evst_ioctl,
#endif
	open:		evst_open,
	release:	evst_close,
	fasync:		evst_fasync,
	mmap:		evst_mmap
};

static int __init evst_reg_chardev(void)
{
	int rc;

	//return pci_register_driver(&evst_pci_driver);
	/* Resister the major and accept dynamic number */
	rc = register_chrdev(diag_major, DIAG_CHAR_DEV_NAME, &evst_fops);

	if (rc < 0)
	{
		DbgMessage1(NULL, FATAL, "edrv: can't get major %d\n", diag_major);
		return -EAGAIN;
	} 

	if (diag_major == 0)
	{
		diag_major = rc;
	}
	
	
	DbgMessage1(NULL, INFORMi, "Registered character device with major: %d\n", diag_major);

	return 0;
}

static int __init evst_init(void)
{
	int rc;

	/* We need to define dbg_trace_level_mparam as ushort as on
	   older kernels there is no module_param for byte!!! :-@  */
	dbg_trace_level = (u8)dbg_trace_level_mparam;

	DbgMessage1(NULL, VERBOSEi, "evst_init\n");

#if (LINUX_VERSION_CODE < 0x20623)
	init_MUTEX(&lediag_filp_sema);
#else 
    sema_init(&lediag_filp_sema, 1);
#endif

	/* Init os_if: calibrate timers, init PCI devices cache */
	os_if_init();

	/* Init mm internals */
	mm_init();

	rc = evst_reg_chardev();

	return rc;
}

static void __exit evst_cleanup(void)
{
	if ( diag_major > 0 ) {
		unregister_chrdev(diag_major, DIAG_CHAR_DEV_NAME);
	}

	/* Release dispatch tables of BRCM devices */
	evst_cleanup_devs();

	/* Cleanup mm internals */
	mm_cleanup();

	/* Cleanup os_if resources */
	os_if_cleanup();
}

/************* Handle ********************************/
/**
 * 
 * @param work
 */
#if (LINUX_VERSION_CODE >= 0x20614)
void evst_handle_default_sb(struct work_struct *work)
{
	struct proxy_t *proxy = container_of(work, struct proxy_t, work.work);
#else
void evst_handle_default_sb(void *data)
{
	struct proxy_t *proxy = (struct proxy_t *)data;
#endif
	um_handle_default_sb(proxy);
} 

/**
 * 
 * @param work
 */
#if (LINUX_VERSION_CODE >= 0x20614)
void evst_handle_sb(struct work_struct *work)
{
	struct proxy_t *proxy = container_of(work, struct proxy_t, work.work);
#else
void evst_handle_sb(void *data) 
{
	struct proxy_t *proxy = (struct proxy_t *)data;
#endif
	um_handle_sb(proxy, proxy->sb_id);
}

/**
 * 
 */
#if (LINUX_VERSION_CODE >= 0x20614)
void evst_handle_timer(struct work_struct *work)
{
	struct proxy_t *proxy = container_of(work, struct proxy_t, work.work);
#else
void evst_handle_timer(void *data)
{
	struct proxy_t *proxy = (struct proxy_t *)data;
#endif
	um_linux_timer(proxy);
} /* evst_handle_timer */

/************* Attach ********************************/
/**
 * 
 * @param proxy
 */
void evst_attach_def_sb_work(
	struct proxy_t     * proxy
	)
{
#if (LINUX_VERSION_CODE >= 0x20614)
	INIT_DELAYED_WORK(&proxy->work, evst_handle_default_sb);
#else
	INIT_WORK(&proxy->work, evst_handle_default_sb, proxy);
#endif
}

/**
 * 
 * @param proxy
 * @param sb_idx
 */
void evst_attach_sb_work( struct proxy_t *proxy, int sb_idx)
{
	proxy->sb_id = sb_idx;
	smp_wmb();
#if (LINUX_VERSION_CODE >= 0x20614)
	INIT_DELAYED_WORK(&proxy->work, evst_handle_sb);
#else
	INIT_WORK(&proxy->work, evst_handle_sb, proxy);
#endif

}

/**
 * 
 * @param proxy
 */
void evst_attach_timer_work(
	struct proxy_t     * proxy
	)
{
#if (LINUX_VERSION_CODE >= 0x20614)
	INIT_DELAYED_WORK(&proxy->work, evst_handle_timer);
#else
	INIT_WORK(&proxy->work, evst_handle_timer, proxy);
#endif
}

/************** Proxy ************************************************/
struct proxy_t * evst_timer_proxy(struct proxy_t *base_proxy)
{
	return base_proxy + 0;
}

struct proxy_t * evst_def_sb_proxy(struct proxy_t *base_proxy)
{
	return base_proxy + 1;
}

/** Here we assume that proxy[i] corresponds to sb_id=i. This
 *  assumption will simplify the implementation in cases when
 *  different functions have different maximum SB number
 *  (e.g. PF vs VF). */
struct proxy_t * evst_sb_proxy(struct proxy_t *base_proxy, int sb_id)
{
	return base_proxy + 2 + sb_id;
}

void evst_set_proxy_name(struct proxy_t *p, const char *p_name)
{
	strncpy(p->name, p_name, PROXY_NAME_LEN);
}

const char * evst_get_proxy_name(struct proxy_t *p)
{
	return p->name;
}
/*********************************************************************/

module_init(evst_init);
module_exit(evst_cleanup);

MODULE_LICENSE("Proprietary");
