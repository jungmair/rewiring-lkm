#ifndef REWIRING_LOCAL_STATE_H
#define REWIRING_LOCAL_STATE_H

#include "global_state.h"

struct local_state {
	//number of virtual pages
	unsigned long vpages_count;

	//mapping of virtual pages to physical pages via page ids
	PageId *mapping;

	//link to global (per-file) state
	struct global_state *global;
};

/**
 * Initializes the given state
 * @param state pointer to be initialized
 */
void init_local_state(struct local_state *state);

/**
 * releases all ressources associated with the state
 * @param state the state to be released
 */
void release_local_state(struct local_state *state);

/**
 * resizes the mapping of the state to a new length
 * @param state the state
 * @param length the new length
 */
int resize_mapping(struct local_state *state, unsigned long length);

/**
 * Returns the page id for a given mapping offset
 * @param state the state
 * @param offset the offset inside the mapping area
 * @return the page id or PAGEID_OFFSET_INVALID
 */
PageId get_page_id(struct local_state *state, unsigned long offset);

/**
 * sets the page id at a given mapping offset and performs some checks
 * @param state  the state
 * @param offset  the offset inside the mapping area
 * @param pageId the page id to set
 */
void set_page_id(struct local_state *state, unsigned long offset,
		 PageId pageId);

#endif //REWIRING_LOCAL_STATE_H
