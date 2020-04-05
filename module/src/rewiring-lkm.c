#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/page_ref.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/version.h>
#include <linux/mutex.h>

#include "communication.h"
#include "global_state.h"
#include "local_state.h"

#define DEVICE_NAME "rewiring"
#define CLASS_NAME  "rewiring"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Michael Jungmair");
MODULE_DESCRIPTION("A kernel module implementing rewiring");
MODULE_VERSION("0.1");

//internal structures for registering the lkm:
static int majorNumber;
static struct class *rewiring_lkm_class = NULL;
static struct device *rewiring_lkm_device = NULL;

static int dev_open(struct inode *, struct file *);

static int dev_release(struct inode *, struct file *);

static long dev_unlocked_ioctl(struct file *file, unsigned int cmd,
			       unsigned long arg);

static int dev_mmap(struct file *filep, struct vm_area_struct *vma);
static void dev_mmap_close(struct vm_area_struct *vma);

static vm_fault_t fault(struct vm_fault *vmf);

vm_fault_t dev_page_mkwrite(struct vm_fault *vmf);

//file operations provided by the module
static struct file_operations fops = {
	.open = dev_open,
	.release = dev_release,
	.unlocked_ioctl = dev_unlocked_ioctl,
	.mmap = dev_mmap,
};
//vm operations provided by the module
static struct vm_operations_struct simple_vm_ops = {
    .fault = fault,
	.page_mkwrite =dev_page_mkwrite,
	.close = dev_mmap_close
};

//helper struct
struct mem_info {
	struct local_state *state;
	struct vm_area_struct *vma;
	struct mm_struct *mm;
};

//init function for module
static int __init rewiring_lkm_init(void)
{
	printk(KERN_INFO "REWIRING_LKM: Initializing\n");
	//allocate major number
	majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
	if (majorNumber < 0) {
		printk(KERN_ALERT
		       "REWIRING_LKM: Failed to register a major number\n");
		return majorNumber;
	}
	//register device class
	rewiring_lkm_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(rewiring_lkm_class)) {
		unregister_chrdev(majorNumber, DEVICE_NAME);
		printk(KERN_ALERT
		       "REWIRING_LKM: Failed to register device class\n");
		return (int)PTR_ERR(rewiring_lkm_class);
	}
	//register the device
	rewiring_lkm_device =
		device_create(rewiring_lkm_class, NULL, MKDEV(majorNumber, 0),
			      NULL, DEVICE_NAME);
	if (IS_ERR(rewiring_lkm_device)) {
		class_destroy(rewiring_lkm_class);
		unregister_chrdev(majorNumber, DEVICE_NAME);
		printk(KERN_ALERT
		       "REWIRING_LKM: Failed to create the device\n");
		return PTR_ERR(rewiring_lkm_device);
	}
	return 0;
}
//exit function for module
static void __exit rewiring_lkm_exit(void)
{
	//cleanup major/minor numbers
	device_destroy(rewiring_lkm_class, MKDEV(majorNumber, 0u));
	class_unregister(rewiring_lkm_class);
	class_destroy(rewiring_lkm_class);
	unregister_chrdev(majorNumber, DEVICE_NAME);
}

static int dev_open(struct inode *inodep, struct file *filep)
{
	//called, when /dev/rewiring is opened
	//create global_state, initialize it and attach it to the file pointer
	struct global_state *state =
		kmalloc(sizeof(struct global_state), GFP_KERNEL);
	if(state==NULL){
        printk(KERN_WARNING "REWIRING_LKM: could not create internal structures, out of memory!\n");
        return 1;
	}
	init_global_state(state);
	filep->private_data = state;
	return 0;
}

static int dev_release(struct inode *inodep, struct file *filep)
{
	if (filep->private_data) {
		//cleanup
		release_global_state(filep->private_data);
	}
	return 0;
}

vm_fault_t dev_page_mkwrite(struct vm_fault *vmf)
{
	//features like "copy-on-write" could be implemented here
	return VM_FAULT_LOCKED;
}

static vm_fault_t fault(struct vm_fault *vmf)
{
	//handles page faults
	//get local state from vm_fault
	struct vm_area_struct *vma = vmf->vma;
	struct local_state *state = vma->vm_private_data;
	//lock global state
	mutex_lock(&state->global->lock);
	unsigned long pos = vmf->pgoff - vma->vm_pgoff;
	PageId pageId = get_page_id(state, pos);
	if (pageId == PAGEID_OFFSET_INVALID) {
		//problematic page id ->unlock, log, segfault
		mutex_unlock(&state->global->lock);
		printk(KERN_WARNING "REWIRING_LKM: invalid offset %lu\n", pos);
		return VM_FAULT_SIGSEGV;
	}
	if (pageId == PAGEID_UNASSIGNED) {
		//fault on previously unassigned page -> alloc new page
		pageId = alloc_new_page(state->global);
        if (pageId == PAGEID_UNASSIGNED) {
            printk(KERN_WARNING "REWIRING_LKM: could not allocate new page\n");
            return VM_FAULT_SIGSEGV;
        }
            set_page_id(state, pos, pageId);
	}
	//retrieve kaddr for page id
	unsigned long kaddr = 0;
	bool validKaddr = kaddr_by_pageId(state->global, pageId, &kaddr);
	if (!validKaddr) {
		mutex_unlock(&state->global->lock);
		return VM_FAULT_SIGSEGV;
	}
	//create page protection flags matching the "mmap protection flags"
	pgprot_t prot = vm_get_page_prot(vmf->vma->vm_flags);
	//create page table entry and insert it into page table -> fault handled
	vm_fault_t res = vmf_insert_pfn_prot(
		vmf->vma, vmf->address, page_to_pfn(virt_to_page(kaddr)), prot);
	//unlock before returning
	mutex_unlock(&state->global->lock);
	return res;
}

static int dev_mmap(struct file *filep, struct vm_area_struct *vma)
{
	//set handlers for page faults etc
	vma->vm_ops = &simple_vm_ops;
	//enable delete_page_range
	vma->vm_flags |= VM_PFNMAP;
	//create local state object and store a pointer to it in the vm_area_struct
	vma->vm_private_data = kmalloc(sizeof(struct local_state), GFP_KERNEL);
	//retrieve local state object
	struct local_state *state = vma->vm_private_data;
    if(state==NULL){
        printk(KERN_WARNING "REWIRING_LKM: could not create internal structures, out of memory!\n");
        return 1;
    }
	//init local state
	init_local_state(state);
	//link global state
	state->global = filep->private_data;

    if(!resize_mapping(state, (vma->vm_end - vma->vm_start))){
        printk(KERN_WARNING "REWIRING_LKM: could not create mapping storage!\n");
        return 1;
    }
	return 0;
}

static void dev_mmap_close(struct vm_area_struct *vma)
{
	release_local_state(vma->vm_private_data);
}

inline int populate_(pte_t *pte, unsigned long addr, void *data)
{
	//callback function for populating the pagetable
	struct mem_info *info = data;
	unsigned long pos = (addr - info->vma->vm_start) / 4096;
	PageId pageId = get_page_id(info->state, pos);

	if (pageId == PAGEID_OFFSET_INVALID) {
		//page requested is at invalid offset (should not happen)
		printk(KERN_WARNING "REWIRING_LKM: invalid offset at %lu\n",
		       pos);
		return 0;
	}
	if (pageId == PAGEID_UNASSIGNED) {
		//no page requested
		return 0;
	}
	unsigned long kaddr = 0;
	bool validKaddr = kaddr_by_pageId(info->state->global, pageId, &kaddr);
	if (!validKaddr) {
        //no valid kaddr-> do nothing
		return 0;
	}
    //calculate page protection flags
	pgprot_t prot = vm_get_page_prot(info->vma->vm_flags);
    //create page table entry
	pte_t pte_val =
		pte_mkdevmap(pfn_pte(page_to_pfn(virt_to_page(kaddr)), prot));
    //set page table entry
	*pte = pte_val;
	return 0;
}
//depending on the linux kernel version, the callback signature varies
//make backwards compatible
#if LINUX_VERSION_CODE <= KERNEL_VERSION(5, 1, 0)
int populate(pte_t *pte, pgtable_t token, unsigned long addr, void *data)
{
	populate_(pte, addr, data);
}
#else
int populate(pte_t *pte, unsigned long addr, void *data)
{
	return populate_(pte, addr, data);
}
#endif

static void unmap_page_range(const struct mem_info *info, unsigned long pages,
			     unsigned long startAddr)
{
	//deletes all page table entries in a vm_area_struct in a specified range
	zap_vma_ptes(info->vma, startAddr, pages * 4096);
}

static void update_page_range(struct mem_info *info, unsigned long start,
			      unsigned long pages)
{
	//updates a certain page range
	unsigned long startAddr =
		info->vma->vm_start + start * 4096 - info->vma->vm_pgoff * 4096;
	//first: clear page table areas
	unmap_page_range(info, pages, startAddr);
	//then populate again using updated page ids
	apply_to_page_range(info->mm, startAddr, pages * 4096, populate, info);
}
static long handle_command(struct file *file, struct cmd *command)
{
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma;
	if (command->type != CREATE_PAGE_IDS) {
		//search for the vm_area_struct representing the mapping
		vma = find_vma(mm, (unsigned long)command->mapping_start);
		//error handling
		if (vma == NULL) {
			return -1;
		}
		if (vma->vm_file != file) {
			return -1;
		}
	} else {
		//CREATE_PAGE_IDS does not need a local state/mapping
	}
	struct local_state *state = vma->vm_private_data;

	switch (command->type) {
	case SET_PAGE_IDS: {
		//retrieve arguments: start,len, ptr to array in userspace
		//check that parameters are valid
		if (command->start + command->len > state->vpages_count) {
			return -1;
		}

		//create temporary array in kernel space and copy data
		PageId *newPageIds = vmalloc(command->len * sizeof(PageId));
		if(newPageIds==NULL){
            printk(KERN_WARNING "REWIRING_LKM: could not allocate memory for temporary storage!\n");
            return -1;
        }
		copy_from_user(newPageIds, command->payload,
			       command->len * sizeof(PageId));
		//process data
		for (unsigned long i = 0; i < command->len; i++) {
			set_page_id(state, command->start + i, newPageIds[i]);
		}
		//free temporary array
		vfree(newPageIds);
		//put important infos into internal structure
		struct mem_info info = {
			.state = vma->vm_private_data,
			.vma = vma,
			.mm = mm,
		};
		//update relevant page range
		update_page_range(&info, command->start, command->len);
	} break;
	case GET_PAGE_IDS: {
		//check that parameter are valid
		if (command->start + command->len > state->vpages_count) {
			return -1;
		}
		//copy part of state->mapping to the userspace
		copy_to_user(command->payload, &state->mapping[command->start],
			     command->len * sizeof(PageId));
	} break;
	case CREATE_PAGE_IDS: {
		for (unsigned int i = 0; i < command->len; i++) {
			PageId pageId = alloc_new_page(file->private_data);
			if(pageId==PAGEID_UNASSIGNED){
                printk(KERN_WARNING "REWIRING_LKM: could not allocate page!\n");
                return -1;
            }
			PageId *target_arr = command->payload;
			copy_to_user(&target_arr[i], &pageId, sizeof(PageId));
		}
	} break;
	default:
		break;
	}
	return 0;
}
/**
 * handles ioctl calls
 * @param file file struct pointer
 * @param cmd ioctl command, has to be REW_CMD
 * @param arg pointer to a struct cmd
 * @return 0, if everything works,1 otherwise
 */
static long dev_unlocked_ioctl(struct file *file, unsigned int cmd,
			       unsigned long arg)
{
	if (cmd != REW_CMD) {
		printk(KERN_WARNING "REWIRING_LKM: unknown cmd!");
		return -1;
	}
	struct cmd command;
	//retrieve command
	copy_from_user(&command, (struct cmd *)arg, sizeof(struct cmd));
	//lock module
	struct global_state *state = (struct global_state *)file->private_data;
	mutex_unlock(&state->lock);
	long res = handle_command(file, &command);
	mutex_unlock(&state->lock);
	return res;
}

//register init/exit functions of module
module_init(rewiring_lkm_init) module_exit(rewiring_lkm_exit)
