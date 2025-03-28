#!/bin/sh

dev="/dev/loop30"
part="/dev/loop30p1"
point="${HOME}/src/proto-nvlink/vm/part1"
code="${HOME}/src/proto-nvlink/src/sw"

km=$code/kernel/pnvl.ko
m0=$code/userspace/master0
m1=$code/userspace/master1
c0=$code/userspace/chiplet0
c1=$code/userspace/chiplet1
mm=$code/userspace/master-mm
cm=$code/userspace/chiplet-mm

while getopts "iura" opt; do
	case "$opt" in
		i) # MOUNT DISK IMAGE
			mkdir -p $point
			sudo losetup -P $dev vda.img
			sudo mount $part $point
			;;
		u) # UPDATE PROGRAMS
			[ -f $km ] && sudo cp $km $point
			[ -f $m0 ] && sudo cp $m0 $point
			[ -f $c0 ] && sudo cp $c0 $point
			[ -f $m1 ] && sudo cp $m1 $point
			[ -f $c1 ] && sudo cp $c1 $point
			[ -f $mm ] && sudo cp $mm $point
			[ -f $cm ] && sudo cp $cm $point
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
			[ -f $km ] && sudo cp $km $point
			[ -f $m0 ] && sudo cp $m0 $point
			[ -f $c0 ] && sudo cp $c0 $point
			[ -f $m1 ] && sudo cp $m1 $point
			[ -f $c1 ] && sudo cp $c1 $point
			[ -f $mm ] && sudo cp $mm $point
			[ -f $cm ] && sudo cp $cm $point
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
