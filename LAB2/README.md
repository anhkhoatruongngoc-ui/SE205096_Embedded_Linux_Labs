# LAB-02: Device Driver Development & File System Management

## Course
Embedded Linux Systems

## Objectives
- Develop a Linux kernel module.
- Implement a character device driver at `/dev/lab2`.
- Enable communication through procfs and sysfs.
- Practice MTD and JFFS2 on the Ubuntu host system.
- Integrate the kernel module into the BusyBox init system.
- Boot and test the system using QEMU.

## Environment
- Target architecture: ARMv7-A Cortex-A9.
- Virtual machine: QEMU `vexpress-a9`.
- Target Linux kernel: 5.15.0.
- Root filesystem: BusyBox initramfs.

## Directory Structure
- `driver/`: Driver source code, Makefile, and kernel module.
- `rootfs/`: Root filesystem used for the lab.
- `output/`: Packaged initramfs image.
- `logs/`: Actual test results and execution logs.
- `mtd/`: Documentation or data related to MTD/JFFS2.
- `configs/`: Configuration files used for the lab.


## Notes
All test results and reports must accurately reflect the work that has actually been completed. Do not include fabricated test results or claim that unverified features are working.

