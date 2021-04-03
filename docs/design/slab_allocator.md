# SLAB Allocator - Design

- Requires hugepages and 2MB pages
- Each huge page is a slab slice
  - the slab slice data are stored at the beginning of the hugepage
- Each slab slice is core-bound
  - cache-locality
  - no cachelines false sharing between cores
- The slab slots index is within the slab slice at the beginning of the hugepage
  - but it doesn't misalign the data, the data are guaranteed of being page-aligned
  - each slot "embeds" the double linked list item, the size matters
  - the slab slot is only 32 bytes
    - 3 pointers and a boolean
    - still 7 bytes available
- Each huge page is allocated on a numa node (TODO)
- Uses a double linked list
  - every time a slot is fetched, the double linked list item is moved to the tail
  - every time a slot is returned, the double linked list item is moved to the head
- No need to pass around the slab allocator struct, the fetch & return relies on the hugepage
  alignment
  - `void *memptr = slab_allocator_mem_alloc(64);`
  - `slab_allocator_mem_free(memptr);`
  - That's it
  - How it works
    - `slab_slice = memptr - memptr % (2*1024*1024)`
    - `object_index = (memptr - slab_slice->data_addr) / slab_slice->slab_allocator->object_size`
- All the operations are BigO(1), no loops of any kind
