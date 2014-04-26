#!/bin/bash
yn="$2"
if [ "$yn" != "y" ]; then
  echo "You have to partition an sd card as follows:"
  echo "First partition: the boot partition. FAT32, at least 16 MiB. There are no reasons why it should be much larger than that. Recommendation: 2/16 of the space of the sd card."
  echo "Second partition: the user progs partition. MinixFS, at least 16 MiB, maybe acquire the whole space? Recommendation: 7/16 of the space of the sd card."
  echo "Third partition: SWEB usually has a third partition which is unused by default and may be used for implementing Swapping/Suspend to Disk/etc. Recommendation: 7/16 of the space of the sd card."
  while [[ "$yn" != "y" && "$yn" != "n" ]]; do
    echo "Do you have a properly partitioned sd card? [y/n]"
    read yn
  done
  if [ "$yn" = "n" ]; then
    echo "Then create one first..."
    return 1
  fi
fi
dev="$3"
while [[ "$dev" = "" || ! -e "$dev" ]]; do
  echo "Which device to mount? (first partition of the sd card, i.e. /dev/mmcblk0p1)"
  read dev
	if [[ "$dev" = "" || ! -e "$dev" ]]; then
		dev="/dev/mmcblk0p1"
	fi
done
echo "Mounting $dev using gvfs-mount ..."
mountpoint=`gvfs-mount -d $dev`
mountpoint=`echo $mountpoint | cut -d " " -f4-`
echo "$mountpoint"
arm-linux-gnueabi-objcopy kernel.x -O binary kernel.img
cp kernel.img $mountpoint
cp $1/* $mountpoint
gvfs-mount -u $mountpoint

