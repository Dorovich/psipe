#!/bin/sh

dev="/dev/loop30"
part="/dev/loop30p1"
point="${HOME}/src/proto-nvlink/vm/part1"
code="${HOME}/src/proto-nvlink/src/sw"

while getopts "iura" opt; do
	case "$opt" in
		i) # MOUNT DISK IMAGE
			mkdir -p $point
			sudo losetup -P $dev vda.img
			sudo mount $part $point
			;;
		u) # UPDATE PROGRAMS
			sudo cp $code/kernel/pnvl.ko $point
			sudo cp $code/userspace/master $point
			sudo cp $code/userspace/chiplet $point
			sudo cp $code/userspace/master-multi $point
			;;
		r) # UNMOUNT DISK IMAGE
			sudo umount $point
			sudo losetup -d $dev
			rm $point
			;;
		a) # DO ALL THREE (MOUNT -> UPDATE -> UNMOUNT)
			mkdir -p $point
			sudo losetup -P $dev vda.img
			sudo mount $part $point
			sudo cp $code/kernel/pnvl.ko $point
			sudo cp $code/userspace/master $point
			sudo cp $code/userspace/chiplet $point
			sudo cp $code/userspace/master-multi $point
			sudo umount $point
			sudo losetup -d $dev
			rm -rf $point
			;;
		*)
			echo "Exited. Check the $0 file for flag info."
			exit 1
			;;
	esac
done
