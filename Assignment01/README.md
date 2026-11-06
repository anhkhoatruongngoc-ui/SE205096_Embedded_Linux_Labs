# ASGM1 – Embedded Linux Device Drivers

## 1. Project Overview

This project implements two Linux kernel device drivers as part of Assignment 1 (ASGM1).

The assignment focuses on Linux Character Device Drivers, IOCTL communication, Miscellaneous Device Drivers, atomic operations, and kernel module development.

Both drivers are developed for an Embedded Linux environment and designed to run on an ARM-based virtual platform using QEMU.

## 2. Project Objectives

- Understand Linux kernel module development.
- Implement Character Device Drivers.
- Understand and use IOCTL system calls.
- Implement Miscellaneous Device Drivers.
- Manage shared data using atomic operations.
- Handle user-space and kernel-space data communication.
- Create and monitor entries in the Proc Filesystem.
- Cross-compile kernel modules for ARM.
- Deploy and test drivers using QEMU.

## 3. Task 1 – Character Device Driver with IOCTL

### 3.1 Overview

Task 1 implements a Linux Character Device Driver named `asgn1_driver`.

The driver demonstrates communication between user space and kernel space through the character device interface and IOCTL operations.

### 3.2 Main Concepts

- Linux Character Device Driver.
- Device registration and unregistration.
- File operations.
- IOCTL system calls.
- User-space and kernel-space data transfer.
- Kernel memory management using `kmalloc()` and `kfree()`.

### 3.3 Driver Components

The implementation includes:

- Kernel module initialization and cleanup.
- Character device operations.
- IOCTL command handling.
- User-space test application.

### 3.4 Build and Testing

Navigate to the Task 1 directory:

```bash
cd ~/embedded_lab2/asgn1
Build the kernel module using the provided Makefile:
make

The module can then be deployed to the ARM initramfs and loaded in QEMU.

Example module loading command:
insmod asgn1_driver.ko

Check loaded modules:
lsmod

Remove the module:
rmmod asgn1_driver

4. Task 2 – Misc Device Driver (Counter Device)
4.1 Overview
Task 2 implements a Linux Miscellaneous Character Device Driver named counter_driver.

The driver provides a counter device that allows users to read, update, and reset a counter value through /dev/counter.
4.2 Device Registration
- Device name: /dev/counter
- Device type: Miscellaneous Character Device
- Major number: 10 (MISC_MAJOR)
- Minor number: Dynamically allocated
The driver uses:
- struct miscdevice
- misc_register()
- misc_deregister()
4.3 Read Operation
When a user reads from /dev/counter:
- The current counter value is returned as a decimal number.
- A newline character is appended.
- The counter is incremented after a successful read.
- The file position is checked to prevent repeated output.
4.4 Write Operation
The write operation supports:
- Setting the counter to a non-negative integer.
- Resetting the counter by writing 0.
- Rejecting negative or invalid input with -EINVAL.
- Parsing input using kstrtoint().
4.5 Atomic Operations
The driver uses Linux atomic operations:
- atomic_t
- atomic_read()
- atomic_set()
- atomic_inc()
These operations are used to manage counter values and operation statistics.
4.6 Proc Filesystem
The driver creates:
/proc/counter_info

This interface displays:
- Current counter value.
- Total number of read operations.
- Total number of write operations.
4.7 Build Instructions
Navigate to the Task 2 directory:
cd ~/embedded_lab2/task2_counter

Build the kernel module:
make

The generated module is:
counter_driver.ko

Clean build files:
make clean

4.8 Testing
Load the module:
insmod counter_driver.ko

Check the device:
ls -l /dev/counter

Test reading:
cat /dev/counter
cat /dev/counter
cat /dev/counter

Test writing:
echo 100 > /dev/counter
cat /dev/counter

Test resetting:
echo 0 > /dev/counter
cat /dev/counter

Test invalid input:
echo -5 > /dev/counter

Check driver statistics:
cat /proc/counter_info

Unload the module:
rmmod counter_driver

5. Project Structure
embedded_lab2/
│
├── README.md
│
├── asgn1/
│   ├── asgn1_driver.c
│   ├── test_asgn1.c
│   └── Makefile
│
└── task2_counter/
    ├── counter_driver.c
    ├── Makefile
    └── README.md

Note: The directory structure above describes the expected project organization. Adjust filenames if your actual source files use different names.
6. Development Environment
Component	Configuration
Operating System	Ubuntu Linux
Kernel Version	Linux 5.15
Target Architecture	ARM
Cross Compiler	arm-linux-gnueabihf-gcc
Virtual Platform	QEMU vexpress-a9
Root Filesystem	BusyBox-based initramfs


7. Deployment on QEMU
The compiled kernel modules are copied into the initramfs root filesystem.

Example:
mkdir -p ~/embedded_lab2/rootfs/initramfs/lib/modules/5.15.0

Copy the modules into the appropriate directory:
cp ~/embedded_lab2/asgn1/asgn1_driver.ko \
~/embedded_lab2/rootfs/initramfs/lib/modules/5.15.0/

cp ~/embedded_lab2/task2_counter/counter_driver.ko \
~/embedded_lab2/rootfs/initramfs/lib/modules/5.15.0/

Rebuild the initramfs:
cd ~/embedded_lab2/rootfs/initramfs

find . -print0 | cpio --null -o -H newc | gzip -1 \
> ~/embedded_lab2/output/initramfs_asgn1.cpio.gz

Boot the ARM virtual machine using QEMU with the generated initramfs.
8. Learning Outcomes
Through this assignment, the following concepts are practiced:
- Linux Kernel Module Development.
- Character Device Driver Implementation.
- IOCTL Communication.
- Miscellaneous Device Framework.
- Atomic Operations.
- Kernel and User-Space Communication.
- Proc Filesystem.
- ARM Cross-Compilation.
- Embedded Linux Deployment and Testing with QEMU.
