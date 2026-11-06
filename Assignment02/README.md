# Assignment02 – SMS Sensor Driver and Application

## 1. Project Overview

This assignment implements an Embedded Linux sensor monitoring system using a Linux character device driver and a user-space C application.

The system generates simulated temperature and humidity measurements. The application communicates with the kernel driver through a character device, reads sensor data, configures the sampling interval using ioctl, and monitors application status using multiple threads.

## 2. System Architecture

### 2.1 Kernel-space Driver

- Registers a character device named `sms_sensor`.
- Provides the device node `/dev/sms_sensor`.
- Generates simulated temperature and humidity values.
- Returns sensor measurements in CSV format.
- Supports the `SMS_SET_INTERVAL` ioctl command.
- Enforces a minimum sampling interval.
- Exposes driver statistics through `/proc/sms_stats`.

### 2.2 User-space Application

- Opens `/dev/sms_sensor`.
- Configures the sampling interval using `ioctl()`.
- Reads sensor measurements using `read()`.
- Uses POSIX threads for sensor reading and status monitoring.
- Handles SIGINT for graceful shutdown.
- Closes the device file descriptor during cleanup.
- Supports host simulation mode using `-t`.

## 3. Directory Structure

```text
Assignment02/
├── README.md
├── app/
│   ├── sms_app.c
│   └── sms_buffer.h
├── driver/
│   ├── Makefile
│   └── sms_sensor.c
└── task2/
    └── logs/
        ├── valgrind_sms.txt
        └── simulation_output.txt
4. Development Environment
Component	Description
Host OS	Ubuntu Linux
Target architecture	ARM 32-bit
Emulated board	QEMU vexpress-a9
Linux kernel	5.15.0
Compiler	arm-linux-gnueabihf-gcc
Programming language	C
Threading	POSIX pthreads


5. Build Instructions
5.1 Build the Kernel Driver
Run on the Ubuntu host:
cd Assignment02/driver
make

The generated kernel module is sms_sensor.ko.
5.2 Build the ARM Application
Run from the repository root:
arm-linux-gnueabihf-gcc \
    -std=c11 -Wall -Wextra -O0 -g -pthread \
    Assignment02/app/sms_app.c \
    -o Assignment02/app/sms_app_arm

6. Driver Interface
Interface	Description
/dev/sms_sensor	Character device for sensor data
/proc/sms_stats	Driver statistics
SMS_SET_INTERVAL	ioctl command to configure the sampling interval


The driver uses a default sampling interval of 100 ms. The configured interval is limited to the range of 1–60000 ms.
7. Application Usage
The application supports the following options:
./sms_app [-d device_path] [-i interval_ms] [-t]

- -d: Specify the sensor device path.
- -i: Set the sampling interval in milliseconds.
- -t: Enable host simulation mode.
8. Test Results
8.1 QEMU Runtime Test
The following behaviors were observed during the QEMU test:
- The driver module loaded successfully.
- The /dev/sms_sensor device node was created.
- The application opened the device successfully.
- The application configured the interval to 500 ms using ioctl.
- Sensor measurements were received in CSV format.
- The status monitoring thread ran.
- SIGINT was handled and the application closed the device.
8.2 Driver Statistics
Measurement	Observed value
Initial read_count	4
Later read_count	29
Configured interval	500 ms


The observed read count did not reach the expected rate of approximately 120 samples per minute. Therefore, the sampling-rate test is recorded as an observed limitation rather than a successful timing verification.
8.3 Memory Monitoring
Measurement point	VmSize	VmRSS	Threads
Initial	18076 kB	156 kB	3
5 seconds	18076 kB	156 kB	3
30 seconds	18076 kB	156 kB	3
Approximately 55 seconds	18076 kB	156 kB	3


The recorded memory values remained constant during these observations.
8.4 Valgrind
The host simulation test reported:
Measurement	Result
Total allocations	3
Total frees	3
Bytes in use at exit	0
Valgrind errors	0


Valgrind reported that all heap blocks were freed and no memory leaks were detected in this run. The log does not independently establish that the test ran for at least 30 seconds.
9. Conclusion
The assignment implements a Linux character device driver and a user-space application for simulated sensor monitoring.

The application successfully communicated with the driver in the QEMU environment, configured the sampling interval, received sensor data, and handled graceful shutdown.

The host simulation Valgrind run reported no memory leaks or errors. Memory monitoring also showed stable recorded values.

However, the observed driver read count did not demonstrate the expected sampling rate. This limitation is documented rather than reported as a successful timing test.
