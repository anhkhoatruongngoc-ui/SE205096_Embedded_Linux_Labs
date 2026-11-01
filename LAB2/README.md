# LAB-02: Viết Device Driver & Quản lý File System

## Môn học
Embedded Linux Systems

## Mục tiêu
- Xây dựng Linux kernel module.
- Triển khai character device `/dev/lab2`.
- Giao tiếp qua procfs và sysfs.
- Thực hành MTD và JFFS2 trên host Ubuntu.
- Tích hợp module vào BusyBox init.
- Boot và kiểm tra hệ thống bằng QEMU.

## Môi trường
- Kiến trúc mục tiêu: ARMv7-A Cortex-A9.
- Máy ảo: QEMU vexpress-a9.
- Kernel mục tiêu: Linux 5.15.0.
- Root filesystem: BusyBox initramfs.

## Cấu trúc
- `driver/`: source code, Makefile và kernel module.
- `rootfs/`: root filesystem dùng cho bài lab.
- `output/`: initramfs đã đóng gói.
- `logs/`: kết quả kiểm thử thực tế.
- `mtd/`: tài liệu hoặc dữ liệu liên quan đến MTD/JFFS2.
- `configs/`: cấu hình phục vụ bài lab.
- `bao_cao/`: báo cáo PDF.

## Ghi chú
Các kết quả kiểm thử và báo cáo cần phản ánh đúng những gì đã thực hiện.
