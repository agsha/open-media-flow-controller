/*
 * Pacifica Cavecreek GPIO LED driver
 *
 * Copyright (c) 2011,  Juniper Networks
 *
 * This file is licensed under GPLv2.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
*/



/* Supports:
 * Pacifica Cavecreek GPIO LEDs
 */

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/stddef.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/proc_fs.h>
#include "pacifica_mxc_gpio.h"


static struct pci_device_id pacifica_mxc_gpio_ids[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_INTEL, LPC_DEV_ID) },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, pacifica_mxc_gpio_ids);
MODULE_LICENSE("GPL");

typedef struct mxc_gpio_t {
    u32 base_address;
    char *base;
    struct proc_dir_entry *gpio_dir;
    int cpu_led_value;
    int app_led_value;
    struct pci_dev *pdev;
}mxc_gpio_t;
static mxc_gpio_t *mxc_gpio;

static int
pacifica_mxc_gpio_write_app_led(struct file *file, const char *buffer,
				unsigned long count, void *data)
{
    mxc_gpio_t *mxc_gpio = (mxc_gpio_t *)data;
    int value;
    int proc_value;

    proc_value = simple_strtoul(buffer,NULL,10);
#ifdef DEBUG
    dev_info(&mxc_gpio->pdev->dev, "Value = %x\n", proc_value);
#endif
    mxc_gpio->app_led_value = proc_value;

    value = ioread32(mxc_gpio->base + CC_LPC_GPIO_LVL2);
#ifdef DEBUG
    dev_info(&mxc_gpio->pdev->dev, "Value[CC_LPC_GPIO_LVL2] = %x\n", value);
#endif
    switch(proc_value) {
	case 0:
	case 0x30:
	    value &= ~(1 << GPIO_52_SHIFT);
	    value &= ~(1 << GPIO_50_SHIFT);
	    break;
	case 1:
	case 0x31:
	    value &= ~(1 << GPIO_52_SHIFT);
	    value |= (1 << GPIO_50_SHIFT);
	    break;
	case 2:
	case 0x32:
	    value |= (1 << GPIO_52_SHIFT);
	    value |= (1 << GPIO_50_SHIFT);
	    break;
	case 3:
	case 0x33:
	    value |= (1 << GPIO_52_SHIFT);
	    value &= ~(1 << GPIO_50_SHIFT);
	    break;
    }
#ifdef DEBUG
    dev_info(&mxc_gpio->pdev->dev, "Value[CC_LPC_GPIO_LVL2] = %x\n", value);
#endif
    iowrite32(value, mxc_gpio->base + CC_LPC_GPIO_LVL2);
    value = ioread32(mxc_gpio->base + CC_LPC_GPIO_LVL2);
#ifdef DEBUG
    dev_info(&mxc_gpio->pdev->dev, "Value[CC_LPC_GPIO_LVL2] = %x\n", value);
#endif

    return count;
}

static int
pacifica_mxc_gpio_read_app_led(char *buf, char **start, off_t off,
			       int count, int *eof, void *data)
{
    mxc_gpio_t *mxc_gpio = (mxc_gpio_t *)data;

    buf[0] = mxc_gpio->app_led_value + 0x30;
    buf[1] = '\0';
    return 2;

}

static int
pacifica_mxc_gpio_write_cpu_led(struct file *file, const char *buffer,
				unsigned long count, void *data)
{
    mxc_gpio_t *mxc_gpio = (mxc_gpio_t *)data;
    int value;
    int proc_value;

    proc_value = simple_strtoul(buffer,NULL,10);
#ifdef DEBUG
    dev_info(&mxc_gpio->pdev->dev, "Value = %x\n", proc_value);
#endif
    mxc_gpio->cpu_led_value = proc_value;

    value = ioread32(mxc_gpio->base + CC_LPC_GPIO_LVL);
#ifdef DEBUG
    dev_info(&mxc_gpio->pdev->dev, "Value[CC_LPC_GPIO_LVL] = %x\n", value);
#endif
    switch(proc_value) {
	    case 0:
	    case 0x30:
		    value &= ~(1 << GPIO_27_SHIFT);
		    value &= ~(1 << GPIO_25_SHIFT);
		    break;
	    case 1:
	    case 0x31:
		    value &= ~(1 << GPIO_27_SHIFT);
		    value |= (1 << GPIO_25_SHIFT);
		    break;
	    case 2:
	    case 0x32:
		    value |= (1 << GPIO_27_SHIFT);
		    value |= (1 << GPIO_25_SHIFT);
		    break;
	    case 3:
	    case 0x33:
		    value |= (1 << GPIO_27_SHIFT);
		    value &= ~(1 << GPIO_25_SHIFT);
		    break;
    }
#ifdef DEBUG
    dev_info(&mxc_gpio->pdev->dev, "Value[CC_LPC_GPIO_LVL] = %x\n", value);
#endif
    iowrite32(value, mxc_gpio->base + CC_LPC_GPIO_LVL);
    value = ioread32(mxc_gpio->base + CC_LPC_GPIO_LVL);
#ifdef DEBUG
    dev_info(&mxc_gpio->pdev->dev, "Value[CC_LPC_GPIO_LVL] = %x\n", value);
#endif

    return count;
}

static int
pacifica_mxc_gpio_read_cpu_led(char *buf, char **start, off_t off,
			       int count, int *eof, void *data)
{
    mxc_gpio_t *mxc_gpio = (mxc_gpio_t *)data;

    buf[0] = mxc_gpio->cpu_led_value + 0x30;
    buf[1] = '\0';
    return 2;

}

static int __devinit pacifica_mxc_gpio_probe(struct pci_dev *pdev,
			const struct pci_device_id *id)
{
	struct proc_dir_entry *gpio_entry;
	int retval = 0;
	u32 value;

	retval = pci_enable_device(pdev);
	if (retval) {
		printk(KERN_ERR "Cannot enable PCI device, aborting\n");
		goto done;
	}

	mxc_gpio = kmalloc(sizeof(mxc_gpio_t), GFP_KERNEL);
	if (!mxc_gpio) {
		retval = -ENOMEM;
		goto done;
	}
	mxc_gpio->pdev = pdev;
	mxc_gpio->base = NULL;
	mxc_gpio->base_address = 0;

#ifdef DEBUG
	dev_info(&pdev->dev, "In pacifica_mxc_gpio_probe\n");
#endif

	if (pci_read_config_dword(pdev, CC_LPC_CFG_GPIO_BASE, &value)
		!= PCIBIOS_SUCCESSFUL) {
		printk(KERN_ERR "Error in reading config\n");
	} else {
#ifdef DEBUG
		dev_info(&pdev->dev, "Value[CC_LPC_CFG_GPIO_BASE] = %x\n",
			 value);
#endif
		mxc_gpio->base_address = (value / 128) * 128;
#ifdef DEBUG
		dev_info(&pdev->dev, "base_address = %x\n",
			mxc_gpio->base_address);
#endif
	}

	retval = pci_request_regions(pdev, "pacifica_gpio");
	if (retval) {
		printk(KERN_ERR "error requesting resources\n");
	}

	request_region(mxc_gpio->base_address, 128, "pacifica_mxc_gpio");

	mxc_gpio->base = ioport_map(mxc_gpio->base_address, 128);
#ifdef DEBUG
	dev_info(&pdev->dev, "base = %p\n", mxc_gpio->base);
#endif

	/* Enable GPIO */
	value = ioread32(mxc_gpio->base);
#ifdef DEBUG
	dev_info(&pdev->dev, "Value[CC_LPC_GPIO_USE_SEL] = %x\n", value);
#endif
	value |= (1 << GPIO_27_SHIFT);
	value |= (1 << GPIO_25_SHIFT);
	iowrite32(value, mxc_gpio->base);
	value = ioread32(mxc_gpio->base);
#ifdef DEBUG
	dev_info(&pdev->dev, "Value[CC_LPC_GPIO_USE_SEL] = %x\n", value);
#endif

	value = ioread32(mxc_gpio->base + CC_LPC_GPIO_SEL);
#ifdef DEBUG
	dev_info(&pdev->dev, "Value[CC_LPC_GPIO_SEL] = %x\n", value);
#endif
	value &= ~(1 << GPIO_27_SHIFT);
	value &= ~(1 << GPIO_25_SHIFT);
	iowrite32(value, mxc_gpio->base + CC_LPC_GPIO_SEL);
	value = ioread32(mxc_gpio->base + CC_LPC_GPIO_SEL);
#ifdef DEBUG
	dev_info(&pdev->dev, "Value[CC_LPC_GPIO_SEL] = %x\n", value);
#endif

	/* Configure GPIOs as output */
	value = ioread32(mxc_gpio->base + CC_LPC_GPIO_USE_SEL2);
#ifdef DEBUG
	dev_info(&pdev->dev, "Value[CC_LPC_GPIO_USE_SEL2] = %x\n", value);
#endif
	value |= (1 << GPIO_50_SHIFT);
	value |= (1 << GPIO_52_SHIFT);
	iowrite32(value, mxc_gpio->base + CC_LPC_GPIO_USE_SEL2);
	value = ioread32(mxc_gpio->base + CC_LPC_GPIO_USE_SEL2);
#ifdef DEBUG
	dev_info(&pdev->dev, "Value[CC_LPC_GPIO_USE_SEL2] = %x\n", value);
#endif

	value = ioread32(mxc_gpio->base + CC_LPC_GPIO_SEL2);
#ifdef DEBUG
	dev_info(&pdev->dev, "Value[CC_LPC_GPIO_SEL2] = %x\n", value);
#endif
	value &= ~(1 << GPIO_50_SHIFT);
	value &= ~(1 << GPIO_52_SHIFT);
	iowrite32(value, mxc_gpio->base + CC_LPC_GPIO_SEL2);
	value = ioread32(mxc_gpio->base + CC_LPC_GPIO_SEL2);
#ifdef DEBUG
	dev_info(&pdev->dev, "Value[CC_LPC_GPIO_SEL2] = %x\n", value);
#endif

	/* Reset the CPU led to Orange */
	value = ioread32(mxc_gpio->base + CC_LPC_GPIO_LVL);
#ifdef DEBUG
	dev_info(&pdev->dev, "Value[CC_LPC_GPIO_LVL] = %x\n", value);
#endif
	value |= (1 << GPIO_27_SHIFT);
	value |= (1 << GPIO_25_SHIFT);
	iowrite32(value, mxc_gpio->base + CC_LPC_GPIO_LVL);
	value = ioread32(mxc_gpio->base + CC_LPC_GPIO_LVL);
#ifdef DEBUG
	dev_info(&pdev->dev, "Value[CC_LPC_GPIO_LVL] = %x\n", value);
#endif
	mxc_gpio->cpu_led_value = 2;

	/* Reset the APP led to Red */
	value = ioread32(mxc_gpio->base + CC_LPC_GPIO_LVL2);
#ifdef DEBUG
	dev_info(&pdev->dev, "Value[CC_LPC_GPIO_LVL2] = %x\n", value);
#endif
	value &= ~(1 << GPIO_52_SHIFT);
	value |= (1 << GPIO_50_SHIFT);
	iowrite32(value, mxc_gpio->base + CC_LPC_GPIO_LVL2);
	value = ioread32(mxc_gpio->base + CC_LPC_GPIO_LVL2);
#ifdef DEBUG
	dev_info(&pdev->dev, "Value[CC_LPC_GPIO_LVL2] = %x\n", value);
#endif
	mxc_gpio->app_led_value = 1;

	/* create cfm directory */
	mxc_gpio->gpio_dir = proc_mkdir("mxc-gpio", NULL);

	gpio_entry             = create_proc_entry("app-led",0600,
						   mxc_gpio->gpio_dir);
	gpio_entry->nlink      = 1;
	gpio_entry->read_proc  = pacifica_mxc_gpio_read_app_led;
	gpio_entry->write_proc = pacifica_mxc_gpio_write_app_led;
        gpio_entry->data       = (void *)mxc_gpio;

	gpio_entry             = create_proc_entry("cpu-led",0600,
						   mxc_gpio->gpio_dir);
	gpio_entry->nlink      = 1;
	gpio_entry->read_proc  = pacifica_mxc_gpio_read_cpu_led;
	gpio_entry->write_proc = pacifica_mxc_gpio_write_cpu_led;
        gpio_entry->data       = (void *)mxc_gpio;

done:
	return retval;
}

static struct pci_driver pacifica_mxc_gpio_driver = {
	.name		= "pacifica_mxc_gpio",
	.id_table	= pacifica_mxc_gpio_ids,
	.probe		= pacifica_mxc_gpio_probe,
};

static int __init pacifica_mxc_gpio_init(void)
{
	return pci_register_driver(&pacifica_mxc_gpio_driver);
}

module_init(pacifica_mxc_gpio_init);

static void __exit pacifica_mxc_gpio_exit(void)
{
	u32 value;

	if(mxc_gpio) {
		/* Reset the CPU led to Red */
		value = ioread32(mxc_gpio->base + CC_LPC_GPIO_LVL);
#ifdef DEBUG
		dev_info(&pdev->dev, "Value[CC_LPC_GPIO_LVL] = %x\n", value);
#endif
		value &= ~(1 << GPIO_27_SHIFT);
		value |= (1 << GPIO_25_SHIFT);
		iowrite32(value, mxc_gpio->base + CC_LPC_GPIO_LVL);
		value = ioread32(mxc_gpio->base + CC_LPC_GPIO_LVL);
#ifdef DEBUG
		dev_info(&pdev->dev, "Value[CC_LPC_GPIO_LVL] = %x\n", value);
#endif
		mxc_gpio->cpu_led_value = 1;

		/* Reset the APP led to Red */
		value = ioread32(mxc_gpio->base + CC_LPC_GPIO_LVL2);
#ifdef DEBUG
		dev_info(&pdev->dev, "Value[CC_LPC_GPIO_LVL2] = %x\n", value);
#endif
		value &= ~(1 << GPIO_52_SHIFT);
		value |= (1 << GPIO_50_SHIFT);
		iowrite32(value, mxc_gpio->base + CC_LPC_GPIO_LVL2);
		value = ioread32(mxc_gpio->base + CC_LPC_GPIO_LVL2);
#ifdef DEBUG
		dev_info(&pdev->dev, "Value[CC_LPC_GPIO_LVL2] = %x\n", value);
#endif
		mxc_gpio->app_led_value = 1;

		remove_proc_entry("cpu-led", mxc_gpio->gpio_dir);
		remove_proc_entry("app-led", mxc_gpio->gpio_dir);
		remove_proc_entry("mxc-gpio", NULL);
		if(mxc_gpio->base)
			ioport_unmap(mxc_gpio->base);
		kfree(mxc_gpio);
	}
	pci_unregister_driver(&pacifica_mxc_gpio_driver);
}
module_exit(pacifica_mxc_gpio_exit);

