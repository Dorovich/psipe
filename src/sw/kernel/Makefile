# Makefile for the Proto-NVLink kernel module

obj-m += pnvl.o
pnvl-objs += pnvl_module.o pnvl_dma.o pnvl_irq.o
ccflags-y = -I${HOME}/src/proto-nvlink/include

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
