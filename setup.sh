#!/bin/env bash

printf "Initializing setup. After this, you may build QEMU.\n"

git submodule update --init

REPOSITORY_DIR=$(git rev-parse --show-toplevel)
REPOSITORY_NAME=$(basename $REPOSITORY_DIR)
PROJECT_NAME="psipe"

echo "source $PROJECT_NAME/Kconfig" >> qemu/hw/misc/Kconfig
echo "subdir('$PROJECT_NAME')" >> qemu/hw/misc/meson.build

ln -s $REPOSITORY_DIR/src/hw/ $REPOSITORY_DIR/qemu/hw/misc/$PROJECT_NAME
ln -s $REPOSITORY_DIR/include/hw/psipe_hw.h $REPOSITORY_DIR/src/hw/psipe_hw.h

echo "file build/qemu-system-riscv64" >> qemu/.gdbinit
echo "target remote localhost:1234" >> qemu/.gdbinit

cd qemu
./configure \
	--disable-bsd-user \
	--disable-guest-agent \
	--disable-werror \
	--enable-curses \
	--enable-slirp \
	--enable-libssh \
	--enable-vde \
	--enable-virtfs \
	--target-list=riscv64-softmmu

printf "\nSetup finished. You may now build QEMU:\n\tcd qemu && make -j \$(nproc)\n"
