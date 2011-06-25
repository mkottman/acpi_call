obj-m := acpi_call.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

load:
	-sudo /sbin/rmmod acpi_call
	sudo /sbin/insmod acpi_call.ko
