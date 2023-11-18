#!/bin/bash

if [[ -z $BANAN_DISK_IMAGE_PATH ]]; then
	echo  "You must set the BANAN_DISK_IMAGE_PATH environment variable" >&2
	exit 1
fi

if [[ -z $BANAN_SYSROOT_TAR ]]; then
	echo  "You must set the BANAN_SYSROOT_TAR environment variable" >&2
	exit 1
fi

if [[ "$1" == "full" ]] || [[ ! -f $BANAN_DISK_IMAGE_PATH ]]; then
	$BANAN_SCRIPT_DIR/image-create.sh
fi

LOOP_DEV=$(sudo losetup --show -f "$BANAN_DISK_IMAGE_PATH")
sudo partprobe $LOOP_DEV

ROOT_PARTITION=${LOOP_DEV}p2
MOUNT_DIR=/mnt

sudo mount $ROOT_PARTITION $MOUNT_DIR

cd $MOUNT_DIR
sudo tar xf $BANAN_SYSROOT_TAR
cd

sudo umount $MOUNT_DIR

sudo losetup -d $LOOP_DEV
