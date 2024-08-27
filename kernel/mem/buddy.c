#include <types.h>
#include <list.h>
#include <paging.h>
#include <string.h>

#include <kernel/mem.h>

/* Physical page metadata. */
size_t npages;
struct page_info *pages;

/*
 * List of free buddy chunks (often also referred to as buddy pages or simply
 * pages). Each order has a list containing all free buddy chunks of the
 * specific buddy order. Buddy orders go from 0 to BUDDY_MAX_ORDER - 1
 */
struct list buddy_free_list[BUDDY_MAX_ORDER];

/* Counts the number of free pages for the given order.
 */
size_t count_free_pages(size_t order)
{
	struct list *node;
	size_t nfree_pages = 0;

	if (order >= BUDDY_MAX_ORDER) {
		return 0;
	}

	list_foreach(buddy_free_list + order, node) {
		++nfree_pages;
	}

	return nfree_pages;
}

/* Shows the number of free pages in the buddy allocator as well as the amount
 * of free memory in kiB.
 *
 * Use this function to diagnose your buddy allocator.
 */
void show_buddy_info(void)
{
	struct page_info *page;
	struct list *node;
	size_t order;
	size_t nfree_pages;
	size_t nfree = 0;

	cprintf("Buddy allocator:\n");

	for (order = 0; order < BUDDY_MAX_ORDER; ++order) {
		nfree_pages = count_free_pages(order);

		cprintf("  order #%u pages=%u\n", order, nfree_pages);

		nfree += nfree_pages * (1 << (order + 12));
	}

	cprintf("  free: %u kiB\n", nfree / 1024);
}

/* Gets the total amount of free pages. */
size_t count_total_free_pages(void)
{
	struct page_info *page;
	struct list *node;
	size_t order;
	size_t nfree_pages;
	size_t nfree = 0;

	for (order = 0; order < BUDDY_MAX_ORDER; ++order) {
		nfree_pages = count_free_pages(order);
		nfree += nfree_pages * (1 << order);
	}

	return nfree;
}

/* Splits lhs into free pages until the order of the page is the requested
 * order req_order.
 *
 * The algorithm to split pages is as follows:
 *  - Given the page of order k, locate the page and its buddy at order k - 1.
 *  - Decrement the order of both the page and its buddy.
 *  - Mark the buddy page as free and add it to the free list.
 *  - Repeat until the page is of the requested order.
 *
 * Returns a page of the requested order.
 */
 struct page_info *buddy_split(struct page_info *lhs, size_t req_order)
{
	/* LAB 1: your code here. */
	size_t order = 0;
	struct page_info *buddy;
	physaddr_t buddy_pa;
	physaddr_t lhs_pa;

	while (lhs->pp_order > req_order) {
        order = lhs->pp_order - 1;
		lhs_pa = page2pa(lhs);
		buddy_pa = lhs_pa ^ (PAGE_SIZE << (lhs->pp_order - 1));
		buddy = pa2page(buddy_pa);

        lhs->pp_order = order;
		lhs->pp_free = 0;

        buddy->pp_order = order;
        buddy->pp_free = 1;

        list_add(&buddy_free_list[order], &(buddy->pp_node));
    }
    return lhs;
}

/* Merges the buddy of the page with the page if the buddy is free to form
 * larger and larger free pages until either the maximum order is reached or
 * no free buddy is found.
 *
 * The algorithm to merge pages is as follows:
 *  - Given the page of order k, locate the page with the lowest address
 *    and its buddy of order k.
 *  - Check if both the page and the buddy are free and whether the order
 *    matches.
 *  - Remove the page and its buddy from the free list.
 *  - Increment the order of the page.
 *  - Repeat until the maximum order has been reached or until the buddy is not
 *    free.
 *
 * Returns the largest merged free page possible.
 */
struct page_info *buddy_merge(struct page_info *page)
{
	/* LAB 1: your code here. */
	size_t order = page->pp_order;

	struct page_info *buddy;
	struct page_info *tmp_page;
	
	physaddr_t buddy_pa;
	physaddr_t page_pa;
	while (order < BUDDY_MAX_ORDER) {
        //size_t buddy_page_num = page_num ^ (1 << order);
        //struct page_info *buddy = &pages[buddy_page_num];
		page_pa = page2pa(page);
		buddy_pa = page_pa ^ (PAGE_SIZE << (page->pp_order));
		buddy = pa2page(buddy_pa);

        if (!buddy->pp_free || buddy->pp_order != order) {
            break;
        }

        if (buddy_pa < page_pa) {
            page = buddy;  
        }
		list_del(&buddy->pp_node);
        order++;
		buddy->pp_order = order;
		buddy->pp_free = 0;
        page->pp_order = order;
		page->pp_free = 0;
    }
	page->pp_free = 1;
	list_add(&buddy_free_list[page->pp_order], &page->pp_node);
    return page;
}

/* Given the order req_order, attempts to find a page of that order or a larger
 * order in the free list. In case the order of the free page is larger than the
 * requested order, the page is split down to the requested order using
 * buddy_split().
 *
 * Returns a page of the requested order or NULL if no such page can be found.
 */
struct page_info *buddy_find(size_t req_order)
{
	/* LAB 1: your code here. */
	struct list *node;
	struct page_info *page;
	for (size_t order = req_order; order <= BUDDY_MAX_ORDER; order++) {

        if (!list_is_empty(&buddy_free_list[order])) {
			node = list_pop(&buddy_free_list[order]);
			page = container_of(node, struct page_info, pp_node);
            if (order > req_order) {
                page = buddy_split(page, req_order);  
            }
            //page->pp_free = 0;  
            return page;
        }
    }

    //fail to find page block with required order
	return NULL;
}

/*
 * Allocates a physical page.
 *
 * if (alloc_flags & ALLOC_ZERO), fills the entire returned physical page with
 * '\0' bytes.
 * if (alloc_flags & ALLOC_HUGE), returns a huge physical 2M page.
 *
 * Beware: this function does NOT increment the reference count of the page -
 * this is the caller's responsibility.
 *
 * Returns NULL if out of free memory.
 *
 * Hint: use buddy_find() to find a free page of the right order.
 * Hint: use page2kva() and memset() to clear the page.
 */
struct page_info *page_alloc(int alloc_flags)
{
	/* LAB 1: your code here. */
	size_t order;
	if(alloc_flags & ALLOC_HUGE){
		order = BUDDY_2M_PAGE;
	} else {
		order = BUDDY_4K_PAGE;
	}

	struct page_info *page = buddy_find(order);
	if(page == NULL){
		return NULL;
	}

	if(alloc_flags & ALLOC_ZERO){
		memset(page2kva(page), 0, (PAGE_SIZE << (page -> pp_order)));
	}
	return page;
}

/*
 * Return a page to the free list.
 * (This function should only be called when pp->pp_ref reaches 0.)
 *
 * Hint: mark the page as free and use buddy_merge() to merge the free page
 * with its buddies before returning the page to the free list.
 */
void page_free(struct page_info *pp)
{
	/* LAB 1: your code here. */
	buddy_merge(pp);
}

/*
 * Decrement the reference count on a page,
 * freeing it if there are no more refs.
 */
void page_decref(struct page_info *pp)
{
	if (--pp->pp_ref == 0) {
		page_free(pp);
	}
}

