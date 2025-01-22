# Makefile for the Proto-NVLink kernel module

obj-m += pnvl.o
pnvl-objs += pnvl_module.o pnvl_dma.o pnvl_irq.o
ccflags-y = -I ${HOME}/src/proto-nvlink/include -g -DDEBUG
KDIR := ../../../linux-6.6.72
ARCH := riscv
CROSS_COMPILE := riscv64-linux-gnu-

all:
	make -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	make -C $(KDIR) M=$(PWD) clean
