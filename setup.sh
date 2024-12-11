#!/bin/env bash

printf "Initializing setup. After this, you may build QEMU.\n"

git submodule update --init

REPOSITORY_DIR=$(git rev-parse --show-toplevel)
REPOSITORY_NAME=$(basename $REPOSITORY_DIR)

echo "source $REPOSITORY_NAME/Kconfig" >> qemu/hw/misc/Kconfig
echo "subdir('$REPOSITORY_NAME')" >> qemu/hw/misc/meson.build

ln -s $REPOSITORY_DIR/src/hw/ $REPOSITORY_DIR/qemu/hw/misc/$REPOSITORY_NAME
ln -s $REPOSITORY_DIR/include/hw/pnvl_hw.h $REPOSITORY_DIR/src/hw/pnvl_hw.h

cd qemu
./configure \
	--disable-bsd-user --disable-guest-agent --disable-werror \
	--enable-curses --enable-slirp --enable-libssh --enable-vde --enable-virtfs \
	--target-list=x86_64-softmmu,riscv64-softmmu,riscv32-softmmu

printf "\nSetup finished. You may now build QEMU:\n\tcd qemu && make -j \$(nproc --ignore 2)\n"
