#ifndef __EDIAG_COMPAT_H__
#define __EDIAG_COMPAT_H__

#include "bcmtype.h"

#define EDIAG_PM_D0_STATE         0    
#define EDIAG_PM_D3_STATE         3    


#ifndef IRQF_SHARED
#define IRQF_SHARED			SA_SHIRQ
#endif

/* Forward declaration - we want to hide their real implementation */
struct proxy_t;
struct pci_dev;
struct pci_bus;
struct pci_dev;
struct semaphore;
struct file;
struct fasync_struct;
struct msix_entry;

/* PCI device related */
struct pci_dev * ediag_pci_get_slot(struct pci_bus * bus, unsigned int devfn);
struct pci_bus * ediag_pci_find_bus(int domain, int bus_number);
struct pci_dev * ediag_pci_find_device_any(struct pci_dev *from);
int ediag_get_domain_num(struct pci_dev * dev);
unsigned char ediag_get_bus_num(struct pci_bus * bus);
void ediag_ret_pci_dev_hdl(struct pci_dev * pci_dev);
void * ediag_pci_resource_start(struct pci_dev * pci_dev, int n);
unsigned long ediag_pci_resource_len(struct pci_dev * pci_dev, int n);
int ediag_set_power_state(
	struct pci_dev *pdev,
	int  power_state);
void ediag_release_pci_dev(struct pci_dev * pdev);
int ediag_init_pci_dev(struct pci_dev *pdev);
void ediag_pci_disable_msix(struct pci_dev * dev);
int ediag_pci_enable_msix(struct pci_dev *dev,
                          struct msix_entry *entries, int nvec);
int ediag_pci_read_config_byte(struct pci_dev *dev, int where, u8 *val);
int ediag_pci_read_config_word(struct pci_dev *dev, int where, u16 *val);
int ediag_pci_read_config_dword(struct pci_dev *dev, int where, u32 *val);
int ediag_pci_write_config_byte(struct pci_dev *dev, int where, u8 val);
int ediag_pci_write_config_word(struct pci_dev *dev, int where, u16 val);
int ediag_pci_write_config_dword(struct pci_dev *dev, int where, u32 val);
unsigned int ediag_get_irq_num(struct pci_dev * pdev);
int ediag_other_drv_up(struct pci_dev * pdev);
u8   ediag_readb(const volatile void __iomem *addr);
u16  ediag_readw(const volatile void __iomem *addr);
u32  ediag_readl(const volatile void __iomem *addr);
u64  ediag_readq(const volatile void __iomem *addr);
void ediag_writeb(u8 b, volatile void __iomem *addr);
void ediag_writew(u16 b, volatile void __iomem *addr);
void ediag_writel(u32 b, volatile void __iomem *addr);
void ediag_writeq(u64 b, volatile void __iomem *addr);
int ediag_pci_enable_sriov(struct pci_dev *pdev, u16_t vfs_num);
void ediag_pci_disable_sriov(struct pci_dev *pdev);
int ediag_pci_find_sriov_capability(struct pci_dev *pdev);
int ediag_pci_find_capability(struct pci_dev *pdev, int cap);
int ediag_get_sriov_bar(struct pci_dev *pdev, u64_t *bar, u32_t *size, int bn);
int ediag_pci_save_state(struct pci_dev *pdev);
int ediag_pci_restore_state(struct pci_dev *pdev);
int ediag_pci_execute_reset_function(struct pci_dev *pdev);
int ediag_check_flr_cap(struct pci_dev *pdev);



/* Semaphores handling functions */
void ediag_init_mutex(struct semaphore ** sem);
void ediag_free_sema(struct semaphore * sem);
int ediag_sema_down(struct semaphore * sema);
void ediag_sema_up(struct semaphore * sema);

/* Workqueue */
void ediag_cancel_work_sync(void * work);
void ediag_flush_workqueue(void);
int ediag_queue_work(void *t);
int ediag_queue_delayed_work(struct proxy_t *t, unsigned long delay);
struct proxy_t * ediag_alloc_proxy(int cnt);
void ediag_free_proxy(struct proxy_t * proxy);
void ediag_set_proxy_host(struct proxy_t *proxy, void *host);
void * ediag_get_work_offset(struct proxy_t * proxy);
int ediag_rss_enabled(void);

/* Barriers */
void ediag_mmiowb(void);
void ediag_rmb(void);
void ediag_wmb(void);
void ediag_smp_mb(void);

/* Atomic stuff */
void ediag_atomic_set(s32_t *v, int i);
int ediag_atomic_dec_and_test(s32_t *v);
int ediag_atomic_inc_and_test(s32_t *v);
int ediag_atomic_read(s32_t *v);
int ediag_atomic_long_read(unsigned long *v);
int ediag_atomic_cmpxchg(s32_t *v, int cmp, int new_v);


/* IO ports related */
#ifndef resource_size_t
#define resource_size_t unsigned long
#endif
struct resource * ediag_request_region(resource_size_t start,
                                       resource_size_t n, const char *name);
void ediag_release_region(resource_size_t start, resource_size_t n);
unsigned ediag_inb(unsigned port);
unsigned ediag_inw(unsigned port);
unsigned ediag_inl(unsigned port);
void ediag_outb(u8_t byte, unsigned port);
void ediag_outw(u16_t word, unsigned port);
void ediag_outl(u32_t dword, unsigned port);

/* DMA and other memory allocation/deallocation/mapping */
void* ediag_pci_alloc_consistent(struct pci_dev *, size_t, u64_t *);
void ediag_pci_free_consistent(struct pci_dev *, size_t, void *, u64_t);
void *ediag_kmalloc_atomic(size_t size);
void *ediag_kmalloc_kernel(size_t size);
void ediag_kfree(void *block);
void *ediag_vmalloc(size_t size);
void ediag_vfree(void *addr);
dma_addr_t ediag_pci_map_single_to(struct pci_dev *pdev, void * buf, size_t size);
void ediag_pci_unmap_single_to(struct pci_dev *pdev, dma_addr_t buf, size_t size);
int ediag_dma_mapping_error(struct pci_dev *pdev, dma_addr_t mapping);
void __iomem* ediag_ioremap_nocache(resource_size_t offset, unsigned long size);
void __iomem* ediag_ioremap_wc(resource_size_t offset, unsigned long size);
void ediag_iounmap(void __iomem *addr);
unsigned long ediag_virt_to_phys(void*);
void* ediag_phys_to_virt(unsigned long);
struct page* ediag_virt_to_page(void*);
void ediag_SetPageReserved(struct page*);
void ediag_ClearPageReserved(struct page*);

/* IRQ and interrupts handling related */
int ediag_request_irq(unsigned int irq_num, const char * device, void * data);
int ediag_request_msix_irq_fp(struct msix_entry *msix_ent, const char * device, void * data);
int ediag_request_msix_irq_def_sb(struct msix_entry *msix_ent, const char * device, void * data);
int ediag_request_irq_igu_tmode(unsigned int irq_num, const char * device, void * data);
int ediag_request_msix_irq_fp_igu_tmode(struct msix_entry *msix_ent, const char * device, void * data);
int ediag_request_msix_irq_def_sb_igu_tmode(struct msix_entry *msix_ent, const char * device, void * data);

int ediag_get_msix_entry_size(void);
int ediag_msix_capable(void);
u32_t ediag_msix_vec(struct msix_entry *entry);
void ediag_msix_ent_set(struct msix_entry *entry, u16_t val);
struct msix_entry *ediag_msix_entry(struct msix_entry *base_entry, int idx);
void ediag_synchronize_irq(unsigned int irq);
int ediag_in_atomic(void);
void ediag_free_irq(unsigned int irq, void *dev_id);
unsigned long ediag_jiffies(void);
unsigned long ediag_HZ(void);
void ediag_msleep(unsigned int msec);
unsigned long ediag_msleep_interruptible(unsigned int msec);
void ediag_udelay(unsigned long usecs);
u64_t ediag_rdtsc(void);
void ediag_schedule(void);
void ediag_disable_irq(struct pci_dev *pdev);
void ediag_enable_irq(struct pci_dev *pdev);


/* String manipulation and output */
void *ediag_memset(void *,int,size_t);
char *ediag_strncpy(char *t, const char *s, size_t n);
void *ediag_memcpy(void *dest, const void *src, size_t count);
int ediag_memcmp(const void *s1, const void *s2, size_t n);
int ediag_strcasecmp(const char *, const char*);
int ediag_strcmp(const char *, const char*);
char *ediag_strrchr(char *a, int n);

/* User <--> kernel transactions */
int ediag_put_user32(u32_t val, u32_t __user * ptr);
int ediag_put_user16(u16_t val, u16_t __user * ptr);
int ediag_put_user8(u8_t val, u8_t __user * ptr);
unsigned long ediag_copy_from_user(void *to,const void __user *from, 
				   unsigned long n);
unsigned long ediag_copy_to_user(void __user *to,
				const void *from, unsigned long n);
/* Async notification handling */
void * ediag_filp_priv(struct file *filp);
void ediag_kill_fasync_io(struct fasync_struct **);

/* Kernel debug stuff */
void ediag_warn_on(int cond);
void ediag_might_sleep(void);

/* Bits tweaking */
u16_t ediag_cpu_to_le16(u16_t);
u32_t ediag_cpu_to_le32(u32_t);
u16_t ediag_le16_to_cpu(u16_t);
u32_t ediag_le32_to_cpu(u32_t);
u32_t ediag_be32_to_cpu(u32_t);
u32_t ediag_be16_to_cpu(u32_t);
u32_t ediag_cpu_to_be32(u32_t);
u32_t ediag_cpu_to_be16(u32_t);


u32_t ediag_crc32_le(unsigned char const *p, size_t len);


/* 64 bits arithmetics in kernel */
u64_t ediag_do_div(u64_t n, u32_t base);

#endif /* __EDIAG_COMPAT_H__ */



