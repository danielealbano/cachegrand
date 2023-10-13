Performance Tips
================

## HugePages

cachegrand has the ability to leverage the hugepages for several optimizations and although the usage is not mandatory
it's expected to be run in production with them enabled.

## Network Settings

Below a number of tips and suggestions to help improve cachegrand performances, although in general they are useful for
every network platform that is built around cache locality.

### Receive Side Scaling (RSS)

The *Receive Side Scaling*, or *RSS*, is a mechanism provided in hardware by network cards to distribute packets across
a number of receive queues evenly using IRQs, this mechanism works great but data received via a queue can be processed
by different CPUs causing cache misses.
To reduce the cache misses it's possible to bind 1 queue to 1 specific cpu using /proc/interrupts and
/proc/irq/IRQ_NUMBER/smp_affinity_list.

As part of the configuration, it's necessary to disable the irqbalance service.

**Replace __NIC__ with the network card interface in use, keep the dash after the name**

```shell
sudo systemctl stop irqbalance.service

export IRQS=($(grep __NIC__\- /proc/interrupts | awk '{print $1}' | tr -d :))
for I in ${!IRQS[@]}; do
  echo $I | sudo tee /proc/irq/${IRQS[I]}/smp_affinity_list > /dev/null;
done;
```

*The `systemctl stop irqbalance.service` command may not work as it is on all the distribution, it has been tested only
on Ubuntu*

It's also important to configure cachegrand to start a worker for each receive queue available, to get the best vertical
scalability. It's possible to start more than one worker per queue or more workers than queues but of course the
benefits will be limited.

Here a simple snippet to list the amount of queues available for your nic
```shell
grep __NIC__\- /proc/interrupts -c
```

### Transmit Packet Steering (XPS)

Meanwhile RSS is a mechanism provided to tune the distribution and locality of the incoming data, the *Transmit Packet
Steering*, or *XPS*, is used for the outgoing packets.

**Replace __NIC__ with the network card interface in use**

```shell
sudo systemctl stop irqbalance.service

export TXQUEUES=($(ls -1qdv /sys/class/net/__NIC__/queues/tx-*))
for i in ${!TXQUEUES[@]}; do
  printf '%x' $((2**i)) | sudo tee ${TXQUEUES[i]}/xps_cpus > /dev/null;
done;
```

*The `systemctl stop irqbalance.service` command may not work as it is on all the distribution, it has been tested only
on Ubuntu*

### Busy Pooling

```shell
sudo sysctl net.core.busy_poll=1
```

### Packets queueing

When using Virtual Machines that are not using a physical network card via a VF, it's possible to turn off the
unprocessed packets queueing because normally the driver will just accept everything that is arriving.

**Replace __NIC__ with the network card interface in use**

```shell
sudo sysctl net.core.default_qdisc=noqueue
sudo tc qdisc replace dev __NIC__ root mq
```

### Kernel and NIC Offload

Both the Kernel and the NIC offer several different potential optimizations to merge, or split, packets and to offload
some of the processing to the hardware. Although in general it's a good idea to keep them on, it's possible to turn them
off in some cases to improve the latencies.
It's important to test out the different settings to find the best combination for your specific use case as any
combination can have a different impact and will depend on the behavior of the applications using cachegrand.

Here some useful documentation
https://www.kernel.org/doc/html/latest/networking/segmentation-offloads.html

Certain kind of offloads are not available on all the network cards and the kernel will try to compensate performing
these operation in software, causing a latency degradation. To improve it it's possible to turn them off.

#### Generic Segmentation Offload (GSO)

The *Generic Segmentation Offload*, or *GSO*, is a mechanism provided by the kernel to split packets at software level
to match the MSS, meanwhile it can be useful to keep it on, it's possible to turn it off to reduce the latency.

```shell
sudo ethtool -K __NIC__ gso off
```

#### Generic Receive Offload (GRO)

The *Generic Receive Offload*, or *GRO*, is a mechanism provided by the kernel to merge packets at software level to
reduce the amount of packets that need to be processed. As for the GSO, it's possible to turn it off to improve the
latency.

```shell
sudo ethtool -K __NIC__ gro off
```

### Ring size

The ring size is the amount of packets that can be stored in the network card before the kernel will start dropping
packets, this is normally a good thing to keep high but it's important to keep in mind that the higher the ring size the
higher the latency.

Higher values are better when using high speed network interfaces like 10Gbps, 25Gbps, 40Gbps or 100Gbps.

```shell
sudo ethtool -G __NIC__ rx 4096 tx 4096
```

It's also possible to tune the size of the ring for the mini packets, useful if dealing with commands that are very
small in size.

```shell
sudo ethtool -G __NIC__ rx-mini 4096
```

And it's also possible to tune the size of the ring for the jumbo packets, useful if dealing with commands that are very
big in size.

```shell
sudo ethtool -G __NIC__ rx-jumbo 4096
```

### Interrupt Coalescing

The *Interrupt Coalescing*, or *IRQ Coalescing*, is a mechanism provided by the kernel to reduce the amount of IRQs
generated by the network card, this is normally a good thing to keep on but it's important to keep in mind that the
higher the coalescing the higher the latency.

Here more information about it
https://people.kernel.org/kuba/easy-network-performance-wins-with-irq-coalescing


Higher values are better when using high speed network interfaces like 10Gbps, 25Gbps, 40Gbps or 100Gbps.

```shell
sudo ethtool -C __NIC__ rx-usecs 1000 rx-frames 1000 tx-usecs 1000 tx-frames 1000
```
