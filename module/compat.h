#ifndef __KCOMPAT24_H__
#define __KCOMPAT24_H__

/* some linux version have not this file, in this case define in this file
 * come from asm/smp.h, include linux/smp.h is more safe */
#ifdef HAVE_ASM_MPSPEC_H
#include <asm/mpspec.h>
#else
#include <linux/smp.h>
#endif

#ifdef HAVE_ASM_APIC_H
#include <asm/apic.h>
#else
#include "apic.h"
#endif

#ifndef HAVE_CACHE_ALIGNED
#include "cache.h"
#endif

/* ugly hack, protoype of pte_page have changed through version of linux.
 * this don't matter except when used as page_address(pte_page(pte)) because
 * the page_address have not changed of prototype, it's difficult to get a
 * reliable work-around so this hack */
#ifdef HAVE_INVALID_PTE_PAGE_PROTO
#define pte_page_address(pte) pte_page(pte)
#else
#define pte_page_address(pte) page_address(pte_page(pte))
#endif

#ifndef HAVE_VIRT_TO_PAGE
#define virt_to_page(va) MAP_NR(va)
#endif

/* FIXME, vmalloc_32 pass specfic flags to vmalloc, I think than older
 * implementation use by default this flags, but tihs have not been carefully
 * checked */
#ifndef HAVE_VMALLOC_32
#define vmalloc_32 vmalloc
#endif

#ifndef HAVE_CPU_NUMBER_MAP
#ifdef CONFIG_SMP
#define cpu_number_map(x) (x)
#else
#define cpu_number_map(x)  0
#endif
#endif

/* some old 2.2 have SMP and no smp_call_function, for now just define it
 * as success ==> oprofile for kernel 2.2 must be compiled with SMP but does
 * not support SMP :/ FIXME */
#if defined(CONFIG_SMP) && !defined(smp_call_function)
#define smp_call_function(func,info,retry,wait)  (0 == 1)
#endif

/* PHE: TODO, cleanup the following, keep only the necessary things for
 * oprofile or make this file a general backward compatiblity linux file ? */
/*

	notes:

	2.3.13 adds new resource allocation

 */

#include <linux/version.h>
#include <linux/netdevice.h>
#include <asm/io.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,0)

#define init_MUTEX(a)           *(a) = MUTEX

#define wait_queue_head_t       struct wait_queue *
#define DECLARE_WAITQUEUE(a, b) struct wait_queue a = {b, NULL};
#define DECLARE_WAIT_QUEUE_HEAD(q) struct wait_queue *q = NULL 
#define init_waitqueue_head(a)  init_waitqueue(a)
#define DECLARE_MUTEX(foo)	struct semaphore foo = MUTEX
#define DECLARE_MUTEX_LOCKED(foo) struct semaphore foo = MUTEX_LOCKED

#define in_irq in_interrupt

#include <asm/spinlock.h>

#ifdef KCOMPAT_INCLUDED
  #define KCOMPINC static
#else
  #define KCOMPINC
#endif

#define net_device device

#define __exit
#define __exitdata
#define __devinit
#define __devinitdata
#define __devexit
#define __devexitdata

/* Not sure what version aliases were introduced in, but certainly in 2.91.66.  */
/* phe: FIXME if we keep this autoconf it */
#ifdef MODULE
  #if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 91)
    #define module_init(x)	int init_module(void) __attribute__((alias(#x)));
    #define module_exit(x)	void cleanup_module(void) __attribute__((alias(#x)));
  #else
    #define module_init(x)	int init_module(void) { return x(); }
    #define module_exit(x)	void cleanup_module(void) { x(); }
  #endif
#else
  #define module_init(x)
  #define module_exit(x)
#endif

#define MODULE_DEVICE_TABLE(foo,bar)


/*
 * Insert a new entry before the specified head..
 */
static __inline__ void list_add_tail(struct list_head *new, struct list_head *head)
{
	__list_add(new, head->prev, head);
}

#define list_for_each(pos, head) \
        for (pos = (head)->next; pos != (head); pos = pos->next)

#define IORESOURCE_IO			0x00000100	/* Resource type */
#define IORESOURCE_MEM			0x00000200

#define request_region			compat_request_region

/* XXX provide real support for this, even though pre-2.3.x support
 * doesn't exist */
#define request_mem_region(x,y,z)	(1)
#define release_mem_region(x,y)		do {} while (0)


/* New-style probing supporting hot-pluggable devices */

#define PCI_PM_CTRL		4	/* PM control and status register */
#define  PCI_PM_CTRL_STATE_MASK	0x0003	/* Current power state (D0 to D3) */

#define PCI_ANY_ID (~0)

#define PCI_GET_DRIVER_DATA pci_compat_get_driver_data
#define PCI_SET_DRIVER_DATA pci_compat_set_driver_data

#define PCI_SET_DMA_MASK(dev, mask)
#define pci_dma_supported(dev, mask) 1

#define pci_enable_device pci_compat_enable_device
#define pci_register_driver pci_compat_register_driver
#define pci_unregister_driver pci_compat_unregister_driver

#define pci_dev_g(n) list_entry(n, struct pci_dev, global_list)
#define pci_dev_b(n) list_entry(n, struct pci_dev, bus_list)

#define pci_for_each_dev(dev) \
	for(dev = pci_devices; dev; dev = dev->next)

#define pci_resource_start(dev,bar) \
(((dev)->base_address[(bar)] & PCI_BASE_ADDRESS_SPACE) ? \
 ((dev)->base_address[(bar)] & PCI_BASE_ADDRESS_IO_MASK) : \
 ((dev)->base_address[(bar)] & PCI_BASE_ADDRESS_MEM_MASK))
#define pci_resource_len pci_compat_get_size
#define pci_resource_end(dev,bar) \
	(pci_resource_len((dev),(bar)) == 0 ? \
	pci_resource_start(dev,bar) : \
	(pci_resource_start(dev,bar) + pci_resource_len((dev),(bar)) - 1)

#define pci_resource_flags(dev,bar) (pci_compat_get_flags((dev),(bar)))

struct pci_device_id {
	unsigned int vendor, device;		/* Vendor and device ID or PCI_ANY_ID */
	unsigned int subvendor, subdevice;	/* Subsystem ID's or PCI_ANY_ID */
	unsigned int class, class_mask;		/* (class,subclass,prog-if) triplet */
	unsigned long driver_data;		/* Data private to the driver */
};

struct pci_driver {
	struct list_head node;
	struct pci_dev *dev;
	char *name;
	const struct pci_device_id *id_table;	/* NULL if wants all devices */
	int (*probe)(struct pci_dev *dev, const struct pci_device_id *id);	/* New device inserted */
	void (*remove)(struct pci_dev *dev);	/* Device removed (NULL if not a hot-plug capable driver) */
	void (*suspend)(struct pci_dev *dev);	/* Device suspended */
	void (*resume)(struct pci_dev *dev);	/* Device woken up */
};



/*
 *
 */
KCOMPINC const struct pci_device_id * pci_compat_match_device(const struct pci_device_id *ids, struct pci_dev *dev);
KCOMPINC int pci_compat_register_driver(struct pci_driver *drv);
KCOMPINC void pci_compat_unregister_driver(struct pci_driver *drv);
KCOMPINC unsigned long pci_compat_get_size (struct pci_dev *dev, int n_base);
KCOMPINC int pci_compat_get_flags (struct pci_dev *dev, int n_base);
KCOMPINC int pci_compat_set_power_state(struct pci_dev *dev, int new_state);
KCOMPINC int pci_compat_enable_device(struct pci_dev *dev);
KCOMPINC int pci_compat_find_capability(struct pci_dev *dev, int cap);
KCOMPINC void *compat_request_region (unsigned long start, unsigned long n, const char *name);
KCOMPINC void * pci_compat_get_driver_data (struct pci_dev *dev);
KCOMPINC void pci_compat_set_driver_data (struct pci_dev *dev, void *driver_data);

#else  /* if kernel version >= 2.3.0 */

#include <linux/spinlock.h>

#ifndef pci_resource_start
  #define pci_resource_start(dev,bar)	((dev)->resource[(bar)].start)
  #define pci_resource_end(dev,bar)	((dev)->resource[(bar)].end)
  #define pci_resource_flags(dev,bar)	((dev)->resource[(bar)].flags)
#endif /* !pci_resource_start */

#ifndef PCI_GET_DRIVER_DATA
  #define PCI_GET_DRIVER_DATA(pdev)		((pdev)->driver_data)
  #define PCI_SET_DRIVER_DATA(pdev,data)	(((pdev)->driver_data) = (data))
#endif /* PCI_GET_DRIVER_DATA */

static inline void netif_init_watchdog(struct net_device *dev, int timeout,
				      void (*tx_timeout) (struct net_device *dev))
{
	dev->watchdog_timeo = timeout;
	dev->tx_timeout = tx_timeout;
}

static inline void netif_start_watchdog(struct net_device *dev)
{
	/* network layer does it for us */
}

static inline void netif_stop_watchdog(struct net_device *dev)
{
	/* network layer does it for us */
}

#endif /* kernel version <=> 2.3.0 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,47)
typedef __u32 dma_addr_t;

/* Pure 2^n version of get_order */
extern __inline__ int __compat_get_order(unsigned long size)
{
	int order;

	size = (size-1) >> (PAGE_SHIFT-1);
	order = -1;
	do {
		size >>= 1;
		order++;
	} while (size);
	return order;
}

extern __inline__ void *
pci_alloc_consistent(struct pci_dev *hwdev,
		     size_t size, dma_addr_t *dma_handle) {
        void *ret;
        int gfp = GFP_ATOMIC;

        if (hwdev == NULL)
                gfp |= GFP_DMA;
        ret = (void *)__get_free_pages(gfp, __compat_get_order(size));

        if (ret != NULL) {
                memset(ret, 0, size);
                *dma_handle = virt_to_bus(ret);
        }
        return ret;
}

extern __inline__ void
pci_free_consistent(struct pci_dev *hwdev, size_t size,
		    void *vaddr, dma_addr_t dma_handle)
{
        free_pages((unsigned long)vaddr, __compat_get_order(size));
}

#define pci_unmap_single(cookie, address, size, dir)
#define pci_map_sg(cookie, sg, nents, dir) (nents)
#define pci_unmap_sg(cookie, sg, nents, dir)
#define sg_dma_len(slp) ((slp)->length)
#define pci_map_single(cookie, address, size, dir) virt_to_bus(address)
#define sg_dma_address(slp) virt_to_bus((slp)->address)
#define PCI_DMA_BIDIRECTIONAL	0
#define PCI_DMA_TODEVICE	1
#define PCI_DMA_FROMDEVICE	2
#define PCI_DMA_NONE		3
#define scsi_to_pci_dma_dir(dir) PCI_DMA_BIDIRECTIONAL
#endif /* version < v2.3.47 */


/*
 * SoftNet
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,40)

#define dev_kfree_skb_irq(a)    dev_kfree_skb(a)
#define netif_stop_queue(dev)   set_bit(0, &dev->tbusy)
#define netif_queue_stopped(dev)        (dev->tbusy)
#define netif_running(dev)              (dev->start)
 
static inline void netif_start_queue(struct net_device *dev)
{
        dev->interrupt = 0;
	clear_bit(0, &dev->tbusy);
	set_bit(0, &dev->start);
}

static inline void netif_wake_queue(struct net_device *dev)
{
	clear_bit(0, &dev->tbusy);
	mark_bh(NET_BH);
}
 
static inline void netif_device_attach(struct net_device *dev)
{
	if (dev->tbusy && !netif_running(dev)) {
		netif_start_queue(dev);
		netif_wake_queue(dev);
	}
}

static inline void netif_device_detach(struct net_device *dev)
{
	if (netif_running(dev)) {
		dev->start = 0;
		netif_stop_queue(dev);
	}
}

void netif_init_watchdog(struct net_device *dev, int timeout,
			 void (*tx_timeout) (struct net_device *dev));
void netif_stop_watchdog(struct net_device *dev);
void netif_start_watchdog(struct net_device *dev);

#endif /* softnet compatibility -- version < 2.3.40 */


/*
 * pci.module.init (2.3.40 version is just a guess)
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,40)

/*
 * a helper function which helps ensure correct pci_driver
 * setup and cleanup for commonly-encountered hotplug/modular cases
 *
 * This MUST stay in a header, as it checks for -DMODULE
 */
static inline int pci_module_init(struct pci_driver *drv)
{
	int rc = pci_register_driver (drv);

	if (rc > 0)
		return 0;

	/* if we get here, we need to clean up pci driver instance
	 * and return some sort of error */
	pci_unregister_driver (drv);
	
	return -ENODEV;
}

#endif /* pci.module.init compatibility -- version < 2.3.40 (a guess) */


/*
 * tasklet compat from Rui Sousa (2.3.40 version is just a guess)
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,40)

#define tasklet_hi_schedule(t)  queue_task((t), &tq_immediate); \
                                mark_bh(IMMEDIATE_BH)

#define tasklet_init(t,f,d)     (t)->next = NULL; \
                                (t)->sync = 0; \
                                (t)->routine = (void (*)(void *))(f); \
                                (t)->data = (void *)(d)

#define tasklet_struct          tq_struct 

#define tasklet_unlock_wait(t)  while (test_bit(0, &(t)->sync)) { }

#endif /* tasklet compatibility from Rui Sousa -- version < 2.3.40 (a guess) */

#ifdef KCOMPAT_INCLUDED
  #include "kcompat24.c"
#endif

#endif /* __KCOMPAT24_H__ */


