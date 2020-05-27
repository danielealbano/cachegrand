# Design - Principles - OBSOLETE

Cachegrand has been designed around these principles:
- lock free and, whenever it's not impacting the consistency, atomic-free using memory barriers
  - if possible use hardware memory transactions to avoid atomic operations
- data structures aligned and paddeded to improve L1 cache efficiency and reduce cacheline contantion / false sharing
- use MPMC (multi producer, multi consumer) algorithms only if strictly necessary, prefer if possible MPSC (multi
  producer single consumer) or SPMC (single producer multi consumer)
  - the only MPMC algorithm implemented is the hashtable so far  
- numa aware
- cache coherent
- super scalable
- avoid freeing and allocating memory continuously, better to allocate hugepages working in append mode only and then
  perform a garbage collection to free up the memory at a later stage
- DOD (data oriented design) over OOP (object oriented programming) to improve performances
