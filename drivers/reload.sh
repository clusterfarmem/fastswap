#sudo swapoff /swapfile
#sudo swapon /swapfile
farmemip=10.10.49.95
clientip=10.10.49.44
#sudo modprobe svcrdma
#sudo modprobe mlx4_ib
#sudo modprobe rdma_ucm
#sudo modprobe ib_umad
#sudo modprobe ib_uverbs
#sudo modprobe ib_ipoib
sudo rmmod fastswap.ko
sudo rmmod fastswap_rdma.ko
echo "waiting 10 secs so you can MANUALLY restart remote backend"
sleep 10
echo "reloading..."
sudo insmod fastswap_rdma.ko sport=50013 sip="$farmemip" cip="$clientip" nq=8
sudo insmod fastswap.ko

# perf uses these in "extra" dir to extract symbols in `perf report -g`
# source` https://stackoverflow.com/questions/44326565/perf-kernel-module-symbols-not-showing-up-in-profiling
mkdir -p /lib/modules/`uname -r`/extra
sudo ln -sf `pwd`/fastswap_rdma.ko  /lib/modules/`uname -r`/extra/fastswap_rdma.ko
sudo ln -sf `pwd`/fastswap.ko  /lib/modules/`uname -r`/extra/fastswap.ko

