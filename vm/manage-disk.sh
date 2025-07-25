#!/bin/sh

dev="/dev/loop30"
part="/dev/loop30p1"
point="vda-mnt"
kcode="../src/sw/kernel"
ucode="../src/sw/userspace"

PROGS="
$kcode/psipe.ko
$ucode/master-mm
$ucode/chiplet-mm
$ucode/mm20nwpl
$ucode/mm50nwpl
$ucode/mm100nwpl
$ucode/mm150nwpl
$ucode/mm200nwpl
$ucode/mm250nwpl
$ucode/mm300nwpl
$ucode/mm350nwpl
$ucode/mm400nwpl
"

# Can run all in sequence with "-iur"
while getopts "iur" opt; do
	case "$opt" in
		i) # MOUNT DISK IMAGE
			mkdir -pv $point
			sudo losetup -vP $dev vda.img
			sudo mount -v $part $point
			;;
		u) # UPDATE PROGRAMS
			for prog in $PROGS
			do
				[ -f $prog ] && sudo cp -v $prog $point
			done
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
