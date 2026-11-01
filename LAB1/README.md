# LAB 1: Kernel Configuration and Embedded System Boot (ARMv7)

## 1. Objectives

This lab covers Linux kernel configuration and compilation, U-Boot,
BusyBox-based root filesystem preparation, and system booting with QEMU.

## 2. Development Environment

- Architecture: ARMv7
- Emulator: QEMU
- Machine: Versatile Express A9
- CPU: Cortex-A9
- Linux kernel: 5.15
- BusyBox: 1.35.0
- U-Boot: 2022.04
- Cross-compiler: arm-linux-gnueabihf-gcc

## 3. Directory Structure

- `configs/kernel.config`: Linux kernel configuration.
- `configs/busybox.config`: BusyBox configuration.
- `output/zImage`: Linux kernel image.
- `output/vexpress-v2p-ca9.dtb`: Device Tree Blob.
- `output/initramfs.cpio.gz`: Compressed initial RAM filesystem.
- `output/u-boot`: U-Boot binary.
- `rootfs/initramfs/`: BusyBox-based root filesystem.
- `bao_cao/`: Lab report directory.

## 4. Booting with QEMU

The following command uses the build artifacts in `~/embedded_lab1`:

    qemu-system-arm \
      -M vexpress-a9 \
      -cpu cortex-a9 \
      -m 512M \
      -smp 2 \
      -nographic \
      -kernel ~/embedded_lab1/output/zImage \
      -dtb ~/embedded_lab1/output/vexpress-v2p-ca9.dtb \
      -initrd ~/embedded_lab1/output/initramfs.cpio.gz \
      -append "console=ttyAMA0,115200 rdinit=/sbin/init mem=512M"

## 5. Notes

- The configuration files document the settings used for the builds.
- The output directory contains the generated build artifacts.
- Device nodes such as `/dev/console` and `/dev/null` are special files and may not be represented in Git as ordinary files.
- Add the lab report PDF to `bao_cao/` when it is available.
- Verify the build and boot process before reporting successful execution.
