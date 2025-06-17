# Makefile for the Proto-SIPE kernel module

obj-m += psipe.o
psipe-objs += psipe_module.o psipe_dma.o psipe_irq.o psipe_queue.o
ccflags-y = -I $(PWD)/../../../include -g -DDEBUG
KDIR := ../../../linux-6.6.72
ARCH := riscv
CROSS_COMPILE := riscv64-linux-gnu-

all:
	make -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	make -C $(KDIR) M=$(PWD) clean
