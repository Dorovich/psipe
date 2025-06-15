#!/bin/sh

dev="/dev/loop30"
part="/dev/loop30p1"
point="vda-mnt"
code="../src/sw"

km=$code/kernel/pnvl.ko
m0=$code/userspace/master0
m1=$code/userspace/master1
c0=$code/userspace/chiplet0
c1=$code/userspace/chiplet1
mm=$code/userspace/master-mm
cm=$code/userspace/chiplet-mm

# Can run all in sequence with "-iur"
while getopts "iur" opt; do
	case "$opt" in
		i) # MOUNT DISK IMAGE
			mkdir -pv $point
			sudo losetup -vP $dev vda.img
			sudo mount -v $part $point
			;;
		u) # UPDATE PROGRAMS
			[ -f $km ] && sudo cp -v $km $point
			[ -f $m0 ] && sudo cp -v $m0 $point
			[ -f $c0 ] && sudo cp -v $c0 $point
			[ -f $m1 ] && sudo cp -v $m1 $point
			[ -f $c1 ] && sudo cp -v $c1 $point
			[ -f $mm ] && sudo cp -v $mm $point
			[ -f $cm ] && sudo cp -v $cm $point
			;;
		r) # UNMOUNT DISK IMAGE
			sudo umount -v $point
			sudo losetup -vd $dev
			rm -rdv $point
			;;
		*)
			echo "Exited. Check the $0 file for flag info."
			exit 1
			;;
	esac
done
