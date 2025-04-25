#!/bin/sh
cd ../qemu-loongarch-runenv
./qemu-system-loongarch64 -m 4G -smp 1 -bios ./loongarch_bios_0310_debug.bin -kernel ../build/images/sel4test-driver-image-loongarch-3A5000 -initrd busybox-rootfs.img -append 'root=/dev/ram console=ttyS0,115200 rdinit=/init' -vga none -nographic