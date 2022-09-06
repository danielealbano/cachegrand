Performance Tips
================

## HugePages

cachegrand has the ability to leverage the hugepages for several optimizations and although the usage is not mandatory
it's expected to be run in production with them enabled.

### Fast Fixed Memory Allocator

When the hugepages are enable it's possible to use the Fast Fixed Memory Allocator (FFMA) which is capable of providing
a ~10% boost, in some cases even more, to cachegrand thanks to the advanced techniques put in place to allocate and free
memory of specific fixed sizes.

### Executable running from hugepages

cachegrand can also leverage the hugepages to run its own code from them, this dramatically reduces the TLB cache
misses and provides up to a 5% speed bump.

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

### Generic Receive Offload (GRO)

The *Generic Receive Offload*, or *GRO*, is a mechanism provided by the kernel to merge together packets at kernel level
to reduce the amount of packets processed by the network stack.

Meanwhile, it's normally a very good idea to keep it on, when using cachegrand there might be cases where it's convenient
to turn it off as the vast majority of commands will fit in a single packet (which can be as long as the MTU) further
reducing extra processing.
This though can have an impact with bigger commands if they are used often than the others, for example if the Redis
`SET` command is very often used with blocks of data bigger than 1400 bytes the network stack will have to process and
generate multiple events for these.

```shell
sudo ethtool -K __NIC__ gro off
```