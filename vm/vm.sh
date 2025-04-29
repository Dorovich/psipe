#!/bin/env bash

# qemu params
name="Linux 6.6.72 on riscv64 PNVL-client"
monitor="none"

# device params
instances=1
server=off
port_base=9990
debug_dev=off

# disk params
disk="vda.img"
ronly=on
lock=off

while getopts "Ddsp:n:mM" opt; do
	case "$opt" in
		D) # DEBUG QEMU
			debug_qemu="-s"
			;;
		d) # DEBUG THE DEVICE (W.I.P.)
			debug_dev=on
			;;
		s) # ACT AS SERVER
			server=on
			name="Linux 6.6.72 on riscv64 PNVL-server"
			;;
		p) # CHANGE THE BASE PORT
			port_base=$OPTARG
			;;
		n) # NUMBER OF DEVICES
			instances=$OPTARG
			;;
		m) # USE QEMU MONITOR
			monitor="stdio"
			;;
		M) # MAINTENANCE OF DISK IMAGE
			instances=0
			ronly=off
			lock=on
			name="Linux 6.6.72 on riscv64 MAINTENANCE"
			;;
		*)
			echo "Exited. Check the $0 file for flag info."
			exit 1
			;;
	esac
done
shift $((OPTIND-1))

if [[ -z "$port_base" || ! "$port_base" =~ ^[0-9]+$ || "$port_base" -lt 1025 ]]; then
	echo "(-p) must be a positive integer"
	exit 1
fi

if [[ -z "$instances" || ! "$instances" =~ ^[0-9]+$ || "$instances" -lt 0 ]]; then
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
	-drive file=$disk,format=raw,if=none,id=hd0,read-only=$ronly,file.locking=$lock \
	-device virtio-blk-device,drive=hd0 \
	-kernel ~/src/proto-nvlink/linux-6.6.72/arch/riscv/boot/Image \
	-append "nokaslr root=/dev/vda1 rw console=ttyS0" \
	-monitor $monitor \
	-name "$name" \
	$args \
	$debug_qemu
