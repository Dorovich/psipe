#!/bin/sh

dev="/dev/loop30"
part="/dev/loop30p1"
point="${HOME}/src/proto-nvlink/vm/part1"
code="${HOME}/src/proto-nvlink/src/sw"

# se pueden hacer los tres pasos a la vez con "./disk.sh -iur" o "./disk -a"

while getopts "iura" opt; do
	case "$opt" in
		i)
			mkdir -p $point
			sudo losetup -P $dev vda.img
			sudo mount $part $point
			;;
		u)
			sudo cp $code/kernel/pnvl.ko $point
			sudo cp $code/userspace/master $point
			sudo cp $code/userspace/chiplet $point
			sudo cp $code/userspace/master-multi $point
			;;
		r)
			sudo umount $point
			sudo losetup -d $dev
			rm $point
			;;
		a)
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
	esac
done
