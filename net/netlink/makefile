obj-m:=kernel_test.o
KERNLE_DIR:=/lib/modules/$(shell uname -r)/build
PWD:=$(shell pwd)
all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules
	gcc test_user.c -o test

clean:
	rm -rf test kernel_test.mod.c kernel_test.ko kernel_test.o
