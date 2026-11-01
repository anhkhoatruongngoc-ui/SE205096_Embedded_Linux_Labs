KDIR := $(HOME)/embedded_lab1/kernel/linux-5.15

obj-m += lab2_driver.o

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
