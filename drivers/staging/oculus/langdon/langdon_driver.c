#include <linux/module.h>
#include <linux/pci.h>
#include <linux/fs.h>
#include <linux/of_irq.h>
#include <linux/dma-mapping.h>
#include <linux/proc_fs.h>
#include <linux/cdev.h>
#include <linux/mm.h>
#include <linux/pfn.h>
#include <linux/dma-direct.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include "include/langdonIoctl.h"


#define DMA_BUFFER_SIZE 4096
#define VENDOR_ID 0x1d9b
#define DEVICE_ID 0x8211
#define MAX_BURST_LENGTH 0x3F
#define MAX_PAT_ENTRIES 64
#define C2H (0)
#define H2C (1)
#define MAX_NUM_MSIX_VECTORS (4)
#define MAX_RETRIES_FOR_1_SEC_TIMEOUT (1000000)


typedef struct _DRIVER_DMA_BUFFER_STRUCT {
    dma_addr_t dma_phys_addr;
    void *dma_virt_addr;
} DRIVER_DMA_BUFFER_STRUCT;

typedef struct _DRIVER_DMA_CHANNEL_STRUCT {
    int channel_number;
    bool is_open;
    int direction;
    int current_buffer;
    int number_of_entries;
    DRIVER_DMA_BUFFER_STRUCT channel_buffers[1];
} DRIVER_DMA_CHANNEL_STRUCT;

typedef struct _DRIVER_USER_INTERRUPT_DATA_STRUCT {
    int channel_number;
    struct msix_entry* pmsix_entry;
    uint32_t vector_table_offset;
    struct tasklet_struct UserIntTasklet;
    wait_queue_head_t waitQueue;
    bool cancelWaitFlag;
} DRIVER_USER_INTERRUPT_DATA_STRUCT;

static int major_number;
static struct cdev simple_cdev;
static dev_t dev_num;
static struct class *device_class = NULL;  // Declare and initialize to NULL
static struct device *device = NULL; // Declare and initialize to NUL
static void __iomem *bar0_base;
static void __iomem *bar2_base;
static DRIVER_VERSION_STRUCT driver_version = { 0, 6, 1};
static struct pci_dev *langdon_pdev = NULL;
static DRIVER_DMA_CHANNEL_STRUCT** dma_channels;
static struct proc_dir_entry *langdon_procFs;
struct msix_entry MSIXEntries[MAX_NUM_MSIX_VECTORS];
static DRIVER_USER_INTERRUPT_DATA_STRUCT user_interrupts[MAX_NUM_MSIX_VECTORS];
//static bool irq_enabled = false;
static struct mutex hw_mutex;

static const struct pci_device_id pci_ids[] = {
    { PCI_DEVICE(VENDOR_ID, DEVICE_ID) },
    { 0, }
};

static int pci_probe(struct pci_dev *pdev, const struct pci_device_id *id);
static void pci_remove(struct pci_dev *pdev);

long langdon_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    int ret = 0;
    int retries = 0;
    uint32_t reg_data;
    int channel_number = 0;
    DRIVER_REGISTER_ACCESS_STRUCT reg_access;
    uint32_t int_vector;

    switch (cmd) {

        case IOCTL_GET_VERSION:
            if (copy_to_user((void __user *)arg, &driver_version, sizeof(DRIVER_VERSION_STRUCT)))
                ret = -EFAULT;
            break;

        case IOCTL_READ_REGISTER:
            if (copy_from_user(&reg_access, (void __user *)arg, sizeof(DRIVER_REGISTER_ACCESS_STRUCT)))
                ret = -EFAULT;
            else {
                mutex_lock(&hw_mutex);
                if(reg_access.bar == 0) {
                    reg_access.data = ioread32(bar0_base + reg_access.offset);
                    if (copy_to_user((void __user *)arg, &reg_access, sizeof(DRIVER_REGISTER_ACCESS_STRUCT)))
                        ret = -EFAULT;
                } else if(reg_access.bar == 2){
                    reg_access.data = ioread32(bar2_base + reg_access.offset);
                    if (copy_to_user((void __user *)arg, &reg_access, sizeof(DRIVER_REGISTER_ACCESS_STRUCT)))
                        ret = -EFAULT;
                } else {
                    ret = -EFAULT;
                }
                mutex_unlock(&hw_mutex);
            }
            break;

        case IOCTL_WRITE_REGISTER:
            if (copy_from_user(&reg_access, (void __user *)arg, sizeof(DRIVER_REGISTER_ACCESS_STRUCT)))
                ret = -EFAULT;
            else {
                mutex_lock(&hw_mutex);
                if(reg_access.bar == 0) {
                    iowrite32(reg_access.data, bar0_base + reg_access.offset);
                }
                else if(reg_access.bar == 2) {
                    iowrite32(reg_access.data, bar2_base + reg_access.offset);
                } else {
                    ret = -EFAULT;
                }
                mutex_unlock(&hw_mutex);
            }
            break;
        case IOCTL_USER_INTERRUPT_WAIT:
            if (copy_from_user(&int_vector, (void __user *)arg, sizeof(uint32_t)))
                ret = -EFAULT;
            else {

                DEFINE_WAIT(wait);
                prepare_to_wait(&(user_interrupts[int_vector].waitQueue), &wait, TASK_INTERRUPTIBLE);
                schedule();
                user_interrupts[int_vector].cancelWaitFlag = false;
                printk(KERN_INFO "Langdon FPGA: waiting IRQ on vector: %d\n", int_vector);
                finish_wait(&(user_interrupts[int_vector].waitQueue), &wait);
                if(!user_interrupts[int_vector].cancelWaitFlag) {
                    ret = 0;
                } else {
                    ret = -EFAULT;
                }
            }
            break;

        case IOCTL_USER_INTERRUPT_CANCEL_WAIT:
            if (copy_from_user(&int_vector, (void __user *)arg, sizeof(uint32_t)))
                ret = -EFAULT;
            else {
                printk(KERN_INFO "Langdon FPGA: canceling IRQ wait on vector: %d\n", int_vector);
                user_interrupts[int_vector].cancelWaitFlag = true;
                tasklet_schedule(&(user_interrupts[int_vector].UserIntTasklet));
                ret = 0;
            }
            break;

        case IOCTL_SHUTDOWN_CHANNEL:
            if (copy_from_user(&channel_number, (void __user *)arg, sizeof(uint32_t)))
                ret = -EFAULT;
            else {
                reg_access.offset = ioread32(bar0_base + LANGDON_FPGA_DMA_CHANNEL_N_BASE_OFFSET_REGISTER+(channel_number*sizeof(uint32_t)));
                iowrite32(DMA_CHANNEL_SHUTDOWN_REQ, bar0_base + reg_access.offset + DMA_CHANNEL_CONTROL_REGISTER);

                // verify channel status is back to IDLE within a second, or error out
                do {
                    reg_data = ioread32(bar0_base + reg_access.offset + DMA_CHANNEL_STATUS_REGISTER);
                } while((reg_data & DMA_CHANNEL_STATUS_BUSY) && retries++ < MAX_RETRIES_FOR_1_SEC_TIMEOUT);

                if (reg_data & DMA_CHANNEL_STATUS_BUSY) {
                    printk(KERN_ERR "Langdon FPGA: Error, channel %d Shutdown failed, channel never went IDLE.\n", channel_number);
                    ret = -EFAULT;
                } else {
                    mutex_lock(&hw_mutex);
                    dma_channels[channel_number]->is_open = false;
                    mutex_unlock(&hw_mutex);
                }
            }
            break;

        default:
            printk(KERN_ERR "Langdon FPGA: Error, Invalid IOCTL: 0x%08x.\n", cmd);
            ret = -ENOTTY; // Not a valid ioctl command
            break;
    }

    return ret;
}

long langdon_dma_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    int ret = 0;
    int channel_number = 0;
    int i = 0;
    int retries = 0;
    uint32_t reg_data;
    uint32_t pat_start;
    uint32_t control_reg_bits = 0;
    DRIVER_REGISTER_ACCESS_STRUCT reg_access;
    DMA_CHANNEL_SETUP_STRUCT channel_setup;
    struct inode * pInode = file->f_path.dentry->d_inode;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,11,0))
    DRIVER_DMA_CHANNEL_STRUCT* pchannel = pde_data(pInode);
#else
    DRIVER_DMA_CHANNEL_STRUCT* pchannel = PDE_DATA(pInode);
#endif
    DRIVER_DMA_BUFFER_STRUCT* pbuffer = &pchannel->channel_buffers[0];

    switch (cmd) {

        case IOCTL_GET_VERSION:
            if (copy_to_user((void __user *)arg, &driver_version, sizeof(DRIVER_VERSION_STRUCT)))
                ret = -EFAULT;
            break;

        case IOCTL_READ_REGISTER:
            if (copy_from_user(&reg_access, (void __user *)arg, sizeof(DRIVER_REGISTER_ACCESS_STRUCT)))
                ret = -EFAULT;
            else {
                mutex_lock(&hw_mutex);
                if(reg_access.bar == 0) {
                    reg_access.data = ioread32(bar0_base + reg_access.offset);
                    if (copy_to_user((void __user *)arg, &reg_access, sizeof(DRIVER_REGISTER_ACCESS_STRUCT)))
                        ret = -EFAULT;
                } else if(reg_access.bar == 2){
                    reg_access.data = ioread32(bar2_base + reg_access.offset);
                    if (copy_to_user((void __user *)arg, &reg_access, sizeof(DRIVER_REGISTER_ACCESS_STRUCT)))
                        ret = -EFAULT;
                } else {
                    ret = -EFAULT;
                }
                mutex_unlock(&hw_mutex);
            }
            break;

        case IOCTL_WRITE_REGISTER:
            if (copy_from_user(&reg_access, (void __user *)arg, sizeof(DRIVER_REGISTER_ACCESS_STRUCT)))
                ret = -EFAULT;
            else {
                mutex_lock(&hw_mutex);
                if(reg_access.bar == 0) {
                    iowrite32(reg_access.data, bar0_base + reg_access.offset);
                }
                else if(reg_access.bar == 2) {
                    iowrite32(reg_access.data, bar2_base + reg_access.offset);
                } else {
                    ret = -EFAULT;
                }
                mutex_unlock(&hw_mutex);
            }
            break;

        case IOCTL_OPEN_CHANNEL:

            if (copy_from_user(&channel_setup, (void __user *)arg, sizeof(DMA_CHANNEL_SETUP_STRUCT))) {
                printk(KERN_ERR "Langdon FPGA: Error opening channel, failed to get channel setup struct\n");
                ret = -EFAULT;
            }
            else  {
                if (pchannel != NULL) {
                    printk(KERN_INFO "Langdon FPGA: Opening channel %d\n", pchannel->channel_number);
                    channel_number = pchannel->channel_number;

                    mutex_lock(&hw_mutex);

                    if(pchannel->is_open) {
                        printk(KERN_ERR "Langdon FPGA: Error opening channel (channel already open)\n");
                        ret = -EFAULT;
                    } else {

                        // get channel offset register
                        reg_access.offset = ioread32(bar0_base + LANGDON_FPGA_DMA_CHANNEL_N_BASE_OFFSET_REGISTER+(channel_number*sizeof(uint32_t)));

                        reg_data = ioread32(bar0_base + reg_access.offset + DMA_CHANNEL_STATUS_REGISTER);
                        if (reg_data & DMA_CHANNEL_STATUS_BUSY) {
                            printk(KERN_ERR "Langdon FPGA: Error, channel %d not IDLE when opening.\n", channel_number);
                            ret = -EFAULT;
                        } else {
                            // setup the requested PAT size
                            pchannel->number_of_entries = channel_setup.number_of_PAT_entries;
                            printk(KERN_INFO "Langdon FPGA: opening channel %d with %d PAT entries\n", channel_number, pchannel->number_of_entries);
                            printk(KERN_INFO "Langdon FPGA: channel offset 0x%08x\n", reg_access.offset);

                            // allocate buffers for PAT
                            pat_start = DMA_CHANNEL_PAT_START;
                            for(i = 0; i < pchannel->number_of_entries && ret == 0; i++) {
                                pbuffer->dma_virt_addr = dma_alloc_coherent(&langdon_pdev->dev, DMA_BUFFER_SIZE, &pbuffer->dma_phys_addr, GFP_KERNEL);
                                if(!pbuffer->dma_virt_addr) {
                                    printk(KERN_ERR "Langdon FPGA: Failed to allocate DMA buffer\n");
                                    ret = -ENOMEM;
                                } else {
                                    printk(KERN_INFO "Langdon FPGA: Allocated DMA buffer at virt: %p, DMA: %pad\n", pbuffer->dma_virt_addr, &pbuffer->dma_phys_addr);
                                    memset(pbuffer->dma_virt_addr, 0x55, DMA_BUFFER_SIZE);
                                    // set buffer in PAT
                                    printk(KERN_INFO "Langdon FPGA: setting PAT entry: [%d]:0x%x --- %pad\n", i, (reg_access.offset + pat_start), &pbuffer->dma_phys_addr);
                                    printk(KERN_INFO "Langdon FPGA: writting buffer address at: 0x%08x\n", reg_access.offset + pat_start);
                                    iowrite32((uint32_t)((unsigned long)pbuffer->dma_phys_addr), bar0_base + reg_access.offset + pat_start );
                                    iowrite32((uint32_t)((unsigned long)pbuffer->dma_phys_addr>>32), bar0_base + reg_access.offset + pat_start + sizeof(uint32_t));
                                    pat_start += sizeof(uint64_t);
                                    pbuffer++;
                                }
                            }

                            if(ret == 0) {
                                // set PAT size for channel
                                iowrite32(pchannel->number_of_entries, bar0_base + reg_access.offset + DMA_CHANNEL_PAT_SIZE_REGISTER);
                                // set RP/WP (depending on direction)
                                printk(KERN_INFO "Channel %d direction: %d\n", pchannel->channel_number, pchannel->direction);
                                if(pchannel->direction == H2C) {
                                    // read RP and then make WP = RP
                                    reg_access.data = ioread32(bar0_base + reg_access.offset + DMA_CHANNEL_RP_REGISTER);
                                    iowrite32(reg_access.data, bar0_base + reg_access.offset + DMA_CHANNEL_WP_REGISTER);
                                } else {
                                    // read WP and then make RP = WP
                                    reg_access.data = ioread32(bar0_base + reg_access.offset + DMA_CHANNEL_WP_REGISTER);
                                    iowrite32(reg_access.data, bar0_base + reg_access.offset + DMA_CHANNEL_RP_REGISTER);
                                }

                                // set near empty/full interrupt threshold
                                iowrite32(channel_setup.near_empty_threshold, bar0_base + reg_access.offset + DMA_CHANNEL_EMPTY_THRESHOLD_REGISTER);
                                iowrite32(channel_setup.near_full_threshold, bar0_base + reg_access.offset + DMA_CHANNEL_FULL_THRESHOLD_REGISTER);


                                // set max burst length, packet mode and udp headers
                                control_reg_bits = ((channel_setup.burst_len)<<8);
                                if(channel_setup.channel_mode != 0) control_reg_bits |= DMA_CHANNEL_PACKET_MODE_ENABLE;
                                if(channel_setup.udp_headers != 0) control_reg_bits |= DMA_CHANNEL_USE_UDP_HEADERS;
                                if(channel_setup.near_empty_threshold != 0) control_reg_bits |= DMA_CHANNEL_NEAR_EMPTY_IRQ_ENABLE;
                                if(channel_setup.near_full_threshold != 0) control_reg_bits |= DMA_CHANNEL_NEAR_FULL_IRQ_ENABLE;
                                if(channel_setup.tlast_enable != 0) control_reg_bits |= DMA_CHANNEL_TLAST_IRQ_ENABLE;
                                iowrite32(control_reg_bits, bar0_base + reg_access.offset + DMA_CHANNEL_CONTROL_REGISTER);
                                iowrite32(control_reg_bits | DMA_CHANNEL_ENABLE, bar0_base + reg_access.offset + DMA_CHANNEL_CONTROL_REGISTER);
                                // log RP/WP/CNTRL
                                printk(KERN_INFO "Channel %d, RP: 0x%x, WP: 0x%x, CNTRL: 0x%x\n",
                                    pchannel->channel_number,
                                    ioread32(bar0_base + reg_access.offset + DMA_CHANNEL_RP_REGISTER),
                                    ioread32(bar0_base + reg_access.offset + DMA_CHANNEL_WP_REGISTER),
                                    ioread32(bar0_base + reg_access.offset + DMA_CHANNEL_CONTROL_REGISTER));

                                // set channel_open = true, and current buffer = 0
                                pchannel->is_open = true;
                                pchannel->current_buffer = 0;
                            }
                        }
                    }

                    mutex_unlock(&hw_mutex);
                } else {
                    printk(KERN_ERR "Langdon FPGA: Error getting channel properties from device file.\n");
                    ret = -EFAULT;
                }
            }
            break;

        case IOCTL_CLOSE_CHANNEL:

            if (pchannel != NULL) {
                printk(KERN_INFO "Langdon FPGA: Closing channel %d\n", pchannel->channel_number);
                channel_number = pchannel->channel_number;

                mutex_lock(&hw_mutex);

                if(!pchannel->is_open) {
                    printk(KERN_ERR "Langdon FPGA: Error closing channel (channel not open)\n");
                    ret = -EFAULT;
                } else {

                    // get channel offset register
                    reg_access.offset = ioread32(bar0_base + LANGDON_FPGA_DMA_CHANNEL_N_BASE_OFFSET_REGISTER+(channel_number*sizeof(uint32_t)));

                    // verify channel status is back to IDLE when closed within a second, or error out
                    do {
                        reg_data = ioread32(bar0_base + reg_access.offset + DMA_CHANNEL_STATUS_REGISTER);
                    } while((reg_data & DMA_CHANNEL_STATUS_BUSY) && retries++ < MAX_RETRIES_FOR_1_SEC_TIMEOUT);

                    if (reg_data & DMA_CHANNEL_STATUS_BUSY) {
                      printk(KERN_ERR "Langdon FPGA: Error, channel %d never went IDLE when disabling.\n", channel_number);
                      ret = -EFAULT;
                    } else {
                        pchannel->is_open = false;

                        // set disable bit
                        iowrite32(0x00, bar0_base + reg_access.offset + DMA_CHANNEL_CONTROL_REGISTER);
                        // set PAT size to 0
                        iowrite32(0, bar0_base + reg_access.offset + DMA_CHANNEL_PAT_SIZE_REGISTER);

                        // deallocate buffers for PAT
                        for(i = 0; i < pchannel->number_of_entries; i++) {
                            if(pbuffer->dma_virt_addr) {
                                dma_free_coherent(&langdon_pdev->dev, DMA_BUFFER_SIZE, pbuffer->dma_virt_addr, pbuffer->dma_phys_addr);
                            }
                            pbuffer++;
                        }
                    }
                }

                mutex_unlock(&hw_mutex);

            } else {
                printk(KERN_ERR "Langdon FPGA: Error getting channel properties from device file.\n");
                ret = -EFAULT;
            }
            break;

        default:
            ret = -ENOTTY; // Not a valid ioctl command
            break;
    }

    return ret;
}


static int langdon_mmmap(struct file *file, struct vm_area_struct *vma) {

    struct inode * pInode = file->f_path.dentry->d_inode;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,11,0))
    DRIVER_DMA_CHANNEL_STRUCT* pchannel = pde_data(pInode);
#else
    DRIVER_DMA_CHANNEL_STRUCT* pchannel = PDE_DATA(pInode);
#endif
    dma_addr_t dma_addr;
    unsigned long size;
    void* dma_virt;
    int ret;

    mutex_lock(&hw_mutex);

    dma_addr = pchannel->channel_buffers[pchannel->current_buffer].dma_phys_addr;
    size = DMA_BUFFER_SIZE;

    dma_virt = pchannel->channel_buffers[pchannel->current_buffer].dma_virt_addr;

    ret = dma_mmap_coherent(&langdon_pdev->dev, vma, dma_virt, dma_addr, size);
    if (ret < 0) {
        pr_err("Failed to map DMA buffer to user space\n");
        mutex_unlock(&hw_mutex);
        return ret;
    }

    // printk(KERN_INFO "Langdon FPGA: mapping buffer %d for channel %d. Physical: %pad\n",
    //         pchannel->current_buffer, pchannel->channel_number, &dma_addr);

    pchannel->current_buffer++;
    if(pchannel->current_buffer == pchannel->number_of_entries) pchannel->current_buffer = 0;
    mutex_unlock(&hw_mutex);

    return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,10,0))

    static const struct file_operations fops = {
        .owner = THIS_MODULE,
        .unlocked_ioctl = langdon_ioctl,
    };

    static struct proc_ops dev_fops = {
        .proc_ioctl = langdon_ioctl,
#ifdef CONFIG_COMPAT
	    .proc_compat_ioctl = langdon_ioctl,
#endif
    };

    static struct proc_ops procfs_DMAfops = {
        .proc_ioctl	= langdon_dma_ioctl,
#ifdef CONFIG_COMPAT
	    .proc_compat_ioctl = langdon_dma_ioctl,
#endif
        .proc_mmap = langdon_mmmap
    };

#else
    static const struct file_operations fops = {
        .owner = THIS_MODULE,
        .compat_ioctl = langdon_ioctl,
    };

    static struct file_operations dev_fops = {
        .compat_ioctl = langdon_ioctl,
    };

    static struct file_operations procfs_DMAfops = {
        .compat_ioctl	= langdon_dma_ioctl,
        .mmap = langdon_mmmap
    };
#endif


#if 0
static void user_interrupt_tasklet(unsigned long ctx) {

    DRIVER_USER_INTERRUPT_DATA_STRUCT* pIntData = (DRIVER_USER_INTERRUPT_DATA_STRUCT*)ctx;
    if(waitqueue_active(&pIntData->waitQueue)) {
        wake_up(&pIntData->waitQueue);
    }
}


static irqreturn_t langdon_interrupt_handler(int irq, void *dev_id) {
    DRIVER_USER_INTERRUPT_DATA_STRUCT* pinterrupt_data = (DRIVER_USER_INTERRUPT_DATA_STRUCT*)dev_id;
    uint32_t msg = ioread32(bar2_base + pinterrupt_data->vector_table_offset + LANGDON_FPGA_INTERRUPT_VECTOR_MSG_OFFSET);

    printk(KERN_INFO "Langdon FPGA: IRQ (%d|0x%x) Handled. MSG=0x%x\n",
            pinterrupt_data->pmsix_entry->entry,
            pinterrupt_data->pmsix_entry->vector,
            msg);

    // clear interrupt bit for current vector
    iowrite32((1<<pinterrupt_data->pmsix_entry->entry), bar0_base + LANGDON_INTERRUPT_STATUS_REGISTER);
    tasklet_schedule(&(pinterrupt_data->UserIntTasklet));
    return IRQ_HANDLED;
}

#endif

static int pci_probe(struct pci_dev *pdev, const struct pci_device_id *id) {
    int ret;
    resource_size_t bar_start, bar_len;
    struct proc_dir_entry *pEntry;
    char DMAName[32];
    int num_channels = 0;
    int i = 0;
//    int j = 0;
    uint32_t reg_offset = 0;
    uint32_t reg_data = 0;
    DRIVER_DMA_CHANNEL_STRUCT* pchannel = NULL;

    printk(KERN_INFO "Langdon FPGA: PCIe device detected\n");

    printk(KERN_INFO "Langdon FPGA: interrupts commented out.\n");

    mutex_init(&hw_mutex);

    // map BAR0 for register access
    if (!(pci_resource_flags(pdev, 0) & IORESOURCE_MEM)) {
        printk(KERN_ERR "BAR0 is not an IO memory region\n");
        return -ENODEV;
    }
    else {
        // Get the start and length of BAR0
        bar_start = pci_resource_start(pdev, 0);
        bar_len = pci_resource_len(pdev, 0);

        // Map BAR0 to kernel virtual memory
        bar0_base = ioremap(bar_start, bar_len);
        if (!bar0_base) {
            printk(KERN_ERR "Failed to map BAR0\n");
            return -ENOMEM;
        }
        else {
            printk(KERN_INFO "Langdon FPGA: BAR0 successfully mapped. len = %d (0x%08x)\n", (int)bar_len, (int)bar_len);
        }
    }

    // map BAR2 for register access
    if (!(pci_resource_flags(pdev, 2) & IORESOURCE_MEM)) {
        printk(KERN_ERR "BAR2 is not an IO memory region\n");
        return -ENODEV;
    }
    else {
        // Get the start and length of BAR2
        bar_start = pci_resource_start(pdev, 2);
        bar_len = pci_resource_len(pdev, 2);

        // Map BAR2 to kernel virtual memory
        bar2_base = ioremap(bar_start, bar_len);
        if (!bar2_base) {
            printk(KERN_ERR "Failed to map BAR2\n");
            return -ENOMEM;
        }
        else {
            printk(KERN_INFO "Langdon FPGA: BAR2 successfully mapped. len = %d (0x%08x)\n", (int)bar_len, (int)bar_len);
        }
    }

    ret = pci_enable_device(pdev);
    if (ret) {
        printk(KERN_ERR "Langdon FPGA: Cannot enable PCI device\n");
        return ret;
    }

    pci_set_master(pdev);

    // Check if MSI-X is supported
    if(pdev->irq != 0) {

		printk(KERN_INFO "pdev->irq != 0, but skipping interrupt support for now.\n");

#if 0
        for(j = 0; j < MAX_NUM_MSIX_VECTORS; j++) {
            MSIXEntries[j].entry = j;
            user_interrupts[j].pmsix_entry = &MSIXEntries[j];
            user_interrupts[j].channel_number = j;
            user_interrupts[j].vector_table_offset = LANGDON_FPGA_INTERRUPT_VECTOR_0_OFFSET + j*(LANGDON_FPGA_INTERRUPT_VECTOR_ENTRY_LEN);
            // Setup the user interrupt tasklet
            tasklet_init(&(user_interrupts[j].UserIntTasklet), user_interrupt_tasklet, (unsigned long)&user_interrupts[j]);
            init_waitqueue_head(&(user_interrupts[j].waitQueue));
            // mask all interrupts until users register a callback
            iowrite32(1, bar2_base + user_interrupts[j].vector_table_offset + LANGDON_FPGA_INTERRUPT_VECTOR_CONTROL_OFFSET);
        }

        ret = pci_enable_msix_exact(pdev, MSIXEntries, j);
        if (ret != 0) {
            printk(KERN_ALERT "MSI-X not supported or enable failed. ret = %d\n", ret);
            return -ENODEV;
        } else {
            printk(KERN_INFO "pci_enable_msix_exact success. # of vectors: %d\n", j);
            for(j = 0; j < MAX_NUM_MSIX_VECTORS; j++) {
                printk(KERN_INFO "langdon entry %d is vector: 0x%x (0x%x), offset: 0x%x\n",
                        MSIXEntries[j].entry, MSIXEntries[j].vector,
                        user_interrupts[j].pmsix_entry->vector, user_interrupts[j].vector_table_offset);
            }
        }
#endif
    }

    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) {
        printk(KERN_ALERT "Failed to register a major number\n");
        return major_number;
    }

    // Create a character device
    if (alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME) < 0) {
        printk(KERN_ALERT "Failed to allocate a major number\n");
        return -1;
    }
    cdev_init(&simple_cdev, &fops);
    cdev_add(&simple_cdev, dev_num, 1);
    simple_cdev.owner = THIS_MODULE;

    // Create a device class
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,5,0))
    device_class = class_create(CLASS_NAME);
#else
    device_class = class_create(THIS_MODULE, CLASS_NAME);
#endif
    if (IS_ERR(device_class)) {
        printk(KERN_ALERT "Failed to create device class\n");
        return -1;
    }

    // Create the device file
    device = device_create(device_class, NULL, dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(device)) {
        printk(KERN_ALERT "Failed to create the device\n");
        return -1;
    }

    // save pdev
    langdon_pdev = pdev;

    printk(KERN_INFO "Langdon FPGA revision: 0x%08x\n", ioread32(bar0_base + DMA_CHANNEL_REVISION_REGISTER));

    // create proc_fs entries for generic register r/w access and each DMA channel
    langdon_procFs = proc_mkdir(DEVICE_NAME, NULL);
    if (langdon_procFs == NULL) {
        printk(KERN_ALERT "Unable to create procfs entry\n");
        return -ENOMEM;
    }

    pEntry = proc_create_data((char*)"dev", S_IFREG | S_IRUGO | S_IWUGO, langdon_procFs, &dev_fops, NULL );
    if (pEntry == NULL) {
        printk(KERN_ALERT "Unable to create procfs langdon device entry\n");
        remove_proc_entry(DEVICE_NAME, NULL);
        return -ENOMEM;
    }

    num_channels = ioread32(bar0_base + LANGDON_FPGA_NUM_CHANNELS_REGISTER);
    printk(KERN_INFO "Found %d DMA Channels\n", num_channels);

    dma_channels = kmalloc(sizeof(DRIVER_DMA_CHANNEL_STRUCT*)*num_channels, GFP_KERNEL);


    for (i = 0; i < num_channels; i++) {
        // get channel register offset
        reg_offset = ioread32(bar0_base + LANGDON_FPGA_DMA_CHANNEL_N_BASE_OFFSET_REGISTER+(i*sizeof(uint32_t)));
        printk(KERN_INFO "Setting up channel %d. Register offset=0x%08x\n", i, reg_offset);

        // get max PAT size
        reg_data = ioread32(bar0_base + reg_offset + DMA_CHANNEL_PAT_MAX_SIZE_REGISTER);
        if(reg_data == 0) {
            printk(KERN_ALERT "Error: DMA Channel has %d Max PAT entries.\n", reg_data);
            continue;
        }

        printk(KERN_INFO "allocating memory for %d PAT entries for channel %d\n", reg_data, i);
        pchannel = (DRIVER_DMA_CHANNEL_STRUCT*)kmalloc(sizeof(DRIVER_DMA_CHANNEL_STRUCT) + reg_data*(sizeof(DRIVER_DMA_BUFFER_STRUCT)), GFP_KERNEL);
        if(pchannel == NULL) {
            printk(KERN_ALERT "Unable to allocate memory for pchannel object\n");
            continue;
        }
        pchannel->channel_number = i;
        pchannel->is_open = false;
        pchannel->current_buffer = 0;
        pchannel->number_of_entries = reg_data;
        // get direction for channel
        reg_data = ioread32(bar0_base + reg_offset + DMA_CHANNEL_CONTROL_REGISTER);
        pchannel->direction = (reg_data & 0x02) >> 1;
        sprintf((char*)DMAName, "%s%d", "DMA_", i);
        pEntry = proc_create_data((char*)DMAName, S_IFREG | S_IRUGO | S_IWUGO, langdon_procFs, &procfs_DMAfops, pchannel);
        if (pEntry == NULL) {
            printk(KERN_ALERT "Unable to create procfs DMA Channel %d entry\n", i);
            continue;
        }
        dma_channels[i] = pchannel;
        printk(KERN_INFO "successfully created proc_fs for channel %d\n", i);
    }

#if 0
    for( i = 0; i < MAX_NUM_MSIX_VECTORS; i++) {
        ret = request_irq(user_interrupts[i].pmsix_entry->vector, langdon_interrupt_handler, IRQF_SHARED, "langdon_fpga", &user_interrupts[i]);
        if (ret) {
            printk(KERN_ALERT "Failed to request irq: 0x%x. ret = %d\n", user_interrupts[i].pmsix_entry->vector, ret);
            return ret;
        } else {
            irq_enabled = true;
            printk(KERN_INFO "Successfully enabled interrupt vector: 0x%x.\n", user_interrupts[i].pmsix_entry->vector);
        }
    }
#endif

    printk(KERN_INFO "successfully initialized Langdon FPGA device driver.\n");
    return 0;
}

static void pci_remove(struct pci_dev *pdev)
{

    char DMAName[32];
    int num_channels = 0;
    int i = 0;

#if 0
    printk(KERN_INFO "Releasing MSI-X interrupts.\n");

    // Release MSI-X interrupts
    if(irq_enabled) {
        for(i = 0; i < MAX_NUM_MSIX_VECTORS; i++) {
          printk(KERN_INFO "langdon_fpga, disabling (and freeing) interrupt vector: 0x%x.\n", MSIXEntries[i].vector);
          disable_irq(MSIXEntries[i].vector);
          free_irq(MSIXEntries[i].vector, &user_interrupts[i]);
        }
        irq_enabled = false;
    }

    // now disable msix interrupts
    printk(KERN_INFO "Disabling MSI-X interrupts.\n");
    pci_disable_msix(pdev);

#endif

    // remove proc_fs
    printk(KERN_INFO "Removing proc_fs entries.\n");
    if (langdon_procFs != NULL) {

        remove_proc_entry("dev", langdon_procFs);

        num_channels = ioread32(bar0_base + LANGDON_FPGA_NUM_CHANNELS_REGISTER);
        for (i = 0; i < num_channels; i++) {
            sprintf((char*)DMAName, "%s%d", "DMA_", i);
            remove_proc_entry(DMAName, langdon_procFs);
            // free memory for channels
            kfree(dma_channels[i]);
        }
        kfree(dma_channels);

        printk(KERN_INFO "Removing /proc entry %s.\n", DEVICE_NAME);
        remove_proc_entry(DEVICE_NAME, NULL);
        langdon_procFs = NULL;
    }

    device_destroy(device_class, dev_num);
    class_destroy(device_class);
    cdev_del(&simple_cdev);
    unregister_chrdev(major_number, DEVICE_NAME);

    pci_disable_device(pdev);
    printk(KERN_INFO "Langdon FPGA: PCIe device removed\n");
}

static struct pci_driver pci_driver = {
    .name = DEVICE_NAME,
    .id_table = pci_ids,
    .probe = pci_probe,
    .remove = pci_remove,
};

module_pci_driver(pci_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("A2e");
MODULE_DESCRIPTION("PCI driver for Langdon FPGA");
MODULE_DEVICE_TABLE(pci, pci_ids);

