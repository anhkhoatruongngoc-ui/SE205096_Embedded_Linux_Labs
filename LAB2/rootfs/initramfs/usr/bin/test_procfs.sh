#!/bin/sh

echo "========================================"
echo " LAB-02: ProcFS and SysFS Test"
echo "========================================"

echo
echo "[1] Testing /proc/lab2_info"

if [ -r /proc/lab2_info ]; then
    cat /proc/lab2_info
else
    echo "ERROR: /proc/lab2_info does not exist or is not readable"
fi

echo
echo "[2] Testing SysFS directory"

SYSFS_DIR="/sys/class/lab2_class/lab2"

if [ -d "$SYSFS_DIR" ]; then
    echo "SysFS directory exists: $SYSFS_DIR"
    ls -la "$SYSFS_DIR"
else
    echo "ERROR: SysFS directory does not exist"
fi

echo
echo "[3] Testing SysFS attributes"

for attr in buffer_len open_count last_data; do
    if [ -r "$SYSFS_DIR/$attr" ]; then
        echo "--- $attr ---"
        cat "$SYSFS_DIR/$attr"
    else
        echo "WARNING: $attr does not exist or is not readable"
    fi
done

echo
echo "========================================"
echo " ProcFS and SysFS test completed"
echo "========================================"
