# Design - Hashtable

The hashtable that is backing the cache in cachegrand rely on a lock free and almost atomic free, relying mostly on
memory fences whenever is necessary, multi producer multi consumer algorithm to guarantee a pretty good vertical
scalability and, at the same time, good performance when running single thread.

Because of the MPMC algorithm without locking there are a set of constraints:
- the memory allocated for the keys or the values can't be immediately freed when a bucket is deleted, there may still
  be a thread reading from the allocated memory;
- the algorithm is non deterministic when used in a multit-hreaded context, because cachegrand is a network server this
  isn't relevant, if the execution of the commands has to be performed in order the application has to handle it in it's
  own business logic (ie. relying on internal locking or using the atomic operations provided by cachegrand to implement
  the locking);

The implemented algorithm is similar to Faceboook F14, the two big differences are:
- cachegrand algorithm is using a mod with a prime number instead of using the upper bits of the hash, while it improves
  the distribution it's also slower, to let the compiler optimize out the mod instruction the implementation to
  calculate the index rely on an huge if with a set of supported prime numbers (that are also the hashtable sizes)
- DOD pattern, the hash is kept in a separated from the bucket itself to take advantage of the cachelines optimizations
  performed by the cpus as much as possible
- the "chunking" approach is similar, cachegrand hashtable has a concept of "neighbourhood" where each one of them is 8
  hash, to match a cacheline, and 2 cachelines are always searched because the modern CPUs perform always a pre-fetch of
  an additional cacheline;
- because of the DOD pattern and the cachelines alignment there is no need for SIMD (vectorial) instructions, the linear
  search on 2 cachelines is way faster than using them because of the cost of performing the load of the registers, my
  algorithm has been tested with SS2, SSE4.1 and AVX2 and in all the cases a linear search on two cachelines was faster
  even if I didn't test it with AVX512 because I hadn't the hardware, there could have been a real speed improvement.
- No need for reference counted tombstones, because searching on the cachelines is blazing fast interrupting the linear
  search brings really a little performance improvement so I preferred to keep the algorithm simpler

