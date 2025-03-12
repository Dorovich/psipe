#!/bin/sh

server=on
port1=9997
port2=9996
disk="vda.img"
name="Linux 6.6.72 on riscv64 PNVL-server"

while getopts "dnp:P:" opt; do
	case "$opt" in
		d)
			flags="-s"
			;;
		n)
			server=off
			disk="vda-cli.img"
			name="Linux 6.6.72 on riscv64 PNVL-client"
			;;
		P)
			port1=$OPTARG
			;;
		p)
			port2=$OPTARG
			;;
	esac
done
shift $((OPTIND-1))

#qemu-system-riscv64 \
~/src/proto-nvlink/qemu/build/qemu-system-riscv64 \
	-machine virt \
	-cpu rv64 \
	-m 2G \
	-smp 1 \
	-drive file=$disk,format=raw,if=none,id=hd0 \
	-device virtio-blk-device,drive=hd0 \
	-kernel ~/src/proto-nvlink/linux-6.6.72/arch/riscv/boot/Image \
	-append "nokaslr root=/dev/vda1 rw console=ttyS0" \
	-device pnvl,server_mode=$server,port=$port1 \
	-device pnvl,server_mode=$server,port=$port2 \
	-monitor stdio \
	-name "$name" \
	$flags
