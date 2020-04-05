#include <linux/kernel.h>
#include <linux/gfp.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include "global_state.h"
#include "communication.h"

void init_global_state(struct global_state *state)
{
	//set values to zero/NULL
	state->pageInfos = NULL;
	state->page_info_size = 0;
	state->ppages_count = 0;
	//init lock
	mutex_init(&state->lock);
}

void release_global_state(struct global_state *state)
{
	//free physical pages
	for (size_t i = 0; i < state->ppages_count; i++) {
		free_page_info(&state->pageInfos[i]);
	}
	//free arrays
	vfree(state->pageInfos);
	//destroy lock
	mutex_destroy(&state->lock);
}

int resize_page_info_arr(struct global_state *state)
{
	unsigned long oldEntries = state->page_info_size;
	//grow by a factor of two
	unsigned long newEntries = oldEntries == 0 ? 1 : (oldEntries * 2);
	//alloc larger array
	struct page_info *newArr =
		vmalloc(newEntries * sizeof(struct page_info));
	if(newArr==NULL){
	    return false;
	}
	//copy old entries
	memcpy(newArr, state->pageInfos, oldEntries * sizeof(struct page_info));
	//set new ones to zero
	memset(&newArr[oldEntries], 0,
	       (newEntries - oldEntries) * sizeof(struct page_info));
	//free old array
	vfree(state->pageInfos);
	//set larger array
	state->page_info_size = newEntries;
	state->pageInfos = newArr;
	return true;
}
void free_page_info(struct page_info *info)
{
	if (info->valid) {
		//return page to kernel
		free_page(info->kaddr);
	}
}

PageId alloc_new_page(struct global_state *state)
{
	if (state->ppages_count == state->page_info_size) {
		//we need to enlarge the array of page infos first
		if(!resize_page_info_arr(state)){
		    return PAGEID_UNASSIGNED;
		}
	}
	//"allocate" new page id
	PageId pageId = state->ppages_count++;
	//get fresh page from kernel
	state->pageInfos[pageId].kaddr = get_zeroed_page(GFP_KERNEL);
	if(state->pageInfos[pageId].kaddr==0){
	    return PAGEID_UNASSIGNED;
	}
	state->pageInfos[pageId].valid = true;
	return pageId;
}

void inc_usage(struct global_state *state, PageId pageId)
{
	if (pageId > state->page_info_size) {
		//check for page id to be in range
		printk(KERN_ALERT "REWIRING_LKM: invalid pageId:%u\n", pageId);
		return;
	}
	state->pageInfos[pageId].usage_count++;
}

void dec_usage(struct global_state *state, PageId pageId)
{
	state->pageInfos[pageId].usage_count--;
}

int kaddr_by_pageId(struct global_state *state, PageId pageId,
		    unsigned long *kaddr)
{
	//page id is too high -> return false
	if (pageId >= state->page_info_size) {
        printk(KERN_ALERT "REWIRING_LKM: invalid pageId:%u\n", pageId);
		return false;
	}
	//get info for page id
	struct page_info *pageInfo = &state->pageInfos[pageId];
	//check if page info is valid
	if (pageInfo->valid) {
		//return kaddr
		*kaddr = pageInfo->kaddr;
		return true;
	}
	//page info is not valid -> return false
	return false;
}