#!/usr/bin/env bash
set -e

arg_projname=MeadowComms
arg_flashimg=$arg_projname-qemu.bin

dd if=/dev/zero bs=1024 count=4096 of=${arg_flashimg}
dd if=build/bootloader/bootloader.bin bs=1 seek=$((0x1000)) of=${arg_flashimg} conv=notrunc
dd if=build/partitions_singleapp.bin bs=1 seek=$((0x8000)) of=${arg_flashimg} conv=notrunc
dd if=build/${arg_projname}.bin bs=1 seek=$((0x10000)) of=${arg_flashimg} conv=notrunc
