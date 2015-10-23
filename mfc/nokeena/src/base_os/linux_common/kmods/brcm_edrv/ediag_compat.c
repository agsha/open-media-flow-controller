#include <linux/delay.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/vmalloc.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <linux/ctype.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/gfp.h>
#include <linux/crc32.h>

#include <asm/byteorder.h>
#include <asm/uaccess.h>
#if (LINUX_VERSION_CODE < 0x2061b)
#include <asm/semaphore.h>
#else
#include <linux/semaphore.h>
#endif
#include <asm/atomic.h>
#if !(defined(IA64) || defined(POWER_PC))
#include <asm/msr.h>
#endif
#include <asm/page.h>
#include <asm/io.h>


#include "ediag_compat.h"
#include "ediag_drv.h"
#include "bcmtype.h"
#include "debug.h"
#include "ediag_proxy.h"

#ifndef rdtsc
#ifdef IA64
#define rdtsc(low, high) \
	do { \
		unsigned long result; \
					\
		/* gcc-IA64 version */ \
		 __asm__ __volatile__("mov %0=ar.itc" : "=r"(result) :: "memory"); \
							\
		 while (__builtin_expect ((int) result == -1, 0)) \
			 __asm__ __volatile__("mov %0=ar.itc" : "=r"(result) :: \
			 "memory"); \
					\
		 low = result & 0xffffffff; \
		 high = (result >> 32) & 0xffffffff; \
	} while (0)
#elif defined(POWER_PC)
#define rdtsc(low, high) \
	do { \
		unsigned long int upper, lower,tmp; \
		__asm__ volatile(\
			"0:     	     \n"\
			"\tmftbu   %0   	\n"\
			"\tmftb    %1   	\n"\
			"\tmftbu   %2   	\n"\
			"\tcmpw    %2,%0	\n"\
			"\tbne     0b         \n"\
			: "=r"(upper),"=r"(lower),"=r"(tmp)\
		); \
		low = lower; \
		high = upper; \
	} while(0)
#else /* x86 */
#define rdtsc(low,high) \
     __asm__ __volatile__("rdtsc" : "=a" (low), "=d" (high))
#endif
#endif /* rdtsc */

/* In older versions there is only PCI_DEV_ENABLED/DISABLED state,
   no ref count, hence we can only check if any other device
   has been loaded and remember it until the exnt of the execution
   (until removing/inserting the kernel medule) */
#if (LINUX_VERSION_CODE < 0x20614)
static int other_drv_is_up = 0;
#endif


void ediag_set_proxy_host(struct proxy_t *proxy, void *host)
{
	proxy->host = host;
}

/**
 * Allocate an array of proxies
 * @param cnt number of proxies to allocate
 *
 * @return pointer to the array of allocate proxies or NULL in
 *         case of failure
 */
struct proxy_t * ediag_alloc_proxy(int cnt)
{
	struct proxy_t * proxy = (struct proxy_t*)ediag_kmalloc_kernel(cnt*sizeof(struct proxy_t));

	if (proxy == NULL) {
		return NULL;
	}

	ediag_memset(proxy, 0, cnt*sizeof(struct proxy_t));

	return proxy;
}

void ediag_free_proxy(struct proxy_t * proxy)
{
	ediag_kfree(proxy);
}


void * ediag_get_work_offset(struct proxy_t * proxy)
{
	return &proxy->work;
}

int ediag_msix_capable(void)
{
#ifdef CONFIG_PCI_MSI
	return 1;
#else
	return 0;
#endif
}

/* Real ISR */
u32_t um_linux_isr_handler(void *arg);
u32_t um_linux_isr_handler_igu_tmode(void *arg);

u32_t um_msix_sb_int(void *arg, int sb_id);
u32_t um_msix_sb_int_igu_tmode(void *arg, int sb_id);

u32_t um_msix_def_sb_int(void *arg);
u32_t um_msix_def_sb_int_igu_tmode(void *arg);

#if (LINUX_VERSION_CODE < 0x20613)
irqreturn_t ediag_msix_def_sb_int(int irq, void * data, struct pt_regs * regs)
#else
irqreturn_t ediag_msix_def_sb_int(int irq, void * data)
#endif
{
	struct proxy_t *proxy = (struct proxy_t *)data;

	return um_msix_def_sb_int(proxy->host) != 0 ? IRQ_HANDLED : IRQ_NONE;
}

#if (LINUX_VERSION_CODE < 0x20613)
irqreturn_t ediag_msix_def_sb_int_igu_tmode(int irq, void * data, struct pt_regs * regs)
#else
irqreturn_t ediag_msix_def_sb_int_igu_tmode(int irq, void * data)
#endif
{
	struct proxy_t *proxy = (struct proxy_t *)data;

	return um_msix_def_sb_int_igu_tmode(proxy->host) != 0 ? IRQ_HANDLED : IRQ_NONE;
}

#if (LINUX_VERSION_CODE < 0x20613)
irqreturn_t ediag_msix_fp_int(int irq, void * data, struct pt_regs * regs)
#else
irqreturn_t ediag_msix_fp_int(int irq, void * data)
#endif
{
	struct proxy_t *proxy = (struct proxy_t *)data;

	return um_msix_sb_int(proxy->host, proxy->sb_id) != 0 ? IRQ_HANDLED : IRQ_NONE;
}

#if (LINUX_VERSION_CODE < 0x20613)
irqreturn_t ediag_msix_fp_int_igu_tmode(int irq, void * data, struct pt_regs * regs)
#else
irqreturn_t ediag_msix_fp_int_igu_tmode(int irq, void * data)
#endif
{
	struct proxy_t *proxy = (struct proxy_t *)data;

	return um_msix_sb_int_igu_tmode(proxy->host, proxy->sb_id) != 0 ? IRQ_HANDLED : IRQ_NONE;
}

#if (LINUX_VERSION_CODE < 0x20613)
irqreturn_t ediag_linux_irq(int irq, void * arg, struct pt_regs * regs)
#else
irqreturn_t ediag_linux_irq(int irq, void * arg)
#endif
{
	return um_linux_isr_handler(arg) != 0 ? IRQ_HANDLED : IRQ_NONE;
}

#if (LINUX_VERSION_CODE < 0x20613)
irqreturn_t ediag_linux_irq_igu_tmode(int irq, void * arg, struct pt_regs * regs)
#else
irqreturn_t ediag_linux_irq_igu_tmode(int irq, void * arg)
#endif
{
	return um_linux_isr_handler_igu_tmode(arg) != 0 ? IRQ_HANDLED : IRQ_NONE;
}

int ediag_request_msix_irq_fp(
	struct msix_entry *msix_ent,
	const char * device,
	void * data
	)
{
	return request_irq(msix_ent->vector, ediag_msix_fp_int, 0, device, data);
}

int ediag_request_msix_irq_fp_igu_tmode(
	struct msix_entry *msix_ent,
	const char * device,
	void * data
	)
{
	return request_irq(msix_ent->vector, ediag_msix_fp_int_igu_tmode, 0, device, data);
}


int ediag_request_msix_irq_def_sb(
	struct msix_entry *msix_ent,
	const char * device,
	void * data
	)
{
	return request_irq(msix_ent->vector, ediag_msix_def_sb_int, 0, device, data);
}

int ediag_request_msix_irq_def_sb_igu_tmode(
	struct msix_entry *msix_ent,
	const char * device,
	void * data
	)
{
	return request_irq(msix_ent->vector, ediag_msix_def_sb_int_igu_tmode, 0, device, data);
}

int ediag_get_msix_entry_size(void)
{
	return sizeof(struct msix_entry);
}


int ediag_request_irq(
	unsigned int irq_num,
	const char * device,
	void * data
	)
{
	return request_irq(irq_num, ediag_linux_irq, IRQF_SHARED, device, data);
}

int ediag_request_irq_igu_tmode(
	unsigned int irq_num,
	const char * device,
	void * data
	)
{
	return request_irq(irq_num, ediag_linux_irq_igu_tmode, IRQF_SHARED, device, data);
}

unsigned int ediag_msix_vec(struct msix_entry *entry)
{
	return entry->vector;
}

void ediag_msix_ent_set(struct msix_entry *entry, u16_t val)
{
	entry->entry = val;
}

struct msix_entry *ediag_msix_entry(struct msix_entry *base_entry, int idx)
{
	return base_entry + idx;
}

void ediag_spin_lock_free(spinlock_t * lock)
{
	kfree(lock);
}

void ediag_spin_lock_init(spinlock_t ** lock)
{
	*lock = (spinlock_t*)ediag_kmalloc_kernel(sizeof(spinlock_t));

	spin_lock_init(*lock);
}

unsigned long ediag_HZ(void)
{
	return HZ;
}

void ediag_schedule(void)
{
	schedule();
}

void ediag_msleep(unsigned int msec)
{
#if (LINUX_VERSION_CODE < 0x20607)
	current->state = TASK_UNINTERRUPTIBLE;
	schedule_timeout((HZ * msec) / 1000);
#else
	msleep(msec);
#endif
}

u64_t ediag_rdtsc(void)
{
	u32_t low, high;

	rdtsc(low,high);

	return (((u64_t)high << 32) | (u64_t)low);
}

unsigned long ediag_msleep_interruptible(unsigned int msec)
{
#if (LINUX_VERSION_CODE < 0x20609)
	unsigned long timeout = (HZ * msec) / 1000 + 1;

	current->state = TASK_INTERRUPTIBLE;
	return (schedule_timeout(timeout) * 1000) / HZ;
#else
	return msleep_interruptible(msec);
#endif
}

void ediag_udelay(unsigned long usecs)
{
	udelay(usecs);
}

unsigned long ediag_jiffies(void)
{
	return jiffies;
}


static void *ediag_alloc_sema(void)
{
	return ediag_kmalloc_kernel(sizeof(struct semaphore));
}

void ediag_init_mutex(struct semaphore ** sem)
{
	*sem = ediag_alloc_sema();
#if (LINUX_VERSION_CODE < 0x20623)
	init_MUTEX(*sem);
#else
    sema_init(*sem, 1);
#endif

}

void ediag_free_sema(struct semaphore * sem)
{
	kfree(sem);
}

dma_addr_t ediag_pci_map_single_to(struct pci_dev *pdev, void * buf, size_t size)
{
	return pci_map_single(pdev, buf, size, PCI_DMA_TODEVICE);
}

void ediag_pci_unmap_single_to(struct pci_dev *pdev, dma_addr_t buf, size_t size)
{
	return pci_unmap_single(pdev, buf, size, PCI_DMA_TODEVICE);
}

int ediag_dma_mapping_error(struct pci_dev *pdev, dma_addr_t mapping)
{
#if (LINUX_VERSION_CODE < 0x2061b)
	return dma_mapping_error(mapping);
#else
	return dma_mapping_error(&pdev->dev, mapping);
#endif
}

struct pci_dev * ediag_pci_get_slot(struct pci_bus * bus,
				    unsigned int devfn)
{
	return pci_get_slot(bus, devfn);
}

struct pci_bus * ediag_pci_find_bus(int domain, int bus_number)
{
	return pci_find_bus(domain, bus_number);
}

struct pci_dev * ediag_pci_find_device_any(struct pci_dev *from)
{
#if (LINUX_VERSION_CODE < 0x20615)
	return pci_find_device(PCI_ANY_ID, PCI_ANY_ID, from);
#else
	return pci_get_device(PCI_ANY_ID, PCI_ANY_ID, from);
#endif
}

int ediag_get_domain_num(struct pci_dev * dev)
{
	return pci_domain_nr(dev->bus);
}

unsigned char ediag_get_bus_num(struct pci_bus * bus)
{
	return bus->number;
}

void * ediag_pci_resource_start(struct pci_dev * pci_dev, int n)
{
	return (void*)((int_ptr_t)pci_resource_start(pci_dev, n));
}

unsigned long ediag_pci_resource_len(struct pci_dev * pci_dev, int n)
{
	return pci_resource_len(pci_dev, n);
}

void ediag_ret_pci_dev_hdl(struct pci_dev * pci_dev)
{
	pci_dev_put(pci_dev);
}

/**
   Sets power state to the requested value:
	EDIAG_PM_D0_STATE_VAL - D0,
	EDIAG_PM_D3_STATE_VAL - D3hot

*  Remark: This should be lm-function.
*  @return 0 in case of success, -EACCES if there is no power
*          management capability for this device, -EINVAL if
*          requested power state is unlnown.
*/
int ediag_set_power_state(
	struct pci_dev *pdev,
	int  power_state)
{
	u16 val;
	int pm_cap;

	if ((pm_cap = pci_find_capability(pdev, PCI_CAP_ID_PM)) == 0) {
		DbgMessage(NULL, FATAL, "Cannot find power management"
		       " capability, aborting\n");
		return -EACCES;
	}

	/* Set the device to D0 state.  If a device is already in D3 state,
	 * we will not be able to read the PCICFG_PM_CSR register using the
	 * PCI memory command, we need to use config access here. */
	pci_read_config_word(pdev, pm_cap + PCI_PM_CTRL, &val);

	switch (power_state) {
	case EDIAG_PM_D0_STATE:
		/* Set the device to D0 state.  This may be already done by the OS. */
		pci_write_config_word(pdev, pm_cap + PCI_PM_CTRL,
		    (val & ~PCI_PM_CTRL_STATE_MASK) |
		    PCI_PM_CTRL_PME_STATUS  | EDIAG_PM_D0_STATE);

		if (val & PCI_PM_CTRL_STATE_MASK)
		/* delay required during transition out of D3hot */
			msleep(20);

		break;
	case EDIAG_PM_D3_STATE:
		/* device can be changed to D3 state only if another driver is not up */
#if (LINUX_VERSION_CODE >= 0x20614)
		if ( atomic_read(&pdev->enable_cnt) == 1 ) {
#else
		if ( ! other_drv_is_up ) {
#endif
			/* Set the device to D3 state. */
			val &= ~PCI_PM_CTRL_STATE_MASK;
			val |= EDIAG_PM_D3_STATE;

			pci_write_config_word(pdev, pm_cap + PCI_PM_CTRL, val);
		}
		break;
	default:
		DbgMessage(pdev, WARNi, "Unknown power state: %d\n", power_state);
		return -EINVAL;
	}


	return 0;
}

#if (LINUX_VERSION_CODE < 0x2061f)
#define EDIAG_DMA_64BIT_MASK DMA_64BIT_MASK
#define EDIAG_DMA_32BIT_MASK DMA_32BIT_MASK
#else
#define EDIAG_DMA_64BIT_MASK DMA_BIT_MASK(64)
#define EDIAG_DMA_32BIT_MASK DMA_BIT_MASK(32)
#endif

int ediag_pci_enable_msix(struct pci_dev *dev,
			  struct msix_entry *entries, int nvec)
{
#ifdef CONFIG_PCI_MSI
	return pci_enable_msix(dev, entries, nvec);
#else
	return -EINVAL;
#endif
}

void ediag_pci_disable_msix(struct pci_dev * dev)
{
#ifdef CONFIG_PCI_MSI
	pci_disable_msix(dev);
#endif
}

int ediag_init_pci_dev(struct pci_dev *pdev)
{
	int rc = 0;

#if (LINUX_VERSION_CODE < 0x20614)
	if ( pdev->is_busmaster ) {
		DbgMessage(NULL, INFORMi, "WARNING: Another driver is up!\n");
		other_drv_is_up = 1;
	}
#endif

	rc = pci_enable_device(pdev);
	if (rc) {
		DbgMessage(NULL, FATAL, "Cannot enable PCI device, aborting\n");
		goto err_out;
	}

	if (!(pci_resource_flags(pdev, 0) & IORESOURCE_MEM)) {
		DbgMessage(NULL, FATAL, "Cannot find PCI device base address,"
		       " aborting\n");
		rc = -ENODEV;
		goto err_out_disable;
	}
#if (defined(CONFIG_PCI_IOV) && defined(VF_INVOLVED))
	if (!pdev->is_virtfn)
#endif
		if (!(pci_resource_flags(pdev, 2) & IORESOURCE_MEM)) {
			DbgMessage(NULL, FATAL, "Cannot find second PCI device"
			       " base address, aborting\n");
			rc = -ENODEV;
			goto err_out_disable;
		}

	/* Don't request regions if device has already been enabled  */
#if (LINUX_VERSION_CODE >= 0x20614)
	if ( atomic_read(&pdev->enable_cnt) == 1 ) {
#else
	if ( ! other_drv_is_up ) {
#endif
		rc = pci_request_regions(pdev, DRV_MODULE_NAME);
		if (rc) {
			DbgMessage(NULL, FATAL,"Cannot obtain PCI resources,"
			       " aborting\n");
			goto err_out_disable;
		}

		pci_set_master(pdev);
	}
#ifdef CONFIG_PCI_IOV
	if (!pdev->is_virtfn)
#endif
		if (pci_find_capability(pdev, PCI_CAP_ID_EXP) == 0) {
			DbgMessage(NULL, FATAL, "Cannot find PCI Express capability,"
			       " aborting\n");
			rc = -EIO;
			goto err_out_release;
		}

	if (pci_set_dma_mask(pdev, EDIAG_DMA_64BIT_MASK) == 0) {
		if (pci_set_consistent_dma_mask(pdev, EDIAG_DMA_64BIT_MASK) != 0) {
			DbgMessage(NULL, FATAL, "pci_set_consistent_dma_mask"
			       " failed, aborting\n");
			rc = -EIO;
			goto err_out_release;
		}

	} else if (pci_set_dma_mask(pdev, EDIAG_DMA_32BIT_MASK) != 0) {
		DbgMessage(NULL, FATAL, "System does not support DMA,"
		       " aborting\n");
		rc = -EIO;
		goto err_out_release;
	} else if (pci_set_consistent_dma_mask(pdev, EDIAG_DMA_32BIT_MASK) != 0) {
		DbgMessage(NULL, FATAL, "pci_set_consistent_dma_mask"
			   " failed, aborting\n");
		rc = -EIO;
		goto err_out_release;
	}



	return 0;


err_out_release:
#if (LINUX_VERSION_CODE >= 0x20614)
	if (atomic_read(&pdev->enable_cnt) == 1) {
#else
	if ( ! other_drv_is_up ) {
#endif
		pci_release_regions(pdev);
#if (LINUX_VERSION_CODE < 0x20614)
		pci_disable_device(pdev);
#endif
	}

err_out_disable:
#if (LINUX_VERSION_CODE >= 0x20614)
	pci_disable_device(pdev);
#endif

err_out:
	return rc;
}

void ediag_release_pci_dev(struct pci_dev * pdev)
{
#if (LINUX_VERSION_CODE >= 0x20614)
	/* If we are the last driver */
	if (atomic_read(&pdev->enable_cnt) == 1) {
#else
	if ( ! other_drv_is_up ) {
#endif
		pci_release_regions(pdev);
/* As in older kernels there is only a binary PCI device enabled/disabled
   state we mustn't disable the device if it was not us who had enabled it. */
#if (LINUX_VERSION_CODE < 0x20614)
		pci_disable_device(pdev);
#endif
	}
#if (LINUX_VERSION_CODE >= 0x20614)
	pci_disable_device(pdev);
#endif
}

int ediag_put_user32(u32_t val, u32_t __user * ptr)
{
	return put_user(val, ptr);
}

int ediag_put_user16(u16_t val, u16_t __user * ptr)
{
	return put_user(val, ptr);
}

int ediag_put_user8(u8_t val, u8_t __user * ptr)
{
	return put_user(val, ptr);
}

unsigned long ediag_copy_from_user(void *to,const void __user *from,
				   unsigned long n)
{
	return copy_from_user(to,from,n);
}

unsigned long ediag_copy_to_user(void __user *to,
				const void *from, unsigned long n)
{
	return copy_to_user(to,from,n);
}

void ediag_synchronize_irq(unsigned int irq)
{
	synchronize_irq(irq);
}

int ediag_in_atomic(void)
{
	return in_atomic();
}

void ediag_free_irq(unsigned int irq, void *dev_id)
{
	free_irq(irq, dev_id);
}

int ediag_mod_timer(struct timer_list *timer, unsigned long expires)
{
	return mod_timer(timer, expires);
}

void * ediag_alloc_timer(void)
{
	return ediag_kmalloc_kernel(sizeof(struct timer_list));
}

void ediag_del_timer_sync(struct timer_list * timer)
{
	del_timer_sync(timer);
}

void ediag_free_timer(void *timer)
{
	kfree(timer);
}

int ediag_pci_read_config_byte(struct pci_dev *dev, int where, u8 *val)
{
	return pci_read_config_byte(dev, where, val);
}
int ediag_pci_read_config_word(struct pci_dev *dev, int where, u16 *val)
{
	return pci_read_config_word(dev, where, val);
}

int ediag_pci_read_config_dword(struct pci_dev *dev, int where, u32 *val)
{
	return pci_read_config_dword(dev, where, val);
}

int ediag_pci_write_config_byte(struct pci_dev *dev, int where, u8 val)
{
	return pci_write_config_byte(dev, where, val);
}
int ediag_pci_write_config_word(struct pci_dev *dev, int where, u16 val)
{
	return pci_write_config_word(dev, where, val);
}
int ediag_pci_write_config_dword(struct pci_dev *dev, int where, u32 val)
{
	return pci_write_config_dword(dev, where, val);
}

unsigned int ediag_get_irq_num(struct pci_dev * pdev)
{
	return pdev->irq;
}

/* Returns TRUE is there is more than one PCI drivers */
int ediag_other_drv_up(struct pci_dev * pdev)
{
#if (LINUX_VERSION_CODE >= 0x20614)
	return (atomic_read(&pdev->enable_cnt) > 1);
#else
	return other_drv_is_up;
#endif
}

#if (LINUX_VERSION_CODE < 0x20609)
u8   ediag_readb(const volatile void __iomem *addr)
{
	return readb(addr);
}

u16  ediag_readw(const volatile void __iomem *addr)
{
	return readw(addr);
}

u32  ediag_readl(const volatile void __iomem *addr)
{
	return readl(addr);
}

void ediag_writeb(u8 b, volatile void __iomem *addr)
{
	writeb(b, addr);
}

void ediag_writew(u16 b, volatile void __iomem *addr)
{
	writew(b, addr);
}

void ediag_writel(u32 b, volatile void __iomem *addr)
{
	writel(b, addr);
}

#else
u8   ediag_readb(const volatile void __iomem *addr)
{
	return ioread8((void __iomem *)addr);
}

u16  ediag_readw(const volatile void __iomem *addr)
{
	return ioread16((void __iomem *)addr);
}

u32  ediag_readl(const volatile void __iomem *addr)
{
	return ioread32((void __iomem *)addr);
}

void ediag_writeb(u8 b, volatile void __iomem *addr)
{
	iowrite8(b, (void __iomem *)addr);
}

void ediag_writew(u16 b, volatile void __iomem *addr)
{
	iowrite16(b, (void __iomem *)addr);
}

void ediag_writel(u32 b, volatile void __iomem *addr)
{
	iowrite32(b, (void __iomem *)addr);
}

#endif

u64  ediag_readq(const volatile void __iomem *addr)
{
	u64 tmp = 0;

#ifdef X86_64
    tmp = *(volatile u64_t*)addr;
#else
	tmp = ediag_readl(addr);
	barrier();
	*((u32*)&tmp + 1) = ediag_readl((u32*)addr + 1);
#endif
	return tmp;
}

void ediag_writeq(u64 b, volatile void __iomem *addr)
{

	ediag_writel(b & 0xffffffff, addr);
	barrier();
	ediag_writel((u64) (b >> 32) & 0xffffffff, (u32*)addr + 1);
	wmb();
}


void ediag_pci_free_consistent(struct pci_dev *dev, size_t sz, void * vaddr, u64_t dma_addr)
{
	pci_free_consistent(dev, sz, vaddr, dma_addr);
}



void* ediag_pci_alloc_consistent(struct pci_dev *dev, size_t sz, u64_t *dma_addr)
{
	dma_addr_t tmp_dma_addr;
	void *res;

    res = pci_alloc_consistent(dev, sz, &tmp_dma_addr);

	*dma_addr = tmp_dma_addr;
	return res;
}


static inline void *ediag_kmalloc(size_t size, int flags)
{
	if (unlikely(in_atomic())) {
		flags &= ~GFP_KERNEL;
		flags |= GFP_ATOMIC;
	}

	return kmalloc(size, flags);
}

void *ediag_kmalloc_atomic(size_t size) {
	return ediag_kmalloc(size, GFP_ATOMIC);
}

void *ediag_kmalloc_kernel(size_t size) {
	return ediag_kmalloc(size, GFP_KERNEL);
}

void ediag_kfree(void *block)
{
	kfree(block);
}

void *ediag_vmalloc(size_t size)
{
	WARN_ON(in_atomic());
	return vmalloc(size);
}

void ediag_vfree(void *addr)
{
	WARN_ON(in_atomic());
	vfree(addr);
}


int ediag_sema_down(struct semaphore * sema)
{
	might_sleep();
	return down_interruptible(sema);
}

void ediag_warn_on(int cond)
{
	WARN_ON(cond);
}

void ediag_sema_up(struct semaphore * sema)
{
	up(sema);
}

void ediag_mmiowb(void)
{
	mmiowb();
}

void ediag_rmb(void)
{
	smp_rmb();
}

void ediag_smp_mb(void)
{
	smp_mb();
}

void ediag_wmb(void)
{
	smp_wmb();
}

void ediag_atomic_set(s32_t *a, int i)
{
	atomic_set((atomic_t*)a, i);
}

int ediag_atomic_dec_and_test(s32_t *v)
{
	return atomic_dec_and_test((atomic_t*)v);
}

int ediag_atomic_inc_and_test(s32_t *v)
{
	return atomic_inc_and_test((atomic_t*)v);
}

int ediag_atomic_read(s32_t *v)
{
	return atomic_read((atomic_t*)v);
}

int ediag_atomic_long_read(unsigned long *v)
{
#if ((defined X86_64) || (defined IA64) || (defined PPC64))
	return atomic_long_read((atomic64_t*)v);
#else
    return atomic_read((atomic_t*)v);
#endif
}


int ediag_atomic_cmpxchg(s32_t *v, int cmp, int new_v)
{
	return atomic_cmpxchg((atomic_t*)v, cmp, new_v);
}

struct resource * ediag_request_region(resource_size_t start,
				       resource_size_t n, const char *name)
{
	return request_region(start, n, name);
}

void ediag_release_region(resource_size_t start, resource_size_t n)
{
	release_region(start, n);
}

unsigned ediag_inb(unsigned port)
{
	return inb(port);
}

unsigned ediag_inw(unsigned port)
{
	return inw(port);
}

unsigned ediag_inl(unsigned port)
{
	return inl(port);
}

void ediag_outb(u8_t byte, unsigned port)
{
	outb(byte, port);
}

void ediag_outw(u16_t word, unsigned port)
{
	outw(word, port);
}

void ediag_outl(u32_t dword, unsigned port)
{
	outl(dword, port);
}

void * ediag_memset(void * buf, int c, size_t sz)
{
	return memset(buf, c, sz);
}

char * ediag_strncpy(char *t, const char *s, size_t n)
{
	return strncpy(t, s, n);
}

void *ediag_memcpy(void *dest, const void *src, size_t count)
{
	return memcpy(dest, src, count);
}

int ediag_memcmp(const void *s1, const void *s2, size_t n)
{
	return memcmp(s1, s2, n);
}

int ediag_strcasecmp(const char *s1, const char *s2)
{
#if (LINUX_VERSION_CODE < 0x20616)  || (!defined(__HAVE_ARCH_STRCASECMP))

	int c1, c2;

	do {
		c1 = tolower(*s1++);
		c2 = tolower(*s2++);
	} while (c1 == c2 && c1 != 0);
	return c1 - c2;
#else
	return strcasecmp(s1, s2);
#endif
}

int ediag_strcmp(const char *a, const char *b)
{
	return strcmp(a, b);
}

char *ediag_strrchr(char *a, int n)
{
	return strrchr(a, n);
}

void __iomem* ediag_ioremap_nocache(resource_size_t offset, unsigned long size)
{
	return ioremap_nocache(offset, size);
}

void __iomem* ediag_ioremap_wc(resource_size_t offset, unsigned long size)
{
    return 0;
    // For write-combined un-remark this line
    //return ioremap_wc(offset, size);
    
}

void ediag_iounmap(void __iomem *addr)
{
	iounmap(addr);
}

void * ediag_filp_priv(struct file *filp)
{
	return filp->private_data;
}

void ediag_kill_fasync_io(struct fasync_struct **fp)
{
	kill_fasync(fp, SIGIO, POLL_PRI);
}

unsigned long ediag_virt_to_phys(void * virt)
{
	return virt_to_phys(virt);
}

void * ediag_phys_to_virt(unsigned long phys)
{
	return phys_to_virt(phys);
}

struct page* ediag_virt_to_page(void* addr)
{
	return virt_to_page(addr);
}


void ediag_cancel_work_sync(void * work)
{
#if (LINUX_VERSION_CODE >= 0x20614)
	cancel_delayed_work((struct delayed_work*)work);
#else
	cancel_delayed_work((struct work_struct*)work);
#endif
	barrier();
	flush_scheduled_work();
}

void ediag_flush_workqueue(void)
{
	flush_scheduled_work();
}


int ediag_queue_work(void *t)
{
#if (LINUX_VERSION_CODE >= 0x20614)
	return schedule_delayed_work((struct delayed_work *)t, 0);
#else
	return schedule_work((struct work_struct*)t);
#endif
}

int ediag_queue_delayed_work(struct proxy_t *proxy, unsigned long delay)
{
	return schedule_delayed_work(&proxy->work, delay);
}

void ediag_SetPageReserved(struct page* page)
{
	SetPageReserved(page);
}

void ediag_ClearPageReserved(struct page* page)
{
	ClearPageReserved(page);
}

u16_t ediag_cpu_to_le16(u16_t val)
{
	return cpu_to_le16(val);
}

u32_t ediag_cpu_to_le32(u32_t val)
{
	return cpu_to_le32(val);
}

u16_t ediag_le16_to_cpu(u16_t val)
{
	return le16_to_cpu(val);
}

u32_t ediag_le32_to_cpu(u32_t val)
{
	return le32_to_cpu(val);
}

u32_t ediag_be32_to_cpu(u32_t val)
{
	return be32_to_cpu(val);
}

u32_t ediag_be16_to_cpu(u32_t val)
{
	return be16_to_cpu(val);
}

u32_t ediag_cpu_to_be32(u32_t val)
{
    return cpu_to_be32(val);
}

u32_t ediag_cpu_to_be16(u32_t val)
{
    return cpu_to_be16(val);
}

u32_t ediag_crc32_le(unsigned char const *p, size_t len)
{
	return ether_crc_le(len, p);
}

u64_t ediag_do_div(u64_t n, u32_t base)
{
	do_div(n, base);
	return n;
}

int ediag_rss_enabled(void)
{
#if (LINUX_VERSION_CODE >= 0x020618)
	return 1;
#else
	return 0;
#endif
}

int ediag_pci_enable_sriov(struct pci_dev *pdev, u16_t vfs_num)
{
#if (defined(CONFIG_PCI_IOV) && defined(VF_INVOLVED))
	return pci_enable_sriov(pdev, vfs_num);
#else
	return -EINVAL;
#endif
}

void ediag_pci_disable_sriov(struct pci_dev *pdev)
{
#if defined(CONFIG_PCI_IOV) && defined(VF_INVOLVED)
	pci_disable_sriov(pdev);
#endif
}

int ediag_pci_find_sriov_capability(struct pci_dev *pdev)
{
#if defined(CONFIG_PCI_IOV) && defined(VF_INVOLVED)
	return pci_find_ext_capability(pdev, PCI_EXT_CAP_ID_SRIOV);
#else
	return 0;
#endif
}

int ediag_check_flr_cap(struct pci_dev *pdev)
{
#ifdef PCI_EXP_DEVCAP_FLR
	int exppos = pci_find_capability(pdev, PCI_CAP_ID_EXP);
	u32_t cap;

	if (!exppos)
		return -ENOTTY;

	pci_read_config_dword(pdev, exppos + PCI_EXP_DEVCAP, &cap);
	if (!(cap & PCI_EXP_DEVCAP_FLR))
		return -ENOTTY;

	return 0;
#else
	return -ENOTTY;
#endif
}


int ediag_pci_find_capability(struct pci_dev *pdev, int cap)
{
	return pci_find_capability(pdev, cap);
}

int ediag_pci_save_state(struct pci_dev *pdev)
{

#if (LINUX_VERSION_CODE <= 0x020609)
	return pci_save_state(pdev, NULL);
#else
	return pci_save_state(pdev);
#endif
}

int ediag_pci_restore_state(struct pci_dev *pdev)
{
#if (LINUX_VERSION_CODE <= 0x020609)
	return pci_restore_state(pdev, NULL);
#elif (LINUX_VERSION_CODE <= 0x020625)
	return pci_restore_state(pdev);
#else
	pci_restore_state(pdev);
	return 0;
#endif
}

int ediag_pci_execute_reset_function(struct pci_dev *pdev)
{
#ifdef VF_INVOLVED
#if (LINUX_VERSION_CODE >= 0x02061F)
	return __pci_reset_function(pdev);
#elif (LINUX_VERSION_CODE >= 0x02061C)
	return pci_execute_reset_function(pdev);
#else
	return -ENOTTY;
#endif
#else
	return -ENOTTY;
#endif
}

int ediag_get_sriov_bar(struct pci_dev *pdev, u64_t *bar, u32_t *size, int bn)
{
#ifdef CONFIG_PCI_IOV
	*bar  = pci_resource_start(pdev, PCI_IOV_RESOURCES + bn*2);
	if (!(*bar))
		return -EINVAL;

	*size = pci_resource_len(pdev, PCI_IOV_RESOURCES + bn*2);
	if (!(*size))
		return -EINVAL;

	return 0;
#else
	return -EINVAL;
#endif
}

void ediag_disable_irq(struct pci_dev *pdev)
{
	disable_irq(ediag_get_irq_num(pdev));
}
void ediag_enable_irq(struct pci_dev *pdev)
{
	enable_irq(ediag_get_irq_num(pdev));
}

void ediag_might_sleep(void)
{
	might_sleep();
}
