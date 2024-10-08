#include <types.h>
#include <paging.h>

#include <kernel/mem.h>

/* This simple physical memory allocator is used only while OpenLSD is setting up
 * its virtual memory system.
 *
 * If n >-0, allocates enough pages of contiguous physical memory to hold 'n'
 * bytes. Doesn't initialize the memory. Returns a kernel virtual address.
 *
 * If n == 0, returns the address of the next free page without allocating
 * anything.
 *
 * IF we're out of memory, boot_alloc() should panic.
 * This function may ONLY be used during initialization, before the buddy
 * allocator has been set up.
 */
void *boot_alloc(uint32_t n)
{
	/* Virtual address of the next byte of free memory. */
	static char *next_free;
	char *result;

	/* Initialize next_free if this is the first time. 'end' is a magic
	 * symbol automatically generated by the linker, which points to the
	 * end of the kernel's bss segment: the first virtual address that the
	 * linker did not assign to any kernel code or global variables.
	 */
	if (!next_free) {
		extern char end[];
		next_free = ROUNDUP((char *)end, PAGE_SIZE);
	}

	/* Allocate a chunk large enough to hold 'n' bytes, then update
	 * next_free. Make sure next_free is kept aligned to a multiple of
	 * PAGE_SIZE.
	 *
	 * LAB 1: your code here.
	 */
	result = next_free;
	if(n){
		next_free = ROUNDUP(next_free + n,  PAGE_SIZE);
	}
	return result;
}

/* The addresses and lengths in the memory map provided by the boot loader may
 * not be page aligned. This function checks every region and ensures that the
 * addresses and lengths are page aligned.
 *
 * For free memory, the base address is rounded up and the length is rounded
 * down. For any other type of memory, the base address is rounded down and the
 * length is rounded up.
 */
void align_boot_info(struct boot_info *boot_info)
{
	struct mmap_entry *entry;
	size_t i;
	physaddr_t base, end;

	entry = (struct mmap_entry *)((physaddr_t)boot_info->mmap_addr);

	for (i = 0; i < boot_info->mmap_len; ++i, ++entry) {
		switch (entry->type) {
		case MMAP_FREE:
			base = ROUNDUP(entry->addr, PAGE_SIZE);
			end = entry->len - (base - entry->addr);
			end = ROUNDDOWN(base + end, PAGE_SIZE);
			break;
		default:
			base = ROUNDDOWN(entry->addr, PAGE_SIZE);
			end = entry->len + (entry->addr - base);
			end = ROUNDUP(base + end, PAGE_SIZE);
			break;
		}

		entry->addr = base;
		entry->len = end - base;
	}
}

void show_boot_mmap(struct boot_info *boot_info)
{
	struct mmap_entry *entry;
	size_t i;
	physaddr_t base, end;

	entry = (struct mmap_entry *)KADDR(boot_info->mmap_addr);

	cprintf("Boot memory map:\n");

	for (i = 0; i < boot_info->mmap_len; ++i, ++entry) {
		const char *region;

		switch (entry->type) {
		case MMAP_FREE: region = "free"; break;
		case MMAP_RESERVED: region = "reserved"; break;
		case MMAP_ACPI_RECLAIMABLE: region = "ACPI"; break;
		case MMAP_ACPI_NVS: region = "ACPI NVS"; break;
		case MMAP_BAD: region = "bad"; break;
		default: region = "?"; break;
		}

		cprintf("  %016p - %016p [%s]\n",
			entry->addr,
			entry->addr + entry->len,
			region);
	}
}
