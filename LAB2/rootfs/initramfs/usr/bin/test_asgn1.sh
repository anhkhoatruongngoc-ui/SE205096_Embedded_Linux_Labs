#!/bin/sh

MODULE="/lib/modules/5.15.0/asgn1_driver.ko"
DEVICE="/dev/asgn1"
TEST_PROGRAM="/usr/bin/test_asgn1"
MAJOR=241

echo "========================================"
echo " ASGN1 DRIVER AUTOMATED TEST"
echo "========================================"

# 1. Kiem tra quyen root
if [ "$(id -u)" != "0" ]; then
    echo "ERROR: Please run this script as root."
    exit 1
fi

# 2. Luu log cuoi cung de doi chieu
echo
echo "[1] Preparing test environment"

# Neu module da duoc nap tu truoc thi go bo
if grep -q '^asgn1_driver ' /proc/modules; then
    rmmod asgn1_driver || {
        echo "ERROR: Cannot unload existing asgn1_driver"
        exit 1
    }
fi

# 3. Nap module
echo
echo "[2] Loading module"

if [ ! -f "$MODULE" ]; then
    echo "ERROR: Module not found: $MODULE"
    exit 1
fi

if ! insmod "$MODULE"; then
    echo "ERROR: insmod failed"
    dmesg | tail -n 30
    exit 1
fi

echo "Module loaded successfully"

# 4. Tao device node neu can
echo
echo "[3] Preparing device node"

if [ ! -e "$DEVICE" ]; then
    mknod "$DEVICE" c "$MAJOR" 0 || {
        echo "ERROR: Cannot create $DEVICE"
        rmmod asgn1_driver
        exit 1
    }
fi

chmod 666 "$DEVICE"
ls -l "$DEVICE"

# 5. Chay chuong trinh C
echo
echo "[4] Running ioctl tests"

if [ ! -x "$TEST_PROGRAM" ]; then
    echo "ERROR: Test program not found: $TEST_PROGRAM"
    rmmod asgn1_driver
    exit 1
fi

"$TEST_PROGRAM"
TEST_RESULT=$?

# 6. Hien thi kernel log truoc khi unload
echo
echo "[5] Driver kernel log"
dmesg | grep 'asgn1_driver' | tail -n 40

# 7. Unload module
echo
echo "[6] Unloading module"

if rmmod asgn1_driver; then
    echo "Module unloaded successfully"
else
    echo "ERROR: Failed to unload module"
    exit 1
fi

# 8. Xac nhan cleanup
echo
echo "[7] Checking cleanup log"

if dmesg | grep 'asgn1_driver: unloaded, resources cleaned up' | tail -n 1; then
    echo "Cleanup log found"
else
    echo "WARNING: Cleanup log not found"
fi

# 9. Ket qua
echo
echo "========================================"

if [ "$TEST_RESULT" -eq 0 ]; then
    echo "ASGN1 TEST SCRIPT COMPLETED: PASS"
else
    echo "ASGN1 TEST SCRIPT COMPLETED: FAIL"
fi

echo "========================================"

exit "$TEST_RESULT"
