# Fastswap
This repository cointains the source code for fastswap (drivers), the far
memory server (farmemserver) and the Linux kernel patches that fastswap needs
(kernel). The kernel patch must be applied on top of Linux kernel 4.11.0.

We assume Ubuntu 16.04 is being used, but other distributions might work as well.
Further, we assume this directory is cloned in your home directory as 'fastswap'.

We only support Mellanox NICs. The best supported NIC is ConnectX-3. However,
if you find issues on ConnectX-4 or ConnectX-5, please let us know and we will
try to address the issues.

## Compiling and installing fastswap kernel (client node)

First you need a copy of the source for kernel 4.11 with SHA
a351e9b9fc24e982ec2f0e76379a49826036da12. We outline the high level steps here.

    cd ~
    wget https://github.com/torvalds/linux/archive/a351e9b9fc24e982ec2f0e76379a49826036da12.zip
    mv a351e9b9fc24e982ec2f0e76379a49826036da12.zip linux-4.11.zip
    unzip linux-4.11.zip
    cd linux-4.11
    git init .
    git add .
    git commit -m "first commit"

Now use the provided patch and apply it against your copy of linux-4.11, and use
the generic Ubuntu config file for kernel 4.11.  You can get the config file
from internet, or you can use the one we provide.

    git apply ~/fastswap/kernel/kernel.patch
    cp ~/fastswap/kernel/config-4.11.0-041100-generic ~/linux-4.11/.config

Make sure you have necessary prerequisites to compile the kernel, and compile
it:

    sudo apt-get install git build-essential kernel-package fakeroot libncurses5-dev libssl-dev ccache bison flex
    make -j `getconf _NPROCESSORS_ONLN` deb-pkg LOCALVERSION=-fastswap

Once it's done, your deb packages should be one directory above, you can simply
install them all:

    cd ..
    sudo dpkg -i *.deb

## OFED setup (client node and far memory node)

We only support Mellanox OFED drivers, and we recommend version 4.1, 4.2 or
4.3. You must have an OFED installation working before proceeding. A good test
before continuing is being able to run both ib\_read\_lat and ib\_write\_lat
between your far memory node and your client. We assume the far memory node has
ip $farmemip and the client has ip $clientip. Note these ips must be bound to
RDMA NICs.

## Far memory server (far memory node)

To build and run the far memory server do:

    cd farmemserver
    make
    ./rmserver 50000

You should see a message saying "listening on port 50000".

## Swap device configuration (client node)

In order to ``activate`` the swap system in Linux, you must have a swap device
pre-registered.  This swap device won't receive any data traffic, we only need
it to activate swap code paths in the kernel. Further, the amount of far memory
the client will be able to access is determined by the swap device size. So if
you want 32GB (default value) of far memory available, your swap device *must*
be exactly 32GB or less. When you type ``free`` in the terminal you must see
that Swap has 32GB of space available in total column.

## Fastswap driver (client node)

To build the drivers:

    cd drivers
    make BACKEND=RDMA

If you have a custom installation path for the Mellanox OFED drivers, modify
the OFA\_DIR variable in the Makefile accordingly. The compilation will only
succeed if you booted on a fastswap kernel.

Now we will load the fastswap drivers.

    sudo insmod fastswap_rdma.ko sport=50000 sip="$farmemip" cip="$clientip" nq=8
    sudo insmod fastswap.ko

sport is the port where the far memory server is running, sip is the far memory
node ip, cip is this node ip (client) and nq must be set to the number of cpus
available in the system. If you type dmesg and you see "ctrl is ready for reqs"
then the connection was successful!

A good next step would be to try out our CFM framework: https://github.com/clusterfarmem/cfm

## DRAM backend

You can use the DRAM backend for experimentation. Compile and load as follows:

    cd drivers
    make BACKEND=DRAM
    sudo insmod fastswap_dram.ko
    sudo insmod fastswap.ko
    
You still need to have swap device enabled, but data won't flow there. By default
the DRAM backend will allocate 32GB of memory.

## Further reading
For more information, please refer to our [paper](https://dl.acm.org/doi/abs/10.1145/3342195.3387522) accepted at [EUROSYS 2020](https://www.eurosys2020.org/)

## Questions
For additional questions please contact us at amaro a@t berkeley.edu
