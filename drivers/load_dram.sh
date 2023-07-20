sudo swapon /swapfile
sudo rmmod fastswap.ko
sudo rmmod fastswap_dram.ko
sudo insmod fastswap_dram.ko
sudo insmod fastswap.ko

