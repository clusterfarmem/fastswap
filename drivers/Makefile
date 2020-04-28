OFA_DIR ?= /usr/src/ofa_kernel/default
OFA_INCLUDE := $(OFA_DIR)/include
OFA_SYMVERS := $(OFA_DIR)/Module.symvers

ifneq ($(LINUXINCLUDE),)
# kbuild part of makefile

LINUXINCLUDE := \
  -I$(OFA_INCLUDE) \
  ${LINUXINCLUDE}

else
# normal makefile

export KBUILD_EXTRA_SYMBOLS=$(OFA_SYMVERS)
KDIR ?= /lib/modules/`uname -r`/build

default:
	$(MAKE) -C $(KDIR) M=$$PWD

clean:
	$(MAKE) -C $(KDIR) M=$$PWD clean
endif

obj-m  := fastswap.o
ifeq ($(BACKEND),RDMA)
	obj-m += fastswap_rdma.o
	CFLAGS_fastswap.o=-DBACKEND=2
else
	obj-m += fastswap_dram.o
	CFLAGS_fastswap.o=-DBACKEND=1
endif
