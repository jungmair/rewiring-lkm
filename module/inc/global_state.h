#ifndef REWIRING_GLOBAL_STATE_H
#define REWIRING_GLOBAL_STATE_H
#include <linux/mutex.h>
typedef unsigned PageId;

/**
 * Everything we store about physical pages
 */
struct page_info {
	/**
     * Is this page information valid?
     */
	int valid;
	/**
     * how often is this physical page used, i.e.  mapped into virtual address space
     */
	unsigned long usage_count;
	/**
     * kernel address
     */
	unsigned long kaddr;
};
struct global_state {
	//number of physical pages
	unsigned long ppages_count;
	//size of page_info array
	unsigned long page_info_size;
	//array of "physical pages"
	struct page_info *pageInfos;
	//lock per file
	struct mutex lock;
};

void free_page_info(struct page_info *info);

/**
 * allocates a new page and returns a pageId
 * @param state the state for which a new physical page is requested
 * @return the page id
 */
PageId alloc_new_page(struct global_state *state);

/**
 * increases the usage count of the page information associated with the pageId
 * @param state the state object to which the page belongs
 * @param pageId the pageId which usage is incremented
 */
void inc_usage(struct global_state *state, PageId pageId);

/**
 * decreases the usage count of the page information associated with the pageId
 * @param state the state object to which the page belongs
 * @param pageId the pageId which usage is decremented
 */
void dec_usage(struct global_state *state, PageId pageId);

/**
 * retrieves the kernel address for a page id
 * @param state the state that stores all page information
 * @param pageId the page id for which the kernel address should be returned
 * @param kaddr a pointer to a unsigned long for storing the resulting kernel address
 * @return 1 if successful, 0 otherwise
 */
int kaddr_by_pageId(struct global_state *state, PageId pageId,
		    unsigned long *kaddr);

/**
 * Initializes the given state
 * @param state pointer to be initialized
 */
void init_global_state(struct global_state *state);

/**
 * releases all ressources associated with the global state
 * @param state the state to be released
 */
void release_global_state(struct global_state *state);

#endif //REWIRING_GLOBAL_STATE_H
