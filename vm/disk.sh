#!/bin/sh

loopdev="/dev/loop24"
loopdev_part="/dev/loop24p1"
dir="part1"

[ -z $IMG ] && echo "Usage:\tIMG=<file.img> <command>" && exit 1

# se pueden hacer los tres pasos a la vez con "./disk.sh -iur"

while getopts "iur" opt; do
	case "$opt" in
		i)
			sudo losetup -P $loopdev $IMG
			sudo mount $loopdev_part $dir
			;;
		u)
			sudo cp ${HOME}/src/proto-nvlink/src/sw/kernel/pnvl.ko $dir
			sudo cp ${HOME}/src/proto-nvlink/src/sw/userspace/master $dir
			sudo cp ${HOME}/src/proto-nvlink/src/sw/userspace/chiplet $dir
			;;
		r)
			sudo umount $dir
			sudo losetup -d $loopdev
			;;
	esac
done
