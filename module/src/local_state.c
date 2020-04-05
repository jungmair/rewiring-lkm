#include <linux/kernel.h>
#include <linux/gfp.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include "communication.h"
#include "local_state.h"

void init_local_state(struct local_state *state)
{
	//initialize values with zero
	state->mapping = NULL;
	state->vpages_count = 0;
}

void release_local_state(struct local_state *state)
{
	//free mapping array
	vfree(state->mapping);
}

int resize_mapping(struct local_state *state, unsigned long length)
{
	if (length == state->vpages_count)
		return true;
	//allocate larger array for page ids
	PageId *newPageIds = vmalloc(sizeof(PageId) * length);
	if(newPageIds==NULL){
	    return false;
	}
	//migrate page ids to new array
	memcpy(newPageIds, state->mapping,
	       sizeof(PageId) * min(state->vpages_count, length));
	//set all other page ids to unassigned
	if (length > state->vpages_count) {
		memset(&newPageIds[state->vpages_count], 0xff,
		       (length - state->vpages_count) * sizeof(PageId));
	}
	//replace mapping with resized array
	vfree(state->mapping);
	state->mapping = newPageIds;
	//set new length of mapping
	state->vpages_count = length;
	return true;
}

PageId get_page_id(struct local_state *state, unsigned long offset)
{
	//check if offset is low enough
	if (offset >= state->vpages_count) {
		return PAGEID_OFFSET_INVALID; //out of bounds -> return special page id
	}
	return state->mapping[offset];
}

void set_page_id(struct local_state *state, unsigned long offset, PageId pageId)
{
	//first: get old page id
	PageId previous = get_page_id(state, offset);
	if (previous == PAGEID_OFFSET_INVALID)
		return; //offset is invalid-> can not set page id
	if (previous != PAGEID_UNASSIGNED) {
		//we replace the previous page -> decrement usage count of previous
		dec_usage(state->global, previous);
	}
	//actually set page id
	state->mapping[offset] = pageId;
	if (pageId != PAGEID_UNASSIGNED) {
		//if "real" page: increment usage count
		inc_usage(state->global, pageId);
	}
}