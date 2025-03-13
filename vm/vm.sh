#!/bin/env bash

server=off
port_base=9990
instances=1
disk="vda.img"
name="Linux 6.6.72 on riscv64 PNVL-client"
monitor="none"
debug_dev=off

while getopts "Ddsp:n:m" opt; do
	case "$opt" in
		D)
			debug_qemu="-s"
			;;
		d)
			debug_dev=on
			;;
		s)
			server=on
			name="Linux 6.6.72 on riscv64 PNVL-server"
			;;
		p)
			port_base=$OPTARG
			;;
		n)
			instances=$OPTARG
			;;
		m)
			monitor="stdio"
			;;
		*)
			echo "Usage: $0 [-d] [-s] [-p <port>] [-n <instances>]"
			exit 1
			;;
	esac
done
shift $((OPTIND-1))

if [[ -z "$port_base" || ! "$port_base" =~ ^[0-9]+$ || "$port_base" -lt 1 ]]; then
	echo "(-p) must be a positive integer"
	exit 1
fi

if [[ -z "$instances" || ! "$instances" =~ ^[0-9]+$ || "$instances" -lt 1 ]]; then
	echo "(-n) must be a positive integer"
	exit 1
fi

args=""
for i in $(seq 1 $instances); do
	port=$((port_base + i))
	args="$args -device pnvl,server_mode=$server,port=$port"
done

#qemu-system-riscv64 \
~/src/proto-nvlink/qemu/build/qemu-system-riscv64 \
	-machine virt \
	-cpu rv64 \
	-m 2G \
	-smp 1 \
	-drive file=$disk,format=raw,if=none,id=hd0,read-only=on,file.locking=off \
	-device virtio-blk-device,drive=hd0 \
	-kernel ~/src/proto-nvlink/linux-6.6.72/arch/riscv/boot/Image \
	-append "nokaslr root=/dev/vda1 rw console=ttyS0" \
	-monitor $monitor \
	-name "$name" \
	$args \
	$debug_qemu
