# uarch-tools

A demonstration of the blocking behavior of the `prefetch` and `prefetchw` instructions on most x86 CPUs. Though not mentioned in teh architecture manuals,
this blocking behavior was first observed during performance tuning of an explicitly prefetched hash table. This experiment shows `prefetch` appearing to block
on the memory subsystem, but only when it is saturated. The `use_randomized` flag allows you to confirm that the overhead of each prefetch sharply declines when
the cache line is already in the cache; change the number of cores used to only one (as long as your memory subsystem cannot be saturated by one core alone), to watch
the overhead disappear in all cases once the memory subsystem is no longer saturated.

The exact cause of this behavior is known only to Intel and AMD, of course, but it has been observed on both a Xeon Gold and a Ryzen 3700X. I suspect there is some internal
queue in which pending prefetches are kept, and it is only when this queue has completely filled as pending prefetches wait for a result from the saturated memory controller,
that the individual instruction start blocking.
