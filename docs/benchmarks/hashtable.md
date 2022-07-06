Hashtable
=========

The in-memory hashtable is able to insert up to **2.1 billions** new keys per second on an **1 x AMD EPYC 7502P** and
**192 GB RAM DDR4** using **2048 threads**.

A number of benchmarking units are available in the `benches` folder, the parallelism and the amount of memory available
has to be manually tuned for the machine on which they are going to run.

TODO